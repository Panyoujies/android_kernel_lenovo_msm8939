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
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_led_flash.h"
#include "msm_camera_io_util.h"
#include "../msm_sensor.h"
#include "msm_led_flash.h"
#include "../cci/msm_cci.h"
#include <linux/debugfs.h>
/*+begin xujt1 add led flash driver 2014-06-20*/
#include <linux/proc_fs.h>
/*+end xujt1 add led flash driver 2014-06-20*/

#include <linux/sysfs.h>
#include <media/lenovo_camera_features.h>

#define FLASH_NAME "camera-led-flash"
#define CAM_FLASH_PINCTRL_STATE_SLEEP "cam_flash_suspend"
#define CAM_FLASH_PINCTRL_STATE_DEFAULT "cam_flash_default"
/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
//#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#define CDBG printk

/*+begin xujt1 add led flash driver 2014-06-20*/
static struct msm_led_flash_ctrl_t *fctrl = NULL;
int i2c_inited = 0;
/*+end xujt1 add led flash driver 2014-06-20*/

/*+Begin: chenglong1 add for  flash absence kernel crash issue on 2015-6-10*/
int led_init_success = 0;
/*+End.*/

/*+begin xujt1 add led flash driver 2014-08-21*/
#define FEATURE_ENABLE_TORCH_LIGHT

#ifdef FEATURE_ENABLE_TORCH_LIGHT
static int flash_ll_ref_cnt = 0;//low level access ref count
struct mutex flash_ll_lock;//low level access lock
int32_t msm_led_i2c_trigger_config(struct msm_led_flash_ctrl_t *fctrl,void *data);
static void msm_lenovo_torch_brightness_set(struct led_classdev *led_cdev,enum led_brightness value);
static enum led_brightness msm_lenovo_torch_brightness_get(struct led_classdev *led_cdev);

static int msm_lenovo_torch_enable(int level)
{
	struct msm_camera_led_cfg_t cfg;
	int rc = 0;

	if (level)
	{
		cfg.cfgtype = MSM_CAMERA_LED_INIT;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_INIT failed!!! rc=%d", __func__, rc);
			return -1;
		} else {
			mdelay(10);
			memset(&cfg, 0x0, sizeof(cfg));
			cfg.cfgtype = MSM_CAMERA_LED_LOW;
			rc = msm_led_i2c_trigger_config(fctrl, &cfg);
			if (rc) {
				printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
				return -2;
			}
		}
	}
	else
	{
		cfg.cfgtype = MSM_CAMERA_LED_OFF;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_OFF failed!!! rc=%d", __func__, rc);
		}

		cfg.cfgtype = MSM_CAMERA_LED_RELEASE;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
			return -1;
		}
	}

	return 0;
}

static struct led_classdev msm_lenovo_torch_led = {
	.name			= "torch-light",
	.brightness_set	= msm_lenovo_torch_brightness_set,
	.brightness_get     = msm_lenovo_torch_brightness_get,	
	.brightness		= LED_OFF,
};

static void msm_lenovo_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	printk("%s: brightness value: %d", __func__, value);
	if (value) {
		msm_lenovo_torch_enable(1);
	} else {
		msm_lenovo_torch_enable(0);
	}
	
	msm_lenovo_torch_led.brightness = value;
};

static enum led_brightness msm_lenovo_torch_brightness_get(struct led_classdev *led_cdev)
{
	return msm_lenovo_torch_led.brightness;
}

static int32_t msm_lenovo_torch_create_classdev(void)
{
	int rc;
	mutex_init(&flash_ll_lock);
	flash_ll_ref_cnt = 0;
	
	rc = led_classdev_register(NULL, &msm_lenovo_torch_led);
	if (!rc) {
		printk("%s [flash_led] create torch file node sucess!!!\n", __func__);
	} else {
		printk("%s [flash_led] create torch file node failed!!! rc=%d\n", __func__, rc);
	}

	return rc;
}
#endif
/*+end xujt1 add led flash driver 2014-08-21*/


/*+Begin: chenglong1 add for multi-led testing*/
#ifdef LENOVO_CAM_PROJECT_PASSION
#define TORCH_OFF 0
#define TORCH_ON 1
static struct led_classdev msm_lenovo_torch_testing_leds[MAX_LED_TRIGGERS];
static char lenovo_testing_torch_name[MAX_LED_TRIGGERS][16];

static int msm_lenovo_torch_state(void)
{
	int i;
	
	for (i=0; i<MAX_LED_TRIGGERS; i++) {
		if (msm_lenovo_torch_testing_leds[i].brightness) return TORCH_ON;
	}
	return TORCH_OFF;
}

