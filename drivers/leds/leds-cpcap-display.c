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
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/leds-cpcap-display.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/cpcap.h>
#include <linux/spi/cpcap-regbits.h>
#include <linux/earlysuspend.h>


struct display_led {
	struct input_dev *idev;
	struct platform_device *pdev;
	struct cpcap_device *cpcap;
	struct led_classdev cpcap_display_dev;
	struct workqueue_struct *working_queue;
	struct delayed_work dwork;
	struct work_struct work;
	struct early_suspend suspend;
	struct cpcap_leds *leds;
	struct regulator *regulator;
	int regulator_state;
	int prev_als_reading;
	int light_percent;
	u8 mode;
	u8 curr_zone;
	u8 prev_zone;
	u8 last_brightness;
	unsigned int als2lux_rate;

	struct mutex sreq_lock; /* Lock request queue access */
	enum led_brightness req_queue[25];
	u8 sreq_read;
	u8 sreq_write;
	struct delayed_work bright_dwork;
	unsigned long prior_jiffies;
	unsigned long bright_delay;
};
struct cpcap_als_reg {
	const char *name;
	uint8_t reg;
} cpcap_als_regs[] = {
	{"CPCAP_MDLC_REG", CPCAP_REG_MDLC},
};

static uint32_t cpcap_als_debug;
module_param_named(als_debug, cpcap_als_debug, uint, 0664);

static void cpcap_display_led_hw_set(struct led_classdev *led_cdev,
			  enum led_brightness value)
{
	int cpcap_status = 0;
	u16 cpcap_backlight_set = 0;
	struct cpcap_leds *leds;
	struct display_led *d_led =
	    container_of(led_cdev, struct display_led,
			 cpcap_display_dev);

	leds = d_led->leds;
	d_led->last_brightness = value;
	if (value > LED_OFF) {
		cpcap_backlight_set = (value * 44 / 100) << 5;
		cpcap_backlight_set |= leds->display_led.display_init;
		cpcap_status = cpcap_regacc_write(d_led->cpcap,
					leds->display_led.display_reg,
					cpcap_backlight_set,
					leds->display_led.display_mask);
		d_led->prior_jiffies = jiffies;
	} else {
		/* Turn off duty cycle prior to disabling */
		cpcap_status = cpcap_regacc_write(d_led->cpcap,
					leds->display_led.display_reg,
					leds->display_led.display_init,
					leds->display_led.display_mask);
		cpcap_status = cpcap_regacc_write(d_led->cpcap,
					leds->display_led.display_reg,
					0x00,
					leds->display_led.display_mask);
		d_led->bright_delay = jiffies - d_led->prior_jiffies;
		if (d_led->bright_delay > (HZ / 15))
			d_led->bright_delay = HZ / 15;
	}
	if (cpcap_status < 0)
		pr_err("%s: Failed setting brightness %i\n",
			__func__, cpcap_status);
}

static void cpcap_display_led_process_set(struct led_classdev *led_cdev,
		int req_brightness)
{
	struct display_led *d_led =
	    container_of(led_cdev, struct display_led,
			 cpcap_display_dev);

	if (cpcap_als_debug) {
		pr_info("%s %d %d %d\n", __func__, req_brightness,
			d_led->sreq_read, d_led->sreq_write);
	}

	mutex_lock(&d_led->sreq_lock);
	/* Dequeue */
	if (req_brightness == -1) {
		cpcap_display_led_hw_set(led_cdev,
			d_led->req_queue[d_led->sreq_read]);
		d_led->sreq_read++;
		if (d_led->sreq_read > (sizeof(d_led->req_queue) /
			sizeof(d_led->req_queue[0]) - 1))
			d_led->sreq_read = sizeof(d_led->req_queue) /
				sizeof(d_led->req_queue[0]) - 1;
		if (d_led->sreq_read != d_led->sreq_write) {
			queue_delayed_work(d_led->working_queue,
				&d_led->bright_dwork, d_led->bright_delay);
		} else {
			d_led->sreq_read = 0;
			d_led->sreq_write = 0;
		}
	/* Enqueue */
	} else if ((req_brightness > d_led->last_brightness) &&
			(((d_led->regulator) && (d_led->regulator_state == 0))
			|| (d_led->sreq_read != d_led->sreq_write))) {
		d_led->req_queue[d_led->sreq_write] = req_brightness;
		d_led->sreq_write++;
		if (d_led->sreq_write >	(sizeof(d_led->req_queue) /
			sizeof(d_led->req_queue[0]) - 1))
			d_led->sreq_write = sizeof(d_led->req_queue) /
				sizeof(d_led->req_queue[0]) - 1;
	/* Realtime */
	} else {
		cancel_delayed_work(&d_led->bright_dwork);
		cpcap_display_led_hw_set(led_cdev, req_brightness);
		d_led->sreq_read = 0;
		d_led->sreq_write = 0;
	}
	mutex_unlock(&d_led->sreq_lock);
	return;
}

