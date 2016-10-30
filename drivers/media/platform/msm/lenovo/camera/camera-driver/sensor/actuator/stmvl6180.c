/*
 *  stmvl6180.c - Linux kernel module for STM VL6180 FlightSense Time-of-Flight
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include "stmvl6180.h"

#include "msm_sd.h"
#include "msm_actuator.h"
#include "msm_cci.h"
//#define DEBUG_I2C_LOG
//debug

static unsigned int enable_laser_flag = 1;
static struct msm_sensor_ctrl_t *vl6180x_dev_ctrl = NULL;

/*
 * Global data
 */
//******************************** IOCTL definitions
#define VL6180_IOCTL_INIT 				_IO('p', 0x01)
#define VL6180_IOCTL_XTALKCALB			_IO('p', 0x02)
#define VL6180_IOCTL_OFFCALB			_IO('p', 0x03)
#define VL6180_IOCTL_STOP				_IO('p', 0x05)
#define VL6180_IOCTL_SETXTALK			_IOW('p', 0x06, unsigned int)
#define VL6180_IOCTL_GETDATA			_IOR('p', 0x0a, unsigned long)
#define VL6180_IOCTL_GETDATAS			_IOR('p', 0x0b, VL6180x_RangeData_t)
#define VL6180_IOCTL_READREG			_IOWR('p', 0x0c, VL6180x_Reg_t)
#define VL6180_IOCTL_WRITEREG			_IOWR('p', 0x0d, VL6180x_Reg_t)
#define VL6180_IOCTL_ENABLE_LASER       _IOW('p', 0x0e, unsigned int)


//******************************** VL6180 registers
#define IDENTIFICATION__MODEL_ID					0x000
#define IDENTIFICATION__REVISION_ID					0x002
#define FIRMWARE__BOOTUP						0x119
#define RESULT__RANGE_STATUS						0x04D
#define GPIO_HV_PAD01__CONFIG						0x132
#define SYSRANGE__MAX_CONVERGENCE_TIME					0x01C
#define SYSRANGE__RANGE_CHECK_ENABLES					0x02D
#define SYSRANGE__MAX_CONVERGENCE_TIME					0x01C
#define SYSRANGE__EARLY_CONVERGENCE_ESTIMATE				0x022
#define SYSTEM__FRESH_OUT_OF_RESET					0x016
#define SYSRANGE__PART_TO_PART_RANGE_OFFSET				0x024
#define SYSRANGE__CROSSTALK_COMPENSATION_RATE				0x01E
#define SYSRANGE__CROSSTALK_VALID_HEIGHT				0x021
#define SYSRANGE__RANGE_IGNORE_VALID_HEIGHT				0x025
#define SYSRANGE__RANGE_IGNORE_THRESHOLD				0x026
#define SYSRANGE__MAX_AMBIENT_LEVEL_MULT				0x02C
#define SYSALS__INTERMEASUREMENT_PERIOD					0x03E
#define SYSRANGE__INTERMEASUREMENT_PERIOD				0x01B
#define SYSRANGE__START							0x018
#define RESULT__RANGE_VAL						0x062
#define RESULT__RANGE_STRAY						0x063
#define RESULT__RANGE_RAW						0x064
#define RESULT__RANGE_RETURN_RATE					0x066
#define RESULT__RANGE_RETURN_SIGNAL_COUNT				0x06C
#define RESULT__RANGE_REFERENCE_SIGNAL_COUNT				0x070
#define RESULT__RANGE_RETURN_AMB_COUNT					0x074
#define RESULT__RANGE_REFERENCE_AMB_COUNT				0x078
#define RESULT__RANGE_RETURN_CONV_TIME					0x07C
#define RESULT__RANGE_REFERENCE_CONV_TIME				0x080
#define SYSTEM__INTERRUPT_CLEAR						0x015
#define RESULT__INTERRUPT_STATUS_GPIO					0x04F
#define SYSTEM__MODE_GPIO1						0x011
#define SYSTEM__INTERRUPT_CONFIG_GPIO					0x014
#define RANGE__RANGE_SCALER						0x096
//******************************** VL6180 registers

#define DEFAULT_CROSSTALK	 0x0 //set 0 for laser calibration//change by lijk3//0//4 // Already coded in 9.7 format

// Filter defines
#define FILTERNBOFSAMPLES		10
#define FILTERSTDDEVSAMPLES		6
#define MINFILTERSTDDEVSAMPLES	3
#define MINFILTERVALIDSTDDEVSAMPLES	4
#define FILTERINVALIDDISTANCE	65535

//distance filter
//#define DISTANCE_FILTER

struct msm_actuator_ctrl_t *vl6180_i2c_client;
struct mutex	  vl6180_mutex;
uint32_t MeasurementIndex = 0;
// Distance Filter global variables
uint32_t Default_ZeroVal = 0;
uint32_t Default_VAVGVal = 0;
uint32_t NoDelay_ZeroVal = 0;
uint32_t NoDelay_VAVGVal = 0;
uint32_t Previous_VAVGDiff = 0;
uint16_t LastTrueRange[FILTERNBOFSAMPLES];
uint32_t LastReturnRates[FILTERNBOFSAMPLES];
uint32_t PreviousRangeStdDev = 0;
uint32_t PreviousStdDevLimit = 0;
uint32_t PreviousReturnRateStdDev = 0;
uint16_t StdFilteredReads = 0;
uint32_t m_chipid = 0;
uint16_t LastMeasurements[8] = {0,0,0,0,0,0,0,0};
uint16_t AverageOnXSamples = 4;
uint16_t CurrentIndex = 0;


#ifdef DISTANCE_FILTER
void VL6180_InitDistanceFilter(void);
uint16_t VL6180_DistanceFilter(uint16_t m_trueRange_mm, uint16_t m_rawRange_mm, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode);
uint32_t VL6180_StdDevDamper(uint32_t AmbientRate, uint32_t SignalRate, uint32_t StdDevLimitLowLight, uint32_t StdDevLimitLowLightSNR, uint32_t StdDevLimitHighLight, uint32_t StdDevLimitHighLightSNR);
#endif

#define VCM_SID 0x24
#define LASER_SID 0x29
/*
 * Communication functions
 */

//*************************************************************************************************
// 16bit - 32bit I2C Write
//*************************************************************************************************

void vl6180_i2c_write_32bits( unsigned short RamAddr, unsigned long RamData )
{
    int rc = 0;
    uint8_t temp[4];
    uint16_t temp_sid = 0;
    temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
    vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_WORD_ADDR;

    temp[0] = (RamData >> 24) & 0xff;
    temp[1] = (RamData >> 16) & 0xff;
    temp[2] = (RamData >> 8) & 0xff;
    temp[3] = RamData & 0xff;

    rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_write_seq(&vl6180_i2c_client->i2c_client,(uint32_t)RamAddr, (uint8_t *)temp,4);
    if (rc < 0)
    	pr_err("%s:rc = %d write error\n", __func__, rc);

    vl6180_i2c_client->i2c_client.cci_client->sid  =  temp_sid;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_BYTE_ADDR;
}

