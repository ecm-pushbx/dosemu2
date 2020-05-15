/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#ifndef _EMU86_SYNCPU_H
#define _EMU86_SYNCPU_H

/***************************************************************************/

#include "host.h"
#include "protmode.h"
#include <limits.h>
#include <stddef.h>

#if ULONG_MAX > 0xffffffffUL
#define PADDING32BIT(n)
#else
#define PADDING32BIT(n) unsigned int padding##n;
#endif

typedef struct {
/* offsets are 8-bit signed */
#define FIELD0		unprotect_stub	/* field of SynCPU at offset 00 */
/* ------------------------------------------------ */
/*80*/  long double   *fpregs;
/*84*/  PADDING32BIT(1)
/*88*/	unsigned long long EMUtime;
/*90*/	SDTR gs_cache;
/*9c*/	SDTR fs_cache;
/*a8*/	SDTR es_cache;
/*b4*/	SDTR ds_cache;
/*c0*/	SDTR cs_cache;
/*cc*/	SDTR ss_cache;
/* ------------------------------------------------ */
/*d8*/  void (*stub_stk_16)(void);
/*dc*/  PADDING32BIT(2)
/*e0*/  void (*stub_stk_32)(void);
/*e4*/  PADDING32BIT(3)
/*e8*/  void (*stub_wri_8)(void);
/*ec*/  PADDING32BIT(4)
/*f0*/  void (*stub_wri_16)(void);
/*f4*/  PADDING32BIT(5)
/*f8*/  void (*stub_wri_32)(void);
/*fc*/  PADDING32BIT(6)
/* ------------------------------------------------ */
/*00*/  void (*unprotect_stub)(void); /* must be at 0 for call (%ebx) */
/*04*/  PADDING32BIT(7)
/*08*/	unsigned int rzero;
/*0c*/	unsigned short gs, __gsh;
/*10*/	unsigned short fs, __fsh;
/*14*/	unsigned short es, __esh;
/*18*/	unsigned short ds, __dsh;
/*1c*/	unsigned int edi;
/*20*/	unsigned int esi;
/*24*/	unsigned int ebp;
/*28*/	unsigned int esp;
/*2c*/	unsigned int ebx;
/*30*/	unsigned int edx;
/*34*/	unsigned int ecx;
/*38*/	unsigned int eax;
/*3c*/	unsigned int trapno;
/*40*/	unsigned int scp_err;
/*44*/	unsigned int eip;
/*48*/	unsigned short cs, __csh;
/*4c*/	unsigned int eflags;
/* ----end of i386 sigcontext 1:1 correspondence--- */
/*50*/	unsigned short ss, __ssh;
/*54*/	unsigned int cr2;
/* ------------------------------------------------ */
/*58*/	unsigned short fpuc, fpus;
/*5c*/	unsigned short fpstt, fptag;
/* ------------------------------------------------ */
/*60*/	unsigned short sigalrm_pending, sigprof_pending;
/*64*/	unsigned int StackMask;
/*68*/ 	unsigned int mem_base;
/*6c*/ 	unsigned int df_increments; /* either 0x040201 or 0xfcfeff */
	/* begin of cr array */
/*70*/	unsigned int cr[5]; /* only cr[0] is used in compiled code */
/* ------------------------------------------------ */
/*80*/	//unsigned int end_mark[0] = cr[4]
	unsigned int tr[2];

	int err;
	unsigned int mode;
	unsigned int sreg1;
	unsigned int dreg1;
	unsigned int xreg1;

/*
 * DR0-3 = linear address of breakpoint 0-3
 * DR4=5 = reserved
 * DR6	b0-b3 = BP active
 *	b13   = BD
 *	b14   = BS
 *	b15   = BT
 * DR7	b0-b1 = G:L bp#0
 *	b2-b3 = G:L bp#1
 *	b4-b5 = G:L bp#2
 *	b6-b7 = G:L bp#3
 *	b8-b9 = GE:LE
 *	b13   = GD
 *	b16-19= LLRW bp#0	LL=00(1),01(2),11(4)
 *	b20-23= LLRW bp#1	RW=00(x),01(w),11(rw)
 *	b24-27= LLRW bp#2
 *	b28-31= LLRW bp#3
 */
	unsigned int dr[8];
	unsigned int mem_ref;
/* CPU register: base(32) limit(16) */
	DTR  GDTR;
/* CPU register: base(32) limit(16) */
	DTR  IDTR;
/* CPU register: sel(16) base(32) limit(16) attr(8) */
	unsigned short LDT_SEL;
	DTR  LDTR;
/* CPU register: sel(16) base(32) limit(16) attr(8) */
	unsigned short TR_SEL;
	DTR  TR;

	/* if not NULL, points to emulated FPU state
	   if NULL, emulator uses FPU instructions, so flags that
	   dosemu needs to restore its own FPU environment. */
	fpregset_t fpstate;

	void (*stub_read_8)(void);
	void (*stub_read_16)(void);
	void (*stub_read_32)(void);
} SynCPU;