static int msm_lenovo_torch_set_level(int level, int torch_idx)
{
	struct msm_camera_led_cfg_t cfg;
	int i ;
	int rc = 0;

	memset(&cfg, 0x0, sizeof(struct msm_camera_led_cfg_t));

	if (msm_lenovo_torch_state() == TORCH_ON)
	{
		cfg.cfgtype = MSM_CAMERA_LED_INIT;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_INIT failed!!! rc=%d", __func__, rc);
			return -1;
		} else {
			cfg.cfgtype = MSM_CAMERA_LED_SET_LEVEL_LOW;
			for (i=0; i<MAX_LED_TRIGGERS; i++) {
				cfg.torch_level[i] = msm_lenovo_torch_testing_leds[i].brightness;
			}
			cfg.torch_level[torch_idx] = level;
			rc = msm_led_i2c_trigger_config(fctrl, &cfg);
			if (rc) {
				printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
				return -2;
			}
		}
	}
	else
	{
		cfg.cfgtype = MSM_CAMERA_LED_SET_LEVEL_LOW;
		for (i=0; i<MAX_LED_TRIGGERS; i++) {
			cfg.torch_level[i] = 0;
		}
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_SET_LEVEL_LOW failed!!! rc=%d", __func__, rc);
			//return -2;
		}
		
		cfg.cfgtype = MSM_CAMERA_LED_OFF;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_OFF failed!!! rc=%d", __func__, rc);
		}

		cfg.cfgtype = MSM_CAMERA_LED_RELEASE;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
			return -1;
		}
	}

	return 0;
}

static void lenovo_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	int torch_idx = 0;

	sscanf(led_cdev->name, "lenovo_torch%d", &torch_idx);
	printk("%s: brightness value: %d, torch_idx: %d\n", __func__, value, torch_idx);

	msm_lenovo_torch_testing_leds[torch_idx].brightness = value;
	msm_lenovo_torch_set_level(value, torch_idx);
};

static enum led_brightness lenovo_torch_brightness_get(struct led_classdev *led_cdev)
{
	int torch_idx = 0;
	
	sscanf(led_cdev->name, "lenovo_torch%d", &torch_idx);
	printk("%s:  torch_idx: %d", __func__, torch_idx);
	
	return msm_lenovo_torch_testing_leds[torch_idx].brightness;
}

static int32_t lenovo_torch_testing_create_classdev(void)
{
	int rc;
	int i, flash_cnt;
	
	//mutex_init(&flash_ll_lock);
	//flash_ll_ref_cnt = 0;

	flash_cnt =  fctrl->torch_num_sources > MAX_LED_TRIGGERS ? MAX_LED_TRIGGERS : fctrl->torch_num_sources ;
	printk("%s: flash_cnt: %d", __func__, flash_cnt);
	for (i=0; i<flash_cnt; i++) {
		sprintf(lenovo_testing_torch_name[i], "lenovo_torch%d", i);
		msm_lenovo_torch_testing_leds[i].name = lenovo_testing_torch_name[i];
		msm_lenovo_torch_testing_leds[i].brightness_set = lenovo_torch_brightness_set;
		msm_lenovo_torch_testing_leds[i].brightness_get = lenovo_torch_brightness_get;
		msm_lenovo_torch_testing_leds[i].brightness = LED_OFF;
		
		rc = led_classdev_register(NULL, &msm_lenovo_torch_testing_leds[i]);
		if (!rc) {
			printk("%s [flash_led] create torch%d node sucess!!!\n", __func__, i);
		} else {
			printk("%s [flash_led] create torch%d node failed!!! rc=%d\n", __func__, i, rc);
		}
	}

	return rc;
}

#if 1
#define FLASH_OFF 0
#define FLASH_ON 1

static int flash_brightness[MAX_LED_TRIGGERS];

static int msm_lenovo_flash_state(void)
{
	int i;
	
	for (i=0; i<MAX_LED_TRIGGERS; i++) {
		if (flash_brightness[i]) return FLASH_ON;
	}
	return FLASH_OFF;
}

static int msm_lenovo_flash_set_level(int level, int torch_idx)
{
	struct msm_camera_led_cfg_t cfg;
	int i ;
	int rc = 0;

	memset(&cfg, 0x0, sizeof(struct msm_camera_led_cfg_t));

	if (msm_lenovo_flash_state() == FLASH_ON)
	{
		cfg.cfgtype = MSM_CAMERA_LED_INIT;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_INIT failed!!! rc=%d", __func__, rc);
			return -1;
		} else {
			cfg.cfgtype = MSM_CAMERA_LED_SET_LEVEL_HIGH;
			for (i=0; i<MAX_LED_TRIGGERS; i++) {
				cfg.flash_level[i] = flash_brightness[i];
			}
			cfg.flash_level[torch_idx] = level;
			rc = msm_led_i2c_trigger_config(fctrl, &cfg);
			if (rc) {
				printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
				return -2;
			}
		}
	}
	else
	{
		cfg.cfgtype = MSM_CAMERA_LED_SET_LEVEL_HIGH;
		for (i=0; i<MAX_LED_TRIGGERS; i++) {
			cfg.flash_level[i] = 0;
		}
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_SET_LEVEL_LOW failed!!! rc=%d", __func__, rc);
			//return -2;
		}
		
		cfg.cfgtype = MSM_CAMERA_LED_OFF;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_OFF failed!!! rc=%d", __func__, rc);
		}

		cfg.cfgtype = MSM_CAMERA_LED_RELEASE;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
			return -1;
		}
	}

	return 0;
}
#endif

