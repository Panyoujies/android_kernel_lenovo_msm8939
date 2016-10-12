
/*******************************************************************************
################################################################################
#                             (C) STMicroelectronics 2014
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 and only version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
#------------------------------------------------------------------------------
#                             Imaging Division
################################################################################
********************************************************************************/

/**
 * @file vl6180x_i2c.c
 *
 * Copyright (C) 2014 ST MicroElectronics
 *
 * provide variable word size byte/Word/dword VL6180x register access via i2c
 *
 */
#include "vl6180x_port.h"

#if I2C_BUFFER_CONFIG == 0
    /* GLOBAL config buffer */
    uint8_t i2c_global_buffer[VL6180x_MAX_I2C_XFER_SIZE];

    #define DECL_I2C_BUFFER
    #define VL6180x_GetI2cBuffer(dev, n_byte)  i2c_global_buffer

#elif I2C_BUFFER_CONFIG == 1
    /* ON STACK */
    #define DECL_I2C_BUFFER  uint8_t LocBuffer[VL6180x_MAX_I2C_XFER_SIZE];
    #define VL6180x_GetI2cBuffer(dev, n_byte)  LocBuffer
#elif I2C_BUFFER_CONFIG == 2
    /* user define buffer type declare DECL_I2C_BUFFER  as access  via VL6180x_GetI2cBuffer */
    #define DECL_I2C_BUFFER
#else
#error "invalid I2C_BUFFER_CONFIG "
#endif

int  VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t  *buff, uint8_t data)
{

	return 0;
}

int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
	return 0;
}

int VL6180x_WrByte(VL6180xDev_t dev, uint16_t index, uint8_t data){
	int  status = 0;

	status = dev->i2c_func_tbl->i2c_write(dev, index,data,MSM_CAMERA_I2C_BYTE_DATA);
	if(status <0)
	{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	return status;
}

int VL6180x_WrWord(VL6180xDev_t dev, uint16_t index, uint16_t data){
	int  status = 0;

	status = dev->i2c_func_tbl->i2c_write(dev, index,data,MSM_CAMERA_I2C_WORD_DATA);
	if(status <0)
	{
	pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	return status;
}

int VL6180x_WrDWord(VL6180xDev_t dev, uint16_t index, uint32_t data){
	int  status = 0;
	uint8_t buffer[4];

	buffer[0]=data>>24;
	buffer[1]=(data>>16)&0xFF;
	buffer[2]=(data>>8)&0xFF;;
	buffer[3]=data&0xFF;

	status = dev->i2c_func_tbl->i2c_write_seq(dev, index,buffer,4);
	if(status <0)
	{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	return status;
}

int VL6180x_UpdateByte(VL6180xDev_t dev, uint16_t index, uint8_t AndData, uint8_t OrData){
	int  status = 0;
	uint8_t data = 0;
	
	status = dev->i2c_func_tbl->i2c_read_seq(dev, index,&data,1);
	if(status <0)
	{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	data = (data & AndData) |OrData;
	status = dev->i2c_func_tbl->i2c_write(dev, index,data,MSM_CAMERA_I2C_BYTE_DATA);
	if(status <0)
	{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	return status;
}

int VL6180x_RdByte(VL6180xDev_t dev, uint16_t index, uint8_t *data){
	int  status = 0;

	status = dev->i2c_func_tbl->i2c_read_seq(dev, index, data,1);
	if(status <0)
	{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);
	}

	return status;
}

int VL6180x_RdWord(VL6180xDev_t dev, uint16_t index, uint16_t *data){
	int  status = 0;
	uint8_t buffer[2];

	status = dev->i2c_func_tbl->i2c_read_seq(dev, index, buffer,2);
	if( status >= 0 ){
		/* VL6180x register are Big endian if cpu is be direct read direct into *data is possible */
		*data=((uint16_t)buffer[0]<<8)|(uint16_t)buffer[1];
	}else{
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, status);       
	}

	return status;
}

int  VL6180x_RdDWord(VL6180xDev_t dev, uint16_t index, uint32_t *data){
	int status = 0;
	uint8_t buffer[4];

	status = dev->i2c_func_tbl->i2c_read_seq(dev, index, buffer,4);
	if( status >= 0){
		/* VL6180x register are Big endian if cpu is be direct read direct into data is possible */
		*data=((uint32_t)buffer[0]<<24)|((uint32_t)buffer[1]<<16)|((uint32_t)buffer[2]<<8)|((uint32_t)buffer[3]);
	}

	return status;
}
