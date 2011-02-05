/*
 * linux/sound/soc/codecs/tlv320dac3100.h
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
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
 * Rev 0.1	Created the file		21-Apr-2010
 * 
 * Rev 0.2	Cleaned up the header file	27-Jun-2010
 *
 * Rev 0.3	Updated the supported Sampling frequencies using the
 *              dac3100_RATES macro		21-Jul-2010
 */

#ifndef _TLV320dac3100_H
#define _TLV320dac3100_H

#define AUDIO_NAME "dac3100"
#define dac3100_VERSION "1.0"

#define TLV320dac3100ID                  (0x18)	


/* Mistral: removed the earlier CONFIG_MINIDSP flag since the DAC3100 does not have
 * a miniDSP configured.
 */
#define CONFIG_TILOAD
//#undef CONFIG_TILOAD

/* Build Macro for enabling/disabling the Adaptive Filtering */
#define CONFIG_ADAPTIVE_FILTER
//#undef CONFIG_ADAPTIVE_FILTER

/* Enable register caching on write */
#define EN_REG_CACHE

/* dac3100 supported sample rate are 8k to 192k */
//#define dac3100_RATES	SNDRV_PCM_RATE_8000_192000
#define dac3100_RATES	(SNDRV_PCM_RATE_8000_48000 )


/* dac3100 supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define dac3100_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define dac3100_FREQ_12000000 12000000      //to be check
#define dac3100_FREQ_24000000 24000000     //to be check
#define dac3100_FREQ_13000000 13000000lu

/* Audio data word length = 16-bits (default setting) */
#define dac3100_WORD_LEN_16BITS		0x00
#define dac3100_WORD_LEN_20BITS		0x01
#define dac3100_WORD_LEN_24BITS		0x02
#define dac3100_WORD_LEN_32BITS		0x03

/* sink: name of target widget */
#define dac3100_WIDGET_NAME			    0
/* control: mixer control name */
#define dac3100_CONTROL_NAME			1
/* source: name of source name */
#define dac3100_SOURCE_NAME			    2

/* D15..D8 dac3100 register offset */
#define dac3100_REG_OFFSET_INDEX        0
/* D7...D0 register data */
#define dac3100_REG_DATA_INDEX          1

/* Serial data bus uses I2S mode (Default mode) */
#define dac3100_I2S_MODE				0x00
#define dac3100_DSP_MODE				0x01
#define dac3100_RIGHT_JUSTIFIED_MODE	0x02
#define dac3100_LEFT_JUSTIFIED_MODE	0x03

/* 8 bit mask value */
#define dac3100_8BITS_MASK              0xFF

/* shift value for CLK_REG_3 register */
#define CLK_REG_3_SHIFT					6	//to be check
/* shift value for DAC_OSR_MSB register */
#define DAC_OSR_MSB_SHIFT				4	//to be check

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS     			2		// 2 to be check

/* dac3100 register space */
#define	dac3100_CACHEREGNUM			256

/* ****************** Page 0 Registers **************************************/
/* Page select register */
#define	PAGE_SELECT			    0
/* Software reset register */
#define	RESET				    1
/* OT FLAG register */
#define OT_FLAG				    3
/* Clock clock Gen muxing, Multiplexers*/
#define	CLK_REG_1			    4
/* PLL P and R-VAL register*/
#define	CLK_REG_2			    5
/* PLL J-VAL register*/
#define	CLK_REG_3			    6
/* PLL D-VAL MSB register */
#define	CLK_REG_4			    7
/* PLL D-VAL LSB register */
#define	CLK_REG_5			    8
/* DAC NDAC_VAL register*/
#define	NDAC_CLK_REG_6		        11
/* DAC MDAC_VAL register */
#define	MDAC_CLK_REG_7		        12
/*DAC OSR setting register1,MSB value*/
#define DAC_OSR_MSB			        13
/*DAC OSR setting register 2,LSB value*/
#define DAC_OSR_LSB			        14
/*Clock setting register 8, PLL*/
#define	NADC_CLK_REG_8		        18
/*Clock setting register 9, PLL*/
#define	MADC_CLK_REG_9		        19
/*ADC Oversampling (AOSR) Register*/
#define ADC_OSR_REG			20
/*Clock setting register 9, Multiplexers*/
#define CLK_MUX_REG_9		        25
/*Clock setting register 10, CLOCKOUT M divider value*/
#define CLK_REG_10			        26
/*Audio Interface Setting Register 1*/
#define INTERFACE_SET_REG_1	        27
/*Audio Interface Setting Register 2*/
#define AIS_REG_2			        28
/*Audio Interface Setting Register 3*/
#define AIS_REG_3			        29
/*Clock setting register 11,BCLK N Divider*/
#define CLK_REG_11			        30
/*Audio Interface Setting Register 4,Secondary Audio Interface*/
#define AIS_REG_4			        31
/*Audio Interface Setting Register 5*/
#define AIS_REG_5			        32
/*Audio Interface Setting Register 6*/
#define AIS_REG_6			        33
/* I2C Bus Condition */
#define I2C_FLAG				    34
/* DAC Flag Registers */
#define DAC_FLAG_1				    37
#define DAC_FLAG_2				    38
/* Interrupt flag (overflow) */
#define OVERFLOW_FLAG				39
/* Interrupt flags (DAC */
#define INTR_FLAG_1				    44
#define INTR_FLAG_2				    46
/* INT1 interrupt control */
#define INT1_CTRL				    48
/* INT2 interrupt control */
#define INT2_CTRL				    49
/* GPIO1 control */
#define GPIO1_CTRL				    51

 /**/
