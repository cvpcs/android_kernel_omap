/*
 * Copyright (C) 2009-2010 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/err.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/leds-cpcap-button.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/cpcap.h>
#include <linux/spi/cpcap-regbits.h>

struct button_led{
	struct cpcap_device *cpcap;
	struct led_classdev cpcap_button_dev;
	struct regulator *regulator;
	int regulator_state;
	int blink;
};

static void cpcap_button_led_set(struct led_classdev *led_cdev,
			  enum led_brightness value)
{
	int cpcap_status = 0;
	struct cpcap_platform_data *data;
	struct spi_device *spi;
	struct cpcap_leds *leds;

	struct button_led *button_led =
	    container_of(led_cdev, struct button_led,
			 cpcap_button_dev);

	spi = button_led->cpcap->spi;
	data = (struct cpcap_platform_data *)spi->controller_data;
	leds = data->leds;

	if (value > LED_OFF) {
		if ((button_led->regulator) &&
		    (button_led->regulator_state == 0)) {
			regulator_enable(button_led->regulator);
			button_led->regulator_state = 1;
		}

		cpcap_status = cpcap_regacc_write(button_led->cpcap,
						  leds->button_led.button_reg,
						  leds->button_led.button_on,
						  leds->button_led.button_mask);

		if (cpcap_status < 0)
			pr_err("%s: Writing to the register failed for %i\n",
			       __func__, cpcap_status);

	} else {
		if ((button_led->regulator) &&
		    (button_led->regulator_state == 1)) {
			regulator_disable(button_led->regulator);
			button_led->regulator_state = 0;
		}

		cpcap_status = cpcap_regacc_write(button_led->cpcap,
						  leds->button_led.button_reg,
						  0x01,
						  leds->button_led.button_mask);
		if (cpcap_status < 0)
			pr_err("%s: Writing to the register failed for %i\n",
				__func__, cpcap_status);

		cpcap_status = cpcap_regacc_write(button_led->cpcap,
						  leds->button_led.button_reg,
						  0x00,
						  leds->button_led.button_mask);
		if (cpcap_status < 0)
			pr_err("%s: Writing to the register failed for %i\n",
			       __func__, cpcap_status);
	}
	return;
}
static ssize_t cpcap_button_led_blink_show(struct device *dev,
	struct device_attribute *attr, char *buf){
	struct button_led *led;
	struct platform_device *pdev = container_of(dev->parent,
		struct platform_device, dev);
	led = platform_get_drvdata(pdev);
	scnprintf(buf , PAGE_SIZE, "0x%X", led->blink);
	printk(KERN_INFO "Blink Reg value  = %x\n", led->blink);
	return sizeof(int);;
}

static ssize_t cpcap_msg_ind_blink(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count){
	unsigned long led_blink = LED_OFF;
	int ret;
	struct button_led *led;
	struct platform_device *pdev =
	container_of(dev->parent, struct platform_device, dev);
	led = platform_get_drvdata(pdev);
	ret = strict_strtoul(buf, 10, &led_blink);
	printk(KERN_INFO "Blink value %lu\n", led_blink);
	if (ret != 0) {
		pr_err("%s: Invalid parameter sent\n", __func__);
		return -1;
	}
	if (led_blink > LED_OFF)
		cpcap_uc_start(led->cpcap, CPCAP_MACRO_6);
	else
		cpcap_uc_stop(led->cpcap, CPCAP_MACRO_6);
	led->blink = led_blink;
	return 0;
}

static DEVICE_ATTR(blink, 0666,
	cpcap_button_led_blink_show, cpcap_msg_ind_blink);



static int cpcap_button_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct button_led *led;

	if (pdev == NULL) {
		pr_err("%s: platform data required - %d\n", __func__, -ENODEV);
		return -ENODEV;

	}
	led = kzalloc(sizeof(struct button_led), GFP_KERNEL);
	if (led == NULL) {
		pr_err("%s: Unable to allocate memory %d\n", __func__, -ENOMEM);
		return -ENOMEM;
	}

	led->cpcap = pdev->dev.platform_data;
	platform_set_drvdata(pdev, led);

	led->regulator = regulator_get(NULL, "sw5");
	if (IS_ERR(led->regulator)) {
		pr_err("%s: Cannot get %s regulator\n", __func__, "sw5");
		ret = PTR_ERR(led->regulator);
		goto exit_request_reg_failed;

	}

	led->regulator_state = 0;

	led->cpcap_button_dev.name = CPCAP_BUTTON_DEV;
	led->cpcap_button_dev.brightness_set = cpcap_button_led_set;
	ret = led_classdev_register(&pdev->dev, &led->cpcap_button_dev);
	if (ret) {
		printk(KERN_ERR "Register button led class failed %d\n", ret);
		goto err_reg_button_failed;
	}
	if ((device_create_file(led->cpcap_button_dev.dev,
			&dev_attr_blink)) < 0) {
		pr_err("%s:File creation failed %d\n", __func__, -ENODEV);
		goto err_dev_blink_create_failed;
	}


	return ret;
err_dev_blink_create_failed:
		led_classdev_unregister(&led->cpcap_button_dev);

err_reg_button_failed:
	if (led->regulator)
		regulator_put(led->regulator);
exit_request_reg_failed:
	kfree(led);
	return ret;
}

static int cpcap_button_led_remove(struct platform_device *pdev)
{
	struct button_led *led = platform_get_drvdata(pdev);

	if (led->regulator)
		regulator_put(led->regulator);

	device_remove_file(led->cpcap_button_dev.dev, &dev_attr_blink);

	led_classdev_unregister(&led->cpcap_button_dev);
	kfree(led);
	return 0;
}

static struct platform_driver cpcap_button_driver = {
	.probe   = cpcap_button_led_probe,
	.remove  = cpcap_button_led_remove,
	.driver  = {
		.name  = CPCAP_BUTTON_DEV,
	},
};

static int cpcap_button_led_init(void)
{
	return cpcap_driver_register(&cpcap_button_driver);
}

static void cpcap_button_led_shutdown(void)
{
	platform_driver_unregister(&cpcap_button_driver);
}

module_init(cpcap_button_led_init);
module_exit(cpcap_button_led_shutdown);

MODULE_DESCRIPTION("Icon/Button Lighting Driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GNU");






