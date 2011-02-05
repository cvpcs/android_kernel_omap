/*
 * linux/arch/arm/mach-omap2/board-ldp-flash.c
 *
 * Copyright (C) 2008 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-2430sdp-flash.c
 * Author: Rohit Choraria <rohitkc@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/types.h>
#include <linux/io.h>

#include <asm/mach/flash.h>
#include <plat/board.h>
#include <plat/gpmc.h>
#include <plat/nand.h>

#if defined(CONFIG_MACH_OMAP_ZOOM2) || defined(CONFIG_MACH_OMAP_ZOOM3)
#include <plat/board-zoom2.h>
#elif defined(CONFIG_MACH_OMAP3630_EDP1) || defined(CONFIG_MACH_OMAP3621_EDP1) || defined(CONFIG_MACH_OMAP3621_BOXER) || defined(CONFIG_MACH_OMAP3621_EVT1A)
#include <plat/board-edp1.h>
#else
#include <plat/board-ldp.h>
#endif

#define GPMC_CS0_BASE	0x60
#define GPMC_CS_SIZE	0x30

#ifdef CONFIG_OMAP3_PM
/*
 * Number of frequencies supported by gpmc
 */
#define NO_GPMC_FREQ_SUPPORTED		2
#define SUPPORTED_FREQ1			83
#define SUPPORTED_FREQ2			166

/*
 * TBD: Get optimized NAND setting for 83MHz
 *      Get 133/66MHz timings.
 */

struct gpmc_cs_config pdc_nand_gpmc_setting[] = {
	{0x1800, 0x00030300, 0x00030200, 0x03000400, 0x00040505, 0x030001C0},
	{0x1800, 0x00141400, 0x00141400, 0x0f010f01, 0x010c1414, 0x1f0f0a80}
};

/* ethernet goes crazy if differt times are used */
struct gpmc_cs_config enet_gpmc_setting[] = {
	{0x611200, 0x001F1F01, 0x00080803, 0x1D091D09, 0x041D1F1F, 0x1D0904C4},
	{0x611200, 0x001F1F01, 0x00080803, 0x1D091D09, 0x041D1F1F, 0x1D0904C4}
};

/*
 * Structure containing the gpmc cs values at different frequencies
 * This structure will be populated run time depending on the
 * values read from FPGA registers..On 3430 SDP FPGA is always on CS3
 */
struct gpmc_freq_config freq_config[NO_GPMC_FREQ_SUPPORTED];
#endif

#define NAND_CMD_UNLOCK1	0x23
#define NAND_CMD_UNLOCK2	0x24
/**
 * @brief platform specific unlock function
 *
 * @param mtd - mtd info
 * @param ofs - offset to start unlock from
 * @param len - length to unlock
 *
 * @return - unlock status
 */
static int omap_zoom2_nand_unlock(struct mtd_info *mtd, loff_t ofs, size_t len)
{
	int ret = 0;
	int chipnr;
	int status;
	unsigned long page;
	struct nand_chip *this = mtd->priv;
	printk(KERN_INFO "nand_unlock: start: %08x, length: %d!\n",
			(int)ofs, (int)len);

	/* select the NAND device */
	chipnr = (int)(ofs >> this->chip_shift);
	this->select_chip(mtd, chipnr);
	/* check the WP bit */
	this->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
	if ((this->read_byte(mtd) & 0x80) == 0) {
		printk(KERN_ERR "nand_unlock: Device is write protected!\n");
		ret = -EINVAL;
		goto out;
	}

	if ((ofs & (mtd->writesize - 1)) != 0) {
		printk(KERN_ERR "nand_unlock: Start address must be"
				"beginning of nand page!\n");
		ret = -EINVAL;
		goto out;
	}

	if (len == 0 || (len & (mtd->writesize - 1)) != 0) {
		printk(KERN_ERR "nand_unlock: Length must be a multiple of "
				"nand page size!\n");
		ret = -EINVAL;
		goto out;
	}

	/* submit address of first page to unlock */
	page = (unsigned long)(ofs >> this->page_shift);
	this->cmdfunc(mtd, NAND_CMD_UNLOCK1, -1, page & this->pagemask);

	/* submit ADDRESS of LAST page to unlock */
	page += (unsigned long)((ofs + len) >> this->page_shift) ;
	this->cmdfunc(mtd, NAND_CMD_UNLOCK2, -1, page & this->pagemask);

	/* call wait ready function */
	status = this->waitfunc(mtd, this);
	udelay(1000);
	/* see if device thinks it succeeded */
	if (status & 0x01) {
		/* there was an error */
		printk(KERN_ERR "nand_unlock: error status =0x%08x ", status);
		ret = -EIO;
		goto out;
	}

 out:
	/* de-select the NAND device */
	this->select_chip(mtd, -1);
	return ret;
}

