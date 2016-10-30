/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_actuator.h"
#include "msm_cci.h"

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
#include "onsemi_ois.h"
#include "onsemi_cmd.h"
#endif
#include <linux/proc_fs.h>
#include <linux/kthread.h>
/*+begin add laser position read by lijk3 sysfs attributes */
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
/*+end add laser position read by lijk3 sysfs attributes */

#ifdef CONFIG_LENOVO_AF_LASER
/*xuhx1 for tof begin */
#include "../laser/vl6180x_api.h"
#include "stmvl6180.h"

#endif

#define TOF_INIT_SIZE 41
#define TOF_RESULT_BYTE 52
#define TOF_SLAVE_ADDR 0x0029
#define TOF_RESULT_START_ADDR 0x0052
/*xuhx1 for tof end */

DEFINE_MSM_MUTEX(msm_actuator_mutex);
#define MSM_ACUTUATOR_DEBUG

#undef CDBG
#ifdef MSM_ACUTUATOR_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define ACTUATOR_INFO				0
#define ACTUATOR_DEBUG				1
#define ACTUATOR_NAME  0 //for z3

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
static int need_init_lc = 0;
int OIS_STATUS = 0;
/**+Begin:
   * Added by ljk for set oismode.
   * Add Commet by chenzc2 in following line.
   * This variable is to record that camera is in preview mode or in camcorder mode.
   */
static int cam_mode = 0;
/* + end  */
#endif

/*+begin for laser af by ljk*/
#ifdef CONFIG_LENOVO_AF_LASER
int LASER_STATUS = 0;
struct msm_camera_laser_data laser_data;

extern int vl6180_init(struct msm_actuator_ctrl_t *a_ctrl);
extern int vl6180_deinit(struct msm_actuator_ctrl_t *a_ctrl);
extern int vl6180_get_data(struct msm_actuator_ctrl_t *a_ctrl, struct msm_camera_laser_data *laser_data);
#endif
/*+end for laser af by ljk*/

#define MAX_QVALUE  4096
static struct v4l2_file_operations msm_actuator_v4l2_subdev_fops;
static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl);
static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl);

static struct msm_actuator msm_vcm_actuator_table;
static struct msm_actuator msm_piezo_actuator_table;
static struct msm_actuator msm_hvcm_actuator_table;

static struct i2c_driver msm_actuator_i2c_driver;
static struct msm_actuator *actuators[] = {
	&msm_vcm_actuator_table,
	&msm_piezo_actuator_table,
	&msm_hvcm_actuator_table,
};

/**+Begin:
   * add ois by ljk
   * Comment by chenzc2 in the following line.
   * This variable to be review because it is not only used by ois but also laser.
   */
struct msm_actuator_ctrl_t *actuator_ctrl = NULL;
/*+End.*/

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
static int32_t msm_actuator_init_lc_tune_data(struct msm_actuator_ctrl_t *a_ctrl, struct msm_actuator_set_info_t *set_info)
{
    /* OIS initial */
    int32_t rc = 0;
    CDBG("%s enter non-thread-mode\n",__func__);

    rc = IniSet();// LC898122 OIS initialize
    CDBG("%s IniSet return %d\n", __func__,rc);

    RtnCen(0x00);// Lens centering

    if (rc == OIS_FW_POLLING_PASS) {
        OIS_STATUS = 1;
    }
    CDBG("%s exit non-thread-mode OIS_STATUS =0x%x\n",__func__,OIS_STATUS);

    return 0;
}
#endif

static int32_t msm_actuator_piezo_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (a_ctrl->curr_step_pos != 0) {
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.data_type = a_ctrl->i2c_data_type;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s: i2c write error:%d\n",
				__func__, rc);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->curr_step_pos = 0;
	}
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_parse_i2c_params(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = a_ctrl->reg_tbl;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0;
	uint32_t size = a_ctrl->reg_tbl_size, i = 0;
	struct msm_camera_i2c_reg_array *i2c_tbl = a_ctrl->i2c_reg_tbl;
	CDBG("%s Enter ljk cam_name=%d\n",__func__,a_ctrl->cam_name);
	for (i = 0; i < size; i++) {
		/* check that the index into i2c_tbl cannot grow larger that
		the allocated size of i2c_tbl */
		if ((a_ctrl->total_steps + 1) < (a_ctrl->i2c_tbl_index)) {
			break;
		}
		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC) {
			value = (next_lens_position <<
				write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift);

			if (write_arr[i].reg_addr != 0xFFFF) {
			  if (a_ctrl->cam_name != ACTUATOR_NAME) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
				if (size != (i+1)) {
				  i2c_byte2 = value & 0xFF;
				  CDBG("byte1:0x%x, byte2:0x%x\n", i2c_byte1, i2c_byte2);
				  i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
				  i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
				  i2c_tbl[a_ctrl->i2c_tbl_index].delay = 0;
				  a_ctrl->i2c_tbl_index++;
				  i++;
				  i2c_byte1 = write_arr[i].reg_addr;
				  i2c_byte2 = (value & 0xFF00) >> 8;
				}
			  } else {
			    i2c_byte1 = write_arr[i].reg_addr;
    			i2c_byte2 = (0x04|((next_lens_position&0x300)>>8));//for onsemi
    			if (size != (i+1)) {
    			  i2c_byte2 = i2c_byte2 & 0xFF;
    			  CDBG("byte1:0x%x, byte2:0x%x\n", i2c_byte1, i2c_byte2);
    			  i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
    			  i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
    			  i2c_tbl[a_ctrl->i2c_tbl_index].delay = 0;
    			  a_ctrl->i2c_tbl_index++;
    			  i++;
    			  i2c_byte1 = write_arr[i].reg_addr;
    			  i2c_byte2 = (next_lens_position&0xFF);
    			}
			  }
			} else {
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
		}
		/*+begin xujt1 Add DW driver IC 2014-06-25*/
		else if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC_DW9718S) {
			value = (next_lens_position & 0x3FF);

			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = value;
			if (size != (i+1)) {
				i2c_byte2 = (value & 0xFF00) >> 8;
				CDBG("byte1:0x%x, byte2:0x%x\n",i2c_byte1, i2c_byte2);
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1; //MSB
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
				i2c_tbl[a_ctrl->i2c_tbl_index].delay = 0;
				a_ctrl->i2c_tbl_index++;
				i++;
				i2c_byte1 = write_arr[i].reg_addr; //LSB
				i2c_byte2 = value & 0xFF;
			}
		}
		else if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC_DW9761B) {
			int rc = 0;
			uint16_t status = 0, index;
			value = (next_lens_position & 0x3FF);

                    //first read the status register and check busy bit
                    for(index = 0; index < 5; index++)
                    {
			    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client,
					0x05,&status,MSM_CAMERA_I2C_BYTE_DATA);
			    if((rc == 0x0) && ((status & 0x03) == 0x0))
			    {
                             CDBG("dw9761b not busy\n");
                             break;
			    }
			    usleep(500);
                    }

			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = value;
			if (size != (i+1)) {
				i2c_byte2 = (value & 0xFF00) >> 8;
				CDBG("byte1:0x%x, byte2:0x%x\n",i2c_byte1, i2c_byte2);
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1; //MSB
				i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
				i2c_tbl[a_ctrl->i2c_tbl_index].delay = 0;
				a_ctrl->i2c_tbl_index++;
				i++;
				i2c_byte1 = write_arr[i].reg_addr; //LSB
				i2c_byte2 = value & 0xFF;
			}
		}
		/*+end xujt1 Add DW driver IC 2014-06-25*/
		else {
			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = (hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift;
		}
		CDBG("i2c_byte1:0x%x, i2c_byte2:0x%x\n", i2c_byte1, i2c_byte2);
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
		i2c_tbl[a_ctrl->i2c_tbl_index].delay = delay;
		a_ctrl->i2c_tbl_index++;
	}
	CDBG("%s Exit\n",__func__);
}

