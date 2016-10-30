/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

/*+Begin: chenglong1 add for feature config 2015-4-14*/
#include <media/lenovo_camera_features.h>
/*+End.*/

#ifdef CONFIG_LENOVO_AF_LASER
extern int LASER_STATUS;

extern int msm_get_laser_data(struct msm_sensor_ctrl_t *s_ctrl,struct sensorb_cfg_data32 *cdata32);
extern int stmvl16180(struct msm_sensor_ctrl_t *p_ctrl);
#endif

#define CONFIG_LENOVO_CAMERA_SENSOR_DEBUG
#undef CDBG
#ifdef CONFIG_LENOVO_CAMERA_SENSOR_DEBUG
//#define CDBG(fmt, args...) pr_err(fmt, ##args)
#define CDBG printk
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct v4l2_file_operations msm_sensor_v4l2_subdev_fops;
static void msm_sensor_adjust_mclk(struct msm_camera_power_ctrl_t *ctrl)
{
	int idx;
	struct msm_sensor_power_setting *power_setting;
	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == SENSOR_CLK &&
			power_setting->seq_val ==  SENSOR_CAM_MCLK) {
			if (power_setting->config_val == 24000000) {
				power_setting->config_val = 23880000;
				CDBG("%s MCLK request adjusted to 23.88MHz\n"
							, __func__);
			}
			break;
		}
	}

	return;
}

static int32_t msm_camera_get_power_settimgs_from_sensor_lib(
	struct msm_camera_power_ctrl_t *power_info,
	struct msm_sensor_power_setting_array *power_setting_array)
{
	int32_t rc = 0;
	uint32_t size;
	struct msm_sensor_power_setting *ps;
	bool need_reverse = 0;

	if ((NULL == power_info->power_setting) ||
		(0 == power_info->power_setting_size)) {

		ps = power_setting_array->power_setting;
		size = power_setting_array->size;
		if ((NULL == ps) || (0 == size)) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -EINVAL;
			goto FAILED_1;
		}

		power_info->power_setting =
		kzalloc(sizeof(*ps) * size, GFP_KERNEL);
		if (!power_info->power_setting) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto FAILED_1;
		}
		memcpy(power_info->power_setting,
			power_setting_array->power_setting,
			sizeof(*ps) * size);
		power_info->power_setting_size = size;
	}

	ps = power_setting_array->power_down_setting;
	size = power_setting_array->size_down;
	if (NULL == ps || 0 == size) {
		ps = power_info->power_setting;
		size = power_info->power_setting_size;
		need_reverse = 1;
	}

	power_info->power_down_setting =
	kzalloc(sizeof(*ps) * size, GFP_KERNEL);
	if (!power_info->power_down_setting) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_UP;
	}
	memcpy(power_info->power_down_setting,
		ps,
		sizeof(*ps) * size);
	power_info->power_down_setting_size = size;

	if (need_reverse) {
		int c, end = size - 1;
		struct msm_sensor_power_setting power_down_setting_t;
		for (c = 0; c < size/2; c++) {
			power_down_setting_t =
				power_info->power_down_setting[c];
			power_info->power_down_setting[c] =
				power_info->power_down_setting[end];
			power_info->power_down_setting[end] =
				power_down_setting_t;
			end--;
		}
	}

	return 0;
FREE_UP:
	kfree(power_info->power_setting);
FAILED_1:
	return rc;
}

static int32_t msm_sensor_get_dt_data(struct device_node *of_node,
	struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, i = 0, ret = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	uint32_t id_info[3];

	s_ctrl->sensordata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;

	rc = of_property_read_string(of_node, "qcom,sensor-name",
		&sensordata->sensor_name);
	CDBG("%s qcom,sensor-name %s, rc %d\n", __func__,
		sensordata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_SENSORDATA;
	}

	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, s_ctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	rc = msm_sensor_get_sub_module_index(of_node, &sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_SENSORDATA;
	}

	/* Get sensor mount angle */
	if (0 > of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_info->sensor_mount_angle)) {
		/* Invalidate mount angle flag */
		CDBG("%s:%d Default sensor mount angle\n",
			__func__, __LINE__);
		sensordata->sensor_info->is_mount_angle_valid = 0;
		sensordata->sensor_info->sensor_mount_angle = 0;
	} else {
		sensordata->sensor_info->is_mount_angle_valid = 1;
	}
	CDBG("%s qcom,mount-angle %d\n", __func__,
		sensordata->sensor_info->sensor_mount_angle);
	if (0 > of_property_read_u32(of_node, "qcom,sensor-position",
		&sensordata->sensor_info->position)) {
		CDBG("%s:%d Default sensor position\n", __func__, __LINE__);
		sensordata->sensor_info->position = 0;
	}
	CDBG("%s qcom,sensor-position %d\n", __func__,
		sensordata->sensor_info->position);
	if (0 > of_property_read_u32(of_node, "qcom,sensor-mode",
		&sensordata->sensor_info->modes_supported)) {
		CDBG("%s:%d Default sensor mode\n", __func__, __LINE__);
		sensordata->sensor_info->modes_supported = 0;
	}
	CDBG("%s qcom,sensor-mode %d\n", __func__,
		sensordata->sensor_info->modes_supported);

	s_ctrl->set_mclk_23880000 = of_property_read_bool(of_node,
						"qcom,mclk-23880000");

	CDBG("%s qcom,mclk-23880000 %d\n", __func__,
		s_ctrl->set_mclk_23880000);

	rc = msm_sensor_get_dt_csi_data(of_node, &sensordata->csi_lane_params);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_SENSOR_INFO;
	}

	rc = msm_camera_get_dt_vreg_data(of_node,
			&sensordata->power_info.cam_vreg,
			&sensordata->power_info.num_vreg);
	if (rc < 0)
		goto FREE_CSI;

	rc = msm_camera_get_dt_power_setting_data(of_node,
			sensordata->power_info.cam_vreg,
			sensordata->power_info.num_vreg,
			&sensordata->power_info);


	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_VREG;
	}


	rc = msm_camera_get_power_settimgs_from_sensor_lib(
			&sensordata->power_info,
			&s_ctrl->power_setting_array);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_VREG;
	}

	sensordata->power_info.gpio_conf = kzalloc(
			sizeof(struct msm_camera_gpio_conf), GFP_KERNEL);
	if (!sensordata->power_info.gpio_conf) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto FREE_PS;
	}
	gconf = sensordata->power_info.gpio_conf;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size) {
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
			GFP_KERNEL);
		if (!gpio_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto FREE_GPIO_CONF;
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
			goto FREE_GPIO_CONF;
		}

		rc = msm_camera_get_dt_gpio_set_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto FREE_GPIO_REQ_TBL;
		}

		rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto FREE_GPIO_SET_TBL;
		}
	}
	rc = msm_sensor_get_dt_actuator_data(of_node,
					     &sensordata->actuator_info);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_GPIO_PIN_TBL;
	}

	sensordata->slave_info = kzalloc(sizeof(struct msm_camera_slave_info),
		GFP_KERNEL);
	if (!sensordata->slave_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto FREE_ACTUATOR_INFO;
	}

	rc = of_property_read_u32_array(of_node, "qcom,slave-id",
		id_info, 3);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_SLAVE_INFO;
	}

	sensordata->slave_info->sensor_slave_addr = id_info[0];
	sensordata->slave_info->sensor_id_reg_addr = id_info[1];
	sensordata->slave_info->sensor_id = id_info[2];
	CDBG("%s:%d slave addr 0x%x sensor reg 0x%x id 0x%x\n",
		__func__, __LINE__,
		sensordata->slave_info->sensor_slave_addr,
		sensordata->slave_info->sensor_id_reg_addr,
		sensordata->slave_info->sensor_id);

	/*Optional property, don't return error if absent */
	ret = of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("%s qcom,misc_regulator %s, rc %d\n", __func__,
		 sensordata->misc_regulator, ret);

	kfree(gpio_array);

	return rc;