#if 1//for duration testing
static int msm_lenovo_flash_set_duration(int ms, int torch_idx)
{
	struct msm_camera_led_cfg_t cfg;
	int rc = 0;

	memset(&cfg, 0x0, sizeof(struct msm_camera_led_cfg_t));

	cfg.cfgtype = MSM_CAMERA_LED_INIT;
	rc = msm_led_i2c_trigger_config(fctrl, &cfg);
	if (rc) {
		printk("%s MSM_CAMERA_LED_INIT failed!!! rc=%d", __func__, rc);
		return -1;
	} else {
		cfg.cfgtype = MSM_CAMERA_LED_SET_DURATION;
		cfg.flash_duration[0] = ms;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
			return -2;
		}

		cfg.cfgtype = MSM_CAMERA_LED_RELEASE;
		rc = msm_led_i2c_trigger_config(fctrl, &cfg);
		if (rc) {
			printk("%s MSM_CAMERA_LED_LOW failed!!! rc=%d", __func__, rc);
			return -1;
		}
	}

	return 0;
}

#endif

static ssize_t torch_factory_testing_read(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "echo to control torch" );
}
static ssize_t torch_factory_testing_write(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	uint32_t torch0_level = 0;
	uint32_t torch1_level = 0;
	sscanf(buf, "led0: %d; led1: %d", &torch0_level, &torch1_level);
	
	CDBG("proc_flash_led_write called: torch0_level=%d, torch1_level=%d\n", torch0_level, torch1_level);
	msm_lenovo_torch_testing_leds[0].brightness = torch0_level;
	msm_lenovo_torch_testing_leds[1].brightness = torch1_level;
	msm_lenovo_torch_set_level(torch0_level, 0);
	msm_lenovo_torch_set_level(torch1_level, 1);
	
	return count;
}

struct device_attribute lenovo_torch_testing_node = __ATTR(torch_factory_testing, S_IRUGO | S_IWUSR | S_IWGRP, 
											torch_factory_testing_read,
											torch_factory_testing_write);

static ssize_t flash_factory_testing_read(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "echo to control flash" );
}
static ssize_t flash_factory_testing_write(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	uint32_t flash0_level = 0;
	uint32_t flash1_level = 0;
	sscanf(buf, "led0: %d; led1: %d", &flash0_level, &flash1_level);
	
	CDBG("proc_flash_led_write called: flash0_level=%d, torch1_level=%d\n", flash0_level, flash1_level);
	flash_brightness[0] = flash0_level;
	flash_brightness[1] = flash1_level;
	msm_lenovo_flash_set_level(flash0_level, 0);
	msm_lenovo_flash_set_level(flash1_level, 1);
	
	return count;
}

struct device_attribute lenovo_flash_testing_node = __ATTR(flash_factory_testing, S_IRUGO | S_IWUSR | S_IWGRP, 
											flash_factory_testing_read,
											flash_factory_testing_write);

static ssize_t flash_duration_testing_read(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "echo to control flash duration" );
}
static ssize_t flash_duration_testing_write(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	uint32_t flash_duration = 0;
	sscanf(buf, "%d", &flash_duration);
	
	CDBG("%s called: duration=%d\n", __func__, flash_duration);
	msm_lenovo_flash_set_duration(flash_duration, 0);
	
	return count;
}

struct device_attribute lenovo_flash_duration_testing_node = __ATTR(flash_duration_testing, S_IRUGO | S_IWUSR | S_IWGRP, 
											flash_duration_testing_read,
											flash_duration_testing_write);

struct attribute	*lenovo_flash_drv_attrs[] ={
	&lenovo_torch_testing_node.attr,
	&lenovo_flash_testing_node.attr,
	&lenovo_flash_duration_testing_node.attr,
	NULL,
};

struct attribute_group lenovo_flash_drv_node ={
	.name = "flash_drv",
	.attrs = lenovo_flash_drv_attrs,
};
#endif
/*+End.*/

int32_t msm_led_i2c_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	*subdev_id = fctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	return 0;
}

