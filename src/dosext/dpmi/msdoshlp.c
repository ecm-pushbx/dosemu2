/*
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: glue between msdos.c and the rest of dosemu
 * This is needed to keep msdos.c portable to djgpp.
 *
 * Author: Stas Sergeev
 *
 * Currently there are only helper stubs here.
 * The helpers itself are in bios.S.
 * TODO: port bios.S asm helpers to C and put here
 */

#include "emu.h"
#include "cpu.h"
#include "utilities.h"
#include "int.h"
#include "hlt.h"
#include "coopth.h"
#include "dpmi.h"
#include "dpmisel.h"
#include "msdoshlp.h"
#include <assert.h>


struct msdos_ops {
    void (*api_call)(struct sigcontext *scp);
    void (*xms_call)(struct RealModeCallStructure *rmreg);
    int (*mouse_callback)(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
    int (*ps2_mouse_callback)(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
    void (*rmcb_handler)(struct RealModeCallStructure *rmreg);
};
static struct msdos_ops msdos;

struct exec_helper_s {
    int initialized;
    int tid;
    far_t entry;
    far_t s_r;
    u_char len;
};
static struct exec_helper_s exec_helper;

static void lrhlp_setup(far_t rmcb)
{
#define MK_LR_OFS(ofs) ((long)(ofs)-(long)MSDOS_lr_start)
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_ip)), rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_cs)), rmcb.segment);
}

static void lwhlp_setup(far_t rmcb)
{
#define MK_LW_OFS(ofs) ((long)(ofs)-(long)MSDOS_lw_start)
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_ip)),
	       rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_cs)),
	       rmcb.segment);
}

static void s_r_call(u_char al, u_short es, u_short di)
{
    u_short saved_ax = LWORD(eax), saved_es = REG(es), saved_di = LWORD(edi);

    LO(ax) = al;
    REG(es) = es;
    LWORD(edi) = di;
    do_call_back(exec_helper.s_r.segment, exec_helper.s_r.offset);
    LWORD(eax) = saved_ax;
    REG(es) = saved_es;
    LWORD(edi) = saved_di;
}

static void exechlp_thr(void *arg)
{
    u_short saved_flags;

    assert(LWORD(esp) >= exec_helper.len);
    LWORD(esp) -= exec_helper.len;
    s_r_call(0, REG(ss), LWORD(esp));
    do_int_call_back(0x21);
    saved_flags = LWORD(eflags);
    s_r_call(1, REG(ss), LWORD(esp));
    LWORD(eflags) = saved_flags;
    LWORD(esp) += exec_helper.len;
}

static void exechlp_hlt(Bit16u off, void *arg)
{
    fake_iret();
    coopth_start(exec_helper.tid, exechlp_thr, NULL);
}

static void exechlp_setup(void)
{
    struct pmaddr_s pma;
    exec_helper.len = DPMI_get_save_restore_address(&exec_helper.s_r, &pma);
    if (!exec_helper.initialized) {
	emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
	hlt_hdlr.name = "msdos exec";
	hlt_hdlr.func = exechlp_hlt;
	exec_helper.entry.offset = hlt_register_handler(hlt_hdlr);
	exec_helper.entry.segment = BIOS_HLT_BLK_SEG;
	exec_helper.tid = coopth_create("msdos exec thr");
	exec_helper.initialized = 1;
    }
}

far_t allocate_realmode_callback(void (*handler)(
	struct RealModeCallStructure *))
{
    msdos.rmcb_handler = handler;
    return DPMI_allocate_realmode_callback(dpmi_sel(),
	    DPMI_SEL_OFF(MSDOS_rmcb_call), dpmi_data_sel(),
	    DPMI_DATA_OFF(MSDOS_rmcb_data));
}

int free_realmode_callback(u_short seg, u_short off)
{
    return DPMI_free_realmode_callback(seg, off);
}

struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(struct sigcontext *))
{
    struct pmaddr_s ret;
    switch (id) {
    case API_CALL:
	msdos.api_call = handler;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_API_call);
	break;
    default:
	dosemu_error("unknown pm handler\n");
	ret = (struct pmaddr_s){ 0, 0 };
	break;
    }
    return ret;
}

struct pmaddr_s get_pmrm_handler(enum MsdOpIds id, void (*handler)(
	struct RealModeCallStructure *))
{
    struct pmaddr_s ret;
    switch (id) {
    case XMS_CALL:
	msdos.xms_call = handler;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_XMS_call);
	break;
    default:
	dosemu_error("unknown pmrm handler\n");
	ret = (struct pmaddr_s){ 0, 0 };
	break;
    }
    return ret;
}

far_t get_rm_handler(enum MsdOpIds id, int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *))
{
    far_t ret;
    switch (id) {
    case MOUSE_CB:
	msdos.mouse_callback = handler;
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_mouse_callback);
	break;
    case PS2MOUSE_CB:
	msdos.ps2_mouse_callback = handler;
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_PS2_mouse_callback);
	break;
    default:
	dosemu_error("unknown rm handler\n");
	ret = (far_t){ 0, 0 };
	break;
    }
    return ret;
}

far_t get_lr_helper(far_t rmcb)
{
    lrhlp_setup(rmcb);
    return (far_t){ .segment = DOS_LONG_READ_SEG,
	    .offset = DOS_LONG_READ_OFF };
}

far_t get_lw_helper(far_t rmcb)
{
    lwhlp_setup(rmcb);
    return (far_t){ .segment = DOS_LONG_WRITE_SEG,
	    .offset = DOS_LONG_WRITE_OFF };
}

far_t get_exec_helper(void)
{
    exechlp_setup();
    return exec_helper.entry;
}

void msdos_pm_call(struct sigcontext *scp, int is_32)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_call)) {
	msdos.api_call(scp);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call)) {
	struct RealModeCallStructure *rmreg = SEL_ADR_CLNT(_es, _edi, is_32);
	msdos.rmcb_handler(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
    }
}

int msdos_pre_pm(struct sigcontext *scp,
		 struct RealModeCallStructure *rmreg)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_XMS_call)) {
	msdos.xms_call(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
	return 0;
    }
    return 1;
}

int msdos_pre_rm(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg)
{
    int ret = 0;
    unsigned int lina = SEGOFF2LINEAR(rmreg->cs, rmreg->ip) - 1;

    if (lina == DPMI_ADD + HLT_OFF(MSDOS_mouse_callback))
	ret = msdos.mouse_callback(scp, rmreg);
    else if (lina == DPMI_ADD + HLT_OFF(MSDOS_PS2_mouse_callback))
	ret = msdos.ps2_mouse_callback(scp, rmreg);

    return ret;
}