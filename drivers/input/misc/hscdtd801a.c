/* drivers/input/misc/hscdtd801a.c
 *
 * GeoMagneticField device driver (HSCDTD801A)
 *
 * Copyright (C) 2011-2014 ALPS ELECTRIC CO., LTD. All Rights Reserved.
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
   
#ifdef ALPS_MAG_DEBUG
#define DEBUG 1
#endif
#include <linux/delay.h>
#include <linux/fs.h>
//#include <linux/hscdtd.h>
#include "hscdtd.h"
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sensors.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#define HSCDTD_DRIVER_NAME		"hscdtd801a"
#define HSCDTD_SENSOE_CLASS_NAME	"hscdtd801a-mag"
#define HSCDTD_INPUT_DEVICE_NAME	"compass"
#define HSCDTD_SNS_KIND			4
#define HSCDTD_LOG_TAG			"[HSCDTD], "


#define I2C_RETRIES		5


#define HSCDTD_CHIP_ID		0x1849

#define HSCDTD_STRA		0x0E
#define HSCDTD_STRB		0x0F
#define HSCDTD_INFO		0x00
#define HSCDTD_XOUT		0x04
#define HSCDTD_YOUT		0x06
#define HSCDTD_ZOUT		0x08
#define HSCDTD_TOUT		0x0A

#define HSCDTD_STATUS		0x02
#define HSCDTD_CTRL1		0x10
#define HSCDTD_CTRL2		0x11
#define HSCDTD_CTRL3		0x12
#define HSCDTD_CCTRL		0x13
#define HSCDTD_ACTRL		0x14
#define HSCDTD_FACTL		0x15

#define HSCDTD_DATA_ACCESS_NUM	6
#define HSCDTD_3AXIS_NUM	3
#define HSCDTD_INITIALL_DELAY	20
#define STRB_OUTV_THR		16000 /* about 4.8(mT) */


#define HSCDTD_DELAY(us)	usleep_range(us, us)

/* Self-test resiter value */
#define HSCDTD_ST_REG_DEF	0x55
#define HSCDTD_ST_REG_PASS	0xAA
#define HSCDTD_ST_REG_X		0x01
#define HSCDTD_ST_REG_Y		0x02
#define HSCDTD_ST_REG_Z		0x04
#define HSCDTD_ST_REG_XYZ	0x07

/* Self-test error number */
#define HSCDTD_ST_OK		0x00
#define HSCDTD_ST_ERR_I2C	0x01
#define HSCDTD_ST_ERR_INIT	0x02
#define HSCDTD_ST_ERR_1ST	0x03
#define HSCDTD_ST_ERR_2ND	0x04
#define HSCDTD_ST_ERR_VAL	0x10
#define HSCDTD_ST_ERR_VAL_X	(HSCDTD_ST_REG_X | HSCDTD_ST_ERR_VAL)
#define HSCDTD_ST_ERR_VAL_Y	(HSCDTD_ST_REG_Y | HSCDTD_ST_ERR_VAL)
#define HSCDTD_ST_ERR_VAL_Z	(HSCDTD_ST_REG_Z | HSCDTD_ST_ERR_VAL)

const int XYZ_TEM_CORRE[3] = {590, -1610, 90}; // *100 value unit ; [lsb/deg]

struct hscdtd_data {
	struct input_dev	*input;
	struct i2c_client	*i2c;
	struct delayed_work	work_data;
	struct mutex		lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend_h;
#endif
	struct sensors_classdev	cdev;
	unsigned int		kind;
	unsigned int		delay_msec;
	bool			factive;
	bool			fsuspend;
	bool			fskip;
	u32				direction;
};