static struct mtd_partition ldp_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader-NAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot-NAND",
		.offset		= 0x0080000,
		.size		= 0x0180000, /* 1.5 M */
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "Boot Env-NAND",
		.offset		= 0x01C0000,
		.size		= 0x0040000,
	},
	{
		.name		= "Kernel-NAND",
		.offset		= 0x0200000,
		.size		= 0x1C00000,   /* 30M */
	},
#ifdef CONFIG_ANDROID
	{
		.name		= "system",
		.offset		= 0x2000000,
		.size		= 0xA000000,   /* 160M */
	},
	{
		.name		= "userdata",
		.offset		= 0xC000000,
		.size		= 0x2000000,    /* 32M */
 	},
	{
		.name		= "cache",
		.offset		= 0xE000000,
		.size		= 0x2000000,    /* 32M */
 	},
#endif
};

/*
 * Unlock the nand partitions "system", "userdata", "cache" on Zoom2
 * once CORE hits OFF mode.
 */
#if defined(CONFIG_ANDROID) && defined(CONFIG_MACH_OMAP_ZOOM2)
static void zoom2_nand_unlock(struct mtd_info *mtd, struct device *dev)
{
	static int off_counter;

	if (get_last_off_on_transaction_id(dev) != off_counter) {
		int offset, size;

		offset = ldp_nand_partitions[4].offset;
		size = ldp_nand_partitions[4].size + ldp_nand_partitions[5].size
				+ ldp_nand_partitions[6].size;
		/* Unlock the partitions "system", "userdata" and "cache" */
		omap_zoom2_nand_unlock(mtd, offset, size);
		off_counter = get_last_off_on_transaction_id(dev);
	}
}
#else
#define zoom2_nand_unlock NULL
#endif

/* NAND chip access: 16 bit */
static struct omap_nand_platform_data ldp_nand_data = {
	.parts		= ldp_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ldp_nand_partitions),
	.nand_setup	= NULL,
	.dma_channel	= -1,		/* disable DMA in OMAP NAND driver */
	.dev_ready	= NULL,
/*	.unlock		= omap_zoom2_nand_unlock,
	.board_unlock	= zoom2_nand_unlock,
	.ecc_opt	= 0x1,*/
};

static struct resource ldp_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ldp_nand_device = {
	.name		= "omap2-nand",
	.id		= 0,
	.dev		= {
	.platform_data	= &ldp_nand_data,
	},
	.num_resources	= 1,
	.resource	= &ldp_nand_resource,
};

/**
 * ldp430_flash_init - Identify devices connected to GPMC and register.
 *
 * @return - void.
 */
void __init ldp_flash_init(void)
{
	u8 nandcs = GPMC_CS_NUM + 1;
	u32 gpmc_base_add = OMAP34XX_GPMC_VIRT;

#ifdef CONFIG_OMAP3_PM
	freq_config[0].freq = SUPPORTED_FREQ1;
	freq_config[1].freq = SUPPORTED_FREQ2;

	/* smc9211 debug ether */
	freq_config[0].gpmc_cfg[LDP_SMC911X_CS] = enet_gpmc_setting[0];
	freq_config[1].gpmc_cfg[LDP_SMC911X_CS] = enet_gpmc_setting[1];
#endif
	/* pop nand part */
	nandcs = LDP3430_NAND_CS;

	ldp_nand_data.cs = nandcs;
	ldp_nand_data.gpmc_cs_baseaddr = (void *)(gpmc_base_add +
					GPMC_CS0_BASE + nandcs * GPMC_CS_SIZE);
	ldp_nand_data.gpmc_baseaddr = (void *) (gpmc_base_add);

	if (platform_device_register(&ldp_nand_device) < 0)
		printk(KERN_ERR "Unable to register NAND device\n");
#ifdef CONFIG_OMAP3_PM
	/*
	 * Setting up gpmc_freq_cfg so tat gpmc module is aware of the
	 * frequencies supported and the various config values for cs
	 */
	gpmc_freq_cfg.total_no_of_freq = NO_GPMC_FREQ_SUPPORTED;
	gpmc_freq_cfg.freq_cfg = freq_config;
#endif
}

