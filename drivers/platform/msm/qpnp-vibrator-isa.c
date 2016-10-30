/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include "../../staging/android/timed_output.h"
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#define QPNP_VIB_VTG_CTL(base)		(base + 0x41)
#define QPNP_VIB_EN_CTL(base)		(base + 0x46)

#define QPNP_VIB_MAX_LEVEL		31
#define QPNP_VIB_MIN_LEVEL		12

#define QPNP_VIB_DEFAULT_TIMEOUT	15000
#define QPNP_VIB_DEFAULT_VTG_LVL	3100

#define QPNP_VIB_EN			BIT(7)
#define QPNP_VIB_VTG_SET_MASK		0x1F
#define QPNP_VIB_LOGIC_SHIFT		4

enum qpnp_vib_mode {
	QPNP_VIB_MANUAL,
	QPNP_VIB_DTEST1,
	QPNP_VIB_DTEST2,
	QPNP_VIB_DTEST3,
};

/**
 *  pwm_config_data - pwm configuration data
 *  @lut_params - lut parameters to be used by pwm driver
 *  @pwm_device - pwm device
 *  @pwm_period_us - period for pwm, in us
 *  @mode - mode the led operates in
 *  @old_duty_pcts - storage for duty pcts that may need to be reused
 *  @default_mode - default mode of LED as set in device tree
 *  @use_blink - use blink sysfs entry
 *  @blinking - device is currently blinking w/LPG mode
 */
struct pwm_config_data {
	struct lut_params	lut_params;
	struct pwm_device	*pwm_dev;
	u32			pwm_period_us;
	struct pwm_duty_cycles	*duty_cycles;
	int	*old_duty_pcts;
	u8	mode;
	u8	default_mode;
	bool	pwm_enabled;
	bool use_blink;
	bool blinking;
};

/**
 *  mpp_config_data - mpp configuration data
 *  @pwm_cfg - device pwm configuration
 *  @current_setting - current setting, 5ma-40ma in 5ma increments
 *  @source_sel - source selection
 *  @mode_ctrl - mode control
 *  @vin_ctrl - input control
 *  @min_brightness - minimum brightness supported
 *  @pwm_mode - pwm mode in use
 *  @max_uV - maximum regulator voltage
 *  @min_uV - minimum regulator voltage
 *  @mpp_reg - regulator to power mpp based LED
 *  @enable - flag indicating LED on or off
 */
struct mpp_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	current_setting;
	u8	source_sel;
	u8	mode_ctrl;
	u8	vin_ctrl;
	u8	min_brightness;
	u8 pwm_mode;
	u32	max_uV;
	u32	min_uV;
	struct regulator *mpp_reg;
	bool	enable;
};

struct qpnp_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

struct qpnp_vib {
	struct spmi_device *spmi;
	struct mpp_config_data	*mpp_cfg;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct qpnp_pwm_info pwm_info;
	enum   qpnp_vib_mode mode;

	u8  reg_vtg_ctl;
	u8  reg_en_ctl;
	u8  active_low;
	u16 base;
	int state;
	int vtg_level;
	int timeout;
	unsigned en_gpio;
	u32 en_gpio_flags;
	struct mutex lock;
	int level;
	struct work_struct isa1000_vib_work;
};

//static u32 p_duty_us = 40000;     //duty default 25000 
//static u32 p_period_us = 50000;//PWM freq:20kHz 
struct qpnp_vib *isa1000_vibr = NULL;
static int vib_level = 0;

static void qpnp_vib_enable(struct timed_output_dev *dev, int value);
void isa1000_vib_set_level(int level);
static int qpnp_vib_read_u8(struct qpnp_vib *vib, u8 *data, u16 reg)
{
	int rc;

	rc = spmi_ext_register_readl(vib->spmi->ctrl, vib->spmi->sid,
							reg, data, 1);
	if (rc < 0)
		dev_err(&vib->spmi->dev,
			"Error reading address: %X - ret %X\n", reg, rc);

	return rc;
}

