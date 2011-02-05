/*
 * linux/sound/soc/codecs/tlv320dac3100.c
 *
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/wm8753.c by Liam Girdwood
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 *
 * Rev 0.1   ASoC driver support   dac3100         21-Apr-2010
 *   
 *			 The AIC3252 ASoC driver is ported for the codec dac3100.
 *     
 * Rev 0.2   Mistral Codec driver cleanup 	   27-Jun-2010
 * 
 * Rev 0.3   Updated the SUSPEND-RESUME fix from the tlv320aic3111.c into this
 *           code-base. 			   12-Jul-2010
 *     
 * Rev 0.4   Updated the STANDBY and ON Code to switch OFF/ON the 
 *           DAC/Headphone/Speaker Drivers.        21-Jul-2010    
 *
 * Rev 0.5   Updated with the calls to the dac3100_parse_biquad_array()        
 *           and dac3100_update_biquad_array()	   30-Jul-2010
 *
 * Rev 0.6   Updated the dac3100_set_bias_level() to get rid of POP sounds
 *                                                 13-Sep-2010
 */

/***************************** INCLUDES ************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h> /* Mistral: Added for mdelay */
#include <linux/interrupt.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "tlv320dac3100.h"
#include <mach/gpio.h>

#include <linux/i2c/twl4030.h>
#include <linux/clk.h>
#include <plat/clock.h>

//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(KERN_ALERT x)
#else
#define DBG(x...)
#endif


extern struct clk *gpt11_fclk;

/*
 ***************************************************************************** 
 * Macros
 ***************************************************************************** 
 */
#define AIC_FORCE_SWITCHES_ON 


#ifdef CONFIG_ADAPTIVE_FILTER
extern void dac3100_add_biquads_controls (struct snd_soc_codec *codec);

extern int dac3100_add_EQ_mixer_controls (struct snd_soc_codec *codec);

extern int dac3100_parse_biquad_array (struct snd_soc_codec *codec);

extern int dac3100_update_biquad_array (struct snd_soc_codec *codec, int speaker_active, int playback_active);
#endif

#define SOC_SINGLE_dac3100(xname) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = __new_control_info, .get = __new_control_get,\
	.put = __new_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
}

#define SOC_DOUBLE_R_dac3100(xname, reg_left, reg_right, shift, mask, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = snd_soc_info_volsw_2r_dac3100, \
	.get = snd_soc_get_volsw_2r_dac3100, .put = snd_soc_put_volsw_2r_dac3100, \
	.private_value = (reg_left) | ((shift) << 8)  | \
		((mask) << 12) | ((invert) << 20) | ((reg_right) << 24) }


#define    AUDIO_CODEC_HPH_DETECT_GPIO		(157)
#define    AUDIO_CODEC_PWR_ON_GPIO		(103)
#define    AUDIO_CODEC_RESET_GPIO		(37)
#define    AUDIO_CODEC_PWR_ON_GPIO_NAME		"audio_codec_pwron"
#define    AUDIO_CODEC_RESET_GPIO_NAME		"audio_codec_reset"

/*
 ***************************************************************************** 
 * Function Prototype
 ***************************************************************************** 
 */
static int dac3100_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params, struct snd_soc_dai *tmp);

static int dac3100_mute(struct snd_soc_dai *dai, int mute);

static int dac3100_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir);

static int dac3100_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt);

static int dac3100_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level);

unsigned int dac3100_read(struct snd_soc_codec *codec, unsigned int reg);

static int dac3100_hw_free (struct snd_pcm_substream *substream, struct snd_soc_dai *device);

static int __new_control_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo);

static int __new_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int __new_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int snd_soc_info_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_info *uinfo);

static int snd_soc_get_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);

static int snd_soc_put_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);

static int dac3100_headset_speaker_path(struct snd_soc_codec *codec);
static irqreturn_t dac3100_irq_handler(int irq, struct snd_soc_codec *codec);

/*
 ***************************************************************************** 
 * Global Variable
 ***************************************************************************** 
 */
static u8 dac3100_reg_ctl;

/* whenever aplay/arecord is run, dac3100_hw_params() function gets called. 
 * This function reprograms the clock dividers etc. this flag can be used to 
 * disable this when the clock dividers are programmed by pps config file
 */
static int soc_static_freq_config = 1;

/* Global Variable Indicating the DAC Power Status */
static int codec_dac_power_status =0;

/* Global Variable to indicate Headset or Speaker Connection */
static int headset_connected = 0;

/* Playback status Global Variable */
static int playback_status = 0;
/*
 ***************************************************************************** 
 * Structure Declaration
 ***************************************************************************** 
 */
static struct snd_soc_device *dac3100_socdev;

/*
 ***************************************************************************** 
 * soc_enum array Structure Initialization
 ***************************************************************************** 
 */
static const char *dac_mute_control[] = {"UNMUTE" , "MUTE"};
static const char *hpdriver_voltage_control[] = {"1.35V", "1.5V", "1.65V", "1.8V"};
static const char *drc_status_control[] = {"DISABLED", "ENABLED"};

static const struct soc_enum dac3100_dapm_enum[] = {
	SOC_ENUM_SINGLE (DAC_MUTE_CTRL_REG, 3, 2, dac_mute_control),
	SOC_ENUM_SINGLE (DAC_MUTE_CTRL_REG, 2, 2, dac_mute_control),
	SOC_ENUM_SINGLE (HPHONE_DRIVERS, 3, 4, hpdriver_voltage_control),
        SOC_ENUM_DOUBLE (DRC_CTRL_1, 6, 5, 2,  drc_status_control),
};

/*
 ***************************************************************************** 
 * snd_kcontrol_new Structure Initialization
 ***************************************************************************** 
 */
static const struct snd_kcontrol_new dac3100_snd_controls[] = {
	/* Output */
	/* sound new kcontrol for PCM Playback volume control */
	SOC_DOUBLE_R_dac3100("DAC Playback Volume", LDAC_VOL, RDAC_VOL, 0, 0xAf,
			     0),
	/* sound new kcontrol for HP driver gain */
	SOC_DOUBLE_R_dac3100("HP Driver Gain", HPL_DRIVER, HPR_DRIVER, 0, 0x23, 0),
	/* sound new kcontrol for LO driver gain */
	SOC_DOUBLE_R_dac3100("LO Driver Gain", SPL_DRIVER, SPR_DRIVER, 0, 0x23, 0),
	/* sound new kcontrol for HP mute */
	SOC_DOUBLE_R("HP DAC Playback Switch", HPL_DRIVER, HPR_DRIVER, 2,
		     0x01, 1),
	/* sound new kcontrol for LO mute */
	SOC_DOUBLE_R("LO DAC Playback Switch", SPL_DRIVER, SPR_DRIVER, 2,
		     0x01, 1),

	/* sound new kcontrol for Analog Volume Control for headphone and Speaker Outputs
         * Please refer to Table 5-24 of the Codec DataSheet
         */
         SOC_DOUBLE_R_dac3100("HP Analog Volume",   LEFT_ANALOG_HPL, RIGHT_ANALOG_HPR, 0, 0x7F, 1),
         SOC_DOUBLE_R_dac3100("SPKR Analog Volume", LEFT_ANALOG_SPL, RIGHT_ANALOG_SPR, 0, 0x7F, 1),

	/* sound new kcontrol for Programming the registers from user space */
	SOC_SINGLE_dac3100("Program Registers"),

        /* Enumerations SOCs Controls */
        SOC_ENUM ("LEFT  DAC MUTE", dac3100_dapm_enum[LEFT_DAC_MUTE_ENUM]),
        SOC_ENUM ("RIGHT DAC MUTE", dac3100_dapm_enum[RIGHT_DAC_MUTE_ENUM]),
        SOC_ENUM ("HP Driver Voltage level", dac3100_dapm_enum[HP_DRIVER_VOLTAGE_ENUM]),
        SOC_ENUM ("DRC Status", dac3100_dapm_enum[DRC_STATUS_ENUM]),

        /* Dynamic Range Compression Control */ 
        SOC_SINGLE ("DRC Hysteresis Value (0=0db 3=db)", DRC_CTRL_1, 0, 0x03, 0),
        SOC_SINGLE ("DRC Threshold Value (0=-3db,7=-24db)",  DRC_CTRL_1, 2, 0x07, 0),
        SOC_SINGLE ("DRC Hold Time",   DRC_CTRL_2, 3, 0x0F, 0),
        SOC_SINGLE ("DRC Attack Time", DRC_CTRL_3, 4, 0x0F, 0),
        SOC_SINGLE ("DRC Delay Rate",  DRC_CTRL_3, 0, 0x0F, 0),

       

};


