/*
 * Flash-led driver for Maxim MAX77819
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/max77819.h>

#include "msm_led_flash.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"


#define M2SH  __CONST_FFS

/* Registers */
#define MAX77819_IFLASH			0x00
#define MAX77819_ITORCH			0x02
#define MAX77819_TORCH_TMR		0x03
#define MAX77819_FLASH_TMR		0x04
#define MAX77819_FLASH_EN		0x05
#define MAX77819_MAX_FLASH1		0x06
#define MAX77819_MAX_FLASH2		0x07
#define MAX77819_MAX_FLASH3		0x08
#define MAX77819_VOUT_CNTL		0x0A
#define MAX77819_VOUT_FLASH		0x0B
#define MAX77819_FLASH_INT		0x0E
#define MAX77819_FLASH_INT_MASK		0x0F
#define MAX77819_FLASH_STATUS		0x10

/* MAX77819_IFLASH */
#define MAX77819_FLASH_I		0x3F

/* MAX77819_ITORCH */
#define MAX77819_TORCH_I		0x0F

/* MAX77819_TORCH_TMR */
#define MAX77819_TORCH_TMR_DUR		0x0F
#define MAX77819_DIS_TORCH_TMR		0x40
#define MAX77819_TORCH_TMR_MODE		0x80
#define MAX77819_TORCH_TMR_ONESHOT	0x00
#define MAX77819_TORCH_TMR_MAXTIMER	0x80

/* MAX77819_FLASH_TMR */
#define MAX77819_FLASH_TMR_DUR		0x06
#define MAX77819_FLASH_TMR_MODE		0x80
#define MAX77819_FLASH_TMR_ONESHOT	0x00
#define MAX77819_FLASH_TMR_MAXTIMER	0x80

/* MAX77819_FLASH_EN */
#define MAX77819_TORCH_FLED_EN		0x0C
#define MAX77819_FLASH_FLED_EN		0xC0
#define MAX77819_OFF			0x00
#define MAX77819_BY_FLASHEN		0x01
#define MAX77819_BY_TORCHEN		0x02
#define MAX77819_BY_I2C			0X03

/* MAX77819_MAX_FLASH1 */
#define MAX77819_MAX_FLASH_HYS		0x03
#define MAX77819_MAX_FLASH_TH		0x7C
#define MAX77819_MAX_FLASH_TH_FROM_VOLTAGE(uV) \
		((((uV) - 2400000) / 33333) << M2SH(MAX77819_MAX_FLASH_TH))
#define MAX77819_MAX_FL_EN		0x80

/* MAX77819_MAX_FLASH2 */
#define MAX77819_LB_TMR_F		0x07
#define MAX77819_LB_TMR_R		0x38
#define MAX77819_LB_TME_FROM_TIME(uSec) ((uSec) / 256)

/* MAX77819_MAX_FLASH3 */
#define MAX77819_FLED_MIN_OUT		0x3F
#define MAX77819_FLED_MIN_MODE		0x80

/* MAX77819_VOUT_CNTL */
#define MAX77819_BOOST_FLASH_MDOE	0x07
#define MAX77819_BOOST_FLASH_MODE_OFF	0x00
#define MAX77819_BOOST_FLASH_MODE_ADAPTIVE	0x01
#define MAX77819_BOOST_FLASH_MODE_FIXED	0x04

/* MAX77819_VOUT_FLASH */
#define MAX77819_BOOST_VOUT_FLASH	0x7F
#define MAX77819_BOOST_VOUT_FLASH_FROM_VOLTAGE(uV)				\
		((uV) <= 3300000 ? 0x00 :					\
		((uV) <= 5500000 ? (((mV) - 3300000) / 25000 + 0x0C) : 0x7F))

/* MAX77819_FLASH_INT_MASK */
#define MAX77819_FLED_OPEN_M		0x04
#define MAX77819_FLED_SHORT_M		0x08
#define MAX77819_MAX_FLASH_M		0x10
#define MAX77819_FLED_FAIL_M		0x20

/* MAX77819_FLASH_STATAUS */
#define MAX77819_TORCH_ON_STAT		0x04
#define MAX77819_FLASH_ON_STAT		0x08

#define MAX_FLASH_CURRENT	1000	// 1000mA(0x1f)
#define MAX_TORCH_CURRENT	250	// 250mA(0x0f)   
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL	15	/* 15.625 + 15.625*15 mA */

