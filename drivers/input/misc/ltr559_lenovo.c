/* ltr559.c
 * LTR-On LTR-559 Proxmity and Light sensor driver
 *
 * Copyright (C) 2011 Lite-On Technology Corp (Singapore)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 *  2011-11-18 Thundersoft porting to MSM7x27A platform.
 *  2011-05-01 Lite-On created base driver.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
//#include <mach/gpio.h>
//#include <linux/earlysuspend.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
//#include "ltr559.h"
#include <linux/input/ltr559.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include <linux/regulator/consumer.h>
/*lenovo-sw caoyi add wake lock for Psensor 20150505 begin*/
#include <linux/wakelock.h>
/*lenovo-sw caoyi add wake lock for Psensor 20150505 end*/

#define SENSOR_NAME			"proximity"
#define LTR559_DRV_NAME		"ltr559"
#define LTR559_MANUFAC_ID	0x05

/*added by fully 20141210*/
#define I2C_RETRY 5
#define ALS_MAX_MEASURE_VAL	65535
#define ALS_VALID_MEASURE_MASK	ALS_MAX_MEASURE_VAL
#define ALS_GAIN_1x		(0 << 2)
//#define ALS_GAIN_2x		(1 << 2)
//#define ALS_GAIN_4x		(2 << 2)
#define ALS_GAIN_8x		(3 << 2)
/*lenovo-sw caoyi1 modify for als 20150720 begin*/
#define ALS_GAIN_48x		(6 << 2)
/*lenovo-sw caoyi1 modify for als 20150720 end*/

#define VENDOR_NAME				"lite-on"
#define LTR559_SENSOR_NAME		"ltr559als"
#define DRIVER_VERSION		"1.0"
#define LTR559_PS_CALIBERATE
//#define LTR559_PS_DYNAMIC_CALIBERATE

struct ltr559_data {

	struct i2c_client *client;
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;

	/* pinctrl data*/
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;

	struct ltr559_platform_data *platform_data;

	/* regulator data */
	bool power_state;
	struct regulator *vdd;
	struct regulator *vio;

	/* interrupt type is level-style */
	struct mutex lockw;
	struct mutex op_lock;

	struct delayed_work ps_work;
	struct delayed_work als_work;

	u8 ps_open_state;
	u8 als_open_state;

	u16 irq;

	u32 ps_state;
	u32 last_lux;
#ifdef LTR559_PS_CALIBERATE
	bool cali_update;
	bool cali_status;
#endif
       u32 dynamic_noise;
};

struct ltr559_data *sensor_info;

struct ltr559_reg {
	const char *name;
	u8 addr;
	u16 defval;
	u16 curval;
};

enum ltr559_reg_tbl{
	REG_ALS_CONTR,
	REG_PS_CONTR,
	REG_ALS_PS_STATUS,
	REG_INTERRUPT,
	REG_PS_LED,
	REG_PS_N_PULSES,
	REG_PS_MEAS_RATE,
	REG_ALS_MEAS_RATE,
	REG_MANUFACTURER_ID,
	REG_INTERRUPT_PERSIST,
	REG_PS_THRES_LOW,
	REG_PS_THRES_UP,
	REG_ALS_THRES_LOW,
	REG_ALS_THRES_UP,
	REG_ALS_DATA_CH1,
	REG_ALS_DATA_CH0,
	REG_PS_DATA
};

uint16_t lux_val_prev = 0;
/*lenovo-sw caoyi1 modify for Lsensor parameter 20150430 begin*/
uint16_t winfac1 = 100;//73//110
uint16_t winfac2 = 100;//112//110
uint16_t winfac3 = 70;//31//70
/*lenovo-sw caoyi1 modify for Lsensor parameter 20150430 end*/
uint8_t eqn_prev = 0;
uint8_t ratio_old = 0;

/*lenovo-sw caoyi add wake lock for Psensor 20150505 begin*/
static struct wake_lock ps_lock;
/*lenovo-sw caoyi add wake lock for Psensor 20150505 end*/

/*lenovo-sw caoyi1 modify for report als data fast 20150629 begin*/
bool als_first_data = true;
/*lenovo-sw caoyi1 modify for report als data fast 20150629 end*/


static int ltr559_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);
static int ltr559_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);
static int ltr559_als_set_poll_delay(struct ltr559_data *data,unsigned long delay);

#ifdef LTR559_PS_CALIBERATE		
static int ltr559_ps_self_caliberate(struct sensors_classdev *sensors_cdev);
#endif

static  struct ltr559_reg reg_tbl[] = {
		{
				.name   = "ALS_CONTR",
				.addr   = 0x80,
				.defval = 0x00,
/*lenovo-sw caoyi1 modify for als 20150720 begin*/
				.curval = 0x19,//0x0D -> 0x19
/*lenovo-sw caoyi1 modify for als 20150720 end*/
		},
		{
				.name = "PS_CONTR",
				.addr = 0x81,
				.defval = 0x00,
				.curval = 0x03,
		},
		{
				.name = "ALS_PS_STATUS", // read-only register
				.addr = 0x8c,
				.defval = 0x00,
				.curval = 0x00,
		},
		{
				.name = "INTERRUPT",
				.addr = 0x8f,
				.defval = 0x00,
				.curval = 0x01,
		},
		{
				.name = "PS_LED",
				.addr = 0x82,
				.defval = 0x7f,  
				.curval = 0x1b,
		},
		{
				.name = "PS_N_PULSES",
				.addr = 0x83,
				.defval = 0x01,
				.curval = 0x06,
		},
		{
				.name = "PS_MEAS_RATE",
				.addr = 0x84,
				.defval = 0x02,
				.curval = 0x00,    //0X02:100MS, 0X00:50MS
		},
		{
				.name = "ALS_MEAS_RATE",
				.addr = 0x85,
				//		.defval = 0x03,
				//		.curval = 0x03,
				.defval = 0x03,
/*lenovo-sw caoyi modify for als measurement time change to 50ms 20150713 begin*/
				.curval = 0x08,	//50ms
/*lenovo-sw caoyi modify for als measurement time change to 50ms 20150713 end*/
		},
		{
				.name = "MANUFACTURER_ID",// read-only register
				.addr = 0x87,
				.defval = 0x05,
				.curval = 0x05,
		},
		{
				.name = "INTERRUPT_PERSIST",
				.addr = 0x9e,
				.defval = 0x00,
				.curval = 0x23,
		},
		{
				.name = "PS_THRES_LOW",
				.addr = 0x92,
				.defval = 0x0000,
				.curval = 0x0000,
		},
		{
				.name = "PS_THRES_UP",
				.addr = 0x90,
				.defval = 0x07ff,
				.curval = 0x07ff,
		},
		{
				.name = "ALS_THRES_LOW",
				.addr = 0x99,
				.defval = 0x0000,
				.curval = 0x0000,
		},
		{
				.name = "ALS_THRES_UP",
				.addr = 0x97,
				.defval = 0xffff,
				//.defval = 0x0000,
				.curval = 0xffff,
		},
		{
				.name = "ALS_DATA_CH1",// read-only register
				.addr = 0x88,
				.defval = 0x0000,
				.curval = 0x0000,
		},
		{
				.name = "ALS_DATA_CH0",// read-only register
				.addr = 0x8a,
				.defval = 0x0000,
				.curval = 0x0000,
		},
		{
				.name = "PS_DATA",// read-only register
				.addr = 0x8d,
				.defval = 0x0000,
				.curval = 0x0000,
		},
};

static struct sensors_classdev sensors_light_cdev = {
	.name = "light",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "60000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "proximity",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
static int sensor_platform_hw_power_onoff(struct ltr559_data *data, bool on);
static int sensor_regulator_configure(struct ltr559_data *data, bool on);
static int sensor_regulator_power_on(struct ltr559_data *data, bool on);
int ltr559_device_init(struct i2c_client *client);
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

/*lenovo-sw caoyi1 modify for als device init fast 20150713 begin*/
int ltr559_device_init_for_als(struct i2c_client *client);
/*lenovo-sw caoyi1 modify for als device init fast 20150713 end*/

static int ltr559_ps_read(struct i2c_client *client)
{
	int psval_lo, psval_hi, psdata;
	psval_lo = i2c_smbus_read_byte_data(client, LTR559_PS_DATA_0);
	if (psval_lo < 0) {
			psdata = psval_lo;
			goto out;
	}
	psval_hi = i2c_smbus_read_byte_data(client, LTR559_PS_DATA_1);
	if (psval_hi < 0) {
			psdata = psval_hi;
			goto out;
	}
	
	psdata = ((psval_hi & 7)* 256) + psval_lo;
    //psdata = ((psval_hi&0x7)<<8) + psval_lo;
    	printk("ps_rawdata = %d\n", psdata);
    
out:
	return psdata;
}


static int ltr559_chip_reset(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, LTR559_ALS_CONTR, MODE_ALS_StdBy);//als standby mode
	ret = i2c_smbus_write_byte_data(client, LTR559_PS_CONTR, MODE_PS_StdBy);//ps standby mode
	ret = i2c_smbus_write_byte_data(client, LTR559_ALS_CONTR, 0x02);//reset
	if(ret < 0)
		printk("%s reset chip fail\n",__func__);

