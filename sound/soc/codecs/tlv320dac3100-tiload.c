/*
 * linux/sound/soc/codecs/tlv320dac3100-tiload.c
 *
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 *
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
 * Rev 0.1 	 Dynamic programming   		Mistral         02-02-2010
 *
 *    The Dynamic programming support is added to codec DAC3100.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/control.h>

#include "tlv320dac3100.h"
#include "tlv320dac3100-tiload.h"

/* enable debug prints in the driver */
//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define dprintk(x...) 	printk(x)
#else
#define dprintk(x...)
#endif

#ifdef CONFIG_TILOAD

/* externs */
extern int dac3100_change_page (struct snd_soc_codec *codec, u8 new_page);
extern int dac3100_write (struct snd_soc_codec *codec, unsigned int reg,
			  unsigned int value);

int tiload_driver_init (struct snd_soc_codec *codec);

static unsigned int magic_num;

/************** Dynamic programming, TI LOAD support  ***************/
static struct cdev *tiload_cdev;
static int tiload_major = 0;	/* Dynamic allocation of Mjr No. */
static int tiload_opened = 0;	/* Dynamic allocation of Mjr No. */
static struct snd_soc_codec *tiload_codec;

/*
 *----------------------------------------------------------------------------
 * Function : tiload_open
 *
 * Purpose  : open method for dynamic programming interface
 *----------------------------------------------------------------------------
 */
static int
tiload_open (struct inode *in, struct file *filp)
{
  if (tiload_opened)
    {
      printk ("%s device is already opened\n", "tiload");
      printk ("%s: only one instance of driver is allowed\n", "tiload");
      return -1;
    }
  tiload_opened++;
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_release
 *
 * Purpose  : close method for dynamic programming interface
 *----------------------------------------------------------------------------
 */
static int
tiload_release (struct inode *in, struct file *filp)
{
  tiload_opened--;
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_read
 *
 * Purpose  : read method for dynamic programming interface
 *----------------------------------------------------------------------------
 */
static ssize_t
tiload_read (struct file *file, char __user * buf,
	     size_t count, loff_t * offset)
{
  static char rd_data[256];
  char reg_addr;
  size_t size;
  struct i2c_client *i2c = tiload_codec->control_data;

  if (count > 128)
    {
      printk ("Max 256 bytes can be read\n");
      count = 128;
    }

  /* copy register address from user space  */
  size = copy_from_user (&reg_addr, buf, 1);
  if (size != 0)
    {
      printk ("read: copy_from_user failure\n");
      return -1;
    }

  if (i2c_master_send (i2c, &reg_addr, 1) != 1)
    {
      dprintk ("Can not write register address\n");
      return -1;
    }
  /* read the codec registers */
  size = i2c_master_recv (i2c, rd_data, count);

  if (size != count)
    {
      printk ("read %d registers from the codec\n", size);
    }

  if (copy_to_user (buf, rd_data, size) != 0)
    {
      dprintk ("copy_to_user failed\n");
      return -1;
    }

  return size;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_write
 *
 * Purpose  : write method for dynamic programming interface
 *----------------------------------------------------------------------------
 */
static ssize_t
tiload_write (struct file *file, const char __user * buf,
	      size_t count, loff_t * offset)
{
  static char wr_data[258];
  size_t size;
  struct i2c_client *i2c = tiload_codec->control_data;

  /* copy buffer from user space  */
  size = copy_from_user (wr_data, buf, count);
  if (size != 0)
    {
      printk ("copy_from_user failure %d\n", size);
      return -1;
    }

  if (wr_data[0] == 0)
    {
      dac3100_change_page (tiload_codec, wr_data[1]);
    }

  size = i2c_master_send (i2c, wr_data, count);
  return size;
}

static int
tiload_ioctl (struct inode *inode, struct file *filp,
	      unsigned int cmd, unsigned long arg)
{
  int num = 0;
  void __user *argp = (void __user *) arg;
  if (_IOC_TYPE (cmd) != DAC3100_IOC_MAGIC)
    return -ENOTTY;

  switch (cmd)
    {
    case DAC3100_IOMAGICNUM_GET:
      num = copy_to_user (argp, &magic_num, sizeof (int));
      break;
    case DAC3100_IOMAGICNUM_SET:
      num = copy_from_user (&magic_num, argp, sizeof (int));
      break;
    }
  return num;
}

/*********** File operations structure for dynamic programming *************/
static struct file_operations tiload_fops = {
  .owner = THIS_MODULE,
  .open = tiload_open,
  .release = tiload_release,
  .read = tiload_read,
  .write = tiload_write,
  .ioctl = tiload_ioctl,
};

/*
 *----------------------------------------------------------------------------
 * Function : tiload_driver_init
 *
 * Purpose  : Registeer a char driver for dynamic programming
 *----------------------------------------------------------------------------
 */
int
tiload_driver_init (struct snd_soc_codec *codec)
{
  int result;
  dev_t dev = MKDEV (tiload_major, 0);

  tiload_codec = codec;

  dprintk ("allocating dynamic major number\n");

  result = alloc_chrdev_region (&dev, 0, 1, "tiload");

  if (result < 0)
    {
      dprintk ("cannot allocate major number %d\n", tiload_major);
      return result;
    }

  tiload_major = MAJOR (dev);
  dprintk ("allocated Major Number: %d\n", tiload_major);

  tiload_cdev = cdev_alloc ();
  cdev_init (tiload_cdev, &tiload_fops);
  tiload_cdev->owner = THIS_MODULE;
  tiload_cdev->ops = &tiload_fops;

  if (cdev_add (tiload_cdev, dev, 1) < 0)
    {
      dprintk ("tiload_driver: cdev_add failed \n");
      unregister_chrdev_region (dev, 1);
      tiload_cdev = NULL;
      return 1;
    }
  printk ("Registered tiload driver, Major number: %d \n", tiload_major);
  return 0;
}

#endif
