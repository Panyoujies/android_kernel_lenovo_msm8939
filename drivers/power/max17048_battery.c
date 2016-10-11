/*
 *  max17048_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>

#define max17048_VCELL_REG	0x02
#define max17048_VCELL_LSB	0x03
#define max17048_SOC_REG	0x04
#define max17048_SOC_LSB	0x05
#define max17048_MODE_REG	0x06
#define max17048_MODE_LSB	0x07
#define max17048_VER_REG	0x08
#define max17048_VER_LSB	0x09
#define max17048_RCOMP_REG  0x0C
#define max17048_RCOMP_LSB	0x0D
#define max17048_CMD_REG	0xFE
#define max17048_CMD_LSB	0xFF
#define max17048_MODEL_ACCESS_REG			0x3E
#define max17048_MODEL_ACCESS_UNLOCK		0x4A57
#define max17048_MODEL_ACCESS_LOCK			0x0000

#define max17048_DELAY			10*HZ //1000->10*HZ
#define max17048_BATTERY_FULL	95

extern int max77819_get_battery_health(void);
extern int get_vbus(void);
extern int charger_is_done;
static int soc_log_count = 15;
//below are from .ini file
u16 INI_RCOMPSeg = 0x80;//#define INI_RCOMPSeg		(0x0180)
int INI_RCOMP = 95;//#define INI_RCOMP 			(95)
int INI_TempCoUp = -550;//#define INI_TempCoUp 		(-550)
int INI_TempCoDown = -7050;//#define INI_TempCoDown 		(-7050)
u8 INI_SOCCHECKA = 231;//#define INI_SOCCHECKA		(231)
u8 INI_SOCCHECKB = 233;//#define INI_SOCCHECKB		(233)
u16 INI_OCVTEST = 57696;//#define INI_OCVTEST 		(57696)
u8 INI_BITS = 19;//#define INI_BITS			(19)
static struct wake_lock battery_wakelock;

#define VERIFY_AND_FIX 1
#define LOAD_MODEL !(VERIFY_AND_FIX)

int chip_version = 0;
module_param(chip_version, int, 0644);
static volatile unsigned int is_in_call = 0;

extern int get_batt_temp(void);//get external temperature
static int max17048_write_reg(struct i2c_client *client, u8 reg, u16 value);
static void max17048_get_vcell(struct i2c_client *client);
static int max17048_read_reg(struct i2c_client *client, u8 reg);
static u8 original_OCV_1=0, original_OCV_2=0;

struct soc_param
{
	int calculated_soc;
	int last_soc;
	int batt_volt;
	int current_now;
	unsigned long last_soc_change_sec;
	int shut_down_volt_sec;
	bool charging;
	bool start_shut_down_mon;
};
struct max17048_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct delayed_work		hand_work;
	struct delayed_work		notifier_work; //jelphi
	struct power_supply		fgbattery;
	struct soc_param        soc_param;

	/* State Of Connect */
	int online;
	/* charger disabled*/
	int charger_enabled;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	int smoothed_soc;
	/* State Of Charge */
	int status;
	int temp;
	int rcomp; 
	
	/* SOC alert jelphi 2015/2/3*/
	int  alert_irq;
  	int  alert_gpio;
};
struct max17048_battery_model_info {                               
    int rcomp0;                                     
    int tempcoup;  /*need to *1000*/                               
    int tempcodown;/*need to *1000*/                               
	int rcomseg;
    u16 ocvtest;                                                   
    u8 socchecka;                                                  
    u8 soccheckb;                                                  
    u8 bits;//18bit or 19 bit                                      
    u8 data[64];                                                   
};                                                                 
struct max17048_battery_model_info bat_ATL =                         
{                                                                  
         .rcomp0 = 108,                                             
         .tempcoup = -625,   /* should div 1000, real is -0.625 */   
         .tempcodown = -6050,    /* should div 1000, real is -6.05 */
		 .rcomseg = 0x80,
         .ocvtest = 57248,                                         
         .socchecka = 230,                                         
         .soccheckb = 232,                                         
         .bits = 19,                                               
         .data = {                                                 
                   	0xAA,0x00,0xB6,0x90,0xB8,0x30,0xB9,0xF0,
					0xBA,0x80,0xBB,0xD0,0xBC,0x70,0xBD,0x20,
					0xBE,0x00,0xBF,0xB0,0xC0,0x90,0xC3,0xC0,
					0xC6,0x10,0xCA,0xE0,0xD0,0xE0,0xD5,0xA0,
					0x02,0xA0,0x20,0x00,0x18,0x00,0x58,0x20,
					0x16,0x80,0x4C,0x00,0x59,0xA0,0x3B,0xE0,
					0x1B,0xE0,0x1F,0x60,0x17,0xE0,0x11,0xC0,
					0x13,0x80,0x0F,0xC0,0x0C,0x00,0x0C,0x00}        
};                                                                 
struct max17048_battery_model_info bat_GUYU =                         
{                                                                  
         .rcomp0 = 95,                                             
         .tempcoup = -550,   /* should div 100, real is -20.1 */   
         .tempcodown = -7050,    /* should div 100, real is -5.5 */
		 .rcomseg = 0x80,
         .ocvtest = 57696,                                         
         .socchecka = 231,                                         
         .soccheckb = 233,                                         
         .bits = 19,                                               
         .data = {                                                 
                   0xA9,0xD0,0xB5,0x00,0xB7,0xD0,0xBA,0x70,        
                   0xBB,0x60,0xBC,0x80,0xBD,0x70,0xBE,0xB0,        
                   0xC0,0x40,0xC1,0x80,0xC3,0xD0,0xC6,0x30,        
                   0xCA,0xE0,0xCE,0x70,0xD2,0x30,0xD7,0x60,        
                   0x01,0x80,0x0A,0x20,0x20,0x00,0x28,0x40,        
                   0x30,0xC0,0x43,0x60,0x3B,0xE0,0x1D,0xE0,        
                   0x1B,0xE0,0x17,0x60,0x13,0xA0,0x10,0x40,        
                   0x12,0x80,0x0F,0xA0,0x0C,0x40,0x0C,0x40}        
};                                                                 
#define ATL_BATT_ID 1800
#define GUYU_BATT_ID 0
struct max17048_battery_model_info *batt_model = NULL;
extern int get_batt_id_volt(void);
static void max17048_get_batt_info(void)
{
	int batt_id_volt = get_batt_id_volt()/1000;
	//printk("%s,volt:%d",__func__,batt_id_volt);
	if(abs(batt_id_volt-ATL_BATT_ID) < 100)
	{
		batt_model = &bat_ATL;
	//	printk("%s:ATL\n",__func__);
	}
	else if(abs(batt_id_volt-GUYU_BATT_ID) < 100)
	{
		batt_model = &bat_GUYU;
	//	printk("%s:GUYU\n",__func__);
	}
	else
	{
		batt_model = &bat_ATL;
	//	printk("%s:ATL\n",__func__);
	}

	INI_RCOMP = batt_model->rcomp0;
	INI_TempCoUp = batt_model->tempcoup;
	INI_TempCoDown = batt_model->tempcodown;
	INI_SOCCHECKA = batt_model->socchecka;
	INI_SOCCHECKB = batt_model->soccheckb;
	INI_OCVTEST = batt_model->ocvtest;
	INI_BITS = batt_model->bits;
	INI_RCOMPSeg = batt_model->rcomseg;
}