static void cpcap_display_led_framework_set(struct led_classdev *led_cdev,
			  enum led_brightness value)
{
	if (cpcap_als_debug)
		pr_info("%s %d\n", __func__, value);
	cpcap_display_led_process_set(led_cdev, value);
	return;
}

static void cpcap_display_led_queue_set(struct work_struct *work)
{
	struct display_led *d_led = container_of((struct delayed_work *)work,
		struct display_led, bright_dwork);
	if (cpcap_als_debug)
		pr_info("%s\n", __func__);
	cpcap_display_led_process_set(&d_led->cpcap_display_dev, -1);
	return;
}

static void cpcap_display_led_read_a2d(struct display_led *d_led)
{
	int err;
	struct cpcap_adc_request request;
	struct cpcap_device *cpcap = d_led->cpcap;
	unsigned int  light_value = 0, lux_value = 0;

	request.format = CPCAP_ADC_FORMAT_RAW;
	request.timing = CPCAP_ADC_TIMING_IMM;
	request.type = CPCAP_ADC_TYPE_BANK_1;

	err = cpcap_adc_sync_read(cpcap, &request);
	if (err < 0) {
		pr_err("%s: A2D read error %d\n", __func__, err);
		return;
	}
	light_value = request.result[CPCAP_ADC_TSY1_AD14];

	if (light_value < d_led->leds->als_data.als_min)
		lux_value = d_led->leds->als_data.lux_min;
	else
		lux_value = (light_value * d_led->als2lux_rate) >> 8;

	if (cpcap_als_debug) {
		pr_info("%s: ALS Data: %x\n",
		       __func__, light_value);
		pr_info("%s: Lux value: %x\n", __func__,
			lux_value);
	}
	input_event(d_led->idev, EV_MSC, MSC_RAW, light_value);
	input_event(d_led->idev, EV_LED, LED_MISC, lux_value);
	input_sync(d_led->idev);
}

static void cpcap_display_led_work(struct work_struct *work)
{
	struct display_led *d_led =
	  container_of(work, struct display_led, work);

	cpcap_display_led_read_a2d(d_led);
	if (cpcap_als_debug)
		pr_info("%s: Workqueue\n", __func__);

}

static void cpcap_display_led_dwork(struct work_struct *work)
{
	struct display_led *d_led =
	  container_of((struct delayed_work *)work, struct display_led, dwork);

	cpcap_display_led_read_a2d(d_led);

	queue_delayed_work(d_led->working_queue, &d_led->dwork,
			      msecs_to_jiffies(d_led->leds->display_led.
					       poll_intvl));
	if (cpcap_als_debug)
		pr_info("%s: Delayed Workqueue\n", __func__);

}