// 32 bits cci read
int vl6180_i2c_read_32bits(unsigned int addr,   uint32_t * ReadData )
{
    int rc = 0;
    uint8_t temp[4];
    uint16_t temp_sid = 0;

    temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
    vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_WORD_ADDR;

    rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_read_seq(&vl6180_i2c_client->i2c_client, (unsigned long)addr, temp, 4);
    printk("%s  addr=0x%x  32bit_data temp[0]=0x%x  temp[1]=0x%x temp[2]=0x%x temp[3]=0x%x\n",__func__,addr, temp[0],temp[1],temp[2],temp[3]);

    *ReadData =  (temp[0]<<24 | temp[1]<<16 | temp[2]<<8 |temp[3]<<0);
    printk("%s  addr=0x%x  32bit_data = 0x%x  \n",__func__,addr, *ReadData);

    if (rc != 0x0) {
        printk("%s error addr=0x%x 32bit_data = 0x%x\n", __func__, addr, *ReadData);
    }

    vl6180_i2c_client->i2c_client.cci_client->sid  =  temp_sid;
    vl6180_i2c_client->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

    return rc;
}

// 16 bits cci write
int vl6180_i2c_write_16bits(unsigned int addr,  uint16_t data)
{
	int rc = 0;
        uint8_t temp_data[2];

        uint16_t temp_sid = 0;

        temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
        vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
        vl6180_i2c_client->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
        temp_data[0] = data >> 8;
        temp_data[1] = data & 0xff;
	rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_write_seq(&vl6180_i2c_client->i2c_client, (uint32_t)addr, temp_data, 2);
	if (rc != 0x0) {
                 printk("%s error addr=0x%x data = 0x%x\n",__func__,addr,data);
	}

        vl6180_i2c_client->i2c_client.cci_client->sid  = temp_sid;
        vl6180_i2c_client->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	return rc;
}

int vl6180_i2c_read_16bits(unsigned int addr,  uint16_t *pdata)
{
    int rc = 0;
    unsigned char temp;
    uint16_t temp_sid = 0;

    temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
    vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_WORD_ADDR;

    rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_read_seq(&vl6180_i2c_client->i2c_client, (unsigned long)addr, (unsigned char*)pdata, 2);
    temp = *((unsigned char*)pdata);
    *((unsigned char*)pdata) = *((unsigned char*)pdata+1);
    *((unsigned char*)pdata+1) = temp;

    if (rc != 0x0) {
        printk("%s error addr=0x%x data = 0x%x  addr_type=%d\n",__func__,addr,*pdata,vl6180_i2c_client->i2c_client.addr_type);
    } else {
        //printk("%s ok addr=0x%x data = 0x%x  addr_type=%d\\n",__func__,addr,*pdata,vl6180_i2c_client->i2c_client.addr_type);
    }

    vl6180_i2c_client->i2c_client.cci_client->sid  =  temp_sid;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_BYTE_ADDR;

    return rc;
}

// 8 bits cci write
int vl6180_i2c_write_byte(unsigned int addr,  uint16_t data)
{
    int rc = 0;
    uint16_t temp_sid = 0;

    temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
    vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_WORD_ADDR;

    rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_write(&vl6180_i2c_client->i2c_client, addr, data, MSM_CAMERA_I2C_BYTE_DATA);
    if (rc != 0x0) {
        printk("%s error addr=0x%x data = 0x%x\n",__func__,addr,data);
    }

    vl6180_i2c_client->i2c_client.cci_client->sid  =  temp_sid;
    vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_BYTE_ADDR;

    return rc;
}

int vl6180_i2c_read_byte(unsigned int addr,  uint16_t *pdata)
{
	int rc = 0;
        uint16_t temp_sid = 0;

        temp_sid = vl6180_i2c_client->i2c_client.cci_client->sid;
        vl6180_i2c_client->i2c_client.cci_client->sid = LASER_SID;
        vl6180_i2c_client->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;

        rc = vl6180_i2c_client->i2c_client.i2c_func_tbl->i2c_read(&vl6180_i2c_client->i2c_client, addr, pdata, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc != 0x0) {
                 printk("%s error addr=0x%x data = 0x%x  addr_type=%d\n",__func__,addr,*pdata,vl6180_i2c_client->i2c_client.addr_type);
	} else {
                // printk("%s ok addr=0x%x data = 0x%x  addr_type=%d\\n",__func__,addr,*pdata,vl6180_i2c_client->i2c_client.addr_type);
	}

        vl6180_i2c_client->i2c_client.cci_client->sid  =  temp_sid;
        vl6180_i2c_client->i2c_client.addr_type=  MSM_CAMERA_I2C_BYTE_ADDR;

	return rc;
}

int vl6180_deinit(struct msm_actuator_ctrl_t *client)
{
    int rc = 0;

    if (!vl6180_i2c_client)
        kfree(vl6180_i2c_client);

    return rc;
}