	return ret;
}

static void ltr559_set_ps_threshold(struct i2c_client *client, u8 addr, u16 value)
{
	i2c_smbus_write_byte_data(client, addr, (value & 0xff));
	i2c_smbus_write_byte_data(client, addr+1, (value >> 8));
}

static int ltr559_ps_enable(struct i2c_client *client, int on)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	int ret=0;
	int contr_data;

	if (on) {

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
		/* power initialization */
		if(!data->power_state)
		{
			ret = sensor_platform_hw_power_onoff(data, true);
			if (ret)
			{
				pr_err("%s: sensor_platform_hw_power_on failed!\n", __func__);
			}
			ret = ltr559_device_init(client);
		}
		/*the end added by fully*/
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

		ret = i2c_smbus_write_byte_data(client, LTR559_PS_CONTR, reg_tbl[REG_PS_CONTR].curval);
		//ret = i2c_smbus_write_byte_data(client, LTR559_INTERRUPT, reg_tbl[REG_INTERRUPT].curval);//enable irq
		if(ret<0){
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}
		contr_data = i2c_smbus_read_byte_data(client, LTR559_PS_CONTR);
		if(contr_data != reg_tbl[REG_PS_CONTR].curval){
			//data->ps_open_state = 1;
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}

		msleep(WAKEUP_DELAY);//10ms

               /*For dynamic calibrated xtalk*/
               #ifdef LTR559_PS_DYNAMIC_CALIBERATE
               ltr559_ps_self_caliberate(&data->ps_cdev);
               #endif
               //enable_irq(data->irq);
               /*For factory calibrated xtalk*/
               #ifndef LTR559_PS_DYNAMIC_CALIBERATE
/*lenovo-sw caoyi1 modify for ensure report ps state data 20151216 begin */
               ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_threshold-1);//data->platform_data->prox_hsyteresis_threshold);
               ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);//data->platform_data->prox_threshold);
/*lenovo-sw caoyi1 modify for ensure report ps state data 20151216 end */
               #endif
/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
		enable_irq(data->irq);
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/
	} else {
		//ret = i2c_smbus_write_byte_data(client, LTR559_INTERRUPT, reg_tbl[REG_INTERRUPT].defval);//disable irq
		ret = i2c_smbus_write_byte_data(client, LTR559_PS_CONTR, MODE_PS_StdBy);
		if(ret<0){
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}
		//data->ps_open_state = 0;
		contr_data = i2c_smbus_read_byte_data(client, LTR559_PS_CONTR);
		if(contr_data != reg_tbl[REG_PS_CONTR].defval){
			//data->ps_open_state = 1;
			pr_err("%s:  enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 2);
		input_sync(data->input_dev_ps);

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
		disable_irq(data->irq);

		if(!data->als_open_state)
		{
			ret = sensor_platform_hw_power_onoff(data, false);
			if (ret)
			{
				pr_err("%s: sensor_platform_hw_power_on failed!\n", __func__);
			}
		}
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/
	}
	pr_err("%s: enable=(%d) OK\n", __func__, on);
	return ret;
}

/*
 * Absent Light Sensor Congfig
 */
static int ltr559_als_enable(struct i2c_client *client, int on)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	int ret;

	if (on) {

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
		/* power initialization */
		if(!data->power_state)
		{
			ret = sensor_platform_hw_power_onoff(data, true);
			if (ret)
			{
				pr_err("%s: sensor_platform_hw_power_on failed!\n", __func__);
			}
/*lenovo-sw caoyi1 modify for als device init fast 20150713 begin*/
			ret = ltr559_device_init_for_als(client);
/*lenovo-sw caoyi1 modify for als device init fast 20150713 end*/
		}
		/*the end added by fully*/
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

		ret = i2c_smbus_write_byte_data(client, LTR559_ALS_CONTR, reg_tbl[REG_ALS_CONTR].curval);
		msleep(WAKEUP_DELAY);
		ret |= i2c_smbus_read_byte_data(client, LTR559_ALS_CONTR);
		
		cancel_delayed_work_sync(&data->als_work);
/*lenovo-sw caoyi1 modify for report als data fast 20150629 begin*/
		als_first_data = true;
		//schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(1));
/*lenovo-sw caoyi1 modify for report als data fast 20150629 end*/

	} else {
		cancel_delayed_work_sync(&data->als_work);

/*lenovo-sw caoyi1 add for report als data after enable 20150701 begin*/
			input_report_abs(data->input_dev_als, ABS_MISC, -1);
			input_sync(data->input_dev_als);
			//printk("%s input_report_abs -1\n",__func__);
/*lenovo-sw caoyi1 add for report als data after enable 20150701 end*/

		ret = i2c_smbus_write_byte_data(client, LTR559_ALS_CONTR, MODE_ALS_StdBy);

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
		if(!data->ps_open_state)
		{
			ret = sensor_platform_hw_power_onoff(data, false);
			if (ret)
			{
				pr_err("%s: sensor_platform_hw_power_on failed!\n", __func__);
			}
		}
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

	}

	pr_err("%s: enable=(%d) ret=%d\n", __func__, on, ret);
	return ret;
}

#if 0
static int ltr559_als_read(struct i2c_client *client)
{
		int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
		int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
		int luxdata;
		int ch1_co, ch0_co, ratio;

		alsval_ch1_lo = i2c_smbus_read_byte_data(client, LTR559_ALS_DATA_CH1_0);
		alsval_ch1_hi = i2c_smbus_read_byte_data(client, LTR559_ALS_DATA_CH1_1);
		if (alsval_ch1_lo < 0 || alsval_ch1_hi < 0)
				return -1;
		alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

		alsval_ch0_lo = i2c_smbus_read_byte_data(client, LTR559_ALS_DATA_CH0_0);
		alsval_ch0_hi = i2c_smbus_read_byte_data(client, LTR559_ALS_DATA_CH0_1);
		if (alsval_ch0_lo < 0 || alsval_ch0_hi < 0)
				return -1;
		alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;


		if ((alsval_ch0 + alsval_ch1) == 0){
				ratio = 1000;
		}else{
			ratio = alsval_ch1 * 1000 / (alsval_ch1 + alsval_ch0);
		}
//				return -1;
		/*
		   ratio = alsval_ch1 * 1000 / (alsval_ch1 + alsval_ch0);
		   if (ratio < 450) {
		   ch0_co = 17743;
		   ch1_co = -11059;
		   } else if ((ratio >= 450) && (ratio < 640)) {
		   ch0_co = 37725;
		   ch1_co = 13363;
		   } else if ((ratio >= 640) && (ratio < 850)) {
		   ch0_co = 16900;
		   ch1_co = 1690;
		   } else if (ratio >= 850) {
		   ch0_co = 0;
		   ch1_co = 0;
		   }
		   luxdata = (alsval_ch0 * ch0_co - alsval_ch1 * ch1_co) / 10000;
		 */
		if (ratio < 450) {
				ch0_co = 17743;
				ch1_co = -11059;
		} else if ((ratio >= 450) && (ratio < 640)) {
				ch0_co = 42785;
				ch1_co = 19548;
		} else if ((ratio >= 640) && (ratio < 850)) {
				ch0_co = 5926;
				ch1_co = -1185;
		} else if (ratio >= 850) {
				ch0_co = 0;
				ch1_co = 0;
		}
		luxdata = (alsval_ch0 * ch0_co - alsval_ch1 * ch1_co) / 10000;
		return luxdata;
}
#endif