static struct sensors_classdev sensors_cdev = {
	.name = HSCDTD_SENSOE_CLASS_NAME,
	.vendor = "ALPS ELECTRIC CO., LTD.",
	.version = 1,
	.handle = SENSORS_MAGNETIC_FIELD_HANDLE,
	.type = SENSOR_TYPE_MAGNETIC_FIELD,
	.max_range = "4800.0",
	.resolution = "0.30",
	.sensor_power = "0.6",
	.min_delay = 10000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 10000,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

/*--------------------------------------------------------------------------
 * i2c read/write function
 *--------------------------------------------------------------------------*/
static int hscdtd_i2c_read(struct i2c_client *i2c, u8 *rxData, int length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr	= i2c->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxData,
		},
		{
			.addr	= i2c->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxData,
		 },
	};

	do {
		err = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&i2c->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int hscdtd_i2c_write(struct i2c_client *i2c, u8 *txData, int length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr	= i2c->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txData,
		},
	};

	do {
		err = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&i2c->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


/*--------------------------------------------------------------------------
 * hscdtd function
 *--------------------------------------------------------------------------*/
static void hscdtd_convert_mount(
			struct hscdtd_data *hscdtd, int *xyz)
{
	int x, y, z;
	x = xyz[0]; y = xyz[1]; z = xyz[2];
	switch(hscdtd->direction){
		 case 0:
            xyz[0] =  x; xyz[1] =  y; xyz[2] = z;
            break;
        case 1:
            xyz[0] =  y; xyz[1] = -x; xyz[2] = z;
            break;
        case 2:
            xyz[0] = -x; xyz[1] = -y; xyz[2] = z;
            break;
        case 3:
            xyz[0] = -y; xyz[1] =  x; xyz[2] = z;
            break;
        case 4:
            xyz[0] = -x; xyz[1] =  y; xyz[2] = -z;
            break;
        case 5:
            xyz[0] = -y; xyz[1] = -x; xyz[2] = -z;
            break;
        case 6:
            xyz[0] =  x; xyz[1] = -y; xyz[2] = -z;
            break;
        case 7:
            xyz[0] =  y; xyz[1] =  x; xyz[2] = -z;
            break;
		case 8:	
			xyz[0] =  z; xyz[1] = -x; xyz[2] = -y; //for lenovo
            break;
        default:
            xyz[0] =  x; xyz[1] =  y; xyz[2] = z;
            break;
	}
	return;
}

static int hscdtd_get_magnetic_field_data(
			struct hscdtd_data *hscdtd, int *xyz)
{
	int err = -1;
	int i;
	u8  sx[HSCDTD_DATA_ACCESS_NUM];
	u8 tem[2];
	int tem_data;
	sx[0] = HSCDTD_XOUT;
	err = hscdtd_i2c_read(hscdtd->i2c, sx,
	    HSCDTD_DATA_ACCESS_NUM);
	if (err < 0)
		return err;
	/*tem_compensate*/
	tem[0] = HSCDTD_TOUT;
	err = hscdtd_i2c_read(hscdtd->i2c, tem, 2);
	if (err < 0)
		return err;
	tem_data = (((int)tem[0]) | ((int)tem[1] << 8));
	if (tem_data & 0x00001000) {
		tem_data = tem_data | (0x11110000);
	}		
printk("mark1--tem_data=%d\n",tem_data/16);	
	/*tem_compensate*/		
	for (i = 0; i < HSCDTD_3AXIS_NUM; i++){
		xyz[i] = (int) ((short)((sx[2*i+1] << 8) | (sx[2*i])));
		xyz[i] += XYZ_TEM_CORRE[i] * ( tem_data - 384 )/16/100;	/*tem_compensate*/
		if (xyz[i] > 16383)
			xyz[i] = 16383;
		else if (xyz[i] < -16384)
			xyz[i] = -16384;
	}		
	hscdtd_convert_mount(hscdtd, xyz);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "x:%d,y:%d,z:%d\n", xyz[0], xyz[1], xyz[2]);

	return err;
}

static int hscdtd_soft_reset(struct hscdtd_data *hscdtd)
{
	int rc;
	u8 buf[2];

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	buf[0] = HSCDTD_FACTL;
	buf[1] = 0x80;
	rc = hscdtd_i2c_write(hscdtd->i2c, buf, 2);
	HSCDTD_DELAY(1000);

	return rc;
}

