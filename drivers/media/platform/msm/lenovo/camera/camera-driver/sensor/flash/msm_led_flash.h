/* Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
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
#ifndef MSM_LED_FLASH_H
#define MSM_LED_FLASH_H

#include <linux/leds.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include "../../../include/lenovo_media/msm_cam_sensor.h"
#include "../../../include/lenovo_soc/qcom/camera2.h"
#include "msm_camera_i2c.h"
#include "msm_sd.h"


struct msm_led_flash_ctrl_t;

struct msm_flash_fn_t {
	int32_t (*flash_get_subdev_id)(struct msm_led_flash_ctrl_t *, void *);
	int32_t (*flash_led_config)(struct msm_led_flash_ctrl_t *, void *);
	int32_t (*flash_led_init)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_release)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_off)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_low)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_high)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led1_low)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led2_low)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led3_low)(struct msm_led_flash_ctrl_t *);
};

/*+Begin: chenglong1 add for flash current multi-level 2015-4-5*/
typedef int (*current_setting_prepare_func)(uint32_t current_in_ma, uint32_t led_idx);
typedef int (*flash_duration_setting_func)(uint32_t duration, uint32_t led_idx);

typedef struct __flash_chip_current_table_prepare_func_tbl {
	current_setting_prepare_func flash_current_setting_func;
	current_setting_prepare_func torch_current_setting_func;
	current_setting_prepare_func flash_level_setting_func;
	current_setting_prepare_func torch_level_setting_func;
	flash_duration_setting_func flash_duration_set_func;
} flash_current_reg_setting_prepare_func;
/*+End. */

struct msm_led_flash_reg_t {
	struct msm_camera_i2c_reg_setting *init_setting;
	struct msm_camera_i2c_reg_setting *off_setting;
	struct msm_camera_i2c_reg_setting *release_setting;
	struct msm_camera_i2c_reg_setting *low_setting;
	struct msm_camera_i2c_reg_setting *high_setting;
	struct msm_camera_i2c_reg_setting *led1_low_setting;
	struct msm_camera_i2c_reg_setting *led2_low_setting;
/*+Begin: chenglong1 add for flash current multi-level 2015-4-5*/
	flash_current_reg_setting_prepare_func p_flash_source_cur_setting_func_tbl;
/*+End. */
};

/*begin add flash power by ljk*/
#define MSM_VREG_MAX_VREGS (10)

struct msm_flash_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_VREG_MAX_VREGS];
	int num_vreg;
};
/*end add flash power by ljk*/

struct msm_led_flash_ctrl_t {
	struct msm_camera_i2c_client *flash_i2c_client;
	struct msm_sd_subdev msm_sd;
	struct platform_device *pdev;
	struct msm_flash_fn_t *func_tbl;
	struct msm_camera_sensor_board_info *flashdata;
	struct msm_led_flash_reg_t *reg_setting;
	/* Flash */
	const char *flash_trigger_name[MAX_LED_TRIGGERS];
	struct led_trigger *flash_trigger[MAX_LED_TRIGGERS];
	uint32_t flash_num_sources;
	uint32_t flash_op_current[MAX_LED_TRIGGERS];
	uint32_t flash_max_current[MAX_LED_TRIGGERS];
	uint32_t flash_max_duration[MAX_LED_TRIGGERS];
	/* Torch */
	const char *torch_trigger_name[MAX_LED_TRIGGERS];
	struct led_trigger *torch_trigger[MAX_LED_TRIGGERS];
	uint32_t torch_num_sources;
	uint32_t torch_op_current[MAX_LED_TRIGGERS];
	uint32_t torch_max_current[MAX_LED_TRIGGERS];

	void *data;
	enum msm_camera_device_type_t flash_device_type;
	enum cci_i2c_master_t cci_i2c_master;
	enum msm_camera_led_config_t led_state;
	uint32_t subdev_id;
	struct msm_pinctrl_info pinctrl_info;
/*begin add flash power by ljk*/
	struct msm_flash_vreg vreg_cfg;
/*end add flash power by ljk*/
};

int msm_flash_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

int msm_flash_probe(struct platform_device *pdev, const void *data);

int32_t msm_led_flash_create_v4lsubdev(struct platform_device *pdev,
	void *data);
int32_t msm_led_i2c_flash_create_v4lsubdev(void *data);

int32_t msm_led_i2c_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl,
	void *arg);

int32_t msm_led_i2c_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data);

int select_max77819_flash_current(uint32_t flash_current);
int msm_flash_led_init(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_release(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_off(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_low(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_high(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led1_low(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led2_low(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led3_low(struct msm_led_flash_ctrl_t *fctrl);
#endif