int32_t msm_led_i2c_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data)
{
	int rc = 0;
	int i = 0;
	struct msm_camera_led_cfg_t *cfg = (struct msm_camera_led_cfg_t *)data;
	CDBG("called led_state %d\n", cfg->cfgtype);

	if (!fctrl->func_tbl) {
		pr_err("failed\n");
		return -EINVAL;
	}

#ifdef FEATURE_ENABLE_TORCH_LIGHT
	mutex_lock(&flash_ll_lock);

	/*+Begin: chenglong1 added on 2015-6-20: "led_init_success" for  sometimes flash init failed 
	                      turn on torch/flash may cause crash.
	                      When the "led_init_success == 1", it means flash initialized successfully, then
	                      we can control the chip as torch or flash.*/
	if ((!flash_ll_ref_cnt || !led_init_success) && (cfg->cfgtype != MSM_CAMERA_LED_INIT)) {
		pr_err("Error status!!!\n");
		mutex_unlock(&flash_ll_lock);
		return -EIO;
	}
#endif

	switch (cfg->cfgtype) {

	case MSM_CAMERA_LED_INIT:
#ifndef FEATURE_ENABLE_TORCH_LIGHT
		if (fctrl->func_tbl->flash_led_init)
			rc = fctrl->func_tbl->flash_led_init(fctrl);
#else
		if ((fctrl->func_tbl->flash_led_init) && (!flash_ll_ref_cnt))
			rc = fctrl->func_tbl->flash_led_init(fctrl);
		flash_ll_ref_cnt ++;
#endif

		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			cfg->flash_current[i] =
				fctrl->flash_max_current[i];
			cfg->flash_duration[i] =
				fctrl->flash_max_duration[i];
			cfg->torch_current[i] =
				fctrl->torch_max_current[i];
		}
		break;

	case MSM_CAMERA_LED_RELEASE:
#ifndef FEATURE_ENABLE_TORCH_LIGHT
		if (fctrl->func_tbl->flash_led_release)
			rc = fctrl->func_tbl->
				flash_led_release(fctrl);
#else
		flash_ll_ref_cnt --;
		if ((fctrl->func_tbl->flash_led_release) && (!flash_ll_ref_cnt))
			rc = fctrl->func_tbl->flash_led_release(fctrl);
#endif
		break;

	case MSM_CAMERA_LED_OFF:
		if (fctrl->func_tbl->flash_led_off)
			rc = fctrl->func_tbl->flash_led_off(fctrl);
		break;

	case MSM_CAMERA_LED_LOW:
		for (i = 0; i < fctrl->torch_num_sources; i++) {
			if (fctrl->torch_max_current[i] > 0) {
			/*+Begin: chenglong1 for muti-level flash current 2015-4-5*/
				fctrl->torch_op_current[i] =
					(cfg->torch_current[i] < fctrl->torch_max_current[i]) ?
					cfg->torch_current[i] : fctrl->torch_max_current[i];

				CDBG("torch source%d: op_current %d max_current %d\n",
						i, fctrl->torch_op_current[i], fctrl->torch_max_current[i]);
				
				if (fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.torch_current_setting_func) {
					fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.torch_current_setting_func(fctrl->torch_op_current[i], i);
				}
			/*+End.*/
			}
		}
		if (fctrl->func_tbl->flash_led_low)
			rc = fctrl->func_tbl->flash_led_low(fctrl);
		break;

	case MSM_CAMERA_LED_HIGH:
		for (i = 0; i < fctrl->flash_num_sources; i++) {
			if (fctrl->flash_max_current[i] > 0) {
				fctrl->flash_op_current[i] =
					(cfg->flash_current[i] < fctrl->flash_max_current[i]) ?
					cfg->flash_current[i] : fctrl->flash_max_current[i];
				CDBG("flash source%d: op_current %d max_current %d\n",
					i, fctrl->flash_op_current[i], fctrl->flash_max_current[i]);
				
				/*+Begin: chenglong1 for muti-level flash current 2015-4-5*/
				if (fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_current_setting_func) {
					fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_current_setting_func(fctrl->flash_op_current[i], i);
				}
				/*+End.*/
			}
		}
		if (fctrl->func_tbl->flash_led_high)
			rc = fctrl->func_tbl->flash_led_high(fctrl);
		break;
	/*+Begin: chenglong1 add for passion flash factory test*/
#ifdef LENOVO_CAM_PROJECT_PASSION
	case MSM_CAMERA_LED_SET_LEVEL_LOW:
		for (i = 0; i < fctrl->torch_num_sources; i++) {
			if (fctrl->torch_max_current[i] > 0) {
				if (cfg->torch_current[i]) {
					fctrl->torch_op_current[i] =
						(cfg->torch_current[i] < fctrl->torch_max_current[i]) ?
						cfg->torch_current[i] : fctrl->torch_max_current[i];

					CDBG("torch source%d: op_current %d max_current %d\n",
							i, fctrl->torch_op_current[i], fctrl->torch_max_current[i]);
					CDBG("torch source%d: torch_level %d\n",
							i, cfg->torch_level[i]);
					
					if (fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.torch_level_setting_func) {
						fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.torch_level_setting_func(cfg->torch_level[i], i);
					}
				}
			}
		}
		if (fctrl->func_tbl->flash_led_low)
			rc = fctrl->func_tbl->flash_led_low(fctrl);
		break;
	case MSM_CAMERA_LED_SET_LEVEL_HIGH:
		for (i = 0; i < fctrl->flash_num_sources; i++) {
			if (fctrl->flash_max_current[i] > 0) {
				fctrl->flash_op_current[i] =
					(cfg->flash_current[i] < fctrl->flash_max_current[i]) ?
					cfg->flash_current[i] : fctrl->flash_max_current[i];
				CDBG("flash source%d: op_current %d max_current %d\n",
					i, fctrl->flash_op_current[i], fctrl->flash_max_current[i]);
				
				if (fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_level_setting_func) {
					fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_level_setting_func(cfg->flash_level[i], i);
				}
			}
		}
		if (fctrl->func_tbl->flash_led_high)
			rc = fctrl->func_tbl->flash_led_high(fctrl);
		break;

	case MSM_CAMERA_LED_SET_DURATION:
		if (fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_duration_set_func) {
			fctrl->reg_setting->p_flash_source_cur_setting_func_tbl.flash_duration_set_func(cfg->flash_duration[0], 0);
		}
		break;
#endif
	/*+End.*/
	default:
		rc = -EFAULT;
		break;
	}

