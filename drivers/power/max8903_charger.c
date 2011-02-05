/*
 * linux/drivers/power/max8903_charger.c
 *
 * Copyright (C) 2010 Barnes & Noble, Inc.
 * 
 * Lei Cao
 * lcao@book.com
 *
 * Max8903 PMIC driver for Linux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <plat/board-boxer.h>
#include <linux/i2c/twl4030.h>
#include <linux/completion.h>

//#define DEBUG(x...)   printk(x)
#define DEBUG(x...)

/*macro for TRITON4030 usb controller reg access*/
#define TRITON_USB_DTCT_CTRL       0x02
#define TRITON_USB_SW_CHRG_CTLR_EN (1 << 1)
#define TRITON_USB_HW_CHRG_CTLR_EN (1 << 0)

#define TRITON_USB_CHGR_USB       0x4
#define TRITON_USB_CHGR_WALL      0x8
#define TRITON_USB_CHGR_UNKNOWN   0xC
#define TRITON_USB_CHGR_MSK       0xC

#define TRITON_USB_500_FSM      (1<<6)
#define TRITON_USB_100_FSM      (1<<5)
#define TRITON_USB_FSM_MSK      (3<<5)

#define TRITON_USB_SW_CHRG_CTRL 0x03

#define TRITON_USB_CHG_DET_EN_SW (1 << 7)
#define TRITON_USB_CHGD_SERX_EN_LOWV (1<<2)

#define TRITON_CHGD_SERX_DM_LOWV    (1 << 5)
#define TRITON_CHGD_SERX_DP_LOWV    (1 << 4)

#define TRITON_USB_OTG_CTRL      0x0A
#define TRITON_USB_OTG_CTRL_CLR  0x0C
#define TRITON_USB_DM_DP_PD      (0x3 << 1)

#define TRITON_USB_DBG_CTRL      0x15
#define TRITON_USB_DMDP_HV       0x3
#define TRITON_USB_DMDP_LV       0x0
#define TRITON_USB_DMDP_MSK      0x3

#define OPMODE_NON_DRIVING       (1 << 3)
#define SUSPENDM                 (1 << 6)
#define TRITON_USB_FUNC_CTRL     0x4
#define TRITON_USB_FUNC_CTRL_SET 0x5
#define TRITON_USB_FUNC_CTRL_CLR 0x6

/*amperage def*/
#define USB_CURRENT_LIMIT_LOW    100000   /* microAmp */
#define USB_CURRENT_LIMIT_HIGH   500000   /* microAmp */
#define AC_CURRENT_LIMIT        1900000  /* microAmp */

#define CHG_ILM_SELECT_AC   1
#define CHG_ILM_SELECT_USB  0

#define CHG_IUSB_SELECT_500mA   1
#define CHG_IUSB_SELECT_100mA   0

#define DEBOUNCE_TIME   (msecs_to_jiffies(500))

/*polarity def*/
#define ENABLED   0
#define DISABLED  1

/*charger type def*/
#define DC_STS     (1<<2)
#define VBUS_STS   (1<<1)
#define DMDP_STS   (1<<0)


#define CHARGER_MAX_TYPE     8
#define DC_WALL_CHARGER      (DC_STS)
#define BN_USB_CHARGER       (DC_STS|VBUS_STS)
#define BN_WALL_CHARGER      (DC_STS|VBUS_STS|DMDP_STS)
#define STD_USB_CHARGER      (VBUS_STS)
#define STD_WALL_CHARGER     (VBUS_STS|DMDP_STS)
const char* charger_str[CHARGER_MAX_TYPE]={"INVALID CHARGER",
                                                         "INVALID CHARGER",
                                                         "USB CHARGER(500mA)",
                                                         "WALL CHARGER(500mA)",
                                                         "DC CHARGER(2A)",
                                                         "INVALID CHARGER",
                                                         "B&N USB  CHARGER(500mA)",
                                                         "B&N WALL CHARGER(2A)"};
        