static int qpnp_vib_write_u8(struct qpnp_vib *vib, u8 *data, u16 reg)
{
	int rc;

	rc = spmi_ext_register_writel(vib->spmi->ctrl, vib->spmi->sid,
							reg, data, 1);
	if (rc < 0)
		dev_err(&vib->spmi->dev,
			"Error writing address: %X - ret %X\n", reg, rc);

	return rc;
}

static int qpnp_vibrator_config(struct qpnp_vib *vib)
{
	u8 reg = 0;
	int rc;

	/* Configure the VTG CTL regiser */
	rc = qpnp_vib_read_u8(vib, &reg, QPNP_VIB_VTG_CTL(vib->base));
	if (rc < 0)
		return rc;
	reg &= ~QPNP_VIB_VTG_SET_MASK;
	reg |= (vib->vtg_level & QPNP_VIB_VTG_SET_MASK);
	rc = qpnp_vib_write_u8(vib, &reg, QPNP_VIB_VTG_CTL(vib->base));
	if (rc)
		return rc;
	vib->reg_vtg_ctl = reg;

	/* Configure the VIB ENABLE regiser */
	rc = qpnp_vib_read_u8(vib, &reg, QPNP_VIB_EN_CTL(vib->base));
	if (rc < 0)
		return rc;
	reg |= (!!vib->active_low) << QPNP_VIB_LOGIC_SHIFT;
	if (vib->mode != QPNP_VIB_MANUAL) {

		vib->pwm_info.pwm_dev = pwm_request(vib->pwm_info.pwm_channel,
								 "qpnp-vib");
		//dev_err(&vib->spmi->dev, "ahe channel %d \n",vib->pwm_info.pwm_channel);
		if (IS_ERR_OR_NULL(vib->pwm_info.pwm_dev)) {
			dev_err(&vib->spmi->dev, "vib pwm request failed\n");
			return -ENODEV;
		}

		rc = pwm_config(vib->pwm_info.pwm_dev, vib->pwm_info.duty_us,
						vib->pwm_info.period_us);
		if (rc < 0) {
			dev_err(&vib->spmi->dev, "vib pwm config failed\n");
			pwm_free(vib->pwm_info.pwm_dev);
			return -ENODEV;
		}

		reg |= BIT(vib->mode - 1);

	}

	rc = qpnp_vib_write_u8(vib, &reg, QPNP_VIB_EN_CTL(vib->base));
	if (rc < 0)
		return rc;
	vib->reg_en_ctl = reg;

	return rc;
}

static int qpnp_vib_isa_gpio_configure(struct qpnp_vib *vib,bool on)
{
	int retval = 0;

	if (on) {
		if (gpio_is_valid(vib->en_gpio)) {
			/* configure touchscreen irq gpio */
			retval = gpio_request(vib->en_gpio,
				"vib_isa_gpio");
			if (retval) {
				dev_err(&vib->spmi->dev,
					"unable to request gpio [%d]\n",
					vib->en_gpio);
				goto err_en_gpio_req;
			}
			retval = gpio_direction_input(vib->en_gpio);
			if (retval) {
				dev_err(&vib->spmi->dev,
					"unable to set direction for gpio " \
					"[%d]\n", vib->en_gpio);
				goto err_en_gpio_dir;
			}
		} else {
			dev_err(&vib->spmi->dev,
				"irq gpio not provided\n");
			goto err_en_gpio_req;
		}

		return 0;
	} else {
		if (gpio_is_valid(vib->en_gpio))
			gpio_free(vib->en_gpio);

		return 0;
	}

err_en_gpio_dir:
	if (gpio_is_valid(vib->en_gpio))
		gpio_free(vib->en_gpio);
err_en_gpio_req:
	return retval;
}

static int qpnp_vib_isa_set_enable(struct qpnp_vib *vib,bool on)
{
	int retval = 0;

	if (on) {
		retval = gpio_direction_output(vib->en_gpio,1);
		if (retval) {
			dev_err(&vib->spmi->dev,
				"unable to set direction for gpio " \
				"[%d]\n", vib->en_gpio);
			goto err_en_gpio_dir;
		}
		gpio_set_value(vib->en_gpio, 1);
		return 0;
	} else {
		retval = gpio_direction_output(vib->en_gpio,0);
		if (retval) {
			dev_err(&vib->spmi->dev,
				"unable to set direction for gpio " \
				"[%d]\n", vib->en_gpio);
			goto err_en_gpio_dir;
		}

		return 0;
	}

err_en_gpio_dir:
	if (gpio_is_valid(vib->en_gpio))
		gpio_free(vib->en_gpio);
	
	return retval;
}