#ifdef FEATURE_ENABLE_TORCH_LIGHT
	mutex_unlock(&flash_ll_lock);
#endif
	CDBG("flash_set_led_state: return %d\n", rc);
	return rc;
}
static int msm_flash_pinctrl_init(struct msm_led_flash_ctrl_t *ctrl)
{
	struct msm_pinctrl_info *flash_pctrl = NULL;

	flash_pctrl = &ctrl->pinctrl_info;
	flash_pctrl->pinctrl = devm_pinctrl_get(&ctrl->pdev->dev);

	if (IS_ERR_OR_NULL(flash_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_active = pinctrl_lookup_state(
					       flash_pctrl->pinctrl,
					       CAM_FLASH_PINCTRL_STATE_DEFAULT);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_suspend = pinctrl_lookup_state(
						flash_pctrl->pinctrl,
						CAM_FLASH_PINCTRL_STATE_SLEEP);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}


int msm_flash_led_init(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	fctrl->led_state = MSM_CAMERA_LED_RELEASE;
	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		pr_err("%s:%d mux install\n", __func__, __LINE__);
	}

	/* CCI Init */
	if (fctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl->flash_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("cci_init failed\n");
			return rc;
		}
	}
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		CDBG("%s:%d PC:: flash pins setting to active state",
				__func__, __LINE__);
		rc = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_active);
		if (rc < 0) {
			devm_pinctrl_put(fctrl->pinctrl_info.pinctrl);
			pr_err("%s:%d cannot set pin to active state",
					__func__, __LINE__);
		}
	}
	msleep(20);

	CDBG("before FL_RESET\n");
	if (power_info->gpio_conf->gpio_num_info->
			valid[SENSOR_GPIO_FL_RESET] == 1)
		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_RESET],
			GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
        i2c_inited = 1;

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->init_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	fctrl->led_state = MSM_CAMERA_LED_INIT;

/*+Begin: chenglong1 add for  flash absence kernel crash issue on 2015-6-10*/
	if (!rc) {
		led_init_success = 1;
	} else {
		led_init_success = 0;
	}
/*+End*/
	return rc;
}

int msm_flash_led_release(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0, ret = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	if (fctrl->led_state != MSM_CAMERA_LED_INIT) {
		pr_err("%s:%d invalid led state\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*+Begin: chenglong1 add for skip off, direct call release case. 2015-5-29*/
	/*+Begin: chenglong1 add "led_init_success" for  flash absence kernel crash issue on 2015-6-10*/
	if (fctrl->flash_i2c_client && fctrl->reg_setting &&led_init_success) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->release_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	/*+End.*/
	
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	if (power_info->gpio_conf->gpio_num_info->
			valid[SENSOR_GPIO_FL_RESET] == 1)
		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_RESET],
			GPIO_OUT_LOW);

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		ret = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_suspend);
		if (ret < 0) {
			devm_pinctrl_put(fctrl->pinctrl_info.pinctrl);
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
		}
	}
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	fctrl->led_state = MSM_CAMERA_LED_RELEASE;
	/* CCI deInit */
	if (fctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl->flash_i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_deinit failed\n");
	}
	
