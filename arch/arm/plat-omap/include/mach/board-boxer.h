/*
 * arch/arm/plat-omap/include/mach/board-boxer.h
 *
 * Hardware definitions for TI OMAP3 LDP.
 *
 * Copyright (C) 2008 Texas Instruments Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ASM_ARCH_OMAP_BOXER_H
#define __ASM_ARCH_OMAP_BOXER_H

extern void ldp_flash_init(void);
extern void twl4030_bci_battery_init(void);
extern unsigned get_last_off_on_transaction_id(struct device *dev);

#define TWL4030_IRQNUM		INT_34XX_SYS_NIRQ
#define LDP3430_NAND_CS		0

#define OMAP3_WAKEUP (PRCM_WAKEUP_T2_KEYPAD |\
			PRCM_WAKEUP_TOUCHSCREEN | PRCM_WAKEUP_UART)

#ifdef CONFIG_BATTERY_MAX17042
#define MAX17042_GPIO_FOR_IRQ			100
#endif

/*addition of MAXIM8903/TI GPIO mapping WRT schematics */
#ifdef CONFIG_CHARGER_MAX8903
#define MAX8903_UOK_GPIO_FOR_IRQ 115
#define MAX8903_DOK_GPIO_FOR_IRQ 114      
#define MAX8903_GPIO_CHG_EN      110
#define MAX8903_GPIO_CHG_STATUS  111
#define MAX8903_GPIO_CHG_FLT     101
#define MAX8903_GPIO_CHG_IUSB    102
#define MAX8903_GPIO_CHG_USUS    104
#define MAX8903_GPIO_CHG_ILM     61
#endif

#define BOARD_ENCORE_REV_EVT1A      0x1
#define BOARD_ENCORE_REV_EVT1B      0x2
#define BOARD_ENCORE_REV_EVT2       0x3
#define BOARD_ENCORE_REV_DVT        0x4
#define BOARD_ENCORE_REV_PVT        0x5
#define BOARD_ENCORE_REV_UNKNOWN    0x6

static inline int is_encore_board_evt2(void)
{
    return (system_rev >= BOARD_ENCORE_REV_EVT2);
}

static inline int is_encore_board_evt1b(void)
{
    return (system_rev == BOARD_ENCORE_REV_EVT1B);
}

#define BN_USB_VENDOR_ID            0x2080
#define BN_USB_PRODUCT_ID_ENCORE    0x0002

#endif /* __ASM_ARCH_OMAP_BOXER_H */