int vl6180_init(struct msm_actuator_ctrl_t *client)
{
    int rc = 0;
    int i;
    uint16_t modelID = 0;
    uint16_t revID = 0;

    uint16_t dataByte;
    int8_t offset_cal;

#ifdef  do_calibration
    int8_t offsetByte;
    int8_t rangeTemp = 0;
    uint16_t ambpart2partCalib1 = 0;
    uint16_t ambpart2partCalib2 = 0;
    uint16_t CrosstalkHeight;
    uint16_t IgnoreThreshold;
    uint16_t IgnoreThresholdHeight;
    uint16_t chipidRange = 0;
#endif
    uint16_t index;

#ifdef USE_INTERRUPTS
    uint16_t chipidgpio = 0;
#endif
    pr_err("vl6180_init ENTER!\n");

    vl6180_i2c_client = client;

    vl6180_i2c_read_byte( IDENTIFICATION__MODEL_ID, &modelID);
    vl6180_i2c_read_byte( IDENTIFICATION__REVISION_ID, &revID);
    pr_err("Model ID : 0x%X, REVISION ID : 0x%X\n", modelID, revID);
    vl6180_i2c_read_byte( IDENTIFICATION__MODEL_ID, &modelID);
    pr_err("reg0x00 : 0x%X\n", revID);
    vl6180_i2c_read_byte( 0x0001, &revID);
    pr_err("reg0x01 : 0x%X\n", revID);
    vl6180_i2c_read_byte( 0x0002, &revID);
    pr_err("reg0x02 : 0x%X\n", revID);
    vl6180_i2c_read_byte( 0x0003, &revID);
    pr_err("reg0x03 : 0x%X\n", revID);

    //waitForStandby
    for (i=0; i<100; i++) {
        vl6180_i2c_read_byte( FIRMWARE__BOOTUP, &modelID);
        if ( (modelID & 0x01) == 1) {
            i=100;
        }
    }

    //range device ready
    for (i=0; i<100; i++) {
        vl6180_i2c_read_byte( RESULT__RANGE_STATUS, &modelID);
        if ((modelID & 0x01) == 1) {
            i = 100;
        }
    }

#ifdef  do_calibration
    vl6180_i2c_write_byte( 0x0207, 0x01);
    vl6180_i2c_write_byte( 0x0208, 0x01);
    vl6180_i2c_write_byte( 0x0133, 0x01);
    vl6180_i2c_write_byte( 0x0096, 0x00);
    vl6180_i2c_write_byte( 0x0097, 0x54);
    vl6180_i2c_write_byte( 0x00e3, 0x00);
    vl6180_i2c_write_byte( 0x00e4, 0x04);
    vl6180_i2c_write_byte( 0x00e5, 0x02);
    vl6180_i2c_write_byte( 0x00e6, 0x01);
    vl6180_i2c_write_byte( 0x00e7, 0x03);
    vl6180_i2c_write_byte( 0x00f5, 0x02);
    vl6180_i2c_write_byte( 0x00D9, 0x05);

    // AMB P2P calibration
    vl6180_i2c_read_byte(SYSTEM__FRESH_OUT_OF_RESET, &dataByte);
    if (dataByte==0x01) {
        pr_err(" AMB P2P calibration : dataByte = 1\n");

        vl6180_i2c_read_byte( 0x26, &dataByte);
        ambpart2partCalib1 = dataByte<<8;
        vl6180_i2c_read_byte( 0x27, &dataByte);
        ambpart2partCalib1 = ambpart2partCalib1 + dataByte;
        vl6180_i2c_read_byte( 0x28, &dataByte);
        ambpart2partCalib2 = dataByte<<8;
        vl6180_i2c_read_byte( 0x29, &dataByte);
        ambpart2partCalib2 = ambpart2partCalib2 + dataByte;
        if (ambpart2partCalib1 != 0) {
            pr_err(" AMB P2P calibration : ambpart2partCalib1 != 0\n");

            // p2p calibrated
            vl6180_i2c_write_byte( 0xDA, (ambpart2partCalib1>>8)&0xFF);
            vl6180_i2c_write_byte( 0xDB, ambpart2partCalib1&0xFF);
            vl6180_i2c_write_byte( 0xDC, (ambpart2partCalib2>>8)&0xFF);
            vl6180_i2c_write_byte( 0xDD, ambpart2partCalib2&0xFF);
        } else {
            pr_err(" AMB P2P calibration : ambpart2partCalib1 == 0 use default settings\n");

            // No p2p Calibration, use default settings
            vl6180_i2c_write_byte( 0xDB, 0xCE);
            vl6180_i2c_write_byte( 0xDC, 0x03);
            vl6180_i2c_write_byte( 0xDD, 0xF8);
        }
    }

    vl6180_i2c_write_byte( 0x009f, 0x00);
    vl6180_i2c_write_byte( 0x00a3, 0x28);
    vl6180_i2c_write_byte( 0x00b7, 0x00);
    vl6180_i2c_write_byte( 0x00bb, 0x28);
    vl6180_i2c_write_byte( 0x00b2, 0x09);
    vl6180_i2c_write_byte( 0x00ca, 0x09);
    vl6180_i2c_write_byte( 0x0198, 0x01);
    vl6180_i2c_write_byte( 0x01b0, 0x17);
    vl6180_i2c_write_byte( 0x01ad, 0x00);
    vl6180_i2c_write_byte( 0x00FF, 0x05);
    vl6180_i2c_write_byte( 0x0100, 0x05);
    vl6180_i2c_write_byte( 0x0199, 0x05);
    vl6180_i2c_write_byte( 0x0109, 0x07);
    vl6180_i2c_write_byte( 0x010a, 0x30);
    vl6180_i2c_write_byte( 0x003f, 0x46);
    vl6180_i2c_write_byte( 0x01a6, 0x1b);
    vl6180_i2c_write_byte( 0x01ac, 0x3e);
    vl6180_i2c_write_byte( 0x01a7, 0x1f);
    vl6180_i2c_write_byte( 0x0103, 0x01);
    vl6180_i2c_write_byte( 0x0030, 0x00);
    vl6180_i2c_write_byte( 0x001b, 0x0A);
    vl6180_i2c_write_byte( 0x003e, 0x0A);
    vl6180_i2c_write_byte( 0x0131, 0x04);
    vl6180_i2c_write_byte( 0x0011, 0x10);
    vl6180_i2c_write_byte( 0x0014, 0x24);
    vl6180_i2c_write_byte( 0x0031, 0xFF);
    vl6180_i2c_write_byte( 0x00d2, 0x01);
    vl6180_i2c_write_byte( 0x00f2, 0x01);

    // RangeSetMaxConvergenceTime
    //vl6180_i2c_write_byte( SYSRANGE__MAX_CONVERGENCE_TIME, 0x3F);
    vl6180_i2c_write_byte( SYSRANGE__MAX_CONVERGENCE_TIME, 0x32);
    vl6180_i2c_write_byte( SYSRANGE__MAX_AMBIENT_LEVEL_MULT, 0xFF);//SNR

    vl6180_i2c_read_byte(SYSTEM__FRESH_OUT_OF_RESET, &dataByte);

    if (dataByte==0x01) {
        //readRangeOffset
        vl6180_i2c_read_byte( SYSRANGE__PART_TO_PART_RANGE_OFFSET, &dataByte);

        rangeTemp = (int8_t)dataByte;
        if (dataByte > 0x7F) {
            rangeTemp -= 0xFF;
        }

        rangeTemp /= 3;
        rangeTemp = rangeTemp +1; //roundg
        //Range_Set_Offset
        offsetByte = *((u8*)(&rangeTemp)); // round
        vl6180_i2c_write_byte( SYSRANGE__PART_TO_PART_RANGE_OFFSET,(u8)offsetByte);
    }

    // ClearSystemFreshOutofReset
    vl6180_i2c_write_byte( SYSTEM__FRESH_OUT_OF_RESET, 0x0);

    // VL6180 CrossTalk
    vl6180_i2c_write_byte( SYSRANGE__CROSSTALK_COMPENSATION_RATE,(DEFAULT_CROSSTALK>>8)&0xFF);
    vl6180_i2c_write_byte( SYSRANGE__CROSSTALK_COMPENSATION_RATE+1,DEFAULT_CROSSTALK&0xFF);

    CrosstalkHeight = 40;
    vl6180_i2c_write_byte( SYSRANGE__CROSSTALK_VALID_HEIGHT,CrosstalkHeight&0xFF);

    // Will ignore all low distances (<100mm) with a low return rate
    IgnoreThreshold = 64; // 64 = 0.5Mcps
    IgnoreThresholdHeight = 33; // 33 * scaler3 = 99mm
    vl6180_i2c_write_byte( SYSRANGE__RANGE_IGNORE_THRESHOLD, (IgnoreThreshold>>8)&0xFF);
    vl6180_i2c_write_byte( SYSRANGE__RANGE_IGNORE_THRESHOLD+1,IgnoreThreshold&0xFF);
    vl6180_i2c_write_byte( SYSRANGE__RANGE_IGNORE_VALID_HEIGHT,IgnoreThresholdHeight&0xFF);

    vl6180_i2c_read_byte( SYSRANGE__RANGE_CHECK_ENABLES, &dataByte);
    dataByte = dataByte & 0xFE; // off ECE
    dataByte = dataByte | 0x02; // on ignore thr
    vl6180_i2c_write_byte( SYSRANGE__RANGE_CHECK_ENABLES, dataByte);

    // Init of Averaging samples
    for (i=0; i<8; i++) {
        LastMeasurements[i]=65535; // 65535 means no valid data
    }
    CurrentIndex = 0;

  #ifdef USE_INTERRUPTS
    // SetSystemInterruptConfigGPIORanging
    vl6180_i2c_read_byte( SYSTEM__INTERRUPT_CONFIG_GPIO, &chipidgpio);
    vl6180_i2c_write_byte( SYSTEM__INTERRUPT_CONFIG_GPIO, (chipidgpio | 0x04));
  #endif

    //RangeSetSystemMode
    chipidRange = 0x01;
    vl6180_i2c_write_byte( SYSRANGE__START, chipidRange);

  #ifdef DISTANCE_FILTER
    VL6180_InitDistanceFilter();
  #endif

     return 0;
////////////default set
#else
    /*+begin ljk add laser af*/
    //first read the status register and check busy bit
    for (index = 0; index < 30; index++) {
        rc = vl6180_i2c_read_byte(SYSTEM__FRESH_OUT_OF_RESET, &dataByte);
        if ((rc == 0x0) && (dataByte == 0x1)) {
            pr_err("st_Vll6180 check reset ok index=%d\n",index);
            break;
        } else {
            pr_err("st_Vll6180 not reset, waiting index=%d status=%d rc=%d\n",index,dataByte,rc);
            msleep(10);
        }
    }
    /*+end ljk add laser af*/
    vl6180_i2c_write_byte( 0x0207, 0x01);
    vl6180_i2c_write_byte( 0x0208, 0x01);
    vl6180_i2c_write_byte( 0x0133, 0x01);
    vl6180_i2c_write_byte( 0x0096, 0x00);
    vl6180_i2c_write_byte( 0x0097, 0x54);
    vl6180_i2c_write_byte( 0x00e3, 0x00);
    vl6180_i2c_write_byte( 0x00e4, 0x04);
    vl6180_i2c_write_byte( 0x00e5, 0x02);
    vl6180_i2c_write_byte( 0x00e6, 0x01);
    vl6180_i2c_write_byte( 0x00e7, 0x03);
    vl6180_i2c_write_byte( 0x00f5, 0x02);
    vl6180_i2c_write_byte( 0x00D9, 0x05);
    // No p2p Calibration, use default settings
    vl6180_i2c_write_byte( 0x00DB, 0xCE);
    vl6180_i2c_write_byte( 0x00DC, 0x03);
    vl6180_i2c_write_byte( 0x00DD, 0xF8);

    vl6180_i2c_write_byte( 0x009f, 0x00);
    vl6180_i2c_write_byte( 0x00a3, 0x28);
    vl6180_i2c_write_byte( 0x00b7, 0x00);
    vl6180_i2c_write_byte( 0x00bb, 0x28);
    vl6180_i2c_write_byte( 0x00b2, 0x09);
    vl6180_i2c_write_byte( 0x00ca, 0x09);
    vl6180_i2c_write_byte( 0x0198, 0x01);
    vl6180_i2c_write_byte( 0x01b0, 0x17);
    vl6180_i2c_write_byte( 0x01ad, 0x00);
    vl6180_i2c_write_byte( 0x00FF, 0x05);
    vl6180_i2c_write_byte( 0x0100, 0x05);
    vl6180_i2c_write_byte( 0x0199, 0x05);
    vl6180_i2c_write_byte( 0x0109, 0x07);
    vl6180_i2c_write_byte( 0x010a, 0x30);
    vl6180_i2c_write_byte( 0x003f, 0x46);
    vl6180_i2c_write_byte( 0x01a6, 0x1b);
    vl6180_i2c_write_byte( 0x01ac, 0x3e);
    vl6180_i2c_write_byte( 0x01a7, 0x1f);
    vl6180_i2c_write_byte( 0x0103, 0x01);
    vl6180_i2c_write_byte( 0x0030, 0x00);
    vl6180_i2c_write_byte( 0x001b, 0x0A);
    vl6180_i2c_write_byte( 0x003e, 0x0A);
    vl6180_i2c_write_byte( 0x0131, 0x04);
    vl6180_i2c_write_byte( 0x0011, 0x10);
    vl6180_i2c_write_byte( 0x0014, 0x24);
    vl6180_i2c_write_byte( 0x0031, 0xFF);
    vl6180_i2c_write_byte( 0x00d2, 0x01);
    vl6180_i2c_write_byte( 0x00f2, 0x01);
    // RangeSetMaxConvergenceTime
    vl6180_i2c_write_byte( SYSRANGE__MAX_CONVERGENCE_TIME, 0x32);

/*+begin ljk add laser af*/
    //ignore_threshold
    rc = vl6180_i2c_read_byte( SYSRANGE__RANGE_IGNORE_THRESHOLD, &dataByte);
    if (rc == 0) {
        rc = vl6180_i2c_write_byte( 0x00da, dataByte);
    } else {
        pr_err("0xda set ignore_threshold fail\n");
    }
    //emitter_bloack_threshold
    rc = vl6180_i2c_read_byte( 0x0028, &dataByte);
    if (rc == 0) {
        rc = vl6180_i2c_write_byte( 0x00dc, dataByte);
    } else {
        pr_err("0xdc set emitter_bloack_threshold fail\n");
    }
    //set triple scale
    rc = vl6180_i2c_read_byte( 0x0024, &dataByte);
    if (rc == 0) {
        offset_cal = (int8_t)dataByte;
        pr_err("st_Vll6180 get triple scale  offset_cal= 0x%x sucess \n",dataByte);
        if (offset_cal > 0x7F) {
            offset_cal = offset_cal -0xFF;
        }

        offset_cal/=3;

        rc = vl6180_i2c_write_byte( 0x0024, offset_cal);
        rc = vl6180_i2c_write_byte( 0x0024, 0x0); //set 0 for laser calibration
        pr_err("st_Vll6180 set triple scale = 0x%x sucess \n",offset_cal);
    } else {
        pr_err("st_Vll6180 set triple scale fail\n");
    }

	vl6180_i2c_write_byte( SYSRANGE__CROSSTALK_COMPENSATION_RATE,(DEFAULT_CROSSTALK>>8)&0xFF);//set 0 for laser calibration
	vl6180_i2c_write_byte( SYSRANGE__CROSSTALK_COMPENSATION_RATE+1,DEFAULT_CROSSTALK&0xFF);
    pr_err("st_Vll6180 set triple scale = 0x%x sucess \n",offset_cal);

    //fresh_out_of reset
    rc = vl6180_i2c_write_byte(0x0016, 0x00);
    if (rc != 0) {
        pr_err("st_Vll6180fresh_out_of reset fail\n");
    }

    //check ready to start range measurement
    rc = vl6180_i2c_read_byte( 0x004d, &dataByte);
    if ((rc == 0) && ((dataByte&0x1) == 0x1)) {
        pr_err("st_Vll6180 ready well \n");
        rc = 0;
    } else {
        pr_err("st_Vll6180 ready fail status=0x%x rc=%d\n",dataByte,rc);
        rc = -1;
    }

    //just test for waiting interrupt
    // msleep(100);
    return rc;
#endif
}