/*+Begin: chenglong1 add for  flash absence kernel crash issue on 2015-6-10*/
	led_init_success = 0;
/*+End*/

/*+begin xujt1 add led flash driver 2014-06-20*/
    i2c_inited = 0;
/*+end xujt1 add led flash driver 2014-06-20*/
	return 0;
}

int msm_flash_led_off(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->off_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	return rc;
}

int msm_flash_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);


	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->low_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

int msm_flash_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

static int32_t msm_led_get_dt_data(struct device_node *of_node,
		struct msm_led_flash_ctrl_t *fctrl)
{
	int32_t rc = 0, i = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct device_node *flash_src_node = NULL;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	uint32_t count = 0;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	uint32_t id_info[3];

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl->flashdata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!fctrl->flashdata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	rc = of_property_read_u32(of_node, "cell-index", &fctrl->subdev_id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}

	CDBG("subdev id %d\n", fctrl->subdev_id);

	rc = of_property_read_string(of_node, "label",
		&flashdata->sensor_name);
	CDBG("%s label %s, rc %d\n", __func__,
		flashdata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	fctrl->pinctrl_info.use_pinctrl = false;
	fctrl->pinctrl_info.use_pinctrl = of_property_read_bool(of_node,
						"qcom,enable_pinctrl");
	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("failed\n");
			return -EINVAL;
		}
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl->flash_trigger_name[i]);
			if (rc < 0) {
				pr_err("failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("default trigger %s\n",
				 fctrl->flash_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&fctrl->flash_op_current[i]);
			if (rc < 0) {
				pr_err("failed rc %d\n", rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->flash_op_current[i]);

			led_trigger_register_simple(
				fctrl->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}

	} else { /*Handle LED Flash Ctrl by GPIO*/
		power_info->gpio_conf =
			 kzalloc(sizeof(struct msm_camera_gpio_conf),
				 GFP_KERNEL);
		if (!power_info->gpio_conf) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			return rc;
		}
		gconf = power_info->gpio_conf;

		gpio_array_size = of_gpio_count(of_node);
		CDBG("%s gpio count %d\n", __func__, gpio_array_size);

		if (gpio_array_size) {
			gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
				GFP_KERNEL);
			if (!gpio_array) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				rc = -ENOMEM;
				goto ERROR4;
			}
			for (i = 0; i < gpio_array_size; i++) {
				gpio_array[i] = of_get_gpio(of_node, i);
				CDBG("%s gpio_array[%d] = %d\n", __func__, i,
					gpio_array[i]);
			}

			rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR4;
			}

			rc = msm_camera_get_dt_gpio_set_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR5;
			}

			rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR6;
			}
		}

		/* Read the max current for an LED if present */
		if (of_get_property(of_node, "qcom,max-current", &count)) {
			count /= sizeof(uint32_t);
			if (count > MAX_LED_TRIGGERS) {
				pr_err("failed\n");
				rc = -EINVAL;
				goto ERROR8;
			}

			fctrl->flash_num_sources = count;
			fctrl->torch_num_sources = count;

			rc = of_property_read_u32_array(of_node,
				"qcom,max-current",
				fctrl->flash_max_current, count);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR8;
			}

			for (; count < MAX_LED_TRIGGERS; count++)
				fctrl->flash_max_current[count] = 0;

			for (count = 0; count < MAX_LED_TRIGGERS; count++)
				fctrl->torch_max_current[count] =
					fctrl->flash_max_current[count] >> 1;
		}

		/* Read the max duration for an LED if present */
		if (of_get_property(of_node, "qcom,max-duration", &count)) {
			count /= sizeof(uint32_t);

			if (count > MAX_LED_TRIGGERS) {
				pr_err("failed\n");
				rc = -EINVAL;
				goto ERROR8;
			}

			rc = of_property_read_u32_array(of_node,
				"qcom,max-duration",
				fctrl->flash_max_duration, count);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR8;
			}

			for (; count < MAX_LED_TRIGGERS; count++)
				fctrl->flash_max_duration[count] = 0;
		}

		flashdata->slave_info =
			kzalloc(sizeof(struct msm_camera_slave_info),
				GFP_KERNEL);
		if (!flashdata->slave_info) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR8;
		}

		rc = of_property_read_u32_array(of_node, "qcom,slave-id",
			id_info, 3);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR9;
		}
		fctrl->flashdata->slave_info->sensor_slave_addr = id_info[0];
		fctrl->flashdata->slave_info->sensor_id_reg_addr = id_info[1];
		fctrl->flashdata->slave_info->sensor_id = id_info[2];

		kfree(gpio_array);
		return rc;
ERROR9:
		kfree(fctrl->flashdata->slave_info);