extern void max17042_update_callback(u8 charger_in);

DEFINE_MUTEX(charger_mutex);
static DECLARE_COMPLETION(charger_probed);

/*data struct for the charger status and control*/
struct max8903_charger {
    struct delayed_work max8903_charger_detect_work;
    int adapter_active;
    int adapter_online;
    int adapter_curr_limit;
    int usb_active;
    int usb_online;
    u8 temperature_ok;
    u8 ftm;                /*Factory Test Mode, in this mode charger on/off is controlled by sysfs entry "ctrl_charge"*/
    struct power_supply usb;
    struct power_supply adapter;
};


static struct max8903_charger* charger;
int uok_irq=0,dok_irq=0;

static inline void soft_connect(int connect)
{
    if (connect)
    {
        twl4030_i2c_write_u8(TWL4030_MODULE_USB, OPMODE_NON_DRIVING, TRITON_USB_FUNC_CTRL_CLR);
    }
    else
    {
        // DM/DP set
        twl4030_i2c_write_u8(TWL4030_MODULE_USB,
                         TRITON_USB_DM_DP_PD,
                         TRITON_USB_OTG_CTRL_CLR);

        twl4030_i2c_write_u8(TWL4030_MODULE_USB, OPMODE_NON_DRIVING, TRITON_USB_FUNC_CTRL_SET);
        // Power down the PHY
        // TODO
    }
}

static inline void reset_charger_detect_fsm(void)
{
    /*enable SW control mode of TRITON USB detection */
    twl4030_i2c_write_u8(TWL4030_MODULE_MAIN_CHARGE,
                       TRITON_USB_SW_CHRG_CTLR_EN,
                       TRITON_USB_DTCT_CTRL);

    /*enable HW control mode of TRITON USB detection (FSM mode)*/
    twl4030_i2c_write_u8(TWL4030_MODULE_MAIN_CHARGE,
                       TRITON_USB_HW_CHRG_CTLR_EN,
                       TRITON_USB_DTCT_CTRL);
}

/*~CEN control*/
void max8903_enable_charge(u8 enable)
{
    int prev_status;

    /*this code will be triggered for FTM only*/
    if(charger->ftm)
    {
        if(enable){
                printk("MAX8903-TESTING: Charger is now On !\n");
                gpio_set_value(MAX8903_GPIO_CHG_EN,ENABLED);
            }
        else{
                printk("MAX8903-TESTING: Charger is now Off !\n");
                gpio_set_value(MAX8903_GPIO_CHG_EN,DISABLED);
        }
        return;
    }
    
    /*probe previous charging status*/
    mutex_lock(&charger_mutex);
    prev_status=gpio_get_value(MAX8903_GPIO_CHG_EN);

    switch(prev_status)
    {
    case ENABLED:
          if(enable){
                printk("MAX8903: Charging is already enabled!\n");
          }
          else{
                gpio_set_value(MAX8903_GPIO_CHG_EN,DISABLED);
                printk("MAX8903: Charging is now disabled!\n");
            }
          break;
    default:
    case DISABLED:
           if(enable){
                printk("MAX8903: Charging is now enabled!\n");
                gpio_set_value(MAX8903_GPIO_CHG_EN,ENABLED);
          }
          else{
                printk("MAX8903: Charging is already disabled!\n");
            }
          break;    
    }
    
       mutex_unlock(&charger_mutex);
   
#ifdef SW_LED_CTRL
    msleep(1000);

    /* turn on/off LED WRT to charging status*/
    current_status = gpio_get_value(MAX8903_GPIO_CHG_STATUS);
    
    if(current_status  != DISABLED ){
            
        printk("MAX8903: Charging is detected enabled! LED ON\n");
        /*turn on buffer gate regardless if it is a VBUS or DC-CHG*/

        gpio_direction_input(MAX8903_GPIO_CHG_STATUS);
        gpio_set_value(MAX8903_DOK_GPIO_FOR_IRQ,1);
        gpio_direction_output(MAX8903_DOK_GPIO_FOR_IRQ,1);
        printk("LED UP\n");
    }
    else{
        printk("MAX8903: Charging is detected disabled! LED OFF\n");
        /*turn off buffer gate */  
        gpio_direction_output(MAX8903_DOK_GPIO_FOR_IRQ,1);
    }
#endif  
 
}