static int8_t I2C_Read(uint8_t *rxData, uint8_t length)
{
	int8_t index;
	struct i2c_msg data[] = {
		{
			.addr = sensor_info->client->addr,
			.flags = 0,
			.len = 1,
			.buf = rxData,
		},
		{
			.addr = sensor_info->client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	for (index = 0; index < I2C_RETRY; index++) {
		if (i2c_transfer(sensor_info->client->adapter, data, 2) > 0)
			break;

                msleep(10);
	}

	if (index >= I2C_RETRY) {
		pr_alert("%s I2C Read Fail !!!!\n",__func__);
		return -EIO;
	}

	return 0;
}

/* I2C Write */
static int8_t I2C_Write(uint8_t *txData, uint8_t length)
{
	int8_t index;
	struct i2c_msg data[] = {
		{
			.addr = sensor_info->client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	for (index = 0; index < I2C_RETRY; index++) {
		if (i2c_transfer(sensor_info->client->adapter, data, 1) > 0)
			break;

                msleep(10);
	}

	if (index >= I2C_RETRY) {
		pr_alert("%s I2C Write Fail !!!!\n", __func__);
		return -EIO;
	}

	return 0;
}


static uint16_t lux_formula(uint16_t ch0_adc, uint16_t ch1_adc, uint8_t eqtn)
{
	uint32_t luxval = 0;
	uint32_t luxval_i = 0;
	uint32_t luxval_f = 0;
	uint16_t ch0_coeff_i = 0;
	uint16_t ch1_coeff_i = 0;
	uint16_t ch0_coeff_f = 0;
	uint16_t ch1_coeff_f = 0;
	int8_t ret; 
	uint8_t gain = 1, als_int_fac;
	uint8_t buffer[2];
	uint16_t win_fac = 0;
	int8_t fac = 1;

	buffer[0] = LTR559_ALS_PS_STATUS;
	ret = I2C_Read(buffer, 1);
	if (ret < 0) {
		pr_alert("%s | 0x%02X", __func__, buffer[0]);
		return ret;
	}

	gain = (buffer[0] & 0x70);
	gain >>= 4;

	if (gain == 0) {			//gain 1
		gain = 1;
	} else if (gain == 1) {		//gain 2
		gain = 2;
	} else if (gain == 2) {		//gain 4
		gain = 4;
	} else if (gain == 3) {		//gain 8
		gain = 8;
	} else if (gain == 6) {		//gain 48
		gain = 48;
	} else if (gain == 7) {		//gain 96
		gain = 96;
	}

	buffer[0] = LTR559_ALS_MEAS_RATE;
	ret = I2C_Read(buffer, 1);
	if (ret < 0) {
		pr_alert("%s | 0x%02X", __func__, buffer[0]);
		return ret;
	}
	als_int_fac = buffer[0] & 0x38;
	als_int_fac >>= 3;

	if (als_int_fac == 0) {
		als_int_fac = 10;
	} else if (als_int_fac == 1) {
		als_int_fac = 5;
	} else if (als_int_fac == 2) {
		als_int_fac = 20;
	} else if (als_int_fac == 3) {
		als_int_fac = 40;
	} else if (als_int_fac == 4) {
		als_int_fac = 15;
	} else if (als_int_fac == 5) {
		als_int_fac = 25;
	} else if (als_int_fac == 6) {
		als_int_fac = 30;
	} else if (als_int_fac == 7) {
		als_int_fac = 35;
	}

	if (eqtn == 1) {
		ch0_coeff_i = 1;
		ch1_coeff_i = 1;
		ch0_coeff_f = 7743;
		ch1_coeff_f = 1059;
		fac = 1;
		win_fac = winfac1;
		luxval_i = ((ch0_adc * ch0_coeff_i) + (ch1_adc * ch1_coeff_i)) * win_fac;
		luxval_f = (((ch0_adc * ch0_coeff_f) + (ch1_adc * ch1_coeff_f)) / 100) * win_fac;
		//luxval = ((17743 * ch0_calc) + (11059 * ch1_adc));
		//luxval = ((1.7743 * ch0_calc) + (1.1059 * ch1_adc)) / (gain * (als_int_fac / 10));
	} else if (eqtn == 2) {
		ch0_coeff_i = 4;
		ch1_coeff_i = 1;
		ch0_coeff_f = 2785;
		ch1_coeff_f = 696;
		win_fac = winfac2;
		if ((ch1_coeff_f * ch1_adc) < (ch0_adc * ch0_coeff_f)) {
			fac = 1;
			luxval_f = (((ch0_adc * ch0_coeff_f) - (ch1_adc * ch1_coeff_f)) / 100) * win_fac;
		} else {
			fac = -1;
			luxval_f = (((ch1_adc * ch1_coeff_f) - (ch0_adc * ch0_coeff_f)) / 100) * win_fac;
		}
		luxval_i = ((ch0_adc * ch0_coeff_i) - (ch1_adc * ch1_coeff_i)) * win_fac;
		//luxval = ((42785 * ch0_calc) - (10696 * ch1_adc));
		//luxval = ((4.2785 * ch0_calc) - (1.9548 * ch1_adc)) / (gain * (als_int_fac / 10));
	} else if (eqtn == 3) {
		ch0_coeff_i = 0;
		ch1_coeff_i = 0;
		ch0_coeff_f = 5926;
		ch1_coeff_f = 1300;
		fac = 1;
		win_fac = winfac3;
		luxval_i = ((ch0_adc * ch0_coeff_i) + (ch1_adc * ch1_coeff_i)) * win_fac;
		luxval_f = (((ch0_adc * ch0_coeff_f) + (ch1_adc * ch1_coeff_f)) / 100) * win_fac;
		//luxval = ((5926 * ch0_calc) + (1185 * ch1_adc));
		//luxval = ((0.5926 * ch0_calc) + (0.1185 * ch1_adc)) / (gain * (als_int_fac / 10));
	} else if (eqtn == 4) {
		ch0_coeff_i = 0;
		ch1_coeff_i = 0;
		ch0_coeff_f = 0;
		ch1_coeff_f = 0;
		fac = 1;
		luxval_i = 0;
		luxval_f = 0;
		//luxval = 0;
	}

if (fac < 0) {
        	luxval = (luxval_i  - (luxval_f / 100)) / (gain * als_int_fac);                              
      } else {
        	luxval = (luxval_i  + (luxval_f / 100)) / (gain * als_int_fac);
                }

/*lenovo-sw caoyi1 modify for als 20150812 begin*/
    if (luxval > 50000)
    {
        luxval = 50000;
	}
/*lenovo-sw caoyi1 modify for als 20150812 end*/


	return luxval;
}


static uint16_t ratioHysterisis (uint16_t ch0_adc, uint16_t ch1_adc)
{
/*lenovo-sw caoyi1 modify for als 20150720 begin*/
#define	RATIO_HYSVAL	1  //10 -> 1
/*lenovo-sw caoyi1 modify for als 20150720 end*/
	int ratio;
	uint8_t buffer[2], eqn_now;
	int8_t ret;
	uint16_t ch0_calc;
	uint32_t luxval = 0;
	int abs_ratio_now_old;

	buffer[0] = LTR559_ALS_CONTR;
	ret = I2C_Read(buffer, 1);
	if (ret < 0) {
		pr_alert("%s | 0x%02X", __func__, buffer[0]);
		return ret;
	}

	ch0_calc = ch0_adc;
	if ((buffer[0] & 0x20) == 0x20) {
		ch0_calc = ch0_adc - ch1_adc;
	}

	if ((ch1_adc + ch0_calc) == 0) {
		ratio = 100;
	} else {
		ratio = (ch1_adc*100) / (ch1_adc + ch0_calc);
	}

/*lenovo-sw caoyi1 modify for als 20150720 begin*/
	if (ratio < 48) {
		eqn_now = 1;
	} else if ((ratio >= 48) && (ratio < 68)) {
		eqn_now = 2;
/*lenovo-sw caoyi1 modify for als 20150720 end*/
	} else if ((ratio >= 68) && (ratio < 99)) {
		eqn_now = 3;
	} else if (ratio >= 99) {
		eqn_now = 4;
	}

	if (eqn_prev == 0) {
		luxval = lux_formula(ch0_calc, ch1_adc, eqn_now);
		ratio_old = ratio;
		eqn_prev = eqn_now;
	} else {
		if (eqn_now == eqn_prev) {
			luxval = lux_formula(ch0_calc, ch1_adc, eqn_now);
			ratio_old = ratio;
			eqn_prev = eqn_now;
		} else {
			abs_ratio_now_old = ratio - ratio_old;
			if (abs_ratio_now_old < 0) {
				abs_ratio_now_old *= (-1);
			}
			if (abs_ratio_now_old > RATIO_HYSVAL) {
				luxval = lux_formula(ch0_calc, ch1_adc, eqn_now);
				ratio_old = ratio;
				eqn_prev = eqn_now;
			} else {
				luxval = lux_formula(ch0_calc, ch1_adc, eqn_prev);
			}
		}
	}

	return luxval;
}



//static uint16_t read_als_adc_value(struct LTR559_data *LTR559)
static int ltr559_als_read(struct i2c_client *client)
{

	int8_t ret = -99;
	uint16_t value = -99;
	int ch0_val;
	int ch1_val;
	uint8_t gain, value_temp, gain_chg_req = 0;
	uint8_t buffer[4], temp;

#define AGC_UP_THRESHOLD		40000
/*lenovo-sw caoyi1 modify for als 20150812 begin*/
#define AGC_DOWN_THRESHOLD  	833//5000
/*lenovo-sw caoyi1 modify for als 20150812 end*/
#define AGC_HYS					15
#define MAX_VAL					50000

	struct ltr559_data *LTR559 = i2c_get_clientdata(client);

	/* ALS */
	buffer[0] = LTR559_ALS_DATA_CH1_0;

	/* read data bytes from data regs */
	ret = I2C_Read(buffer, 4);

	if (ret < 0) {
		dev_err(&LTR559->client->dev, "%s | 0x%02X", __func__, buffer[0]);

		return ret;
	}

	/* ALS Ch0 */
 	ch0_val = (uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8);
		dev_dbg(&LTR559->client->dev, 
			"%s | als_ch0 value = 0x%04X\n", __func__, 
			ch0_val);

	if (ch0_val > ALS_MAX_MEASURE_VAL) {
		dev_err(&LTR559->client->dev,
		        "%s: ALS Value Error: 0x%X\n", __func__,
		        ch0_val);
	}
	ch0_val &= ALS_VALID_MEASURE_MASK;
//	input_report_abs(LTR559->input_dev_als, ABS_MISC, ch0_val);
//	input_sync(LTR559->input_dev_als);

	/* ALS Ch1 */
 	ch1_val = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
		dev_dbg(&LTR559->client->dev, 
			"%s | als_ch1 value = 0x%04X\n", __func__, 
			ch1_val);

	if (ch1_val > ALS_MAX_MEASURE_VAL) {
		dev_err(&LTR559->client->dev,
		        "%s: ALS Value Error: 0x%X\n", __func__,
		        ch1_val);
	}
	ch1_val &= ALS_VALID_MEASURE_MASK;
	//input_report_abs(LTR559->input_dev_als, ABS_MISC, ch1_val);
	//input_sync(LTR559->input_dev_als);

	buffer[0] = LTR559_ALS_PS_STATUS;
	ret = I2C_Read(buffer, 1);
	if (ret < 0) {
		dev_err(&LTR559->client->dev, "%s | 0x%02X", __func__, buffer[0]);

		return ret;
	}

	value_temp = buffer[0];
	temp = buffer[0];
	gain = (value_temp & 0x70);	
	gain >>= 4;

	if (gain == 0) {			//gain 1
		gain = 1;
	} else if (gain == 1) {		//gain 2
		gain = 2;
	} else if (gain == 2) {		//gain 4
		gain = 4;
	} else if (gain == 3) {		//gain 8
		gain = 8;
	} else if (gain == 6) {		//gain 48
		gain = 48;
	} else if (gain == 7) {		//gain 96
		gain = 96;
	}

	buffer[0] = LTR559_ALS_CONTR;
	ret = I2C_Read(buffer, 1);
	if (ret < 0) {
		dev_err(&LTR559->client->dev, "%s | 0x%02X", __func__, buffer[0]);

		return ret;
	}
	value_temp = buffer[0];
	value_temp &= 0xE3;

	if ((ch0_val == 0) && (ch1_val > 50 )) {
		value = lux_val_prev;
	} else {
		if (gain == 1) {
			if ((ch0_val + ch1_val) < ((AGC_DOWN_THRESHOLD * 10) / AGC_HYS)) {
				value = ratioHysterisis(ch0_val, ch1_val);
/*lenovo-sw caoyi1 modify for als 20150720 begin*/
				value_temp |= ALS_GAIN_48x;
/*lenovo-sw caoyi1 modify for als 20150720 end*/
/*lenovo-sw caoyi1 modify for als only set 48x gain 20150819 begin*/
				gain_chg_req = 0;
/*lenovo-sw caoyi1 modify for als only set 48x gain 20150819 end*/
			} else {
				value = ratioHysterisis(ch0_val, ch1_val);
			}
/*lenovo-sw caoyi1 modify for als 20150720 begin*/
		} else if (gain == 48) {
/*lenovo-sw caoyi1 modify for als 20150720 end*/
			if ((ch0_val + ch1_val) > AGC_UP_THRESHOLD) {
				value = ratioHysterisis(ch0_val, ch1_val);
				value_temp |= ALS_GAIN_1x;
/*lenovo-sw caoyi1 modify for als only set 48x gain 20150819 begin*/
				gain_chg_req = 0;
/*lenovo-sw caoyi1 modify for als only set 48x gain 20150819 end*/
			} else {
				value = ratioHysterisis(ch0_val, ch1_val);
			}
		} else {
			value = ratioHysterisis(ch0_val, ch1_val);
		}
		if (gain_chg_req) {
			buffer[0] = LTR559_ALS_CONTR;
			buffer[1] = value_temp;
			ret = I2C_Write(buffer, 2);
			if (ret < 0) {
				dev_err(&LTR559->client->dev, "%s | 0x%02X", __func__, buffer[0]);

				return ret;
			}
		}
		
	}

	if ((value > MAX_VAL) || (((ch0_val + ch1_val) > MAX_VAL) && (temp & 0x80))) {
		value = MAX_VAL;
	}
/*lenovo-sw caoyi add for Lsensor value 20150819 begin*/
	if(value > 12000){
		value = 12000;
	}
/*lenovo-sw caoyi add for Lsensor value 20150819  end*/
	lux_val_prev = value;	

	return value;
}



static void ltr559_ps_work_func(struct work_struct *work)
{
	struct ltr559_data *data = container_of(work, struct ltr559_data, ps_work.work);
	struct i2c_client *client=data->client;
	int als_ps_status;
	int psdata;
       static int val_temp = 1;
//	static u32 ps_state_last = 1;	// xuke @ 20140828	Far as default.
	mutex_lock(&data->op_lock);

	als_ps_status = i2c_smbus_read_byte_data(client, LTR559_ALS_PS_STATUS);
	printk("%s ps_open_state=%d, als_ps_status=0x%x\n",__func__,data->ps_open_state,als_ps_status);
	if (als_ps_status < 0)
			goto workout;
	/* Here should check data status,ignore interrupt status. */
	/* Bit 0: PS Data
	 * Bit 1: PS interrupt
	 * Bit 2: ASL Data
	 * Bit 3: ASL interrupt
	 * Bit 4: ASL Gain 0: ALS measurement data is in dynamic range 2 (2 to 64k lux)
	 *                 1: ALS measurement data is in dynamic range 1 (0.01 to 320 lux)
	 */
	if ((data->ps_open_state == 1) && ((als_ps_status&0x03) == 0x03)) {
		psdata = ltr559_ps_read(client);
		/* Begin, lenovo-sw youwc1: update proximity sensor raw data */
		data->ps_cdev.raw_data.proximity = psdata;
		/* End, lenovo-sw youwc1: update proximity sensor raw data */
		printk("%s ps data=%d(0x%x), prox_threshold=%d, prox_hsyteresis_threshold=%d\n",__func__,psdata,psdata,data->platform_data->prox_threshold,data->platform_data->prox_hsyteresis_threshold);
		if(psdata > data->platform_data->prox_threshold)
		{
			printk("%s for near state added by fully.\n",__func__);
			data->ps_state = 0; //near
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, 0x07ff);
			val_temp = 0;

			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			input_sync(data->input_dev_ps);
			printk("%s, report ABS_DISTANCE=%s\n",__func__, data->ps_state ? "far" : "near");
		}
		else if (psdata < data->platform_data->prox_hsyteresis_threshold)
		{
		        printk("%s for far state added by fully.\n",__func__);
			data->ps_state = 1; //far

			val_temp = 1;

			#ifdef LTR559_PS_DYNAMIC_CALIBERATE
                        if ( data->dynamic_noise > 50 && psdata < (data->dynamic_noise - 50) ) 
			{
                                data->dynamic_noise = psdata;
                                if(psdata < 5){
                                    data->platform_data->prox_threshold = psdata+85;
                                    data->platform_data->prox_hsyteresis_threshold = psdata+45;
                                    pr_err("ltr559 rubber error(psdata---%d)!\n", psdata);
                                }else if(psdata < 80){
                                    data->platform_data->prox_threshold = psdata+85;
                                    data->platform_data->prox_hsyteresis_threshold = psdata+45;
                                }else if(psdata < 300){
                                    data->platform_data->prox_threshold = psdata+200;
                                    data->platform_data->prox_hsyteresis_threshold = psdata+100;
                                }else if(psdata < 600){
                                    data->platform_data->prox_threshold = psdata+300;
                                    data->platform_data->prox_hsyteresis_threshold = psdata+200;
                                }else if(psdata < 800){
                                    data->platform_data->prox_threshold = psdata+400;
                                    data->platform_data->prox_hsyteresis_threshold = psdata+300;
                                }else{
                                    data->platform_data->prox_threshold = 2047;
                                    data->platform_data->prox_hsyteresis_threshold = 1000;
                                    pr_err("ltr559 the proximity sensor rubber or structure is error(psdata---%d)!\n", psdata);
                                }
                                pr_info("ltr559 self calibrate when state far : noise = %d , thd_val_low = %d , htd_val_high = %d \n", psdata, data->platform_data->prox_hsyteresis_threshold, data->platform_data->prox_threshold);
                        }
			#endif
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, 0);
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);
			//input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			//input_sync(data->input_dev_ps);
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			input_sync(data->input_dev_ps);
			printk("%s, report ABS_DISTANCE=%s\n",__func__, data->ps_state ? "far" : "near");
		}else{
                        data->ps_state = val_temp;
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			input_sync(data->input_dev_ps);
			printk("%s, report ABS_DISTANCE=%s\n",__func__, data->ps_state ? "far" : "near");
		}
	}
workout:
	enable_irq(data->irq);
	mutex_unlock(&data->op_lock);
}

static void ltr559_als_work_func(struct work_struct *work)
{
	struct ltr559_data *data = container_of(work, struct ltr559_data, als_work.work);
	struct i2c_client *client=data->client;
	int als_ps_status;
	int als_data;

	mutex_lock(&data->op_lock);
	
	//printk("%s\n",__func__);

	if(!data->als_open_state)
		goto workout;

	als_ps_status = i2c_smbus_read_byte_data(client, LTR559_ALS_PS_STATUS);
	if (als_ps_status < 0)
		goto workout;


	if ((data->als_open_state == 1) && ((als_ps_status & 0x04)==0x04)) {
		als_data = ltr559_als_read(client);
		if (als_data > 50000)
			als_data = 50000;
		
		//printk("%s als data %d\n",__func__,als_data);
/*lenovo-sw caoyi1 modify for report als data after enable 20150701 begin*/
		if (als_data >= 0){
/*lenovo-sw caoyi1 modify for report als data after enable 20150701 end*/
			data->last_lux = als_data;
			input_report_abs(data->input_dev_als, ABS_MISC, als_data);
			/* Begin, lenovo-sw youwc1: update light sensor raw data */
			data->als_cdev.raw_data.light = als_data;
			/* End, lenovo-sw youwc1: update light sensor raw data */
			input_sync(data->input_dev_als);
/*lenoov-sw caoyi1 modify for report als data fast 20150629 begin*/
			als_first_data = false;
/*lenoov-sw caoyi1 modify for report als data fast 20150629 end*/
		}
	}
/*lenoov-sw caoyi1 modify for report als data fast 20150629 begin*/
	if(als_first_data == true)
	{
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(10));
	}
	else
	{
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));
	}