static int max17048_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, swab16(value));

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17048_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}


void prepare_to_load_model(struct i2c_client *client) {
	 
	  u8 OCV_1,OCV_2;
	  
    u16 msb;
    u16 check_times = 0;
    do {
    	  msleep(100);
    	  //Step1:unlock model access, enable access to OCV and table registers
        max17048_write_reg(client, max17048_MODEL_ACCESS_REG, max17048_MODEL_ACCESS_UNLOCK);
        //Step2:Read OCV, verify Model Access Unlocked  
        msb = max17048_read_reg(client, 0x0E);//read OCV             
        OCV_1 = (msb)&(0x00FF);//"big endian":low byte save to MSB
        OCV_2 = ((msb)&(0xFF00))>>8;

				if(check_times++ >= 3) {//avoid of while(1)
				    check_times = 0;
				    printk("max17048:time out2...");
				    break;
				    
				}
    }while ((OCV_1==0xFF)&&(OCV_2==0xFF));//verify Model Access Unlocked
}


void load_model(struct i2c_client *client) {	

	u8 *model_data = batt_model->data;/*[64] = {
		0xA0, 0x90, 0xB6, 0x20, 0xB9, 0x80, 0xBA, 0x10,
		0xBC, 0x20, 0xBC, 0xF0, 0xBD, 0xD0, 0xC0, 0x00,
		0xC2, 0xF0, 0xC4, 0x60, 0xC6, 0x10, 0xC7, 0x90,
		0xCC, 0xB0, 0xCF, 0x20, 0xD1, 0x10, 0xD6, 0xF0,
		0x00, 0xC0, 0x0D, 0x10, 0x1B, 0x80, 0x0E, 0x20,
		0x21, 0x00, 0x1D, 0x20, 0x11, 0x00, 0x0D, 0xA0,
		0x0D, 0xC0, 0x09, 0xE0, 0x08, 0xF0, 0x07, 0xF0,
		0x08, 0x60, 0x08, 0xD0, 0x05, 0xD0, 0x05, 0xD0,

		
		0xAA,0x00,0xB6,0xC0,0xB7,0xE0,0xB9,0x30,
		0xBA,0xF0,0xBB,0x90,0xBC,0xA0,0xBD,0x40,
		0xBE,0x20,0xBF,0x40,0xC0,0xD0,0xC3,0xF0,
		0xC6,0xE0,0xCC,0x90,0xD1,0xA0,0xD7,0x30,
		0x01,0xC0,0x0E,0xC0,0x2C,0x00,0x1C,0x00,
		0x00,0xC0,0x46,0x00,0x51,0xE0,0x31,0xE0,
		0x22,0x80,0x2B,0xE0,0x13,0xC0,0x15,0xA0,
		0x10,0x20,0x10,0x20,0x0E,0x00,0x0E,0x00,
*/			     
	// };	//you need to get model data from maxim integrated
	 
   /******************************************************************************
	Step 5. Write the Model
	Once the model is unlocked, the host software must write the 64 byte model
	to the device. The model is located between memory 0x40 and 0x7F.
	The model is available in the INI file provided with your performance
	report. See the end of this document for an explanation of the INI file.
	Note that the table registers are write-only and will always read
	0xFF. Step 9 will confirm the values were written correctly.
	*/
	int k=0;
	u16 value = 0;
	//Once the model is unlocked, the host software must write the 64 bytes model to the device
	for (k=0; k < 0x40; k+=2) {
		value = (model_data[k]<<8)+model_data[k+1];
		//The model is located between memory 0x40 and 0x7F
		max17048_write_reg(client, 0x40+k, value);
	}

	//Write RCOMPSeg (for MAX17048/MAX17049 only)
	for (k=0; k < 0x10; k++) {
	    max17048_write_reg(client,0x80+k*2, INI_RCOMPSeg);
	}
}

