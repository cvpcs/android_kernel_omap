/*
 * Boxer panel support
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Copyright (c) 2010 Barnes & Noble
 * David Bolcsfoldi <dbolcsfoldi@intrinsyc.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c/twl4030.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/workqueue.h>

#include <plat/mcspi.h>
#include <plat/gpio.h>
#include <plat/mux.h>
#include <asm/mach-types.h>
#include <plat/control.h>

#include <plat/display.h>

/* Delay between Panel configuration and Panel enabling */
#define LCD_RST_DELAY		100
#define LCD_INIT_DELAY		200

#define LCD_XRES		1024
#define LCD_YRES		600

#define LCD_PIXCLOCK_MIN	39000 /* CPT MIN PIX Clock is 39MHz */
#define Lcd_Pixclock_Typ	45000 /* Typical PIX clock is 45MHz */
#define LCD_PIXCLOCK_MAX	52000 /* Maximum is 52MHz */

/* Current Pixel clock */
#define LCD_PIXEL_CLOCK		48000

static struct workqueue_struct *boxer_panel_wq;
static struct omap_dss_device *boxer_panel_dssdev;
static struct regulator *boxer_panel_regulator;
static struct spi_device *boxer_spi_device;
static atomic_t boxer_panel_is_enabled = ATOMIC_INIT(0);

/*NEC NL8048HL11-01B  Manual
 * defines HFB, HSW, HBP, VFP, VSW, VBP as shown below
 */

static struct omap_video_timings boxer_panel_timings = {
	/* 1024 x 600 @ 60 Hz  Reduced blanking VESA CVT 0.31M3-R */
	.x_res          = LCD_XRES,
	.y_res          = LCD_YRES,
	.pixel_clock    = LCD_PIXEL_CLOCK,
	.hfp            = 70,//48,
	.hsw            = 40,
	.hbp            = 200,//65,
	.vfp            = 10,//3,
	.vsw            = 10,
	.vbp            = 11,//25,
};


static int boxer_panel_probe(struct omap_dss_device *dssdev)
{
	dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
			       OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_IPC;
	dssdev->panel.timings = boxer_panel_timings;
	dssdev->panel.recommended_bpp = 24;
	return 0;
}

static void boxer_panel_remove(struct omap_dss_device *dssdev)
{
}

static int spi_send(struct spi_device *spi, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = 0;
	uint16_t msg;
	msg=(reg_addr<<10)|reg_data;

	if (spi_write(spi, (unsigned char *)&msg, 2))
		printk(KERN_ERR "error in spi_write %x\n", msg);

	udelay(10);

	return ret;
}

static int spi_rw(struct spi_device *spi, u8 *buf, u8 *bufo ,size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.rx_buf		= bufo,
			.len		= len,
		};
	struct spi_message	m;
    int r;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	r=spi_sync(spi, &m);
	return r;
}

static void boxer_init_panel(void)
{
	spi_send(boxer_spi_device, 0, 0x00);

/*   For evt1b, the original sequence was */
//     spi_send(boxer_spi_device, 0, 0xad);
//     spi_send(boxer_spi_device, 1, 0x30);
//     spi_send(boxer_spi_device, 2, 0x40);
//     spi_send(boxer_spi_device, 3, 0x04);

	spi_send(boxer_spi_device,   0, 0xad);
	spi_send(boxer_spi_device,   1, 0x30);
	spi_send(boxer_spi_device,   2, 0x40);
	spi_send(boxer_spi_device, 0xe, 0x5f);
	spi_send(boxer_spi_device, 0xf, 0xa4);
	spi_send(boxer_spi_device, 0xd, 0x00);
	spi_send(boxer_spi_device, 0x2, 0x43);
	spi_send(boxer_spi_device, 0xa, 0x28);
	spi_send(boxer_spi_device, 0x10, 0x41);
}

static void boxer_panel_work_func(struct work_struct *work)
{
	regulator_enable(boxer_panel_regulator);

	msleep(LCD_RST_DELAY);

    boxer_spi_device->mode = SPI_MODE_0;
    boxer_spi_device->bits_per_word = 16;
	spi_setup(boxer_spi_device);

	boxer_init_panel();

    msleep(LCD_INIT_DELAY); 
    
   	if (boxer_panel_dssdev->platform_enable)
    	boxer_panel_dssdev->platform_enable(boxer_panel_dssdev);
}

static DECLARE_WORK(boxer_panel_work, boxer_panel_work_func);

static int boxer_panel_enable(struct omap_dss_device *dssdev)
{
    if (atomic_add_unless(&boxer_panel_is_enabled, 1, 1)) {
		boxer_panel_dssdev = dssdev;
		queue_work(boxer_panel_wq, &boxer_panel_work);
	}
 
	return 0;
}