/*lenoov-sw caoyi1 modify for report als data fast 20150629 end*/
workout:
	mutex_unlock(&data->op_lock);
}

static irqreturn_t ltr559_irq_handler(int irq, void *arg)
{
	struct ltr559_data *data = (struct ltr559_data *)arg;

	printk("%s\n",__func__);		
	if (NULL == data)
		return IRQ_HANDLED;
/*lenovo-sw caoyi add wake lock for Psensor 20150505 begin*/
	wake_lock_timeout(&ps_lock,HZ/2);
/*lenovo-sw caoyi add wake lock for Psensor 20150505 end*/
	disable_irq_nosync(data->irq);
	schedule_delayed_work(&data->ps_work, 0);

	return IRQ_HANDLED;
}

static int ltr559_gpio_irq(struct ltr559_data *data)
{
        struct device_node *np = data->client->dev.of_node;
	 int err = 0;

        data->platform_data->int_gpio = of_get_named_gpio_flags(np, "ltr,irq-gpio", 0, &data->platform_data->irq_gpio_flags);
        if (data->platform_data->int_gpio < 0)
                return -EIO;

        if (gpio_is_valid(data->platform_data->int_gpio)) {
                err = gpio_request(data->platform_data->int_gpio, "ltr559_irq_gpio");
                if (err) {
                        printk( "%s irq gpio request failed\n",__func__);
			return -EINTR;
                }

                err = gpio_direction_input(data->platform_data->int_gpio);
                if (err) {
                        printk("%s set_direction for irq gpio failed\n",__func__);
			return -EIO;
                }
        }

	data->irq = data->client->irq = gpio_to_irq(data->platform_data->int_gpio);

        if (request_irq(data->irq, ltr559_irq_handler, IRQ_TYPE_LEVEL_LOW/*IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING*/,
                LTR559_DRV_NAME, data)) {
                printk("%s Could not allocate ltr559_INT !\n", __func__);
		return -EINTR;
        }

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
//        irq_set_irq_wake(data->irq, 1);
		disable_irq(data->irq);
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

        printk(KERN_INFO "%s: INT No. %d", __func__, data->irq);
        return 0;
}