#define CAM_FLASH_PINCTRL_STATE_SLEEP "cam_flash_suspend"
#define CAM_FLASH_PINCTRL_STATE_DEFAULT "cam_flash_default"

#define MAX77819_TORCH_ON   1
#define CONFIG_MAX77819_FLASH_DEBUG
#undef CDBG
#ifdef CONFIG_MAX77819_FLASH_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

struct msm_led_flash_ctrl_t fctrl_max;
struct max77819_flash *flash_max77819;
struct max77819_flash{
	struct max77819_dev *chip;
	struct regmap	*regmap;
};

 void max77819_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brightness)
{
	struct max77819_flash *flash = platform_get_drvdata(fctrl_max.pdev);
	int rc = 0, value;
	CDBG("%s:%d called,brightness = %d\n", __func__, __LINE__,brightness);
	if (!flash) {
		pr_err("max77819_torch_brightness_set failed\n");
		return;
	}

	/*Clear out flash interrupts*/
	regmap_read(flash->regmap, MAX77819_FLASH_INT, &value);
	if (brightness > LED_OFF && brightness == MAX77819_TORCH_ON) {
		rc = regmap_update_bits(flash->regmap, MAX77819_ITORCH, MAX77819_TORCH_I,
						0x05 << M2SH(MAX77819_TORCH_I));
		
		rc |= regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_TORCH_FLED_EN,
						(MAX77819_BY_I2C << M2SH(MAX77819_TORCH_FLED_EN)));
	} else if (brightness > LED_OFF) {
		rc = regmap_update_bits(flash->regmap, MAX77819_ITORCH, MAX77819_TORCH_I,
						brightness << M2SH(MAX77819_TORCH_I));
		
		rc |= regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_TORCH_FLED_EN,
						(MAX77819_BY_I2C << M2SH(MAX77819_TORCH_FLED_EN)));
	} else{
		/* Torch OFF */
		rc = regmap_update_bits(flash->regmap, MAX77819_FLASH_EN,
						MAX77819_TORCH_FLED_EN, 0);
	}
};

 struct led_classdev max77819_torch_led = {
	.name			= "torch-light",
	.brightness_set	= max77819_torch_brightness_set,
	.brightness		= LED_OFF,
};

uint8_t max77819_torch_current_to_dac(uint32_t torch_current)
{
    /*---------------------------------------------------
    //led torch brightness level(23.436mA ~ 374.98mA),16 steps, 23.436mA/step
    //Itorch = (brightness code + 1 ) * 23.436mA
    //so brightness code = Itorch /23.436 - 1
    ----------------------------------------------------*/

    uint8_t dac = 0;
    CDBG("max77819_torch_current_to_dac: current = 0x%x\n", torch_current);
    if(torch_current < 24)  torch_current = 24;
    if(torch_current > 374) torch_current = 374;

    dac = (uint8_t )((torch_current * 1000) / 23436)  - 1;
    CDBG("max77819_torch_current_to_dac: dac = 0x%x\n", dac);
    return dac;
}

uint8_t max77819_flash_current_to_dac(uint32_t flash_current)
{
    /*---------------------------------------------------
    //led flash brightness level(23.436mA ~ 1500mA),64 steps, 23.436mA/step
    //Iflash = (brightness code + 1 ) * 23.436mA
    //so brightness code = Iflash /23.436 - 1
    ----------------------------------------------------*/
    uint8_t dac = 0;
    CDBG("max77819_flash_current_to_dac: current = 0x%x\n", flash_current);
    if(flash_current < 24)  flash_current = 24;
    if(flash_current > 1500) flash_current = 1500;

    dac = (uint8_t )((flash_current * 1000) / 23436)  - 1;
    CDBG("max77819_flash_current_to_dac: dac = 0x%x\n", dac);
    return dac;
}

 int32_t max77819_torch_create_classdev(struct platform_device *pdev ,
				void *data)
{
	int rc;
	max77819_torch_brightness_set(&max77819_torch_led, LED_OFF);
	rc = led_classdev_register(&pdev->dev, &max77819_torch_led);
	if (rc) {
		pr_err("Failed to register led dev. rc = %d\n", rc);
	}

	return rc;
};

 int max77819_flash_pinctrl_init(struct msm_led_flash_ctrl_t *ctrl)
{
	struct msm_pinctrl_info *flash_pctrl = NULL;
	flash_pctrl = &ctrl->pinctrl_info;
	if (flash_pctrl->use_pinctrl != true) {
		pr_err("%s: %d PINCTRL is not enables in Flash driver node\n",
			__func__, __LINE__);
		return 0;
	}
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

 int max77819_flash_led_init(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash)
{
	int rc = 0, value;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;

	/*clear interrupt status register*/
	regmap_read(flash->regmap, MAX77819_FLASH_INT, &value);
	
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	rc = regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_FLASH_FLED_EN,
					(MAX77819_BY_FLASHEN << M2SH(MAX77819_FLASH_FLED_EN)));

	return rc;

}

 int max77819_flash_led_release(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl_max) {
		pr_err("%s:%d fctrl_max NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);
	
	regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_FLASH_FLED_EN,
					0);
	regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_TORCH_FLED_EN,
					0);

	return rc;
}

 int max77819_flash_led_off(struct msm_led_flash_ctrl_t *fctrl_max, 
	struct max77819_flash *flash)
{
	int rc = 0, value;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s:%d called\n", __func__, __LINE__);