FREE_SLAVE_INFO:
	kfree(s_ctrl->sensordata->slave_info);
FREE_ACTUATOR_INFO:
	kfree(s_ctrl->sensordata->actuator_info);
FREE_GPIO_PIN_TBL:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
FREE_GPIO_SET_TBL:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_set_tbl);
FREE_GPIO_REQ_TBL:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
FREE_GPIO_CONF:
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
FREE_PS:
	kfree(s_ctrl->sensordata->power_info.power_setting);
	kfree(s_ctrl->sensordata->power_info.power_down_setting);
FREE_VREG:
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
FREE_CSI:
	kfree(s_ctrl->sensordata->csi_lane_params);
FREE_SENSOR_INFO:
	kfree(s_ctrl->sensordata->sensor_info);
FREE_SENSORDATA:
	kfree(s_ctrl->sensordata);
	kfree(gpio_array);
	return rc;
}

static void msm_sensor_misc_regulator(
	struct msm_sensor_ctrl_t *sctrl, uint32_t enable)
{
	int32_t rc = 0;
	if (enable) {
		sctrl->misc_regulator = (void *)rpm_regulator_get(
			&sctrl->pdev->dev, sctrl->sensordata->misc_regulator);
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(sctrl->misc_regulator,
				RPM_REGULATOR_MODE_HPM);
			if (rc < 0) {
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
				rpm_regulator_put(sctrl->misc_regulator);
			}
		} else {
			pr_err("%s: Failed to vote for rpm regulator on %s: %d\n",
				__func__,
				sctrl->sensordata->misc_regulator, rc);
		}
	} else {
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(
				(struct rpm_regulator *)sctrl->misc_regulator,
				RPM_REGULATOR_MODE_AUTO);
			if (rc < 0)
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
			rpm_regulator_put(sctrl->misc_regulator);
		}
	}
}

int32_t msm_sensor_free_sensor_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (!s_ctrl->pdev && !s_ctrl->sensor_i2c_client->client)
		return 0;
	kfree(s_ctrl->sensordata->slave_info);
	kfree(s_ctrl->sensordata->cam_slave_info);
	kfree(s_ctrl->sensordata->actuator_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_set_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata->power_info.power_setting);
	kfree(s_ctrl->sensordata->power_info.power_down_setting);
	kfree(s_ctrl->sensordata->csi_lane_params);
	kfree(s_ctrl->sensordata->sensor_info);
	kfree(s_ctrl->sensordata->power_info.clk_info);
	kfree(s_ctrl->sensordata);
	return 0;
}

static struct msm_cam_clk_info cam_8960_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_clk", 24000000},
};

static struct msm_cam_clk_info cam_8610_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 24000000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};

static struct msm_cam_clk_info cam_8974_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 24000000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};

int msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_power_ctrl_t *power_info;
	enum msm_camera_device_type_t sensor_device_type;
	struct msm_camera_i2c_client *sensor_i2c_client;

	if (!s_ctrl) {
		pr_err("%s:%d failed: s_ctrl %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	sensor_device_type = s_ctrl->sensor_device_type;
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	if (!power_info || !sensor_i2c_client) {
		pr_err("%s:%d failed: power_info %p sensor_i2c_client %p\n",
			__func__, __LINE__, power_info, sensor_i2c_client);
		return -EINVAL;
	}
	return msm_camera_power_down(power_info, sensor_device_type,
		sensor_i2c_client);
}

int msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct msm_camera_power_ctrl_t *power_info;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;
	uint32_t retry = 0;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	sensor_i2c_client = s_ctrl->sensor_i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;

	if (!power_info || !sensor_i2c_client || !slave_info ||
		!sensor_name) {
		pr_err("%s:%d failed: %p %p %p %p\n",
			__func__, __LINE__, power_info,
			sensor_i2c_client, slave_info, sensor_name);
		return -EINVAL;
	}

	if (s_ctrl->set_mclk_23880000)
		msm_sensor_adjust_mclk(power_info);

	for (retry = 0; retry < 3; retry++) {
		rc = msm_camera_power_up(power_info, s_ctrl->sensor_device_type,
			sensor_i2c_client);
		if (rc < 0)
			return rc;
		rc = msm_sensor_check_id(s_ctrl);
		if (rc < 0) {
			msm_camera_power_down(power_info,
				s_ctrl->sensor_device_type, sensor_i2c_client);
			msleep(20);
			continue;
		} else {
			break;
		}
	}

	return rc;
}

int msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}
	sensor_i2c_client = s_ctrl->sensor_i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;

	if (!sensor_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %p %p %p\n",
			__func__, __LINE__, sensor_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__, sensor_name);
		return rc;
	}

    pr_err("%s: read id: 0x%x expected id 0x%x:\n", __func__, chipid, slave_info->sensor_id);
/*+Begin: ljk for s5k2p8 match id. */
/*+Begin: huangsh4 for imx179 match id. */
    if (!strcmp(sensor_name, "s5k2p8")) {
        if ((chipid != 0x2102) &&(chipid != 0x2108)) {
            pr_err("msm_sensor_match_id chip id doesnot match 0x2102 or 2108\n");
            return -ENODEV;
        }
        return rc;
    } else if (!strcmp(sensor_name, "imx179")) {
         chipid = chipid&0xFFF;
	slave_info->sensor_id &=0x0fff; 
	return rc;
    } else if (strstr(sensor_name, "imx214") != NULL) {
         //for imx214 sensor only compare low 12bits for matching ID
         chipid = chipid & 0x0FFF;
	slave_info->sensor_id &= 0x0FFF; 
	return rc;   
    } else if (chipid != slave_info->sensor_id) {
            pr_err("msm_sensor_match_id chip id doesnot match\n");
            return -ENODEV;
    }
    return rc;
}
/*+End.*/
/*+End.*/

/*+Begin: chenglong1 for insensor otp*/
#ifdef LENOVO_CAMERA_INSENSOR_OTP
#define MODULE_INFO_GROUP1_BASE 0x1802
#define MODULE_INFO_GROUP_SIZE 0x10

#define AWB_TABLE_GROUP1_BASE 0x1833
#define AWB_TABLE_GROUP_SIZE 0x1D

#define USE_FIXED_GOLDEN_RATIO
#ifdef USE_FIXED_GOLDEN_RATIO
#define GOLDEN_R_GR_RATIO 0x177
#define GOLDEN_B_GR_RATIO 0x136
#endif

typedef struct __hi545_otp_module_info_t {
	unsigned char module_id;
	unsigned char calibration_ver;
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char sensor_id;
	unsigned char lens_id;
	unsigned char vcm_id;
	unsigned char driver_ic_id;
	unsigned char IR_BG_id;
	unsigned char color_temp_id;
	unsigned char AF_FF_flag;
	unsigned char light_source_flag;
	unsigned char camera_position_id;
	unsigned char reserved;
	unsigned char checksum;
} hi545_otp_module_info_t;