int vl6180_get_data(struct msm_actuator_ctrl_t *client, struct msm_camera_laser_data *laser_data)
{
// start single shot ranging mode
    uint16_t dataByte;
    uint16_t loop = 5;
    uint16_t index;
    uint32_t signal=0;
    uint32_t noise = 0;
    uint16_t calc;
    int rc = 0;
    pr_err("enter vl6180_get_data\n");

    rc = vl6180_i2c_write_byte( 0x0018, 0x01);
    if (rc == 0) {
        //pr_err("st_Vll6180 start range measurement ok \n");
    } else {
        pr_err("st_Vll6180 start range measurement fail \n");
    }

// check status register to make sure ranging is done
    do {
        usleep(5000);
        rc = vl6180_i2c_read_byte(0x004F, &dataByte);
        if (rc == 0) {
            //pr_err("st_Vll6180 reading range result  sucess status = 0x%x  loop=%d \n",dataByte,loop );
        } else {
            pr_err("st_Vll6180 reading range result fail 1 status=0x%x rc=%d loop =%d\n",dataByte,rc,loop );
        }

        loop --;
    } while ((( dataByte& 0x07) != 0x04) && (loop > 0));

    // clean flag
     rc = vl6180_i2c_write_byte( 0x0015, 0x07);
     if (rc != 0)
     {
         pr_err("st_Vll6180 start range measurement fail\n");
     }

     //test
     rc = vl6180_i2c_read_byte(0x0011, &dataByte);
     if (rc == 0x0)
     {
         // pr_err("st_Vll6180 reading 0x11 status = %d  \n",dataByte);
     }
     else
     {
         pr_err("st_Vll6180 reading 0x11 status = %d   fail\n",dataByte);
     }

     rc = 	vl6180_i2c_read_byte(0x0014, &dataByte);
     if (rc == 0x0)
     {
         //pr_err("st_Vll6180 reading 0x14 status = %d  \n",dataByte);
     }
     else
     {
         pr_err("st_Vll6180 reading 0x14 status = %d   fail\n",dataByte);
     }
     //test

     // read ranging value
     rc = vl6180_i2c_read_byte(0x0062, &dataByte);
     if (rc == 0x0)
     {
         laser_data->position = dataByte;
         pr_err("st_Vll6180 reading range result [%d]  sucess range = %d  \n",index,dataByte);
     }
     else
     {
         pr_err("st_Vll6180 reading range result fail 1 status=0x%x rc=%d\n",dataByte,rc);
     }
     //read SNR

     rc = 	vl6180_i2c_read_32bits( 0x006c, &signal);
     if (rc == 0x0)
     {
         laser_data->signal  = signal;
         pr_err("st_Vll6180 reading ----signal [%d]  sucess signal = 0x%x  \n",index,signal);
     }
     else
     {
         pr_err("st_Vll6180 reading ----signal fail 1 status=0x%x rc=%x\n",signal,rc);
     }

     rc = vl6180_i2c_read_32bits(0x0074, &noise);
     if (rc == 0x0)
     {
         laser_data->noise  = noise;
         pr_err("st_Vll6180 reading ----noise [%d]  sucess noise = 0x%x \n",index,noise);
     }
     else
     {
         pr_err("st_Vll6180 reading ----noise fail 1 status=0x%x rc=%x\n",noise,rc);
     }

     rc = vl6180_i2c_read_16bits( SYSRANGE__CROSSTALK_COMPENSATION_RATE, &calc);
     if (rc == 0x0)
     {
         laser_data->compensation_rate  = calc;
         pr_err("st_Vll6180 reading ----calc [%d]  sucess calc = 0x%x \n",index,calc);
     }
     else
     {
         pr_err("st_Vll6180 reading ----calc fail 1 status=0x%x rc=%x\n",calc,rc);
     }

     return rc;
}