bool verify_model_is_correct(struct i2c_client *client) 
{
    u8 SOC_1, SOC_2;
    u16 msb;

    msleep(200);//Delay at least 150ms(max17048/1/3/4 only)

    //Step 7. Write OCV:write(reg[0x0E], INI_OCVTest_High_Byte, INI_OCVTest_Low_Byte)
    max17048_write_reg(client,0x0E, INI_OCVTEST);

    //Step 7.1 Disable Hibernate (MAX17048/49 only)
    max17048_write_reg(client,0x0A,0x0);

    //Step 7.2. Lock Model Access (MAX17048/49/58/59 only)
    max17048_write_reg(client, max17048_MODEL_ACCESS_REG, max17048_MODEL_ACCESS_LOCK);

    //Step 8: Delay between 150ms and 600ms, delaying beyond 600ms could cause the verification to fail
    msleep(500);
 
    //Step 9. Read SOC register and compare to expected result
    msb = max17048_read_reg(client, max17048_SOC_REG);

    SOC_1 = (msb)&(0x00FF);//"big endian":low byte save MSB
    SOC_2 = ((msb)&(0xFF00))>>8;
	
    if(SOC_1 >= INI_SOCCHECKA && SOC_1 <= INI_SOCCHECKB) {
				printk("####max17048:model was loaded successfully####\n");
        return true;
    }
    else {		
				printk("!!!!max17048:model was NOT loaded successfully!!!!\n");
        return false; 
    }   
}