typedef struct __hi545_otp_awb_table_t {
	int r_gr_ratio;
	int b_gr_ratio;
	int gb_gr_ratio;
	int golden_r_gr_ratio;
	int golden_b_gr_ratio;
	int golden_gb_gr_ratio;
	int r_value;
	int b_value;
	int gr_value;
	int gb_value;
	int golden_r_value;
	int golden_b_value;
	int golden_gr_value;
	int golden_gb_value;
} hi545_otp_awb_table_t;

unsigned char hi545_otp_module_info[MODULE_INFO_GROUP_SIZE];
unsigned char hi545_otp_awb_table[AWB_TABLE_GROUP_SIZE];
hi545_otp_awb_table_t hi545_otp_awb_table_info;
uint32_t golden_r_gr_ratio;
uint32_t golden_b_gr_ratio;
uint16_t WBC_R_Gain;
uint16_t WBC_B_Gain;
uint8_t wbc_inited = 0;

struct msm_camera_i2c_reg_array hi545_otp_init_setting_arry[] = {
	{0x0A02, 0x01, 0},
	{0x0118, 0x00, 100},
	{0x0F02, 0x00, 0},
	{0x011A, 0x01, 0},
	{0x011B, 0x09, 0},
	{0x0D04, 0x01, 0},
	{0x0D00, 0x07, 0},
	{0x004C, 0x01, 0},
	{0x003E, 0x01, 0},
	{0x0118, 0x01, 0},
};

int hi545_write_reg_array(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_i2c_reg_array *reg_array, int array_size)
{
	struct msm_camera_i2c_reg_setting conf_array;
	int rc = 0;
	
	conf_array.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	conf_array.data_type = MSM_CAMERA_I2C_BYTE_DATA;
	conf_array.delay = 0;
	conf_array.size = array_size;
	conf_array.reg_setting = reg_array;
	
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(s_ctrl->sensor_i2c_client, &conf_array);
	
	return rc;
}

int hi545_prepare_otp_read_addr(struct msm_sensor_ctrl_t *s_ctrl, uint16_t addr)
{
	struct msm_camera_i2c_reg_array hi545_addr_setting_array[] = {
		{0x010a, (addr>>8) & 0xff},
		{0x010b, addr & 0xff},
		{0x0102, 0x00},
	};

	return hi545_write_reg_array(s_ctrl, hi545_addr_setting_array, sizeof(hi545_addr_setting_array)/sizeof(hi545_addr_setting_array[0]));
}

int hi545_read_reg_val(struct msm_sensor_ctrl_t *s_ctrl, uint16_t addr)
{
	uint16_t local_data = 0;
	int rc = 0;
	struct msm_camera_i2c_read_config read_config = {
		.slave_addr = 0x40,
		.reg_addr = addr,
		.data_type =MSM_CAMERA_I2C_BYTE_DATA,
		.data = local_data};
	uint16_t orig_slave_addr = 0, read_slave_addr = 0;
	
	read_slave_addr = read_config.slave_addr;
	//CDBG("%s:CFG_SLAVE_READ_I2C:\n", __func__);
	//CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
	//	__func__, read_config.slave_addr,
	//	read_config.reg_addr, read_config.data_type);
	
	if (s_ctrl->sensor_i2c_client->cci_client) {
		orig_slave_addr =
			s_ctrl->sensor_i2c_client->cci_client->sid;
		s_ctrl->sensor_i2c_client->cci_client->sid =
			read_slave_addr >> 1;
	} else if (s_ctrl->sensor_i2c_client->client) {
		orig_slave_addr =
			s_ctrl->sensor_i2c_client->client->addr;
		s_ctrl->sensor_i2c_client->client->addr =
			read_slave_addr >> 1;
	} else {
		pr_err("%s: error: no i2c/cci client found.\n", __func__);
		rc = -EFAULT;
	}
	//CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x\n",
	//		__func__, orig_slave_addr,
	//		read_slave_addr >> 1);
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			read_config.reg_addr,
			&local_data, read_config.data_type);
	if (rc < 0) {
		pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
	}

	return local_data;
}

int hi545_init_otp_setting(struct msm_sensor_ctrl_t *s_ctrl)
{
	return hi545_write_reg_array( s_ctrl, hi545_otp_init_setting_arry, sizeof(hi545_otp_init_setting_arry)/sizeof(hi545_otp_init_setting_arry[0]));
}

unsigned char hi545_otp_calc_checksum(unsigned char *ploadData, int len)
{
	int i;
	unsigned long sum=0;
	for (i=0; i<len; i++) {
		sum += *ploadData++;
	}
	return (sum%0xff+1);
}

void hi545_otp_dump_module_info(hi545_otp_module_info_t *module_info)
{
	CDBG("%s: module_id: %x\n", __func__, module_info->module_id);
	CDBG("%s: calibration_ver: %x\n", __func__, module_info->calibration_ver);
	CDBG("%s: year: %d\n", __func__, module_info->year);
	CDBG("%s: month: %d\n", __func__, module_info->month);
	CDBG("%s: day: %d\n", __func__, module_info->day);
	CDBG("%s: sensor_id: %x\n", __func__, module_info->sensor_id);
	CDBG("%s: lens_id: %x\n", __func__, module_info->lens_id);
	CDBG("%s: vcm_id: %x\n", __func__, module_info->vcm_id);
	CDBG("%s: driver_ic_id: %x\n", __func__, module_info->driver_ic_id);
	CDBG("%s: IR_BG_id: %x\n", __func__, module_info->IR_BG_id);
	CDBG("%s: color_temp_id: %x\n", __func__, module_info->color_temp_id);
	CDBG("%s: AF_FF_flag: %x\n", __func__, module_info->AF_FF_flag);
	CDBG("%s: light_source_flag: %x\n", __func__, module_info->light_source_flag);
	CDBG("%s: camera_position_id: %x\n", __func__, module_info->camera_position_id);
}

int hi545_otp_read_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned char module_info_flag = 0;
	unsigned char module_info_group = 0;
	unsigned short module_info_group_base = 0;
	unsigned char checksum = 0;
	int i;

	CDBG("%s: Enter +\n", __func__);
	hi545_prepare_otp_read_addr(s_ctrl, 0x1801);
	module_info_flag = hi545_read_reg_val(s_ctrl, 0x108);
	CDBG("%s: module_info_flag=%x\n", __func__, module_info_flag);
	if ((module_info_flag & 0x0c) == 0x04) {
		module_info_group = 3;
	} else if ((module_info_flag & 0x30) == 0x10) {
		module_info_group = 2;
	} else if ((module_info_flag & 0xc0) == 0x40) {
		module_info_group = 1;
	}

	if (!module_info_group) {
		CDBG("%s: can't find valid module info otp!!!\n", __func__);
		return -1;
	}

	CDBG("%s: found module info in group %d\n", __func__, module_info_group);
	module_info_group_base = MODULE_INFO_GROUP1_BASE+MODULE_INFO_GROUP_SIZE*(module_info_group-1);
	CDBG("%s: group base: %x size: %d\n", __func__, module_info_group_base, MODULE_INFO_GROUP_SIZE);
	for (i=0; i<MODULE_INFO_GROUP_SIZE; i++) {
		hi545_prepare_otp_read_addr(s_ctrl, module_info_group_base+i);
		hi545_otp_module_info[i] = hi545_read_reg_val(s_ctrl, 0x108);
		//CDBG("%s:  0x%x = %x\n", __func__, module_info_group_base+i, hi545_otp_module_info[i]);
	}

	checksum = hi545_otp_calc_checksum(hi545_otp_module_info, MODULE_INFO_GROUP_SIZE-1 );
	if (checksum != hi545_otp_module_info[MODULE_INFO_GROUP_SIZE-1]) {
		pr_err("%s: module info checksum mismatch!!! calc_val: %x, read_val: %x\n", __func__, checksum, hi545_otp_module_info[MODULE_INFO_GROUP_SIZE-1]);
		return -2;
	} else {
		printk("%s: module info checksum match success!!! val: %x\n", __func__, checksum);
	}

	hi545_otp_dump_module_info((hi545_otp_module_info_t *)hi545_otp_module_info);

	CDBG("%s: Exit -\n", __func__);
	
	return 0;
}