/*charger differentiation*/
/*                             
  5  types of charger:
   
  I.   DC charger:  DC-CHG only, we charge via DC input limit 2A   ( development use ONLY)
  II.  BN charger:  VBUS/DC signal,D+/D- shorted, we charge via DC input,limit 2A
  III. USB charger: VBUS/DC signal,D+/D- not shorted,charge via VBUS,limit 500mA
  IV.  USB charger: VBUS only,D+/D- not shorted,charge via VBUS,limit 500mA 
  V.   USB charger: VBUS only,D+/D- shorted,charge via VBUS,limit 500mA
*/
void max8903_charger_enable(int on)
{
    u8 value;
    u8 charger_type = 0;

    wait_for_completion(&charger_probed);   

    /*USB/WALL removal*/
    if (on == 0) {
        soft_connect(0);
        charger->adapter_online = 0;
        charger->adapter_active = 0;
        gpio_set_value(MAX8903_GPIO_CHG_ILM, CHG_ILM_SELECT_USB);
        charger->adapter_curr_limit = USB_CURRENT_LIMIT_HIGH; /* back to high limit by default */
        charger->usb_online = 0;
        charger->usb_active = 0;
        power_supply_changed(&charger->usb);
        power_supply_changed(&charger->adapter);
        max17042_update_callback(0);
        max8903_enable_charge(0);
        printk("Charger Unplugged!\n");
    }

    /*insertion*/
    else {
        /*check DM/DP*/
        twl4030_i2c_read(TWL4030_MODULE_MAIN_CHARGE, &value, TRITON_USB_DTCT_CTRL, 1);

        if ((value & TRITON_USB_CHGR_MSK) == TRITON_USB_CHGR_UNKNOWN) {
            // Oh dear, charger state machine seems to have crapped itself,
            // reset it and see if it recovers
            printk(KERN_ERR "Unknown charger type: 0x%X, resetting charger state machine\n", value);
            reset_charger_detect_fsm();
        }
        // Sleep for half a second here, waiting for the 
        // state machine to reset/do its detection thing
        msleep(500);

        /*check DC*/
        if ( !gpio_get_value(MAX8903_DOK_GPIO_FOR_IRQ) ) {
            charger_type |= DC_STS;
        }

        /*check VBUS*/
        if ( !gpio_get_value(MAX8903_UOK_GPIO_FOR_IRQ) ) {
            charger_type |= VBUS_STS;
        }

        /*check DM/DP*/
        twl4030_i2c_read(TWL4030_MODULE_MAIN_CHARGE, &value, TRITON_USB_DTCT_CTRL, 1);
        if ( (value & TRITON_USB_CHGR_MSK)== TRITON_USB_CHGR_WALL)
        {
            charger_type|=DMDP_STS;
        }

        /*determine charger type*/
        if (charger_type <=7) {
            printk("%s Detected!\n",charger_str[charger_type]);
        }
        /*should never reach this */
        else {
            printk("!!!!   Charger Detection Fault!  !!!!\n");
        }
        switch(charger_type) {
            case DC_WALL_CHARGER:
            case BN_WALL_CHARGER:
                charger->adapter_online = 1;
                charger->adapter_active = 1;
                charger->adapter_curr_limit = AC_CURRENT_LIMIT;
                gpio_set_value(MAX8903_GPIO_CHG_ILM, CHG_ILM_SELECT_AC);
                max17042_update_callback(1);
                max8903_enable_charge(1);
                power_supply_changed(&charger->adapter);
                break;
            case STD_WALL_CHARGER :
                charger->adapter_online = 1;
                charger->adapter_active = 1;
                charger->adapter_curr_limit = USB_CURRENT_LIMIT_HIGH;
                gpio_set_value(MAX8903_GPIO_CHG_IUSB, CHG_IUSB_SELECT_500mA);
                gpio_set_value(MAX8903_GPIO_CHG_ILM, CHG_ILM_SELECT_USB);
                max17042_update_callback(1);
                max8903_enable_charge(1);
                power_supply_changed(&charger->adapter);
                break;
            case BN_USB_CHARGER:
            case STD_USB_CHARGER :
            /*everything else, use it as USB charger*/
            default:
                soft_connect(1); // We want USB enumeration for this type.
                charger->usb_online = 1;
                charger->usb_active = 1;
                charger->adapter_curr_limit = USB_CURRENT_LIMIT_HIGH;
                gpio_set_value(MAX8903_GPIO_CHG_IUSB, CHG_IUSB_SELECT_500mA);
                gpio_set_value(MAX8903_GPIO_CHG_ILM, CHG_ILM_SELECT_USB);
                max17042_update_callback(1);
                max8903_enable_charge(1);
                power_supply_changed(&charger->usb);
                break;
        }
    }
}