void cleanup_model_load(struct i2c_client *client) 
{	
    u16 original_ocv=0;

    original_ocv = ((u16)((original_OCV_1)<<8)+(u16)original_OCV_2);
    //step9.1, Unlock Model Access (MAX17048/49/58/59 only): To write OCV, requires model access to be unlocked
    max17048_write_reg(client,max17048_MODEL_ACCESS_REG, max17048_MODEL_ACCESS_UNLOCK);

    //step 10 Restore CONFIG and OCV: write(reg[0x0C], INI_RCOMP, Your_Desired_Alert_Configuration)
    //jelphi changed below for soc alert
    max17048_write_reg(client,0x0C, (INI_RCOMP<<8)|0x1E);//RCOMP0=94 , battery empty Alert threshold = 2% -> 0x1E
    max17048_write_reg(client,0x0E, original_ocv); 

    //step 10.1 Restore Hibernate (MAX17048/49 only)
    //do nothing for MAX17048
    
    //step 11 Lock Model Access
    max17048_write_reg(client,max17048_MODEL_ACCESS_REG, max17048_MODEL_ACCESS_LOCK);
    //step 12,//delay at least 150ms before reading SOC register
    mdelay(200); 
}
void handle_model(struct i2c_client *client,int load_or_verify) 
{
    	bool model_load_ok = false;
    	u16 check_times = 0;
    	u16 status,msb;
	
    	//firstly check POR
    	//status = max17048_read_reg(client, 0x1A);
	//if(!((status>>8)&0x01))  //if por is not set,do nothing
	  //	return;
	
	  //remember the OCV
	do {
	  	  //unlock
	  	max17048_write_reg(client, max17048_MODEL_ACCESS_REG, max17048_MODEL_ACCESS_UNLOCK);
    	  	msleep(100);  
    	  	//read first time OCV
        	msb = max17048_read_reg(client, 0x0E);//read OCV             
        	original_OCV_1 = (msb)&(0x00FF);//"big endian":low byte  to MSB
        	original_OCV_2 = ((msb)&(0xFF00))>>8;

		printk("max17048: handle_model got OCV 0x%x\n", msb);
		
		if(check_times++ >= 3) {
		    check_times = 0;
		    printk("max17048:failed to read OCV...");
		    break;
		}
    	}while ((original_OCV_1==0xFF)&&(original_OCV_2==0xFF));//verify Model Access Unlocked

	do {
		if (load_or_verify == LOAD_MODEL) {		
      			// Steps 1-4		
	    		prepare_to_load_model(client);
			// Step 5
			load_model(client);
        	}
		
        	// Steps 6-9
        	model_load_ok = verify_model_is_correct(client);
        	if (!model_load_ok) {
            		load_or_verify = LOAD_MODEL;
        	}
			
		if (check_times++ >= 3) {
			check_times = 0;
		    	printk("max17048 handle model :time out1...");
		    	break;
		}
    	} while (!model_load_ok);

    	// Steps 10-12
    	cleanup_model_load(client);
		
    	//clear up por
    	status = max17048_read_reg(client, 0x1A);
    	status = swab16(status) & 0xef; //need to do Msb and lsb exchanging jelphi
    	max17048_write_reg(client,0x1A,status);
}

//update 
void update_rcomp(struct i2c_client *client) 
{    
	int NewRCOMP = INI_RCOMP;
	u16 cfg=0;
	int temp = 25;
	struct max17048_chip *chip = i2c_get_clientdata(client);

	temp = get_batt_temp()/10;//get temperature input
	
	chip->temp = temp;
	if(temp > 20) {
		NewRCOMP += (int)((temp-20) * INI_TempCoUp/1000);
	} 
	else if(temp < 20) {
		NewRCOMP += (int)((temp-20) * INI_TempCoDown/1000);
	} 

	if(NewRCOMP > 0xFF){
		NewRCOMP = 0xFF;
	}
	else if(NewRCOMP < 0) 
		NewRCOMP = 0;
	
    cfg = ((NewRCOMP&0xFF) << 8)|0x1c;	//soc alert:4%   
    max17048_write_reg(client, 0x0c, cfg);
    //msleep(150); //I think the subsequent code is delay work, this msleep is redundant 
    chip->rcomp = NewRCOMP;
}

extern bool is_charger_enabled(void);
extern bool is_in_otg_mode(void);
extern int get_vchg(void);
extern int is_charger_plug_in(void);
static int get_batt_status(struct max17048_chip* chip)
{
	if(is_charger_plug_in()&&!is_in_otg_mode()&&is_charger_enabled())
	{
		if(chip->smoothed_soc == 100)
			chip->status = POWER_SUPPLY_STATUS_FULL;
		else
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
	}else
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	return 0;
}
#define SOC_CHANGE_PER_SEC		5
#define SHUT_DOWN_VOLT		3450
#define SHUT_DOWN_VOLT_SEC		60
#define SOC_NOT_NEED_SMOOTH_SEC		10*60
bool full_capacity = false;
static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

static int calculate_delta_time(unsigned long *time_stamp, int *delta_time_s)
{
	unsigned long now_tm_sec = 0;

	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;

	if (get_current_time(&now_tm_sec)) {
		pr_err("RTC read failed\n");
		return 0;
	}

	*delta_time_s = (now_tm_sec - *time_stamp);

	/* remember this time */
	*time_stamp = now_tm_sec;
	return 0;
}
static int bound_soc(int soc)
{
	soc = max(0, soc);
	soc = min(100, soc);
	return soc;
}
static int soc_remap(int soc,bool is_charging)
{
	int mapped_soc = 0;
	if(soc >= 87)
	{
		mapped_soc = bound_soc(soc+1);
		if(soc < 98)
			return mapped_soc;
		if(is_charging && soc != 100)
			return 99;
		else
			return mapped_soc;
	}
#if 0
	if(soc < 87 && soc >= 80)
	{
		return bound_soc(soc+1);
	}
	if(soc < 80 && soc >= 74)
	{
		return bound_soc(soc);
	}
	if(soc < 74 && soc >= 60)
	{
		return bound_soc(soc - 1);
	}
	if(soc < 60 && soc >= 36)
	{
		return bound_soc(soc - 2);
	}
	if(soc < 36 && soc >= 28)
	{
		return bound_soc(soc - 1);
	}
	if(soc < 28 && soc >=0)
	{
		return bound_soc(soc);
	}
#endif
	return bound_soc(soc);
}
/*smooth_soc func
 *parameter: soc_param struct. you need set the follwing members
 *1. calculated_soc : the soc read from fuel gauge IC
 *2. batt_volt: battery voltage, use it to monitor the shutdown voltage
 *3. charging: battery charging status
 *this func will do following calibrations:
 *1. smooth the soc, if system don't enter suspend, soc only can be changed
 *with 1% step. if system suspend is more than SOC_NOT_NEED_SMOOTH_SEC,report
 *the calculated_soc directly.
 *2. when battery voltage is lower than SHUT_DOWN_VOLT during SHUT_DOWN_VOLT_SEC
 *report 0% directly.
 *3. soc will keep a same value more than 0.5 minitues.
 *4. soc don't increase if battery is not charging
 * */