static void ltr559_gpio_irq_free(struct ltr559_data *data)
{
	free_irq(data->irq, data);
	gpio_free(data->platform_data->int_gpio);
}

static ssize_t ltr559_show_enable_ps(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->ps_open_state);
}

static ssize_t ltr559_store_enable_ps(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);
	data->cali_status = false;

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "enable 559 PS sensor -> %ld\n", val);

	mutex_lock(&data->lockw);
#if 1	// xuke @ 20140828	Use the unified interface.
	ltr559_ps_set_enable(&data->ps_cdev, (unsigned int)val);
#else
	ltr559_ps_enable(data->client, (int)val);
#endif
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_poll_delay_als(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->platform_data->als_poll_interval);
}

static ssize_t ltr559_store_poll_delay_als(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "set 559 ALS sensor poll delay -> %ld\n", val);

	mutex_lock(&data->lockw);
	ltr559_als_set_poll_delay(data,val);
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_enable_als(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->als_open_state);
}

static ssize_t ltr559_store_enable_als(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "enable 559 ALS sensor -> %ld\n", val);

	mutex_lock(&data->lockw);
#if 1	// xuke @ 20140828	Use the unified interface.
	ltr559_als_set_enable(&data->als_cdev, (unsigned int)val);
#else
	ltr559_als_enable(data->client, (int)val);
#endif
	mutex_unlock(&data->lockw);
	return size;
}

static ssize_t ltr559_driver_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "Chip: %s %s\nVersion: %s\n",
						VENDOR_NAME, LTR559_SENSOR_NAME, DRIVER_VERSION);
}

static ssize_t ltr559_show_debug_regs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 val,high,low;
	int i;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	after = buf;

	after += sprintf(after, "%-17s%5s%14s%16s\n", "Register Name", "address", "default", "current");
	for (i = 0; i < sizeof(reg_tbl)/sizeof(reg_tbl[0]); i++) {
			if (reg_tbl[i].name == NULL || reg_tbl[i].addr == 0) {
					break;
			}
			if (i < 10) {
					val = i2c_smbus_read_byte_data(client, reg_tbl[i].addr);
					after += sprintf(after, "%-20s0x%02x\t  0x%02x\t\t  0x%02x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval, val);
			} else {
					low = i2c_smbus_read_byte_data(client, reg_tbl[i].addr);
					high = i2c_smbus_read_byte_data(client, reg_tbl[i].addr+1);
					after += sprintf(after, "%-20s0x%02x\t0x%04x\t\t0x%04x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval,(high << 8) + low);
			}
	}
	after += sprintf(after, "\nYou can echo '0xaa=0xbb' to set the value 0xbb to the register of address 0xaa.\n ");

	return (after - buf);
}

static ssize_t ltr559_store_debug_regs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	char *after, direct;
	u8 addr, val;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	addr = simple_strtoul(buf, &after, 16);
	direct = *after;
	val = simple_strtoul((after+1), &after, 16);

	if (!((addr >= 0x80 && addr <= 0x93)
				|| (addr >= 0x97 && addr <= 0x9e)))
		return -EINVAL;

	mutex_lock(&data->lockw);
	if (direct == '=')
		i2c_smbus_write_byte_data(client, addr, val);
	else
		printk("%s: register(0x%02x) is: 0x%02x\n", __func__, addr, i2c_smbus_read_byte_data(client, addr));
	mutex_unlock(&data->lockw);

	return (after - buf);
}