void hi545_otp_dump_wbc_info(hi545_otp_awb_table_t *wbc_info)
{
	CDBG("%s: r/gr ratio: %x\n", __func__, wbc_info->r_gr_ratio);
	CDBG("%s: b/gr ratio: %x\n", __func__, wbc_info->b_gr_ratio);
	CDBG("%s: gb/gr ratio: %x\n", __func__, wbc_info->gb_gr_ratio);
	CDBG("%s: golden r/gr ratio: %x\n", __func__, wbc_info->golden_r_gr_ratio);
	CDBG("%s: golden b/gr ratio: %x\n", __func__, wbc_info->golden_b_gr_ratio);
	CDBG("%s:golden  gb/gr ratio: %x\n", __func__, wbc_info->golden_gb_gr_ratio);
	CDBG("%s: r value: %x\n", __func__, wbc_info->r_value);
	CDBG("%s: b value: %x\n", __func__, wbc_info->b_value);
	CDBG("%s: gr value: %x\n", __func__, wbc_info->gr_value);
	CDBG("%s: gb value: %x\n", __func__, wbc_info->gb_value);
	CDBG("%s: golden r value: %x\n", __func__, wbc_info->golden_r_value);
	CDBG("%s: golden b value: %x\n", __func__, wbc_info->golden_b_value);
	CDBG("%s: golden gr value: %x\n", __func__, wbc_info->golden_gr_value);
	CDBG("%s: golden gb value: %x\n", __func__, wbc_info->golden_gb_value);
}