static int smooth_soc(struct soc_param *soc_p)
{
	int soc=0;
	int soc_change = 0;
	int time_since_last_change_sec = 0;
	unsigned long last_change_sec;

	soc = soc_p->calculated_soc;

	last_change_sec = soc_p->last_soc_change_sec;
	calculate_delta_time(&last_change_sec, &time_since_last_change_sec);

	if((time_since_last_change_sec < 30) && (soc_p->last_soc != 0))
	{
//		if(abs(soc-soc_p->last_soc) > 0)
//		{
//			printk("%s:last soc is %d,current soc is %d,detla time is %d\n",__func__,soc_p->last_soc,soc,time_since_last_change_sec);
		return soc_p->last_soc;
//		}
	}
	if (soc_p->last_soc != 0) {
		/*
		 * last_soc < soc  ... if we have not been charging at all
		 * since the last time this was called, report previous SoC.
		 */
		if (soc_p->last_soc < soc && !soc_p->charging)
		{
			printk("%s,last_soc <soc,not charging,last_soc:%d,current_soc:%d\n",__func__,soc_p->last_soc,soc);
			soc = soc_p->last_soc;
		}
#if 1
		/*
		 * last_soc > soc  ... if the battery curent is <= 0
		 * report previous SoC.
		 */
		if (soc_p->last_soc > soc && soc_p->charging)
		{
			/*sometimes,even charger is work,but battery still in discharging status,
			 * in this case ,we use battery current to present the charging status, when current > 0, 
			 it means battery is discharging; current,soc can't increase;
			 when current <= 0, it means battery is charging,soc can't decrease
			*/
			
			if(soc_p->current_now <= -1100)
				soc = soc_p->last_soc;
			printk("%s,last soc > soc,and charging current is %d\n",__func__,soc_p->current_now);
		}
#endif
		if(time_since_last_change_sec != 0)
			soc_change = min((int)abs(soc_p->last_soc - soc),
				time_since_last_change_sec / SOC_CHANGE_PER_SEC);
		else
			soc_change = abs(soc_p->last_soc - soc);
		/*
		 *normally soc will be updated periodically with a short time(20s), when system enter
		 *suspend , this interval will be larger and soc will be recalibrated by OCV. so when
		 *it is larger than SOC_NOT_NEED_SMOOTH_SEC.we don't do soc smooth, report it directly.
		 * */
		if(time_since_last_change_sec < SOC_NOT_NEED_SMOOTH_SEC)
		{
			soc_change = min(1, soc_change);
		}

		if (soc < soc_p->last_soc)
			soc = soc_p->last_soc - soc_change;
		if (soc > soc_p->last_soc)
			soc = soc_p->last_soc + soc_change;
	}
	//report shutdown capacity when battery voltage is below 3450mV.
	if(soc_p->batt_volt < SHUT_DOWN_VOLT)
	{
		if(soc_p->start_shut_down_mon == true)
			soc_p->shut_down_volt_sec += time_since_last_change_sec;
		soc_p->start_shut_down_mon = true;
		if(soc_p->shut_down_volt_sec > SHUT_DOWN_VOLT_SEC)
		{
			printk("%s,battery voltage is too low during %d sec,report 0\n",__func__,soc_p->shut_down_volt_sec);
			soc_change = 100;
			soc = 0;
		}
	}else{
		soc_p->start_shut_down_mon = false;
		soc_p->shut_down_volt_sec = 0;
	}
	if(soc_change != 0)
	{
		printk("%s,battery capacity change %d ,update\n",__func__,soc_change);
	//	if(bqdi != NULL)
	//		power_supply_changed(&bqdi->bat_psy);
	}
	soc_p->last_soc_change_sec = last_change_sec;

	pr_debug("%s,last_soc = %d, calculated_soc = %d, soc = %d, time since last change = %d\n",__func__,
			soc_p->last_soc, soc_p->calculated_soc,
			soc, time_since_last_change_sec);
	soc_p->last_soc = bound_soc(soc);
	if(soc_p->last_soc == 100)
		full_capacity = true;
	else
		full_capacity = false;
	if(soc_p->last_soc < 2 && (soc_p->batt_volt > 3600 || is_in_call == 1))
	{
		printk("%s,soc is below 2, but voltage %d is large than 3400mV or phone is in call\n",__func__,soc_p->batt_volt);
		soc_p->last_soc = 2;
	//	fg_reg_show(bqdi);
	}
	if(soc_p->batt_volt < SHUT_DOWN_VOLT && soc_p->charging)
	{
		printk("%s,voltage %d is below 3450,report 0\n",__func__,soc_p->batt_volt);
		soc_p->last_soc = 0;
	}
	return soc_p->last_soc;
}
static void max17048_get_soc(struct i2c_client *client);
static int get_batt_soc(struct max17048_chip *chip)
{
	get_batt_status(chip);
	/*	update latest soc here */
	if(charger_is_done)
	chip->soc = 100;
	else
    max17048_get_soc(chip->client);
	chip->soc_param.charging = (chip->status == POWER_SUPPLY_STATUS_CHARGING || chip->status == POWER_SUPPLY_STATUS_FULL);
	chip->soc_param.current_now = 1;
	chip->soc_param.calculated_soc = soc_remap(chip->soc,chip->soc_param.charging);
	chip->soc_param.batt_volt = chip->vcell;
	chip->smoothed_soc = smooth_soc(&chip->soc_param);
	if(soc_log_count)
	{
//	printk("max17048_log:soc is :%d,smoothed soc is :%d\n",chip->soc,smoothed_soc);
	soc_log_count--;
	}
	return chip->smoothed_soc;
}
static int max17048_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17048_chip *chip = container_of(psy,
				struct max17048_chip, fgbattery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		get_batt_status(chip);
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    	max17048_get_vcell(chip->client);
		val->intval = chip->vcell*1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_batt_soc(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_batt_temp();
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if(chip->status == POWER_SUPPLY_STATUS_DISCHARGING)
		val->intval = 1*get_vchg()*1000/1410;
		else
		val->intval = -1*get_vchg()*1000/1410;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->charger_enabled;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77819_get_battery_health();
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17048_batt_property_is_writeable(struct power_supply *psy,
                                    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		break;
	}
	return 0;
}
extern int max77819_charger_enable (int en);
static int max17048_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct max17048_chip *chip = container_of(psy,
				struct max17048_chip, fgbattery);
    switch(psp){
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        if(val->intval){
			chip->charger_enabled = 1;				
		}else{
			chip->charger_enabled = 0;				
		}
		max77819_charger_enable(chip->charger_enabled);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
/*
static void max17048_reset(struct i2c_client *client)
{
	max17048_write_reg(client, max17048_CMD_REG, 0x5400);//
}
*/

static void max17048_get_vcell(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	u16 fg_vcell = 0;
	u32 vcell_mV = 0;

	fg_vcell = max17048_read_reg(client, max17048_VCELL_REG);
	vcell_mV = (u32)(((fg_vcell & 0xFF)<<8) + ((fg_vcell & 0xFF00)>>8))*5/64;//78125uV/(1000*1000) = 5/64 mV/cell
	chip->vcell = vcell_mV;

//	printk("max17048:chip->vcell = \t%d\t mV\n", chip->vcell);
}

static void max17048_get_soc(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	u16 fg_soc = 0;
//	static u8 soc_count = 1;

	fg_soc = max17048_read_reg(client, max17048_SOC_REG);

	chip->soc = ((u16)(fg_soc & 0xFF)<<8) + ((u16)(fg_soc & 0xFF00)>>8);
	pr_debug("max17048:chip->soc = %d, fg_soc = %d\n", chip->soc, fg_soc);

	if(INI_BITS == 19) {
	    chip->soc = chip->soc/512;
	}else if(INI_BITS == 18){
	    chip->soc = chip->soc/256;
	}

	if(chip->soc>100)	chip->soc = 100;
	/*
	if((soc_count++)%5 == 0){
	    soc_count = 1;
	    printk("max17048:Get SOC = %d\n", chip->soc);
	}
	*/
}

static void max17048_get_version(struct i2c_client *client)
{
    u16 fg_version = 0;

    fg_version = max17048_read_reg(client, max17048_VER_REG);
    chip_version = ((u16)(fg_version & 0xFF)<<8) + ((u16)(fg_version & 0xFF00)>>8);
    //chip_version = swab16(fg_version);

    dev_info(&client->dev, "max17048 Fuel-Gauge Ver 0x%04x\n", chip_version);
}

#if 0
static void max17048_get_online(struct i2c_client *client)
{
    struct max17048_chip *chip = i2c_get_clientdata(client);

    //if (chip->pdata->battery_online)
	//		chip->online = chip->pdata->battery_online();
    //else
			chip->online = 1;
}

static void max17048_get_status(struct i2c_client *client)
{
    struct max17048_chip *chip = i2c_get_clientdata(client);

/*
    if (!chip->pdata->charger_online || !chip->pdata->charger_enable) {
	    chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
	    return;
    }

    if (chip->pdata->charger_online()) {
	if (chip->pdata->charger_enable())
	    chip->status = POWER_SUPPLY_STATUS_CHARGING;
	else
	    chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    } else {
	chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
    }
*/
    if (chip->soc > max17048_BATTERY_FULL)
	    chip->status = POWER_SUPPLY_STATUS_FULL;
}
#endif

static void max17048_work(struct work_struct *work)
{
    struct max17048_chip *chip;
	static u8 log_count = 1;
    chip = container_of(work, struct max17048_chip, work.work);

    max17048_get_vcell(chip->client);
    max17048_get_soc(chip->client);
    update_rcomp(chip->client);		//update rcomp periodically
    //max17048_get_online(chip->client);
    //max17048_get_status(chip->client);

	if((log_count++)%3 == 0){
	    log_count = 1;
	    printk("max17048_log,%d,%d,%d,%d,%d\n",chip->vcell,chip->temp,chip->soc,chip->rcomp,get_vbus()/1000);
	}
    schedule_delayed_work(&chip->work, max17048_DELAY);
}

static void max17048_handle_work(struct work_struct *work)
{
    struct max17048_chip *chip;
    
    chip = container_of(work, struct max17048_chip, hand_work.work);
    handle_model(chip->client,LOAD_MODEL);
    schedule_delayed_work(&chip->hand_work,HZ*60*60);
}
/***********************************************************************************
 * Use ALRT. When battery capacity is below alert_soc_threshold,
 * ALRT PIN will pull up and cause a
 * interrput, this is the interrput callback.
 ***********************************************************************************/
static irqreturn_t max17048_interrupt(int irq, void *_chip)
{
    struct max17048_chip *chip = _chip;

    schedule_delayed_work(&chip->notifier_work, 0);

    return IRQ_HANDLED;
}

/*===========================================================================
  Function:       interrupt_notifier_work
  Description:    send a notifier event to sysfs
  Calls:
  Called By:
  Input:          struct work_struct *
  Output:         none
  Return:         none
  Others:         none
===========================================================================*/
static void interrupt_notifier_work(struct work_struct *work)
{
    struct max17048_chip *chip = container_of(work,struct max17048_chip,notifier_work.work);
    u16 data = 0;
    int soc = 0;
    chip = container_of(work,struct max17048_chip,notifier_work.work);
    max17048_get_soc(chip->client);
	soc = chip->soc;
    printk("low battery intr = %d\n",soc);
    data = max17048_read_reg(chip->client, 0x0c);//soc alert status bit in config register 
    
    if(((data>>8) & 0x20) == 0x20){ //check alert bit in config register: 0x0c
        data = swab16(data) & 0xffdf; 
        max17048_write_reg(chip->client,0x0C, data); //clr soc alert bit
       // if(soc >= 1)
       	//	pm_wakeup_event(chip->client->dev,200);
      //  else
        //	pm_stay_awake(chip->client->dev);
    }
	if(soc <= 1)
	wake_lock_timeout(&battery_wakelock,5*HZ);
    power_supply_changed(&chip->fgbattery);
    return;
}
static void max17048_hw_init(struct i2c_client*client)
{
	//set init rcomp and soc alert value
	max17048_write_reg(client,0x0C, (INI_RCOMP<<8)|0x1E);//battery empty Alert threshold = 2% -> 0x1E --19bit
	//vreset value
	max17048_write_reg(client,0x18, (((2800/40) << 1) << 8));//preset vreset to 2.8v
	return;
	
}

static enum power_supply_property max17048_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

extern bool max17048_tmp_chip_init;
static int max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);			
	struct max17048_chip *chip;
    struct device_node *dev_node = client->dev.of_node;
	int ret;

	if (!max17048_tmp_chip_init) {
		pr_err("tmp chip not found deferring probe\n");
		return -EPROBE_DEFER;
	}
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "Failed: I2C bus function is not correct\n");		
		return -EIO;
	}
	
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Failed: OOM\n");
		return -ENOMEM;
	}
	chip->client = client;	
	i2c_set_clientdata(client, chip);
	
	max17048_get_batt_info();
	handle_model(client,LOAD_MODEL);
	
	chip->fgbattery.name		= "battery";
	chip->fgbattery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->fgbattery.get_property	= max17048_get_property;
    chip->fgbattery.property_is_writeable = max17048_batt_property_is_writeable;
	chip->fgbattery.set_property	= max17048_set_property;
	chip->fgbattery.properties	= max17048_battery_props;
	chip->fgbattery.num_properties	= ARRAY_SIZE(max17048_battery_props);
