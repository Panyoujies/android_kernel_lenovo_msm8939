/*
 * max77819.c
 *
 * Copyright 2013 Maxim Integrated Products, Inc.
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/mfd/max77819.h>


#define MAX_BRIGHTNESS     0xEF

/* registers */
#define MAX77819_WLED_INT		0x9B
#define MAX77819_WLED_INT_MASK	0x9C
#define MAX77819_WLEDBSTCNTL	0x98
#define MAX77819_IWLED			0x99

/* MAX77819_WLED_INT */
#define MAX77819_WLEDOL			0x10
#define MAX77819_WLEDOVP_I		0x80

/* MAX77819_WLED_INT_MASK */
#define MAX77819_WLEDOL_M		0x10
#define MAX77819_WLEDOVP_M		0x80

/* MAX77819_WLEDBSTCNTL */
#define MAX77819_WLEDOVP		0x02
#define MAX77819_WLEDFOSC		0x0C
#define MAX77819_WLEDPWM2EN		0x10
#define MAX77819_WLEDPWM1EN		0x20
#define MAX77819_WLED2EN		0x40
#define MAX77819_WLED1EN		0x80

/* MAX77819_IWLED */
#define MAX77819_CUR			0xFF

struct max77819_wled {
    struct max77819_dev                    *chip;
    struct max77819_io                     *io;
    struct device                          *dev;
    struct backlight_device                *bl_dev;
    int                                     brightness;
};
static struct max77819_wled *pmax77819_wled;
static int max77819_bl_get_brightness(struct backlight_device *bl_dev);
extern int LCD_ID;
static int max77819_bl_off(struct backlight_device *bl_dev)
{
        struct max77819_wled *me = bl_get_data(bl_dev);
	struct regmap *regmap = me->io->regmap;
	int ret;
	/*int curr_val = 0;*/
	/*get current backlight current setting*/
	/*ret = regmap_read(regmap, MAX77819_IWLED, &curr_val);*/
	/*do{*/
		/*curr_val = curr_val - 21;*/
		/*if(curr_val<=0)*/
			/*curr_val = 0;*/
		 /*ret |= regmap_write(regmap, MAX77819_IWLED, curr_val);*/
		/*udelay(200);*/
	/*}while(curr_val>0);*/
    ret = regmap_write(regmap, MAX77819_IWLED, 0);//set current to 0
	ret |= regmap_update_bits(regmap, MAX77819_WLEDBSTCNTL,
			MAX77819_WLED1EN | MAX77819_WLED2EN | MAX77819_WLEDPWM1EN | MAX77819_WLEDPWM2EN, 0); //disable dual wled
	if (IS_ERR_VALUE(ret))
		dev_err(&bl_dev->dev, "failed to power off backlight : %d\n", ret);
	return ret;
}

static int initbacklight = 0;