/* the sturcture contains the different values for mclk */
static const struct dac3100_rate_divs dac3100_divs[] = {
/* 
 * mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N, 
 * codec_speficic_initializations 
 */
	/* 8k rate */
	// DDenchev (MMS)
	{12000000, 8000, 1, 7, 1680, 128, 2, 42, 4,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 1}}},
	{13000000, 8000, 1, 6, 3803, 128, 3, 27, 4,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 4}}},
	{24000000, 8000, 2, 7, 6800, 768, 15, 1, 24,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 11.025k rate */
	// DDenchev (MMS)
	{12000000, 11025, 1, 7, 560, 128, 5, 12, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 11025, 1, 6, 1876, 128, 3, 19, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},
	{24000000, 11025, 2, 7, 5264, 512, 16, 1, 16,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 12k rate */
	// DDenchev (MMS)
	{12000000, 12000, 1, 7, 1680, 128, 2, 28, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 12000, 1, 6, 3803, 128, 3, 18, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},

	/* 16k rate */
	// DDenchev (MMS)
	{12000000, 16000, 1, 7, 1680, 128, 2, 21, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 16000, 1, 6, 6166, 128, 3, 14, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},
	{24000000, 16000, 2, 7, 6800, 384, 15, 1, 12,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 22.05k rate */
	// DDenchev (MMS)
	{12000000, 22050, 1, 7, 560, 128, 5, 6, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 22050, 1, 6, 5132, 128, 3, 10, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},
	{24000000, 22050, 2, 7, 5264, 256, 16, 1, 8,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 24k rate */
	// DDenchev (MMS)
	{12000000, 24000, 1, 7, 1680, 128, 2, 14, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 24000, 1, 6, 3803, 128, 3, 9, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},

	/* 32k rate */
	// DDenchev (MMS)
	{12000000, 32000, 1, 6, 1440, 128, 2, 9, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 32000, 1, 6, 6166, 128, 3, 7, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},
	{24000000, 32000, 2, 7, 1680, 192, 7, 2, 6,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 44.1k rate */
	// DDenchev (MMS)
	{12000000, 44100, 1, 7, 560, 128, 5, 3, 4,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 1}}},
	{13000000, 44100, 1, 6, 5132, 128, 3, 5, 4,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 4}}},
	{24000000, 44100, 2, 7, 5264, 128, 8, 2, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/* 48k rate */
	// DDenchev (MMS)
	{12000000, 48000, 1, 7, 1680, 128, 2, 7, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},
	{13000000, 48000, 1, 6, 6166, 128, 7, 2, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 4}}},
	{24000000, 48000, 2, 8, 1920, 128, 8, 2, 4,
	 {{DAC_INSTRUCTION_SET, 1}, {61, 1}}},

	/*96k rate : GT 21/12/2009: NOT MODIFIED */
	{12000000, 96000, 1, 8, 1920, 64, 2, 8, 2,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 7}}},
	{13000000, 96000, 1, 6, 6166, 64, 7, 2, 2,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 10}}},
	{24000000, 96000, 2, 8, 1920, 64, 4, 4, 2,
	 {{DAC_INSTRUCTION_SET, 7}, {61, 7}}},

	/*192k : GT 21/12/2009: NOT MODIFIED */
	{12000000, 192000, 1, 8, 1920, 32, 2, 8, 1,
	 {{DAC_INSTRUCTION_SET, 17}, {61, 13}}},
	{13000000, 192000, 1, 6, 6166, 32, 7, 2, 1,
	 {{DAC_INSTRUCTION_SET, 17}, {61, 13}}},
	{24000000, 192000, 2, 8, 1920, 32, 4, 4, 1,
	 {{DAC_INSTRUCTION_SET, 17}, {61, 13}}},
};

struct snd_soc_dai_ops tlv320dac3100_dai_ops = {
	.hw_params = dac3100_hw_params,
	.digital_mute = dac3100_mute,
	.set_sysclk = dac3100_set_dai_sysclk,
	.set_fmt = dac3100_set_dai_fmt,
	.hw_free = dac3100_hw_free,
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz., 
 *          playback and capture, DAI runtime information viz. state of DAI 
 *			and pop wait state, and DAI private data. 
 *          The dac3100 rates ranges from 8k to 192k
 *          The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */
struct snd_soc_dai tlv320dac3100_dai = {
	.name = "TLV320dac3100",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 2,
		     .channels_max = 2,
		     .rates = dac3100_RATES,
		     .formats = dac3100_FORMATS,},
	.ops = &tlv320dac3100_dai_ops,
};

EXPORT_SYMBOL_GPL(tlv320dac3100_dai);


/* AUDIO NOISE ISSUE : =======================================================*/

struct snd_soc_codec  *AudioRecovery_codec;
struct snd_soc_device *AudioRecovery_socdev;

/* ===========================================================================*/

#ifdef DEBUG

/*
 *----------------------------------------------------------------------------
 * Function : debug_print_registers
 * Purpose  : Debug routine to dump all the Registers of Page 0
 *
 *----------------------------------------------------------------------------
 */
void debug_print_registers (struct snd_soc_codec *codec)
{
	int i;
	u32 data;

    /* Dump for Page 0 */
	for (i = 0 ; i < 80 ; i++) {
		data = dac3100_read(codec, i);
		printk(KERN_ALERT "Pg 0 reg = %d val = %x\n", i, data);
	}
    /* Dump for Page 1 */
	for (i = 158 ; i < 174 ; i++) {
		data = dac3100_read(codec, i);
		printk(KERN_ALERT "Pg 1 reg = %d val = %x\n", (i % PAGE_1), data);
	}
   
}
#endif //DEBUG

/*
 ***************************************************************************** 
 * Initializations
 ***************************************************************************** 
 */
/*
 * dac3100 register cache
 * We are caching the registers here.
 * There is no point in caching the reset register.
 * NOTE: In AIC32, there are 127 registers supported in both page0 and page1
 *       The following table contains the page0 and page 1registers values.
 */
static const u8 dac3100_reg[dac3100_CACHEREGNUM] = {
	0x00, 0x00, 0x00, 0x02,	/* 0 */
	0x00, 0x11, 0x04, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x01,	/* 8 */
	0x01, 0x00, 0x80, 0x80,	/* 12 */
	0x08, 0x00, 0x01, 0x01,	/* 16 */
	0x80, 0x80, 0x04, 0x00,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x00, 0x01, 0x00,	/* 28 */
	0x00, 0x00, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x00, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
//	0x00, 0x02, 0x02, 0x00,	/* 52 */
	0x00, 0x00, 0x02, 0x00,	/* 52 */
	0x00, 0x00, 0x00, 0x00,	/* 56 */
	0x01, 0x04, 0x00, 0x14,	/* 60 */
	0x0C, 0x00, 0x00, 0x00,	/* 64 */
	0x0F, 0x38, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0xEE,	/* 72 */
	0x10, 0xD8, 0x7E, 0xE3,	/* 76 */
	0x00, 0x00, 0x80, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
//	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x00, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x04,	/* 156, PAGE1-28 */
	0x06, 0x3E, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x7F, 0x7F, 0x7F, 0x7F,	/* 164, PAGE1-36 */
	0x02, 0x02, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x80,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	
};

/* 
 * dac3100 initialization data 
 * This structure initialization contains the initialization required for
 * dac3100.
 * These registers values (reg_val) are written into the respective dac3100 
 * register offset (reg_offset) to  initialize dac3100. 
 * These values are used in dac3100_init() function only. 
 */
static const struct dac3100_configs dac3100_reg_init[] = {
	/* Carry out the software reset */
	{RESET, 0x01},
	/* Connect MIC1_L and MIC1_R to CM */
	{MICPGA_CM, 0xC0},
	/* PLL is CODEC_CLKIN */
//	{CLK_REG_1, MCLK_2_CODEC_CLKIN}, //if pb set DIN
	{CLK_REG_1, PLLCLK_2_CODEC_CLKIN},
	/* DAC_MOD_CLK is BCLK source */
	{AIS_REG_3, DAC_MOD_CLK_2_BDIV_CLKIN},
    
	/* Switch off PLL */
    {CLK_REG_2,  0x00},
	/* Switch off NDAC Divider */
    {NDAC_CLK_REG_6, 0x00},
	/* Switch off MDAC Divider */
	{MDAC_CLK_REG_7, 0x00},
	/* Switch off BCLK_N Divider */
	{CLK_REG_11, 0x00},
    
	/* Setting up DAC Channel */
	{DAC_CHN_REG, LDAC_2_LCHN | RDAC_2_RCHN | SOFT_STEP_2WCLK},
	{DAC_MUTE_CTRL_REG, 0x0C},   	/* Mistral: Updated this value from 0x00 to 0x0C to MUTE DAC Left and Right channels */
	/* DAC_L and DAC_R Output Mixer Routing */ 
	{DAC_MIXER_ROUTING, 0x44},  //DAC_X is routed to the channel mixer amplifier
    
	/* Headphone powerup */
	/* Left Channel Volume routed to HPL and gain -6 db */
	{LEFT_ANALOG_HPL, 0x8C}, 
	/* Right Channel Volume routed to HPR and gain -6 db */
	{RIGHT_ANALOG_HPR, 0x8C}, 
	/* Left Analog Volume routed to class-D and gain -6 db */
	{LEFT_ANALOG_SPL, 0x8C},        /* Mistral: Updated this value from 0x80 to 0x30. */
	{CLASS_D_SPK, 0x06},            /* Mistral: Updated this value from 0xc6 to 0x06 */
    {SPL_DRIVER, 0x04},             /* Mistral: Added this new value to keep the Class-D Speaker in MUTE by default */
	{HP_OUT_DRIVERS, 0xCE},         /* Mistral modified value to power down DAC after HP and SPK Amplifiers */
    {PGA_RAMP, 0x70},               /* Mistral: Updated the Speaker Power-up Ramp time to 30.5 ms */
	/* HPL unmute and gain 0db */
	{HPL_DRIVER, 0x0}, /* Mistral: Updated this value from 0x6. We do not need to UNMUTE the HPL at startup */
	/* HPR unmute and gain 0db */
	{HPR_DRIVER, 0x0}, /* Mistral: Updated this value from 0x6. We do not need to UNMUTE the HPR at startup */ 
	/* Headphone drivers */
	{HPHONE_DRIVERS, 0x04}, /* Mistral: Updated this value from 0xC4. We do not need to Power up HPL and HPR at startup */

    /* Mistral Added following configuration for EQ Setup */
    /* reg[0][65] should be configured to -6dB */
    /* Mistral: Updated the DACR volume also to -6dB */ 
    {LDAC_VOL, 0xFD}, /* Update DACL volume control from -6db to -1.5db to get speaker and Headphone volume loud enough on Encore device */
    {RDAC_VOL, 0xFD}, /* Update DACR volume control from -6db to -1.5db to get speaker and Headphone volume loud enough on Encore device */
    
    /* reg[1][42] should be configured at 0x04. This is already done above */
    /* reg[1][35] should be configured correctly. This is already done above */ 
    /* reg[1][32] should be powered up, Will be done at run-time */
    /* reg[1][38] should be routed to Class-D Output Driver. This is already done above */
        
};

