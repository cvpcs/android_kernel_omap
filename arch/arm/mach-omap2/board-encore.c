
/*
 *
 * Copyright (C) 2008 Texas Instruments Inc.
 * Vikram Pandita <vikram.pandita@ti.com>
 *
 * Modified from mach-omap2/board-ldp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/synaptics_i2c_rmi.h>
#ifdef CONFIG_TOUCHSCREEN_PIXCIR_I2C
#include <linux/pixcir_i2c_s32.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_CYTTSP_I2C
#include <linux/cyttsp.h>
#endif

#ifdef CONFIG_INPUT_KXTF9
#include <linux/kxtf9.h>
#define KXTF9_DEVICE_ID			"kxtf9"
#define KXTF9_I2C_SLAVE_ADDRESS		0x0F
#define KXTF9_GPIO_FOR_PWR		34
#define	KXTF9_GPIO_FOR_IRQ		113
#endif /* CONFIG_INPUT_KXTF9 */

#ifdef CONFIG_BATTERY_MAX17042
#include <linux/max17042.h>
#endif

#ifdef CONFIG_MAX9635
#include <linux/max9635.h>
#define MAX9635_I2C_SLAVE_ADDRESS   MAX9635_I2C_SLAVE_ADDRESS1
#define MAX9635_GPIO_FOR_IRQ        0
#define MAX9635_DEFAULT_POLL_INTERVAL 1000
#endif

#include <linux/spi/spi.h>
#include <linux/i2c/twl4030.h>
#include <linux/interrupt.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/switch.h>
#include <linux/dma-mapping.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/board-boxer.h>
#include <plat/mcspi.h>
#include <mach/gpio.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/gpmc.h>
#if 0
#include <mach/hsmmc.h>
#endif
#include <plat/usb.h>
#include <plat/mux.h>

#include <asm/system.h> // For system_serial_high & system_serial_low
#include <asm/io.h>
#include <asm/delay.h>
#include <plat/control.h>
#include <plat/sram.h>

#include <plat/display.h>
#include <linux/i2c/twl4030.h>

#include <linux/usb/android_composite.h>

#include "mmc-twl4030.h"
#include "omap3-opp.h"
#include "prcm-common.h"

#if defined(CONFIG_MACH_ENCORE) && defined (CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB)
#include "sdram-samsung-k4x4g303pb-or-hynix-h8mbx00u0mer-0em.h"
#elif defined(CONFIG_MACH_ENCORE) && defined (CONFIG_MACH_SDRAM_SAMSUNG_K4X4G303PB)
#include "sdram-samsung-k4x4g303pb.h"
#elif defined(CONFIG_MACH_ENCORE) && defined(CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM)
#include "sdram-hynix-h8mbx00u0mer-0em.h"
#endif

#include <media/v4l2-int-device.h>

#ifdef CONFIG_PM
#include <../drivers/media/video/omap-vout/omapvout.h>
#endif

#ifndef CONFIG_TWL4030_CORE
#error "no power companion board defined!"
#endif

#ifdef CONFIG_WL127X_RFKILL
#include <linux/wl127x-rfkill.h>
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <linux/bootmem.h>
#endif

 
#define DEFAULT_BACKLIGHT_BRIGHTNESS 105

#ifdef CONFIG_TOUCHSCREEN_CYTTSP_I2C
/* tma340 i2c address */
#define CYTTSP_I2C_SLAVEADDRESS 34
#define OMAP_CYTTSP_GPIO        99
#define OMAP_CYTTSP_RESET_GPIO 46
#endif

#define CONFIG_DISABLE_HFCLK 1
#define ENABLE_VAUX1_DEDICATED	0x03
#define ENABLE_VAUX1_DEV_GRP	0x20

#define ENABLE_VAUX3_DEDICATED  0x03
#define ENABLE_VAUX3_DEV_GRP	0x20
#define TWL4030_MSECURE_GPIO	22

#define WL127X_BTEN_GPIO	60

#define BOXER_EXT_QUART_PHYS	0x48000000
#define BOXER_EXT_QUART_VIRT	0xfa000000
#define BOXER_EXT_QUART_SIZE	SZ_256

#ifdef CONFIG_WL127X_RFKILL
#if 0
static struct wl127x_rfkill_platform_data wl127x_plat_data = {
	.bt_nshutdown_gpio = 109,	/* UART_GPIO (spare) Enable GPIO */
	.fm_enable_gpio = 161,		/* FM Enable GPIO */
};

static struct platform_device boxer_wl127x_device = {
	.name           = "wl127x-rfkill",
	.id             = -1,
	.dev.platform_data = &wl127x_plat_data,
};
#endif
#endif


static int boxer_twl4030_keymap[] = {
	KEY(0, 0, KEY_HOME),
	KEY(0, 1, KEY_VOLUMEUP),
	KEY(0, 2, KEY_VOLUMEDOWN),
	0
};

static struct matrix_keymap_data boxer_twl4030_keymap_data = {
	.keymap			= boxer_twl4030_keymap,
	.keymap_size	= ARRAY_SIZE(boxer_twl4030_keymap),
};

static struct twl4030_keypad_data boxer_kp_twl4030_data = {
	.rows			= 8,
	.cols			= 8,
	.keymap_data	= &boxer_twl4030_keymap_data,
	.rep			= 1,
};

// HOME key code for HW > EVT2A
static struct gpio_keys_button boxer_gpio_buttons[] = {
	{
		.code			= KEY_POWER,
		.gpio			= 14,
		.desc			= "POWER",
		.active_low		= 0,
		.wakeup			= 1,
	},
	{
		.code			= KEY_HOME,
		.gpio			= 48,
		.desc			= "HOME",
		.active_low		= 1,
		.wakeup			= 1,
	},
};

static struct gpio_keys_platform_data boxer_gpio_key_info = {
	.buttons	= boxer_gpio_buttons,
	.nbuttons	= ARRAY_SIZE(boxer_gpio_buttons),
//	.rep		= 1,		/* auto-repeat */
};

static struct platform_device boxer_keys_gpio = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &boxer_gpio_key_info,
	},
};

static struct omap2_mcspi_device_config boxer_lcd_mcspi_config = {
	.turbo_mode		= 0,
	.single_channel		= 1,
};