static int hscdtd_force_setup(struct hscdtd_data *hscdtd)
{
	u8 buf[2];

	buf[0] = HSCDTD_ACTRL;
	buf[1] = 0x01;

	return hscdtd_i2c_write(hscdtd->i2c, buf, 2);
}

static void hscdtd_measure_setup(
			struct hscdtd_data *hscdtd, bool en)
{
	u8 buf[2];

	if (en) {
		buf[0] = HSCDTD_CTRL1;
		buf[1] = 0x95;
		hscdtd_i2c_write(hscdtd->i2c, buf, 2);
		HSCDTD_DELAY(20);
		hscdtd_force_setup(hscdtd);
	} else
		hscdtd_soft_reset(hscdtd);
}

static void hscdtd_schedule_setup(
			struct hscdtd_data *hscdtd, bool en)
{
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s, en = %d\n", __func__, en);

	if (en)
		schedule_delayed_work(&hscdtd->work_data,
		    msecs_to_jiffies(hscdtd->delay_msec));
	else
		cancel_delayed_work(&hscdtd->work_data);
}

static int hscdtd_get_hardware_data(
			struct hscdtd_data *hscdtd, int *xyz)
{
	int ret = 0;
	hscdtd_measure_setup(hscdtd, true);
	HSCDTD_DELAY(4000);
	ret = hscdtd_get_magnetic_field_data(hscdtd, xyz);
	hscdtd_measure_setup(hscdtd, false);
	if (ret)
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "measurement error.\n");
	return ret;
}

static int hscdtd_self_test_A(struct hscdtd_data *hscdtd)
{
	int rc = HSCDTD_ST_OK;
	u8 sx[2], cr1[1];

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	/* Control resister1 backup  */
	cr1[0] = HSCDTD_CTRL1;
	if (hscdtd_i2c_read(hscdtd->i2c, cr1, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "Control resister1 value, %02X\n", cr1[0]);

	/* Move active mode (force state)  */
	sx[0] = HSCDTD_CTRL1;
	sx[1] = 0x94;
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(40);

	/* Dummy read */
	sx[0] = HSCDTD_STRA;
	hscdtd_i2c_read(hscdtd->i2c, sx, 1);
	/* Get inital value of self-test-A register  */
	sx[0] = HSCDTD_STRA;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRA reg. initial value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_DEF) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err: Initial value of STRA reg. is %02X\n", sx[0]);
		rc = HSCDTD_ST_ERR_INIT;
		goto err_STRA;
	}

	/* do self-test-A  */
	sx[0] = HSCDTD_FACTL;
	sx[1] = 0x20;
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(50);

	/* Get 1st value of self-test-A register  */
	sx[0] = HSCDTD_STRA;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRA reg. 1st value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_PASS) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err: 1st value of STRA reg. is %02X\n", sx[0]);
		rc = HSCDTD_ST_ERR_1ST;
		goto err_STRA;
	}
	HSCDTD_DELAY(20);

	/* Get 2nd value of self-test-A register  */
	sx[0] = HSCDTD_STRA;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRA reg. 2nd value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_DEF) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err: 2nd value of STRA reg. is %02X\n", sx[0]);
		rc = HSCDTD_ST_ERR_2ND;
	}

err_STRA:
	/* Resume */
	sx[0] = HSCDTD_CTRL1;
	sx[1] = cr1[0];
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(40);

	return rc;
}