/* 
 * DAPM Mixer Controls
 */
/* Left DAC_L Mixer */
static const struct snd_kcontrol_new hpl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC switch", DAC_MIXER_ROUTING, 6, 2, 1),
	SOC_DAPM_SINGLE("MIC1_L switch", DAC_MIXER_ROUTING, 5, 1, 0),
//	SOC_DAPM_SINGLE("Left_Bypass switch", HPL_ROUTE_CTL, 1, 1, 0),
};

/* Right DAC_R Mixer */
static const struct snd_kcontrol_new hpr_output_mixer_controls[] = {

	SOC_DAPM_SINGLE("R_DAC switch", DAC_MIXER_ROUTING, 2, 2, 1),
	SOC_DAPM_SINGLE("MIC1_R switch", DAC_MIXER_ROUTING, 1, 1, 0),
//	SOC_DAPM_SINGLE("Right_Bypass switch", HPR_ROUTE_CTL, 1, 1, 0),
};

static const struct snd_kcontrol_new lol_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC switch", DAC_MIXER_ROUTING, 6, 2, 1),    //SOC_DAPM_SINGLE("L_DAC switch", LOL_ROUTE_CTL, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1_L switch", DAC_MIXER_ROUTING, 5, 1, 0),
//	SOC_DAPM_SINGLE("Left_Bypass switch", HPL_ROUTE_CTL, 1, 1, 0),
};

static const struct snd_kcontrol_new lor_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC switch", DAC_MIXER_ROUTING, 2, 2, 1),    //SOC_DAPM_SINGLE("R_DAC switch", LOR_ROUTE_CTL, 3, 1, 0),
	SOC_DAPM_SINGLE("MIC1_R switch", DAC_MIXER_ROUTING, 1, 1, 0),
//	SOC_DAPM_SINGLE("Right_Bypass switch", LOR_ROUTE_CTL, 1, 1, 0),
};

/* Right DAC_R Mixer */
static const struct snd_kcontrol_new left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1_L switch", MICPGA_PIN_CFG, 6, 1, 0),
	SOC_DAPM_SINGLE("MIC1_M switch", MICPGA_PIN_CFG, 2, 1, 0),
};

static const struct snd_kcontrol_new right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("MIC1_R switch", MICPGA_PIN_CFG, 4, 1, 0),
	SOC_DAPM_SINGLE("MIC1_M switch", MICPGA_PIN_CFG, 2, 1, 0),
};

/* 
 * DAPM Widget Controls
 */
static const struct snd_soc_dapm_widget dac3100_dapm_widgets[] = {
	/* Left DAC to Left Outputs */
	/* dapm widget (stream domain) for left DAC */
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", DAC_CHN_REG, 7, 1),
  
	/* dapm widget (path domain) for left DAC_L Mixer */

	SND_SOC_DAPM_MIXER("HPL Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_output_mixer_controls[0],
			   ARRAY_SIZE(hpl_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPL Power", HPHONE_DRIVERS, 7, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("LOL Output Mixer", SND_SOC_NOPM, 0, 0,
			   &lol_output_mixer_controls[0],
			   ARRAY_SIZE(lol_output_mixer_controls)),
	SND_SOC_DAPM_PGA("LOL Power", CLASS_D_SPK, 7, 1, NULL, 0),

	/* Right DAC to Right Outputs */

	/* dapm widget (stream domain) for right DAC */
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", DAC_CHN_REG, 6, 1),
	/* dapm widget (path domain) for right DAC_R mixer */
	SND_SOC_DAPM_MIXER("HPR Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_output_mixer_controls[0],
			   ARRAY_SIZE(hpr_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPR Power", HPHONE_DRIVERS, 6, 0, NULL, 0), 

	SND_SOC_DAPM_MIXER("LOR Output Mixer", SND_SOC_NOPM, 0, 0,
			   &lor_output_mixer_controls[0],
			   ARRAY_SIZE(lor_output_mixer_controls)),
	SND_SOC_DAPM_PGA("LOR Power", CLASS_D_SPK, 6, 1, NULL, 0),

	SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 0, 0,
			   &left_input_mixer_controls[0],
			   ARRAY_SIZE(left_input_mixer_controls)),

	SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 0, 0,
			   &right_input_mixer_controls[0],
			   ARRAY_SIZE(right_input_mixer_controls)),

	/* No widgets are required for ADC since the DAC3100 Audio 
         * Codec Chipset does not contain a ADC
         */
	

	/* dapm widget (platform domain) name for HPLOUT */
	SND_SOC_DAPM_OUTPUT("HPL"),
	/* dapm widget (platform domain) name for HPROUT */
	SND_SOC_DAPM_OUTPUT("HPR"),
	/* dapm widget (platform domain) name for LOLOUT */
	SND_SOC_DAPM_OUTPUT("LOL"),
	/* dapm widget (platform domain) name for LOROUT */
	SND_SOC_DAPM_OUTPUT("LOR"),

	/* dapm widget (platform domain) name for MIC1LP */
	SND_SOC_DAPM_INPUT("MIC1LP"),
	/* dapm widget (platform domain) name for MIC1RP*/
	SND_SOC_DAPM_INPUT("MIC1RP"),
	/* dapm widget (platform domain) name for MIC1LM */
	SND_SOC_DAPM_INPUT("MIC1LM"),
};


/*
* DAPM audio route definition. *
* Defines an audio route originating at source via control and finishing 
* at sink. 
*/
static const struct snd_soc_dapm_route dac3100_dapm_routes[] = {
	/* ******** Right Output ******** */
	{"HPR Output Mixer", "R_DAC switch", "Right DAC"},
	{"HPR Output Mixer",  "MIC1_R switch", "MIC1RP"},
	//{"HPR Output Mixer", "Right_Bypass switch", "Right_Bypass"},

	{"HPR Power", NULL, "HPR Output Mixer"},
	{"HPR", NULL, "HPR Power"},


	{"LOR Output Mixer", "R_DAC switch", "Right DAC"},
	{"LOR Output Mixer",  "MIC1_R switch", "MIC1RP"},
//	{"LOR Output Mixer", "Right_Bypass switch", "Right_Bypass"},

	{"LOR Power", NULL, "LOR Output Mixer"},
	{"LOR", NULL, "LOR Power"},
	
	/* ******** Left Output ******** */
	{"HPL Output Mixer", "L_DAC switch", "Left DAC"},
	{"HPL Output Mixer", "MIC1_L switch", "MIC1LP"},
	//{"HPL Output Mixer", "Left_Bypass switch", "Left_Bypass"},

	{"HPL Power", NULL, "HPL Output Mixer"},
	{"HPL", NULL, "HPL Power"},


	{"LOL Output Mixer", "L_DAC switch", "Left DAC"},
	{"LOL Output Mixer", "MIC1_L switch", "MIC1LP"},
//	{"LOL Output Mixer", "Left_Bypass switch", "Left_Bypass"},

	{"LOL Power", NULL, "LOL Output Mixer"},
	{"LOL", NULL, "LOL Power"},

	/* ******** terminator ******** */
	//{NULL, NULL, NULL},
};

#define dac3100_DAPM_ROUTE_NUM (sizeof(dac3100_dapm_routes)/sizeof(struct snd_soc_dapm_route))

static void i2c_dac3100_headset_access_work(struct work_struct *work);
static struct work_struct works;
static struct snd_soc_codec *codec_work_var_glob;

/*
 ***************************************************************************** 
 * Function Definitions
 ***************************************************************************** 
 */

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_info_volsw_2r_dac3100
 * Purpose  : Info routine for the ASoC Widget related to the Volume Control
 *
 *----------------------------------------------------------------------------
 */
static int snd_soc_info_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_info *uinfo)
{
    int mask = (kcontrol->private_value >> 12) & 0xff;

	DBG("snd_soc_info_volsw_2r_dac3100 (%s)\n", kcontrol->id.name);

    uinfo->type =
        mask == 1 ? SNDRV_CTL_ELEM_TYPE_BOOLEAN : SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 2;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = mask;
    return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_get_volsw_2r_dac3100
 * Purpose  : Callback to get the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_get_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value & dac3100_8BITS_MASK;
	int reg2 = (kcontrol->private_value >> 24) & dac3100_8BITS_MASK;
	int mask;
	int shift;
	unsigned short val, val2;

	DBG("snd_soc_get_volsw_2r_dac3100 %s\n", kcontrol->id.name);

        /* Check the id name of the kcontrol and configure the mask and shift */
	if (!strcmp(kcontrol->id.name, "DAC Playback Volume")) {
		mask = dac3100_8BITS_MASK;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "HP Driver Gain")) {
		mask = 0xF;
		shift = 3;
	} else if (!strcmp(kcontrol->id.name, "LO Driver Gain")) {
		mask = 0x3;
		shift = 3;
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		mask = 0x7F;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "HP Analog Volume")) {
                mask = 0x7F;   
                shift = 0; 
        } else if (!strcmp(kcontrol->id.name, "SPKR Analog Volume")) {
                mask = 0x7F;
                shift = 0;
        } else {
		printk(KERN_ALERT "Invalid kcontrol name\n");
		return -1;
	}

	val = (snd_soc_read(codec, reg) >> shift) & mask;
	val2 = (snd_soc_read(codec, reg2) >> shift) & mask;

	if (!strcmp(kcontrol->id.name, "DAC Playback Volume")) {
		ucontrol->value.integer.value[0] =
		    (val <= 48) ? (val + 127) : (val - 129);
		ucontrol->value.integer.value[1] =
		    (val2 <= 48) ? (val2 + 127) : (val2 - 129);
	} else if (!strcmp(kcontrol->id.name, "HP Driver Gain")) {
		ucontrol->value.integer.value[0] =
		    (val <= 9) ? (val + 0) : (val - 15);
		ucontrol->value.integer.value[1] =
		    (val2 <= 9) ? (val2 + 0) : (val2 - 15);
	} else if (!strcmp(kcontrol->id.name, "LO Driver Gain")) {
		ucontrol->value.integer.value[0] =
		    ((val/6) <= 4) ? ((val/6 -1)*6) : ((val/6 - 0)*6);
		ucontrol->value.integer.value[1] =
		    ((val2/6) <= 4) ? ((val2/6-1)*6) : ((val2/6 - 0)*6);
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		ucontrol->value.integer.value[0] =
		    ((val*2) <= 40) ? ((val*2 + 24)/2) : ((val*2 - 254)/2);
		ucontrol->value.integer.value[1] =
		    ((val2*2) <= 40) ? ((val2*2 + 24)/2) : ((val2*2 - 254)/2);
        } else if (!strcmp(kcontrol->id.name, "HP Analog Volume")) {
                ucontrol->value.integer.value[0] = 
                      ((val*2) <= 40) ? ((val*2 +24)/2) :((val2*2 - 254)/2);
                ucontrol->value.integer.value[1] = 
                      ((val*2) <=40) ? ((val*2 + 24)/2) : ((val2*2 - 254)/2);    
	} else if (!strcmp(kcontrol->id.name, "SPKR Analog Volume")) {
                ucontrol->value.integer.value[0] = 
                      ((val*2) <= 40) ? ((val*2 +24)/2) :((val2*2 - 254)/2);
                ucontrol->value.integer.value[1] = 
                      ((val*2) <=40) ? ((val*2 + 24)/2) : ((val2*2 - 254)/2);    
        }
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : snd_soc_put_volsw_2r_dac3100
 * Purpose  : Callback to set the value of a double mixer control that spans
 *            two registers.
 *
 *----------------------------------------------------------------------------
 */