static struct spi_board_info boxer_spi_board_info[] __initdata = {
	[0] = {
		.modalias		= "boxer_disp_spi",
		.bus_num		= 4,	/* McSPI4 */
		.chip_select		= 0,
		.max_speed_hz		= 375000,
		.controller_data	= &boxer_lcd_mcspi_config,
	},
};

#define LCD_EN_GPIO                     36
#define LCD_BACKLIGHT_GPIO              58
#define LCD_BACKLIGHT_EN_EVT2           47

#define LCD_CABC0_GPIO					44
#define LCD_CABC1_GPIO					45

#define PM_RECEIVER			TWL4030_MODULE_PM_RECEIVER
#define ENABLE_VAUX2_DEDICATED		0x09
#define ENABLE_VAUX2_DEV_GRP		0x20
#define ENABLE_VAUX3_DEDICATED		0x03
#define ENABLE_VAUX3_DEV_GRP		0x20

#define ENABLE_VPLL2_DEDICATED          0x05
#define ENABLE_VPLL2_DEV_GRP            0xE0
#define TWL4030_VPLL2_DEV_GRP           0x33
#define TWL4030_VPLL2_DEDICATED         0x36

#define t2_out(c, r, v) twl4030_i2c_write_u8(c, r, v)

// Panel on completion to signal when panel is on.
static DECLARE_COMPLETION(panel_on);

static int boxer_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	complete_all(&panel_on);	
	//omap_pm_set_min_bus_tput(&dssdev->dev, OCP_INITIATOR_AGENT,166 * 1000 * 4);
	return 0;
}

static void boxer_panel_disable_lcd(struct omap_dss_device *dssdev)
{
	INIT_COMPLETION(panel_on);
	/* disabling LCD on Boxer EVT1 shuts down SPI bus, so don't touch! */
    omap_pm_set_min_bus_tput(&dssdev->dev, OCP_INITIATOR_AGENT, 0);
}

/* EVT1 is connected to full 24 bit display panel so gamma table correction */
/* is only to compensate for LED backlight color */
/*
 * omap2dss - fixed size: 256 elements each four bytes / XRGB
 */
#define RED_MASK 	0x00FF0000
#define GREEN_MASK 	0x0000FF00
#define BLUE_MASK	0x000000FF
#define RED_SHIFT	16
#define GREEN_SHIFT	8
#define BLUE_SHIFT	0
#define MAX_COLOR_DEPTH	255

static const u8 led_color_correction_samsung[256]=
{
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
15,15,15,15,15,15,15,10,10,10,10,10,10,9,8,7,
6,5,4,4,2,2,2,2,2,2,1,1,1,1,0,0,
};

static const u8 led_color_correction_nichia[256]=
{
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
15,15,15,15,15,15,15,10,10,10,10,10,10,9,8,7,
6,5,4,4,2,2,2,2,2,2,1,1,1,1,0,0,
};

/* Use the Samsung table as the default */
static const u8 *color_component_correction = led_color_correction_samsung;

/* Select the appropriate gamma table based on the LED backlight attached */
static int __init get_backlighttype(char *str)
{
	if (!strcmp(str, "1476AY")) {
		printk("LED backlight type set to Nichia\n");
		color_component_correction = led_color_correction_nichia;
	}
	/* Samsung will have the value 1577AS, but since Samsung is the */
	/* default, nothing needs to be done here */
	return 1;
}

__setup("backlighttype=", get_backlighttype);

static int boxer_clut_fill(void * ptr, u32 size)
{
	u16 count;
	u32 temp;
	u32 *byte = (u32 *)ptr;
	u16 color_corrected_value;
	u8 red, green, blue;
	for (count = 0; count < size / sizeof(u32); count++) {
	  red   = count;
	  green = count;
	  blue  = count;
	  color_corrected_value = color_component_correction[count]+blue;
	  color_corrected_value = (color_corrected_value >= MAX_COLOR_DEPTH) ? MAX_COLOR_DEPTH:color_corrected_value; 
	  temp = (((red << RED_SHIFT) & RED_MASK) | ((green << GREEN_SHIFT) & GREEN_MASK) | ((color_corrected_value << BLUE_SHIFT) & BLUE_MASK));
	  *byte++ = temp;
	}
	return 0;
}

static struct omap_dss_device boxer_lcd_device = {
	.name = "lcd",
	.driver_name = "boxer_panel",
	.type = OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines = 24,
	.platform_enable	= boxer_panel_enable_lcd,
	.platform_disable	= boxer_panel_disable_lcd,
//	.clut_size		= sizeof(u32) * 256,
//	.clut_fill		= boxer_clut_fill,
 };
/* Roberto and JM comments: we could disable panel TV output for Boxer EVT1 board purposes*/

static int boxer_panel_enable_tv(struct omap_dss_device *dssdev)
{
#define ENABLE_VDAC_DEDICATED           0x03
#define ENABLE_VDAC_DEV_GRP             0x20

	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			ENABLE_VDAC_DEDICATED,
			TWL4030_VDAC_DEDICATED);
	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			ENABLE_VDAC_DEV_GRP, TWL4030_VDAC_DEV_GRP);

	return 0;
}

static void boxer_panel_disable_tv(struct omap_dss_device *dssdev)
{
	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00,
			TWL4030_VDAC_DEDICATED);
	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00,
			TWL4030_VDAC_DEV_GRP);
}
static struct omap_dss_device boxer_tv_device = {
	.name = "tv",
	.driver_name = "venc",
	.type = OMAP_DISPLAY_TYPE_VENC,
	.phy.venc.type = OMAP_DSS_VENC_TYPE_COMPOSITE,
	.platform_enable = boxer_panel_enable_tv,
	.platform_disable = boxer_panel_disable_tv,
};

static struct omap_dss_device *boxer_dss_devices[] = {
	&boxer_lcd_device,
	&boxer_tv_device,
};

static struct omap_dss_board_info boxer_dss_data = {
//	.get_last_off_on_transaction_id = get_last_off_on_transaction_id,
	.num_devices = ARRAY_SIZE(boxer_dss_devices),
	.devices = boxer_dss_devices,
	.default_device = &boxer_lcd_device,
};

static struct platform_device boxer_dss_device = {
	.name          = "omapdss",
	.id            = -1,
	.dev            = {
		.platform_data = &boxer_dss_data,
	},
};