static int hscdtd_self_test_B(struct hscdtd_data *hscdtd)
{
	int rc = HSCDTD_ST_OK, xyz[3];
	u8 sx[2], cr1[1];
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	/* Control resister1 backup  */
	cr1[0] = HSCDTD_CTRL1;
	if (hscdtd_i2c_read(hscdtd->i2c, cr1, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "Control resister1 value, %02X\n", cr1[0]);

	/* Move active mode (force state)  */
	sx[0] = HSCDTD_CTRL1;
	sx[1] = 0x95;
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(40);

	/* Dummy read */
	sx[0] = HSCDTD_STRB;
	hscdtd_i2c_read(hscdtd->i2c, sx, 1);
	sx[0] = HSCDTD_STRB;
	hscdtd_i2c_read(hscdtd->i2c, sx, 1);
	/* Get inital value of self-test-B register  */
	sx[0] = HSCDTD_STRB;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRB reg. initial value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_DEF) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err: Initial value of STRB reg. is %02X\n", sx[0]);
		rc = HSCDTD_ST_ERR_INIT;
		goto err_STRB;
	}

	/* do self-test-B  */
	sx[0] = HSCDTD_ACTRL;
	sx[1] = 0x20;
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(6000);

	/* Get 1st value of self-test-B register  */
	sx[0] = HSCDTD_STRB;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRB reg. 1st value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_PASS) {
		if ((sx[0] < HSCDTD_ST_REG_X) || (sx[0] > HSCDTD_ST_REG_XYZ)) {
			dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
				"Err: 1st value of STRB reg. is %02X\n",sx[0]);
			rc = HSCDTD_ST_ERR_1ST;
			goto err_STRB;
		} else {
			dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
				"Err: 1st value of STRB reg. is %02X\n",sx[0]);
			rc = (int)(sx[0] | HSCDTD_ST_ERR_VAL);
			goto err_STRB;
		}
	}
	HSCDTD_DELAY(20);

	/* Get 2nd value of self-test-B register  */
	sx[0] = HSCDTD_STRB;
	if (hscdtd_i2c_read(hscdtd->i2c, sx, 1))
		return HSCDTD_ST_ERR_I2C;
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "STRB reg. 2nd value, %02X\n", sx[0]);
	if (sx[0] != HSCDTD_ST_REG_DEF) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err: 2nd value of STRB reg. is %02X\n", sx[0]);
		rc = HSCDTD_ST_ERR_2ND;
			goto err_STRB;
	}

	/* Measurement sensor value */
	if (hscdtd_get_hardware_data(hscdtd, xyz))
		return HSCDTD_ST_ERR_I2C;

	/* Check output value */
	if ((xyz[0] <= -STRB_OUTV_THR) || (xyz[0] >= STRB_OUTV_THR))
		rc |= HSCDTD_ST_REG_X;
	if ((xyz[1] <= -STRB_OUTV_THR) || (xyz[1] >= STRB_OUTV_THR))
		rc |= HSCDTD_ST_REG_Y;
	if ((xyz[2] <= -STRB_OUTV_THR) || (xyz[2] >= STRB_OUTV_THR))
		rc |= HSCDTD_ST_REG_Z;
	if (rc)
		rc |= HSCDTD_ST_ERR_VAL;

err_STRB:
	/* Resume */
	sx[0] = HSCDTD_CTRL1;
	sx[1] = cr1[0];
	if (hscdtd_i2c_write(hscdtd->i2c, sx, 2))
		return HSCDTD_ST_ERR_I2C;
	HSCDTD_DELAY(40);
	
	return rc;
}

static int hscdtd_register_init(struct hscdtd_data *hscdtd)
{
	int v[HSCDTD_3AXIS_NUM], ret = 0;
	u8  buf[2];
	u16 chip_info;

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	if (hscdtd_soft_reset(hscdtd)) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "Err. Can't execute software reset");
		return -1;
	}

	buf[0] = HSCDTD_INFO;
	ret = hscdtd_i2c_read(hscdtd->i2c, buf, 2);
	if (ret < 0)
		return ret;

	chip_info = (u16)((buf[1]<<8) | buf[0]);
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "chip_info, 0x%04X\n", chip_info);
	if (chip_info != HSCDTD_CHIP_ID) {
		dev_err(&hscdtd->i2c->adapter->dev, HSCDTD_LOG_TAG
		    "chipID error(0x%04X).\n", chip_info);
		return -1;
	}

	mutex_lock(&hscdtd->lock);
	ret = hscdtd_get_hardware_data(hscdtd, v);
	hscdtd->kind = HSCDTD_SNS_KIND;
	mutex_unlock(&hscdtd->lock);
	dev_info(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "x:%d y:%d z:%d\n", v[0], v[1], v[2]);

	return ret;
}


