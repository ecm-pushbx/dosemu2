#include "mhpdbg.h"
#include "debug.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#ifdef __linux__
#include <linux/version.h>
#endif

#include "emu.h"
#include "utilities.h"
#include "int.h"

#include "video.h"
#include "vgaemu.h" /* root@zaphod */

#include "dpmi.h"
#include "cpu-emu.h"
#include "dosemu_config.h"
#include "sig.h"

/*
 * All of the functions in this module need to be declared with
 *   __attribute__((no_instrument_function))
 * so that they can safely handle signals that occur in DPMI context when
 * DOSEMU is built with the "-pg" gcc flag (which enables instrumentation for
 * gprof profiling).
 *
 * The reason for this is that mcount(), implicitly called from functions
 * instrumented with "-pg", requires access to thread-local state, and on x86,
 * TLS is implemented using the GS to refer to a segment in which the
 * thread-local variables are stored.
 *
 * However, in DPMI context, GS does not refer to this segment, and the kernel
 * does not (cannot?) restore it to do so when it invokes a signal handler, so
 * we must prevent mcount() from being called at all in this context.
 */


/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, sigcontext_t);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scanned by the kernel) are handled here.
 *
 * We have 4 main cases:
 * 1. VM86 faults from vm86() (i386 only)
 * 2. DPMI faults with LDT _cs (native DPMI only)
 * 3. Faults (PF/DE) generated from cpuemu. In this case _cs is the Linux
 *    userspace _cs
 * 4. DOSEMU itself crashes (bad!)
 *
 * DANG_END_FUNCTION
 */