/*xuhx1 for tof begin */
static int stmvl6180_ioctl_handler(struct file *file, unsigned int cmd, unsigned long arg, void __user *p)
{
    int rc=0;
	struct msm_camera_slave_info *slave_info;
   VL6180xDev_t vl6180x_dev;

	uint16_t sensor_id_reg_addr = 0;
	uint16_t sid = 0;

	pr_err("SANK Entering handler check vl6180x_dev_ctrl %p",vl6180x_dev_ctrl);



	if(!vl6180x_dev_ctrl)
		return 0;

	vl6180x_dev = vl6180x_dev_ctrl->sensor_i2c_client;
	slave_info = vl6180x_dev_ctrl->sensordata->slave_info;

	if(!vl6180x_dev || !slave_info) {
			pr_err("SANK Entering vl6180x_dev slave_info");
		return 0;
		}
	sid = vl6180x_dev->cci_client->sid;
	sensor_id_reg_addr =  slave_info->sensor_id_reg_addr;
	vl6180x_dev->cci_client->sid = 0x0029;
	slave_info->sensor_id_reg_addr = 0x0052;


    pr_err("SANK cmd %u,VL6180_IOCTL_GETDATAS %ld ",cmd, VL6180_IOCTL_GETDATAS);

    switch (cmd) {
    case VL6180_IOCTL_INIT:    /* init.  */
    {
		pr_err("SANK VL6180_IOCTL_INIT %u ", VL6180_IOCTL_INIT);

        //client = i2c_getclient();
        if (1)
        {
           // struct stmvl6180_data *data = i2c_get_clientdata(client);
            //turn on p sensor only if it's not enabled by other client
            if (1){//data->enable_ps_sensor==0) {
                pr_err("ioclt INIT to enable PS sensor=====\n");
                //stmvl6180_set_enable(client,0); /* Power Off */
                //re-init
                VL6180x_Prepare(vl6180x_dev);
                VL6180x_UpscaleSetScaling(vl6180x_dev, 3);

                //set parameters
                //VL6180x_RangeSetInterMeasPeriod(vl6180x_dev, 10); //10ms
                //set interrupt mode
                //VL6180x_RangeSetupGPIO1(vl6180x_dev, GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT, INTR_POL_HIGH);
                VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
                VL6180x_RangeClearInterrupt(vl6180x_dev);

                //start
                //range_set_systemMode(client->addr, RANGE_START_SINGLESHOT);
                //data->ps_is_singleshot = 1;
                VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
                //data->ps_is_singleshot = 1;
                //data->enable_ps_sensor= 1;

                /* we need this polling timer routine for house keeping*/
                //spin_lock_irqsave(&data->update_lock.wait_lock, flags);
                /*
                 * If work is already scheduled then subsequent schedules will not
                 * change the scheduled time that's why we have to cancel it first.
                 */
                //__cancel_delayed_work(&data->dwork);
                //schedule_delayed_work(&data->dwork, msecs_to_jiffies(INT_POLLING_DELAY));
                //spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

                //stmvl6180_set_enable(client, 1); /* Power On */
            }


        }
		vl6180x_dev->cci_client->sid = sid;
		slave_info->sensor_id_reg_addr = sensor_id_reg_addr;

        return 0;
    }
    case VL6180_IOCTL_XTALKCALB:    /*crosstalk calibration*/
    {
       // client = i2c_getclient();
       pr_err("SANK VL6180_IOCTL_XTALKCALB %u ",VL6180_IOCTL_XTALKCALB);
        if (1)
        {
            //struct stmvl6180_data *data = i2c_get_clientdata(client);
            //turn on p sensor only if it's not enabled by other client
            if (1){//data->enable_ps_sensor==0) {
                pr_err("ioclt XTALKCALB to enable PS sensor for crosstalk calibration=====\n");
                //stmvl6180_set_enable(client,0); /* Power Off */
                //re-init
                VL6180x_Prepare(vl6180x_dev);
                VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
                VL6180x_FilterSetState(vl6180x_dev, 1); // turn off wrap around filter
#endif

                VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
                VL6180x_RangeClearInterrupt(vl6180x_dev);
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);

                //start
                VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
               // data->ps_is_singleshot = 1;
               // data->enable_ps_sensor= 1;

                /* we need this polling timer routine for house keeping*/
               // spin_lock_irqsave(&data->update_lock.wait_lock, flags);
                /*
                 * If work is already scheduled then subsequent schedules will not
                 * change the scheduled time that's why we have to cancel it first.
                 */
              //  __cancel_delayed_work(&data->dwork);
               // schedule_delayed_work(&data->dwork, msecs_to_jiffies(INT_POLLING_DELAY));
              //  spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

//stmvl6180_set_enable(client, 1); /* Power On */
            }


        }

		vl6180x_dev->cci_client->sid = sid;
		slave_info->sensor_id_reg_addr = sensor_id_reg_addr;

        return 0;



    }
    case VL6180_IOCTL_SETXTALK:
    {
       // client = i2c_getclient();
       pr_err("SANK VL6180_IOCTL_SETXTALK %ld ",VL6180_IOCTL_SETXTALK);
        if (1)
        {
            unsigned int xtalkint=0;
            //struct stmvl6180_data *data = i2c_get_clientdata(client);
            if (copy_from_user(&xtalkint, (unsigned int *)p, sizeof(unsigned int))) {
                rc = -EFAULT;
            }
            pr_err("ioctl SETXTALK as 0x%x\n", xtalkint);
            VL6180x_SetXTalkCompensationRate(vl6180x_dev, xtalkint);

        }
		  vl6180x_dev->cci_client->sid = sid;
		  slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
        return 0;
    }
    case VL6180_IOCTL_OFFCALB:  /*offset calibration*/
    {
       // client = i2c_getclient();
       pr_err("SANK VL6180_IOCTL_OFFCALB %u ",VL6180_IOCTL_OFFCALB);
        if (1)
        {
          //  struct stmvl6180_data *data = i2c_get_clientdata(client);
            //turn on p sensor only if it's not enabled by other client
            if (1) {
                pr_err("ioclt XTALKCALB to enable PS sensor for crosstalk calibration=====\n");
               // stmvl6180_set_enable(client,0); /* Power Off */
                //re-init
                VL6180x_Prepare(vl6180x_dev);
                VL6180x_UpscaleSetScaling(vl6180x_dev, 1);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
//VL6180x_FilterSetState(vl6180x_dev, 0); // turn off wrap around filter
#endif

                VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
                VL6180x_RangeClearInterrupt(vl6180x_dev);
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, 0);
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);

                //start
                VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
             //   data->ps_is_singleshot = 1;
             ////   data->enable_ps_sensor= 1;

                /* we need this polling timer routine for house keeping*/
            //    spin_lock_irqsave(&data->update_lock.wait_lock, flags);
                /*
                 * If work is already scheduled then subsequent schedules will not
                 * change the scheduled time that's why we have to cancel it first.
                 */
            //    __cancel_delayed_work(&data->dwork);
             //   schedule_delayed_work(&data->dwork, msecs_to_jiffies(INT_POLLING_DELAY));
             //   spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

             //   stmvl6180_set_enable(client, 1); /* Power On */
            }


        }

        vl6180x_dev->cci_client->sid = sid;
        slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
        return 0;
    }
    case VL6180_IOCTL_STOP:
    {
       // client = i2c_getclient();
       pr_err("SANK VL6180_IOCTL_STOP %u ",VL6180_IOCTL_STOP);
        if (1)
        {
          //  struct stmvl6180_data *data = i2c_get_clientdata(client);
            //turn off p sensor only if it's enabled by other client
            if (1) {

                //turn off p sensor
             //   data->enable_ps_sensor = 0;
                if (1) {
                  //  VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP);
                  //  VL6180x_RangeClearInterrupt(vl6180x_dev);

              //  stmvl6180_set_enable(client, 0);

             //   spin_lock_irqsave(&data->update_lock.wait_lock, flags);
                /*
                * If work is already scheduled then subsequent schedules will not
                * change the scheduled time that's why we have to cancel it first.
                */
             //   __cancel_delayed_work(&data->dwork);
             //   spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
            }
        }
		  vl6180x_dev->cci_client->sid = sid;
		  slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
        return 0;
    }
    	}
    case VL6180_IOCTL_GETDATA:    /* Get proximity value only */
    {
		pr_err("SANK VL6180_IOCTL_GETDATA %ld ",VL6180_IOCTL_GETDATA);
      //  client = i2c_getclient();
     //   if (1)
      //  {
      //      struct stmvl6180_data *data = i2c_get_clientdata(client);
      //      distance = data->rangeData.FilteredData.range_mm;
      //  }
        //printk("vl6180_getDistance return %ld\n",distance);
      //  return put_user(distance, (unsigned long *)p);

	  return 0;

    }
    case VL6180_IOCTL_GETDATAS:  /* Get all range data */
    {
      //  client = i2c_getclient();
      pr_err("SANK VL6180_IOCTL_GETDATAS %ld ",VL6180_IOCTL_GETDATAS);
        if (1)
        {
           // struct stmvl6180_data *data = i2c_get_clientdata(client);
          		VL6180x_RangeData_t data;
           		rc = VL6180x_RangePollMeasurement(vl6180x_dev, &data);
            pr_err("IOCTL_GETDATAS, m_range_mm:%d===\n",data.range_mm);
            if (copy_to_user((VL6180x_RangeData_t *)p, &(data), sizeof(VL6180x_RangeData_t))) {
                rc = -EFAULT;
            }
        }
        else
            rc = -EFAULT;

	   vl6180x_dev->cci_client->sid = sid;
	   slave_info->sensor_id_reg_addr = sensor_id_reg_addr;

       return 0;
    }
    case VL6180_IOCTL_READREG:  /*READ Register */
    {
      //  client = i2c_getclient();
      pr_err("SANK VL6180_IOCTL_READREG %ld ",VL6180_IOCTL_READREG);
        	pr_err("SANK Entering READ stm ioctl");
        if (1)
        {
            VL6180x_Reg_t reg;
            pr_err("SANK doing copy from user p %p",p);

            if (copy_from_user(&reg, (VL6180x_Reg_t *)p, sizeof(VL6180x_Reg_t))) {
                rc =  -EFAULT;
            }
            pr_err("SANK reg.num_bytes %d reg.reg_index 0x%x",reg.num_bytes,reg.reg_index);

            if (reg.num_bytes ==1 )
            {
                    uint8_t data;
                    pr_err("SANK before read");
                   // VL6180x_RdByte(vl6180x_dev, reg.reg_index,&data);
                   if(vl6180x_dev != NULL)
                   {
                       if(vl6180x_dev->i2c_func_tbl != NULL)
                       {
                           if(vl6180x_dev->i2c_func_tbl->i2c_read_seq != NULL)
                           {
                                 rc = vl6180x_dev->i2c_func_tbl->i2c_read_seq(vl6180x_dev,  reg.reg_index, &data,MSM_CAMERA_I2C_BYTE_DATA);
                           } else
                           {
                               pr_err("dev->i2c_func_tbl->i2c_read_seq is null");
                           }
                       }
                       else
                       {
                           pr_err("dev->i2c_func_tbl is null");
                       }
                   }
                   else
                   {
                       pr_err("dev is null");
                   }



                    pr_err("SANK after  read data 0x0%x",data);
                    reg.reg_data = data;
            }
            else if (reg.num_bytes ==2)
            {
                    uint16_t data;
                    VL6180x_RdWord(vl6180x_dev, reg.reg_index ,&data);
                    reg.reg_data = data;
            }
            else if (reg.num_bytes == 4)
            {
                    uint32_t data;
                    VL6180x_RdDWord(vl6180x_dev, reg.reg_index ,&data);
                    reg.reg_data = data;
            }
            else
                    rc =  -EFAULT;

			pr_err("SANK doing copy to  user");
            if (copy_to_user((VL6180x_Reg_t *)p, &reg, sizeof(VL6180x_Reg_t)))
                rc = -EFAULT;
        }  else {
        pr_err("SANK else part ERROR");
               rc = -EFAULT;
        }

		  vl6180x_dev->cci_client->sid = sid;
		  slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
		  pr_err("SANK returning");

          return rc;

       }

    case VL6180_IOCTL_WRITEREG:     /*Write Register */
    {
       // client = i2c_getclient();
       pr_err("SANK VL6180_IOCTL_WRITEREG %ld ",VL6180_IOCTL_WRITEREG);
        pr_err("SANK Entering WRITE  stm ioctl");
        if (1)
        {
            VL6180x_Reg_t reg;
            if (copy_from_user(&reg, (VL6180x_Reg_t *)p, sizeof(VL6180x_Reg_t))) {
                rc =  -EFAULT;
            }
            if (reg.num_bytes ==1 )
            {
                    uint8_t data= reg.reg_data & 0xff;
                    VL6180x_WrByte(vl6180x_dev, reg.reg_index,data);
            }
            else if (reg.num_bytes ==2)
            {
                    uint16_t data = reg.reg_data & 0xffff;
                    VL6180x_WrWord(vl6180x_dev, reg.reg_index ,data);
            }
            else if (reg.num_bytes == 4)
            {
                    uint32_t data = reg.reg_data;
                    VL6180x_WrDWord(vl6180x_dev, reg.reg_index ,data);
            }
            else
                    rc =  -EFAULT;

        }
        else
            rc =  -EFAULT;

		vl6180x_dev->cci_client->sid = sid;
		slave_info->sensor_id_reg_addr = sensor_id_reg_addr;

        return rc;

    }

    case VL6180_IOCTL_ENABLE_LASER:
            {
                unsigned int enable_laser = 0;
                pr_err("SANK VL6180_IOCTL_ENABLE_LASER %ld ",VL6180_IOCTL_ENABLE_LASER);

                if (copy_from_user(&enable_laser, (unsigned int *)p, sizeof(unsigned int))) {
                    rc = -EFAULT;
                }

                pr_err("ioctl enable_laser as 0x%x\n", enable_laser);
                enable_laser_flag = enable_laser;

                vl6180x_dev->cci_client->sid = sid;
                slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
                return 0;
            }

    default:
            {
                pr_err("SANK default case");
                return -EINVAL;
            }
    }

    return rc;
}