static void boxer_panel_disable(struct omap_dss_device *dssdev)
{
    if (atomic_dec_and_test(&boxer_panel_is_enabled)) {
		cancel_work_sync(&boxer_panel_work);

		if (dssdev->platform_disable)
			dssdev->platform_disable(dssdev);

		regulator_disable(boxer_panel_regulator);
    } else {
        printk("%s: attempting to disable panel twice!\n", __FUNCTION__);
        WARN_ON(1);
    }
}

static int boxer_panel_suspend(struct omap_dss_device *dssdev)
{
    boxer_panel_disable(dssdev);
	return 0;
}

static int boxer_panel_resume(struct omap_dss_device *dssdev)
{
	return boxer_panel_enable(dssdev);
}

static struct omap_dss_driver boxer_driver = {
	.probe          = boxer_panel_probe,
	.remove         = boxer_panel_remove,

	.enable         = boxer_panel_enable,
	.disable        = boxer_panel_disable,
	.suspend        = boxer_panel_suspend,
	.resume         = boxer_panel_resume,

	.driver		= {
		.name	= "boxer_panel",
		.owner	= THIS_MODULE,
	},
};



static ssize_t lcd_reg_store(struct device *dev, struct device_attribute *attr, 
                                        const char *buf, size_t count)
{
    int argc; 
    char **args;
	int r,val;

    struct spi_device *spi = to_spi_device(dev);
 
    args = argv_split(GFP_KERNEL, buf, &argc);

    if (args == NULL) {
        dev_err(dev, "error getting arguments\n");
		return count;
    }

    if (argc==2) {
		r=simple_strtoul(*args, NULL, 0);
		args++;
		val=simple_strtoul(*args, NULL, 0);
		printk("set lcd panel spi reg %x = %x\n",r,val);
		spi_send(spi, r, val);
    }
	argv_free(args);

    return count;
}


static DEVICE_ATTR(lcd_reg, S_IWUSR, NULL, lcd_reg_store);

static struct attribute *boxer_lcd_spi_attributes[] = {
	&dev_attr_lcd_reg,
    NULL
};


static struct attribute_group boxer_lcd_spi_attributes_group = {
    .attrs = boxer_lcd_spi_attributes,
};



static int boxer_spi_probe(struct spi_device *spi)
{
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 16;
	spi_setup(spi);
	
	boxer_spi_device = spi;
	printk("spi_probe mode : %x, per_word %d, chip_select %d, speed %d, master_bus %d,master_cs %d \n",spi->mode,spi->bits_per_word,spi->chip_select,spi->max_speed_hz,spi->master->bus_num, spi->master->num_chipselect);

	boxer_init_panel();

	if (sysfs_create_group(&spi->dev.kobj, &boxer_lcd_spi_attributes_group)) {
		printk( "error creating sysfs entries\n");
	}

	omap_dss_register_driver(&boxer_driver);
	return 0;
}

static int boxer_spi_remove(struct spi_device *spi)
{
	sysfs_remove_group(&spi->dev.kobj, &boxer_lcd_spi_attributes_group);
	omap_dss_unregister_driver(&boxer_driver);

	return 0;
}


static struct spi_driver boxer_spi_driver = {
	.probe           = boxer_spi_probe,
	.remove	= __devexit_p(boxer_spi_remove),
	.driver         = {
		.name   = "boxer_disp_spi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
};

static int __init boxer_lcd_init(void)
{
    int ret = 0;

	boxer_panel_wq = create_singlethread_workqueue("boxer-panel-wq");

    printk("Enabling power for LCD\n");
    boxer_panel_regulator = regulator_get(NULL, "vlcd");

    if (IS_ERR(boxer_panel_regulator)) {
        printk(KERN_ERR "Unable to get vlcd regulator, reason: %ld!\n", IS_ERR(boxer_panel_regulator));
        ret = -ENODEV;
        goto out;
    }

    ret = regulator_enable(boxer_panel_regulator);

    if (ret) {
        printk(KERN_ERR "Failed to enable regulator vlcd!\n");
        regulator_put(boxer_panel_regulator);
        goto out;
    }

    atomic_inc(&boxer_panel_is_enabled);
	return spi_register_driver(&boxer_spi_driver);
out:
    return ret;
}

static void __exit boxer_lcd_exit(void)
{
    spi_unregister_driver(&boxer_spi_driver);
    regulator_disable(boxer_panel_regulator);
    regulator_put(boxer_panel_regulator);
	destroy_workqueue(boxer_panel_wq);
}


module_init(boxer_lcd_init);
module_exit(boxer_lcd_exit);
MODULE_LICENSE("GPL");