#ifdef ONSEMI_LC_MODULE
struct lc_cal_data lc_cal_data_eeprom;
uint16_t inital_pos_val = 0;
struct lc_reg_settings_t lc_reg_setting1[]=
{
	{0x80, 0x34, MSM_CAMERA_I2C_BYTE_DATA},
	{0x81, 0x20, MSM_CAMERA_I2C_BYTE_DATA},
	{0x84, 0xE0, MSM_CAMERA_I2C_BYTE_DATA},
	{0x87, 0x05, MSM_CAMERA_I2C_BYTE_DATA},
	{0xA4, 0x24, MSM_CAMERA_I2C_BYTE_DATA},
	{0x3A, 0x0000, MSM_CAMERA_I2C_WORD_DATA},
	{0x04, 0x0000, MSM_CAMERA_I2C_WORD_DATA},
	{0x02, 0x0000, MSM_CAMERA_I2C_WORD_DATA},
	{0x18, 0x0000, MSM_CAMERA_I2C_WORD_DATA},
	{0x88, 0x70, MSM_CAMERA_I2C_BYTE_DATA},
	{0x28, 0x8080, MSM_CAMERA_I2C_WORD_DATA},
	{0x4C, 0x4000, MSM_CAMERA_I2C_WORD_DATA},
	{0x83, 0x2C, MSM_CAMERA_I2C_BYTE_DATA},
	{0x85, 0xC0, MSM_CAMERA_I2C_BYTE_DATA},

};
struct lc_reg_settings_t lc_reg_setting2[]=
{
	{0x84, 0xE3, MSM_CAMERA_I2C_BYTE_DATA},
	{0x97, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x98, 0x42, MSM_CAMERA_I2C_BYTE_DATA},
	{0x99, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x9A, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
};
struct lc_reg_settings_t CsHalReg[]=
{
	{ 0x76, 0x0C, MSM_CAMERA_I2C_BYTE_DATA},	/*0C,0076,0dB*/
	{ 0x77, 0x50, MSM_CAMERA_I2C_BYTE_DATA},	/*50,0077,30dB*/
	{ 0x78, 0x40, MSM_CAMERA_I2C_BYTE_DATA},	/*40,0078,12dB*/
	{ 0x86, 0x40, MSM_CAMERA_I2C_BYTE_DATA},	/*40,0086*/
	{ 0xF0, 0x00, MSM_CAMERA_I2C_BYTE_DATA},	/*00,00F0,Through,0dB,fs/1,invert=0*/
	{ 0xF1, 0x00, MSM_CAMERA_I2C_BYTE_DATA},	/*00,00F1,LPF,1800Hz,0dB,fs/1,invert=0*/
};
struct lc_reg_settings_t CsHalFil[]=
{
	{ 0x30, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,0030,LPF,1800Hz,0dB,fs/1,invert=0*/
	{ 0x40, 0x8010, MSM_CAMERA_I2C_WORD_DATA},	/*8010,0040,0dB,invert=1*/
	{ 0x42, 0x7150, MSM_CAMERA_I2C_WORD_DATA},	/*7150,0042,HBF,30Hz,1000Hz,0dB,fs/1,invert=0*/
	{ 0x44, 0x8F90, MSM_CAMERA_I2C_WORD_DATA},	/*8F90,0044,HBF,30Hz,1000Hz,0dB,fs/1,invert=0*/
	{ 0x46, 0x61B0, MSM_CAMERA_I2C_WORD_DATA},	/*61B0,0046,HBF,30Hz,1000Hz,0dB,fs/1,invert=0*/
	{ 0x48, 0x65B0, MSM_CAMERA_I2C_WORD_DATA},	/*65B0,0048,-2dB,invert=0*/
	{ 0x4A, 0x2870, MSM_CAMERA_I2C_WORD_DATA},	/*2870,004A,-10dB,invert=0*/
	{ 0x4C, 0x4030, MSM_CAMERA_I2C_WORD_DATA},	/*4030,004C,-6dB,invert=0*/
	{ 0x4E, 0x7FF0, MSM_CAMERA_I2C_WORD_DATA},	/*7FF0,004E,0dB,invert=0*/
	{ 0x50, 0x04F0, MSM_CAMERA_I2C_WORD_DATA},	/*04F0,0050,LPF,300Hz,0dB,fs/1,invert=0*/
	{ 0x52, 0x7610, MSM_CAMERA_I2C_WORD_DATA},	/*7610,0052,LPF,300Hz,0dB,fs/1,invert=0*/
	{ 0x54, 0x16C0, MSM_CAMERA_I2C_WORD_DATA},	/*16C0,0054,DI,-15dB,fs/16,invert=0*/
	{ 0x56, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,0056,DI,-15dB,fs/16,invert=0*/
	{ 0x58, 0x7FF0, MSM_CAMERA_I2C_WORD_DATA},	/*7FF0,0058,DI,-15dB,fs/16,invert=0*/
	{ 0x5A, 0x0680, MSM_CAMERA_I2C_WORD_DATA},	/*0680,005A,LPF,400Hz,0dB,fs/1,invert=0*/
	{ 0x5C, 0x72F0, MSM_CAMERA_I2C_WORD_DATA},	/*72F0,005C,LPF,400Hz,0dB,fs/1,invert=0*/
	{ 0x5E, 0x7F70, MSM_CAMERA_I2C_WORD_DATA},	/*7F70,005E,HPF,35Hz,0dB,fs/1,invert=0*/
	{ 0x60, 0x7ED0, MSM_CAMERA_I2C_WORD_DATA},	/*7ED0,0060,HPF,35Hz,0dB,fs/1,invert=0*/
	{ 0x62, 0x7FF0, MSM_CAMERA_I2C_WORD_DATA},	/*7FF0,0062,Through,0dB,fs/1,invert=0*/
	{ 0x64, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,0064,Through,0dB,fs/1,invert=0*/
	{ 0x66, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,0066,Through,0dB,fs/1,invert=0*/
	{ 0x68, 0x5130, MSM_CAMERA_I2C_WORD_DATA},	/*5130,0068,HPF,400Hz,-3.5dB,fs/1,invert=0*/
	{ 0x6A, 0x72F0, MSM_CAMERA_I2C_WORD_DATA},	/*72F0,006A,HPF,400Hz,-3.5dB,fs/1,invert=0*/
	{ 0x6C, 0x8010, MSM_CAMERA_I2C_WORD_DATA},	/*8010,006C,0dB,invert=1*/
	{ 0x6E, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,006E,Cutoff,invert=0*/
	{ 0x70, 0x0000, MSM_CAMERA_I2C_WORD_DATA},	/*0000,0070,Cutoff,invert=0*/
	{ 0x72, 0x18E0, MSM_CAMERA_I2C_WORD_DATA},	/*18E0,0072,LPF,1800Hz,0dB,fs/1,invert=0*/
	{ 0x74, 0x4E30, MSM_CAMERA_I2C_WORD_DATA},	/*4E30,0074,LPF,1800Hz,0dB,fs/1,invert=0*/
};
#define ONSEMI_LC_BIAS_START	0
#define ONSEMI_LC_OFFSET_START	1
#define ONSEMI_LC_INFINITY_H_BYTE 2
#define ONSEMI_LC_INFINITY_L_BYTE 3
#define ONSEMI_LC_MACRO_H_BYTE 4
#define ONSEMI_LC_MACRO_L_BYTE 5
#define ONSEMI_LC_DATA_EEPROM_START 0x23
static int32_t msm_lc_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc=-EFAULT;
	int i=0;
	int size_array_setting;
	struct lc_reg_settings_t *lc_reg_settings=NULL;
	int loop=0;
	uint16_t val=0;
	int lc_sid=0;
	uint8_t lc_eeprom_data[7];
	CDBG("%s:Enter\n", __func__);
	lc_reg_settings=lc_reg_setting1;
	size_array_setting=sizeof(lc_reg_setting1)/sizeof(struct lc_reg_settings_t);
	CDBG("%s:size of setting1=%d\n", __func__, size_array_setting);
	for(i=0;i<size_array_setting;i++)
	{
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
										&a_ctrl->i2c_client,
										lc_reg_settings->reg_addr,
										lc_reg_settings->reg_data,
										lc_reg_settings->reg_data_type);
		if(rc<0)
		{
			printk("%s : %d write lc_reg_setting1 fail\n", __func__, __LINE__);
			return rc;
		}
		lc_reg_settings++;
	}
	loop=10;
	while(loop--)
	{
		val =-1;
		rc=a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
										&a_ctrl->i2c_client,
										0x85,
										&val,
										MSM_CAMERA_I2C_BYTE_DATA);
		if(rc<0)
		{
			printk("%s : %d read status reg[0x85] fail\n", __func__, __LINE__);
			return rc;
		}
		if(val==0)
		{
			break;
		}
		msleep(1);
	}
	CDBG("%s :%d  loop=%d, val=%d\n", __func__, __LINE__,loop,val);
	lc_reg_settings=lc_reg_setting2;
	size_array_setting=sizeof(lc_reg_setting2)/sizeof(struct lc_reg_settings_t);
	CDBG("%s:size of setting2=%d\n", __func__, size_array_setting);
	for(i=0;i<size_array_setting;i++)
	{
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
										&a_ctrl->i2c_client,
										lc_reg_settings->reg_addr,
										lc_reg_settings->reg_data,
										lc_reg_settings->reg_data_type);
		if(rc<0)
		{
			printk("%s : %d write lc_reg_setting2 fail\n", __func__, __LINE__);
			return rc;
		}
		lc_reg_settings++;
	}
	lc_reg_settings=CsHalReg;
	size_array_setting=sizeof(CsHalReg)/sizeof(struct lc_reg_settings_t);
	CDBG("%s:size of CsHalReg=%d\n", __func__, size_array_setting);
	for(i=0;i<size_array_setting;i++)
	{
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
										&a_ctrl->i2c_client,
										lc_reg_settings->reg_addr,
										lc_reg_settings->reg_data,
										lc_reg_settings->reg_data_type);
		if(rc<0)
		{
			printk("%s : %d write CsHalReg fail\n", __func__, __LINE__);
			return rc;
		}
		lc_reg_settings++;
	}
	lc_reg_settings=CsHalFil;
	size_array_setting=sizeof(CsHalFil)/sizeof(struct lc_reg_settings_t);
	CDBG("%s:size of CsHalFil=%d\n", __func__, size_array_setting);
	for(i=0;i<size_array_setting;i++)
	{
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
										&a_ctrl->i2c_client,
										lc_reg_settings->reg_addr,
										lc_reg_settings->reg_data,
										lc_reg_settings->reg_data_type);
		if(rc<0)
		{
			printk("%s : %d write CsHalFil fail\n", __func__, __LINE__);
			return rc;
		}
		lc_reg_settings++;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x86,
									0x60,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write DSSEL 1/16 INTON \n", __func__, __LINE__);
		return rc;
	}
	{
		lc_sid=a_ctrl->i2c_client.cci_client->sid;
		a_ctrl->i2c_client.cci_client->sid = 0xA4>>1; //eeprom sid
		rc=a_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
									&a_ctrl->i2c_client,
									ONSEMI_LC_DATA_EEPROM_START,
									lc_eeprom_data,
									7);
		a_ctrl->i2c_client.cci_client->sid = lc_sid;
		if(rc<0)
		{
			printk("%s : %d read close loop data fail\n", __func__, __LINE__);
			return rc;
		}
		lc_cal_data_eeprom.lc_bias = lc_eeprom_data[ONSEMI_LC_BIAS_START];
		lc_cal_data_eeprom.lc_offset= lc_eeprom_data[ONSEMI_LC_OFFSET_START];
		lc_cal_data_eeprom.lc_infinity= ((lc_eeprom_data[ONSEMI_LC_INFINITY_H_BYTE]<<8)&0xff00)|
										lc_eeprom_data[ONSEMI_LC_INFINITY_L_BYTE];
		lc_cal_data_eeprom.lc_macro= ((lc_eeprom_data[ONSEMI_LC_MACRO_H_BYTE]<<8)&0xff00)|
										lc_eeprom_data[ONSEMI_LC_MACRO_L_BYTE];
	}
	for(i=0;i<7;i++)
		CDBG("the lc data[%d]=0x%x\n",i, lc_eeprom_data[i]);
		CDBG("%s bias=0x%x,offset=0x%x,macro pos=0x%x, infinity pos=0x%x, checksum=%d\n",__func__,
		lc_cal_data_eeprom.lc_bias,
		lc_cal_data_eeprom.lc_offset,
		lc_cal_data_eeprom.lc_macro,
		lc_cal_data_eeprom.lc_infinity,
		lc_eeprom_data[6]);
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x28,
									lc_cal_data_eeprom.lc_offset,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write lc_bias fail\n", __func__, __LINE__);
		return rc;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x29,
									lc_cal_data_eeprom.lc_bias,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write lc_offset fail\n", __func__, __LINE__);
		return rc;
	}
	rc=a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
									&a_ctrl->i2c_client,
									0x3C,
									&val,
									MSM_CAMERA_I2C_WORD_DATA);
	if(rc<0)
	{
		printk("%s : %d read direct move target pos fail\n", __func__, __LINE__);
		return rc;
	}
	CDBG("%s : current default pos value=0x%x\n", __func__,val );
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x18,
									val,
									MSM_CAMERA_I2C_WORD_DATA);
	if(rc<0)
	{
		printk("%s : %d write step move start pos fail\n", __func__, __LINE__);
		return rc;
	}
	inital_pos_val = val;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x5A,
									0x0800,
									MSM_CAMERA_I2C_WORD_DATA);
	if(rc<0)
	{
		printk("%s : %d  step move par setting ram fail\n", __func__, __LINE__);
		return rc;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x83,
									0xAC,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write step move par setting reg fail\n", __func__, __LINE__);
		return rc;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0xA0,
									0x02,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write step move curret limitation(126us) fail\n", __func__, __LINE__);
		return rc;
	}
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&a_ctrl->i2c_client,
									0x87,
									0x85,
									MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
	{
		printk("%s : %d write step move par setting reg fail\n", __func__, __LINE__);
		return rc;
	}
	return 0;
}
#endif
static int32_t msm_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, struct reg_settings_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	enum msm_camera_i2c_reg_addr_type save_addr_type;
	CDBG("Enter\n");

	save_addr_type = a_ctrl->i2c_client.addr_type;
	for (i = 0; i < size; i++) {

		switch (settings[i].addr_type) {
		case MSM_ACTUATOR_BYTE_ADDR:
			a_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
			break;
		case MSM_ACTUATOR_WORD_ADDR:
			a_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
			break;
		default:
			pr_err("Unsupport addr type: %d\n",
				settings[i].addr_type);
			break;
		}

		switch (settings[i].i2c_operation) {
		case MSM_ACT_WRITE:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data,
				settings[i].data_type);
			break;
		case MSM_ACT_POLL:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data,
				settings[i].data_type);
			break;
		default:
			pr_err("Unsupport i2c_operation: %d\n",
				settings[i].i2c_operation);
			break;

		if (0 != settings[i].delay)
			msleep(settings[i].delay);

		if (rc < 0)
			break;
		}
	}

	a_ctrl->curr_step_pos = 0;
	/*
	 * Recover register addr_type after the init
	 * settings are written.
	 */
	a_ctrl->i2c_client.addr_type = save_addr_type;
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int16_t next_lens_pos = 0;
	uint16_t damping_code_step = 0;
	uint16_t wait_time = 0;
	CDBG("Enter\n");

	damping_code_step = damping_params->damping_step;
	wait_time = damping_params->damping_delay;

	/* Write code based on damping_code_step in a loop */
	for (next_lens_pos =
		curr_lens_pos + (sign_direction * damping_code_step);
		(sign_direction * next_lens_pos) <=
			(sign_direction * code_boundary);
		next_lens_pos =
			(next_lens_pos +
				(sign_direction * damping_code_step))) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_pos, damping_params->hw_params, wait_time);
		curr_lens_pos = next_lens_pos;
	}

	if (curr_lens_pos != code_boundary) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
	}
	CDBG("Exit\n");
}