static int hscdtd_parse_dt(struct device *dev, u32 *direction)
{
	struct device_node *np = dev->of_node;
	int rc;

	rc = of_property_read_u32(np, "alps,layout", direction);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read alps,layout\n");
		return rc;
	} 
	return 0;
}
static void hscdtd_enable_set(struct hscdtd_data *hscdtd, int en)
{
	mutex_lock(&hscdtd->lock);
	hscdtd->fskip = true;
	if (en) {
		if (!hscdtd->factive) {
			hscdtd_measure_setup(hscdtd, true);
			hscdtd_schedule_setup(hscdtd, true);
		}
		hscdtd->factive = true;
	} else {
		hscdtd_schedule_setup(hscdtd, false);
		hscdtd_measure_setup(hscdtd, false);
		hscdtd->factive = false;
	}
	mutex_unlock(&hscdtd->lock);
	
	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s, enable = %d\n", __func__, en);
}

static void hscdtd_delay_set(struct hscdtd_data *hscdtd, int delay)
{
	if (delay < 10)
		delay = 10;
	else if (delay > 200)
		delay = 200;
	mutex_lock(&hscdtd->lock);
	hscdtd->delay_msec = delay;
	mutex_unlock(&hscdtd->lock);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s, rate = %d (msec)\n",
	    __func__, hscdtd->delay_msec);
}

/*--------------------------------------------------------------------------
 * suspend/resume function
 *--------------------------------------------------------------------------*/
static int hscdtd_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct hscdtd_data *hscdtd = i2c_get_clientdata(client);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	mutex_lock(&hscdtd->lock);
	hscdtd->fsuspend = true;
	hscdtd->fskip = true;
	hscdtd_schedule_setup(hscdtd, false);
	hscdtd_measure_setup(hscdtd, false);
	mutex_unlock(&hscdtd->lock);

	return 0;
}

static int hscdtd_resume(struct i2c_client *client)
{
	struct hscdtd_data *hscdtd = i2c_get_clientdata(client);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	mutex_lock(&hscdtd->lock);
	hscdtd->fskip = true;
	if (hscdtd->factive)
		hscdtd_measure_setup(hscdtd, true);
	if (hscdtd->factive)
		hscdtd_schedule_setup(hscdtd, true);
	hscdtd->fsuspend = false;
	mutex_unlock(&hscdtd->lock);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hscdtd_early_suspend(struct early_suspend *handler)
{
	struct hscdtd_data *hscdtd =
	    container_of(handler, struct hscdtd_data, early_suspend_h);
	hscdtd_suspend(hscdtd->i2c, PMSG_SUSPEND);
}

static void hscdtd_early_resume(struct early_suspend *handler)
{
	struct hscdtd_data *hscdtd =
	    container_of(handler, struct hscdtd_data, early_suspend_h);
	hscdtd_resume(hscdtd->i2c);
}
#endif


/*--------------------------------------------------------------------------
 * sysfs
 *--------------------------------------------------------------------------*/
static ssize_t hscdtd_kind_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", (int)hscdtd->kind);
}

static ssize_t hscdtd_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", (hscdtd->factive) ? 1 : 0);
}

static ssize_t hscdtd_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	int new_value;

	if (hscdtd->fsuspend) {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please resume device\n");
		return size;
	}

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "%s: invalid value %d\n",
		    __func__, *buf);
		return -EINVAL;
	}

	hscdtd_enable_set(hscdtd, new_value);

	return size;
}

static ssize_t hscdtd_axis_direction_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hscdtd->direction);
}

static ssize_t hscdtd_delay_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hscdtd->delay_msec);
}

static ssize_t hscdtd_delay_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int err;
	long new_delay;
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);

	err = strict_strtol(buf, 10, &new_delay);
	if (err < 0)
		return err;

	hscdtd_delay_set(hscdtd, (int)new_delay);

	return size;
}