void isa1000_vib_set_level(int level)
{
        int rc = 0;
	// dev_err(&isa1000_vibr->spmi->dev,"isa1000_vib_set_level:: (%d , %d) \n", level, vib_level);
        if  (level != 0) {
			if(vib_level != level) {
				//pwm_disable(isa1000_vibr->pwm_info.pwm_dev);
				/*
				rc = pwm_config(vib->pwm_info.pwm_dev, p_duty_us,
						p_period_us);
				*/
				  /* Set PWM duty cycle corresponding to the input 'level' */        
               			 /* Customer TODO: This is only an example. 
                			*   Please modify for PWM on Hong Mi 2A platform
                			*/
				if((vib_level + level) == 0) {
					rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
                                			(isa1000_vibr->pwm_info.period_us * (level + 130)) / 256,
                                			isa1000_vibr->pwm_info.period_us);
				} else {
            				rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
                                			(isa1000_vibr->pwm_info.period_us * (level + 128)) / 256,
                                			isa1000_vibr->pwm_info.period_us);
				}
				if (rc < 0) {
					dev_err(&isa1000_vibr->spmi->dev, "ahe vib pwm config failed\n");
					return ;
				}
				pwm_enable(isa1000_vibr->pwm_info.pwm_dev);
				
				rc = qpnp_vib_isa_set_enable(isa1000_vibr,true);
				if (rc) {
					dev_err(&isa1000_vibr->spmi->dev, "ahe vib set isa enable  failed\n");
					return ;
				}
				//dev_err(&isa1000_vibr->spmi->dev,"isa1000_vib ON :: (%d , %d) \n", level, ( ((level + 128)*100) /256));
               			pr_debug("isa1000_vib ON :: (%d , %d) \n", level,  ((level + 128)*100) /256);
				vib_level = level;
			}
		   }else {
		   	rc = qpnp_vib_isa_set_enable(isa1000_vibr,false);
			if (rc) {
				dev_err(&isa1000_vibr->spmi->dev, "ahe vib set isa enable  failed\n");
				return ;
			}
			 rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
                                		(isa1000_vibr->pwm_info.period_us * ( 128)) / 256,
                                		isa1000_vibr->pwm_info.period_us);
			if (rc < 0) {
					dev_err(&isa1000_vibr->spmi->dev, "ahe vib pwm config failed\n");
					return ;
			}
			pwm_disable(isa1000_vibr->pwm_info.pwm_dev);		
			vib_level = 0;
			//dev_err(&isa1000_vibr->spmi->dev,"isa1000_vib OFF  \n");
			pr_debug("isa1000_vib OFF  \n");
      		  }
	return;
}
EXPORT_SYMBOL(isa1000_vib_set_level);

