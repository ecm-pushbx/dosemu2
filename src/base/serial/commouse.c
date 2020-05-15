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
 * mouse.com backend
 *
 * FIXME: only supporting MS protocol (M, M3) for now
 *
 * Author: Stas Sergeev
 */
#include <string.h>
#include "emu.h"
#include "init.h"
#include "hlt.h"
#include "int.h"
#include "port.h"
#include "coopth.h"
#include "ser_defs.h"

#define read_reg(num, offset) do_serial_in((num), com_cfg[(num)].base_port + (offset))
#define read_char(num)        read_reg((num), UART_RX)
#define read_LCR(num)         read_reg((num), UART_LCR)
#define read_LSR(num)         read_reg((num), UART_LSR)
#define read_IIR(num)         read_reg((num), UART_IIR)
#define write_reg(num, offset, byte) do_serial_out((num), com_cfg[(num)].base_port + (offset), (byte))
#define write_char(num, byte) write_reg((num), UART_TX, (byte))
#define write_DLL(num, byte)  write_reg((num), UART_DLL, (byte))
#define write_DLM(num, byte)  write_reg((num), UART_DLM, (byte))
#define write_FCR(num, byte)  write_reg((num), UART_FCR, (byte))
#define write_LCR(num, byte)  write_reg((num), UART_LCR, (byte))
#define write_MCR(num, byte)  write_reg((num), UART_MCR, (byte))
#define write_IER(num, byte)  write_reg((num), UART_IER, (byte))

#define _com_num config.mouse.com
static u_short irq_hlt;
static void com_irq(Bit16u idx, void *arg);

static int com_mouse_init(void)
{
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
  int num;
  if (config.mouse.com_num == -1 || !config.mouse.intdrv)
    return 0;
  for (num = 0; num < config.num_ser; num++) {
    if (com_cfg[num].real_comport == config.mouse.com_num)
      break;
  }
  if (num >= config.num_ser)
    return 0;

  _com_num = num;
  hlt_hdlr.name       = "commouse isr";
  hlt_hdlr.func       = com_irq;
  irq_hlt = hlt_register_handler(hlt_hdlr);

  return 1;
}

static void com_irq(Bit16u idx, void *arg)
{
  uint8_t iir, val;
  s_printf("COMMOUSE: got irq\n");
  iir = read_IIR(_com_num);
  switch (iir & UART_IIR_CND_MASK) {
    case UART_IIR_NO_INT:
      break;
    case UART_IIR_RDI:
      while (read_LSR(_com_num) & UART_LSR_DR) {
        val = read_char(_com_num);
        /*
         * talk to int33 explicitly as we dont want to talk
         * to for example sermouse.c
         */
        DOSEMUMouseProtocol(&val, 1, MOUSE_MS3BUTTON, "int33 mouse");
      }
      break;
    default:
      s_printf("COMMOUSE: unexpected interrupt cond %#x\n", iir);
      break;
  }

  do_eoi_iret();
}

static int get_char(int num)
{
  LWORD(edx) = com_cfg[_com_num].real_comport - 1;
  HI(ax) = 2;
  LO(ax) = 0;
  do_int_call_back(0x14);
  if (HI(ax) & 0x80)
    return -1;
  return LO(ax);
}

static Bit8u get_lsr(int num)
{
  LWORD(edx) = com_cfg[num].real_comport - 1;
  HI(ax) = 3;
  LO(ax) = 0;
  do_int_call_back(0x14);
  return HI(ax);
}

static int com_mouse_reset(void)
{
  #define MAX_RD 20
  int ch, i;
  uint8_t imr, imr1;
  char buf[3];
  struct vm86_regs saved_regs;

  if (_com_num == -1)
    return -1;
  write_IER(_com_num, 0);

  saved_regs = REGS;
  LWORD(edx) = com_cfg[_com_num].real_comport - 1;
  HI(ax) = 0;	// init port
  LO(ax) = 0x80 | UART_LCR_WLEN8;	// 1200, 8N1
  do_int_call_back(0x14);

  write_MCR(_com_num, UART_MCR_DTR);
  for (i = 0; i < MAX_RD; i++) {
    set_IF();
    coopth_wait();
    clear_IF();
    if (!(get_lsr(_com_num) & UART_LSR_DR))
      break;
    get_char(_com_num);	// read out everything
  }
  if (i == MAX_RD) {
    s_printf("COMMOUSE: error, reading junk\n");
    goto out_err;
  }
  write_MCR(_com_num, UART_MCR_DTR | UART_MCR_RTS);
  set_IF();
  coopth_wait();
  clear_IF();
  if (get_lsr(_com_num) & UART_LSR_FE)
    get_char(_com_num);
  for (i = 0; i < 2; i++) {
    ch = get_char(_com_num);
    if (ch == -1)
      break;
    buf[i] = ch;
  }
  buf[i] = '\0';
  REGS = saved_regs;
  if (buf[0] != 'M') {
    s_printf("COMMOUSE: unsupported ID %s\n", buf);
    return -1;
  }

  SETIVEC(com[_com_num].interrupt, BIOS_HLT_BLK_SEG, irq_hlt);
  write_MCR(_com_num, com[_com_num].MCR | UART_MCR_OUT2);
  imr = imr1 = port_inb(0x21);
  imr &= ~(1 << com_cfg[_com_num].irq);
  if (imr != imr1)
    port_outb(0x21, imr);
  write_IER(_com_num, UART_IER_RDI);
  return 0;

out_err:
  REGS = saved_regs;
  return -1;
}

static void com_mouse_post_init(void)
{
  if (_com_num == -1)
    return;
  mouse_enable_native_cursor_id(1, "int33 mouse");
  com[_com_num].ivec.segment = ISEG(com[_com_num].interrupt);
  com[_com_num].ivec.offset = IOFF(com[_com_num].interrupt);
  com_mouse_reset();
}

static struct mouse_client com_mouse =  {
  "com mouse",		/* name */
  com_mouse_init,	/* init */
  NULL,			/* close */
  NULL,
  com_mouse_post_init,
  com_mouse_reset,
};

CONSTRUCTOR(static void com_mouse_register(void))
{
  register_mouse_client(&com_mouse);
}