static ssize_t ltr559_show_adc_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	u8 high,low;
	char *after;

	after = buf;

	low = i2c_smbus_read_byte_data(client, LTR559_PS_DATA_0);
	high = i2c_smbus_read_byte_data(client, LTR559_PS_DATA_1);
	if (low < 0 || high < 0)
		after += sprintf(after, "Failed to read PS adc data.\n");
	else
		after += sprintf(after, "%d\n", (high << 8) + low);

	return (after - buf);
}

static ssize_t ltr559_show_lux_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int lux;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	lux = ltr559_als_read(client);

	return sprintf(buf, "%d\n", lux);
}

/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 begin*/
static ssize_t ltr559_show_als_raw(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ch0_val, ch1_val, als_raw;
	uint8_t buffer[4];

	buffer[0] = LTR559_ALS_DATA_CH1_0;

	/* read data bytes from data regs */
	I2C_Read(buffer, 4);

	/* ALS Ch0 */
	ch0_val = (uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8);
	ch0_val &= ALS_VALID_MEASURE_MASK;

	/* ALS Ch1 */
	ch1_val = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
	ch1_val &= ALS_VALID_MEASURE_MASK;

	als_raw = (ch1_val << 16) | ch0_val;

	return sprintf(buf, "0x%08x\n", als_raw);

}
/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 end*/

/*lenovo-sw caoyi1 add for psensor value 20150401 begin*/
static ssize_t ltr559_show_ps_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->ps_state);
}
/*lenovo-sw caoyi1 add for psensor value 20150401 end*/

#ifdef LTR559_PS_CALIBERATE
static ssize_t ltr559_show_ps_threshold(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->platform_data->prox_threshold);
}

static ssize_t ltr559_store_ps_threshold(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "ltr559 PS prox_threshold -> %ld\n", val);

	mutex_lock(&data->lockw);
	data->platform_data->prox_threshold = val;
	if (data->ps_state == 1) {
		//ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_LOW_0, 0);
		//ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);
	}
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_ps_hsyteresis_threshold(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->platform_data->prox_hsyteresis_threshold);
}

static ssize_t ltr559_store_ps_hsyteresis_threshold(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "ltr559 PS prox_hsyteresis_threshold -> %ld\n", val);

	mutex_lock(&data->lockw);
	data->platform_data->prox_hsyteresis_threshold = val;
	if (data->ps_state == 0) {
		//ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
		//ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_UP_0, 0x07ff);
	}
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_ps_cali_update_flag(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->cali_update);
}

static ssize_t ltr559_store_ps_cali_update_flag(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lockw);
	data->cali_update = 0;
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_ps_cali_action(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        struct ltr559_data *data = dev_get_drvdata(dev);

        return sprintf(buf, "%u\n", data->cali_status);
}


static ssize_t ltr559_store_ps_cali_action(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);
	int ret = 0;

	data->cali_status = false;

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "559 PS sensor cali_action-> %ld\n", val);

	mutex_lock(&data->lockw);
#if 1	// xuke @ 20140828	Use the unified interface.
	ret = ltr559_ps_self_caliberate(&data->ps_cdev);
	if(ret != 0) {
		printk(KERN_INFO "559 PS sensor cali_action error-> %d\n", ret);
		data->cali_status = false;
	} else{
		printk(KERN_INFO "559 PS sensor cali_action succeed-> %d\n", ret);
		data->cali_status = true;
	}
#else
	ltr559_ps_enable(data->client, (int)val);
#endif
	mutex_unlock(&data->lockw);

	return size;
}

#endif

static DEVICE_ATTR(debug_regs, S_IRUGO | S_IWUGO, ltr559_show_debug_regs,
				ltr559_store_debug_regs);
static DEVICE_ATTR(enable_als_sensor, S_IRUGO | S_IWUGO, ltr559_show_enable_als,
				ltr559_store_enable_als);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO, ltr559_show_enable_ps,
				ltr559_store_enable_ps);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUGO, ltr559_show_poll_delay_als,
				ltr559_store_poll_delay_als);
static DEVICE_ATTR(info, S_IRUGO, ltr559_driver_info_show, NULL);
/* Begin, lenovo-sw youwc1: update attribute right to -rw-rw-rw- */
static DEVICE_ATTR(raw_adc, S_IRUGO | S_IWUGO, ltr559_show_adc_data, NULL);
static DEVICE_ATTR(lux_adc, S_IRUGO | S_IWUGO, ltr559_show_lux_data, NULL);
/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 begin*/
static DEVICE_ATTR(als_raw, S_IRUGO | S_IWUGO, ltr559_show_als_raw, NULL);
/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 end*/
/*lenovo-sw caoyi1 add for psensor value 20150401 begin*/
static DEVICE_ATTR(ps_state, S_IRUGO , ltr559_show_ps_state, NULL);
/*lenovo-sw caoyi1 add for psensor value 20150401 end*/
/* End, lenovo-sw youwc1: update attribute right to -rw-rw-rw- */
#ifdef LTR559_PS_CALIBERATE
static DEVICE_ATTR(cali_param_1, S_IRUGO | S_IWUGO, ltr559_show_ps_threshold,
				ltr559_store_ps_threshold);
static DEVICE_ATTR(cali_param_2, S_IRUGO | S_IWUGO, ltr559_show_ps_hsyteresis_threshold,
				ltr559_store_ps_hsyteresis_threshold);
static DEVICE_ATTR(cali_update, S_IRUGO | S_IWUGO, ltr559_show_ps_cali_update_flag,
				ltr559_store_ps_cali_update_flag);
static DEVICE_ATTR(cali_action, S_IRUGO | S_IWUGO,ltr559_show_ps_cali_action, ltr559_store_ps_cali_action);
#endif


static struct attribute *ltr559_attributes[] = {
		&dev_attr_enable.attr,
		&dev_attr_info.attr,
		&dev_attr_enable_als_sensor.attr,
		&dev_attr_poll_delay.attr,
		&dev_attr_debug_regs.attr,
		&dev_attr_raw_adc.attr,
		&dev_attr_lux_adc.attr,
/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 begin*/
		&dev_attr_als_raw.attr,
/*lenovo-sw caoyi1 add for Lsensor raw value 20150423 end*/
/*lenovo-sw caoyi1 add for psensor value 20150401 begin*/
		&dev_attr_ps_state.attr,
/*lenovo-sw caoyi1 add for psensor value 20150401 end*/
#ifdef LTR559_PS_CALIBERATE
		&dev_attr_cali_param_1.attr,
		&dev_attr_cali_param_2.attr,
		&dev_attr_cali_update.attr,
		&dev_attr_cali_action.attr,		
#endif
		NULL,
};

static const struct attribute_group ltr559_attr_group = {
		.attrs = ltr559_attributes,
};

static int ltr559_als_set_poll_delay(struct ltr559_data *data,unsigned long delay)
{
	//mutex_lock(&data->op_lock);
	if (delay < 1)
		delay = 1;
	if (delay > 1000)
		delay = 1000;

	if (data->platform_data->als_poll_interval != delay) {
			data->platform_data->als_poll_interval = delay;
	}

	if(!data->als_open_state)
		return -ESRCH;

	pr_info("%s poll_interval=%d",__func__,data->platform_data->als_poll_interval);
	//cancel_delayed_work_sync(&data->als_work);
	//schedule_delayed_work(&data->als_work,msecs_to_jiffies(data->platform_data->als_poll_interval));
	//mutex_unlock(&data->op_lock);
	return 0;
}

static int ltr559_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, als_cdev);
	int ret = 0;

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = ltr559_als_enable(data->client, enable);
	if(ret < 0){
		pr_err("%s: enable(%d) failed!\n", __func__, enable);
		return -EFAULT;
	}

	data->als_open_state = enable;
	pr_err("%s: enable=(%d), data->als_open_state=%d\n", __func__, enable, data->als_open_state);
	return ret;
}
static int ltr559_als_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, als_cdev);
	ltr559_als_set_poll_delay(data, delay_msec);
	return 0;
}