int msm_sensor_laser_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
    struct msm_camera_i2c_client *sensor_i2c_client;
    int ret = 0;
    uint8_t mode = 0;
    int state = 0;
    uint16_t sid = 0;

    pr_err("%s:%d Enter laser power up\n", __func__, __LINE__);
    sensor_i2c_client = (s_ctrl->sensor_i2c_client);

    if (!sensor_i2c_client) {
        pr_err("%s:%d sensor_i2c_client is null:%d\n", __func__, __LINE__, ret);
    }

    sid = sensor_i2c_client->cci_client->sid;
    sensor_i2c_client->cci_client->sid = TOF_SLAVE_ADDR;
    ret=VL6180x_InitData(sensor_i2c_client);
    if (ret < 0) {
        pr_err("%s:%d initdata failed:%d\n", __func__, __LINE__, ret);
    }

    state = 0x01;
    ret = VL6180x_FilterSetState(sensor_i2c_client, state);
    if (ret < 0) {
        pr_err("%s:%d filter failed:%d\n", __func__, __LINE__, ret);
    }

    ret = VL6180x_DisableGPIOxOut(sensor_i2c_client, 1);
    if (ret < 0) {
        pr_err("%s:%d GPIO failed:%d\n", __func__, __LINE__, ret);
    }

    ret = VL6180x_Prepare(sensor_i2c_client);
    if (ret < 0) {
        pr_err("%s:%d prepare failed:%d\n", __func__, __LINE__, ret);
    }

    mode = MODE_START_STOP | MODE_SINGLESHOT;
    ret = VL6180x_RangeSetSystemMode(sensor_i2c_client, mode);
    if (ret < 0) {
        pr_err("%s:%d range failed:%d\n", __func__, __LINE__, ret);
    }

    sensor_i2c_client->cci_client->sid = sid;
    return state;
}