static struct regulator_consumer_supply boxer_vdda_dac_supply = {
	.supply		= "vdda_dac",
	.dev		= &boxer_dss_device.dev,
};

static struct regulator_consumer_supply boxer_vdds_dsi_supply = {
	.supply		= "vdds_dsi",
	.dev		= &boxer_dss_device.dev,
};

#ifdef CONFIG_FB_OMAP2
static struct resource boxer_vout_resource[2] = { };
//static struct resource boxer_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
//};
#else
static struct resource boxer_vout_resource[2] = {
};
#endif
/*
#ifdef CONFIG_PM
struct vout_platform_data boxer_vout_data = {
	.set_min_bus_tput = omap_pm_set_min_bus_tput,
	.set_max_mpu_wakeup_lat =  omap_pm_set_max_mpu_wakeup_lat,
	.set_vdd1_opp = omap_pm_set_min_mpu_freq,
	.set_cpu_freq = omap_pm_cpu_set_freq,
};
#endif
*/
static struct platform_device boxer_vout_device = {
	.name		= "omap_vout",
	.num_resources	= ARRAY_SIZE(boxer_vout_resource),
	.resource	= &boxer_vout_resource[0],
	.id		= -1,
/*#ifdef CONFIG_PM
	.dev		= {
		.platform_data = &boxer_vout_data,
	}
#else*/
	.dev		= {
		.platform_data = NULL,
	}
//#endif
};
/* This is just code left here to leverage on this for Maxim battery charger*/
#ifdef CONFIG_REGULATOR_MAXIM_CHARGER
static struct bq24073_mach_info bq24073_init_dev_data = {
	.gpio_nce = OMAP_BQ24072_CEN_GPIO,
	.gpio_en1 = OMAP_BQ24072_EN1_GPIO,
	.gpio_en2 = OMAP_BQ24072_EN2_GPIO,
	.gpio_nce_state = 1,
	.gpio_en1_state = 0,
	.gpio_en2_state = 0,
};

static struct regulator_consumer_supply bq24073_vcharge_supply = {
       .supply         = "bq24073",
};

static struct regulator_init_data bq24073_init  = {

       .constraints = {
               .min_uV                 = 0,
               .max_uV                 = 5000000,
               .min_uA                 = 0,
               .max_uA                 = 1500000,
               .valid_modes_mask       = REGULATOR_MODE_NORMAL
                                       | REGULATOR_MODE_STANDBY,
               .valid_ops_mask         = REGULATOR_CHANGE_CURRENT
                                       | REGULATOR_CHANGE_MODE
                                       | REGULATOR_CHANGE_STATUS,
       },
       .num_consumer_supplies  = 1,
       .consumer_supplies      = &bq24073_vcharge_supply,

       .driver_data = &bq24073_init_dev_data,
};

/* GPIOS need to be in order of BQ24073 */
static struct platform_device boxer_curr_regulator_device = {
	.name           = "bq24073", /* named after init manager for ST */
	.id             = -1,
	.dev 		= {
		.platform_data = &bq24073_init,
	},
};
#endif

static void encore_backlight_set_power(struct omap_pwm_led_platform_data *self, int on_off)
{
	if (on_off) {
		// Wait for panel to turn on.
		wait_for_completion_interruptible(&panel_on);

	    gpio_direction_output(LCD_BACKLIGHT_EN_EVT2, 0);
        gpio_set_value(LCD_BACKLIGHT_EN_EVT2, 0);
	} else {
		gpio_direction_output(LCD_BACKLIGHT_EN_EVT2, 1);
        gpio_set_value(LCD_BACKLIGHT_EN_EVT2, 1);
	}
}

static struct omap_pwm_led_platform_data boxer_backlight_data = {
	.name = "lcd-backlight",
	.intensity_timer = 8,
	.def_on = 0,
	.def_brightness = DEFAULT_BACKLIGHT_BRIGHTNESS,
	.blink_timer = 0,
	.set_power = encore_backlight_set_power, 
};

static struct platform_device boxer_backlight_led_device = {
	.name		= "omap_pwm_led",
	.id		= -1,
	.dev		= {
		.platform_data = &boxer_backlight_data,
	},
};

#ifdef CONFIG_CHARGER_MAX8903
static struct platform_device max8903_charger_device = {
	.name		= "max8903_charger",
	.id		= -1,
};
#endif

static void boxer_backlight_init(void)
{
	printk("Enabling backlight PWM for LCD\n");

	boxer_backlight_data.def_on = 1; // change the PWM polarity
        gpio_request(LCD_BACKLIGHT_EN_EVT2, "lcd backlight evt2");

	omap_cfg_reg(N8_34XX_GPIO58_PWM);

	gpio_request(LCD_CABC0_GPIO, "lcd CABC0");
	gpio_direction_output(LCD_CABC0_GPIO,0);
	gpio_set_value(LCD_CABC0_GPIO,0);

	gpio_request(LCD_CABC1_GPIO, "lcd CABC1");
	gpio_direction_output(LCD_CABC1_GPIO,0);
	gpio_set_value(LCD_CABC1_GPIO,0);
}

static struct regulator_consumer_supply boxer_vlcdtp_supply[] = {
    { .supply = "vlcd" },
    { .supply = "vtp" }, 
};

static struct regulator_init_data boxer_vlcdtp = {
    .supply_regulator_dev = NULL,
    .constraints = {
        .min_uV = 3300000,
        .max_uV = 3300000,
        .valid_modes_mask = REGULATOR_MODE_NORMAL,
        .valid_ops_mask = REGULATOR_CHANGE_STATUS,
    },
    .num_consumer_supplies = 2,
    .consumer_supplies = boxer_vlcdtp_supply,
};

static struct fixed_voltage_config boxer_lcd_touch_regulator_data = {
    .supply_name = "vdd_lcdtp",
    .microvolts = 3300000,
    .gpio = LCD_EN_GPIO,
    .enable_high = 1,
    .enabled_at_boot = 0,
    .init_data = &boxer_vlcdtp,
};

static struct platform_device boxer_lcd_touch_regulator_device = {
    .name   = "reg-fixed-voltage",
    .id     = -1,
    .dev    = {
        .platform_data = &boxer_lcd_touch_regulator_data,
    },
};

/* Use address that is most likely unused and untouched by u-boot */
#define BOXER_RAM_CONSOLE_START 0x8e000000
#define BOXER_RAM_CONSOLE_SIZE (0x20000)