ERROR8:
		kfree(fctrl->flashdata->power_info.gpio_conf->gpio_num_info);
ERROR6:
		kfree(gconf->cam_gpio_set_tbl);
ERROR5:
		kfree(gconf->cam_gpio_req_tbl);
ERROR4:
		kfree(gconf);
ERROR1:
		kfree(fctrl->flashdata);
		kfree(gpio_array);
	}
	return rc;
}

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
};

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
};

#ifdef CONFIG_DEBUG_FS
static int set_led_status(void *data, u64 val)
{
	struct msm_led_flash_ctrl_t *fctrl =
		 (struct msm_led_flash_ctrl_t *)data;
	int rc = -1;
	pr_debug("set_led_status: Enter val: %llu", val);
	if (!fctrl) {
		pr_err("set_led_status: fctrl is NULL");
		return rc;
	}
	if (!fctrl->func_tbl) {
		pr_err("set_led_status: fctrl->func_tbl is NULL");
		return rc;
	}
	if (val == 0) {
		pr_debug("set_led_status: val is disable");
		rc = msm_flash_led_off(fctrl);
	} else {
		pr_debug("set_led_status: val is enable");
		rc = msm_flash_led_low(fctrl);
	}

	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(ledflashdbg_fops,
	NULL, set_led_status, "%llu\n");
#endif

#ifndef LENOVO_CAM_PROJECT_PASSION//chenglong1 passion due to sepolicy limitation, factory app can't access procfs, move it to sysfs
/*+begin xujt1 add led flash driver 2014-06-20*/
ssize_t  proc_flash_led_write (struct file *file, const char __user *buf, size_t nbytes, loff_t *ppos)
{
#ifdef LENOVO_CAM_PROJECT_PASSION//chenglong1 for passion flash factory test
{
	uint32_t torch0_level = 0;
	uint32_t torch1_level = 0;
	sscanf(buf, "led1: %d; led2: %d", &torch0_level, &torch1_level);
	
	CDBG("proc_flash_led_write called: torch0_level=%d, torch1_level=%d\n", torch0_level, torch1_level);
	msm_lenovo_torch_testing_leds[0].brightness = torch0_level;
	msm_lenovo_torch_testing_leds[1].brightness = torch1_level;
	msm_lenovo_torch_set_level(torch0_level, 0);
	msm_lenovo_torch_set_level(torch1_level, 1);
}  
#else
    char string[nbytes];
    int rc = 0;
    CDBG("proc_flash_led_write called\n");
	
    sscanf(buf, "%s", string);
#ifdef FEATURE_ENABLE_TORCH_LIGHT
	if (!strcmp((const char *)string, (const char *)"on")) {
		msm_lenovo_torch_enable(1);
	} else {
		msm_lenovo_torch_enable(0);
	}
	rc = 0;
#else
    if (!strcmp((const char *)string, (const char *)"on"))
    {
    		CDBG("ljk MSM_CAMERA_LED_LOW called\n");
		if (fctrl->func_tbl->flash_led_init)
		{
		    rc = fctrl->func_tbl->flash_led_init(fctrl);
    		if (rc < 0)
    			pr_err("flash_led_init failed\n");
            else
            {
                if ((fctrl->func_tbl->flash_led_init)&&(i2c_inited==1))
    			    rc = fctrl->func_tbl->flash_led_low(fctrl);
    		}
    	}
    	else
    		pr_err("no flash_led_init func on\n");

    }
    else if (!strcmp((const char *)string, (const char *)"off"))
    {
        	CDBG("MSM_CAMERA_LED_OFF called\n");
		if ((fctrl->func_tbl->flash_led_off)&&(i2c_inited==1))
   		{
		    rc = fctrl->func_tbl->flash_led_off(fctrl);
    		if (rc < 0)
    			pr_err("flash_led_init failed\n");
            else
            {
        		if (fctrl->func_tbl->flash_led_release)
        			rc = fctrl->func_tbl->flash_led_release(fctrl);
    	     }
    	}
    	else
    		pr_err("no flash_led_release func off\n");

    }
#endif
#endif
    return nbytes;
}

const struct file_operations proc_flash_led_operations = {
	.owner	= THIS_MODULE,
	.write	= proc_flash_led_write,
};
/*+end xujt1 add led flash driver 2014-06-20*/
#endif

int msm_flash_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_led_flash_ctrl_t *fctrl = NULL;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	fctrl = (struct msm_led_flash_ctrl_t *)(id->driver_data);
	if (fctrl->flash_i2c_client)
		fctrl->flash_i2c_client->client = client;
	/* Set device type as I2C */
	fctrl->flash_device_type = MSM_CAMERA_I2C_DEVICE;

	/* Assign name for sub device */
	snprintf(fctrl->msm_sd.sd.name, sizeof(fctrl->msm_sd.sd.name),
		"%s", id->name);

	rc = msm_led_get_dt_data(client->dev.of_node, fctrl);
	if (rc < 0) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true)
		msm_flash_pinctrl_init(fctrl);

	if (fctrl->flash_i2c_client != NULL) {
		fctrl->flash_i2c_client->client = client;
		if (fctrl->flashdata->slave_info->sensor_slave_addr)
			fctrl->flash_i2c_client->client->addr =
				fctrl->flashdata->slave_info->
				sensor_slave_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!fctrl->flash_i2c_client->i2c_func_tbl)
		fctrl->flash_i2c_client->i2c_func_tbl =
			&msm_sensor_qup_func_tbl;

	rc = msm_led_i2c_flash_create_v4lsubdev(fctrl);