static void dosemu_fault1(int signal, sigcontext_t *scp)
{
  if (fault_cnt > 1) {
    error("Fault handler re-entered! signal=%i _trapno=0x%X\n",
      signal, _trapno);
    if (!in_vm86 && !DPMIValidSelector(_cs)) {
      gdb_debug();
      _exit(43);
    } else {
      error("BUG: Fault handler re-entered not within dosemu code! in_vm86=%i\n",
        in_vm86);
    }
    goto bad;
  }
#ifdef __x86_64__
  if (_trapno == 0x0e && _cr2 > 0xffffffff)
  {
    dosemu_error("Accessing reserved memory at %08"PRI_RG"\n"
	  "\tMaybe a null segment register\n",_cr2);
    goto bad;
  }
#endif


#ifdef __i386__
  /* case 1: note that _cr2 must be 0-based */
  if (in_vm86 && config.cpu_vm == CPUVM_VM86) {
    if (_trapno == 0x0e) {
      /* we can get to instremu from here, so unblock SIGALRM & friends.
       * It is needed to interrupt instremu when it runs for too long. */
      signal_unblock_async_sigs();
      if (vga_emu_fault(_cr2, _err, NULL) == True)
        return;
    }
    vm86_fault(_trapno, _err, _cr2);
    return;
  }
#endif

  /* case 2: At first let's find out where we came from */
  if (DPMIValidSelector(_cs)) {
    int ret = DPMI_RET_FAULT;
    assert(config.cpu_vm_dpmi == CPUVM_NATIVE);
    if (_trapno == 0x0e) {
      int rc;
#ifdef X86_EMULATOR
#ifdef HOST_ARCH_X86
     /* DPMI code touches cpuemu prot */
      if (config.cpuemu > 1 && !CONFIG_CPUSIM && e_handle_pagefault(scp))
        return;
#endif
#endif
      signal_unblock_async_sigs();
      rc = vga_emu_fault(DOSADDR_REL(LINP(_cr2)), _err, scp);
      /* going for dpmi_fault() or deinit_handler(),
       * careful with async signals and sas_wa */
      signal_restore_async_sigs();
      if (rc == True)
        ret = dpmi_check_return();
    }
    if (_trapno == 0x10) {
      dbug_printf("coprocessor exception, calling IRQ13\n");
      print_exception_info(scp);
      pic_request(PIC_IRQ13);
      dpmi_return(scp, DPMI_RET_DOSEMU);
      return;
    }

    /* Not in dosemu code: dpmi_fault() will handle that */
    if (ret == DPMI_RET_FAULT)
      ret = dpmi_fault(scp);
    if (ret != DPMI_RET_CLIENT)
      dpmi_return(scp, ret);
    return;
  }

#ifdef X86_EMULATOR
  /* case 3 */
  if (config.cpuemu > 1) {
    /* Possibilities:
     * 1. Compiled code touches VGA prot
     * 2. Compiled code touches cpuemu prot
     * 3. Compiled code touches DPMI prot
     * 4. fullsim code touches DPMI prot
     * 5. dosemu code touches cpuemu prot (bug)
     * Compiled code means dpmi-jit, otherwise vm86 not here.
     */
    if (_trapno == 0x0e) {
      /* cases 1, 2, 3, 4 */
      if ((in_vm86 || config.cpuemu >= 4) && e_emu_pagefault(scp, !in_vm86))
        return;
      if (!CONFIG_CPUSIM && e_handle_pagefault(scp)) {
        /* case 5, any jit, bug */
        dosemu_error("touched jit-protected page%s\n",
                     in_vm86 ? " in vm86-emu" : "");
        return;
      }
    } else if ((in_vm86 || config.cpuemu >= 4) &&
               !CONFIG_CPUSIM && e_handle_fault(scp)) {
      /* compiled code can cause fault (usually DE, Divide Exception) */
      return;
    }
  }
#endif

  /* case 4 */
  error("Fault in dosemu code, in_dpmi=%i\n", dpmi_active());
  /* TODO - we can start gdb here */
  /* start_gdb() */
  /* Going to die from here */

bad:
/* All recovery attempts failed, going to die :( */

  {
#ifdef __x86_64__
    unsigned char *fsbase, *gsbase;
#endif
    error("cpu exception in dosemu code outside of %s!\n"
	  "sig: %i trapno: 0x%02x  errorcode: 0x%08x  cr2: 0x%08"PRI_RG"\n"
	  "eip: 0x%08"PRI_RG"  esp: 0x%08"PRI_RG"  eflags: 0x%08x\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n"
	  "fs: 0x%04x  gs: 0x%04x\n",
	  (in_dpmi_pm() ? "DPMI client" : "VM86()"),
	  signal, _trapno, _err, _cr2,
	  _rip, _rsp, _eflags, _cs, _ds, _es, _ss, _fs, _gs);
#ifdef __x86_64__
    dosemu_arch_prctl(ARCH_GET_FS, &fsbase);
    dosemu_arch_prctl(ARCH_GET_GS, &gsbase);
    error("@fsbase: %p gsbase: %p\n", fsbase, gsbase);
#endif
    error("@\n");

    error("Please install gdb, update dosemu from git, compile it with debug\n"
        "info and make a bug report with the content of ~/.dosemu/boot.log at\n"
"https://github.com/stsp/dosemu2/issues\n");
    error("@Please provide any additional info you can, like the test-cases,\n"
          "URLs and all the rest that fits.\n\n");
    gdb_debug();

    if (DPMIValidSelector(_cs))
      print_exception_info(scp);
    if (in_vm86)
	show_regs();
    fatalerr = 4;
    __leavedos_main(0, signal);		/* shouldn't return */
  }
}