static struct resource boxer_ram_console_resource[] = { 
    {
        .start  = BOXER_RAM_CONSOLE_START,
        .end    = BOXER_RAM_CONSOLE_START + BOXER_RAM_CONSOLE_SIZE - 1,
        .flags  = IORESOURCE_MEM,
    }
};

static struct platform_device boxer_ram_console_device = {
    .name           = "ram_console",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(boxer_ram_console_resource),
    .resource       = boxer_ram_console_resource,
};

static struct platform_device *boxer_devices[] __initdata = {
#ifdef CONFIG_ANDROID_RAM_CONSOLE
    &boxer_ram_console_device,
#endif
    &boxer_lcd_touch_regulator_device,    
	&boxer_dss_device,
	&boxer_backlight_led_device,
	&boxer_keys_gpio,
#ifdef CONFIG_WL127X_RFKILL
//	&boxer_wl127x_device,
#endif
	&boxer_vout_device,
#ifdef CONFIG_REGULATOR_MAXIM_CHARGER
	&boxer_curr_regulator_device,
#endif
#ifdef CONFIG_CHARGER_MAX8903
	&max8903_charger_device,
#endif
};

static void __init omap_boxer_init_irq(void)
{
#if defined(CONFIG_MACH_ENCORE) && defined (CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB)
	omap2_init_common_hw(	h8mbx00u0mer0em_K4X4G303PB_sdrc_params , NULL,
				omap3621_mpu_rate_table,
				omap3621_dsp_rate_table,
				omap3621_l3_rate_table);

#elif defined(CONFIG_MACH_ENCORE) && defined(CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM)
	omap2_init_common_hw(	h8mbx00u0mer0em_sdrc_params , NULL,
				omap3621_mpu_rate_table,
				omap3621_dsp_rate_table,
				omap3621_l3_rate_table);
#elif defined(CONFIG_MACH_ENCORE) && defined (CONFIG_MACH_SDRAM_SAMSUNG_K4X4G303PB)
	omap2_init_common_hw(	samsung_k4x4g303pb_sdrc_params, NULL,
				omap3621_mpu_rate_table,
				omap3621_dsp_rate_table,
				omap3621_l3_rate_table);
#endif
	omap_init_irq();
	omap_gpio_init();
}

static struct regulator_consumer_supply boxer_vmmc1_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply boxer_vsim_supply = {
	.supply		= "vmmc_aux",
};

static struct regulator_consumer_supply boxer_vmmc2_supply = {
	.supply		= "vmmc",
};

/* VMMC1 for OMAP VDD_MMC1 (i/o) and MMC1 card */
static struct regulator_init_data boxer_vmmc1 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &boxer_vmmc1_supply,
};

/* VMMC2 for MMC2 card */
static struct regulator_init_data boxer_vmmc2 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 1850000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &boxer_vmmc2_supply,
};

/* VSIM for OMAP VDD_MMC1A (i/o for DAT4..DAT7) */
static struct regulator_init_data boxer_vsim = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &boxer_vsim_supply,
};

static struct regulator_init_data boxer_vdac = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &boxer_vdda_dac_supply,
};

static struct regulator_init_data boxer_vdsi = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &boxer_vdds_dsi_supply,
};

static struct twl4030_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.wires		= 4,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc		= 2,
		.wires		= 8,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc		= 3,
		.wires		= 4,
		.gpio_wp	= -EINVAL,
	},
	{}      /* Terminator */
};

static int __ref boxer_twl_gpio_setup(struct device *dev,
		unsigned gpio, unsigned ngpio)
{
	/* gpio + 0 is "mmc0_cd" (input/IRQ),
	 * gpio + 1 is "mmc1_cd" (input/IRQ)
	 */
	mmc[0].gpio_cd = gpio + 0;
	mmc[1].gpio_cd = gpio + 1;
	twl4030_mmc_init(mmc);

	/* link regulators to MMC adapters ... we "know" the
	 * regulators will be set up only *after* we return.
	*/
	boxer_vmmc1_supply.dev = mmc[0].dev;
	boxer_vsim_supply.dev = mmc[0].dev;
	boxer_vmmc2_supply.dev = mmc[1].dev;

	return 0;
}

static struct omap_lcd_config boxer_lcd_config __initdata = {
        .ctrl_name      = "internal",
};

static struct omap_uart_config boxer_uart_config __initdata = {
	.enabled_uarts	= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)),
};

static struct omap_board_config_kernel boxer_config[] __initdata = {
	{ OMAP_TAG_UART,	&boxer_uart_config },
        { OMAP_TAG_LCD,         &boxer_lcd_config },
};

static struct twl4030_usb_data boxer_usb_data = {
      .usb_mode	= T2_USB_MODE_ULPI,
#ifdef CONFIG_REGULATOR_MAXIM_CHARGER
      .bci_supply     = &bq24073_vcharge_supply,
#endif
};
static struct twl4030_gpio_platform_data boxer_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.setup		= boxer_twl_gpio_setup,
};

static struct twl4030_madc_platform_data boxer_madc_data = {
	.irq_line	= 1,
};

static struct twl4030_ins sleep_on_seq[] = {

	/* Turn off HFCLKOUT */
	{MSG_SINGULAR(DEV_GRP_P1, 0x19, RES_STATE_OFF), 2},
	/* Turn OFF VDD1 */
	{MSG_SINGULAR(DEV_GRP_P1, 0xf, RES_STATE_OFF), 2},
	/* Turn OFF VDD2 */
	{MSG_SINGULAR(DEV_GRP_P1, 0x10, RES_STATE_OFF), 2},
	/* Turn OFF VPLL1 */
	{MSG_SINGULAR(DEV_GRP_P1, 0x7, RES_STATE_OFF), 2},
	/* Turn OFF REGEN */
    /* test stability without REGEN {MSG_SINGULAR(DEV_GRP_P1, 0x15, RES_STATE_OFF), 2}, */
};

static struct twl4030_script sleep_on_script = {
	.script	= sleep_on_seq,
	.size	= ARRAY_SIZE(sleep_on_seq),
	.flags	= TWL4030_SLEEP_SCRIPT,
};