//	printk("max17048:soc parameter,%d,%d,%d,%d,%lu,%d,%d,%d\n",chip->soc_param.calculated_soc,chip->soc_param.last_soc,chip->soc_param.batt_volt,chip->soc_param.current_now,chip->soc_param.last_soc_change_sec,chip->soc_param.shut_down_volt_sec,chip->soc_param.charging,chip->soc_param.start_shut_down_mon);
	//init soc_smooth parameter
	chip->soc_param.calculated_soc = 0;
	chip->soc_param.last_soc = 0;
	chip->soc_param.batt_volt = 0;
	chip->soc_param.current_now = 0;
	chip->soc_param.last_soc_change_sec = 0;
	chip->soc_param.shut_down_volt_sec = 0;
	chip->soc_param.charging = 0;
	chip->soc_param.start_shut_down_mon = 0;
	ret = power_supply_register(&client->dev, &chip->fgbattery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err_out;
	}

	//max17048_reset(client);
	max17048_get_version(client);
  /*added by jelphi**********************************/
	//max17048_write_reg(client, 0x0c, 0x941C);
	
	max17048_hw_init(client);
	if (dev_node == NULL) {
		pr_err("max17048:Device Tree node doesn't exist.\n");
		ret = -ENODEV;
		goto err_out;
	}
	else{
		chip->alert_gpio = of_get_named_gpio(dev_node, "max17048,alert-gpio", 0);
		pr_err("max17048:alert_gpio = %d.\n", chip->alert_gpio);
		if (gpio_request(chip->alert_gpio, "max17048_alert_gpio") < 0){
		pr_err("max17048:alert_gpio = %d request err\n", chip->alert_gpio);
        ret = -ENOMEM;
        goto err_free_gpio;
    }

    gpio_direction_input(chip->alert_gpio);
    chip->alert_irq = gpio_to_irq(chip->alert_gpio);
    /* request irq interruption */
    ret = request_irq(chip->alert_irq, max17048_interrupt,
        IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,"max17048_irq",chip);
    if(ret){
        pr_err("max17048 request alert_irq failed \n");               
        goto err_free_irq;
    }else{
        enable_irq_wake(chip->alert_irq);
    }
	}
	wake_lock_init(&battery_wakelock,WAKE_LOCK_SUSPEND,"battery_wakelock");
	/***************************************************/
	max17048_get_soc(chip->client);
	max17048_get_vcell(chip->client);
	printk("max17048:init soc is:%d,init vcell is:%d,init vbus is:%d\n",chip->soc,chip->vcell,get_vbus()/1000);
	INIT_DELAYED_WORK(&chip->work, max17048_work);
	INIT_DELAYED_WORK(&chip->hand_work, max17048_handle_work);
	INIT_DELAYED_WORK(&chip->notifier_work,interrupt_notifier_work);//jelphi added for support alert int
	schedule_delayed_work(&chip->hand_work,HZ*60*60);
	schedule_delayed_work(&chip->work, 0);

	return 0;
	