int msm_get_laser_data(struct msm_sensor_ctrl_t *s_ctrl,struct sensorb_cfg_data32 *cdata32)
{
    int rc;
    VL6180x_RangeData_t RangeData;

    uint16_t sid = 0;
    uint16_t sensor_id_reg_addr = 0;
    struct msm_camera_i2c_client * sensor_i2c_client;
    struct msm_camera_slave_info *slave_info;
    struct tof_read_custom_t rof_read_data;

    //pr_err("Enter laser32 \r\n");
    if (!s_ctrl) {
        pr_err("%s:%d failed: %p\n", __func__, __LINE__, s_ctrl);
        return -EINVAL;
    }

    memset(&RangeData,0,sizeof(VL6180x_RangeData_t));

    sensor_i2c_client = s_ctrl->sensor_i2c_client;
    slave_info = s_ctrl->sensordata->slave_info;

    if (!sensor_i2c_client || !slave_info) {
        pr_err("%s:%d failed: %p %p\n", __func__, __LINE__, sensor_i2c_client, slave_info);
        return -EINVAL;
    }

    sid = sensor_i2c_client->cci_client->sid;
    sensor_id_reg_addr =  slave_info->sensor_id_reg_addr;
    sensor_i2c_client->cci_client->sid = 0x0029;
    slave_info->sensor_id_reg_addr = 0x0052;

    rc = VL6180x_RangePollMeasurement(sensor_i2c_client, &RangeData);
    if (rc < 0) {
        pr_err("VL6180x_RangePollMeasurement fail \n");
    } else {
        //pr_err("SANK Range %d Signal_rate %d error_code %d "
		//	"Dmax %d, range_mm %d, raw_range %d, rtn_rate %d "
		//	"ref_rate %d, rtn_amb_rate %d, ref_amb_cnt %d "
		//	"rtn_conv_time %d, ref_conv_time %d \r\n",
		//	RangeData.range_mm,
		//	RangeData.signalRate_mcps,
		//	RangeData.errorStatus,
		//	RangeData.DMax,
		//	RangeData.FilteredData.range_mm,
		//	RangeData.FilteredData.rawRange_mm,
		//	RangeData.FilteredData.rtnRate,
		//	RangeData.FilteredData.refRate,
		//	RangeData.FilteredData.rtnAmbRate,
		//	RangeData.FilteredData.refAmbRate,
		//	RangeData.FilteredData.rtnConvTime,
		//	RangeData.FilteredData.refConvTime);

        rof_read_data.valid_data = RangeData.valid_data;
        rof_read_data.range_mm = RangeData.range_mm;
        rof_read_data.signalRate_mcps = RangeData.signalRate_mcps;
        rof_read_data.errorStatus = RangeData.errorStatus;
        rof_read_data.DMaxSq = RangeData.DMax;
        rof_read_data.filtered_range_mm = RangeData.FilteredData.range_mm;
        rof_read_data.filtered_rawRange_mm = RangeData.FilteredData.rawRange_mm;
        rof_read_data.filtered_rtnRate = RangeData.FilteredData.rtnRate;
        rof_read_data.filtered_refRate = RangeData.FilteredData.refRate;
        rof_read_data.filtered_rtnAmbRate = RangeData.FilteredData.rtnAmbRate;
        rof_read_data.filtered_refAmbRate = RangeData.FilteredData.refAmbRate;
        rof_read_data.filtered_rtnConvTime = RangeData.FilteredData.rtnConvTime;
        rof_read_data.filtered_refConvTime = RangeData.FilteredData.refConvTime;

        rc = copy_to_user((void *) compat_ptr(cdata32->cfg.read_tof_data.dbuffer), &rof_read_data, cdata32->cfg.read_tof_data.num_bytes);
    }

    sensor_i2c_client->cci_client->sid = sid;
    slave_info->sensor_id_reg_addr = sensor_id_reg_addr;
    //pr_err("Exit \r\n");
    return rc;
}