/*DOUT Function Control*/
#define DOUT_CTRL				    53
/*DIN Function Control*/
#define DIN_CTL					    54
/*DAC Instruction Set Register*/
#define DAC_INSTRUCTION_SET			60
/*DAC channel setup register*/
#define DAC_CHN_REG				    63
/*DAC Mute and volume control register*/
#define DAC_MUTE_CTRL_REG			64
/*Left DAC channel digital volume control*/
#define LDAC_VOL				    65
/*Right DAC channel digital volume control*/
#define RDAC_VOL				    66
/* Headset detection */
#define HS_DETECT				    67
/* DRC Control Registers */
#define DRC_CTRL_1				    68
#define DRC_CTRL_2				    69
#define DRC_CTRL_3				    70
/* Beep Generator */
#define BEEP_GEN_L				    71
#define BEEP_GEN_R				    72
/* Beep Length */
#define BEEP_LEN_MSB				73
#define BEEP_LEN_MID				74
#define BEEP_LEN_LSB				75
/* Beep Functions */
#define BEEP_SINX_MSB				76
#define BEEP_SINX_LSB				77
#define BEEP_COSX_MSB				78
#define BEEP_COSX_LSB				79

/*Channel AGC Control Register 1*/
#define CHN_AGC_1			        86
/*Channel AGC Control Register 2*/
#define CHN_AGC_2			        87
/*Channel AGC Control Register 3 */
#define CHN_AGC_3			        88
/*Channel AGC Control Register 4 */
#define CHN_AGC_4			        89
/*Channel AGC Control Register 5 */
#define CHN_AGC_5			        90
/*Channel AGC Control Register 6 */
#define CHN_AGC_6			        91
/*Channel AGC Control Register 7 */
#define CHN_AGC_7			        92
/* VOL/MICDET-Pin SAR ADC Volume Control */
#define VOL_MICDECT_ADC			    116
/* VOL/MICDET-Pin Gain*/
#define VOL_MICDECT_GAIN		    117

/******************** Page 1 Registers **************************************/
#define PAGE_1				        128
/* Headphone drivers */
#define HPHONE_DRIVERS			    (PAGE_1 + 31)
/* Class-D Speakear Amplifier */
#define CLASS_D_SPK			        (PAGE_1 + 32)
/* HP Output Drivers POP Removal Settings */
#define HP_OUT_DRIVERS			    (PAGE_1 + 33)
/* Output Driver PGA Ramp-Down Period Control */
#define PGA_RAMP			        (PAGE_1 + 34)
/* DAC_L and DAC_R Output Mixer Routing */
#define DAC_MIXER_ROUTING   		(PAGE_1 + 35)
/*Left Analog Vol to HPL */
#define LEFT_ANALOG_HPL			    (PAGE_1 + 36)
/* Right Analog Vol to HPR */
#define RIGHT_ANALOG_HPR		    (PAGE_1 + 37)
/* Left Analog Vol to SPL */
#define LEFT_ANALOG_SPL			    (PAGE_1 + 38)
/* Right Analog Vol to SPR */
#define RIGHT_ANALOG_SPR		    (PAGE_1 + 39)
/* HPL Driver */
#define HPL_DRIVER			        (PAGE_1 + 40)
/* HPR Driver */
#define HPR_DRIVER			        (PAGE_1 + 41)
/* SPL Driver */
#define SPL_DRIVER			        (PAGE_1 + 42)
/* SPR Driver */
#define SPR_DRIVER			        (PAGE_1 + 43)
/* HP Driver Control */
#define HP_DRIVER_CONTROL		    (PAGE_1 + 44)
/*MICBIAS Configuration Register*/
#define MICBIAS_CTRL			    (PAGE_1 + 46) 	// (PAGE_1 + 51)
/* MIC PGA*/
#define MICPGA_VOL_CTRL			    (PAGE_1 + 47)
/* Delta-Sigma Mono ADC Channel Fine-Gain Input Selection for P-Terminal */
#define MICPGA_PIN_CFG			    (PAGE_1 + 48)
/* ADC Input Selection for M-Terminal */
#define MICPGA_MIN_CFG			    (PAGE_1 + 49)
/* Input CM Settings */
#define MICPGA_CM			        (PAGE_1 + 50)
/*MICBIAS Configuration*/
#define MICBIAS				        (PAGE_1 + 46)  // (PAGE_1 + 51)

