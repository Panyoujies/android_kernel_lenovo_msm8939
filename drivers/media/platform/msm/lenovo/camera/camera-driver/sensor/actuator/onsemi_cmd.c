//*************************************************************************************************
// LGIT LC898111A ES1 (2012.07.24)
//*************************************************************************************************

#include "msm_actuator.h"
#include "../eeprom/msm_eeprom.h"

#include "onsemi_cmd.h"
#include "onsemi_ois.h"

//Slave Address 0x24

unsigned char I2cSlvAddrWr = 0x24;
unsigned char EEPROMSlaveAddrWr = 0x50;
unsigned char SensorSlvAddrWr = 0x10;
unsigned char I2cAddrLen = 2;
extern struct msm_actuator_ctrl_t * actuator_ctrl;
extern struct msm_eeprom_ctrl_t *eeprom_data_ctrl;

//*************************************************************************************************
// 16bit - 16bit I2C Write
//*************************************************************************************************
///for driver ic  16b
void RamWriteA( unsigned short RamAddr, unsigned short RamData )
{
    int rc = 0;
    uint8_t temp_data[2];
    temp_data[0] = RamData >> 8;
    temp_data[1] = RamData & 0xff;
    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR
    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(&actuator_ctrl->i2c_client,(uint32_t)RamAddr, temp_data,2);
    if(rc < 0)
    	pr_err("%s:rc = %d write error\n", __func__, rc);
}

void RamWriteA_Ex(uint32_t address, uint8_t * data, uint8_t length)
{
    int rc = 0;
    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR

    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(&actuator_ctrl->i2c_client,address,data,length);
    if(rc < 0)
    	pr_err("%s:rc = %d write error\n", __func__, rc);
}

//*************************************************************************************************
// 16bit - 16bit I2C Read
//*************************************************************************************************

void RamReadA( unsigned short RamAddr, void * ReadData )
{
    int rc = 0;
    unsigned char temp;

    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR

    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
									&actuator_ctrl->i2c_client,
									(unsigned long)RamAddr,
									(unsigned char *)ReadData,
						            2);
    temp = *((unsigned char*)ReadData);
    *((unsigned char*)ReadData) = *((unsigned char*)ReadData+1);
    *((unsigned char*)ReadData+1) = temp;

    if(rc < 0)
    	pr_err("%s:rc = %d read error\n", __func__, rc);
}

void RamRead32A( unsigned short RamAddr, void * ReadData )
{
    int rc = 0;
    unsigned long   temp_32;

    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR
    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
									&actuator_ctrl->i2c_client,
									(unsigned long)RamAddr,
									(unsigned char *)ReadData,
						            4);
    temp_32= (*((unsigned char*)ReadData)<<24 | *((unsigned char*)ReadData+1)<<16 | *((unsigned char*)ReadData+2)<<8 | *((unsigned char*)ReadData+3));
    pr_err("%s:rc = %d ReadData_32b=0x%lx\n", __func__, rc,temp_32);
}

//*************************************************************************************************
// 16bit - 32bit I2C Write
//*************************************************************************************************

void RamWrite32A( unsigned short RamAddr, unsigned long RamData )
{
    int rc = 0;
    uint8_t temp[4];
    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR

    temp[0] = (RamData >> 24) & 0xff;
    temp[1] = (RamData >> 16) & 0xff;
    temp[2] = (RamData >> 8) & 0xff;
    temp[3] = RamData & 0xff;

    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(&actuator_ctrl->i2c_client,(uint32_t)RamAddr, (uint8_t *)temp,4);
    if(rc < 0)
    	pr_err("%s:rc = %d write error\n", __func__, rc);
}

//*************************************************************************************************
// 16bit - 8bit I2C Write
//*************************************************************************************************

void RegWriteA(unsigned short RegAddr, unsigned char RegData)
{
    int rc = 0;

    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR

    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_write(
									&actuator_ctrl->i2c_client,
									(uint32_t)RegAddr,
									RegData,
									1);//MSM_CAMERA_I2C_BYTE_DATA
    if(rc < 0)
    	pr_err("%s:rc = %d write error    addr=0x%x  data=0x%x\n", __func__, rc,RegAddr,RegData);

}

//*************************************************************************************************
// 16bit - 8bit I2C Read
//*************************************************************************************************
//8 bit read
void RegReadA(unsigned short RegAddr, unsigned char *RegData)
{

    int rc = 0;
    actuator_ctrl->i2c_client.addr_type = 2;//MSM_CAMERA_I2C_WORD_ADDR

    rc = actuator_ctrl->i2c_client.i2c_func_tbl->i2c_read(
									&actuator_ctrl->i2c_client,
									(unsigned long)RegAddr,
									(unsigned short *)RegData,
									1);//MSM_CAMERA_I2C_BYTE_DATA
    if(rc < 0)
    	pr_err("%s:rc = %d read addr=0x%x  RegData =0x%x\n", __func__, rc,RegAddr,*RegData);
}

#if 0
void AF_SetPos( short pos )
{
	#define	 SW_FSTMODE		0x04	// Fast Stable Mode

    RegWriteA( TCODEH, SW_FSTMODE | ((pos & 0x300) >> 8) );
    RegWriteA( TCODEL, pos & 0xFF );
}
#endif