#ifdef LTR559_PS_CALIBERATE
static int ltr559_ps_self_caliberate(struct sensors_classdev *sensors_cdev)			
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, ps_cdev);
	struct ltr559_platform_data *pdata = data->platform_data;
	int i=0;
	int ps;
	int data_total=0;
	int noise = 0;
	int count = 5;
	int max = 0;
	
	if(!data)
	{
		pr_err("ltr559_data is null!!\n");
		return -EFAULT;
	}

	// wait for register to be stable
	//msleep(15);

	for (i = 0; i < count; i++) {
		// wait for ps value be stable
		
		//msleep(15);
		
		ps = ltr559_ps_read(data->client);
		if (ps < 0) {
			i--;
			continue;
		}
				
		if(ps & 0x8000){
			noise = 0;
			break;
		} else {
			noise = ps;
		}	
		
		data_total += ps;

		if (max++ > 10) {
			pr_err("ltr559 read data error!\n");
			return -EFAULT;
		}
	}
	
	noise = data_total/count;
	
	if(data->dynamic_noise==0){
       	data->dynamic_noise = noise;
	}

	/*first check whether there is a obstacle above the phone when ps enable*/
	if(noise > (data->dynamic_noise + 25)){
		noise = data->dynamic_noise;
	}else{
		data->dynamic_noise = noise;
	}
	/*the end of first check whether there is a obstacle above the phone when ps enable*/

	if(noise < 5){
		pdata->prox_threshold = noise+85;
		pdata->prox_hsyteresis_threshold = noise+45;
		pr_err("Rubber error(noise----%d)\n", noise);
	}else if(noise < 80){
		pdata->prox_threshold = noise+85;
		pdata->prox_hsyteresis_threshold =  noise+45;
	}else if(noise < 300){
		pdata->prox_threshold = noise+200;
		pdata->prox_hsyteresis_threshold =  noise+100;
	}else if(noise < 600){
		pdata->prox_threshold = noise+300;
		pdata->prox_hsyteresis_threshold =  noise+200;
	}else if(noise < 800){
		pdata->prox_threshold = noise+400;
		pdata->prox_hsyteresis_threshold =  noise+300;
	}else{
		pdata->prox_threshold = 2047;
		pdata->prox_hsyteresis_threshold = 1000;
		pr_err("Rubber error(noise----%d), screen will not be turned off!\n", noise);
		//return -EAGAIN;
	}

//	if (data->ps_state == 1) {
		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_LOW_0, 2047);
		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_UP_0, 2046);
// 	} else if (data->ps_state == 0) {
//		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
//		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);
//	}

	data->cali_update = true;
	
	printk("%s : noise = %d , thd_val_low = %d , htd_val_high = %d \n",__func__, noise, pdata->prox_hsyteresis_threshold, pdata->prox_threshold);
	return 0;
}
#endif

static int ltr559_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, ps_cdev);
	int ret = 0;

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = ltr559_ps_enable(data->client, enable);
	if(ret < 0){
		pr_err("%s: enable(%d) failed!\n", __func__, enable);
		return -EFAULT;
	}

	data->ps_open_state = enable;
	pr_err("%s: enable=(%d), data->ps_open_state=%d\n", __func__, enable, data->ps_open_state);
	return ret;
}

static int ltr559_suspend(struct device *dev)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	int ret = 0;

	printk("%s\n", __func__);
	mutex_lock(&data->lockw);
#if 1	// xuke @ 20140828	Can not suspend p-sensor with kernel PM.
// None.
#else
	ret = ltr559_ps_enable(data->client, 0);
#endif
       if (data->als_open_state == 1)
	    ret |= ltr559_als_enable(data->client, 0);

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
	if(data->ps_open_state == 1){
		ret = enable_irq_wake(data->irq);
		if (ret)
			printk(KERN_WARNING "%s: enable_irq_wake(%d) failed, ret=(%d)\n", __func__, data->irq, ret);
	}
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr559_resume(struct device *dev)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	int ret=0;

	printk("%s\n", __func__);
	mutex_lock(&data->lockw);

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
	if(data->ps_open_state == 1){
		ret = disable_irq_wake(data->irq);
		if (ret)
			printk(KERN_WARNING "%s: disable_irq_wake(%d) failed, ret=(%d)\n", __func__, data->irq, ret);
	}
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

	if (data->als_open_state == 1)
	    ret = ltr559_als_enable(data->client, 1);
#if 1	// xuke @ 20140828	No need to resume p-sensor with kernel PM.
// None.
#else
	if (data->ps_open_state == 1)
		ret = ltr559_ps_enable(data->client, 1);
#endif
	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr559_check_chip_id(struct i2c_client *client)
{
	int id;

	id = i2c_smbus_read_byte_data(client, LTR559_MANUFACTURER_ID);
	printk("%s read the  LTR559_MANUFAC_ID is 0x%x\n", __func__, id);
	if (id != LTR559_MANUFAC_ID) {
		return -EINVAL;
	}
	return 0;
}

int ltr559_device_init(struct i2c_client *client)
{
	int retval = 0;
	int i;

	retval = i2c_smbus_write_byte_data(client, LTR559_ALS_CONTR, 0x02);//reset chip
	if(retval < 0)
	{
		printk("%s   i2c_smbus_write_byte_data(LTR559_ALS_CONTR, 0x02);  ERROR !!!.\n",__func__);
	}

	msleep(WAKEUP_DELAY);

	/*Reinitialise the PS and ALS register settings*/
	i=4;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //PS LED
	i=5;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //PS N PULSES
	i=6;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //PS MEAS_RATE  
	i=7;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //ALS MEAS RATE
	i=9;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //INTERRUPT PERSIST
	i=3;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //INTERRUPT


	return retval;
}

/*lenovo-sw caoyi1 modify for als device init fast 20150713 begin*/
int ltr559_device_init_for_als(struct i2c_client *client)
{
	int retval = 0;
	int i;

	msleep(10);

	/*ALS register settings*/
	i=7;
	i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval); //ALS MEAS RATE

	return retval;
}
/*lenovo-sw caoyi1 modify for als device init fast 20150713 end*/

static int sensor_regulator_configure(struct ltr559_data *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0, LTR559_VDD_MAX_UV);
		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0, LTR559_VIO_MAX_UV);
		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd, LTR559_VDD_MIN_UV, LTR559_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,"Regulator set failed vdd rc=%d\n",rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev,"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio, LTR559_VIO_MIN_UV, LTR559_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev, "Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, LTR559_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int sensor_regulator_power_on(struct ltr559_data *data, bool on)
{
	int rc = 0;

	if (!on) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			dev_err(&data->client->dev, "Regulator vio re-enabled rc=%d\n", rc);
			/*
			 * Successfully re-enable regulator.
			 * Enter poweron delay and returns error.
			 */
			if (!rc) {
				rc = -EBUSY;
				goto enable_delay;
			}
		}
		return rc;
	} else {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
			return rc;
		}
	}

enable_delay:
/*lenovo-sw caoyi1 modify for sensor enable LDO fast 20150714 begin*/
	msleep(60); //130
/*lenovo-sw caoyi1 modify for sensor enable LDO fast 20150714 end*/
	dev_dbg(&data->client->dev, "Sensor regulator power on =%d\n", on);
	return rc;
}