/****************************************************************************/
/*  Page 3 Registers 				              	  	    */
/****************************************************************************/
#define PAGE_3				        (128 * 3)

/* Timer Clock MCLK Divider */
#define TIMER_MCLK_DIV			    (PAGE_3 + 16)

/****************************************************************************/
#define BIT7					    (0x01 << 7)
#define CODEC_CLKIN_MASK			0x03

#define MCLK_2_CODEC_CLKIN			0x00
#define PLLCLK_2_CODEC_CLKIN		0x03

/*Bclk_in selection*/
#define BDIV_CLKIN_MASK				0x03
#define	DAC_MOD_CLK_2_BDIV_CLKIN 	0x01
#define SOFT_RESET				    0x01
#define PAGE0					    0x00
#define PAGE1					    0x01
#define BIT_CLK_MASTER				0x08
#define WORD_CLK_MASTER				0x04
#define ENABLE_PLL				    BIT7
#define ENABLE_NDAC				    BIT7
#define ENABLE_MDAC				    BIT7
#define ENABLE_NADC				    BIT7
#define ENABLE_MADC				    BIT7
#define ENABLE_BCLK				    BIT7
#define ENABLE_DAC				    (0x03 << 6)
#define LDAC_2_LCHN				    (0x01 << 4 )
#define RDAC_2_RCHN				    (0x01 << 2 )
#define SOFT_STEP_2WCLK				(0x01)
#define MUTE_ON					    0x0C
#define DEFAULT_VOL				    0x0
#define HEADSET_ON_OFF				0xC0

/* DAC volume normalization, 0=-127dB, 127=0dB, 175=+48dB */
#define DAC_MAX_VOLUME                  175
#define DAC_POS_VOL                     127

/****************************************************************************/
/*  DAPM related Enum Defines  		              	  	            */
/****************************************************************************/

#define LEFT_DAC_MUTE_ENUM	0
#define RIGHT_DAC_MUTE_ENUM 	1
#define HP_DRIVER_VOLTAGE_ENUM  2
#define DRC_STATUS_ENUM         3 
/*
 ***************************************************************************** 
 * Structures Definitions
 ***************************************************************************** 
 */
/*
 *----------------------------------------------------------------------------
 * @struct  dac3100_setup_data |
 *          i2c specific data setup for dac3100.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
    struct dac3100_setup_data {
	unsigned short i2c_address;
};

/*
 *----------------------------------------------------------------------------
 * @struct  dac3100_priv |
 *          dac3100 priviate data structure to set the system clock, mode and
 *          page number. 
 * @field   u32 | sysclk |
 *          system clock
 * @field   s32 | master |
 *          master/slave mode setting for dac3100
 * @field   u8 | page_no |
 *          page number. Here, page 0 and page 1 are used.
 *----------------------------------------------------------------------------
 */
struct dac3100_priv {
	u32 sysclk;
	s32 master;
	u8 page_no;
};

/*
 *----------------------------------------------------------------------------
 * @struct  dac3100_configs |
 *          dac3100 initialization data which has register offset and register 
 *          value.
 * @field   u16 | reg_offset |
 *          dac3100 Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the dac3100 register to initialize the dac3100
 *----------------------------------------------------------------------------
 */
struct dac3100_configs {
	u8 reg_offset;
	u8 reg_val;
};

/*
 *----------------------------------------------------------------------------
 * @struct  dac3100_rate_divs |
 *          Setting up the values to get different freqencies 
 *          
 * @field   u32 | mclk |
 *          Master clock 
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | blck_N |
 *          value for block N
 * @field   u32 | aic3254_configs |
 *          configurations for dac3100 register value
 *----------------------------------------------------------------------------
 */
struct dac3100_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 blck_N;
	struct dac3100_configs codec_specific_regs[NO_FEATURE_REGS];
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz., 
 *          playback and capture, DAI runtime information viz. state of DAI 
 *			and pop wait state, and DAI private data. 
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai tlv320dac3100_dai;

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions dac3100_probe(), dac3100_remove(), 
 *			dac3100_suspend() and ai3111_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_dac3100;

#endif				/* _TLV320dac3100_H */