static ssize_t hscdtd_self_test_A_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = -1;
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	if (hscdtd->fsuspend) {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please resume device\n");
		return sprintf(buf, "%d\n", ret);
	}

	if (!hscdtd->factive) {
		mutex_lock(&hscdtd->lock);
		ret = hscdtd_self_test_A(hscdtd);
		mutex_unlock(&hscdtd->lock);
		dev_dbg(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Self test-A result : %d\n", ret);
	} else
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please turn off sensor\n");

	return sprintf(buf, "%d\n", ret);
}

static ssize_t hscdtd_self_test_B_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = -1;
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s\n", __func__);

	if (hscdtd->fsuspend) {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please resume device\n");
		return sprintf(buf, "%d\n", ret);
	}

	if (!hscdtd->factive) {
		mutex_lock(&hscdtd->lock);
		ret = hscdtd_self_test_B(hscdtd);
		mutex_unlock(&hscdtd->lock);
		dev_dbg(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Self test-B result : %d\n", ret);
	} else
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please turn off sensor\n");

	return sprintf(buf, "%d\n", ret);
}

static ssize_t hscdtd_get_hw_data_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int xyz[] = {0, 0, 0};
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);

	if (hscdtd->fsuspend) {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please resume device\n");
		return sprintf(buf, "%d,%d,%d\n", xyz[0], xyz[1], xyz[2]);
	}

	if (!hscdtd->factive) {
		mutex_lock(&hscdtd->lock);
		hscdtd_get_hardware_data(hscdtd, xyz);
		mutex_unlock(&hscdtd->lock);
		dev_dbg(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "%s: %d, %d, %d\n",
		    __func__, xyz[0], xyz[1], xyz[2]);
	} else
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "Error: Please turn off sensor\n");

	return sprintf(buf, "%d,%d,%d\n", xyz[0], xyz[1], xyz[2]);
}

#if 0
static ssize_t hscdtd_suspend_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", (hscdtd->fsuspend) ? 1 : 0);
}

static ssize_t hscdtd_suspend_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct hscdtd_data *hscdtd = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		dev_err(&hscdtd->i2c->adapter->dev,
		    HSCDTD_LOG_TAG "%s: invalid value %d\n",
		    __func__, *buf);
		return -EINVAL;
	}

	dev_dbg(&hscdtd->i2c->adapter->dev,
	    HSCDTD_LOG_TAG "%s, suspend = %d\n", __func__, new_value);

	if (new_value)
		hscdtd_suspend(hscdtd->i2c, PMSG_SUSPEND);
	else
		hscdtd_resume(hscdtd->i2c);

	return size;
}
#endif

static struct device_attribute attributes[] = {
	__ATTR(kind, S_IRUGO,
		hscdtd_kind_show, NULL),
	__ATTR(axis, S_IRUGO,
		hscdtd_axis_direction_show, NULL),
	__ATTR(enable, S_IWUGO | S_IRUGO,
		hscdtd_enable_show, hscdtd_enable_store),
	__ATTR(delay, S_IWUGO | S_IRUGO,
		hscdtd_delay_show, hscdtd_delay_store),
#if 0
	__ATTR(suspend, S_IWUGO | S_IRUGO,
		hscdtd_suspend_show, hscdtd_suspend_store),
#endif
	__ATTR(self_test_A, S_IRUGO,
		hscdtd_self_test_A_show, NULL),
	__ATTR(self_test_B, S_IRUGO,
		hscdtd_self_test_B_show, NULL),
	__ATTR(get_hw_data, S_IRUGO,
		hscdtd_get_hw_data_show, NULL)
};

static int hscdtd_enable_sensors_class(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct hscdtd_data *hscdtd = container_of(sensors_cdev,
			struct hscdtd_data, cdev);

	hscdtd_enable_set(hscdtd, (int)enable);

	return 0;
}

static int hscdtd_delay_sensors_class(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct hscdtd_data *hscdtd = container_of(sensors_cdev,
			struct hscdtd_data, cdev);
	hscdtd_delay_set(hscdtd, (int)delay_msec);

	return 0;
}