int stmvl16180(struct msm_sensor_ctrl_t *p_ctrl)
{
    vl6180x_dev_ctrl = p_ctrl;
    if (!vl6180x_dev_ctrl)
        pr_err("SANK vl6180x_dev is null");
    else
        pr_err("SANK vl6180x_dev is not null %p",vl6180x_dev_ctrl);
    return 0;
}

static int stmvl6180_open(struct inode *inode, struct file *file)
{
    pr_err("SANK Entering STM open");
    return 0;
}

static long stmvl6180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    //mutex_lock(&vl6180_mutex);
    pr_err("SANK Entering stm ioctl");
    ret = stmvl6180_ioctl_handler(file, cmd, arg, (void __user *)arg);
    //mutex_unlock(&vl6180_mutex);

    return ret;
}

static const struct file_operations stmvl6180_ranging_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = stmvl6180_ioctl,
    .open = stmvl6180_open
    //.flush = stmvl6180_flush,
};

static struct miscdevice stmvl6180_ranging_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "stmvl6180_ranging",
    .fops   = &stmvl6180_ranging_fops
};

int stmvl6180_init(void)
{
    int err = 0;

    printk("stmvl6180_init===\n");

    err = misc_register(&stmvl6180_ranging_dev);
    //to register as a misc device
    if (err != 0)
        printk(KERN_INFO "Could not register misc. dev for stmvl6180 ranging\n");

    return err;
}