static void isa1000_vib_ctrl(struct work_struct *work)
{
	isa1000_vib_set_level(isa1000_vibr->level);
}
static int qpnp_vib_set(struct qpnp_vib *vib, int on)
{
	int rc;
	u8 val;

	if (on) {
		if (vib->mode != QPNP_VIB_MANUAL){

			//pwm_disable(vib->pwm_info.pwm_dev);
			rc = pwm_config(vib->pwm_info.pwm_dev, vib->pwm_info.duty_us,
						vib->pwm_info.period_us);
			if (rc < 0) {
				dev_err(&vib->spmi->dev, "ahe vib pwm config failed\n");
				return -ENODEV;
			}
			pwm_enable(vib->pwm_info.pwm_dev);
			rc = qpnp_vib_isa_set_enable(vib,true);
			if (rc) {
				dev_err(&vib->spmi->dev, "ahe vib set isa enable  failed\n");
				return rc;
			}
			//dev_err(&vib->spmi->dev, "ahe  pwm value:  (%d ,%d)\n",,(p_duty_us*100/p_period_us));		
		}
		else {
			val = vib->reg_en_ctl;
			val |= QPNP_VIB_EN;
			rc = qpnp_vib_write_u8(vib, &val,
					QPNP_VIB_EN_CTL(vib->base));
			if (rc < 0)
				return rc;
			vib->reg_en_ctl = val;
		}
	} else {
		if (vib->mode != QPNP_VIB_MANUAL){
			rc = qpnp_vib_isa_set_enable(vib,false);
			if (rc) {
				dev_err(&vib->spmi->dev, "ahe vib set isa disable  failed\n");
				return rc;
			}
			//p_duty_us = p_period_us*4/5;//set by lixh10 
			
			 rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
                                		(isa1000_vibr->pwm_info.period_us * ( 128)) / 256,
                                		isa1000_vibr->pwm_info.period_us);
			if (rc < 0) {
				dev_err(&vib->spmi->dev, "ahe vib pwm config failed\n");
				return -ENODEV;
			}
			
			pwm_disable(vib->pwm_info.pwm_dev);	
		}
		else {
			val = vib->reg_en_ctl;
			val &= ~QPNP_VIB_EN;
			rc = qpnp_vib_write_u8(vib, &val,
					QPNP_VIB_EN_CTL(vib->base));
			if (rc < 0)
				return rc;
			vib->reg_en_ctl = val;
		}
	}

	return 0;
}

static ssize_t qpnp_vib_pwm_period_ns_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count;
	if(isa1000_vibr==NULL)
		return count;
	//struct qpnp_vib *vib = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",isa1000_vibr->pwm_info.period_us);//vib->pwm_info.period_us
}

static ssize_t qpnp_vib_pwm_period_ns_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error;
	if(isa1000_vibr==NULL)
		return count;
	//struct qpnp_vib *vib = dev_get_drvdata(dev);
	error = sscanf(buf, "%d", &val);
	if(error != 1)
		return error;

	if (val == 0 ){
		pr_err("Error setting pwm_period_ns : %d\n",val);
		return count;
		}
	else {
		isa1000_vibr->pwm_info.period_us= val;
		pr_err( "ahe set pwm_period_ns : %d\n",val);
	}
	return count;
}

static DEVICE_ATTR(pwm_period_ns, 0664,
							qpnp_vib_pwm_period_ns_show, 
							 qpnp_vib_pwm_period_ns_store);

static ssize_t qpnp_vib_pwm_duty_ns_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count;
	if(isa1000_vibr==NULL)
		return count;
	//struct qpnp_vib *vib = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",isa1000_vibr->pwm_info.duty_us);//vib->pwm_info.duty_us
}

static ssize_t qpnp_vib_pwm_duty_ns_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int error;
#if 0
	int i ,j;
	int dura[6] ={5,25,5,20,10,10};
	int level[6] = {127,100,-127,60,-100,-10};
	j=0;
#endif
	if(isa1000_vibr==NULL){
		printk( "ahe vibrator is not ready yet!!!!!\n");
		return count;
	}
	//struct qpnp_vib *vib = dev_get_drvdata(dev);
	error = sscanf(buf, "%d", &val);
	if(error != 1)
		return error;

	if ( val >127)
		val = 127;
	else if(val < -127 )
		val =-127 ;
	
#if 0
	printk( "ahe set pwm_duty_ns : %d\n",val);
	if(val==44){
		for(i=0;i<3;i++){
			//isa1000_vibr->level= level[j];
			//schedule_work(&isa1000_vibr->isa1000_vib_work);
			isa1000_vib_set_level(level[j]);
			mdelay(dura[j]);
			j++;
		}
	}else{
#endif
		isa1000_vibr->level= val;
		schedule_work(&isa1000_vibr->isa1000_vib_work);
#if 0
	}
#endif
	//isa1000_vibr->level= val;
	//p_duty_us= val;
	//schedule_work(&isa1000_vibr->isa1000_vib_work);
	return count;
}