static int hscdtd_create_sysfs(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto out_sysfs;

	return 0;

out_sysfs:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "Unable to create interface\n");

	return -EIO;
}

static void hscdtd_remove_sysfs(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}


/*--------------------------------------------------------------------------
 * work function
 *--------------------------------------------------------------------------*/
static void hscdtd_polling(struct work_struct *work)
{
	int xyz[HSCDTD_3AXIS_NUM];
	struct hscdtd_data *hscdtd =
	    container_of(work, struct hscdtd_data, work_data.work);

	mutex_lock(&hscdtd->lock);
	if (hscdtd->fsuspend) {
		mutex_unlock(&hscdtd->lock);
		return;
	}
	if (hscdtd->fskip)
		hscdtd->fskip = false;
	else {
		if (hscdtd->factive) {
			if (!hscdtd_get_magnetic_field_data(hscdtd, xyz)) {
				input_report_abs(hscdtd->input,
				    ABS_X, xyz[0]);
				input_report_abs(hscdtd->input,
				    ABS_Y, xyz[1]);
				input_report_abs(hscdtd->input,
				    ABS_Z, xyz[2]);
				//hscdtd->input->sync = 0;
				input_event(hscdtd->input,
				    EV_SYN, SYN_REPORT, 0);
			}
			hscdtd_force_setup(hscdtd);	/* For next polling */
		}
	}
	if (hscdtd->factive) {
		schedule_delayed_work(&hscdtd->work_data,
		    msecs_to_jiffies(hscdtd->delay_msec));
	}
	mutex_unlock(&hscdtd->lock);
}


/*--------------------------------------------------------------------------
 * i2c device
 *--------------------------------------------------------------------------*/
static int hscdtd_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;
	struct hscdtd_data *hscdtd;

	dev_dbg(&client->adapter->dev,
		HSCDTD_LOG_TAG "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		rc = -EIO;
		goto out_region;
	}

	hscdtd = kzalloc(sizeof(struct hscdtd_data), GFP_KERNEL);
	if (!hscdtd) {
		dev_err(&client->adapter->dev,
		    "failed to allocate memory for module data\n");
		rc = -ENOMEM;
		goto out_region;
	}
	hscdtd->i2c = client;
	i2c_set_clientdata(client, hscdtd);

	rc = hscdtd_parse_dt(&client->dev, &hscdtd->direction);
	if (rc) {
		dev_warn(&client->adapter->dev,
			 "Warning no platform data for hscdtd\n");
		hscdtd->direction = 0;	
	} else {
		dev_dbg(&client->adapter->dev, "get platform_data");
	}

	mutex_init(&hscdtd->lock);

	hscdtd->factive = false;
	hscdtd->fsuspend = false;
	hscdtd->delay_msec = HSCDTD_INITIALL_DELAY;

	rc = hscdtd_register_init(hscdtd);
	if (rc) {
		rc = -EIO;
		dev_err(&client->adapter->dev, "hscdtd_register_init\n");
		goto out_kzalloc;
	}
	dev_dbg(&client->adapter->dev,
	    "initialize %s sensor\n", HSCDTD_DRIVER_NAME);

	hscdtd->input = input_allocate_device();
	if (!hscdtd->input) {
		rc = -ENOMEM;
		dev_err(&client->adapter->dev, "input_allocate_device\n");
		goto out_kzalloc;
	}
	dev_dbg(&client->adapter->dev, "input_allocate_device\n");

	input_set_drvdata(hscdtd->input, hscdtd);

	hscdtd->input->name		= HSCDTD_INPUT_DEVICE_NAME;
	hscdtd->input->id.bustype	= BUS_I2C;
	hscdtd->input->evbit[0]		= BIT_MASK(EV_ABS);

	input_set_abs_params(hscdtd->input, ABS_X, -16384, 16383, 0, 0);
	input_set_abs_params(hscdtd->input, ABS_Y, -16384, 16383, 0, 0);
	input_set_abs_params(hscdtd->input, ABS_Z, -16384, 16383, 0, 0);

	rc = input_register_device(hscdtd->input);
	if (rc) {
		rc = -ENOMEM;
		dev_err(&client->adapter->dev, "input_register_device\n");
		goto out_idev_allc;
	}
	dev_dbg(&client->adapter->dev, "input_register_device\n");

	INIT_DELAYED_WORK(&hscdtd->work_data, hscdtd_polling);

	rc = hscdtd_create_sysfs(&hscdtd->input->dev);
	if (rc) {
		rc = -ENOMEM;
		dev_err(&client->adapter->dev, "hscdtd_create_sysfs\n");
		goto out_idev_reg;
	}
	dev_dbg(&client->adapter->dev, "hscdtd_create_sysfs\n");

	hscdtd->cdev = sensors_cdev;
	hscdtd->cdev.sensors_enable = hscdtd_enable_sensors_class;
	hscdtd->cdev.sensors_poll_delay = hscdtd_delay_sensors_class;
	rc = sensors_classdev_register(&client->dev, &hscdtd->cdev);
	if (rc) {
		dev_err(&client->adapter->dev,
		    "sensors_classdev_register\n");
		goto out_sysfs;
	}
	dev_dbg(&client->adapter->dev, "sensors_classdev_register\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	hscdtd->early_suspend_h.suspend = hscdtd_early_suspend;
	hscdtd->early_suspend_h.resume  = hscdtd_early_resume;
	register_early_suspend(&hscdtd->early_suspend_h);
	dev_dbg(&client->adapter->dev, "register_early_suspend\n");
#endif

	dev_info(&client->adapter->dev,
	    HSCDTD_LOG_TAG "detected %s geomagnetic field sensor\n",
	    HSCDTD_DRIVER_NAME);

	return 0;

out_sysfs:
	hscdtd_remove_sysfs(&hscdtd->input->dev);
out_idev_reg:
	input_unregister_device(hscdtd->input);
out_idev_allc:
	input_free_device(hscdtd->input);
out_kzalloc:
	mutex_destroy(&hscdtd->lock);
	kfree(hscdtd);
out_region:

	return rc;
}