static struct twl4030_ins wakeup_p12_seq[] = {
	/* Turn on HFCLKOUT */
	{MSG_SINGULAR(DEV_GRP_P1, 0x19, RES_STATE_ACTIVE), 2},
	/* Turn ON VDD1 */
	{MSG_SINGULAR(DEV_GRP_P1, 0xf, RES_STATE_ACTIVE), 2},
	/* Turn ON VDD2 */
	{MSG_SINGULAR(DEV_GRP_P1, 0x10, RES_STATE_ACTIVE), 2},
	/* Turn ON VPLL1 */
	{MSG_SINGULAR(DEV_GRP_P1, 0x7, RES_STATE_ACTIVE), 2},
    /* Turn ON REGEN */
    /* test stability, regen never turned off    {MSG_SINGULAR(DEV_GRP_P1, 0x15, RES_STATE_ACTIVE), 2}, */
};

static struct twl4030_script wakeup_p12_script = {
	.script = wakeup_p12_seq,
	.size   = ARRAY_SIZE(wakeup_p12_seq),
	.flags  = TWL4030_WAKEUP12_SCRIPT,
};

static struct twl4030_ins wakeup_p3_seq[] = {
	{MSG_SINGULAR(DEV_GRP_P1, 0x19, RES_STATE_ACTIVE), 2},
};

static struct twl4030_script wakeup_p3_script = {
	.script = wakeup_p3_seq,
	.size   = ARRAY_SIZE(wakeup_p3_seq),
	.flags  = TWL4030_WAKEUP3_SCRIPT,
};

static struct twl4030_ins wrst_seq[] = {
/*
 * Reset twl4030.
 * Reset VMMC1 regulator.
 * Reset VDD1 regulator.
 * Reset VDD2 regulator.
 * Reset VPLL1 regulator.
 * Enable sysclk output.
 * Reenable twl4030.
 */
	{MSG_SINGULAR(DEV_GRP_NULL, 0x1b, RES_STATE_OFF), 2},
	{MSG_SINGULAR(DEV_GRP_P1, 0x5, RES_STATE_WRST), 15},
    {MSG_SINGULAR(DEV_GRP_P1, 0xf, RES_STATE_WRST), 15},
	{MSG_SINGULAR(DEV_GRP_P1, 0x10, RES_STATE_WRST), 15},
	{MSG_SINGULAR(DEV_GRP_P1, 0x7, RES_STATE_WRST), 0x60},
	{MSG_SINGULAR(DEV_GRP_P1, 0x19, RES_STATE_ACTIVE), 2},
	{MSG_SINGULAR(DEV_GRP_NULL, 0x1b, RES_STATE_ACTIVE), 2},
};

static struct twl4030_script wrst_script = {
	.script = wrst_seq,
	.size   = ARRAY_SIZE(wrst_seq),
	.flags  = TWL4030_WRST_SCRIPT,
};

static struct twl4030_script *twl4030_scripts[] = {
	&sleep_on_script,
	&wakeup_p12_script,
	&wakeup_p3_script,
	&wrst_script,
};

static struct twl4030_resconfig twl4030_rconfig[] = {
	{ .resource = RES_HFCLKOUT, .devgroup = DEV_GRP_P3, .type = -1,
		.type2 = -1 },
	{ .resource = RES_VDD1, .devgroup = DEV_GRP_P1, .type = -1,
		.type2 = -1 },
	{ .resource = RES_VDD2, .devgroup = DEV_GRP_P1, .type = -1,
		.type2 = -1 },
	{ 0, 0},
};

static struct twl4030_power_data boxer_t2scripts_data = {
	.scripts	= twl4030_scripts,
	.num		= ARRAY_SIZE(twl4030_scripts),
	.resource_config = twl4030_rconfig,
};

static struct twl4030_platform_data __refdata boxer_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.madc		= &boxer_madc_data,
	.usb		= &boxer_usb_data,
	.gpio		= &boxer_gpio_data,
	.keypad		= &boxer_kp_twl4030_data,
	.power		= &boxer_t2scripts_data,
	.vmmc1          = &boxer_vmmc1,
	.vmmc2          = &boxer_vmmc2,
	.vsim           = &boxer_vsim,
	.vdac		= &boxer_vdac,
	.vpll2		= &boxer_vdsi,
};


#ifdef CONFIG_TOUCHSCREEN_CYTTSP_I2C

int  cyttsp_dev_init(int resource) 
{
	if (resource)
	{
		if (gpio_request(OMAP_CYTTSP_RESET_GPIO, "tma340_reset") < 0) {
			printk(KERN_ERR "can't get tma340 xreset GPIO\n");
			return -1;
		}

		if (gpio_request(OMAP_CYTTSP_GPIO, "cyttsp_touch") < 0) {
			printk(KERN_ERR "can't get cyttsp interrupt GPIO\n");
			return -1;
		}

		gpio_direction_input(OMAP_CYTTSP_GPIO);
		omap_set_gpio_debounce(OMAP_CYTTSP_GPIO, 0);
	}
	else
	{
		gpio_free(OMAP_CYTTSP_GPIO);
		gpio_free(OMAP_CYTTSP_RESET_GPIO);
	}
    return 0;
}

static struct cyttsp_platform_data cyttsp_platform_data = {
	.maxx = 600,
	.maxy = 1024,
	.flags = 0,
	.gen = CY_GEN3,
	.use_st = CY_USE_ST,
	.use_mt = CY_USE_MT,
	.use_hndshk = CY_SEND_HNDSHK,
	.use_trk_id = CY_USE_TRACKING_ID,
	.use_sleep = CY_USE_SLEEP,
	.use_gestures = CY_USE_GESTURES,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP1 | CY_GEST_GRP2 |
		CY_GEST_GRP3 | CY_GEST_GRP4 |
		CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state 
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state 
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
};

#endif

#ifdef CONFIG_TOUCHSCREEN_PIXCIR_I2C
static void pixcir_dev_init(void)
{
	
        printk("board-3621_boxer.c: pixcir_dev_init ...\n");
        if (gpio_request(PIXCIR_I2C_S32_GPIO, "pixcir_touch") < 0) {
                printk(KERN_ERR "can't get pixcir pen down GPIO\n");
                return;
        }

        printk("board-3621_boxer.c: pixcir_dev_init > Initialize pixcir irq pin %d !\n", PIXCIR_I2C_S32_GPIO);
        gpio_direction_input(PIXCIR_I2C_S32_GPIO);
        omap_set_gpio_debounce(PIXCIR_I2C_S32_GPIO, 0);  // set 0 to disable debounce
        //omap_set_gpio_debounce(PIXCIR_I2C_S32_GPIO, 1);
        //omap_set_gpio_debounce_time(PIXCIR_I2C_S32_GPIO, 0x1);	
}

