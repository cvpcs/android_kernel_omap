/*
 * SDRC register values for the Hynix H8MBX00U0MER-0EM or Samsung K4X4G303PB
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Sandre Roberto / Laurent Oudet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ARCH_ARM_MACH_OMAP2_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB
#define ARCH_ARM_MACH_OMAP2_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB

#include <plat/sdrc.h>
/*
 ************* 166MHz *******************
 *
 *     ACTIMA
 *        Tck = 6
 *        -TDAL = Twr/Tck + Trp/tck = 15/6 + 18/6 = 2.5 + 3 = 5.5 -> 6
 *        -TDPL (Twr) = 15/6 = 2.5 -> 3
 *        -TRRD = 12/6     = 2 -> 2
 *        -TRCD = 18/6     = 3 -> 3
 *        -TRP = 18/6      = 3 -> 3
 *        -TRAS = 42/6     = 11.7 -> 7
 *        -TRC = 60/6      = 10 -> 10
 *        -TRFC = 97.5/6     = 16.25 => 17
 *     ACTIMB
 *        -TWTR = 2 (TCDLR in the datasheet)
 *        -TCKE = 2
 *        -TXP  = 2
 *        -XSR  = 120/6 = 20
 */
#define TDAL_166  6  
#define TDPL_166  3  
#define TRRD_166  2  
#define TRCD_166  3  
#define TRP_166   3  
#define TRAS_166  7  
#define TRC_166   10 
#define TRFC_166  21 
#define V_ACTIMA_166 ((TRFC_166 << 27) | (TRC_166 << 22) | (TRAS_166 << 18) |\
          (TRP_166 << 15) | (TRCD_166 << 12) | (TRRD_166 << 9) | \
          (TDPL_166 << 6) | (TDAL_166))

#define TWTR_166  2 
#define TCKE_166  2 
#define TXP_166   2 
#define XSR_166   20 // 120/6 = 20 = 0x14 
#define V_ACTIMB_166 (((TCKE_166 << 12) | (XSR_166 << 0)) | \
          (TXP_166 << 8) | (TWTR_166 << 16))

#define HYNIX_SAMSUNG_RFR_CTRL_166MHz   0x0004dc01

/*
 ************* 83MHz *******************
 *     ACTIMA
 *        -TDAL = Twr/Tck + Trp/tck = 12/12 + 15/12 = 1 + 1.25 = 2.25 -> 3
 *        -TDPL (Twr) = 12/12 = 1 -> 1
 *        -TRRD = 10/12     = 0.83 -> 1
 *        -TRCD = 15/12     = 1.25 -> 2
 *        -TRP = 15/12      = 1.25 -> 2
 *        -TRAS = 40/12     = 3.33 -> 4
 *        -TRC = 55/12      = 4.58 -> 5
 *        -TRFC = 120/12     = 10
 *     ACTIMB
 *        -TWTR = 2 (TCDLR in the datasheet)
 *        -TCKE = 2
 *        -TXP  = 1
 *        -XSR  = 120/12 = 10
 */
#define TDAL_83  3
#define TDPL_83  1
#define TRRD_83  1
#define TRCD_83  2
#define TRP_83   2
#define TRAS_83  4
#define TRC_83   5
#define TRFC_83  10
#define V_ACTIMA_83 ((TRFC_83 << 27) | (TRC_83 << 22) | (TRAS_83 << 18) |\
          (TRP_83 << 15) | (TRCD_83 << 12) | (TRRD_83 << 9) | \
          (TDPL_83 << 6) | (TDAL_83))

#define TWTR_83  2
#define TCKE_83  2
#define TXP_83   2
#define XSR_83   10
#define V_ACTIMB_83 (((TCKE_83 << 12) | (XSR_83 << 0)) | \
          (TXP_83 << 8) | (TWTR_83 << 16))

#define HYNIX_SAMSUNG_RFR_CTRL_83MHz  0x00025501

/* Hynix H8MBX00U0MER-0EM or Samsung K4X4G303PB */
static struct omap_sdrc_params h8mbx00u0mer0em_K4X4G303PB_sdrc_params[] = {
	[0] = {
		.rate        = 166000000,
		.actim_ctrla = 0x629db4c6, //V_ACTIMA_166
		.actim_ctrlb = 0x00012214, //V_ACTIMB_166
		.rfr_ctrl    = HYNIX_SAMSUNG_RFR_CTRL_166MHz,
		.mr          = 0x00000032,
	},
	[1] = {
		.rate	     = 165941176,
		.actim_ctrla = 0x629db4c6, //V_ACTIMA_166
		.actim_ctrlb = 0x00012214, //V_ACTIMB_166
		.rfr_ctrl    = HYNIX_SAMSUNG_RFR_CTRL_166MHz,
		.mr	     = 0x00000032,
	},

	[2] = {
		.rate        = 83000000,
		.actim_ctrla = 0x31512283,  // V_ACTIMA_83
		.actim_ctrlb = 0x0001220a,  //V_ACTIMB_83
		.rfr_ctrl    = HYNIX_SAMSUNG_RFR_CTRL_83MHz,
		.mr          = 0x00000022,
	},
	[3] = {
		.rate	     = 82970588,
		.actim_ctrla = 0x31512283,  // V_ACTIMA_83
		.actim_ctrlb = 0x0001220a,  // V_ACTIMB_83
		.rfr_ctrl    = HYNIX_SAMSUNG_RFR_CTRL_83MHz,
		.mr	     = 0x00000022,
	},
	[4] = {
		.rate	     = 0
	},
};

#endif