int snd_soc_put_volsw_2r_dac3100(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value & dac3100_8BITS_MASK;
	int reg2 = (kcontrol->private_value >> 24) & dac3100_8BITS_MASK;
	int err;
	unsigned short val, val2, val_mask;

	DBG("snd_soc_put_volsw_2r_dac3100 (%s)\n", kcontrol->id.name);

	val = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];

        /* Mistral: This block needs to be revisted */  
	if (!strcmp(kcontrol->id.name, "DAC Playback Volume")) {
		val = (val >= 127) ? (val - 127) : (val + 129);
//		 (val <= 48) ? (val + 127) : (val - 129);
		val2 = (val2 >= 127) ? (val2 - 127) : (val2 + 129);
		val_mask = dac3100_8BITS_MASK;	/* 8 bits */
	} else if (!strcmp(kcontrol->id.name, "HP Driver Gain")) {
		val = (val >= 0) ? (val - 0) : (val + 15);
		val2 = (val2 >= 0) ? (val2 - 0) : (val2 + 15);
		val_mask = 0xF;	/* 4 bits */
	} else if (!strcmp(kcontrol->id.name, "LO Driver Gain")) {
		val = (val/6 >= 1) ? ((val/6 +1)*6) : ((val/6 + 0)*6);
		val2 = (val2/6 >= 1) ? ((val2/6 +1)*6) : ((val2/6 + 0)*6);
		val_mask = 0x3;	/* 2 bits */
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		val = (val*2 >= 24) ? ((val*2 - 24)/2) : ((val*2 + 254)/2);
		val2 = (val2*2 >= 24) ? ((val2*2 - 24)/2) : ((val2*2 + 254)/2);
		val_mask = 0x7F;	/* 7 bits */
	} else if (!strcmp(kcontrol->id.name, "HP Analog Volume")) {
                val_mask = 0x7F; /* 7 Bits */
        } else if (!strcmp(kcontrol->id.name, "SPKR Analog Volume")) {
                val_mask = 0x7F; /* 7 Bits */
	} else {
		printk(KERN_ALERT "Invalid control name\n");
		return -1;
	}

	if ((err = snd_soc_update_bits(codec, reg, val_mask, val)) < 0) {
		printk(KERN_ALERT "Error while updating bits\n");
		return err;
	}

	err = snd_soc_update_bits(codec, reg2, val_mask, val2);
	return err;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_info
 * Purpose  : This function is to initialize data for new control required to 
 *            program the dac3100 registers.
 *            
 *----------------------------------------------------------------------------
 */
static int __new_control_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{

	DBG("+ new control info\n");

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get
 * Purpose  : This function is to read data of new control for 
 *            program the dac3100 registers.
 *            
 *----------------------------------------------------------------------------
 */
static int __new_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 val;

	DBG("+ new control get (%d)\n", dac3100_reg_ctl);

	val = dac3100_read(codec, dac3100_reg_ctl);
	ucontrol->value.integer.value[0] = val;

	DBG("+ new control get val(%d)\n", val);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *            the driver.
 * 
 *----------------------------------------------------------------------------
 */
static int __new_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct dac3100_priv *dac3100 = codec->private_data;

	u32 data_from_user = ucontrol->value.integer.value[0];
	u8 data[2];

	DBG("+ new control put (%s)\n", kcontrol->id.name);

	DBG("reg = %d val = %x\n", data[0], data[1]);

	dac3100_reg_ctl = data[0] = (u8) ((data_from_user & 0xFF00) >> 8);
	data[1] = (u8) ((data_from_user & 0x00FF));

	if (!data[0]) {
		dac3100->page_no = data[1];
	}

	DBG("reg = %d val = %x\n", data[0], data[1]);

	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ALERT "Error in i2c write\n");
		return -EIO;
	}
	DBG("- new control put\n");

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_change_page
 * Purpose  : This function is to switch between page 0 and page 1.
 *            
 *----------------------------------------------------------------------------
 */
int dac3100_change_page(struct snd_soc_codec *codec, u8 new_page)
{
	struct dac3100_priv *dac3100 = codec->private_data;
	u8 data[2];

	data[0] = 0;
	data[1] = new_page;
	dac3100->page_no = new_page;
	DBG("dac3100_change_page => %d (w 30 %02x %02x)\n", new_page, data[0], data[1]);

//	DBG("w 30 %02x %02x\n", data[0], data[1]);

	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ALERT "Error in changing page to %d\n", new_page);
		return -1;
	}
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_write_reg_cache
 * Purpose  : This function is to write dac3100 register cache
 *            
 *----------------------------------------------------------------------------
 */
static inline void dac3100_write_reg_cache(struct snd_soc_codec *codec,
					   u16 reg, u8 value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= dac3100_CACHEREGNUM) {
		return;
	}
	cache[reg] = value;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_read_reg_cache
 * Purpose  : This function is to read the dac3100 registers through the 
 *            Register Cache Array instead of I2C Transfers
 *            
 *----------------------------------------------------------------------------
 */
static unsigned char dac3100_read_reg_cache(struct snd_soc_codec *codec, unsigned int reg)
{
    u8 *cache = codec->reg_cache;

    /* Confirm the Register Offset is within the Array bounds */
    if (reg >= dac3100_CACHEREGNUM) {
	return (0);
    } 

    return (cache[reg]);
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_write
 * Purpose  : This function is to write to the dac3100 register space.
 *            
 *----------------------------------------------------------------------------
 */
int dac3100_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct dac3100_priv *dac3100 = codec->private_data;
	u8 data[2];
	u8 page;

	page = reg / 128;
	data[dac3100_REG_OFFSET_INDEX] = reg % 128;
//	DBG("# dac3100 write reg(%d) new_page(%d) old_page(%d) value(0x%02x)\n", reg, page, dac3100->page_no, value);


	if (dac3100->page_no != page) {
		dac3100_change_page(codec, page);
	}
	
	DBG("w 30 %02x %02x\n", data[dac3100_REG_OFFSET_INDEX], value);
	

	/* data is
	 *   D15..D8 dac3100 register offset
	 *   D7...D0 register data
	 */
	data[dac3100_REG_DATA_INDEX] = value & dac3100_8BITS_MASK;
#if defined(EN_REG_CACHE)
	if ((page == 0) || (page == 1)) {
		dac3100_write_reg_cache(codec, reg, value);
	}
#endif
	if (!data[dac3100_REG_OFFSET_INDEX]) {
		/* if the write is to reg0 update dac3100->page_no */
		dac3100->page_no = value;
	}

	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ALERT "Error in i2c write\n");
		return -EIO;
	}
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_read
 * Purpose  : This function is to read the dac3100 register space.
 *            
 *----------------------------------------------------------------------------
 */
