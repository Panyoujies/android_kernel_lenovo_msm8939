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

#include <linux/miscdevice.h>
#include "laser/vl6180x_api.h"
#include "laser/vl6180x_def.h"
#include "laser/vl6180x_port.h"
/*
 * Defines
 */
#define STMVL6180_DRV_NAME	"stmvl6180"


#define DRIVER_VERSION		"1.0"
#define I2C_M_WR			0x00


//if don't want to have output from vl6180_dbgmsg, comment out #DEBUG macro
#define DEBUG
#define vl6180_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)

//Device Registers
#define VL6180_MODEL_ID_REG				0x0000
#define VL6180_MODEL_REV_MAJOR_REG		0x0001
#define VL6180_MODEL_REV_MINOR_REG		0x0002
#define VL6180_MODULE_REV_MAJOR_REG		0x0003
#define VL6180_MODULE_REV_MINOR_REG		0x0004

#define VL6180_REVISION_ID_REG			0x0005
#define VL6180_REVISION_ID_REG_BYTES	1
#define VL6180_DATE_HI_REG				0x0006
#define VL6180_DATE_HI_REG_BYTES		1
#define VL6180_DATE_LO_REG				0x0007
#define VL6180_DATE_LO_REG_BYTES		1
#define VL6180_TIME_REG					0x0008
#define VL6180_TIME_REG_BYTES			2
#define VL6180_CODE_REG					0x000a
#define VL6180_CODE_REG_BYTES			1
#define VL6180_FIRMWARE_REVISION_ID_REG	0x000b
#define VL6180_FIRMWARE_REVISION_ID_REG_BYTES 1
/**
 * range data structure
 */
typedef struct
{
	unsigned int m_range;
	unsigned int m_trueRange_mm;
	unsigned int m_rawRange_mm;
	unsigned int m_rtnRate;
	unsigned int m_refRate;
	unsigned int m_rtnAmbRate;
	unsigned int m_refAmbRate;
	unsigned int m_ConvTime;
	unsigned int m_rtnSignalCount;
	unsigned int m_refSignalCount;
	unsigned int m_rtnAmbientCount;
	unsigned int m_refAmbientCount;
	unsigned int m_rtnConvTime;
	unsigned int m_refConvTime;
	int m_strayLightFactor;
}RangeData;

typedef struct
{
	unsigned int m_rawRange_mm;
	unsigned int m_rtnRate;
	unsigned int m_xtalkCompRate;
}XtalkCalData;

/*
 *  driver data structs
 */
struct stmvl6180_data {
	struct i2c_client *client;

	unsigned int is_6180;
	unsigned int enable;
	/* Range Data */
	RangeData rangeData;
	/* crosstalk clibration data */
	XtalkCalData xtalkCalData;

	/* Register Data for tool */
	unsigned int register_addr;
	unsigned int register_bytes;

	/* Debug */
	unsigned int enableDebug;
};