static int pixcir_power(int power_state)
{
	return 0;
}

static struct pixcir_i2c_s32_platform_data pixcir_platform_data[] = {
	{
		.version	= 0x0,
		.power		= &pixcir_power,
		.flags		= 0,
		.irqflags	= IRQF_TRIGGER_FALLING , /* IRQF_TRIGGER_LOW,*/
	}
};
#endif

// This code is yanked from arch/arm/mach-omap2/prcm.c
void machine_emergency_restart(void)
{
	s16 prcm_offs;
	u32 l;

	// delay here to allow eMMC to finish any internal housekeeping before reset
	// even if mdelay fails to work correctly, 8 second button press should work 
	// this used to be an msleep but scheduler is gone here and calling msleep
	// will cause a panic
	mdelay(1600);

	prcm_offs = OMAP3430_GR_MOD;
	l = ('B' << 24) | ('M' << 16) | 'h';
	/* Reserve the first word in scratchpad for communicating
	* with the boot ROM. A pointer to a data structure
	* describing the boot process can be stored there,
	* cf. OMAP34xx TRM, Initialization / Software Booting
	* Configuration. */
	omap_writel(l, OMAP343X_SCRATCHPAD + 4);
//	omap3_configure_core_dpll_warmreset();
}

#ifdef CONFIG_INPUT_KXTF9
/* KIONIX KXTF9 Digital Tri-axis Accelerometer */

static void kxtf9_dev_init(void)
{
	printk("board-encore.c: kxtf9_dev_init ...\n");

//	if (gpio_request(KXTF9_GPIO_FOR_PWR, "kxtf9_pwr") < 0) {
//		printk(KERN_ERR "+++++++++++++ Can't get GPIO for kxtf9 power\n");
//		return;
//	}
        // Roberto's comment: G-sensor is powered by VIO and does not need to be powered enabled
	//gpio_direction_output(KXTF9_GPIO_FOR_PWR, 1);
	
	if (gpio_request(KXTF9_GPIO_FOR_IRQ, "kxtf9_irq") < 0) {
		printk(KERN_ERR "Can't get GPIO for kxtf9 IRQ\n");
		return;
	}

	printk("board-encore.c: kxtf9_dev_init > Init kxtf9 irq pin %d !\n", KXTF9_GPIO_FOR_IRQ);
	gpio_direction_input(KXTF9_GPIO_FOR_IRQ);
	omap_set_gpio_debounce(KXTF9_GPIO_FOR_IRQ, 0);
}

struct kxtf9_platform_data kxtf9_platform_data_here = {
        .min_interval   = 1,
        .poll_interval  = 1000,

        .g_range        = KXTF9_G_8G,
        .shift_adj      = SHIFT_ADJ_2G,

		// Map the axes from the sensor to the device.
		
		//. SETTINGS FOR THE EVT1A TEST RIG:
        .axis_map_x     = 1,
        .axis_map_y     = 0,
        .axis_map_z     = 2,
        .negate_x       = 1,
        .negate_y       = 0,
        .negate_z       = 0,
		
		//. SETTINGS FOR THE ENCORE PRODUCT:
        //. .axis_map_x     = 1,
        //. .axis_map_y     = 0,
        //. .axis_map_z     = 2,
        //. .negate_x       = 1,
        //. .negate_y       = 0,
        //. .negate_z       = 0,

        .data_odr_init          = ODR12_5,
        .ctrl_reg1_init         = KXTF9_G_8G | RES_12BIT | TDTE | WUFE | TPE,
        .int_ctrl_init          = KXTF9_IEN | KXTF9_IEA | KXTF9_IEL,
        .int_ctrl_init          = KXTF9_IEN,
        .tilt_timer_init        = 0x03,
        .engine_odr_init        = OTP12_5 | OWUF50 | OTDT400,
        .wuf_timer_init         = 0x16,
        .wuf_thresh_init        = 0x28,
        .tdt_timer_init         = 0x78,
        .tdt_h_thresh_init      = 0xFF,
        .tdt_l_thresh_init      = 0x14,
        .tdt_tap_timer_init     = 0x53,
        .tdt_total_timer_init   = 0x24,
        .tdt_latency_timer_init = 0x10,
        .tdt_window_timer_init  = 0xA0,

        .gpio = KXTF9_GPIO_FOR_IRQ,
};
#endif	/* CONFIG_INPUT_KXTF9 */

#ifdef CONFIG_BATTERY_MAX17042
static void max17042_dev_init(void)
{
        printk("board-encore.c: max17042_dev_init ...\n");

        if (gpio_request(MAX17042_GPIO_FOR_IRQ, "max17042_irq") < 0) {
                printk(KERN_ERR "Can't get GPIO for max17042 IRQ\n");
                return;
        }

        printk("board-encore.c: max17042_dev_init > Init max17042 irq pin %d !\n", MAX17042_GPIO_FOR_IRQ);
        gpio_direction_input(MAX17042_GPIO_FOR_IRQ);
        omap_set_gpio_debounce(MAX17042_GPIO_FOR_IRQ, 0);        
        printk("max17042 GPIO pin read %d\n", gpio_get_value(MAX17042_GPIO_FOR_IRQ));
}
#endif

#ifdef CONFIG_BATTERY_MAX17042
struct max17042_platform_data max17042_platform_data_here = {

	//fill in device specific data here
	//load stored parameters from Rom Tokens? 
	//.val_FullCAP =
	//.val_Cycles =
	//.val_FullCAPNom =
	//.val_SOCempty =
	//.val_Iavg_empty =
	//.val_RCOMP0 =
	//.val_TempCo=
	//.val_k_empty0 =
	//.val_dQacc =
	//.val_dPacc =
	
        .gpio = MAX17042_GPIO_FOR_IRQ,
};
#endif