static DEVICE_ATTR(pwm_duty_ns, 0664,
							qpnp_vib_pwm_duty_ns_show, 
							 qpnp_vib_pwm_duty_ns_store);

#if 0
static ssize_t qpnp_vib_enable_isa_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error;
	struct qpnp_vib *vib = dev_get_drvdata(dev);
	error = sscanf(buf, "%d", &val);
	if(error != 1)
		return error;

	if (val < 100){
		
		printk("Error setting vib duration : %d\n",val);
		return count;
		}
	else {
		qpnp_vib_enable(&vib->timed_dev, val);
		//printk( "ahe set pwm_duty_ns : %d\n",val);

	}
	return count;
}

static DEVICE_ATTR(enable_isa, 0664,
							NULL, 
							 qpnp_vib_enable_isa_store);

#endif
static struct attribute *qpnp_vib_isa_attrs[] = {
	&dev_attr_pwm_duty_ns.attr,
	&dev_attr_pwm_period_ns.attr,
	//&dev_attr_enable_isa.attr,
	NULL,
};

static const struct attribute_group qpnp_vib_group = {
	.attrs = qpnp_vib_isa_attrs,
};

static void qpnp_vib_enable(struct timed_output_dev *dev, int value)
{
	struct qpnp_vib *vib = container_of(dev, struct qpnp_vib,
					 timed_dev);

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0)
		vib->state = 0;
	else {
		value = (value > vib->timeout ?
				 vib->timeout : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	
	//dev_err(&vib->spmi->dev, "ahe vibrator default calling  $$$  %d\n",value);
	pr_debug("ahe vibrator default calling  $$$  %d\n",value);
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);
}


static void qpnp_vib_update(struct work_struct *work)
{
	struct qpnp_vib *vib = container_of(work, struct qpnp_vib,
					 work);
	qpnp_vib_set(vib, vib->state);
}