static int hscdtd_remove(struct i2c_client *client)
{
	struct hscdtd_data *hscdtd = i2c_get_clientdata(client);

	dev_dbg(&client->adapter->dev, HSCDTD_LOG_TAG "%s\n", __func__);

	hscdtd_measure_setup(hscdtd, false);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hscdtd->early_suspend_h);
#endif
	sensors_classdev_unregister(&hscdtd->cdev);
	hscdtd_remove_sysfs(&hscdtd->input->dev);
	input_unregister_device(hscdtd->input);
	input_free_device(hscdtd->input);
	mutex_destroy(&hscdtd->lock);
	kfree(hscdtd);

	return 0;
}


/*--------------------------------------------------------------------------
 * module
 *--------------------------------------------------------------------------*/
static const struct i2c_device_id HSCDTD_id[] = {
	{ HSCDTD_DRIVER_NAME, 0 },
	{ }
};

static struct of_device_id hscdtd801a_match_table[] = {
	{ .compatible = "alps,hscdtd801a", },
	{ },
};

static struct i2c_driver hscdtd_driver = {
	.probe		= hscdtd_probe,
	.remove		= hscdtd_remove,
	.id_table	= HSCDTD_id,
	.driver		= {
		.owner	= THIS_MODULE,
		.name = HSCDTD_DRIVER_NAME,
		.of_match_table = hscdtd801a_match_table,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= hscdtd_suspend,
	.resume		= hscdtd_resume,
#endif
};

static int __init hscdtd_init(void)
{
	pr_debug(HSCDTD_LOG_TAG "%s\n", __func__);
	return i2c_add_driver(&hscdtd_driver);
}

static void __exit hscdtd_exit(void)
{
	pr_debug(HSCDTD_LOG_TAG "%s\n", __func__);
	i2c_del_driver(&hscdtd_driver);
}

module_init(hscdtd_init);
module_exit(hscdtd_exit);

MODULE_DESCRIPTION("ALPS Geomagnetic Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