/*jelphi added below for supporting alert_int*/
err_free_irq:
	if (chip->alert_irq > 0)
		free_irq(chip->alert_irq, chip);
err_free_gpio:
		gpio_free(chip->alert_gpio);
/*********************************************/
err_out:
	kfree(chip);
	return ret;
}

static int max17048_remove(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->fgbattery);
	cancel_delayed_work(&chip->work);
	if (chip->alert_irq > 0)
		free_irq(chip->alert_irq, chip);
	gpio_free(chip->alert_gpio);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM

static int max17048_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&chip->work);
	cancel_delayed_work_sync(&chip->hand_work);
	return 0;
}

static int max17048_resume(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	schedule_delayed_work(&chip->work, max17048_DELAY);
	schedule_delayed_work(&chip->hand_work,HZ*60*60);
	power_supply_changed(&chip->fgbattery);
	return 0;
}

#else

#define max17048_suspend NULL
#define max17048_resume NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id max17048_of_ids[] = {
    { .compatible = "maxim,max17048" },
    { },
};
MODULE_DEVICE_TABLE(of, max17048_of_ids);
#endif /* CONFIG_OF */

static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17048_id);

static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name	= "max17048",
#ifdef CONFIG_OF
    		.of_match_table  = max17048_of_ids,
#endif /* CONFIG_OF */				
	},
	.probe		= max17048_probe,
	.remove		= max17048_remove,
	.suspend		= max17048_suspend,
	.resume		= max17048_resume,
	.id_table	= max17048_id,
};

module_i2c_driver(max17048_i2c_driver);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("max17048 Fuel Gauge");
MODULE_LICENSE("GPL");