static irqreturn_t max8903_fault_interrupt(int irq, void *_di)
{
    // struct max8903_charger *di=_di;
    disable_irq(irq);
    printk("Fault Status Changed!\n");
    enable_irq(irq);
    return IRQ_HANDLED;
}

/*WALL online status*/
static int adapter_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{

    struct max8903_charger *mc= container_of(psy, struct max8903_charger, adapter);
    int ret = 0;

    switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                val->intval =  mc->adapter_online;
                break;
        case POWER_SUPPLY_PROP_CURRENT_AVG:
                val->intval = mc->adapter_curr_limit;
                break;
        default:
                ret = -EINVAL;
                break;
    }

        return ret;
}
/*USB online status*/
static int usb_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
        struct max8903_charger *mc= container_of(psy, struct max8903_charger, usb);
        int ret = 0;
        
        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = mc->usb_online;
            break;
        case POWER_SUPPLY_PROP_CURRENT_AVG:
            val->intval = USB_CURRENT_LIMIT_HIGH;
            break;
        default:
            ret = -EINVAL;
            break;
        }
        
        return ret;
}

/*only interested in ONLINE property*/
static enum power_supply_property power_props[] = {
POWER_SUPPLY_PROP_ONLINE,
POWER_SUPPLY_PROP_CURRENT_AVG,
};

/****************************************
 For manufaturing test purpose only
*****************************************/
static ssize_t set_charge(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    char* command[]={"LOW","HIGH"}; 
    int index=0;
    if(!charger)
        return -EINVAL;

    index=simple_strtol(buf,0,10);
    if(index>1){
        printk("MAX8903-TESTING: INVALID COMMAND only 1 and 0 are acceptable!!\n");
        return -EINVAL;
    }

   printk("MAX8903-TESTING:  == %s!\n",command[index]);
   /*set the FTM mode*/
   if(charger->ftm!=1)
        charger->ftm=1;
    /*for evt1b and older HW, pulling low on ~CEN means enable charge, EVT2 and higher will reverse this logic 
      a board HW rev detection will be put in u-boot for differentiation*/
    max8903_enable_charge(1-index);    

    if(index){
       printk("MAX8903-TESTING: PULL HIGH PIN(~CEN)!\n");
    }
    else{   
            printk("MAX8903-TESTING: PULL LOW PIN(~CEN)!\n");
        }
    return 0;
}

/*no need to read entry, write only for controlling the charging circuit*/
static DEVICE_ATTR(ctrl_charge, S_IRUGO | S_IWUSR, NULL, set_charge);

static struct attribute *max8903_control_sysfs_entries[] = {
    &dev_attr_ctrl_charge.attr,
    NULL,
};

