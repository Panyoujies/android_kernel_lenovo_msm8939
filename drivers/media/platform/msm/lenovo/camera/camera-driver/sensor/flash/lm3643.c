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
#include <linux/module.h>
#include <linux/export.h>
#include "msm_led_flash.h"

#define FLASH_NAME "qcom,led-flash"

#define CONFIG_LENOVOB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_LENOVOB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver lm3643_i2c_driver;

static struct msm_camera_i2c_reg_array lm3643_init_array[] = {
/*
	{0x0A, 0x00},
	{0x08, 0x04},
	{0x09, 0x19},
*/
	{0x01, 0xF3}, //gpio enable & led on
	{0x08, 0x0F}, //set flash time-out duration to 400ms
};

static struct msm_camera_i2c_reg_array lm3643_off_array[] = {
	{0x01, 0xF0},
};

static struct msm_camera_i2c_reg_array lm3643_release_array[] = {
	{0x01, 0xF0},
};

static struct msm_camera_i2c_reg_array lm3643_low_array[] = {
	{0x05, 0x3F}, //LED1 torch current
	{0x06, 0x3F}, //LED2 torch current
	{0x01, 0xDB},
};

static struct msm_camera_i2c_reg_array lm3643_high_array[] = {
	{0x03, 0x3F}, //LED1 flash current
	{0x04, 0x3F}, //LED2 flash current
	{0x01, 0xEF},
	{0x08, 0x0F}, //set flash time-out duration to 400ms
};

static struct msm_camera_i2c_reg_array lm3643_led1_low_array[] = {
	{0x05, 0x3F}, //LED1 torch current
	{0x01, 0xD9},
};

static struct msm_camera_i2c_reg_array lm3643_led2_low_array[] = {
	{0x06, 0x3F}, //LED2 torch current
	{0x01, 0xDA},
};

static void __exit msm_flash_lm3643_i2c_remove(void)
{
	i2c_del_driver(&lm3643_i2c_driver);
	return;
}

static const struct of_device_id lm3643_trigger_dt_match[] = {
	{.compatible = "qcom,led-flash", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, lm3643_trigger_dt_match);

static const struct i2c_device_id flash_i2c_id[] = {
	{"qcom,led-flash", (kernel_ulong_t)&fctrl},
	{ }
};

static const struct i2c_device_id lm3643_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

uint8_t lm3643_torch_current_to_dac(uint32_t torch_current)
{
    /*---------------------------------------------------
    //led torch brightness level(0.977mA ~ 179mA),default 89.3mA
    //Itorch1/2 = (brightness code * 1.4mA) + 0.977mA
    //so brightness code = (Itorch1/2 -0.977mA)/1.4mA
    ----------------------------------------------------*/
    uint8_t dac = 0;
    CDBG("lm3643_torch_current_to_dac: xujt1 torch_current = 0x%x\n", torch_current);
    if(torch_current < 1)  torch_current = 1;
    if(torch_current > 179) torch_current = 179;

    dac = (uint8_t )((torch_current * 1000 - 977) / 1400);
    CDBG("lm3643_torch_current_to_dac: xujt1 dac = 0x%x\n", dac);
    return dac;
}

uint8_t lm3643_flash_current_to_dac(uint32_t flash_current)
{
    /*---------------------------------------------------
    //led flash brightness level(10.9mA ~ 1.5A),default 729mA
    //Iflash1/2 = (brightness code * 11.725mA) + 10.9mA
    //so brightness code = (Iflash1/2 -10.9mA)/11.725mA
    ----------------------------------------------------*/
    uint8_t dac = 0;
    CDBG("lm3643_flash_current_to_dac: xujt1 flash_current = 0x%x\n", flash_current);
    if(flash_current < 11)  flash_current = 11;
    if(flash_current > 1500) flash_current = 1500;

    dac = (uint8_t )((flash_current * 1000 - 10900) / 11725);
    CDBG("lm3643_flash_current_to_dac: xujt1 dac = 0x%x\n", dac);
    return dac;
}

static int msm_flash_lm3643_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!id) {
		pr_err("msm_flash_lm3643_i2c_probe: id is NULL");
		id = lm3643_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver lm3643_i2c_driver = {
	.id_table = lm3643_i2c_id,
	.probe  = msm_flash_lm3643_i2c_probe,
	.remove = __exit_p(msm_flash_lm3643_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3643_trigger_dt_match,
	},
};

static int msm_flash_lm3643_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_device(lm3643_trigger_dt_match, &pdev->dev);
	if (!match)
		return -EFAULT;
	return msm_flash_probe(pdev, match->data);
}

static struct platform_driver lm3643_platform_driver = {
	.probe = msm_flash_lm3643_platform_probe,
	.driver = {
		.name = "qcom,led-flash",
		.owner = THIS_MODULE,
		.of_match_table = lm3643_trigger_dt_match,
	},
};

static int __init msm_flash_lm3643_init_module(void)
{
	int32_t rc = 0;
	CDBG("%s:%d rc %d   1\n", __func__, __LINE__, rc);

	rc = platform_driver_register(&lm3643_platform_driver);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&lm3643_i2c_driver);
}

static void __exit msm_flash_lm3643_exit_module(void)
{
	if (fctrl.pdev)
		platform_driver_unregister(&lm3643_platform_driver);
	else
		i2c_del_driver(&lm3643_i2c_driver);
}

static struct msm_camera_i2c_client lm3643_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting lm3643_init_setting = {
	.reg_setting = lm3643_init_array,
	.size = ARRAY_SIZE(lm3643_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_off_setting = {
	.reg_setting = lm3643_off_array,
	.size = ARRAY_SIZE(lm3643_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_release_setting = {
	.reg_setting = lm3643_release_array,
	.size = ARRAY_SIZE(lm3643_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_low_setting = {
	.reg_setting = lm3643_low_array,
	.size = ARRAY_SIZE(lm3643_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_high_setting = {
	.reg_setting = lm3643_high_array,
	.size = ARRAY_SIZE(lm3643_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_led1_low_setting = {
	.reg_setting = lm3643_led1_low_array,
	.size = ARRAY_SIZE(lm3643_led1_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3643_led2_low_setting = {
	.reg_setting = lm3643_led2_low_array,
	.size = ARRAY_SIZE(lm3643_led2_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t lm3643_regs = {
	.init_setting = &lm3643_init_setting,
	.off_setting = &lm3643_off_setting,
	.low_setting = &lm3643_low_setting,
	.high_setting = &lm3643_high_setting,
	.release_setting = &lm3643_release_setting,
	.led1_low_setting = &lm3643_led1_low_setting,
	.led2_low_setting = &lm3643_led2_low_setting,
};

static struct msm_flash_fn_t lm3643_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
	.flash_led1_low = msm_flash_led1_low,
	.flash_led2_low = msm_flash_led2_low,
	.flash_led3_low = msm_flash_led3_low,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &lm3643_i2c_client,
	.reg_setting = &lm3643_regs,
	.func_tbl = &lm3643_func_tbl,
};

/*subsys_initcall(msm_flash_i2c_add_driver);*/
module_init(msm_flash_lm3643_init_module);
module_exit(msm_flash_lm3643_exit_module);
MODULE_DESCRIPTION("lm3643 FLASH");
MODULE_LICENSE("GPL v2");