	if (!fctrl_max) {
		pr_err("%s:%d fctrl_max NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*clear interrupt status register*/
	regmap_read(flash->regmap, MAX77819_FLASH_INT, &value);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	return rc;

}

 int max77819_flash_led_low(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash)
{
	int rc = 0,value;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;

	rc = regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_TORCH_FLED_EN,
					(MAX77819_BY_TORCHEN<< M2SH(MAX77819_TORCH_FLED_EN)));
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);
	rc = regmap_update_bits(flash->regmap, MAX77819_ITORCH, MAX77819_TORCH_I,
					0x00 << M2SH(MAX77819_TORCH_I));
	regmap_read(flash->regmap, MAX77819_ITORCH, &value);
	CDBG("%s MAX77819_IFLASH  value = %x  %d\n", __func__, value,__LINE__);   
	return rc;
}

 int max77819_flash_led_high(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	rc = regmap_update_bits(flash->regmap, MAX77819_IFLASH, MAX77819_FLASH_I,
					0x00 << M2SH(MAX77819_FLASH_I));
	
	return rc;

}

 int max77819_flash_led_low_ext(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash,uint32_t torch_current)
{
	int rc = 0,value;
	uint8_t dac_value;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s MAX77819_ITORCH xujt1 torch_current = %x  %d\n", __func__, torch_current,__LINE__);  
	dac_value = max77819_torch_current_to_dac(torch_current);
	rc = regmap_update_bits(flash->regmap, MAX77819_FLASH_EN, MAX77819_TORCH_FLED_EN,
					(MAX77819_BY_TORCHEN<< M2SH(MAX77819_TORCH_FLED_EN)));
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);
	rc = regmap_update_bits(flash->regmap, MAX77819_ITORCH, MAX77819_TORCH_I,
					dac_value << M2SH(MAX77819_TORCH_I));
	regmap_read(flash->regmap, MAX77819_ITORCH, &value);
	CDBG("%s MAX77819_ITORCH xujt1 value = %x  %d\n", __func__, value,__LINE__);   
	return rc;
}

 int max77819_flash_led_high_ext(struct msm_led_flash_ctrl_t *fctrl_max,
	struct max77819_flash *flash,uint32_t flash_current)
{
	int rc = 0,value;
	uint8_t dac_value;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s MAX77819_IFLASH xujt1 flash_current = 0x%x  %d\n", __func__, flash_current,__LINE__);
	dac_value = max77819_flash_current_to_dac(flash_current);
	
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	rc = regmap_update_bits(flash->regmap, MAX77819_IFLASH, MAX77819_FLASH_I,
					dac_value << M2SH(MAX77819_FLASH_I));
	
	regmap_read(flash->regmap, MAX77819_IFLASH, &value);
	CDBG("%s MAX77819_IFLASH xujt1 value = %x  %d\n", __func__, value,__LINE__); 
	