static int32_t msm_actuator_piezo_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t dest_step_position = move_params->dest_step_pos;
	struct damping_params_t ringing_params_kernel;
	int32_t rc = 0;
	int32_t num_steps = move_params->num_steps;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (copy_from_user(&ringing_params_kernel,
		&(move_params->ringing_params[0]),
		sizeof(struct damping_params_t))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	if (num_steps == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
		(num_steps *
		a_ctrl->region_params[0].code_per_step),
		ringing_params_kernel.hw_params, 0);

	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->curr_step_pos = dest_step_position;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct damping_params_t ringing_params_kernel;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	uint16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	uint16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;
	struct msm_camera_i2c_reg_setting reg_setting;

	if (copy_from_user(&ringing_params_kernel,
		&(move_params->ringing_params[a_ctrl->curr_region_index]),
		sizeof(struct damping_params_t))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}


	CDBG("called, dir %d, num_steps %d\n", dir, num_steps);

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	if ((sign_dir > MSM_ACTUATOR_MOVE_SIGNED_NEAR) ||
		(sign_dir < MSM_ACTUATOR_MOVE_SIGNED_FAR)) {
		pr_err("Invalid sign_dir = %d\n", sign_dir);
		return -EFAULT;
	}
	if ((dir > MOVE_FAR) || (dir < MOVE_NEAR)) {
		pr_err("Invalid direction = %d\n", dir);
		return -EFAULT;
	}
	if (dest_step_pos > a_ctrl->total_steps) {
		pr_err("Step pos greater than total steps = %d\n",
		dest_step_pos);
		return -EFAULT;
	}
	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->i2c_tbl_index = 0;
	CDBG("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel,
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

		} else {
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel,
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
		}
		a_ctrl->curr_step_pos = target_step_pos;
	}

	move_params->curr_lens_pos = curr_lens_pos;
	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	CDBG("Exit\n");

	return rc;
}