unsigned int dac3100_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct dac3100_priv *dac3100 = codec->private_data;
	u8 value;
	u8 page = reg / 128;
    u8 cache_value;

        /* Can be used to optimize the Reads from page 0 and 1 */
#if defined (EN_REG_CACHE)
        if ((page == 0) || (page == 1)) {	
            cache_value = dac3100_read_reg_cache (codec, reg);
            //DBG("Reg%x-Cache %02x\n", reg, value);
        }         
#endif
	reg = reg % 128;

	//DBG("r 30 %02x\n", reg);
	
	if (dac3100->page_no != page) {
		dac3100_change_page(codec, page);
	}
        /* Using I2C will make the code non-portable. Use the codec->hw_write callback instead */
	/*i2c_master_send(codec->control_data, (char *)&reg, 1);
	i2c_master_recv(codec->control_data, &value, 1); */

	codec->hw_write (codec->control_data, (char *)&reg, 1);
	value = codec->hw_read  (codec->control_data, 1);

        //DBG ("Reg %X Val %x Cache %x\r\n", reg, value, cache_value);
	return value;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_get_divs
 * Purpose  : This function is to get required divisor from the "dac3100_divs"
 *            table.
 *            
 *----------------------------------------------------------------------------
 */
static inline int dac3100_get_divs(int mclk, int rate)
{
	int i;

	DBG("+ dac3100_get_divs mclk(%d) rate(%d)\n", mclk, rate);

	for (i = 0; i < ARRAY_SIZE(dac3100_divs); i++) {
		if ((dac3100_divs[i].rate == rate)
		    && (dac3100_divs[i].mclk == mclk)) {
	DBG("%d %d %d %d %d %d %d\n",
	dac3100_divs[i].p_val,
	dac3100_divs[i].pll_j,
	dac3100_divs[i].pll_d,
	dac3100_divs[i].dosr,
	dac3100_divs[i].ndac,
	dac3100_divs[i].mdac,
	dac3100_divs[i].blck_N);

			return i;
		}
	}
	printk(KERN_ALERT "Master clock and sample rate is not supported\n");
	return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_add_controls
 * Purpose  : This function is to add non dapm kcontrols.  The different 
 *            controls are in "dac3100_snd_controls" table.
 *            The following different controls are supported
 *                # DAC Playback volume control 
 *		  # PCM Playback Volume
 *		  # HP Driver Gain
 *		  # HP DAC Playback Switch
 *		  # PGA Capture Volume
 *		  # Program Registers
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	DBG("+ dac3100_add_controls num_controls(%d)\n", ARRAY_SIZE(dac3100_snd_controls));
	for (i = 0; i < ARRAY_SIZE(dac3100_snd_controls); i++) {
		err =
		    snd_ctl_add(codec->card,
				snd_soc_cnew(&dac3100_snd_controls[i], codec,
					     NULL));
		if (err < 0) {
			printk(KERN_ALERT "Invalid control\n");
			return err;
		}
	}
	DBG("- dac3100_add_controls \n");

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_add_widgets
 * Purpose  : This function is to add the dapm widgets 
 *            The following are the main widgets supported
 *                # Left DAC to Left Outputs
 *                # Right DAC to Right Outputs
 *		  # Left Inputs to Left ADC
 *		  # Right Inputs to Right ADC
 *
 *----------------------------------------------------------------------------
 */
static int dac3100_add_widgets(struct snd_soc_codec *codec)
{
	int i;

	DBG("+ dac3100_add_widgets num_widgets(%d) num_routes(%d)\n",
			ARRAY_SIZE(dac3100_dapm_widgets), dac3100_DAPM_ROUTE_NUM);
	for (i = 0; i < ARRAY_SIZE(dac3100_dapm_widgets); i++) {
		snd_soc_dapm_new_control(codec, &dac3100_dapm_widgets[i]);
	}

	DBG("snd_soc_dapm_add_routes\n");

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(codec, &dac3100_dapm_routes[0],
				dac3100_DAPM_ROUTE_NUM);


	DBG("snd_soc_dapm_new_widgets\n");
	snd_soc_dapm_new_widgets(codec);
	DBG("- dac3100_add_widgets\n");
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_hw_params
 * Purpose  : This function is to set the hardware parameters for dac3100.
 *            The functions set the sample rate and audio serial data word 
 *            length.
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params, struct snd_soc_dai *tmp)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct dac3100_priv *dac3100 = codec->private_data;
	int i;
	u8 data;

	DBG("+ SET dac3100_hw_params\n");


	dac3100_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	i = dac3100_get_divs(dac3100->sysclk, params_rate(params));
	DBG("- Sampling rate: %d, %d\n", params_rate(params), i);


	if (i < 0) {
		printk(KERN_ALERT "sampling rate not supported\n");
		return i;
	}

	if (soc_static_freq_config) {

		/* We will fix R value to 1 and will make P & J=K.D as varialble */

		/* Setting P & R values */
		dac3100_write(codec, CLK_REG_2,
			      ((dac3100_divs[i].p_val << 4) | 0x01));

		/* J value */
		dac3100_write(codec, CLK_REG_3, dac3100_divs[i].pll_j);

		/* MSB & LSB for D value */
		dac3100_write(codec, CLK_REG_4, (dac3100_divs[i].pll_d >> 8));
		dac3100_write(codec, CLK_REG_5,
			      (dac3100_divs[i].pll_d & dac3100_8BITS_MASK));

		/* NDAC divider value */
		dac3100_write(codec, NDAC_CLK_REG_6, dac3100_divs[i].ndac);

		/* MDAC divider value */
		dac3100_write(codec, MDAC_CLK_REG_7, dac3100_divs[i].mdac);

		/* DOSR MSB & LSB values */
		dac3100_write(codec, DAC_OSR_MSB, dac3100_divs[i].dosr >> 8);
		dac3100_write(codec, DAC_OSR_LSB,
			      dac3100_divs[i].dosr & dac3100_8BITS_MASK);
	}
	/* BCLK N divider */
	dac3100_write(codec, CLK_REG_11, dac3100_divs[i].blck_N);

	data = dac3100_read(codec, INTERFACE_SET_REG_1);

	data = data & ~(3 << 4);

	printk(KERN_ALERT "- Data length: %d\n", params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (dac3100_WORD_LEN_20BITS << DAC_OSR_MSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (dac3100_WORD_LEN_24BITS << DAC_OSR_MSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (dac3100_WORD_LEN_32BITS << DAC_OSR_MSB_SHIFT);
		break;
	}
    /* Write to Page 0 Reg 27 for the Codec Interface control 1 Register */
	dac3100_write(codec, INTERFACE_SET_REG_1, data);

    /* Switch on the Codec into ON State after all the above configuration */
	dac3100_set_bias_level(codec, SND_SOC_BIAS_ON);

    /* The below block is not required since these are RESERVED REGISTERS
     * in the DAc3100 Codec Chipset 
	 * for (j = 0; j < NO_FEATURE_REGS; j++) {
	 * 	dac3100_write(codec,
	 *		      dac3100_divs[i].codec_specific_regs[j].reg_offset,
	 *		      dac3100_divs[i].codec_specific_regs[j].reg_val);
	 *}
     */ 

	DBG("- SET dac3100_hw_params\n");

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_mute
 * Purpose  : This function is to mute or unmute the left and right DAC
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 dac_reg;
	u8 value;
    
	DBG("+ dac3100_mute %d\n", mute);

	dac_reg = dac3100_read(codec, DAC_MUTE_CTRL_REG);
    
    /* Also update the global Playback Status Flag. This is required for biquad update. */
	if (mute) {
        playback_status = 0;
        
        /* Left Channel Volume routed to HPL and gain -12 db */
        dac3100_write(codec, LEFT_ANALOG_HPL, 0x98); 
        /* Right Channel Volume routed to HPR and gain -12 db */
        dac3100_write(codec, RIGHT_ANALOG_HPR, 0x98); 
        /* Left Analog Volume routed to class-D and gain -12 db */
        dac3100_write(codec, LEFT_ANALOG_SPL, 0x98);       
        /* Software Delay for the volume to settle down to -12 db */
        mdelay(100);    
        /* MUTE the DAC */
		dac3100_write(codec, DAC_MUTE_CTRL_REG, dac_reg | MUTE_ON);
        /* MUTE THE Class-D Speaker Driver */
        value = dac3100_read (codec, SPL_DRIVER);
        dac3100_write (codec, SPL_DRIVER, (value & ~0x04));
        
        /* MUTE the Headphone Left and Headphone Right  */
        value = dac3100_read (codec, HPL_DRIVER);
        dac3100_write (codec, HPL_DRIVER, (value & ~0x04));

        value = dac3100_read (codec, HPR_DRIVER);
        dac3100_write (codec, HPR_DRIVER, (value & ~0x04));        
    }
	else {
        playback_status = 1;

        dac_reg &= ~MUTE_ON;
		dac3100_write(codec, DAC_MUTE_CTRL_REG, dac_reg);
        
        /* if headset was not connected, then unmute the Speaker Driver as well */
        if (headset_connected == 0) {
            /* UNMUTE THE Class-D Speaker Driver */
            value = dac3100_read (codec, SPL_DRIVER);
            dac3100_write (codec, SPL_DRIVER, (value | 0x04));
            
            /* Left Analog Volume routed to class-D and gain 0db */
            dac3100_write(codec, LEFT_ANALOG_SPL, 0x80);       
        }    
        else {
            /* UNMUTE the Headphone Left and Headphone Right  */
            value = dac3100_read (codec, HPL_DRIVER);
            dac3100_write (codec, HPL_DRIVER, (value | 0x04));

            value = dac3100_read (codec, HPR_DRIVER);
            dac3100_write (codec, HPR_DRIVER, (value | 0x04));
            
            /* Left Channel Volume routed to HPL and gain 0db */
            dac3100_write(codec, LEFT_ANALOG_HPL, 0x80); 
            /* Right Channel Volume routed to HPR and gain 0db */
            dac3100_write(codec, RIGHT_ANALOG_HPR, 0x80); 
        }
        /* Software Delay for the volume to settle down to 0db */
        mdelay(100);    
    }
	DBG("- dac3100_mute %d\n", mute);

#ifdef DEBUG
	DBG("++ dac3100_dump\n");
	debug_print_registers (codec);
	DBG("-- dac3100_dump\n");
#endif //DEBUG

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_set_dai_sysclk
 * Purpose  : This function is to set the DAI system clock
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct dac3100_priv *dac3100 = codec->private_data;

	DBG("dac3100_set_dai_sysclk clk_id(%d) (%d)\n", clk_id, freq);

	switch (freq) {
	case dac3100_FREQ_12000000:
	case dac3100_FREQ_24000000:
	case dac3100_FREQ_13000000:
		dac3100->sysclk = freq;
		return 0;
	}
	printk(KERN_ALERT "Invalid frequency to set DAI system clock\n");
	return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_set_dai_fmt
 * Purpose  : This function is to set the DAI format
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct dac3100_priv *dac3100 = codec->private_data;
	u8 iface_reg;

	iface_reg = dac3100_read(codec, INTERFACE_SET_REG_1);
	iface_reg = iface_reg & ~(3 << 6 | 3 << 2);

	DBG("+ dac3100_set_dai_fmt (%x) \n", fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		dac3100->master = 1;
		iface_reg |= BIT_CLK_MASTER | WORD_CLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		dac3100->master = 0;
		break;
	default:
		printk(KERN_ALERT "Invalid DAI master/slave interface\n");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg |= (dac3100_DSP_MODE << CLK_REG_3_SHIFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg |= (dac3100_RIGHT_JUSTIFIED_MODE << CLK_REG_3_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg |= (dac3100_LEFT_JUSTIFIED_MODE << CLK_REG_3_SHIFT);
		break;
	default:
		printk(KERN_ALERT "Invalid DAI interface format\n");
		return -EINVAL;
	}

	DBG("- dac3100_set_dai_fmt (%x) \n", iface_reg);
	dac3100_write(codec, INTERFACE_SET_REG_1, iface_reg);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_set_bias_level
 * Purpose  : This function is to get triggered when dapm events occurs.
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	struct dac3100_priv *dac3100 = codec->private_data;
	u8 value;

	DBG("++ dac3100_set_bias_level %d\n", level);

	if (level == codec->bias_level)
		return 0;

	switch (level) {
		/* full On */
	case SND_SOC_BIAS_ON:
		DBG("dac3100_set_bias_level ON\n");
		// WA, enable the gpt11_fclk to avoid audio quality problems when screen OFF
		clk_enable(gpt11_fclk);
		/* all power is driven by DAPM system */
		if (dac3100->master) {
			/* Switch on PLL */
			value = dac3100_read(codec, CLK_REG_2);
			dac3100_write(codec, CLK_REG_2, (value | ENABLE_PLL));

			/* Switch on NDAC Divider */
			value = dac3100_read(codec, NDAC_CLK_REG_6);
			dac3100_write(codec, NDAC_CLK_REG_6, value | ENABLE_NDAC);

			/* Switch on MDAC Divider */
			value = dac3100_read(codec, MDAC_CLK_REG_7);
			dac3100_write(codec, MDAC_CLK_REG_7,
				      value | ENABLE_MDAC);

			/* Switch on BCLK_N Divider */
			value = dac3100_read(codec, CLK_REG_11);
			dac3100_write(codec, CLK_REG_11, value | ENABLE_BCLK);

		    /* Switch ON Left and Right DACs */			
		    value = dac3100_read (codec, DAC_CHN_REG);
            dac3100_write (codec, DAC_CHN_REG, (value | ENABLE_DAC));
                       
            /* We will UNMUTE the DAC here, and the code inside the 
             * dac3100_mute() will be made dummy.
             */           
            /* UNMUTE the Left and Right DACs */
			value = dac3100_read (codec, DAC_MUTE_CTRL_REG);
            value &= ~0x0C;   
            dac3100_write (codec, DAC_MUTE_CTRL_REG, value);

            /* Check for the status of the headset_connected. If 
             * headset was detected, enable and route Audio to Headset
             * or else enable the Speakers 
             */
            if (headset_connected == 0) {
                /* Switch ON the Class_D Speaker Amplifier */   
                value = dac3100_read (codec, CLASS_D_SPK);
                value |= 0x80;
                dac3100_write (codec, CLASS_D_SPK, value);
                DBG ("Speaker Enabled \n");  
                
                /* UNMUTE THE Class-D Speaker Driver */
                value = dac3100_read (codec, SPL_DRIVER);
                dac3100_write (codec, SPL_DRIVER, (value | 0x04));
                
		    /*} else if(headset_connected == 1) {*/
		    } else {

                /* Switch ON Left and Right Headphone Drivers */			
                value = dac3100_read (codec, HPHONE_DRIVERS);
                value |= 0xC0;   
                dac3100_write (codec, HPHONE_DRIVERS, value);
                DBG ("Headset Enabled\n");
                /* UNMUTE the Headphone Left and Headphone Right  */
                value = dac3100_read (codec, HPL_DRIVER);
                value |= 0x04;
                dac3100_write (codec, HPL_DRIVER, value);
   
                value = dac3100_read (codec, HPR_DRIVER);
                value |= 0x04;
                dac3100_write (codec, HPR_DRIVER, value);
		    } 
		}
		break;

		/* partial On */
	case SND_SOC_BIAS_PREPARE:
		 DBG("dac3100_set_bias_level PREPARE\n");
		break;

		/* Off, with power */
	case SND_SOC_BIAS_STANDBY:
		/*
		 * all power is driven by DAPM system,
		 * so output power is safe if bypass was set
		 */
		 DBG("dac3100_set_bias_level STANDBY\n");
		if (dac3100->master) {
            /* FIRST MUTE the Headphone Left and Headphone Right  */
            value = dac3100_read (codec, HPL_DRIVER);
            value &= ~0x84;
            dac3100_write (codec, HPL_DRIVER, value);

            value = dac3100_read (codec, HPR_DRIVER);
            value &= ~0x84;
            dac3100_write (codec, HPR_DRIVER, value);

            /* MUTE THE Class-D Speaker Driver */
            value = dac3100_read (codec, SPL_DRIVER);
            value &= ~0x04;
            dac3100_write (codec, SPL_DRIVER, value);

            /* SECOND MUTE the Left and Right DACs */
			value = dac3100_read (codec, DAC_MUTE_CTRL_REG);
            value |= 0x0C;   
            dac3100_write (codec, DAC_MUTE_CTRL_REG, value);

            /* Switch OFF the Class_D Speaker Amplifier */   
            value = dac3100_read (codec, CLASS_D_SPK);
            value &= ~0x80;
            dac3100_write (codec, CLASS_D_SPK, value);
 
			/* Switch OFF Left and Right DACs */			
			value = dac3100_read (codec, DAC_CHN_REG);
            dac3100_write (codec, DAC_CHN_REG, (value & ~ENABLE_DAC));
  
			/* Switch OFF Left and Right Headphone Drivers */			
			value = dac3100_read (codec, HPHONE_DRIVERS);
            value &= ~0xC0;   
            dac3100_write (codec, HPHONE_DRIVERS, value);

			/* Switch off PLL */
			value = dac3100_read(codec, CLK_REG_2);
			dac3100_write(codec, CLK_REG_2, (value & ~ENABLE_PLL));

			/* Switch off NDAC Divider */
			value = dac3100_read(codec, NDAC_CLK_REG_6);
			dac3100_write(codec, NDAC_CLK_REG_6,
				      value & ~ENABLE_NDAC);

			/* Switch off MDAC Divider */
			value = dac3100_read(codec, MDAC_CLK_REG_7);
			dac3100_write(codec, MDAC_CLK_REG_7,
				      value & ~ENABLE_MDAC);

			/* Switch off BCLK_N Divider */
			dac3100_write(codec, CLK_REG_11, value & ~ENABLE_BCLK);
			if (gpt11_fclk->usecount > 0)
				clk_disable(gpt11_fclk);
		}
		break;

		/* Off, without power */
	case SND_SOC_BIAS_OFF:
		/* force all power off */
		/*
		 * all power is driven by DAPM system,
		 * so output power is safe if bypass was set
		 */
		DBG("dac3100_set_bias_level OFF\n");

		if (dac3100->master) {
            /* FIRST MUTE the Headphone Left and Headphone Right  */
            value = dac3100_read (codec, HPL_DRIVER);
            value &= ~0x84;
            dac3100_write (codec, HPL_DRIVER, value);

            value = dac3100_read (codec, HPR_DRIVER);
            value &= ~0x84;
            dac3100_write (codec, HPR_DRIVER, value);

            /* MUTE THE Class-D Speaker Driver */
            value = dac3100_read (codec, SPL_DRIVER);
            value &= ~0x04;
            dac3100_write (codec, SPL_DRIVER, value);

            /* SECOND MUTE the Left and Right DACs */
			value = dac3100_read (codec, DAC_MUTE_CTRL_REG);
            value |= 0x0C;   
            dac3100_write (codec, DAC_MUTE_CTRL_REG, value);

            /* Switch OFF the Class_D Speaker Amplifier */   
            value = dac3100_read (codec, CLASS_D_SPK);
            value &= ~0x80;
            dac3100_write (codec, CLASS_D_SPK, value);
 
			/* Switch OFF Left and Right DACs */			
			value = dac3100_read (codec, DAC_CHN_REG);
            dac3100_write (codec, DAC_CHN_REG, (value & ~ENABLE_DAC));
  
			/* Switch OFF Left and Right Headphone Drivers */			
			value = dac3100_read (codec, HPHONE_DRIVERS);
            value &= ~0xC0;   
            dac3100_write (codec, HPHONE_DRIVERS, value);

			/* Switch off PLL */
			value = dac3100_read(codec, CLK_REG_2);
			dac3100_write(codec, CLK_REG_2, (value & ~ENABLE_PLL));

			/* Switch off NDAC Divider */
			value = dac3100_read(codec, NDAC_CLK_REG_6);
			dac3100_write(codec, NDAC_CLK_REG_6,
				      value & ~ENABLE_NDAC);

			/* Switch off MDAC Divider */
			value = dac3100_read(codec, MDAC_CLK_REG_7);
			dac3100_write(codec, MDAC_CLK_REG_7,
				      value & ~ENABLE_MDAC);

			/* Switch off BCLK_N Divider */
			dac3100_write(codec, CLK_REG_11, value & ~ENABLE_BCLK);
			if (gpt11_fclk->usecount >0)
				clk_disable(gpt11_fclk);
		}
   		break;
	}
	codec->bias_level = level;
	DBG("-- dac3100_set_bias_level\n");

	return 0;
}


/*
 *----------------------------------------------------------------------------
 * Function : dac3100_hw_free
 * Purpose  : This function is to get triggered when dapm events occurs. 
 *            Note that Android layer will invoke this function during closure
 *            of the Audio Resources. 
 *----------------------------------------------------------------------------
 */
static int dac3100_hw_free (struct snd_pcm_substream *substream, struct snd_soc_dai *device)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec   = socdev->card->codec;

    DBG("+ dac3100_hw_free \n");
	dac3100_set_bias_level (codec, SND_SOC_BIAS_STANDBY);
        
    DBG("- dac3100_hw_free \n");
	return (0);
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_suspend
 * Purpose  : This function is to suspend the dac3100 driver.
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
    int ret, gpio;
	u8 regvalue;
	
	DBG("+ dac3100_suspend\n");

    DBG("dac3100_suspend OFF State\r\n");
	dac3100_set_bias_level(codec, SND_SOC_BIAS_OFF);

    /* Update the Global Variable to indicate that DAC is OFF */
    codec_dac_power_status = 1;

	 /* Perform the Device Soft Power Down */
    regvalue = dac3100_read (codec, MICBIAS_CTRL);
    dac3100_write (codec, MICBIAS_CTRL, (regvalue | 0x80));

	/* Turn OFF the Power GPIO which supplies the AVDD Input to Audio Codec */
    gpio = AUDIO_CODEC_PWR_ON_GPIO;
	ret = gpio_request(gpio, AUDIO_CODEC_PWR_ON_GPIO_NAME);
	gpio_set_value(gpio, 0);

	DBG("-dac3100_suspend\n");

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_resume
 * Purpose  : This function is to resume the dac3100 driver
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
    int ret, gpio;
	u8 regvalue;
	u8 *cache = codec->reg_cache;

	DBG("+ dac3100_resume\n");
	DBG("dac3100_resume: Level %d\r\n", codec->suspend_bias_level);
	/* Turn ON the Power GPIO which supplies the VDD Input to Audio Codec */
     gpio = AUDIO_CODEC_PWR_ON_GPIO;
	ret = gpio_request(gpio, AUDIO_CODEC_PWR_ON_GPIO_NAME);
	gpio_direction_output(gpio, 1);

    udelay(100);

    /* Perform the Device Soft Power UP */
    regvalue = dac3100_read (codec, MICBIAS_CTRL);
    dac3100_write (codec, MICBIAS_CTRL, (regvalue & ~0x80));
	
	/* Add deley of 100 ms */
	msleep(100);

	dac3100_change_page(codec, 0);
	dac3100_set_bias_level(codec, codec->suspend_bias_level);

	DBG("- dac3100_resume\n");

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tlv320dac3100_init
 * Purpose  : This function is to initialise the dac3100 driver
 *            register the mixer and codec interfaces with the kernel.
 *            
 *----------------------------------------------------------------------------
 */
 #define TRITON_AUDIO_IF_PAGE 	0x1
static int tlv320dac3100_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	struct dac3100_priv *dac3100 = codec->private_data;
	int ret = 0;
	int i = 0;
	int hph_detect_gpio = AUDIO_CODEC_HPH_DETECT_GPIO;
	int hph_detect_irq = 0;

	printk(KERN_ALERT "+tlv320dac3100_init\n");

	ret = gpio_request(hph_detect_gpio, "dac3100-headset");

	if (ret < 0) {
		goto err1;
	}
	gpio_direction_input(hph_detect_gpio);
	omap_set_gpio_debounce(hph_detect_gpio,1);
	omap_set_gpio_debounce_time(hph_detect_gpio,0xFF);
	hph_detect_irq = OMAP_GPIO_IRQ(hph_detect_gpio);

	codec->name = "dac3100";
	codec->owner = THIS_MODULE;
	codec->read = dac3100_read;
	codec->write = dac3100_write;
	/* Mistral: Enabled the bias_level routine */	
	codec->set_bias_level = dac3100_set_bias_level;
	codec->dai = &tlv320dac3100_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(dac3100_reg);

	codec->reg_cache =
	    kmemdup(dac3100_reg, sizeof(dac3100_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		printk(KERN_ERR "dac3100: kmemdup failed\n");
		return -ENOMEM;
	}

	dac3100->page_no = 0;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "dac3100: failed to create pcms\n");
		goto pcm_err;
	}

	printk(KERN_ALERT "*** Configuring dac3100 registers ***\n");
	for (i = 0; i < sizeof(dac3100_reg_init) / sizeof(struct dac3100_configs); i++) {
		dac3100_write(codec, dac3100_reg_init[i].reg_offset, dac3100_reg_init[i].reg_val);
        mdelay (20); /* Added delay across register writes */
	}
	printk(KERN_ALERT "*** Done Configuring dac3100 registers ***\n");

	ret = request_irq(hph_detect_irq, dac3100_irq_handler,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED | IRQF_SHARED , "dac3100", codec);

	/* off, with power on */
	//dac3100_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	dac3100_add_controls(codec);
	dac3100_add_widgets(codec);

#ifdef CONFIG_ADAPTIVE_FILTER
	dac3100_add_biquads_controls (codec);

	dac3100_add_EQ_mixer_controls (codec);
    
    dac3100_parse_biquad_array (codec);
   
#endif 
    /* Moved the dac3100_headset_speaker_path here after the biquad configuration */
    dac3100_headset_speaker_path(codec);

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "dac3100: failed to register card\n");
		goto card_err;
	}

	  printk(KERN_ALERT "-tlv320dac3100_init\n");

	return ret;

err1:
	free_irq(hph_detect_irq, codec);

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_headset_speaker_path
 * Purpose  : This function is to check for the presence of Headset and
 *            configure the Headphone of the Class D Speaker driver 
 *            Registers appropriately.
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_headset_speaker_path(struct snd_soc_codec *codec)
{
	int headset_detect_gpio = AUDIO_CODEC_HPH_DETECT_GPIO;
	int headset_present = 0;
	u8 value;

	headset_present = gpio_get_value(headset_detect_gpio);

	if(!headset_present) {
		// Headset
        /* Before powering up the Headphone Drivers, check if there is any playback request active */
        if (playback_status == 1) {
            printk(KERN_ALERT "headset present and headset path Activated\n");
            dac3100_write(codec, HPHONE_DRIVERS, 0xC4); // ON
            dac3100_write(codec, CLASS_D_SPK, 0x06); // OFF
        
            /* MUTE THE Class-D Speaker Driver */
            value = dac3100_read (codec, SPL_DRIVER);
            dac3100_write (codec, SPL_DRIVER, (value & ~0x04));
            
            /* UNMUTE the Headphone Left and Headphone Right  */
            value = dac3100_read (codec, HPL_DRIVER);
            dac3100_write (codec, HPL_DRIVER, (value | 0x04));

            value = dac3100_read (codec, HPR_DRIVER);
            dac3100_write (codec, HPR_DRIVER, (value | 0x04));
            
        } else
            printk(KERN_ALERT "Headset present, but no playback request\n");

		headset_connected = 1;
	} else {
		//SPK
        /* First check if the playback request is active */
        if (playback_status == 1) {
            printk(KERN_ALERT "headset removed and headset path Deactivated\n");
		
            dac3100_write(codec, HPHONE_DRIVERS ,0x4); // OFF
            dac3100_write(codec, CLASS_D_SPK ,0xC6 ); //ON
        
            /* UNMUTE THE Class-D Speaker Driver */
            value = dac3100_read (codec, SPL_DRIVER);
            dac3100_write (codec, SPL_DRIVER, (value | 0x04));
        } else
            printk(KERN_ALERT "Headset removed, but no playback request\n");
        
		headset_connected = 0;
	}
    /* Update the Biquad Array */
    dac3100_update_biquad_array (codec, headset_present, playback_status);
	return 0;
}

/*
 * This interrupt is called when HEADSET is insert or remove from conector.
   On this interup sound will be rouote to HEadset or speaker path.
 */
static irqreturn_t dac3100_irq_handler(int irq, struct snd_soc_codec *codec)
{
	printk(KERN_DEBUG "interrupt of headset found\n");
	//disable_irq(irq);
	codec_work_var_glob = codec;
	schedule_work(&works);
	//enable_irq(irq);
	return IRQ_HANDLED;
}

/*
 *----------------------------------------------------------------------------
 * Function : i2c_dac3100_headset_access_work
 * Purpose  : Worker Thread Function.
 *            
 *----------------------------------------------------------------------------
 */
static void i2c_dac3100_headset_access_work(struct work_struct *work)
{
	dac3100_headset_speaker_path(codec_work_var_glob);
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 *----------------------------------------------------------------------------
 * Function : tlv320dac3100_codec_probe
 * Purpose  : This function attaches the i2c client and initializes 
 *				dac3100 CODEC.
 *            NOTE:
 *            This function is called from i2c core when the I2C address is
 *            valid.
 *            If the i2c layer weren't so broken, we could pass this kind of 
 *            data around
 *            
 *----------------------------------------------------------------------------
 */
static int tlv320dac3100_codec_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = dac3100_socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = tlv320dac3100_init(socdev);
	INIT_WORK(&works, i2c_dac3100_headset_access_work);
	if (ret < 0) {
		printk(KERN_ERR "dac3100: failed to attach codec at addr\n");
		return -1;
	}

	return ret;
}

/*
 *----------------------------------------------------------------------------
 * Function : tlv320dac3100_i2c_remove
 * Purpose  : This function removes the i2c client and uninitializes 
 *                              DAC3100 CODEC.
 *            NOTE:
 *            This function is called from i2c core 
 *            If the i2c layer weren't so broken, we could pass this kind of 
 *            data around
 *            
 *----------------------------------------------------------------------------
 */
static int __exit tlv320dac3100_i2c_remove(struct i2c_client *i2c)
{
	struct snd_soc_codec *codec = i2c_get_clientdata (i2c);
        kfree (codec->reg_cache);

        put_device(&i2c->dev);
        return 0;
}

/* i2c Device ID Struct used during Driver Initialization and Shutdown */
static const struct i2c_device_id tlv320dac3100_id[] = {
        {"tlv320dac3100", 0},
        {}
};

MODULE_DEVICE_TABLE(i2c, tlv320dac3100_id);

/* Definition of the struct i2c_driver structure */
static struct i2c_driver tlv320dac3100_i2c_driver = {
	.driver = {
		.name = "tlv320dac3100",
	},
	.probe = tlv320dac3100_codec_probe,
	.remove = __exit_p(tlv320dac3100_i2c_remove),
	.id_table = tlv320dac3100_id,
};

#endif //#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_probe
 * Purpose  : This is first driver function called by the SoC core driver.
 *            
 *----------------------------------------------------------------------------
 */

static unsigned int i2c_master_recv_wrapper(struct snd_soc_codec *codec, unsigned int ignored) {
	u8 value;
	if(i2c_master_recv(codec->control_data, &value, 1) != 1) {
		printk(KERN_ALERT " *** Error performing i2c_recv for tlv320dac3100 soc codec! ***");
	}
	return (unsigned int)value;
}

static int dac3100_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	struct dac3100_priv *dac3100;
	int ret = 0;
	
	int gpio = AUDIO_CODEC_PWR_ON_GPIO;

	ret = gpio_request(gpio, AUDIO_CODEC_PWR_ON_GPIO_NAME);
	gpio_direction_output(gpio, 0);
	gpio_set_value(gpio, 1);

	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if(codec == NULL)
		return -ENOMEM;

	dac3100 = kzalloc(sizeof(struct dac3100_priv), GFP_KERNEL);
	if (dac3100 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = dac3100;
	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	dac3100_socdev = socdev;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
		codec->hw_write = (hw_write_t) i2c_master_send;
		codec->hw_read = i2c_master_recv_wrapper;
		ret = i2c_add_driver(&tlv320dac3100_i2c_driver);
		if (ret != 0)
			printk(KERN_ERR "Can't add TLV320dac3100 i2c driver!!");
#else
	/* Add other interfaces here */
#endif

	/* Audiorecovery: Save codec struct */
	AudioRecovery_codec = codec;
	AudioRecovery_socdev = socdev;

	return ret;
}

/*
 *----------------------------------------------------------------------------
 * Function : dac3100_remove
 * Purpose  : to remove dac3100 soc device 
 *            
 *----------------------------------------------------------------------------
 */
static int dac3100_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	/* power down chip */
        /* Mistral: there is no code currently for SND_SOC_BIAS_OFF. Need to Change it */
	if (codec->control_data)
		dac3100_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&tlv320dac3100_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

/* AUDIO NOISE ISSUE : =======================================================*/

void AudioRecovery_dump (void)
{
	int i;
	u32 data;
	struct snd_soc_codec * codec = AudioRecovery_codec;

    /* Dump for Page 0 */
	for (i = 0 ; i < 80 ; i++) {
		data = dac3100_read(codec, i);
		printk(KERN_ALERT "Pg 0 reg = %d val = %x\n", i, data);
	}
    /* Dump for Page 1 */
	for (i = 158 ; i < 174 ; i++) {
		data = dac3100_read(codec, i);
		printk(KERN_ALERT "Pg 1 reg = %d val = %x\n", (i % PAGE_1), data);
	}
   
}

#define AUDIO_CODEC_POWER_ENABLE_GPIO    103
#define AUDIO_CODEC_RESET_GPIO           37
#define AUDIO_CODEC_IRQ_GPIO             59
#define AIC3100_NAME "tlv320dac3100"
#define AIC3100_I2CSLAVEADDRESS 0x18

void AudioRecovery_recovery (void)
{
	int i;	
	
	printk(KERN_ALERT "AUDIO NOISE RECOVERY !!!!!");

	/* STEP 1: Turn OFF Audio Codec and put it under reset */
       	gpio_request(AUDIO_CODEC_RESET_GPIO, "AUDIO_CODEC_RESET_GPIO");
       	gpio_direction_output(AUDIO_CODEC_RESET_GPIO, 0);
	gpio_set_value(AUDIO_CODEC_RESET_GPIO, 0);

        gpio_request(AUDIO_CODEC_POWER_ENABLE_GPIO, "AUDIO DAC3100 POWER ENABLE"); 
     	gpio_direction_output(AUDIO_CODEC_POWER_ENABLE_GPIO, 0);
	gpio_set_value(AUDIO_CODEC_POWER_ENABLE_GPIO, 0);

	/* STEP 2: Turn ON Audio Codec and release reset */
      
	mdelay (100);

	gpio_set_value(AUDIO_CODEC_POWER_ENABLE_GPIO, 1);

	 mdelay (100);

       	gpio_set_value(AUDIO_CODEC_RESET_GPIO, 1);

	/* STEP 3: reprogramm Audio Codec Register */
    
	printk(KERN_ALERT "*** Configuring dac3100 registers ***\n");
	for (i = 0; i < sizeof(dac3100_reg_init) / sizeof(struct dac3100_configs); i++) {
		dac3100_write(AudioRecovery_codec, dac3100_reg_init[i].reg_offset, dac3100_reg_init[i].reg_val);
        	mdelay (20); /* Added delay across register writes */
	}
	printk(KERN_ALERT "*** Done Configuring dac3100 registers ***\n");
	
	dac3100_headset_speaker_path(AudioRecovery_codec);

	

}

/* ========================================================= */

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions dac3100_probe(), dac3100_remove(),  
 *          dac3100_suspend() and dac3100_resume()
 *
 */
struct snd_soc_codec_device soc_codec_dev_dac3100 = {
	.probe = dac3100_probe,
	.remove = dac3100_remove,
	.suspend = dac3100_suspend,
	.resume = dac3100_resume,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_dac3100);

static int __init tlv320dac3100_modinit(void)
{
	return snd_soc_register_dai(&tlv320dac3100_dai);
}

module_init(tlv320dac3100_modinit);

static void __exit tlv320dac3100_exit(void)
{
	snd_soc_unregister_dai(&tlv320dac3100_dai);
}

module_exit(tlv320dac3100_exit);

MODULE_DESCRIPTION("ASoC TLV320dac3100 codec driver");
MODULE_AUTHOR("jazbjohn@mistralsolutions.com");
MODULE_LICENSE("GPL");