	return rc;

}

 int32_t max77819_flash_get_dt_data(struct device_node *of_node,
		struct msm_led_flash_ctrl_t *fctrl_max)
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

	CDBG("max77819_flash_get_dt_data called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl_max->flashdata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!fctrl_max->flashdata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	flashdata = fctrl_max->flashdata;
	power_info = &flashdata->power_info;

	rc = of_property_read_u32(of_node, "cell-index", &fctrl_max->subdev_id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}

	CDBG("subdev id %d\n", fctrl_max->subdev_id);

	rc = of_property_read_string(of_node, "label",
		&flashdata->sensor_name);
	CDBG("%s label %s, rc %d\n", __func__,
		flashdata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl_max->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl_max->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl_max->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	fctrl_max->pinctrl_info.use_pinctrl = false;
	fctrl_max->pinctrl_info.use_pinctrl = of_property_read_bool(of_node,
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
				&fctrl_max->flash_trigger_name[i]);
			if (rc < 0) {
				pr_err("failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("default trigger %s\n",
				 fctrl_max->flash_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&fctrl_max->flash_op_current[i]);
			if (rc < 0) {
				pr_err("failed rc %d\n", rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl_max->flash_op_current[i]);

			led_trigger_register_simple(
				fctrl_max->flash_trigger_name[i],
				&fctrl_max->flash_trigger[i]);
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
		fctrl_max->flashdata->slave_info->sensor_slave_addr = id_info[0];
		fctrl_max->flashdata->slave_info->sensor_id_reg_addr = id_info[1];
		fctrl_max->flashdata->slave_info->sensor_id = id_info[2];

		kfree(gpio_array);
		return rc;
ERROR9:
		kfree(fctrl_max->flashdata->slave_info);
ERROR8:
		kfree(fctrl_max->flashdata->power_info.gpio_conf->gpio_num_info);
ERROR6:
		kfree(gconf->cam_gpio_set_tbl);
ERROR5:
		kfree(gconf->cam_gpio_req_tbl);
ERROR4:
		kfree(gconf);
ERROR1:
		kfree(fctrl_max->flashdata);
		kfree(gpio_array);
	}
	return rc;
}


 int max77819_flash_hw_setup(struct max77819_io *io)
{
	struct regmap *regmap = io->regmap;
	unsigned int value;
	int ret = 0;

	/*Disable flash and torch by default*/
	regmap_write(regmap, MAX77819_IFLASH, 0x00);
	regmap_write(regmap, MAX77819_ITORCH, 0x00);
	
	/* Torch Safty Timer Disabled, run for MAX timer */
	ret = regmap_write(regmap, MAX77819_TORCH_TMR,
			MAX77819_TORCH_TMR_DUR | MAX77819_DIS_TORCH_TMR | MAX77819_TORCH_TMR_MODE);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Flash Safty Timer = 1000ms, run for MAX timer */
	ret = regmap_write(regmap, MAX77819_FLASH_TMR,
			MAX77819_FLASH_TMR_DUR | MAX77819_FLASH_TMR_MAXTIMER);
	if (IS_ERR_VALUE(ret))
		return ret;
			
	/* Max Flash setting */
	value = 2 << M2SH(MAX77819_MAX_FLASH_HYS);
	value |= MAX77819_MAX_FLASH_TH_FROM_VOLTAGE(3400000);
	value |= MAX77819_MAX_FL_EN;

	ret = regmap_write(regmap, MAX77819_MAX_FLASH1, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Low battery mask timer for Max Flash */
	value = MAX77819_LB_TME_FROM_TIME(256) << M2SH(MAX77819_LB_TMR_F);
	value |= MAX77819_LB_TME_FROM_TIME(256) << M2SH(MAX77819_LB_TMR_R);
	ret = regmap_write(regmap, MAX77819_MAX_FLASH2, value);
	
	if (IS_ERR_VALUE(ret))
		return ret;
	
 	/* recommended boost mode: adaptive*/
 	ret = regmap_write(regmap, MAX77819_VOUT_CNTL, MAX77819_BOOST_FLASH_MODE_ADAPTIVE);
	return ret;
}

 int32_t max77819_flash_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl_max,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	*subdev_id = fctrl_max->subdev_id;
	
	return 0;
}

 int32_t max77819_flash_trigger_config(struct msm_led_flash_ctrl_t *fctrl_max,
	void *data)
{
	int rc = 0;
	struct msm_camera_led_cfg_t *cfg = (struct msm_camera_led_cfg_t *)data;
	struct max77819_flash *flash = platform_get_drvdata(fctrl_max->pdev);
	
	if (!flash) {
		pr_err("max77819_flash_trigger_config failed\n");
		return -EINVAL;
	}
	switch (cfg->cfgtype) {

	case MSM_CAMERA_LED_INIT:
		rc = max77819_flash_led_init(fctrl_max, flash);
		break;

	case MSM_CAMERA_LED_RELEASE:
		rc = max77819_flash_led_release(fctrl_max, flash);
		break;

	case MSM_CAMERA_LED_OFF:
		rc = max77819_flash_led_off(fctrl_max, flash);
		break;

	case MSM_CAMERA_LED_LOW:
		rc = max77819_flash_led_low(fctrl_max, flash);
		break;

	case MSM_CAMERA_LED_HIGH:
		rc = max77819_flash_led_high(fctrl_max, flash);
		break;
		
	default:
		rc = -EFAULT;
		break;
	}

	return rc;
}

 struct of_device_id max77819_flash_of_ids[] = {
    { .compatible = "maxim,"MAX77819_FLASH_NAME},
    { },
};
MODULE_DEVICE_TABLE(of, max77819_flash_of_ids);

 int max77819_flash_probe(struct platform_device *pdev)
{
	struct max77819_flash *flash;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;	
	struct device_node *of_node = pdev->dev.of_node;	
	struct max77819_dev *chip = dev_get_drvdata(pdev->dev.parent);
	struct max77819_io *io = max77819_get_io(chip);
	int rc;

	if (!of_node){
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl_max.pdev = pdev;

	rc = max77819_flash_get_dt_data(of_node, &fctrl_max);
	if (rc < 0){
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	max77819_flash_pinctrl_init(&fctrl_max);

	/* Assign name for sub device */
	snprintf(fctrl_max.msm_sd.sd.name, sizeof(fctrl_max.msm_sd.sd.name),
			"%s", fctrl_max.flashdata->sensor_name);
	
	/* Set device type as Platform*/
	fctrl_max.flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;

	rc = msm_led_flash_create_v4lsubdev(pdev, &fctrl_max);
	if (rc < 0){
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	flashdata = fctrl_max.flashdata;
	power_info = &flashdata->power_info;

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl_max.pinctrl_info.use_pinctrl == true) {
		pr_err("%s:%d PC:: flash pins setting to active state",
				__func__, __LINE__);
		rc = pinctrl_select_state(fctrl_max.pinctrl_info.pinctrl,
				fctrl_max.pinctrl_info.gpio_state_active);
		if (rc)
			pr_err("%s:%d cannot set pin to active state",
					__func__, __LINE__);
	}

	flash = devm_kzalloc(&pdev->dev, sizeof(struct max77819_flash), GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	// initial setup
	rc = max77819_flash_hw_setup(io);
	if (rc < 0){
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		goto err_out;
	}

	flash->chip = chip;
	flash->regmap = io->regmap;	

	platform_set_drvdata(pdev, flash);

	max77819_torch_create_classdev(pdev, NULL);

	return rc;
	
err_out:
	devm_kfree(&pdev->dev, flash);
	return rc;
}

 int max77819_flash_remove(struct platform_device *pdev)
{
	struct max77819_flash *flash = platform_get_drvdata(pdev);

	devm_kfree(&pdev->dev, flash);
	return 0;
}

 struct msm_flash_fn_t max77819_func_tbl = {
	.flash_get_subdev_id = max77819_flash_trigger_get_subdev_id,
	.flash_led_config = max77819_flash_trigger_config,
};

 struct msm_led_flash_ctrl_t fctrl_max = {
	.func_tbl = &max77819_func_tbl,
};

 struct platform_driver max77819_fled_driver =
{
	.driver		=
	{
		.name	= MAX77819_FLASH_NAME,
		.owner	= THIS_MODULE,
    	.of_match_table  = max77819_flash_of_ids,
	},
	.probe		= max77819_flash_probe,
	.remove		= max77819_flash_remove,
};

 int __init max77819_flash_init(void)
{
	return platform_driver_register(&max77819_fled_driver);
}
module_init(max77819_flash_init);

 void __exit max77819_flash_exit(void)
{
	platform_driver_unregister(&max77819_fled_driver);
}
module_exit(max77819_flash_exit);

MODULE_ALIAS("platform:max77819-flash");
MODULE_AUTHOR("Bo Yang<bo.yang@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77819 Flash Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