static int32_t msm_actuator_park_lens(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	uint16_t next_lens_pos = 0;
	struct msm_camera_i2c_reg_setting reg_setting;

	a_ctrl->i2c_tbl_index = 0;
	if ((a_ctrl->curr_step_pos > a_ctrl->total_steps) ||
		(!a_ctrl->park_lens.max_step) ||
		(!a_ctrl->step_position_table) ||
		(!a_ctrl->i2c_reg_tbl) ||
		(!a_ctrl->func_tbl) ||
		(!a_ctrl->func_tbl->actuator_parse_i2c_params)) {
		pr_err("%s:%d Failed to park lens.\n",
			__func__, __LINE__);
		return 0;
	}

	if (a_ctrl->park_lens.max_step > a_ctrl->max_code_size)
		a_ctrl->park_lens.max_step = a_ctrl->max_code_size;

	next_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	while (next_lens_pos) {
		next_lens_pos = (next_lens_pos > a_ctrl->park_lens.max_step) ?
			(next_lens_pos - a_ctrl->park_lens.max_step) : 0;
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_pos, a_ctrl->park_lens.hw_params,
			a_ctrl->park_lens.damping_delay);

		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		reg_setting.data_type = a_ctrl->i2c_data_type;

		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n",
				__func__, __LINE__);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		/* Use typical damping time delay to avoid tick sound */
		usleep_range(10000, 12000);
	}

	return 0;
}