/* noinline is to prevent gcc from moving TLS access around init_handler() */
__attribute__((noinline))
static void dosemu_fault0(int signal, sigcontext_t *scp)
{
  pthread_t tid;

  if (fault_cnt > 2) {
   /*
    * At this point we already tried leavedos(). Now try _exit()
    * and NOT exit(3), because glibc is probably malfunctions if
    * we are here.
    */
    _exit(255);
  }

  tid = pthread_self();
  if (!pthread_equal(tid, dosemu_pthread_self)) {
#ifdef __GLIBC__
    char name[128];
#endif
    /* disable cancellation to prevent main thread from terminating
     * this one due to SIGSEGV elsewhere while we are doing backtrace */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
#if defined(HAVE_PTHREAD_GETNAME_NP) && defined(__GLIBC__)
    pthread_getname_np(tid, name, sizeof(name));
    dosemu_error("thread %s got signal %i\n", name, signal);
#else
    dosemu_error("thread %i got signal %i\n", tid, signal);
#endif
    _exit(23);
    return;
  }

#ifdef __linux__
  if (kernel_version_code < KERNEL_VERSION(2, 6, 14)) {
    sigset_t set;

    /* this emulates SA_NODEFER, so that we can double fault.
       SA_NODEFER only works as documented in Linux kernels >= 2.6.14.
    */
    sigemptyset(&set);
    sigaddset(&set, signal);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
  }
#endif

  if (debug_level('g')>7)
    g_printf("Entering fault handler, signal=%i _trapno=0x%X\n",
      signal, _trapno);

  dosemu_fault1(signal, scp);

  if (debug_level('g')>8)
    g_printf("Returning from the fault handler\n");
}

#ifdef __linux__
SIG_PROTO_PFX
void dosemu_fault(int signal, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  /* need to call init_handler() before any syscall.
   * Additionally, TLS access should be done in a separate no-inline
   * function, so that gcc not to move the TLS access around init_handler(). */
  init_handler(scp, uct->uc_flags);
  fault_cnt++;
  dosemu_fault0(signal, scp);
  fault_cnt--;
  deinit_handler(scp, &uct->uc_flags);
}
#endif /* __linux__ */

/*
 * DANG_BEGIN_FUNCTION print_exception_info
 *
 * Prints information about an exception: exception number, error code,
 * address, reason, etc.
 *
 * DANG_END_FUNCTION
 *
 */