static struct attribute_group max8903_attr_group = {
    .name   = NULL,         /* put in device directory */
    .attrs  = max8903_control_sysfs_entries,
};

/*Charger Module Initialization*/
static int __init max8903_charger_probe(struct platform_device *pdev)
{

    struct max8903_charger *mc;
    int ret,flt_irq;

    /*allocate mem*/
    mc = kzalloc(sizeof(*mc), GFP_KERNEL);
    if (!mc)
        return -ENOMEM;
    charger = mc;
    platform_set_drvdata(pdev, mc);

    printk("MAXIM 8903 Charger Initializing...\n");
    //INIT_DELAYED_WORK(&mc->max8903_charger_detect_work, max8903_charger_detect);
    /* Create power supplies for WALL/USB which are the only 2 ext supplies*/
    mc->adapter.name        = "ac";
    mc->adapter.type        = POWER_SUPPLY_TYPE_MAINS;
    mc->adapter.properties      = power_props;
    mc->adapter.num_properties  = ARRAY_SIZE(power_props);
    mc->adapter.get_property    = &adapter_get_property;

    mc->usb.name            = "usb";
    mc->usb.type            = POWER_SUPPLY_TYPE_USB;
    mc->usb.properties      = power_props;
    mc->usb.num_properties      = ARRAY_SIZE(power_props);
    mc->usb.get_property        = usb_get_property;

    mc->adapter_online = 0;
    mc->adapter_active = 0;
    mc->adapter_curr_limit = USB_CURRENT_LIMIT_HIGH; /* default limit is high limit */
    mc->usb_online = 0;
    mc->usb_active = 0;

    ret = power_supply_register(&pdev->dev, &mc->adapter);
    if (ret) {
        printk("MAXIM 8903 failed to register WALL CHARGER\n");
        goto exit0;
    }

    ret = power_supply_register(&pdev->dev, &mc->usb);
    if (ret) {
        printk("MAXIM 8903 failed to register USB CHARGER\n");
        goto exit1;
    }

    /****************************/
    /*Control pins configuration*/  
    /****************************/
    /*~DOK Status*/
    if (gpio_request(MAX8903_DOK_GPIO_FOR_IRQ, "max8903_chg_dok") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_dok\n");
        goto exit2;
    }
    /*~UOK Status*/
    if (gpio_request(MAX8903_UOK_GPIO_FOR_IRQ, "max8903_chg_uok") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_uok\n");
        goto exit3;
    }
    gpio_direction_input(MAX8903_UOK_GPIO_FOR_IRQ);
    /*~CHG Status*/
    if (gpio_request(MAX8903_GPIO_CHG_STATUS, "max8903_chg_chg") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_chg\n");
        goto exit4;
    }
    gpio_direction_input(MAX8903_GPIO_CHG_STATUS);         

    /*IUSB control*/
    if (gpio_request(MAX8903_GPIO_CHG_IUSB, "max8903_chg_iusb") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_iusb\n");
        goto exit5;
    }
    gpio_direction_output(MAX8903_GPIO_CHG_IUSB, CHG_IUSB_SELECT_500mA);    

    /*USUS control*/
    if (gpio_request(MAX8903_GPIO_CHG_USUS, "max8903_chg_usus") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_usus\n");
        goto exit6;
    }

    gpio_direction_output(MAX8903_GPIO_CHG_USUS, 0);    

    /*~CEN control */
    if (gpio_request(MAX8903_GPIO_CHG_EN, "max8903_chg_en") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_en\n");
        goto exit7;
    }
    gpio_direction_output(MAX8903_GPIO_CHG_EN, DISABLED);

    if (gpio_request(MAX8903_GPIO_CHG_ILM, "max8903_chg_ilm") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_ilm\n");
        goto exit8;
    }
    gpio_direction_output(MAX8903_GPIO_CHG_ILM, CHG_ILM_SELECT_USB);    /* set to USB  current limit by default */

    if (gpio_request(MAX8903_GPIO_CHG_FLT, "max8903_chg_flt") < 0) {
        printk(KERN_ERR "Can't get GPIO for max8903 chg_flt\n");
        goto exit9;
    }
    gpio_direction_input(MAX8903_GPIO_CHG_FLT);

    /*~FLT status*/
    flt_irq= gpio_to_irq(MAX8903_GPIO_CHG_FLT) ;
    ret  = request_irq( flt_irq,
                        max8903_fault_interrupt,
                        IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_DISABLED,
                        "max8903-fault-irq",
                        mc);

    printk("MAXIM8903: Request CHARGER FLT IRQ successfully!\n");
    if (ret < 0) {
        printk(KERN_ERR "MAXIM 8903 Can't Request IRQ for max8903 flt_irq\n");
        goto exita;
    }

    /*
    create sysfs for manufacture testing coverage on charging
    the operator should be able to write 1 to turn on the charging and 0 to
    turn off the charging to verify the charging circuit is functioning
    */
    ret = sysfs_create_group(&pdev->dev.kobj, &max8903_attr_group);

    if (ret){
        printk(KERN_ERR "MAXIM 8903 Can't Create Sysfs Entry for FTM\n");
        goto exitd;
    }

    complete_all(&charger_probed);
    return 0;

exitd:
    free_irq(flt_irq,mc);
exita:
    gpio_free(MAX8903_GPIO_CHG_FLT);
exit9:
    gpio_free(MAX8903_GPIO_CHG_ILM);
exit8:
    gpio_free(MAX8903_GPIO_CHG_EN);
exit7:
    gpio_free(MAX8903_GPIO_CHG_USUS);
exit6:
    gpio_free(MAX8903_GPIO_CHG_IUSB);
exit5:
    gpio_free(MAX8903_GPIO_CHG_STATUS);
exit4:
    gpio_free(MAX8903_UOK_GPIO_FOR_IRQ);
exit3:
    gpio_free(MAX8903_DOK_GPIO_FOR_IRQ);
exit2:
    power_supply_unregister(&mc->usb);
exit1:
    power_supply_unregister(&mc->adapter);
exit0:
    kfree(mc);
    return ret;
}