static int sensor_platform_hw_power_onoff(struct ltr559_data *data, bool on)
{
	int err = 0;

	if (data->power_state != on) {
		if(on){
			if(!IS_ERR_OR_NULL(data->pinctrl)){
				err = pinctrl_select_state(data->pinctrl, data->pin_default);
				if (err){
					dev_err(&data->client->dev, "Can't select pinctrl state on=%d\n", on);
					goto power_out;
				}
			}

			err = sensor_regulator_configure(data, true);
			if(err){
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_power_on(data, true);
			if (err){
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			data->power_state = true;
		}else{
			if(!IS_ERR_OR_NULL(data->pinctrl)){
				err = pinctrl_select_state(data->pinctrl, data->pin_sleep);
				if (err){
					dev_err(&data->client->dev, "Can't select pinctrl state on=%d\n", on);
					goto power_out;
				}
			}

			err = sensor_regulator_power_on(data, false);
			if (err){
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_configure(data, false);
			if(err){
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}

			data->power_state = false;
		}
	}
power_out:
	return err;
}

static int ltr559_pinctrl_init(struct ltr559_data *data)
{
	struct i2c_client *client = data->client;

	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(data->pinctrl);
	}

	data->pin_default =
		pinctrl_lookup_state(data->pinctrl, "default");
	if (IS_ERR_OR_NULL(data->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(data->pin_default);
	}

	data->pin_sleep =
		pinctrl_lookup_state(data->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(data->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(data->pin_sleep);
	}

	return 0;
}
 
static int ltr559_parse_dt(struct device *dev, struct ltr559_data *data)
{
	struct ltr559_platform_data *pdata = data->platform_data;
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int rc = 0;

	/* ps tuning data*/
	rc = of_property_read_u32(np, "ltr,ps-threshold", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps threshold\n");
		return rc;
	}
	pdata->prox_threshold = tmp;

	rc = of_property_read_u32(np, "ltr,ps-hysteresis-threshold", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->prox_hsyteresis_threshold = tmp;

	rc = of_property_read_u32(np, "ltr,als-polling-time", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->als_poll_interval = tmp;

	return 0;
}

int ltr559_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltr559_data *data;
	struct ltr559_platform_data *pdata;
	int ret = 0;

	/* check i2c*/
	if (!i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WRITE_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
			dev_err(&client->dev, "LTR-559ALS functionality check failed.\n");
			return -EIO;
	}

	/* platform data memory allocation*/
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct ltr559_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "No platform data\n");
			return -ENODEV;
		}
	}

	/* data memory allocation */
	data = kzalloc(sizeof(struct ltr559_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit_kfree_pdata;
	}
	data->client = client;
	data->platform_data = pdata;
	sensor_info = data;
	ret = ltr559_parse_dt(&client->dev, data);
	if (ret) {
		dev_err(&client->dev, "can't parse platform data\n");
		ret = -EFAULT;
		goto exit_kfree_data;
	}

	/* pinctrl initialization */
	ret = ltr559_pinctrl_init(data);
	if (ret) {
		ret = -EFAULT;
		dev_err(&client->dev, "Can't initialize pinctrl\n");
		goto exit_kfree_data;
	}

	/* power initialization */
	ret = sensor_platform_hw_power_onoff(data, true);
	if (ret) {
		ret = -ENOEXEC;
		dev_err(&client->dev,"power on fail\n");
		goto exit_kfree_data;
	}

	/* set client data as ltr559_data*/
	i2c_set_clientdata(client, data);

	ret = ltr559_check_chip_id(client);
	if(ret){
		ret = -ENXIO;
		dev_err(&client->dev,"the manufacture id is not match\n");
		goto exit_power_off;
	}

	ret = ltr559_device_init(client);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev,"device init failed\n");
		goto exit_power_off;
	}

	/* request gpio and irq */
	ret = ltr559_gpio_irq(data);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev,"gpio_irq failed\n");
		goto exit_chip_reset;
	}

	/* Register Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		ret = -ENOMEM;
		dev_err(&client->dev,"Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		ret = -ENOMEM;
		dev_err(&client->dev,"Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}

	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_als, ABS_MISC, 0, 65535, 0, 0);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 2, 0, 0);

	data->input_dev_als->name = "light";
	data->input_dev_ps->name = "proximity";
	data->input_dev_als->id.bustype = BUS_I2C;
	data->input_dev_als->dev.parent =&data->client->dev;
	data->input_dev_ps->id.bustype = BUS_I2C;
	data->input_dev_ps->dev.parent =&data->client->dev;

	input_set_drvdata(data->input_dev_als, data);
	input_set_drvdata(data->input_dev_ps, data);

/*lenovo-sw caoyi add wake lock for Psensor 20150505 begin*/
	wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock") ;
/*lenovo-sw caoyi add wake lock for Psensor 20150505 end*/

	ret = input_register_device(data->input_dev_als);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev,"Unable to register input device als: %s\n",data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	ret = input_register_device(data->input_dev_ps);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev,"Unable to register input device ps: %s\n",data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}
	printk("%s input device success.\n",__func__);

	/* init delayed works */
	INIT_DELAYED_WORK(&data->ps_work, ltr559_ps_work_func);
	INIT_DELAYED_WORK(&data->als_work, ltr559_als_work_func);

	/* init mutex */
	mutex_init(&data->lockw);
	mutex_init(&data->op_lock);

	/* create sysfs group */
	ret = sysfs_create_group(&client->dev.kobj, &ltr559_attr_group);
	if (ret){
		ret = -EROFS;
		dev_err(&client->dev,"Unable to creat sysfs group\n");
		goto exit_unregister_dev_ps;
	}

	/* Register sensors class */
	data->als_cdev = sensors_light_cdev;
	data->als_cdev.sensors_enable = ltr559_als_set_enable;
	data->als_cdev.sensors_poll_delay = ltr559_als_poll_delay;
	data->ps_cdev = sensors_proximity_cdev;
	data->ps_cdev.sensors_enable = ltr559_ps_set_enable;
	data->ps_cdev.sensors_poll_delay = NULL;
#ifdef LTR559_PS_CALIBERATE
	data->ps_cdev.sensors_self_test = ltr559_ps_self_caliberate;
	data->cali_update = false;
	data->cali_status = false;
#endif
       data->dynamic_noise = 0;

	ret = sensors_classdev_register(&client->dev, &data->als_cdev);
	if(ret) {
		ret = -EROFS;
		dev_err(&client->dev,"Unable to register to als sensor class\n");
		goto exit_remove_sysfs_group;
	}

	ret = sensors_classdev_register(&client->dev, &data->ps_cdev);
	if(ret) {
		ret = -EROFS;
		dev_err(&client->dev,"Unable to register to ps sensor class\n");
		goto exit_unregister_als_class;
	}

	dev_dbg(&client->dev,"probe succece\n");

/*lenovo-sw caoyi modify for LDO power control 20150318 begin*/
	/* power off */
	ret = sensor_platform_hw_power_onoff(data, false);
	if (ret) {
		dev_err(&client->dev,"power on fail\n");
		goto exit_kfree_data;
	}
/*lenovo-sw caoyi modify for LDO power control 20150318 end*/

	return 0;

exit_unregister_als_class:
	sensors_classdev_unregister(&data->als_cdev);
exit_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &ltr559_attr_group);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);	
exit_free_dev_ps:
	if (data->input_dev_ps)
			input_free_device(data->input_dev_ps);
exit_free_dev_als:
	if (data->input_dev_als)
			input_free_device(data->input_dev_als);
exit_free_irq:
	ltr559_gpio_irq_free(data);
exit_chip_reset:
	ltr559_chip_reset(client);
exit_power_off:
	sensor_platform_hw_power_onoff(data, false);
exit_kfree_data:
	kfree(data);
exit_kfree_pdata:
	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	data->platform_data= NULL;

	return ret;
}

static int ltr559_remove(struct i2c_client *client)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	struct ltr559_platform_data *pdata=data->platform_data;

	if (data == NULL || pdata == NULL) 
		return 0;

	ltr559_ps_enable(client, 0);
	ltr559_als_enable(client, 0);
	input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);
	
	input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);
	
	ltr559_gpio_irq_free(data);
	
	sysfs_remove_group(&client->dev.kobj, &ltr559_attr_group);	

	cancel_delayed_work_sync(&data->ps_work);
	cancel_delayed_work_sync(&data->als_work);

	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	pdata = NULL;

	kfree(data);
	data = NULL;

	return 0;
}

static struct i2c_device_id ltr559_id[] = {
		{"ltr559", 0},
		{}
};

static struct of_device_id ltr_match_table[] = {
		{ .compatible = "ltr,ltr559",},
		{ },
};

MODULE_DEVICE_TABLE(i2c, ltr559_id);
static SIMPLE_DEV_PM_OPS(ltr559_pm_ops, ltr559_suspend, ltr559_resume);
static struct i2c_driver ltr559_driver = {
		.driver = {
				.name = LTR559_DRV_NAME,
				.owner = THIS_MODULE,
				.pm = &ltr559_pm_ops,
				.of_match_table = ltr_match_table,
		},
		.probe = ltr559_probe,
		.remove = ltr559_remove,
		.id_table = ltr559_id,
};

static int ltr559_driver_init(void)
{
		pr_info("Driver ltr5590 init.\n");
		return i2c_add_driver(&ltr559_driver);
};

static void ltr559_driver_exit(void)
{
		pr_info("Unload ltr559 module...\n");
		i2c_del_driver(&ltr559_driver);
}

module_init(ltr559_driver_init);
module_exit(ltr559_driver_exit);
MODULE_AUTHOR("Lite-On Technology Corp.");
MODULE_DESCRIPTION("Lite-On LTR-559 Proximity and Light Sensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