static int max77819_bl_update_status(struct backlight_device *bl_dev)
{
	struct max77819_wled *me = NULL;
	struct regmap *regmap = NULL;
	int brightness = bl_dev->props.brightness;
	int ret = 0;
    unsigned int value = 0;

        if(NULL == bl_get_data(bl_dev))
        {
		printk("bl_get_data(bl_dev) is NULL\n");
		return -1;
        }
        me = bl_get_data(bl_dev);

        if(NULL == me->io->regmap)
        {
		printk("me->io->regmap is NULL\n");
		return -1;
        }
        regmap = me->io->regmap;

	/*Clear out backlight interrupts*/
	regmap_read(regmap, MAX77819_WLED_INT, &value);
	
	if (brightness == 0)
		ret = max77819_bl_off(bl_dev);
	else
	{
	          if(initbacklight < 2 && brightness == MAX_BRIGHTNESS){
			if(LCD_ID == 1)
				brightness = 0xA3;
			else
				brightness = 0xB8;
			initbacklight ++;
	          }

		if (brightness > MAX_BRIGHTNESS)
 			brightness = MAX_BRIGHTNESS;
		
		ret = regmap_read(regmap, MAX77819_WLEDBSTCNTL, &value);
		if (IS_ERR_VALUE(ret)){
			dev_err(&bl_dev->dev, "can't read WLEDBSTCNTL : %d\n", ret);
			return ret;
		}
		
		if ((value&(MAX77819_WLED1EN|MAX77819_WLED2EN))!= 0xc0){
			ret = regmap_write(regmap, MAX77819_IWLED, 0);
			if (IS_ERR_VALUE(ret))
			{
				dev_err(&bl_dev->dev, "can't write IWLED : %d\n", ret);
				return ret;
			}
			
			ret = regmap_update_bits(regmap, MAX77819_WLEDBSTCNTL,
				MAX77819_WLEDOVP | MAX77819_WLEDPWM1EN | MAX77819_WLEDPWM2EN |MAX77819_WLED1EN |MAX77819_WLED2EN,
				MAX77819_WLEDOVP | MAX77819_WLEDPWM1EN | MAX77819_WLEDPWM2EN |MAX77819_WLED1EN |MAX77819_WLED2EN);
			if (IS_ERR_VALUE(ret))
				dev_err(&bl_dev->dev, "can't write WLEDBSTCNTL : %d\n", ret);
		}
		ret = regmap_write(regmap, MAX77819_IWLED, (unsigned int)brightness);
		if (IS_ERR_VALUE(ret)) {
			dev_err(&bl_dev->dev, "can't write IWLED : %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int max77819_bl_get_brightness(struct backlight_device *bl_dev)
{
    struct max77819_wled *me = bl_get_data(bl_dev);
	struct regmap *regmap = me->io->regmap;
	unsigned int value;
	int ret;
	
	ret = regmap_read(regmap, MAX77819_WLEDBSTCNTL, &value);
	printk("max77819_bl_get_brightness MAX77819_WLEDBSTCNTL = 0x%x\n",value);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(&bl_dev->dev, "can't read WLEDBSTCNTL : %d\n", ret);
		return ret;
	}
	//else if ((value & (MAX77819_WLED2EN | MAX77819_WLED1EN)) == 0)
	//	return 0;

	ret = regmap_read(regmap, MAX77819_IWLED, &value);
	printk("max77819_bl_get_brightness MAX77819_IWLED = 0x%x\n",value);

	if (IS_ERR_VALUE(ret))
	{
		dev_err(&bl_dev->dev, "can't read IWLED : %d\n", ret);
		return ret;
	}
	
	return value;
}

static const struct backlight_ops max77819_bl_ops = {
	.update_status  = max77819_bl_update_status,
	.get_brightness = max77819_bl_get_brightness,
};

static int max77819_bl_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct max77819_dev *chip = dev_get_drvdata(dev->parent);
    struct max77819_wled *me;
    struct backlight_properties bl_props;
    int rc;

	me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
	if (unlikely(!me)) {
		dev_err(dev, "out of memory (%luB requested)\n", sizeof(*me));
		return -ENOMEM;
	}
    pmax77819_wled = me;
	dev_set_drvdata(dev, me);

	me->io	 = max77819_get_io(chip);
	me->dev  = dev;

	/* brightness is initially MAX */
	me->brightness = MAX_BRIGHTNESS;

	memset(&bl_props, 0x00, sizeof(bl_props));
	bl_props.type			= BACKLIGHT_RAW;
	bl_props.max_brightness = MAX_BRIGHTNESS;
	bl_props.brightness = MAX_BRIGHTNESS;
 	me->bl_dev = backlight_device_register("max77819-bl", dev, me,
		&max77819_bl_ops, &bl_props);
	if (unlikely(IS_ERR(me->bl_dev))) {
		rc = PTR_ERR(me->bl_dev);
		me->bl_dev = NULL;
		goto abort;
	}
	
	return 0;

abort:
    devm_kfree(dev, me);	
	dev_set_drvdata(dev, NULL);
	return rc;
}

static int max77819_bl_remove(struct platform_device *pdev)
{
    struct max77819_wled *me = platform_get_drvdata(pdev);
	struct backlight_device *bl = me->bl_dev;

	max77819_bl_off(bl);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,11,0)
	backlight_device_unregister(bl);
#endif
	return 0;
}

#if 0
static void max77819_bl_shutdown(struct platform_device *pdev)
{
    struct max77819_wled *me = platform_get_drvdata(pdev);
	struct backlight_device *bl = me->bl_dev;

	max77819_bl_off(bl);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,11,0)
	backlight_device_unregister(bl);
#endif
}
#endif

#ifdef CONFIG_PM
static int max77819_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
        //struct max77819_wled *me = platform_get_drvdata(pdev);
        //struct backlight_device *bl = me->bl_dev;

        //return max77819_bl_off(bl);
        return 0;
}

static int max77819_bl_resume(struct platform_device *pdev)
{
    //struct max77819_wled *me = platform_get_drvdata(pdev);
	//struct backlight_device *bl = me->bl_dev;
	//backlight_update_status(bl);	
	return 0;
}
#endif

int tmp_brightness = 0;

void max77819_update_brightness(int brightness)
{
    if((NULL == pmax77819_wled||NULL == pmax77819_wled->bl_dev))
    {
        printk("pmax77819_wled is NULL\n");
        return; 
    }
    if(brightness == tmp_brightness)
		return;
    pmax77819_wled->bl_dev->props.brightness = brightness; 
    max77819_bl_update_status(pmax77819_wled->bl_dev);
    //if(brightness == 0){
    //     max77819_bl_get_brightness(pmax77819_wled->bl_dev);
    //	printk("max77819_update_brightness  after.\n");
   // }
    tmp_brightness = brightness;
    printk("susan ==jinjt==%s %d brightness=%d\n",__func__,__LINE__,brightness);
}
#ifdef CONFIG_OF
static struct of_device_id max77819_bl_of_ids[] = {
    { .compatible = "maxim,"MAX77819_WLED_NAME },
    { },
};
MODULE_DEVICE_TABLE(of, max77819_bl_of_ids);
#endif /* CONFIG_OF */

static struct platform_driver max77819_bl_driver = {
	.driver	= {
		.name = MAX77819_WLED_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
    		.of_match_table  = max77819_bl_of_ids,
#endif /* CONFIG_OF */			
	},
	.probe = max77819_bl_probe,
	.remove = max77819_bl_remove,
#ifdef CONFIG_PM
       .suspend = max77819_bl_suspend,
       .resume = max77819_bl_resume,
#endif
      // .shutdown = max77819_bl_shutdown,
};

static int __init max77819_bl_init(void)
{
	return platform_driver_register(&max77819_bl_driver);
}
module_init(max77819_bl_init);

static void __exit max77819_bl_exit(void)
{
	platform_driver_unregister(&max77819_bl_driver);
}
module_exit(max77819_bl_exit);

MODULE_ALIAS("platform:max77819-backlight");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77819 backlight");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