static ssize_t cpcap_als_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct display_led *d_led;
	struct platform_device *pdev =
	  container_of(dev->parent, struct platform_device, dev);

	unsigned i, n, reg_count;
	short unsigned int value;

	d_led = platform_get_drvdata(pdev);
	reg_count = sizeof(cpcap_als_regs) / sizeof(cpcap_als_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		cpcap_regacc_read(d_led->cpcap, cpcap_als_regs[i].reg, &value);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       cpcap_als_regs[i].name,
			       value);
		if (cpcap_als_debug) {
			pr_info("%s: %s Register read: %x\n",
				__func__, cpcap_als_regs[i].name, value);
		}
	}
	return n;
}

static ssize_t cpcap_als_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct display_led *d_led;
	struct platform_device *pdev =
	  container_of(dev->parent, struct platform_device, dev);

	unsigned i, reg_count, value;
	int error;
	char name[30];

	d_led = platform_get_drvdata(pdev);
	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}
	if (sscanf(buf, "%30s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	reg_count = sizeof(cpcap_als_regs) / sizeof(cpcap_als_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, cpcap_als_regs[i].name)) {
			error =	cpcap_regacc_write(d_led->cpcap,
				  cpcap_als_regs[i].reg,
				  value,
				  d_led->leds->display_led.display_mask);
			if (error) {
				pr_err("%s:Failed to write register %s\n",
					__func__, name);
				return -1;
			}
			if (cpcap_als_debug) {
				pr_info("%s: %s Register write: %x\n",
				       __func__, name, value);
			}
			return count;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, 0644, cpcap_als_registers_show,
		cpcap_als_registers_store);

static int cpcap_display_led_suspend(struct platform_device *pdev,
				     pm_message_t state)
{
	struct display_led *d_led = platform_get_drvdata(pdev);

	if (cpcap_als_debug)
		pr_info("%s : Suspending...\n", __func__);

	cancel_delayed_work(&d_led->bright_dwork);
	cancel_delayed_work_sync(&d_led->bright_dwork);
	if (d_led->last_brightness != LED_OFF)
		cpcap_display_led_process_set(&d_led->cpcap_display_dev,
						LED_OFF);
	if (d_led->mode == AUTOMATIC) {
		cancel_delayed_work(&d_led->dwork);
		cancel_delayed_work_sync(&d_led->dwork);
	}

	if ((d_led->regulator) && (d_led->regulator_state)) {
		regulator_disable(d_led->regulator);
		d_led->regulator_state = 0;
	}
	return 0;
}

static int cpcap_display_led_resume(struct platform_device *pdev)
{
	struct display_led *d_led = platform_get_drvdata(pdev);

	if (cpcap_als_debug)
		pr_info("%s : Resuming...\n", __func__);

	if (d_led->mode == AUTOMATIC) {
		queue_delayed_work(d_led->working_queue, &d_led->dwork,
			      msecs_to_jiffies(d_led->leds->display_led.
					       poll_intvl));
	}

	if ((d_led->regulator) && (d_led->regulator_state == 0)) {
		regulator_enable(d_led->regulator);
		d_led->regulator_state = 1;
	}

	if (d_led->sreq_read != d_led->sreq_write)
		queue_delayed_work(d_led->working_queue,
			&d_led->bright_dwork, HZ / 30);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cpcap_display_led_early_suspend(struct early_suspend *h)
{
	struct display_led *d_led =
	  container_of(h, struct display_led, suspend);
	if (cpcap_als_debug)
		pr_info("%s : Early Suspend...\n", __func__);

	cpcap_display_led_suspend(d_led->pdev, PMSG_SUSPEND);
}

static void cpcap_display_led_late_resume(struct early_suspend *h)
{
	struct display_led *d_led =
	  container_of(h, struct display_led, suspend);
	if (cpcap_als_debug)
		pr_info("%s : Late Resume\n", __func__);

	cpcap_display_led_resume(d_led->pdev);
}
#endif

static ssize_t cpcap_display_led_als_store(struct device *dev,
					   struct device_attribute
					   *attr, const char *buf, size_t size)
{
	unsigned long mode;
	struct display_led *d_led;
	struct platform_device *pdev =
	  container_of(dev->parent, struct platform_device, dev);

	d_led = platform_get_drvdata(pdev);

	if ((strict_strtoul(buf, 10, &mode)) < 0)
		return -1;

	if (cpcap_als_debug)
		pr_info("%s : als node - Store value %lu\n", __func__, mode);

	if (mode == AUTOMATIC) {
		d_led->mode = AUTOMATIC;
		queue_delayed_work(d_led->working_queue, &d_led->dwork,
			      msecs_to_jiffies(d_led->leds->display_led.
					       poll_intvl));
	} else {
		d_led->mode = MANUAL;
		cancel_delayed_work(&d_led->dwork);
		cancel_delayed_work_sync(&d_led->dwork);
	}

	return d_led->mode;
}

static ssize_t cpcap_display_led_als_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct display_led *d_led;
	struct platform_device *pdev =
	  container_of(dev->parent, struct platform_device, dev);

	d_led = platform_get_drvdata(pdev);

	scnprintf(buf , PAGE_SIZE, "0x%X", d_led->mode);
	if (cpcap_als_debug)
		pr_info("%s : als node - Show value %d\n",
			 __func__, d_led->mode);

	return 0;

}


static DEVICE_ATTR(als, 0644, cpcap_display_led_als_show,
	cpcap_display_led_als_store);

static int cpcap_display_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct display_led *d_led;

	struct cpcap_platform_data *data;
	struct spi_device *spi;
	unsigned int num, den;

	if (pdev == NULL) {
		pr_err("%s: platform data required %d\n", __func__, -ENODEV);
		return -ENODEV;

	}
	d_led = kzalloc(sizeof(struct display_led), GFP_KERNEL);
	if (d_led == NULL) {
		pr_err("%s: Unable to allacate memory %d\n", __func__, -ENOMEM);
		return -ENOMEM;
	}

	d_led->pdev = pdev;
	d_led->cpcap = pdev->dev.platform_data;
	spi = d_led->cpcap->spi;
	data = (struct cpcap_platform_data *)spi->controller_data;
	d_led->leds = data->leds;
	d_led->mode = AUTOMATIC;
	platform_set_drvdata(pdev, d_led);

	d_led->idev = input_allocate_device();
	if (!d_led->idev) {
		ret = -ENOMEM;
		pr_err("%s: input device allocate failed: %d\n", __func__,
		       ret);
		goto err_input_allocate_failed;
	}

	d_led->idev->name = BACKLIGHT_ALS;
	num = (unsigned int)((d_led->leds->als_data.lux_max -
			d_led->leds->als_data.lux_min) << 16);
	den = (unsigned int)((d_led->leds->als_data.als_max -
			d_led->leds->als_data.als_min) << 8);
	if (den != 0)
		d_led->als2lux_rate = (unsigned int)(num / den);
	else
		d_led->als2lux_rate = 0xFFFF00;

	input_set_capability(d_led->idev, EV_MSC, MSC_RAW);
	input_set_capability(d_led->idev, EV_LED, LED_MISC);

	if (input_register_device(d_led->idev)) {
		pr_err("%s: input device register failed\n", __func__);
		goto err_input_register_failed;
	}

	d_led->regulator = regulator_get(NULL, "sw5");
	if (IS_ERR(d_led->regulator)) {
		pr_err("%s: Cannot get %s regulator\n", __func__, "sw5");
		ret = PTR_ERR(d_led->regulator);
		goto err_reg_request_failed;

	}
	d_led->regulator_state = 0;

	d_led->cpcap_display_dev.name = CPCAP_DISPLAY_DEV;
	d_led->cpcap_display_dev.brightness_set =
		cpcap_display_led_framework_set;
	ret = led_classdev_register(&pdev->dev, &d_led->cpcap_display_dev);
	if (ret) {
		printk(KERN_ERR "Register display led class failed %d\n", ret);
		goto err_reg_led_class_failed;
	}

	if ((device_create_file(d_led->cpcap_display_dev.dev,
				&dev_attr_als)) < 0) {
		pr_err("%s:File device creation failed: %d\n",
		       __func__, -ENODEV);
		goto err_dev_als_create_failed;
	}
	if ((device_create_file(d_led->cpcap_display_dev.dev,
				&dev_attr_registers)) < 0) {
		pr_err("%s:File device creation failed: %d\n",
		       __func__, -ENODEV);
		goto err_dev_registers_create_failed;
	}
	d_led->working_queue = create_singlethread_workqueue("cpcap_als_wq");
	if (!d_led->working_queue) {
		pr_err("%s: Cannot create work queue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_wq_failed;
	}
	INIT_WORK(&d_led->work, cpcap_display_led_work);

#ifdef CONFIG_HAS_EARLYSUSPEND
	d_led->suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	d_led->suspend.suspend = cpcap_display_led_early_suspend;
	d_led->suspend.resume = cpcap_display_led_late_resume;
	register_early_suspend(&d_led->suspend);
#endif
	queue_work(d_led->working_queue, &d_led->work);

	INIT_DELAYED_WORK(&d_led->dwork, cpcap_display_led_dwork);
	queue_delayed_work(d_led->working_queue, &d_led->dwork,
			      msecs_to_jiffies(d_led->leds->display_led.
					       poll_intvl));
	mutex_init(&d_led->sreq_lock);

	d_led->bright_delay = HZ / 15;
	INIT_DELAYED_WORK(&d_led->bright_dwork, cpcap_display_led_queue_set);
	cpcap_display_led_resume(d_led->pdev);

	return ret;

err_create_singlethread_wq_failed:
	if (d_led->working_queue)
		destroy_workqueue(d_led->working_queue);
	device_remove_file(d_led->cpcap_display_dev.dev, &dev_attr_registers);
err_dev_registers_create_failed:
	device_remove_file(d_led->cpcap_display_dev.dev, &dev_attr_als);
err_dev_als_create_failed:
	led_classdev_unregister(&d_led->cpcap_display_dev);
err_reg_led_class_failed:
	if (d_led->regulator)
		regulator_put(d_led->regulator);
err_reg_request_failed:
err_input_register_failed:
	input_free_device(d_led->idev);
err_input_allocate_failed:
	kfree(d_led);
	return ret;
}

static int cpcap_display_led_remove(struct platform_device *pdev)
{
	struct display_led *d_led = platform_get_drvdata(pdev);

	if (d_led->regulator)
		regulator_put(d_led->regulator);

	cancel_delayed_work(&d_led->dwork);
	cancel_delayed_work_sync(&d_led->dwork);

	cancel_delayed_work(&d_led->bright_dwork);
	cancel_delayed_work_sync(&d_led->bright_dwork);

	if (d_led->working_queue)
		destroy_workqueue(d_led->working_queue);

	device_remove_file(d_led->cpcap_display_dev.dev, &dev_attr_als);
	device_remove_file(d_led->cpcap_display_dev.dev, &dev_attr_registers);
	led_classdev_unregister(&d_led->cpcap_display_dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&d_led->suspend);
#endif
	input_unregister_device(d_led->idev);
	kfree(d_led);
	return 0;
}

static struct platform_driver cpcap_display_led_driver = {
	.probe   = cpcap_display_led_probe,
	.remove  = cpcap_display_led_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = cpcap_display_led_suspend,
	.resume  = cpcap_display_led_resume,
#endif
	.driver  = {
		.name  = CPCAP_DISPLAY_DRV,
	},
};


static int cpcap_display_led_init(void)
{
	return cpcap_driver_register(&cpcap_display_led_driver);
}

static void cpcap_display_led_shutdown(void)
{
	platform_driver_unregister(&cpcap_display_led_driver);
}

module_init(cpcap_display_led_init);
module_exit(cpcap_display_led_shutdown);

MODULE_DESCRIPTION("Display Lighting Driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GNU");