int hi545_otp_read_awb_table(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned char awb_table_flag = 0;
	unsigned char awb_table_group = 0;
	unsigned short awb_table_group_base = 0;
	unsigned char checksum = 0;
	int i;

	//hi545_init_otp_setting();
	CDBG("%s: Enter +\n", __func__);
	
	hi545_prepare_otp_read_addr(s_ctrl, 0x1832);
	awb_table_flag = hi545_read_reg_val(s_ctrl, 0x108);
	CDBG("%s: awb_table_flag=%x\n", __func__, awb_table_flag);
	if ((awb_table_flag & 0x0c) == 0x04) {
		awb_table_group = 3;
	} else if ((awb_table_flag & 0x30) == 0x10) {
		awb_table_group = 2;
	} else if ((awb_table_flag & 0xc0) == 0x40) {
		awb_table_group = 1;
	}

	if (!awb_table_group) {
		CDBG("%s: can't find valid wbc info otp!!!\n", __func__);
		return -1;
	}

	CDBG("%s: found wbc info in group %d\n", __func__, awb_table_group);

	awb_table_group_base = AWB_TABLE_GROUP1_BASE+AWB_TABLE_GROUP_SIZE*(awb_table_group-1);
	CDBG("%s: group base: %x size: %d\n", __func__, awb_table_group_base, AWB_TABLE_GROUP_SIZE);
	for (i=0; i<AWB_TABLE_GROUP_SIZE; i++) {
		hi545_prepare_otp_read_addr(s_ctrl, awb_table_group_base+i);
		hi545_otp_awb_table[i] = hi545_read_reg_val(s_ctrl, 0x108);
		//CDBG("%s:  0x%x = %x\n", __func__, awb_table_group_base+i, hi545_otp_awb_table[i]);
	}

	checksum = hi545_otp_calc_checksum(hi545_otp_awb_table, AWB_TABLE_GROUP_SIZE-1);
	if (checksum != hi545_otp_awb_table[AWB_TABLE_GROUP_SIZE-1]) {
		pr_err("%s: module info checksum mismatch!!! calc_val: %x, read_val: %x\n", __func__, checksum, hi545_otp_awb_table[AWB_TABLE_GROUP_SIZE-1]);
		return -2;
	} else {
		printk("%s: module info checksum match success!!! val: %x\n", __func__, checksum);
	}

	hi545_otp_awb_table_info.r_gr_ratio = ((unsigned short)hi545_otp_awb_table[0]<<2) | ((hi545_otp_awb_table[1] & 0xc0)>>6);
	hi545_otp_awb_table_info.b_gr_ratio = ((unsigned short)hi545_otp_awb_table[2]<<2) | ((hi545_otp_awb_table[3] & 0xc0)>>6);
	hi545_otp_awb_table_info.gb_gr_ratio = ((unsigned short)hi545_otp_awb_table[4]<<2) | ((hi545_otp_awb_table[5] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_r_gr_ratio = ((unsigned short)hi545_otp_awb_table[6]<<2) | ((hi545_otp_awb_table[7] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_b_gr_ratio = ((unsigned short)hi545_otp_awb_table[8]<<2) | ((hi545_otp_awb_table[9] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_gb_gr_ratio = ((unsigned short)hi545_otp_awb_table[10]<<2) | ((hi545_otp_awb_table[11] & 0xc0)>>6);
	hi545_otp_awb_table_info.r_value = ((unsigned short)hi545_otp_awb_table[12]<<2) | ((hi545_otp_awb_table[13] & 0xc0)>>6);
	hi545_otp_awb_table_info.b_value = ((unsigned short)hi545_otp_awb_table[14]<<2) | ((hi545_otp_awb_table[15] & 0xc0)>>6);
	hi545_otp_awb_table_info.gr_value = ((unsigned short)hi545_otp_awb_table[16]<<2) | ((hi545_otp_awb_table[17] & 0xc0)>>6);
	hi545_otp_awb_table_info.gb_value = ((unsigned short)hi545_otp_awb_table[18]<<2) | ((hi545_otp_awb_table[19] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_r_value = ((unsigned short)hi545_otp_awb_table[20]<<2) | ((hi545_otp_awb_table[21] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_b_value = ((unsigned short)hi545_otp_awb_table[22]<<2) | ((hi545_otp_awb_table[23] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_gr_value = ((unsigned short)hi545_otp_awb_table[24]<<2) | ((hi545_otp_awb_table[25] & 0xc0)>>6);
	hi545_otp_awb_table_info.golden_gb_value = ((unsigned short)hi545_otp_awb_table[26]<<2) | ((hi545_otp_awb_table[27] & 0xc0)>>6);

	hi545_otp_dump_wbc_info(&hi545_otp_awb_table_info);
	CDBG("%s: Exit -\n", __func__);
	
	return 0;
}

int hi545_apply_wbc(struct msm_sensor_ctrl_t *s_ctrl, uint16_t R_gain, uint16_t B_gain)
{
	int ret = 0;
	struct msm_camera_i2c_reg_array wbc_apply_array[] = {
		{0x050c, R_gain>>8},
		{0x050d, R_gain&0xff},
		{0x050e, B_gain>>8},
		{0x050f, B_gain&0xff},
	};
	
	ret = hi545_write_reg_array(s_ctrl,  wbc_apply_array, sizeof(wbc_apply_array)/sizeof(wbc_apply_array[0]));
	if (!ret) {
		printk("%s: Apply WBC success!!!\n", __func__);
		return 0;
	}

	return ret;
}


int hi545_otp_apply_wbc(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned short R_gain = 0;
	unsigned short B_gain = 0;

	CDBG("%s: Enter +\n", __func__);
	if (!hi545_otp_read_awb_table(s_ctrl)) {
		#ifdef USE_FIXED_GOLDEN_RATIO
		golden_r_gr_ratio = GOLDEN_R_GR_RATIO;
		golden_b_gr_ratio = GOLDEN_B_GR_RATIO;
		#else
		golden_r_gr_ratio = hi545_otp_awb_table_info.golden_r_gr_ratio;
		golden_b_gr_ratio = hi545_otp_awb_table_info.golden_b_gr_ratio;
		#endif
		R_gain = (unsigned short)(((golden_r_gr_ratio*1000)/hi545_otp_awb_table_info.r_gr_ratio*0x100)/1000);
		B_gain = (unsigned short)(((golden_b_gr_ratio*1000)/hi545_otp_awb_table_info.b_gr_ratio*0x100)/1000);

		CDBG("%s: golden_r_gr_ratio: %x, golden_b_gr_ratio: %x\n", __func__, golden_r_gr_ratio, golden_b_gr_ratio);
		CDBG("%s: R_gin: %x, B_gain: %x\n", __func__, R_gain, B_gain);

		WBC_R_Gain = R_gain;
		WBC_B_Gain = B_gain;
		wbc_inited = 1;
		
		if (!hi545_apply_wbc(s_ctrl, R_gain, B_gain)) {
			printk("%s: Apply WBC success!!!\n", __func__);
			return 0;
		}

		CDBG("%s: Apply WBC Failed, due to I2C write error!!!\n", __func__);
		
		return -2;
	}

	return -1;
}

int hi545_restore_normal_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	int ret = 0;
	struct msm_camera_i2c_reg_array restore_normal_array[] = {
		{0x0118, 0x00, 100},
		{0x003e, 0x00},
		{0x0118, 0x01},
	};
	
	ret = hi545_write_reg_array(s_ctrl,  restore_normal_array, sizeof(restore_normal_array)/sizeof(restore_normal_array[0]));
	if (!ret) {
		CDBG("%s: Apply WBC success!!!\n", __func__);
		return 0;
	}

	return -1;
}

int hi545_apply_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	CDBG("%s: Enter +\n", __func__);
	if (!wbc_inited) {
		rc |= hi545_init_otp_setting(s_ctrl);

		rc |= hi545_otp_read_module_info(s_ctrl);
		rc |= hi545_otp_apply_wbc(s_ctrl);
		rc |= hi545_restore_normal_stream(s_ctrl);
	} else {
		rc = hi545_apply_wbc(s_ctrl, WBC_R_Gain, WBC_B_Gain);
	}
	CDBG("%s: Exit -\n", __func__);
	return rc;
}
#endif
/*+End.*/
static struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(container_of(sd, struct msm_sd_subdev, sd),
		struct msm_sensor_ctrl_t, msm_sd);
}

static void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &s_ctrl->stop_setting);
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return;
}

static int msm_sensor_get_af_status(struct msm_sensor_ctrl_t *s_ctrl,
			void __user *argp)
{
	/* TO-DO: Need to set AF status register address and expected value
	We need to check the AF status in the sensor register and
	set the status in the *status variable accordingly*/
	return 0;
}

static long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	if (!s_ctrl) {
		pr_err("%s s_ctrl NULL\n", __func__);
		return -EBADF;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
#ifdef CONFIG_COMPAT
		if (is_compat_task())
			rc = s_ctrl->func_tbl->sensor_config32(s_ctrl, argp);
		else
#endif
			rc = s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
		return rc;
	case VIDIOC_MSM_SENSOR_GET_AF_STATUS:
		return msm_sensor_get_af_status(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
	case MSM_SD_SHUTDOWN:
		msm_sensor_stop_stream(s_ctrl);
		return 0;
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_sensor_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG32:
		cmd = VIDIOC_MSM_SENSOR_CFG;
	default:
		return msm_sensor_subdev_ioctl(sd, cmd, arg);
	}
}

long msm_sensor_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_sensor_subdev_do_ioctl);
}

static int msm_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data32 *cdata = (struct sensorb_cfg_data32 *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
    //if (CFG_GET_LASER_REG > cdata->cfgtype) 
        CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
		
    #ifdef CONFIG_LENOVO_AF_LASER
        stmvl16180(s_ctrl);
    #endif

		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		cdata->cfg.sensor_info.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_info.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
			CDBG("%s:%d subdev_intf[%d] %d\n", __func__, __LINE__,
				i, cdata->cfg.sensor_info.subdev_intf[i]);
		}
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting32 conf_array32;
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array32,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.data_type = conf_array32.data_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;
		conf_array.reg_setting = compat_ptr(conf_array32.reg_setting);
		conf_array.qup_i2c_batch = conf_array32.qup_i2c_batch;

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
			(void *)(conf_array.reg_setting),
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		uint16_t local_data = 0;
		uint16_t orig_slave_addr = 0, read_slave_addr = 0;
		if (copy_from_user(&read_config,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		read_slave_addr = read_config.slave_addr;
		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				read_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				read_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				read_slave_addr >> 1);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user(&read_config.data,
			(void *)&local_data, sizeof(uint16_t))) {
			pr_err("%s:%d copy failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	/*+Begin: chenglong1 add for insensor otp*/
	#ifdef LENOVO_CAMERA_INSENSOR_OTP
	case CFG_APPLY_INSENSOR_OTP: {
		//if (!strcmp(cdata->cfg.sensor_name, "hi545")) {
			rc = hi545_apply_otp(s_ctrl);
			if (rc)
			{
				pr_err("%s: %d failed: apply otp: %d", __func__, __LINE__, rc);
				rc = -EFAULT;
			}
		//}
		break;
	}
	#endif
	/*+End.*/
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting32 conf_array32;
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array32,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_seq_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;
		conf_array.reg_setting = compat_ptr(conf_array32.reg_setting);

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

    case CFG_WRITE_I2C_ARRAY_L: {
		struct msm_camera_i2c_reg_setting32 conf_array32;
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
    /*+begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
    #if 1
		uint16_t data_count;
		uint16_t array_length;
		uint16_t array_index;
    #define MAX_BURST_LEN 3 //hardware limit, max can write 3 words once
		struct msm_camera_i2c_seq_reg_array_ex {
		    uint16_t reg_addr;
		    uint16_t reg_data[MAX_BURST_LEN];
		    uint16_t reg_data_size;
		};

		struct msm_camera_i2c_seq_reg_array_ex *reg_setting_ex = NULL;
    #endif
    /*+end. */

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__, __LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}


		if (copy_from_user(&conf_array32, (void *)compat_ptr(cdata->cfg.setting), sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.data_type = conf_array32.data_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;
		conf_array.reg_setting = compat_ptr(conf_array32.reg_setting);

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *	(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		
		if (copy_from_user(reg_setting,
			(void *)(conf_array.reg_setting),
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
        pr_err("%s : ljk loopsize=0x%x\n", __func__,conf_array.size);
    	for (i=0; i<conf_array.size; i++)
    	{
        /*+begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
            if (conf_array.reg_setting->reg_addr == 0x6004)
            {
                        while (conf_array.reg_setting->reg_addr != 0xEEEE)
                        {
			        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
											s_ctrl->sensor_i2c_client,
											conf_array.reg_setting->reg_addr,
											conf_array.reg_setting->reg_data,
											conf_array.reg_setting->reg_data_type);
    		               if (rc < 0) {
    			             pr_err("%s : %d write conf_array fail, addr=0x%x data=0x%x datatype=0x%d\n", __func__, __LINE__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data,conf_array.reg_setting->reg_data_type);
    			             break;
    		               }
			        //CDBG("%s:xujt1 {0x%04x,0x%04x}\n", __func__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data);
			        conf_array.reg_setting++;
			        i++;
                        }

			    data_count = 0;
			    array_index = 0;

			    //alloc buffer for arrange data
			    if (conf_array.reg_setting->reg_addr == 0xEEEE && conf_array.reg_setting->reg_data != 0x0000) {
                          if (conf_array.reg_setting->reg_data % MAX_BURST_LEN == 0)
			           array_length = conf_array.reg_setting->reg_data / MAX_BURST_LEN;
				else
				    array_length = (conf_array.reg_setting->reg_data / MAX_BURST_LEN) + 1;

				reg_setting_ex = kzalloc(array_length * (sizeof(struct msm_camera_i2c_seq_reg_array_ex)), GFP_KERNEL);
				if (!reg_setting_ex) {
					pr_err("%s: %d failed\n", __func__, __LINE__);
					rc = -ENOMEM;
					break;
				}

			    }

                        //move array point to the first register
			    conf_array.reg_setting++;
			    i++;

 			    //save the begain register address
 			    reg_setting_ex[array_index].reg_addr = conf_array.reg_setting->reg_addr;
//printk("%s:xujt1  reg_setting_ex[%d].reg_addr = 0x%04x\n", __func__,array_index,reg_setting_ex[array_index].reg_addr);

                        do {
                               //save data
			  	    reg_setting_ex[array_index].reg_data[data_count] = conf_array.reg_setting->reg_data;
				    //CDBG("%s:xujt1  reg_setting_ex[%d].reg_data[%d]  = 0x%04x\n", __func__,array_index,data_count,reg_setting_ex[array_index].reg_data[data_count]);
				    data_count++;

                               reg_setting_ex[array_index].reg_data_size = data_count;
				    //CDBG("%s:xujt1 reg_setting_ex[%d].reg_data_size  = 0x%04x\n", __func__, array_index,reg_setting_ex[array_index].reg_data_size);
                              //move the nexe register
                              conf_array.reg_setting++;
				   i++;

				   if (conf_array.reg_setting->reg_addr != 0xFFFF)
				   {
				      //save address
				      if (data_count % MAX_BURST_LEN == 0)
				      {
				          array_index++;
                                     reg_setting_ex[array_index].reg_addr = conf_array.reg_setting->reg_addr;
					  //CDBG("%s:xujt1  reg_setting_ex[%d].reg_addr  = 0x%04x\n", __func__, array_index,reg_setting_ex[array_index].reg_addr);
					    data_count = 0; //reset index to zero
				      }
				   }
                        	}while(conf_array.reg_setting->reg_addr != 0xFFFF);


			     //write registers with burst mode
                        for (array_index  = 0; array_index < array_length; array_index++)
                        {
                            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq_ex(s_ctrl->sensor_i2c_client,
					   	                                reg_setting_ex[array_index].reg_addr,reg_setting_ex[array_index].reg_data,reg_setting_ex[array_index].reg_data_size);
                            if (rc < 0)
    		              {
    			            pr_err("%s : %d i2c_write_seq_ex fail, addr=0x%x \n", __func__, __LINE__,reg_setting_ex[array_index].reg_addr);
    			            break;
    		              }
                        }

		           //CDBG("%s:xujt1 %d 0x6004 burst write done  rc = %d\n", __func__, __LINE__, rc);

			    if(reg_setting_ex !=  NULL)
			    {
			        kfree(reg_setting_ex);
				  reg_setting_ex = NULL;
			    }
	     	}
              else
		{
			    //default non-burst mode
	                 rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
											s_ctrl->sensor_i2c_client,
											conf_array.reg_setting->reg_addr,
											conf_array.reg_setting->reg_data,
											conf_array.reg_setting->reg_data_type);
			    //CDBG("%s:xujt1 {0x%04x,0x%04x}\n", __func__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data);
                        if(rc < 0)
    		          {
    			        pr_err("%s : %d write conf_array fail, addr=0x%x data=0x%x datatype=0x%d\n", __func__, __LINE__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data,conf_array.reg_setting->reg_data_type);
                            break;
			    }
	     }

            conf_array.reg_setting++;
    	}

/*+begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
             if (conf_array.delay > 0) {
			if (conf_array.delay > 20)
				msleep(conf_array.delay);
			else
				usleep_range(conf_array.delay * 1000, (conf_array.delay	* 1000) + 1000);
	       }
/*+end. */
		kfree(reg_setting);
		break;
	}


	case CFG_POWER_UP:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 1);

			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;
	case CFG_POWER_DOWN:
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;
	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting32 stop_setting32;
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(&stop_setting32,
				(void *)compat_ptr((cdata->cfg.setting)),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		stop_setting->addr_type = stop_setting32.addr_type;
		stop_setting->data_type = stop_setting32.data_type;
		stop_setting->delay = stop_setting32.delay;
		stop_setting->size = stop_setting32.size;
		stop_setting->qup_i2c_batch = stop_setting32.qup_i2c_batch;

		reg_setting = compat_ptr(stop_setting32.reg_setting);

		if (!stop_setting->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting,
			stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}

    #ifdef CONFIG_LENOVO_AF_LASER
    /*xuhx1 for tof begin */
	case CFG_GET_LASER_REG: {
            if (LASER_STATUS == 1) {
			    rc = msm_get_laser_data(s_ctrl, cdata);
			    if (rc < 0) {
				    pr_err("%s:%d Laser get data failed\n", __func__, __LINE__);
			    }
            }
		else{
			pr_err("%s:%d Laser hardware is not ready  failed\n", __func__, __LINE__);
		}
        }
        break;
    /*xuhx1 for tof end */
    #endif
    
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}
#endif

int msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		cdata->cfg.sensor_info.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_info.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
			CDBG("%s:%d subdev_intf[%d] %d\n", __func__, __LINE__,
				i, cdata->cfg.sensor_info.subdev_intf[i]);
		}
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	
    case CFG_WRITE_I2C_ARRAY_L: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

    /*+begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
		uint16_t data_count;
		uint16_t array_length;
		uint16_t array_index;
    #define MAX_BURST_LEN  3 //hardware limit, max can write 3 words once
		struct msm_camera_i2c_seq_reg_array_ex {
		    uint16_t reg_addr;
		    uint16_t reg_data[MAX_BURST_LEN];
		    uint16_t reg_data_size;
		};

		struct msm_camera_i2c_seq_reg_array_ex *reg_setting_ex = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__, __LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		
		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		
		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size * (sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		conf_array.reg_setting = reg_setting;
    	printk("%s : ljk loopsize=0x%x\n", __func__,conf_array.size);
    	for (i=0;i<conf_array.size;i++)
    	{
        /*+begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
#if 1
            if (conf_array.reg_setting->reg_addr == 0x6004)
            {
                        while(conf_array.reg_setting->reg_addr != 0xEEEE)
                        {
			        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
											s_ctrl->sensor_i2c_client,
											conf_array.reg_setting->reg_addr,
											conf_array.reg_setting->reg_data,
											conf_array.reg_setting->reg_data_type);
    		               if(rc < 0)
    		               {
    			             pr_err("%s : %d write conf_array fail, addr=0x%x data=0x%x datatype=0x%d\n", __func__, __LINE__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data,conf_array.reg_setting->reg_data_type);
    			             break;
    		               }
//printk("%s:xujt1 {0x%04x,0x%04x}\n", __func__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data);
			        conf_array.reg_setting++;
			        i++;
                        }

			    data_count = 0;
			    array_index = 0;

			    //alloc buffer for arrange data
			    if(conf_array.reg_setting->reg_addr == 0xEEEE && conf_array.reg_setting->reg_data != 0x0000)
			    {
                          if(conf_array.reg_setting->reg_data % MAX_BURST_LEN == 0)
			           array_length =  conf_array.reg_setting->reg_data / MAX_BURST_LEN;
				else
				    array_length =  (conf_array.reg_setting->reg_data / MAX_BURST_LEN) + 1;

				reg_setting_ex = kzalloc(array_length * (sizeof(struct msm_camera_i2c_seq_reg_array_ex)), GFP_KERNEL);
				if (!reg_setting_ex) {
					pr_err("%s: %d failed\n", __func__, __LINE__);
					rc = -ENOMEM;
					break;
				}

			    }

                        //move array point to the first register
			    conf_array.reg_setting++;
			    i++;

 			    //save the begain register address
 			    reg_setting_ex[array_index].reg_addr = conf_array.reg_setting->reg_addr;
//printk("%s:xujt1  reg_setting_ex[%d].reg_addr = 0x%04x\n", __func__,array_index,reg_setting_ex[array_index].reg_addr);

                        do {
                               //save data
			  	    reg_setting_ex[array_index].reg_data[data_count] = conf_array.reg_setting->reg_data;
//printk("%s:xujt1  reg_setting_ex[%d].reg_data[%d]  = 0x%04x\n", __func__,array_index,data_count,reg_setting_ex[array_index].reg_data[data_count]);
				    data_count++;

                               reg_setting_ex[array_index].reg_data_size = data_count;
//printk("%s:xujt1 reg_setting_ex[%d].reg_data_size  = 0x%04x\n", __func__, array_index,reg_setting_ex[array_index].reg_data_size);
                              //move the nexe register
                              conf_array.reg_setting++;
				   i++;

				   if (conf_array.reg_setting->reg_addr != 0xFFFF)
				   {
				      //save address
				      if(data_count % MAX_BURST_LEN == 0)
				      {
				          array_index++;
                                     reg_setting_ex[array_index].reg_addr = conf_array.reg_setting->reg_addr;
//printk("%s:xujt1  reg_setting_ex[%d].reg_addr  = 0x%04x\n", __func__, array_index,reg_setting_ex[array_index].reg_addr);
					    data_count = 0; //reset index to zero
				      }
				   }
                        	}while(conf_array.reg_setting->reg_addr != 0xFFFF);


			     //write registers with burst mode
                        for(array_index  = 0; array_index < array_length; array_index++)
                        {
                            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq_ex(s_ctrl->sensor_i2c_client,
					   	                                reg_setting_ex[array_index].reg_addr,reg_setting_ex[array_index].reg_data,reg_setting_ex[array_index].reg_data_size);
                            if(rc<0)
    		              {
    			            pr_err("%s : %d i2c_write_seq_ex fail, addr=0x%x \n", __func__, __LINE__,reg_setting_ex[array_index].reg_addr);
    			            break;
    		              }
                        }

//printk("%s:xujt1 %d 0x6004 burst write done  rc = %ld\n", __func__, __LINE__, rc);

			    if(reg_setting_ex !=  NULL)
			    {
			        kfree(reg_setting_ex);
				  reg_setting_ex = NULL;
			    }
	     	}
              else
		{
			    //default non-burst mode
	                 rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
											s_ctrl->sensor_i2c_client,
											conf_array.reg_setting->reg_addr,
											conf_array.reg_setting->reg_data,
											conf_array.reg_setting->reg_data_type);
//printk("%s:xujt1 {0x%04x,0x%04x}\n", __func__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data);
                        if(rc < 0)
    		          {
    			        pr_err("%s : %d write conf_array fail, addr=0x%x data=0x%x datatype=0x%d\n", __func__, __LINE__,conf_array.reg_setting->reg_addr,conf_array.reg_setting->reg_data,conf_array.reg_setting->reg_data_type);
                            break;
			    }
	     }
#endif
/*+end. */
            conf_array.reg_setting++;
    	}

/*+Begining: xujt1. add burst interface write for s5k2p8 camera. 2014-04-01. */
             if (conf_array.delay > 0)
             	{
			if (conf_array.delay > 20)
				msleep(conf_array.delay);
			else
				usleep_range(conf_array.delay * 1000, (conf_array.delay	* 1000) + 1000);
	       }
/*+End. */
		kfree(reg_setting);
		break;
	}

	/*+Begin: chenglong1 add for insensor otp*/
	#ifdef LENOVO_CAMERA_INSENSOR_OTP
	case CFG_APPLY_INSENSOR_OTP: {
		//if (!strcmp(cdata->cfg.sensor_name, "hi545")) {
			rc = hi545_apply_otp(s_ctrl);
			if (rc)
			{
				pr_err("%s: %d failed: apply otp: %d", __func__, __LINE__, rc);
				rc = -EFAULT;
			}
		//}
		break;
	}
	#endif
	/*+End.*/
	
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		uint16_t local_data = 0;
		uint16_t orig_slave_addr = 0, read_slave_addr = 0;
		if (copy_from_user(&read_config,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		read_slave_addr = read_config.slave_addr;
		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				read_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				read_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				read_slave_addr >> 1);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user(&read_config.data,
			(void *)&local_data, sizeof(uint16_t))) {
			pr_err("%s:%d copy failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CFG_SLAVE_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_array_write_config write_config;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		uint16_t write_slave_addr = 0;
		uint16_t orig_slave_addr = 0;

		if (copy_from_user(&write_config,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_array_write_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:CFG_SLAVE_WRITE_I2C_ARRAY:", __func__);
		CDBG("%s:slave_addr=0x%x, array_size=%d\n", __func__,
			write_config.slave_addr,
			write_config.conf_array.size);

		if (!write_config.conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(write_config.conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
				(void *)(write_config.conf_array.reg_setting),
				write_config.conf_array.size *
				sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		write_config.conf_array.reg_setting = reg_setting;
		write_slave_addr = write_config.slave_addr;
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				write_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				write_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				write_slave_addr >> 1);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &(write_config.conf_array));
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 1);

			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_POWER_DOWN:
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;

		if (!stop_setting->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting,
			stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int msm_sensor_check_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static int msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (!on && s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

static int msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd,
	unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops msm_sensor_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops msm_sensor_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops msm_sensor_subdev_ops = {
	.core = &msm_sensor_subdev_core_ops,
	.video  = &msm_sensor_subdev_video_ops,
};

static struct msm_sensor_fn_t msm_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = msm_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
/*+begining: ljk. add burst interface write for s5k2p8 camera. 2014-04-01. */
	.i2c_write_seq_ex = msm_camera_cci_i2c_write_seq_ex,
/*+end. */
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_write_conf_tbl = msm_camera_qup_i2c_write_conf_tbl,
};

int32_t msm_sensor_platform_probe(struct platform_device *pdev,
				  const void *data)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *)data;
	struct msm_camera_cci_client *cci_client = NULL;
	uint32_t session_id;
	unsigned long mount_pos = 0;
	s_ctrl->pdev = pdev;
	CDBG("%s called data %p\n", __func__, data);
	CDBG("%s pdev name %s\n", __func__, pdev->id_entry->name);
	if (pdev->dev.of_node) {
		rc = msm_sensor_get_dt_data(pdev->dev.of_node, s_ctrl);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
	}
	s_ctrl->sensordata->power_info.dev = &pdev->dev;
	s_ctrl->sensor_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	s_ctrl->sensor_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client->cci_client) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}
	/* TODO: get CCI subdev */
	cci_client = s_ctrl->sensor_i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
	cci_client->sid =
		s_ctrl->sensordata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;
	if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
		s_ctrl->sensor_i2c_client->i2c_func_tbl =
			&msm_sensor_cci_func_tbl;
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;
	s_ctrl->sensordata->power_info.clk_info =
		kzalloc(sizeof(cam_8974_clk_info), GFP_KERNEL);
	if (!s_ctrl->sensordata->power_info.clk_info) {
		pr_err("%s:%d failed nomem\n", __func__, __LINE__);
		kfree(cci_client);
		return -ENOMEM;
	}
	memcpy(s_ctrl->sensordata->power_info.clk_info, cam_8974_clk_info,
		sizeof(cam_8974_clk_info));
	s_ctrl->sensordata->power_info.clk_info_size =
		ARRAY_SIZE(cam_8974_clk_info);
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		kfree(s_ctrl->sensordata->power_info.clk_info);
		kfree(cci_client);
		return rc;
	}

	pr_info("%s %s probe succeeded\n", __func__,
		s_ctrl->sensordata->sensor_name);
	v4l2_subdev_init(&s_ctrl->msm_sd.sd,
		s_ctrl->sensor_v4l2_subdev_ops);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, pdev);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =
		s_ctrl->msm_sd.sd.name;

	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	msm_sensor_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_sensor_v4l2_subdev_fops.compat_ioctl32 =
		msm_sensor_subdev_fops_ioctl;
#endif
	s_ctrl->msm_sd.sd.devnode->fops =
		&msm_sensor_v4l2_subdev_fops;

	CDBG("%s:%d\n", __func__, __LINE__);

	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

int msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id, struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t session_id;
	unsigned long mount_pos = 0;
	CDBG("%s %s_i2c_probe called\n", __func__, client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!client->dev.of_node) {
		CDBG("msm_sensor_i2c_probe: of_node is NULL");
		s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
		if (!s_ctrl) {
			pr_err("%s:%d sensor ctrl structure NULL\n", __func__,
				__LINE__);
			return -EINVAL;
		}
		s_ctrl->sensordata = client->dev.platform_data;
	} else {
		CDBG("msm_sensor_i2c_probe: of_node exisists");
		rc = msm_sensor_get_dt_data(client->dev.of_node, s_ctrl);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
	}

	s_ctrl->sensor_device_type = MSM_CAMERA_I2C_DEVICE;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		s_ctrl->sensordata->power_info.dev = &client->dev;
		if (s_ctrl->sensordata->slave_info->sensor_slave_addr)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensordata->slave_info->
				sensor_slave_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;
	if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
		s_ctrl->sensor_i2c_client->i2c_func_tbl =
			&msm_sensor_qup_func_tbl;
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;

	if (!client->dev.of_node) {
		s_ctrl->sensordata->power_info.clk_info =
			kzalloc(sizeof(cam_8960_clk_info), GFP_KERNEL);
		if (!s_ctrl->sensordata->power_info.clk_info) {
			pr_err("%s:%d failed nomem\n", __func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(s_ctrl->sensordata->power_info.clk_info,
			cam_8960_clk_info, sizeof(cam_8960_clk_info));
		s_ctrl->sensordata->power_info.clk_info_size =
			ARRAY_SIZE(cam_8960_clk_info);
	} else {
		s_ctrl->sensordata->power_info.clk_info =
			kzalloc(sizeof(cam_8610_clk_info), GFP_KERNEL);
		if (!s_ctrl->sensordata->power_info.clk_info) {
			pr_err("%s:%d failed nomem\n", __func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(s_ctrl->sensordata->power_info.clk_info,
			cam_8610_clk_info, sizeof(cam_8610_clk_info));
		s_ctrl->sensordata->power_info.clk_info_size =
			ARRAY_SIZE(cam_8610_clk_info);
	}

	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__, client->name);
		kfree(s_ctrl->sensordata->power_info.clk_info);
		return rc;
	}

	CDBG("%s %s probe succeeded\n", __func__, client->name);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->msm_sd.sd, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, client);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =
		s_ctrl->msm_sd.sd.name;
	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	rc = camera_init_v4l2(&s_ctrl->sensor_i2c_client->client->dev,
		&session_id);
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	CDBG("%s:%d\n", __func__, __LINE__);

	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;
}

int32_t msm_sensor_init_default_params(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                       rc = -ENOMEM;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_cam_clk_info      *clk_info = NULL;
	unsigned long mount_pos = 0;

	/* Validate input parameters */
	if (!s_ctrl) {
		pr_err("%s:%d failed: invalid params s_ctrl %p\n", __func__,
			__LINE__, s_ctrl);
		return -EINVAL;
	}

	if (!s_ctrl->sensor_i2c_client) {
		pr_err("%s:%d failed: invalid params sensor_i2c_client %p\n",
			__func__, __LINE__, s_ctrl->sensor_i2c_client);
		return -EINVAL;
	}

	/* Initialize cci_client */
	s_ctrl->sensor_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client->cci_client) {
		pr_err("%s:%d failed: no memory cci_client %p\n", __func__,
			__LINE__, s_ctrl->sensor_i2c_client->cci_client);
		return -ENOMEM;
	}

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = s_ctrl->sensor_i2c_client->cci_client;

		/* Get CCI subdev */
		cci_client->cci_subdev = msm_cci_get_subdev();

		/* Update CCI / I2C function table */
		if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
			s_ctrl->sensor_i2c_client->i2c_func_tbl =
				&msm_sensor_cci_func_tbl;
	} else {
		if (!s_ctrl->sensor_i2c_client->i2c_func_tbl) {
			CDBG("%s:%d\n", __func__, __LINE__);
			s_ctrl->sensor_i2c_client->i2c_func_tbl =
				&msm_sensor_qup_func_tbl;
		}
	}

	/* Update function table driven by ioctl */
	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;

	/* Update v4l2 subdev ops table */
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;

	/* Initialize clock info */
	clk_info = kzalloc(sizeof(cam_8974_clk_info), GFP_KERNEL);
	if (!clk_info) {
		pr_err("%s:%d failed no memory clk_info %p\n", __func__,
			__LINE__, clk_info);
		rc = -ENOMEM;
		goto FREE_CCI_CLIENT;
	}
	memcpy(clk_info, cam_8974_clk_info, sizeof(cam_8974_clk_info));
	s_ctrl->sensordata->power_info.clk_info = clk_info;
	s_ctrl->sensordata->power_info.clk_info_size =
		ARRAY_SIZE(cam_8974_clk_info);

	/* Update sensor mount angle and position in media entity flag */
	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	return 0;

FREE_CCI_CLIENT:
	kfree(cci_client);
	return rc;
}