void print_exception_info(sigcontext_t *scp)
{
  int i;

  switch(_trapno)
    {
    case 0:
      error("@Division by zero\n");
      break;


    case 1:
      error("@Debug exception\n");
      break;


    case 3:
      error("@Breakpoint exception (caused by INT 3 instruction)\n");
      break;


    case 4:
      error("@Overflow exception (caused by INTO instruction)\n");
      break;


    case 5:
      error("@Bound exception (caused by BOUND instruction)\n");
      break;


    case 6: {
      unsigned char *csp;
      int ps = getpagesize();
      unsigned pa = _rip & (ps - 1);
      int sub = min(pa, 10);
      int sup = min(ps - pa, 10);
      error("@Invalid opcode\n");
      error("@Opcodes: ");
      csp = (unsigned char *) _rip - sub;
      for (i = 0; i < 10 - sub; i++)
        error("@XX ");
      for (i = 0; i < sub; i++)
	error("@%02x ", *csp++);
      error("@-> ");
      for (i = 0; i < sup; i++)
	error("@%02x ", *csp++);
      for (i = 0; i < 10 - sup; i++)
        error("@XX ");
      error("@\n");
      break;
    }

    case 7:
      error("@Coprocessor exception (coprocessor not available)\n");
      /* I'd like to print some info on the EM, MP, and TS flags in CR0,
       * but I don't know where I can get that information :-(
       * Anyway, this exception should not happen... (Erik Mouw)
       */
      break;


    case 8:
      error("@Double fault\n");
      break;


    case 9:
      error("@Coprocessor segment overflow\n");
      break;


    case 0xa:
      error("@Invalid TSS\n");
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xb:
      error("@Segment not available\n");
      /* This is the same code as case 0x0a; the compiler merges these
       * blocks, so I don't have to edit some dirty constructions to
       * generate one block of code. (Erik Mouw)
       */
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xc:
      error("@Stack exception\n");
      break;


    case 0xd:
      error("@General protection exception\n");
      /* This is the same code as case 0x0a; the compiler merges these
       * blocks, so I don't have to edit some dirty constructions to
       * generate one block of code. (Erik Mouw)
       */
      if(_err & 0x02)
	error("@IDT");
      else if(_err & 0x04)
	error("@LDT");
      else
	error("@GDT");

      error("@ selector: 0x%04x\n", ((_err >> 3) & 0x1fff ));

      if(_err & 0x01)
	error("@Exception was not caused by DOSEMU\n");
      else
	error("@Exception was caused by DOSEMU\n");
      break;


    case 0xe:
      error("@Page fault: ");
      if(_err & 0x02)
	error("@write");
      else
	error("@read");

      error("@ instruction to linear address: 0x%08"PRI_RG"\n", _cr2);

      error("@CPU was in ");
      if(_err & 0x04)
	error("@user mode\n");
      else
	error("@supervisor mode\n");

      error("@Exception was caused by ");
      if(_err & 0x01)
	error("@insufficient privilege\n");
      else
	error("@non-available page\n");
      break;

   case 0x10: {
      int i, n;
      unsigned short sw;
      fpregset_t p = __fpstate;
      error ("@Coprocessor Error:\n");
#ifdef __x86_64__
      error ("@cwd=%04x swd=%04x ftw=%04x\n", p->cwd, p->swd, p->ftw);
      error ("@cs:rip=%04x:%08lx ds:data=%04x:%08lx\n",	_cs,p->rip,_ds,p->rdp);
      sw = p->swd;
#else
      error ("@cw=%04x sw=%04x tag=%04x\n",
	     ((unsigned short)(p->cw)),((unsigned short)(p->sw)),
	((unsigned short)(p->tag)));
      error ("@cs:eip=%04x:%08x ds:data=%04x:%08x\n",
	     ((unsigned short)(p->cssel)),(unsigned)p->ipoff,
	     ((unsigned short)(p->datasel)),(unsigned)p->dataoff);
      sw = p->sw;
#endif
      if ((sw&0x80)==0) error("@No error summary bit,why?\n");
      else {
	if (sw&0x20) error("@Precision\n");
	if (sw&0x10) error("@Underflow\n");
	if (sw&0x08) error("@Overflow\n");
	if (sw&0x04) error("@Divide by 0\n");
	if (sw&0x02) error("@Denormalized\n");
	if ((sw&0x41)==0x01) error("@Invalid op\n");
	  else if ((sw&0x41)==0x41) error("@Stack fault\n");
      }
      n = (sw >> 11) & 7;
      for (i=0; i<8; i++) {
	unsigned short *r = p->_st[i].significand;
	unsigned short e = p->_st[i].exponent;
	error ("@fpr[%d] = %04x:%04x%04x%04x%04x\n",n,e,r[3],r[2],r[1],r[0]);
	n = (n+1) & 7;
      }
      } break;

   case 0x13: {
#ifdef __x86_64__
      int i;
      unsigned mxcsr;
      fpregset_t p = __fpstate;
      error ("@SIMD Floating-Point Exception:\n");
      mxcsr = p->mxcsr;
      error ("@mxcsr=%08x, mxcr_mask=%08x\n",mxcsr,(unsigned)(p->mxcr_mask));
      if (mxcsr&0x40) error("@Denormals are zero\n");
      if (mxcsr&0x20) error("@Precision\n");
      if (mxcsr&0x10) error("@Underflow\n");
      if (mxcsr&0x08) error("@Overflow\n");
      if (mxcsr&0x04) error("@Divide by 0\n");
      if (mxcsr&0x02) error("@Denormalized\n");
      if (mxcsr&0x01) error("@Invalid op\n");
      for (i=0; i<sizeof(p->_xmm)/sizeof(p->_xmm[0]); i++)
      {
	error ("@xmm[%d] = %08x:%08x:%08x:%08x\n",i,
	      (unsigned)p->_xmm[i].element[0], (unsigned)p->_xmm[i].element[1],
	      (unsigned)p->_xmm[i].element[2], (unsigned)p->_xmm[i].element[3]);
      }
#else
      error ("@SIMD Floating-Point Exception\n");
#endif
      break;
    }

    default:
      error("@Unknown exception\n");
      break;
    }
}