static int qpnp_vib_get_time(struct timed_output_dev *dev)
{
	struct qpnp_vib *vib = container_of(dev, struct qpnp_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart qpnp_vib_timer_func(struct hrtimer *timer)
{
	struct qpnp_vib *vib = container_of(timer, struct qpnp_vib,
							 vib_timer);

	vib->state = 0;
	pr_debug("ahe vibrator default calling  **** \n");
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int qpnp_vibrator_suspend(struct device *dev)
{
	struct qpnp_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	qpnp_vib_set(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(qpnp_vibrator_pm_ops, qpnp_vibrator_suspend, NULL);

static int qpnp_vib_parse_dt(struct qpnp_vib *vib)
{
	struct spmi_device *spmi = vib->spmi;
	int rc;
	const char *mode;
	u32 temp_val;

	vib->timeout = QPNP_VIB_DEFAULT_TIMEOUT;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,vib-timeout-ms", &temp_val);
	if (!rc) {
		vib->timeout = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vib timeout\n");
		return rc;
	}

	vib->vtg_level = QPNP_VIB_DEFAULT_VTG_LVL;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,vib-vtg-level-mV", &temp_val);
	if (!rc) {
		vib->vtg_level = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vtg level\n");
		return rc;
	}

	vib->vtg_level /= 100;
	if (vib->vtg_level < QPNP_VIB_MIN_LEVEL)
		vib->vtg_level = QPNP_VIB_MIN_LEVEL;
	else if (vib->vtg_level > QPNP_VIB_MAX_LEVEL)
		vib->vtg_level = QPNP_VIB_MAX_LEVEL;

	vib->mode = QPNP_VIB_MANUAL;
	rc = of_property_read_string(spmi->dev.of_node, "qcom,mode", &mode);
	if (!rc) {
		if (strcmp(mode, "manual") == 0)
			vib->mode = QPNP_VIB_MANUAL;
		else if (strcmp(mode, "dtest1") == 0)
			vib->mode = QPNP_VIB_DTEST1;
		else if (strcmp(mode, "dtest2") == 0)
			vib->mode = QPNP_VIB_DTEST2;
		else if (strcmp(mode, "dtest3") == 0)
			vib->mode = QPNP_VIB_DTEST3;
		else {
			dev_err(&spmi->dev, "Invalid mode\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read mode\n");
		return rc;
	}

	if (vib->mode != QPNP_VIB_MANUAL) {
		/*
		temp_val = of_get_named_gpio(spmi->dev.of_node,
				"qcom,pwm-channel", 0);
		vib->pwm_info.pwm_channel = temp_val;
		*/
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,pwm-channel", &temp_val);
		if (!rc)
			vib->pwm_info.pwm_channel = temp_val;
		else
			return rc;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,period-us", &temp_val);
		if (!rc)
			vib->pwm_info.period_us = temp_val;
		else
			return rc;

		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,duty-us", &temp_val);
		if (!rc)
			vib->pwm_info.duty_us = temp_val;
		else
			return rc;
	}

	vib->active_low = of_property_read_bool(spmi->dev.of_node,
				"qcom,active-low");
	//add by lixh10
	vib->en_gpio = of_get_named_gpio_flags(spmi->dev.of_node, "qcom,en-gpio",
				0, &vib->en_gpio_flags);
	if (vib->en_gpio < 0)
		return vib->en_gpio;
	dev_err(&spmi->dev, "ahe en_gpio :%d\n",vib->en_gpio);

	return 0;
}

static int qpnp_vibrator_probe(struct spmi_device *spmi)
{
	struct qpnp_vib *vib;
	struct resource *vib_resource;
	int rc;
	
	vib = devm_kzalloc(&spmi->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->spmi = spmi;

	vib_resource = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!vib_resource) {
		dev_err(&spmi->dev, "Unable to get vibrator base address\n");
		return -EINVAL;
	}
	vib->base = vib_resource->start;

	rc = qpnp_vib_parse_dt(vib);
	if (rc) {
		dev_err(&spmi->dev, "DT parsing failed\n");
		return rc;
	}
	rc = qpnp_vibrator_config(vib);
	if (rc) {
		dev_err(&spmi->dev, "vib config failed\n");
		return rc;
	}
	rc = qpnp_vib_isa_gpio_configure(vib,true);
	if (rc) {
		dev_err(&spmi->dev, "vib isa gpio config failed\n");
		return rc;
	}

	mutex_init(&vib->lock);
	INIT_WORK(&vib->work, qpnp_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = qpnp_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = qpnp_vib_get_time;
	vib->timed_dev.enable = qpnp_vib_enable;
	dev_set_drvdata(&spmi->dev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		return rc;

	if (sysfs_create_group(&vib->timed_dev.dev->kobj, &qpnp_vib_group)) {
		dev_err(&vib->spmi->dev, "ahe failed to create sysfs group\n");
		return -EAGAIN;
	}
	/*
	if (sysfs_create_group(&vib->spmi->dev.kobj, &qpnp_vib_group)) {
		dev_err(&vib->spmi->dev, "ahe failed to create sysfs group\n");
		return -EAGAIN;
	}
	*/
	INIT_WORK(&vib->isa1000_vib_work, isa1000_vib_ctrl);
	isa1000_vibr = vib;
	return rc;
}

static int qpnp_vibrator_remove(struct spmi_device *spmi)
{
	struct qpnp_vib *vib = dev_get_drvdata(&spmi->dev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	mutex_destroy(&vib->lock);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-vibrator",
	},
	{}
};

static struct spmi_driver qpnp_vibrator_driver = {
	.driver		= {
		.name	= "qcom,qpnp-vibrator",
		.of_match_table = spmi_match_table,
		.pm	= &qpnp_vibrator_pm_ops,
	},
	.probe		= qpnp_vibrator_probe,
	.remove		= qpnp_vibrator_remove,
};

static int __init qpnp_vibrator_init(void)
{
	return spmi_driver_register(&qpnp_vibrator_driver);
}
module_init(qpnp_vibrator_init);

static void __exit qpnp_vibrator_exit(void)
{
	return spmi_driver_unregister(&qpnp_vibrator_driver);
}
module_exit(qpnp_vibrator_exit);

MODULE_DESCRIPTION("qpnp vibrator driver");
MODULE_LICENSE("GPL v2");