static int32_t msm_actuator_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	uint32_t qvalue = 0;
	int16_t cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");

	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	a_ctrl->max_code_size = max_code_size;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}
	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		qvalue =
			a_ctrl->region_params[region_index].qvalue;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		for (; step_index <= step_boundary; step_index++) {
			if ( qvalue > 1 && qvalue <= MAX_QVALUE)
				cur_code = step_index * code_per_step / qvalue;
			else
				cur_code = step_index * code_per_step;
			cur_code += set_info->af_tuning_params.initial_code;
			if (cur_code < max_code_size){
				a_ctrl->step_position_table[step_index] =
					cur_code;
			} else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
			CDBG("step_position_table [%d] %d\n", step_index,
			a_ctrl->step_position_table[step_index]);
		}
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	CDBG("Enter\n");

	if (a_ctrl->curr_step_pos != 0)
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl, move_params);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_vreg_control(struct msm_actuator_ctrl_t *a_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_actuator_vreg *vreg_cfg;
	struct device *dev = NULL;

	vreg_cfg = &a_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_ACTUATOT_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	if (a_ctrl->act_device_type == MSM_CAMERA_I2C_DEVICE)
		dev = &(a_ctrl->i2c_client.client->dev);
	else if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		dev = &(a_ctrl->pdev->dev);

	if (dev == NULL) {
		pr_err("%s:a_ctrl device structure got corrupted\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(dev,
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	if (a_ctrl->actuator_state != ACTUATOR_POWER_DOWN) {

		if (a_ctrl->func_tbl && a_ctrl->func_tbl->actuator_park_lens) {
			rc = a_ctrl->func_tbl->actuator_park_lens(a_ctrl);
			if (rc < 0)
				pr_err("%s:%d Lens park failed.\n",
					__func__, __LINE__);
		}

		rc = msm_actuator_vreg_control(a_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		kfree(a_ctrl->step_position_table);
		a_ctrl->step_position_table = NULL;
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->actuator_state = ACTUATOR_POWER_DOWN;
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint16_t next_lens_position;
	uint16_t delay;
	uint32_t hw_params = 0;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("%s Enter %d\n", __func__, __LINE__);
	if (set_pos->number_of_steps  == 0)
		return rc;

	a_ctrl->i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		next_lens_position = set_pos->pos[index];
		delay = set_pos->delay[index];
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
		next_lens_position, hw_params, delay);

		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		reg_setting.data_type = a_ctrl->i2c_data_type;

		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n",
				__func__, __LINE__);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
	}
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

/*xuhx1 for tof begin */
#ifdef CONFIG_LENOVO_AF_LASER
static int msm_tof_power_up (struct msm_actuator_ctrl_t *a_ctrl)
{
    int rc = 0;
    uint16_t sid = 0;
    struct msm_camera_i2c_client *sensor_i2c_client;
    CDBG("Enter");
    LASER_STATUS = 0;	
    if (!a_ctrl) {
        pr_err("%s:%d failed: %p\n", __func__, __LINE__, a_ctrl);
        return -EINVAL;
    }
    sensor_i2c_client = &a_ctrl->i2c_client;

    if (!sensor_i2c_client) {
        pr_err("%s:%d failed: %p\n", __func__, __LINE__, sensor_i2c_client);
        return -EINVAL;
    }

    sid = sensor_i2c_client->cci_client->sid;
    sensor_i2c_client->cci_client->sid = TOF_SLAVE_ADDR;
    
    rc = VL6180x_InitData(sensor_i2c_client);
    if (rc < 0)
        pr_err("IniData fail");
    
    if (rc == CALIBRATION_WARNING)
        CDBG("calibration warning!!\n");
    
    rc = VL6180x_FilterSetState(sensor_i2c_client, 1); /* activate wrap around filter */
    if (rc < 0)
        pr_err("filter set fail");
    
    //status = VL6180x_DisableGPIOxOut(sensor_i2c_client, 1); /* diable gpio 1 output, not needed when polling */
    //if (status < 0)
    //CDBG("disable gpio fail");

    rc = VL6180x_Prepare(sensor_i2c_client);
    if (rc< 0)
        pr_err("prepare fail");
    
    rc =VL6180x_RangeSetSystemMode(sensor_i2c_client, MODE_START_STOP|MODE_SINGLESHOT);
    if (rc < 0)
        pr_err("range set fail");
    
    //for (i = 0; i < TOF_INIT_SIZE; i++) {
    //    sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, Tof_init_setting[i].addr, Tof_init_setting[i].data, MSM_CAMERA_I2C_BYTE_DATA);
    //}
    sensor_i2c_client->cci_client->sid = sid;

    if (rc >= 0){
        LASER_STATUS = 1;
        pr_err("%s:%d LASER hardware init ok \n", __func__, __LINE__);
    }

    return rc;
}

static int msm_tof_do_calib(struct msm_actuator_ctrl_t *a_ctrl, struct msm_actuator_cfg_data *cdata)
{
    int rc = 0;
    uint16_t sid = 0, crosstalk = 0;
    uint8_t offset = 0;
    int data = 0;

    struct msm_camera_i2c_client *sensor_i2c_client;
    CDBG("Enter");

    if (!a_ctrl) {
        pr_err("%s:%d failed: %p\n", __func__, __LINE__, a_ctrl);
        return -EINVAL;
    }
    sensor_i2c_client = &a_ctrl->i2c_client;

    if (!sensor_i2c_client) {
        pr_err("%s:%d failed: %p\n", __func__, __LINE__, sensor_i2c_client);
        return -EINVAL;
    }

    sid = sensor_i2c_client->cci_client->sid;
    sensor_i2c_client->cci_client->sid = TOF_SLAVE_ADDR;

    //offset calibration
    data = (int8_t)(cdata->cfg.laser_calib.offset);
    if(data>0x7f)
    {
        data = data -0xFF;
    }
    data = data/3;//upscaling for offset 3
    rc = VL6180x_WrByte(sensor_i2c_client, 0x24, data );
    printk("%s: cdata.cfg.laser_calib.offset>>> = %d\n", __func__, cdata->cfg.laser_calib.offset);

    rc = VL6180x_RdByte(sensor_i2c_client, 0x24, &offset );
    printk("%s: cdata.cfg.laser_calib.offset<<<= %d\n", __func__, offset);

     //crosstalk calibration
    data = cdata->cfg.laser_calib.crosstalk;
    rc = VL6180x_WrWord(sensor_i2c_client, 0x1e, data);
    printk("%s: cdata.cfg.laser_calib.crosstalk>>> = %d\n", __func__, cdata->cfg.laser_calib.crosstalk);


    rc = VL6180x_RdWord(sensor_i2c_client, 0x1e, &crosstalk);
    printk("%s: cdata.cfg.laser_calib.crosstalk<<<= %d\n", __func__, crosstalk);

    sensor_i2c_client->cci_client->sid = sid;

    return rc;
}
#endif
/*xuhx1 for tof end */

static int32_t msm_actuator_set_param(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info) {
	struct reg_settings_t *init_settings = NULL;
	int32_t rc = -EFAULT;
	uint16_t i = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	for (i = 0; i < ARRAY_SIZE(actuators); i++) {
		if (set_info->actuator_params.act_type ==
			actuators[i]->act_type) {
			a_ctrl->func_tbl = &actuators[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("Actuator function table not found\n");
		return rc;
	}
	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}
	if (set_info->af_tuning_params.region_size
		> MAX_ACTUATOR_REGION) {
		pr_err("MAX_ACTUATOR_REGION is exceeded.\n");
		return -EFAULT;
	}

	a_ctrl->region_size = set_info->af_tuning_params.region_size;
	a_ctrl->pwd_step = set_info->af_tuning_params.pwd_step;
	a_ctrl->total_steps = set_info->af_tuning_params.total_steps;

	if (copy_from_user(&a_ctrl->region_params,
		(void *)set_info->af_tuning_params.region_params,
		a_ctrl->region_size * sizeof(struct region_params_t)))
		return -EFAULT;

	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = a_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->actuator_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = a_ctrl->cci_master;
	} else {
		a_ctrl->i2c_client.client->addr =
			set_info->actuator_params.i2c_addr;
	}

	a_ctrl->i2c_data_type = set_info->actuator_params.i2c_data_type;
	a_ctrl->i2c_client.addr_type = set_info->actuator_params.i2c_addr_type;
	if (set_info->actuator_params.reg_tbl_size <=
		MAX_ACTUATOR_REG_TBL_SIZE) {
		a_ctrl->reg_tbl_size = set_info->actuator_params.reg_tbl_size;
	} else {
		a_ctrl->reg_tbl_size = 0;
		pr_err("MAX_ACTUATOR_REG_TBL_SIZE is exceeded.\n");
		return -EFAULT;
	}

	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
	a_ctrl->i2c_reg_tbl =
		kmalloc(sizeof(struct msm_camera_i2c_reg_array) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (!a_ctrl->i2c_reg_tbl) {
		pr_err("kmalloc fail\n");
		return -ENOMEM;
	}

	if (copy_from_user(&a_ctrl->reg_tbl,
		(void *)set_info->actuator_params.reg_tbl_params,
		a_ctrl->reg_tbl_size *
		sizeof(struct msm_actuator_reg_params_t))) {
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		return -EFAULT;
	}

	if (set_info->actuator_params.init_setting_size &&
		set_info->actuator_params.init_setting_size
		<= MAX_ACTUATOR_INIT_SET) {
		if (a_ctrl->func_tbl->actuator_init_focus) {
			init_settings = kmalloc(sizeof(struct reg_settings_t) *
				(set_info->actuator_params.init_setting_size),
				GFP_KERNEL);
			if (init_settings == NULL) {
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error allocating memory for init_settings\n");
				return -EFAULT;
			}
			if (copy_from_user(init_settings,
				(void *)set_info->actuator_params.init_settings,
				set_info->actuator_params.init_setting_size *
				sizeof(struct reg_settings_t))) {
				kfree(init_settings);
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error copying init_settings\n");
				return -EFAULT;
			}
			rc = a_ctrl->func_tbl->actuator_init_focus(a_ctrl,
				set_info->actuator_params.init_setting_size,
				init_settings);
			kfree(init_settings);
			if (rc < 0) {
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error actuator_init_focus\n");
				return -EFAULT;
			}
		}
	}

	/* Park lens data */
	a_ctrl->park_lens = set_info->actuator_params.park_lens;
	a_ctrl->initial_code = set_info->af_tuning_params.initial_code;
	if (a_ctrl->func_tbl->actuator_init_step_table)
		rc = a_ctrl->func_tbl->
			actuator_init_step_table(a_ctrl, set_info);

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
	if (a_ctrl->cam_name == ACTUATOR_NAME && need_init_lc == 1) {
	    actuator_ctrl = a_ctrl;
		msm_actuator_init_lc_tune_data(a_ctrl, set_info);
		need_init_lc = 0;
	}
#endif

	a_ctrl->curr_step_pos = 0;
	a_ctrl->curr_region_index = 0;
	CDBG("Exit rc=%d\n",rc);

	return rc;
}

static int msm_actuator_init(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_config(struct msm_actuator_ctrl_t *a_ctrl,
	void __user *argp)
{
	struct msm_actuator_cfg_data *cdata =
		(struct msm_actuator_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(a_ctrl->actuator_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_ACTUATOR_INIT:
		rc = msm_actuator_init(a_ctrl);
    #ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
		need_init_lc = 1;
	#endif
		if (rc < 0)
			pr_err("msm_actuator_init failed %d\n", rc);
		break;
	case CFG_GET_ACTUATOR_INFO:
		cdata->is_af_supported = 1;
		cdata->cfg.cam_name = a_ctrl->cam_name;
		break;

	case CFG_SET_ACTUATOR_INFO:
		rc = msm_actuator_set_param(a_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("init table failed %d\n", rc);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc = a_ctrl->func_tbl->actuator_set_default_focus(a_ctrl,
			&cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;

	case CFG_MOVE_FOCUS:
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl,
			&cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;
	case CFG_ACTUATOR_POWERDOWN:
		rc = msm_actuator_power_down(a_ctrl);
		if (rc < 0)
			pr_err("msm_actuator_power_down failed %d\n", rc);
		break;

	case CFG_SET_POSITION:
		rc = a_ctrl->func_tbl->actuator_set_position(a_ctrl,
			&cdata->cfg.setpos);
		if (rc < 0)
			pr_err("actuator_set_position failed %d\n", rc);
		break;

	case CFG_ACTUATOR_POWERUP:
		rc = msm_actuator_power_up(a_ctrl);
		if (rc < 0)
			pr_err("Failed actuator power up%d\n", rc);
		break;

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
    /*+Begin: for set oismode */
	case CFG_SET_OISMODE:
		cam_mode = cdata->cfg.cammode;
		CDBG("%s CFG_SET_OISMODE cam_mode=%d\n", __func__,cam_mode);
        if (OIS_STATUS == 1) {
            //ois init success
            pr_err("%s set oismode = %d\n",__func__, cam_mode);

            if (cam_mode == 1) //video
        	    SetH1cMod(0xff); /* Lvl Change Active mode */
        	else               //preview
       	        SetH1cMod(0x00);
    	}
		break;
    /*+End.*/

    /* +begin ljk add command to enable/disable OIS*/
       case CFG_SET_OIS_ENABLE:
            if (OIS_STATUS == 1) {
                //ois init success
                pr_err("%s set ois_enable = %d\n", __func__, cdata->cfg.ois_enable);
                if (cdata->cfg.ois_enable == 0x01) {
                    OisEna();
                } else {
                    OisOff();
                }
            }
    	   	break;
    /*+End.*/
#endif


#ifdef CONFIG_LENOVO_AF_LASER
/* +begin xujt1 add command to init laser 2015-01-19*/
	case CFG_LASER_INIT:
		rc = msm_tof_power_up(a_ctrl);
		if (rc < 0)
		pr_err("msm_tof_power_up failed %d\n", rc);
		break;
	case CFG_LASER_CALIB:
		rc = msm_tof_do_calib(a_ctrl,cdata);
		if (rc < 0)
		pr_err("msm_tof_do_calib failed %d\n", rc);
		break;
/* +end xujt1 add command to init laser 2015-01-19*/
#endif

	default:
		break;
	}
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_get_subdev_id(struct msm_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = a_ctrl->pdev->id;
	else
		*subdev_id = a_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_actuator_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
    OIS_STATUS = 0;
    pr_err("%s oismode = %d\n",__func__, cam_mode);
#endif

	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_actuator_internal_ops = {
	.close = msm_actuator_close,
};

static long msm_actuator_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_actuator_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_actuator_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_ACTUATOR_CFG:
		return msm_actuator_config(a_ctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		msm_actuator_close(sd, NULL);
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_actuator_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_actuator_cfg_data32 *u32 =
		(struct msm_actuator_cfg_data32 *)arg;
	struct msm_actuator_cfg_data actuator_data;
	void *parg = arg;
	long rc;

	switch (cmd) {
	case VIDIOC_MSM_ACTUATOR_CFG32:
		cmd = VIDIOC_MSM_ACTUATOR_CFG;
		switch (u32->cfgtype) {
		case CFG_SET_ACTUATOR_INFO:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.set_info.actuator_params.act_type =
				u32->cfg.set_info.actuator_params.act_type;

			actuator_data.cfg.set_info.actuator_params
				.reg_tbl_size =
				u32->cfg.set_info.actuator_params.reg_tbl_size;

			actuator_data.cfg.set_info.actuator_params.data_size =
				u32->cfg.set_info.actuator_params.data_size;

			actuator_data.cfg.set_info.actuator_params
				.init_setting_size =
				u32->cfg.set_info.actuator_params
				.init_setting_size;

			actuator_data.cfg.set_info.actuator_params.i2c_addr =
				u32->cfg.set_info.actuator_params.i2c_addr;

			actuator_data.cfg.set_info.actuator_params
				.i2c_addr_type =
				u32->cfg.set_info.actuator_params.i2c_addr_type;

			actuator_data.cfg.set_info.actuator_params
				.i2c_data_type =
				u32->cfg.set_info.actuator_params.i2c_data_type;

			actuator_data.cfg.set_info.actuator_params
				.reg_tbl_params =
				compat_ptr(
				u32->cfg.set_info.actuator_params
				.reg_tbl_params);

			actuator_data.cfg.set_info.actuator_params
				.init_settings =
				compat_ptr(
				u32->cfg.set_info.actuator_params
				.init_settings);

			actuator_data.cfg.set_info.af_tuning_params
				.initial_code =
				u32->cfg.set_info.af_tuning_params.initial_code;

			actuator_data.cfg.set_info.af_tuning_params.pwd_step =
				u32->cfg.set_info.af_tuning_params.pwd_step;

			actuator_data.cfg.set_info.af_tuning_params
				.region_size =
				u32->cfg.set_info.af_tuning_params.region_size;

			actuator_data.cfg.set_info.af_tuning_params
				.total_steps =
				u32->cfg.set_info.af_tuning_params.total_steps;

			actuator_data.cfg.set_info.af_tuning_params
				.region_params = compat_ptr(
				u32->cfg.set_info.af_tuning_params
				.region_params);

			actuator_data.cfg.set_info.actuator_params.park_lens =
				u32->cfg.set_info.actuator_params.park_lens;

			parg = &actuator_data;
			break;
		case CFG_SET_DEFAULT_FOCUS:
		case CFG_MOVE_FOCUS:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.move.dir = u32->cfg.move.dir;

			actuator_data.cfg.move.sign_dir =
				u32->cfg.move.sign_dir;

			actuator_data.cfg.move.dest_step_pos =
				u32->cfg.move.dest_step_pos;

			actuator_data.cfg.move.num_steps =
				u32->cfg.move.num_steps;

			actuator_data.cfg.move.curr_lens_pos =
				u32->cfg.move.curr_lens_pos;

			actuator_data.cfg.move.ringing_params =
				compat_ptr(u32->cfg.move.ringing_params);
			parg = &actuator_data;
			break;
		case CFG_SET_POSITION:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			memcpy(&actuator_data.cfg.setpos, &(u32->cfg.setpos),
				sizeof(struct msm_actuator_set_position_t));
			break;

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
/* +begin add ois by ljk*/
		case CFG_SET_OISMODE:
		CDBG("%s:%d CFG_SET_OISMODE32  cammode=%d\n", __func__, __LINE__,u32->cfg.cammode);

			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.cammode = u32->cfg.cammode;

			parg = &actuator_data;
			break;
		case CFG_SET_OIS_ENABLE:
		CDBG("%s:%d CFG_SET_OIS_ENABLE32  ois_enable=%d\n", __func__, __LINE__,u32->cfg.ois_enable);

			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.ois_enable = u32->cfg.ois_enable;

			parg = &actuator_data;
			break;
		case CFG_LASER_CALIB:

			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.cfg.laser_calib.offset = u32->cfg.laser_calib.offset;
			actuator_data.cfg.laser_calib.crosstalk = u32->cfg.laser_calib.crosstalk;
			pr_err("%s: CFG_LASER_CALIB32  offset = %d,crosstalk = %d \n", __func__, 
				  actuator_data.cfg.laser_calib.offset,actuator_data.cfg.laser_calib.crosstalk);
			parg = &actuator_data;
			break;
/* +end add ois by ljk*/
#endif
		default:
			actuator_data.cfgtype = u32->cfgtype;
			parg = &actuator_data;
			break;
		}
	}

	rc = msm_actuator_subdev_ioctl(sd, cmd, parg);

	switch (cmd) {

	case VIDIOC_MSM_ACTUATOR_CFG:

		switch (u32->cfgtype) {

		case CFG_SET_DEFAULT_FOCUS:
		case CFG_MOVE_FOCUS:
			u32->cfg.move.curr_lens_pos =
				actuator_data.cfg.move.curr_lens_pos;
			break;
		default:
			break;
		}
	}

	return rc;
}

static long msm_actuator_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_actuator_subdev_do_ioctl);
}
#endif

static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	rc = msm_actuator_vreg_control(a_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	a_ctrl->actuator_state = ACTUATOR_POWER_UP;

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	mutex_lock(a_ctrl->actuator_mutex);
	if (on)
		rc = msm_actuator_power_up(a_ctrl);
	else
		rc = msm_actuator_power_down(a_ctrl);
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_actuator_subdev_core_ops = {
	.ioctl = msm_actuator_subdev_ioctl,
	.s_power = msm_actuator_power,
};

static struct v4l2_subdev_ops msm_actuator_subdev_ops = {
	.core = &msm_actuator_subdev_core_ops,
};

static const struct i2c_device_id msm_actuator_i2c_id[] = {
	{"qcom,actuator", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_actuator_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *act_ctrl_t = NULL;
	struct msm_actuator_vreg *vreg_cfg = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_actuator_i2c_probe: client is null\n");
		return -EINVAL;
	}

	act_ctrl_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
		GFP_KERNEL);
	if (!act_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%p\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&act_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", act_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	if (of_find_property(client->dev.of_node,
		"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &act_ctrl_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data(client->dev.of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto probe_failure;
		}
	}

	act_ctrl_t->i2c_driver = &msm_actuator_i2c_driver;
	act_ctrl_t->i2c_client.client = client;
	act_ctrl_t->curr_step_pos = 0,
	act_ctrl_t->curr_region_index = 0,
	act_ctrl_t->actuator_state = ACTUATOR_POWER_DOWN;
	/* Set device type as I2C */
	act_ctrl_t->act_device_type = MSM_CAMERA_I2C_DEVICE;
	act_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	act_ctrl_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	act_ctrl_t->actuator_mutex = &msm_actuator_mutex;
	act_ctrl_t->cam_name = act_ctrl_t->subdev_id;
	CDBG("act_ctrl_t->cam_name: %d", act_ctrl_t->cam_name);
	/* Assign name for sub device */
	snprintf(act_ctrl_t->msm_sd.sd.name, sizeof(act_ctrl_t->msm_sd.sd.name),
		"%s", act_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&act_ctrl_t->msm_sd.sd,
		act_ctrl_t->i2c_client.client,
		act_ctrl_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&act_ctrl_t->msm_sd.sd, act_ctrl_t);
	act_ctrl_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	act_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&act_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	act_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	act_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	act_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&act_ctrl_t->msm_sd);
	msm_actuator_v4l2_subdev_fops = v4l2_subdev_fops;

#ifdef CONFIG_COMPAT
	msm_actuator_v4l2_subdev_fops.compat_ioctl32 =
		msm_actuator_subdev_fops_ioctl;
#endif
	act_ctrl_t->msm_sd.sd.devnode->fops =
		&msm_actuator_v4l2_subdev_fops;

	pr_info("msm_actuator_i2c_probe: succeeded\n");
	CDBG("Exit\n");

	return 0;

probe_failure:
	kfree(act_ctrl_t);
	return rc;
}

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
ssize_t proc_ois_write (struct file *file, const char __user *buf, size_t nbytes, loff_t *ppos)
{
    char string[nbytes];
    static int flag = 0;
    sscanf(buf, "%s", string);
    if (!strcmp ((const char*)string, (const char*)"on"))
    {
            if (flag == 1)
            {
        	    CDBG("ois on called +\n");
                OisEna();           // OIS enable
        	    CDBG("ois on called -\n");
                flag = 0;
    	    }
    }
    else if (!strcmp((const char *)string, (const char *)"off"))
    {
            if (flag == 0)
            {
        	    CDBG("ois off called + \n");
            	OisOff();
        	    CDBG("ois off called -\n");
                flag = 1;
    	    }
    }
    return nbytes;
}

const struct file_operations proc_ois_operations = {
	.owner	= THIS_MODULE,
	.write	= proc_ois_write,
};
#endif

#ifdef CONFIG_LENOVO_AF_LASER

/*+begin add cci release interface for laser calibration xujt1 2015-05-15 */
static int msm_actuator_release_cci(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_release failed\n");
	}
	CDBG("Exit\n");
	return rc;
}
/*+end add cci release interface for laser calibration xujt1 2015-05-15 */

ssize_t proc_laser_write (struct file *file, const char __user *buf, size_t nbytes, loff_t *ppos)
{
    char string[nbytes + 1];
    static int flag = 0;
    	int rc = 0;
    memset(string, 0, sizeof(char) *(nbytes + 1));
    sscanf(buf, "%s", string);
    string[nbytes] = '\0';
    pr_err("%s -> %s, nbytes = %d \n",__func__,string,(int)nbytes);
    if (!strcmp((const char *)string, (const char *)"on"))
    {
         //   if(flag == 1)
            {
                    actuator_ctrl->i2c_client.cci_client->retries = 3;
		    actuator_ctrl->i2c_client.cci_client->id_map = 0;
		    actuator_ctrl->i2c_client.cci_client->cci_i2c_master = 0;
        	    CDBG("laser on called +   \n");
		    rc = msm_actuator_init(actuator_ctrl);
                    rc = msm_actuator_vreg_control(actuator_ctrl, 1);
                	if (rc < 0) {
                		pr_err("%s failed %d\n", __func__, __LINE__);
                		return rc;
                	}
                   rc = vl6180_init(actuator_ctrl);
                   if(rc == 0)
                   {
                        LASER_STATUS = 1;
                   }

        	    CDBG("laser on called -\n");
                flag = 0;
    	    }
    }
    //else if (!strcmp((const char *)string, (const char *)"off"))
    else if (strstr((const char *)string, (const char *)"off")  != NULL)
    {
            if(flag == 0)
            {
        	    CDBG("laser off called + \n");

                    rc = msm_actuator_vreg_control(actuator_ctrl, 0);
                	if (rc < 0) {
                		pr_err("%s failed %d\n", __func__, __LINE__);
                		return rc;
                	}
                        LASER_STATUS = 0;
/*+begin add cci release interface for laser calibration xujt1 2015-05-15 */
                 //we should release cci interface here
	        msm_actuator_release_cci(actuator_ctrl);
/*+end add cci release interface for laser calibration xujt1 2015-05-15 */

        	    CDBG("laser off called -\n");
                flag = 1;
    	    }
    }
    return nbytes;
}
/*+begin add test mode read laser data by ljk 2014-12-29 */

const struct file_operations proc_laser_operations = {
	.owner	= THIS_MODULE,
	.write	= proc_laser_write,
};
/*+end add test mode read laser data by ljk 2014-12-29 */

/*+begin add laser position read by lijk3 sysfs callback function */
extern int vl6180_i2c_read_16bits(unsigned int addr, uint16_t *pdata);
extern int vl6180_i2c_write_16bits(unsigned int addr, uint16_t data);
extern int vl6180_i2c_write_byte(unsigned int addr,  uint16_t data);
extern int vl6180_i2c_read_byte(unsigned int addr,  uint16_t *pdata);

static ssize_t laser_show_position(struct device *dev, struct device_attribute *attr, char *buf)
{
        int rc = 0;

        CDBG("%s enter  \n",__func__);

        if (LASER_STATUS == 1)//laser init success
        {
                 rc = vl6180_get_data(actuator_ctrl,&laser_data);
            	if (rc < 0)
            		return rc;
            	return sprintf(buf, "%d\n", laser_data.position*3);//change by lijk3

        }
        else
        {
                   pr_err("%s LASER_STATUS == 0 not ready\n",__func__);
        }
        CDBG("%s fail exit\n",__func__);

        return 0;
}

static ssize_t laser_show_rtnrate(struct device *dev, struct device_attribute *attr, char *buf)
{
    int rc = 0;
	uint16_t rtnrate = 0;

    CDBG("%s enter  \n",__func__);

    //ois init success
    if (LASER_STATUS == 1) {
        rc = vl6180_get_data(actuator_ctrl,&laser_data);
	    vl6180_i2c_read_16bits( 0x066, &rtnrate);
        pr_err("%s rtnrate(0x66) =%d\n",__func__,rtnrate);

        if (rc < 0)
            return rc;

		return sprintf(buf, "%d\n", rtnrate);//change by lijk3
    } else {
        pr_err("%s LASER_STATUS == 0 not ready\n",__func__);
    }

    CDBG("%s fail exit\n",__func__);

    return 0;
}

static ssize_t laser_show_raw(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* If proximity work,then ALS must be enable */
    int rc = 0;
	uint16_t raw = 0;

    CDBG("%s enter  \n",__func__);

    //ois init success
    if (LASER_STATUS == 1) {
        rc = vl6180_get_data(actuator_ctrl,&laser_data);
	    vl6180_i2c_read_byte( 0x064, &raw);
        CDBG("%s raw(0x64) =%d\n",__func__,raw);

        if (rc < 0)
            return rc;

        return sprintf(buf, "%d\n", raw);//change by lijk3

    } else {
        pr_err("%s LASER_STATUS == 0 not ready\n",__func__);
    }

    CDBG("%s fail exit\n",__func__);

    return 0;
}

static ssize_t __ref laser_set_crosstalk(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int crosstalk, rc;

	rc= kstrtoint(strstrip((char *)buf), 0, &crosstalk);
    pr_err("st_Vll6180 write ----crosstalk = %d  \n",crosstalk);

	if (rc)
		return rc;

//	crosstalk = (int8_t)crosstalk;
	rc = vl6180_i2c_write_16bits( 0x1e, crosstalk );

	if (rc == 0x0) {
            pr_err("st_Vll6180 write ----reg[0x1e]  sucess calc = 0x%x  \n",crosstalk);
	} else {
	    pr_err("st_Vll6180 reading ----calc fail 1 rc=%d\n",rc);
	}

	return count;
}

static ssize_t laser_get_crosstalk(struct device *dev, struct device_attribute *attr, char *buf)
{
        int rc = 0;
	uint16_t xtk = 0;

        CDBG("%s enter  \n",__func__);

        if (LASER_STATUS == 1)//ois init success
        {
                rc = vl6180_get_data(actuator_ctrl,&laser_data);
               	rc = vl6180_i2c_read_16bits( 0x1e, &xtk  );
            	if (rc < 0)
            		return rc;
                   pr_err("%s xtk == %d\n",__func__,xtk);

            	return sprintf(buf, "%d\n", xtk);//change by lijk3

        }
        else
        {
                   pr_err("%s LASER_STATUS == 0 not ready\n",__func__);
        }
        CDBG("%s fail exit\n",__func__);

        return 0;
}


static ssize_t __ref laser_set_offset(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int offset, rc;

	rc= kstrtoint(strstrip((char *)buf), 0, &offset);
    pr_err("st_Vll6180 write ----offset = %d  \n",offset);

	if (rc)
		return rc;
        offset = (int8_t)offset;
        if(offset>0x7f)
        {
            offset = offset -0xFF;
        }
	offset = offset/3;//upscaling for offset 3
	rc = vl6180_i2c_write_byte( 0x24, offset );

	if (rc == 0x0) {
            pr_err("st_Vll6180 write ----reg[0x24]  sucess calc = 0x%x  \n", offset);
	} else {
	    pr_err("st_Vll6180 reading ----calc fail 1  rc=%d\n",rc);
	}

	return count;
}

static ssize_t laser_get_offset(struct device *dev, struct device_attribute *attr, char *buf)
{
        int rc = 0;
	uint16_t offset = 0;
        CDBG("%s enter  \n",__func__);

        if (LASER_STATUS == 1)//ois init success
        {
                rc = vl6180_get_data(actuator_ctrl,&laser_data);
               	rc = vl6180_i2c_read_byte( 0x24, &offset  );
            	if (rc < 0)
            		return rc;
                   pr_err("%s offset == %d\n",__func__,offset);

            	return sprintf(buf, "%d\n", offset*3);//change by lijk3

        }
        else
        {
                   pr_err("%s LASER_STATUS == 0 not ready\n",__func__);
        }
        CDBG("%s fail exit\n",__func__);

        return 0;
}

/*+end add laser position read by lijk3 sysfs callback function */

/*+begin add laser position read by lijk3 sysfs attributes */
static SENSOR_DEVICE_ATTR(position, S_IRUGO, laser_show_position, NULL, 0);
static SENSOR_DEVICE_ATTR(rtnrate, S_IRUGO, laser_show_rtnrate,	NULL, 0);
static SENSOR_DEVICE_ATTR(raw, S_IRUGO, laser_show_raw,	NULL, 0);
static SENSOR_DEVICE_ATTR(crosstalk, S_IRWXUGO |S_IRWXU, laser_get_crosstalk, laser_set_crosstalk, 0);
static SENSOR_DEVICE_ATTR(offset, S_IRWXUGO |S_IRWXU, laser_get_offset, laser_set_offset, 0);


static struct attribute *laser_attributes[] = {
	&sensor_dev_attr_position.dev_attr.attr,
	&sensor_dev_attr_rtnrate.dev_attr.attr,
	&sensor_dev_attr_raw.dev_attr.attr,
	&sensor_dev_attr_crosstalk.dev_attr.attr,
	&sensor_dev_attr_offset.dev_attr.attr,
	NULL
};

static const struct attribute_group laser_attr_group = {
	.attrs = laser_attributes,
};
/*+end add laser position read by lijk3 sysfs attributes */
#endif

static int32_t msm_actuator_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_actuator_ctrl_t *msm_actuator_t = NULL;
	struct msm_actuator_vreg *vreg_cfg;
#if defined(CONFIG_LENOVO_EEPROM_ONSEMI_OIS)||defined(CONFIG_LENOVO_AF_LASER)
    struct proc_dir_entry * rcdir;
#endif

#ifdef CONFIG_LENOVO_EEPROM_ONSEMI_OIS
    rcdir = proc_create_data("CTP_OIS_CTRL", S_IFREG | S_IWUGO | S_IWUSR, NULL, &proc_ois_operations, NULL);
    if (rcdir == NULL) {
        CDBG("proc_create_data CTP_OIS_CTRL fail\n");
    }
#endif

#ifdef CONFIG_LENOVO_AF_LASER
    rcdir = proc_create_data("CTP_LASER_CTRL", S_IFREG | S_IWUGO | S_IWUSR, NULL, &proc_laser_operations, NULL);
    if (rcdir == NULL) {
        CDBG("proc_create_data CTP_LASER_CTRL fail\n");
    }
	CDBG("Enter\n");
/*+begin add laser position read by lijk3 sysfs attributes */
    rc = sysfs_create_group(&pdev->dev.kobj, &laser_attr_group);
    if (rc) {
        CDBG("sysfs_create_group laser_attr_group fail\n");
        rc = -EINVAL;
    }
#endif
/*+end add laser position read by lijk3 sysfs attributes */

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_actuator_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
		GFP_KERNEL);
	if (!msm_actuator_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_actuator_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_actuator_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_actuator_t->cci_master, rc);
	if (rc < 0) {
		kfree(msm_actuator_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_actuator_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_actuator_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	msm_actuator_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	msm_actuator_t->actuator_mutex = &msm_actuator_mutex;
	msm_actuator_t->cam_name = pdev->id;

	/* Set platform device handle */
	msm_actuator_t->pdev = pdev;
	/* Set device type as platform device */
	msm_actuator_t->act_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_actuator_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_actuator_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_actuator_t->i2c_client.cci_client) {
		kfree(msm_actuator_t->vreg_cfg.cam_vreg);
		kfree(msm_actuator_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_actuator_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = MASTER_MAX;
	v4l2_subdev_init(&msm_actuator_t->msm_sd.sd,
		msm_actuator_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_actuator_t->msm_sd.sd, msm_actuator_t);
	msm_actuator_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	msm_actuator_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_actuator_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_actuator_t->msm_sd.sd.name), "msm_actuator");
	media_entity_init(&msm_actuator_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_actuator_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_actuator_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	msm_actuator_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_actuator_t->msm_sd);
	msm_actuator_t->actuator_state = ACTUATOR_POWER_DOWN;
	msm_actuator_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_actuator_v4l2_subdev_fops.compat_ioctl32 =
		msm_actuator_subdev_fops_ioctl;
#endif
	msm_actuator_t->msm_sd.sd.devnode->fops =
		&msm_actuator_v4l2_subdev_fops;
	actuator_ctrl = msm_actuator_t; //add for laser node

	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_actuator_i2c_dt_match[] = {
	{.compatible = "qcom,actuator"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_actuator_i2c_dt_match);

static struct i2c_driver msm_actuator_i2c_driver = {
	.id_table = msm_actuator_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(msm_actuator_i2c_remove),
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = msm_actuator_i2c_dt_match,
	},
};

static const struct of_device_id msm_actuator_dt_match[] = {
	{.compatible = "qcom,actuator", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_actuator_dt_match);

static struct platform_driver msm_actuator_platform_driver = {
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = msm_actuator_dt_match,
	},
};

static int __init msm_actuator_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(&msm_actuator_platform_driver,
		msm_actuator_platform_probe);
	if (!rc)
		return rc;

	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_actuator_i2c_driver);
}

static struct msm_actuator msm_vcm_actuator_table = {
	.act_type = ACTUATOR_VCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_set_position,
		.actuator_park_lens = msm_actuator_park_lens,
	},
};

static struct msm_actuator msm_piezo_actuator_table = {
	.act_type = ACTUATOR_PIEZO,
	.func_tbl = {
		.actuator_init_step_table = NULL,
		.actuator_move_focus = msm_actuator_piezo_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus =
			msm_actuator_piezo_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_park_lens = NULL,
	},
};

static struct msm_actuator msm_hvcm_actuator_table = {
	.act_type = ACTUATOR_HVCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_set_position,
		.actuator_park_lens = msm_actuator_park_lens,
	},
};

module_init(msm_actuator_init_module);
MODULE_DESCRIPTION("MSM ACTUATOR");
MODULE_LICENSE("GPL v2");