#ifdef CONFIG_MAX9635
static int max9635_device_resource(int allocate)
{
	if (allocate) {
		if (gpio_request(MAX9635_GPIO_FOR_IRQ, "max9635_irq") < 0) {
			printk(KERN_ERR "Failed to get GPIO for max9635\n");
			return -1;
		}
	
		gpio_direction_input(MAX9635_GPIO_FOR_IRQ);
		omap_set_gpio_debounce(MAX9635_GPIO_FOR_IRQ, 0);
	}
	else
	{
		gpio_free(MAX9635_GPIO_FOR_IRQ);
	}
	return 0;
}

static struct max9635_pdata __initdata max9635_platform_data = {
    .gpio           = MAX9635_GPIO_FOR_IRQ,
    .poll_interval  = MAX9635_DEFAULT_POLL_INTERVAL,
	.device_resource = max9635_device_resource,
};
#endif /* CONFIG_MAX9635 */

static struct i2c_board_info __initdata boxer_i2c_bus1_info[] = {
#ifdef CONFIG_BATTERY_MAX17042
	{
		I2C_BOARD_INFO(MAX17042_DEVICE_ID, MAX17042_I2C_SLAVE_ADDRESS),
		.platform_data = &max17042_platform_data_here,
		.irq = OMAP_GPIO_IRQ(MAX17042_GPIO_FOR_IRQ),
	},
#endif	/*CONFIG_BATTERY_MAX17042*/	
	{
		I2C_BOARD_INFO("tps65921", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &boxer_twldata,
	},
#ifdef CONFIG_INPUT_KXTF9
	{
		I2C_BOARD_INFO(KXTF9_DEVICE_ID, KXTF9_I2C_SLAVE_ADDRESS),
		.platform_data = &kxtf9_platform_data_here,
		.irq = OMAP_GPIO_IRQ(KXTF9_GPIO_FOR_IRQ),
	},
#endif /* CONFIG_INPUT_KXTF9 */
#ifdef CONFIG_MAX9635
    {
        I2C_BOARD_INFO(MAX9635_NAME, MAX9635_I2C_SLAVE_ADDRESS),
        .platform_data = &max9635_platform_data,
        .irq = OMAP_GPIO_IRQ(MAX9635_GPIO_FOR_IRQ),
    },
#endif /* CONFIG_MAX9635 */
};

#if defined(CONFIG_SND_SOC_DAC3100) || defined(CONFIG_SND_SOC_DAC3100_MODULE) || defined (CONFIG_SND_OMAP_SOC_OMAP3_EDP)
#define AUDIO_CODEC_POWER_ENABLE_GPIO    103
#define AUDIO_CODEC_RESET_GPIO           37
#define AUDIO_CODEC_IRQ_GPIO             59
#define AIC3100_NAME "tlv320dac3100"
#define AIC3100_I2CSLAVEADDRESS 0x18

static void audio_dac_3100_dev_init(void)
{
        printk("board-encore.c: audio_dac_3100_dev_init ...\n");
        if (gpio_request(AUDIO_CODEC_RESET_GPIO, "AUDIO_CODEC_RESET_GPIO") < 0) {
                printk(KERN_ERR "can't get AUDIO_CODEC_RESET_GPIO \n");
                return;
        }

        printk("board-encore.c: audio_dac_3100_dev_init > set AUDIO_CODEC_RESET_GPIO to output Low!\n");
        gpio_direction_output(AUDIO_CODEC_RESET_GPIO, 0);
	gpio_set_value(AUDIO_CODEC_RESET_GPIO, 0);

        printk("board-encore.c: audio_dac_3100_dev_init ...\n");
        if (gpio_request(AUDIO_CODEC_POWER_ENABLE_GPIO, "AUDIO DAC3100 POWER ENABLE") < 0) {
                printk(KERN_ERR "can't get AUDIO_CODEC_POWER_ENABLE_GPIO \n");
                return;
        }
//// 2.7 merge conflict 
//        printk("board-encore.c: audio_dac_3100_dev_init > set AUDIO_CODEC_RESET_GPIO to output High!\n");
//       gpio_direction_output(AUDIO_CODEC_RESET_GPIO, 0);
//// 2.7 merge conflict

        printk("board-encore.c: audio_dac_3100_dev_init > set AUDIO_CODEC_POWER_ENABLE_GPIO to output and value high!\n");
        gpio_direction_output(AUDIO_CODEC_POWER_ENABLE_GPIO, 0);
	gpio_set_value(AUDIO_CODEC_POWER_ENABLE_GPIO, 1);

	/* 1 msec delay needed after PLL power-up */
        mdelay (1);

        printk("board-encore.c: audio_dac_3100_dev_init > set AUDIO_CODEC_RESET_GPIO to output and value high!\n");
	gpio_set_value(AUDIO_CODEC_RESET_GPIO, 1);

}
#endif
static struct i2c_board_info __initdata boxer_i2c_bus2_info[] = {
#ifdef CONFIG_TOUCHSCREEN_PIXCIR_I2C 
	{
		I2C_BOARD_INFO(PIXCIR_I2C_S32_NAME, PIXCIR_I2C_S32_SLAVEADDRESS),
		.platform_data = &pixcir_platform_data,
		.irq = OMAP_GPIO_IRQ(PIXCIR_I2C_S32_GPIO),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_CYTTSP_I2C
	{
        I2C_BOARD_INFO(CY_I2C_NAME, CYTTSP_I2C_SLAVEADDRESS),
        .platform_data = &cyttsp_platform_data,
        .irq = OMAP_GPIO_IRQ(OMAP_CYTTSP_GPIO),
	},
#endif

#if defined(CONFIG_SND_SOC_DAC3100) || defined(CONFIG_SND_SOC_DAC3100_MODULE)  || defined (CONFIG_SND_OMAP_SOC_OMAP3_EDP)
	{
		I2C_BOARD_INFO(AIC3100_NAME,  AIC3100_I2CSLAVEADDRESS),
                .irq = OMAP_GPIO_IRQ(AUDIO_CODEC_IRQ_GPIO),
	},
#endif
};


#if defined(CONFIG_USB_ANDROID) || defined(CONFIG_USB_ANDROID_MODULE)
static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.vendor = "B&N     ",
	.product = "Ebook Disk      ",
	.release = 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name = "usb_mass_storage",
	.id = -1,
	.dev = {
		.platform_data = &mass_storage_pdata,
		},
};

// Reserved for serial number passed in from the bootloader.
static char adb_serial_number[32] = "";

static char *usb_functions_ums[] = {
	"usb_mass_storage",
};

static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= ENCORE_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
	},
	{
		.product_id	= ENCORE_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
	{
		.product_id	= ENCORE_RNDIS_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= ENCORE_RNDIS_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= ENCORE_VENDOR_ID,
	.product_id	= ENCORE_PRODUCT_ID,
	.manufacturer_name = "B&N",
	.product_name	= "NookColor",
	.serial_number	= "11223344556677",
	.num_products   = ARRAY_SIZE(usb_products),
	.products	= usb_products,
	.num_functions	= ARRAY_SIZE(usb_functions_all),
	.functions	= usb_functions_all,
};

static struct platform_device android_usb_device = {
	.name		= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};
#endif


static int __init omap_i2c_init(void)
{

    int i2c1_devices;

/* Disable OMAP 3630 internal pull-ups for I2Ci */
	if (cpu_is_omap3630()) {

		u32 prog_io;

		prog_io = omap_ctrl_readl(OMAP343X_CONTROL_PROG_IO1);
		/* Program (bit 19)=1 to disable internal pull-up on I2C1 */
		prog_io |= OMAP3630_PRG_I2C1_PULLUPRESX;
		/* Program (bit 0)=1 to disable internal pull-up on I2C2 */
		prog_io |= OMAP3630_PRG_I2C2_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP343X_CONTROL_PROG_IO1);

		prog_io = omap_ctrl_readl(OMAP36XX_CONTROL_PROG_IO_WKUP1);
		/* Program (bit 5)=1 to disable internal pull-up on I2C4(SR) */
		prog_io |= OMAP3630_PRG_SR_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP36XX_CONTROL_PROG_IO_WKUP1);
	}

	i2c1_devices = ARRAY_SIZE(boxer_i2c_bus1_info);