/*Charger Module DeInitialization*/
static int __exit max8903_charger_remove(struct platform_device *pdev)
{
    struct max8903_charger *mc = platform_get_drvdata(pdev);
    /*unregister,clean up*/
    if(mc)
    {
        power_supply_unregister(&mc->usb);
        power_supply_unregister(&mc->adapter);
    }
    gpio_free(MAX8903_UOK_GPIO_FOR_IRQ);
    gpio_free(MAX8903_DOK_GPIO_FOR_IRQ);
    gpio_free(MAX8903_GPIO_CHG_IUSB);
    gpio_free(MAX8903_GPIO_CHG_USUS);
    gpio_free(MAX8903_GPIO_CHG_FLT);
    gpio_free(MAX8903_GPIO_CHG_EN);
    gpio_free(MAX8903_GPIO_CHG_STATUS);
    if(mc)
        kfree(mc);
    return 0;
}

static struct platform_driver max8903_charger_driver = {
    .driver = {
        .name = "max8903_charger",
    },
    .probe = max8903_charger_probe,
    .remove =  __exit_p(max8903_charger_remove),
};

static int __init max8903_charger_init(void)
{
    printk("MAXIM 8903 Charger registering!\n");
    return platform_driver_register(&max8903_charger_driver);
}

static void __exit max8903_charger_exit(void)
{
    platform_driver_unregister(&max8903_charger_driver);
}

module_init(max8903_charger_init);
module_exit(max8903_charger_exit);

MODULE_AUTHOR("Lei Cao <lcao@book.com>");
MODULE_DESCRIPTION("MAXIM 8903 PMIC driver");
MODULE_LICENSE("GPL");