union _SynCPU {
	SynCPU s;
	unsigned char b[sizeof(SynCPU)];
	unsigned short w[sizeof(SynCPU)/2];
	unsigned int d[sizeof(SynCPU)/4];
};

extern union _SynCPU TheCPU_union;
#define TheCPU TheCPU_union.s

#define SCBASE		offsetof(SynCPU,FIELD0)
#define Ofs_END		(int)(offsetof(SynCPU,cr[4])-SCBASE)

#define SC(o) ((signed char)(o))
#define CPUOFFS(o)	(((unsigned char *)&(TheCPU.FIELD0))+SC(o))

#define CPUBYTE(o)	TheCPU_union.b[SCBASE+SC(o)]
#define CPUWORD(o)	TheCPU_union.w[(SCBASE+SC(o))/2]
#define CPULONG(o)	TheCPU_union.d[(SCBASE+SC(o))/4]

#define rEAX		TheCPU.eax
#define Ofs_EAX		(unsigned char)(offsetof(SynCPU,eax)-SCBASE)
#define rECX		TheCPU.ecx
#define Ofs_ECX		(unsigned char)(offsetof(SynCPU,ecx)-SCBASE)
#define rEDX		TheCPU.edx
#define Ofs_EDX		(unsigned char)(offsetof(SynCPU,edx)-SCBASE)
#define rEBX		TheCPU.ebx
#define Ofs_EBX		(unsigned char)(offsetof(SynCPU,ebx)-SCBASE)
#define rESP		TheCPU.esp
#define Ofs_ESP		(unsigned char)(offsetof(SynCPU,esp)-SCBASE)
#define rEBP		TheCPU.ebp
#define Ofs_EBP		(unsigned char)(offsetof(SynCPU,ebp)-SCBASE)
#define rESI		TheCPU.esi
#define Ofs_ESI		(unsigned char)(offsetof(SynCPU,esi)-SCBASE)
#define rEDI		TheCPU.edi
#define Ofs_EDI		(unsigned char)(offsetof(SynCPU,edi)-SCBASE)
#define Ofs_EIP		(unsigned char)(offsetof(SynCPU,eip)-SCBASE)

#define Ofs_CS		(unsigned char)(offsetof(SynCPU,cs)-SCBASE)
#define Ofs_DS		(unsigned char)(offsetof(SynCPU,ds)-SCBASE)
#define Ofs_ES		(unsigned char)(offsetof(SynCPU,es)-SCBASE)
#define Ofs_SS		(unsigned char)(offsetof(SynCPU,ss)-SCBASE)
#define Ofs_FS		(unsigned char)(offsetof(SynCPU,fs)-SCBASE)
#define Ofs_GS		(unsigned char)(offsetof(SynCPU,gs)-SCBASE)
#define Ofs_EFLAGS	(unsigned char)(offsetof(SynCPU,eflags)-SCBASE)
#define Ofs_CR0		(unsigned char)(offsetof(SynCPU,cr[0])-SCBASE)
#define Ofs_CR2		(unsigned char)(offsetof(SynCPU,cr2)-SCBASE)
#define Ofs_STACKM	(unsigned char)(offsetof(SynCPU,StackMask)-SCBASE)
#define Ofs_ETIME	(unsigned char)(offsetof(SynCPU,EMUtime)-SCBASE)
#define Ofs_RZERO	(unsigned char)(offsetof(SynCPU,rzero)-SCBASE)
#define Ofs_SIGAPEND	(unsigned char)(offsetof(SynCPU,sigalrm_pending)-SCBASE)
#define Ofs_SIGFPEND	(unsigned char)(offsetof(SynCPU,sigprof_pending)-SCBASE)
#define Ofs_MEMBASE	(unsigned char)(offsetof(SynCPU,mem_base)-SCBASE)
#define Ofs_DF_INCREMENTS (unsigned char)(offsetof(SynCPU,df_increments)-SCBASE)

#define Ofs_FPR		(unsigned char)(offsetof(SynCPU,fpregs)-SCBASE)
#define Ofs_FPSTT	(unsigned char)(offsetof(SynCPU,fpstt)-SCBASE)
#define Ofs_FPUS	(unsigned char)(offsetof(SynCPU,fpus)-SCBASE)
#define Ofs_FPUC	(unsigned char)(offsetof(SynCPU,fpuc)-SCBASE)
#define Ofs_FPTAG	(unsigned char)(offsetof(SynCPU,fptag)-SCBASE)