#ifdef CONFIG_MAX9635
	// right now evt2 is not stuffed with the max9635 light sensor due to i2c conflict 
	// tbd if it will be reworked on specific units
	--i2c1_devices;
#endif

	omap_register_i2c_bus(1, 100, boxer_i2c_bus1_info,
			i2c1_devices);
	omap_register_i2c_bus(2, 400, boxer_i2c_bus2_info,
			ARRAY_SIZE(boxer_i2c_bus2_info));
	return 0;
}

#if 0
static int __init wl127x_vio_leakage_fix(void)
{
	int ret = 0;

	ret = gpio_request(WL127X_BTEN_GPIO, "wl127x_bten");
	if (ret < 0) {
		printk(KERN_ERR "wl127x_bten gpio_%d request fail",
						WL127X_BTEN_GPIO);
		goto fail;
	}

	gpio_direction_output(WL127X_BTEN_GPIO, 1);
	mdelay(10);
	gpio_direction_output(WL127X_BTEN_GPIO, 0);
	udelay(64);

	gpio_free(WL127X_BTEN_GPIO);
fail:
	return ret;
}
#endif

static void __init omap_boxer_init(void)
{
	/*we need to have this enable function here to light up the BL*/
	boxer_panel_enable_lcd(&boxer_lcd_device);
	omap_i2c_init();
	/* Fix to prevent VIO leakage on wl127x */
//	wl127x_vio_leakage_fix();

	platform_add_devices(boxer_devices, ARRAY_SIZE(boxer_devices));

	omap_board_config = boxer_config;
	omap_board_config_size = ARRAY_SIZE(boxer_config);

	spi_register_board_info(boxer_spi_board_info,
				ARRAY_SIZE(boxer_spi_board_info));
#ifdef  CONFIG_TOUCHSCREEN_PIXCIR_I2C
	pixcir_dev_init();
#endif
#ifdef CONFIG_TOUCHSCREEN_CYTTSP_I2C
//    cyttsp_dev_init();
#endif

#ifdef CONFIG_INPUT_KXTF9
	kxtf9_dev_init();
#endif /* CONFIG_INPUT_KXTF9 */

#ifdef CONFIG_BATTERY_MAX17042
	max17042_dev_init();
#endif

#if defined(CONFIG_SND_SOC_DAC3100) || defined(CONFIG_SND_SOC_DAC3100_MODULE) || defined(CONFIG_SND_OMAP_SOC_OMAP3_EDP)
        audio_dac_3100_dev_init();
#endif
//	synaptics_dev_init();
//	msecure_init();
//	ldp_flash_init();
	omap_serial_init(0, 0);
	usb_musb_init();
	boxer_backlight_init();
#if defined(CONFIG_USB_ANDROID) || defined(CONFIG_USB_ANDROID_MODULE)
	platform_device_register(&usb_mass_storage_device);
	// Set the device serial number passed in from the bootloader.
	if (system_serial_high != 0 || system_serial_low != 0) {
		snprintf(adb_serial_number, sizeof(adb_serial_number), "%08x%08x", system_serial_high, system_serial_low);
		adb_serial_number[16] = '\0';
		android_usb_pdata.serial_number = adb_serial_number;
	}
	platform_device_register(&android_usb_device);
#endif
        BUG_ON(!cpu_is_omap3630());
}

static struct map_desc boxer_io_desc[] __initdata = {
	{
		.virtual	= ZOOM2_QUART_VIRT,
		.pfn		= __phys_to_pfn(ZOOM2_QUART_PHYS),
		.length		= ZOOM2_QUART_SIZE,
		.type		= MT_DEVICE
	},
};

static void __init omap_boxer_map_io(void)
{
#ifdef CONFIG_ANDROID_RAM_CONSOLE
    reserve_bootmem(BOXER_RAM_CONSOLE_START, BOXER_RAM_CONSOLE_SIZE, 0);
#endif /* CONFIG_ANDROID_RAM_CONSOLE */
	omap2_set_globals_343x();
	iotable_init(boxer_io_desc, ARRAY_SIZE(boxer_io_desc));
	omap2_map_common_io();
}
 
MACHINE_START(ENCORE, "encore")
	/* phys_io is only used for DEBUG_LL early printing.  The Boxer's
	 * console is on an external quad UART sitting at address 0x10000000
	 */
	.phys_io	= BOXER_EXT_QUART_PHYS,
	.io_pg_offst	= ((BOXER_EXT_QUART_VIRT) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_boxer_map_io,
	.init_irq	= omap_boxer_init_irq,
	.init_machine	= omap_boxer_init,
	.timer		= &omap_timer,
MACHINE_END