#ifdef CONFIG_DEBUG_FS
	dentry = debugfs_create_file("ledflash", S_IRUGO, NULL, (void *)fctrl,
		&ledflashdbg_fops);
	if (!dentry)
		pr_err("Failed to create the debugfs ledflash file");
#endif
	CDBG("%s:%d probe success\n", __func__, __LINE__);
	return 0;

probe_failure:
	CDBG("%s:%d probe failed\n", __func__, __LINE__);
	return rc;
}

int msm_flash_probe(struct platform_device *pdev,
	const void *data)
{
	int rc = 0;
/*+begin xujt1 add led flash driver 2014-06-20*/
//	struct msm_led_flash_ctrl_t *fctrl =
//		(struct msm_led_flash_ctrl_t *)data;
/*+end xujt1 add led flash driver 2014-06-20*/
	struct device_node *of_node = pdev->dev.of_node;
	struct msm_camera_cci_client *cci_client = NULL;

#ifndef LENOVO_CAM_PROJECT_PASSION//chenglong1 passion due to sepolicy limitation, factory app can't access procfs, move it to sysfs
/*+begin xujt1 add led flash driver 2014-06-20*/
	struct proc_dir_entry * rcdir;
	fctrl =(struct msm_led_flash_ctrl_t *)data;
	rcdir = proc_create_data("CTP_FLASH_CTRL", S_IFREG | S_IWUGO | S_IWUSR, NULL, &proc_flash_led_operations, NULL);
	if(rcdir == NULL)
        {
		CDBG("proc_create_data CTP_FLASH_CTRL fail\n");
	}
/*+end xujt1 add led flash driver 2014-06-20*/
#else
        fctrl =(struct msm_led_flash_ctrl_t *)data;
#endif
	/*+End.*/
	
	if (!of_node) {
		pr_err("of_node NULL\n");
		goto probe_failure;
	}
	fctrl->pdev = pdev;

	rc = msm_led_get_dt_data(pdev->dev.of_node, fctrl);
	if (rc < 0) {
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true)
		msm_flash_pinctrl_init(fctrl);

	/* Assign name for sub device */
	snprintf(fctrl->msm_sd.sd.name, sizeof(fctrl->msm_sd.sd.name),
			"%s", fctrl->flashdata->sensor_name);
	/* Set device type as Platform*/
	fctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;

	if (NULL == fctrl->flash_i2c_client) {
		pr_err("%s flash_i2c_client NULL\n",
			__func__);
		rc = -EFAULT;
		goto probe_failure;
	}

	fctrl->flash_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!fctrl->flash_i2c_client->cci_client) {
		pr_err("%s failed line %d kzalloc failed\n",
			__func__, __LINE__);
		return rc;
	}

	cci_client = fctrl->flash_i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = fctrl->cci_i2c_master;
	if (fctrl->flashdata->slave_info->sensor_slave_addr)
		cci_client->sid =
			fctrl->flashdata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;

	if (!fctrl->flash_i2c_client->i2c_func_tbl)
		fctrl->flash_i2c_client->i2c_func_tbl =
			&msm_sensor_cci_func_tbl;

	fctrl->flash_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

#ifdef FEATURE_ENABLE_TORCH_LIGHT
	if (!rc)
		msm_lenovo_torch_create_classdev();
#endif

/*+Begin: chenglong1 for passion flash factory testing*/
#ifdef LENOVO_CAM_PROJECT_PASSION
         /* for passion dual flash testing*/
	rc = sysfs_create_group(&msm_lenovo_torch_led.dev->kobj, &lenovo_flash_drv_node);
	if(rc)
	{
		CDBG("create flash_drv sysfs node failed!!!\n");
	}
	
	lenovo_torch_testing_create_classdev();//chenglong1
#endif
/*+End*/
	rc = msm_led_flash_create_v4lsubdev(pdev, fctrl);

	CDBG("%s: probe success\n", __func__);
	return 0;

probe_failure:
	CDBG("%s probe failed\n", __func__);
	return rc;
}