// 'Base' is 1st field of xs_cache
#define Ofs_XDS		(unsigned char)(offsetof(SynCPU,ds_cache)-SCBASE)
#define Ofs_XSS		(unsigned char)(offsetof(SynCPU,ss_cache)-SCBASE)
#define Ofs_XES		(unsigned char)(offsetof(SynCPU,es_cache)-SCBASE)
#define Ofs_XCS		(unsigned char)(offsetof(SynCPU,cs_cache)-SCBASE)
#define Ofs_XFS		(unsigned char)(offsetof(SynCPU,fs_cache)-SCBASE)
#define Ofs_XGS		(unsigned char)(offsetof(SynCPU,gs_cache)-SCBASE)

#define Ofs_stub_wri_8	(unsigned char)(offsetof(SynCPU,stub_wri_8)-SCBASE)
#define Ofs_stub_wri_16	(unsigned char)(offsetof(SynCPU,stub_wri_16)-SCBASE)
#define Ofs_stub_wri_32	(unsigned char)(offsetof(SynCPU,stub_wri_32)-SCBASE)
#define Ofs_stub_stk_16	(unsigned char)(offsetof(SynCPU,stub_stk_16)-SCBASE)
#define Ofs_stub_stk_32	(unsigned char)(offsetof(SynCPU,stub_stk_32)-SCBASE)
#define Ofs_stub_read_8	(unsigned int)(offsetof(SynCPU,stub_read_8)-SCBASE)
#define Ofs_stub_read_16	(unsigned int)(offsetof(SynCPU,stub_read_16)-SCBASE)
#define Ofs_stub_read_32	(unsigned int)(offsetof(SynCPU,stub_read_32)-SCBASE)

#define rAX		CPUWORD(Ofs_AX)
#define Ofs_AX		(Ofs_EAX)
#define Ofs_AXH		(Ofs_EAX+2)
#define rAL		CPUBYTE(Ofs_AL)
#define Ofs_AL		(Ofs_EAX)
#define rAH		CPUBYTE(Ofs_AH)
#define Ofs_AH		(Ofs_EAX+1)
#define rCX		CPUWORD(Ofs_CX)
#define Ofs_CX		(Ofs_ECX)
#define rCL		CPUBYTE(Ofs_CL)
#define Ofs_CL		(Ofs_ECX)
#define rCH		CPUBYTE(Ofs_CH)
#define Ofs_CH		(Ofs_ECX+1)
#define rDX		CPUWORD(Ofs_DX)
#define Ofs_DX		(Ofs_EDX)
#define rDL		CPUBYTE(Ofs_DL)
#define Ofs_DL		(Ofs_EDX)
#define rDH		CPUBYTE(Ofs_DH)
#define Ofs_DH		(Ofs_EDX+1)
#define rBX		CPUWORD(Ofs_BX)
#define Ofs_BX		(Ofs_EBX)
#define rBL		CPUBYTE(Ofs_BL)
#define Ofs_BL		(Ofs_EBX)
#define rBH		CPUBYTE(Ofs_BH)
#define Ofs_BH		(Ofs_EBX+1)
#define rSP		CPUWORD(Ofs_SP)
#define Ofs_SP		(Ofs_ESP)
#define rBP		CPUWORD(Ofs_BP)
#define Ofs_BP		(Ofs_EBP)
#define rSI		CPUWORD(Ofs_SI)
#define Ofs_SI		(Ofs_ESI)
#define rDI	        CPUWORD(Ofs_DI)
#define Ofs_DI		(Ofs_EDI)
#define Ofs_FLAGS	(Ofs_EFLAGS)
#define Ofs_FLAGSL	(Ofs_EFLAGS)

#define REG1		TheCPU.sreg1
#define REG3		TheCPU.dreg1
#define SBASE		TheCPU.xreg1
#define SIGAPEND	TheCPU.sigalrm_pending
#define SIGFPEND	TheCPU.sigprof_pending
#define MEMREF		TheCPU.mem_ref
#define EFLAGS		TheCPU.eflags
#define FLAGS		CPUWORD(Ofs_EFLAGS)
#define FPX		TheCPU.fpstt

#define CS_DTR		TheCPU.cs_cache
#define DS_DTR		TheCPU.ds_cache
#define ES_DTR		TheCPU.es_cache
#define SS_DTR		TheCPU.ss_cache
#define FS_DTR		TheCPU.fs_cache
#define GS_DTR		TheCPU.gs_cache

#define LONG_CS		(TheCPU.cs_cache.BoundL - TheCPU.mem_base)
#define LONG_DS		(TheCPU.ds_cache.BoundL - TheCPU.mem_base)
#define LONG_ES		(TheCPU.es_cache.BoundL - TheCPU.mem_base)
#define LONG_SS		(TheCPU.ss_cache.BoundL - TheCPU.mem_base)
#define LONG_FS		(TheCPU.fs_cache.BoundL - TheCPU.mem_base)
#define LONG_GS		(TheCPU.gs_cache.BoundL - TheCPU.mem_base)

extern char OVERR_DS, OVERR_SS;

#endif
