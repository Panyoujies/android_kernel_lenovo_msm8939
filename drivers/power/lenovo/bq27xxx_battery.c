/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 */
/*
   BQ27531+BQ2419x for K5

 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/reboot.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/string.h>
#include <linux/power/bq27x00_battery.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include "bq275x_firmware.h"
#include "bq275x_firmware_2.h"
#define BQ27XXX_REG_DUMP

#define DRIVER_VERSION			"1.2.0"

#define bq27xxx_REG_CNTL		0x00 /*control*/
#define BQ27x00_REG_TEMP		0x06 /*Temperature*/
#define BQ27x00_REG_VOLT		0x08 /*Voltage*/
#define BQ27x00_REG_FLAGS		0x0A /*FLAGS*/
#define BQ27x00_REG_NAC			0x0C /* Nominal available capaciy */
#define BQ27x00_REG_FAC			0x0E /* Full Available capaciy */
#define bq27xxx_REG_RM			0x10 /*Remaining Capacity*/
#define bq27xxx_REG_FCC			0x12 /*FullChargeCapacity*/
#define BQ27x00_REG_AI			0x14 /*AverageCurrent*/
#define BQ27xxx_REG_INTER_TEMP  0x16 /*internal temp*/
#define BQ27x00_REG_CYCT		0x1E /* Cycle count total */
#define BQ27500_REG_SOC			0x20 /*SOC*///!!!!!!!!!!!!!!!!!!!!!need to be modified as firmware updated!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define BQ27500_REG_TRC			0x6c
#define BQ27500_REG_TFCC		0x70
#define BQ27x00_REG_QMAX		0x62
#define bq27xxx_REG_CCV			0x72 /*CalcChargingVoltage*/
#define bq27xxx_REG_RCUF		0x6C /*remain capacity unfilt*/
#define bq27xxx_REG_RCF			0x6E /*remain capacity filt*/
#define bq27xxx_REG_FCCU		0x70 /*full charge capacity unfilt*/
#define bq27xxx_REG_FCCF		0x70 /*full charge capacity filt*/
#define bq27xxx_REG_TC			0x74 /*true capacity*/
#define BQ27500_FLAG_DSC		BIT(0)/*Discharging detected*/
#define BQ27500_FLAG_SOC1		BIT(2)/*Soc threshold 1*/
#define BQ27500_FLAG_BAT_DET	BIT(3)/*bat_detected*/
#define BQ27500_FLAG_CHG_EN		BIT(8)/*enable charge*/
#define BQ27500_FLAG_FC			BIT(9)/*Full-charged condition reached*/
#define BQ27500_FLAG_CNTL_EN		BIT(10)/*Full-charged condition reached*/
#define BQ27500_FLAG_UT			BIT(14)/*Undertemperature*/
#define BQ27500_FLAG_OT			BIT(15)/*Overtemperature*/

#define data_flash_ver_1 0xAA05
#define data_flash_ver_2 0xAB03

int is_testmode;

int soc_prev = 0;
volatile int rd_count=0;
volatile int htemp_cnt=0;
int temp_prev = 2986;
struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
};


enum current_level {
    CURRENT_LOW,
    CURRENT_HIGH,
    CURRENT_UNDEFINE,
};

enum usbin_health {
	USBIN_UNKNOW,
	USBIN_OK,
	USBIN_OVP,
	USBIN_DPM,
};

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int flags;
	int voltage;
	int current_now;
};

struct bq27xxx_platform_data{
   unsigned fg_int_gpio; 
   u32  fg_int_flag;
};

/**
 *calculated_soc: SOC read from the fuel gauge IC
 *last_soc: SOC which report last time
 *last_soc_report_time: last soc report time, use this member to calculated the soc report time interval  
 *charging: if battery is charging
 *
 * */
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
enum batt_type_enum{
	BATT_1 = 0,
	BATT_ATL_F,
	BATT_LG_N,
	BATT_BK_F,
	BATT_2,
};
struct batt_id_inf
{
	bool support_fast_chg;
	int batt_id_volt;
	int batt_type;
};
struct bq27x00_device_info {
	struct device 		*dev;
    struct bq27xxx_platform_data *board;

	struct bq27x00_reg_cache cache;
	struct soc_param soc_param;
	struct delayed_work firmware_up_work;
#ifdef BQ27XXX_REG_DUMP	
	struct delayed_work reg_dump_work;
#endif
	struct power_supply	bat_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*qpnp_bat_psy;
	struct bq27x00_access_methods bus;
	int fg_irq_n;
	int df_ver;
	struct batt_id_inf *batt_id;
	int prev_usb_max_ma;
	int prev_ibat_max_ma;
	int charger_en;

	struct mutex lock;
};

struct bq27x00_device_info *bqdi;

static enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
/*
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
*/
	POWER_SUPPLY_PROP_TECHNOLOGY,
/*
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
*/
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,	
//	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static unsigned int bc_fw = 0;

static unsigned int fw_stat = 0;
module_param(fw_stat, uint, 0444);
MODULE_PARM_DESC(fw_stat, "get fw download state");

static unsigned int fw_delay = 0;
module_param(fw_delay, uint, 0644);
MODULE_PARM_DESC(fw_delay, "Get fw download delay");

static unsigned int fw_long_delay = 70;
module_param(fw_long_delay, uint, 0644);
MODULE_PARM_DESC(fw_long_delay, "Get fw download delay");

static unsigned int fw_more_delay = 70;
module_param(fw_more_delay, uint, 0644);
MODULE_PARM_DESC(fw_long_delay, "Get fw download delay");

static unsigned int tim = 60;
module_param(tim, uint, 0644);
MODULE_PARM_DESC(tim, "The value is charge state update time");

/*static int bat_status = 0;*/

static volatile unsigned int is_in_call = 0;

static inline int bq27x00_read(struct bq27x00_device_info *di, u8 reg,
		bool single)
{
	return di->bus.read(di, reg, single);
}
static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret,i;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0){
		if(is_testmode!=0)
		{
			pr_err("bq27xxx : in testmode do not retry\n");
			return ret;
		}

		for(i=0;i<3;i++)
		{//sleep 100ms to wait device and master ready
			pr_err("bq27531 i2c read reg 0x%x error\n",reg);
			msleep(100);
			ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
			if(ret > 0)
				goto ret_true;
		}
		return ret;
	}
ret_true:
	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];
	return ret;
}

static int bq27x00_write_i2c(struct bq27x00_device_info *di, u8 reg, int num, unsigned char *buf,int rom_mode)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[1];
    unsigned char *data;
    int i = 0;
	int ret = 0;

	if (!client->adapter)
		return -ENODEV;
    
	data = kzalloc(sizeof(char) * (num+1), GFP_KERNEL);
    
    data[0] = reg;
    for(i=0;i<num;i++)
        data[i+1] = buf[i];
    

	if(rom_mode)
		msg[0].addr = 0x0b;
	else
		msg[0].addr = client->addr;

	msg[0].flags = 0;
	msg[0].buf = data;
	msg[0].len = num+1;
    /*printk("%s: i2c addr0 is 0x%x, addr1=0x%x \n",__func__,msg[0].addr,msg[1].addr);*/

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
    /*ret = i2c_master_send(client, data, num+1);*/
    
	if (ret < 0)
	{
		pr_err("bq27531 write reg 0x%x error\n",reg);
	}
    kfree(data);

	return ret;
}

static void fg_reg_show(struct bq27x00_device_info *di)
{

	unsigned char data[2]={0};
	int ret,fg_cont,fg_rm_cap,fg_fc_cap,fg_cc_volt,
		nac,fac,rcuf,rcf,tc,fccu,temp;
	int internal_temp = 0;

	ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);
	if(ret <0){
		printk("%s: status cmd write error\n",__func__);
	}
	mdelay(50);
	nac = bq27x00_read(di,BQ27x00_REG_NAC, false);//nominal available capacity
	fac = bq27x00_read(di,BQ27x00_REG_FAC, false);//full available capacity
	rcuf = bq27x00_read(di,bq27xxx_REG_RCUF, false);//remain capacity unflittered,mA units
	rcf = bq27x00_read(di,bq27xxx_REG_RCF, false);//remain capacity flittered,mA units
	fccu = bq27x00_read(di,bq27xxx_REG_FCCU, false);//full capacity flittered
	tc = bq27x00_read(di,bq27xxx_REG_TC, false);//true capacity

	fg_cont = bq27x00_read(di,bq27xxx_REG_CNTL, false);
	fg_rm_cap = bq27x00_read(di,bq27xxx_REG_RM,false);
	fg_fc_cap = bq27x00_read(di,bq27xxx_REG_FCC,false);
	fg_cc_volt = bq27x00_read(di,bq27xxx_REG_CCV,false);
	temp = bq27x00_read(di,BQ27x00_REG_TEMP,false);
	internal_temp = bq27x00_read(di,BQ27xxx_REG_INTER_TEMP,false);
	dev_info(di->dev,"nac=%d,fac=%d,rcuf=%d,rcf=%d,tc=%d,fccu=%d",
					nac,fac,rcuf,rcf,tc,fccu);
	dev_info(di->dev,"Ctrl=%d,rc=%d,fc=%d,ccv=%d,temp=%d\n",
					fg_cont,fg_rm_cap,fg_fc_cap,fg_cc_volt,temp-2731);
	dev_info(di->dev,"internal temp is %d\n",internal_temp - 2731);
}
#define SOC_CHANGE_PER_SEC		5
#define SHUT_DOWN_VOLT		3400
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
 *3. soc will keep a same value more than 1 minitues.
 *4. soc don't increase if battery is not charging
 * */
static int fg_reset_c = 0;
static void bq27xxx_soft_reset(void);
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
		if(abs(soc-soc_p->last_soc) > 0)
		{
			printk("%s:last soc is %d,current soc is %d,detla time is %d\n",__func__,soc_p->last_soc,soc,time_since_last_change_sec);
			return soc_p->last_soc;
		}
	}
	if (soc_p->last_soc != 0) {
		/*
		 * last_soc < soc  ... if we have not been charging at all
		 * since the last time this was called, report previous SoC.
		 */
		if (soc_p->last_soc < soc && !soc_p->charging)
			soc = soc_p->last_soc;
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
			if(soc_p->current_now <= 0)
				soc = soc_p->last_soc;
			printk("last soc > soc,and charging current is %d\n",soc_p->current_now);
		}
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
		printk("battery voltage is too low during %d sec,report 0\n",soc_p->shut_down_volt_sec);
		if(soc_p->shut_down_volt_sec > SHUT_DOWN_VOLT_SEC)
		{
			soc_change = 100;
			soc = 0;
		}
	}else{
		soc_p->start_shut_down_mon = false;
		soc_p->shut_down_volt_sec = 0;
	}
	if(soc_change != 0)
	{
		printk("battery capacity change %d ,update\n",soc_change);
	//	if(bqdi != NULL)
	//		power_supply_changed(&bqdi->bat_psy);
	}
	soc_p->last_soc_change_sec = last_change_sec;

	pr_debug("last_soc = %d, calculated_soc = %d, soc = %d, time since last change = %d\n",
			soc_p->last_soc, soc_p->calculated_soc,
			soc, time_since_last_change_sec);
	soc_p->last_soc = bound_soc(soc);

	/*sometimes, fg can't resume to normal when battery temp return from a cold temp,
	 * so its soc will always keep as 0,in this case do fg reset*/
	if(soc_p->calculated_soc == 0 && soc_p->batt_volt > 3700)
	{
		printk("calculated_soc error,reset fuel gauge\n");	
		if(bqdi != NULL)
		{
			fg_reg_show(bqdi);
			if(fg_reset_c > 10)
				bq27xxx_soft_reset();
			fg_reset_c++;
		}
	}else
		fg_reset_c = 0;

	if(soc_p->last_soc == 100)
		full_capacity = true;
	else
		full_capacity = false;
	if(soc_p->last_soc < 2 && (soc_p->batt_volt > 3400 || is_in_call == 1))
	{
		printk("soc is below 2, but voltage %d is large than 3400mV or phone is in call\n",soc_p->batt_volt);
		soc_p->last_soc = 2;
		if(bqdi != NULL)
			fg_reg_show(bqdi);
	}
	return soc_p->last_soc;
}

extern bool is_charging_enabled(void);
extern int is_charger_plug_in(void);
extern bool is_in_otg_mode(void);
static int bq27x00_battery_status(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	if(is_charger_plug_in()&&!is_in_otg_mode()&&is_charging_enabled())
	{
		if(di->cache.capacity == 100)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
	}else
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	return 0;
}
static int soc_remap(int soc,bool is_charging)
{
	int mapped_soc = 0;
	if(soc >= 87)
	{
		mapped_soc = bound_soc(soc+2);
		if(soc < 98)
			return mapped_soc;
		if(is_charging && soc != 100)
			return 99;
		else
			return mapped_soc;
	}
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
	return bound_soc(soc);
}
extern bool charger_charging_done(void);
static int open_soc_remap = 1;
module_param(open_soc_remap, uint, 0644);
static int bq27x00_battery_current(struct bq27x00_device_info *di,
		union power_supply_propval *val);
static int bq27x00_battery_read_rsoc(struct bq27x00_device_info *di)
{
	int rsoc = -1;
	int volt = -1;
	union power_supply_propval battery_status;
	union power_supply_propval current_now;

	bq27x00_battery_status(di,&battery_status);
	bq27x00_battery_current(di,&current_now);
	volt = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	rsoc = bq27x00_read(di, BQ27500_REG_SOC, false);
	if (rsoc < 0 || volt < 0)
	{
		dev_err(di->dev, "error reading soc %d or volt %d\n",rsoc,volt);
		return di->soc_param.last_soc;
	}
	pr_info("bq27xx rsoc %d volt %d\n",rsoc,volt);
	if(charger_charging_done())
		rsoc = 100;
//	else if(rsoc == 100)
//		rsoc = 99;
	di->soc_param.charging = (battery_status.intval == POWER_SUPPLY_STATUS_CHARGING || battery_status.intval == POWER_SUPPLY_STATUS_FULL);
	di->soc_param.current_now = (int)current_now.intval;
	if(open_soc_remap == 1)
		di->soc_param.calculated_soc = soc_remap(rsoc,di->soc_param.charging);
	else
		di->soc_param.calculated_soc = rsoc;
	di->soc_param.batt_volt = volt;
	return smooth_soc(&di->soc_param);
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27x00_read(di, BQ27x00_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27x00_battery_temperature(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	if(di->qpnp_bat_psy == NULL)
	{	di->qpnp_bat_psy = power_supply_get_by_name("battery_qpnp");
		if(di->qpnp_bat_psy == NULL)
		{
			val->intval = 250;
			return 0;
		}
	}
	di->qpnp_bat_psy->get_property(di->qpnp_bat_psy,POWER_SUPPLY_PROP_TEMP,val);
	di->cache.temperature = val->intval;
	return 0;
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	int curr;
	curr = bq27x00_read(di, BQ27x00_REG_AI, false);
	val->intval = (int)((s16)curr);
	val->intval *= -1000;
	pr_debug("current now is %d\n",val->intval);
	return 0;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	int volt;

	volt = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	if (volt < 0)
		return volt;

	val->intval = volt * 1000;

	return 0;
}


static irqreturn_t fg_thread_handler(int irq, void *devid)
{
	struct bq27x00_device_info *di = (struct bq27x00_device_info *)devid;
	/*int ret,chrg_fault;*/
	dev_info(di->dev, "FG ISR\n");
	return IRQ_HANDLED;
}
static int bq27x00_simple_value(int value,
		union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
		struct bq27x00_device_info, bat_psy);

static int bq27x00_batt_property_is_writeable(struct power_supply *psy,
                                    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}
extern int charger_enable_charger(int val);
static int
bq27x00_battery_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

    switch(psp){
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        if(val->intval){
			di->charger_en = 1;				
		}else{
			di->charger_en = 0;				
		}
		charger_enable_charger(di->charger_en);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		di->prev_usb_max_ma = val->intval;
        	break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		di->prev_ibat_max_ma = val->intval;
//		pr_info("set ibatmax with %d\n",di->prev_ibat_max_ma);
		
//		if(di->df_ver == data_flash_ver_n)
//			di->prev_ibat_max_ma = 1200;
		break;
    default:
        return -EINVAL;
    }
    
    return 0;
}
extern int charger_get_battery_health(void);
extern int fast_charger_state(void);
static int bq27x00_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {

		case POWER_SUPPLY_PROP_STATUS:
			ret = bq27x00_battery_status(di, val);
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			ret = bq27x00_battery_voltage(di, val);
			break;

		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
				val->intval = di->charger_en;
				break;

		case POWER_SUPPLY_PROP_PRESENT:
			ret = bq27x00_read(di, BQ27x00_REG_FLAGS, false);
			val->intval = (ret & BQ27500_FLAG_BAT_DET)>0 ? 1:0;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
			ret = bq27x00_battery_current(di, val);
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			di->cache.capacity = bq27x00_battery_read_rsoc(di);
			ret = bq27x00_simple_value(di->cache.capacity, val);
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = bq27x00_battery_temperature(di, val);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = charger_get_battery_health();
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			val->intval = bq27x00_battery_read_cyct(di);
		case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
			val->intval = di->prev_usb_max_ma;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			if(di->prev_ibat_max_ma > 2000)
				val->intval = 4000;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = fast_charger_state();
			if(ret<=0)
				val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			else
				val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;	
			
		default:
			return -EINVAL;
	}
	return ret;
}

int fw_ver_get(struct bq27x00_device_info *di)
{

	int ret;
	unsigned char data[2]={0};
    /*unsigned int count = 0;*/

    data[0]=0x02;
    data[1]=0x00;
    /*do{*/
        ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);
        if(ret <0){
            printk("%s: status cmd write error %d\n",__func__,ret);
        }

        mdelay(50);
        ret = bq27x00_read(di,bq27xxx_REG_CNTL, false);
        printk("%s: FW ver ret is 0x%x\n",__func__,ret);
        /*count++;*/
        /*mdelay(70);*/
    /*}while(ret == 0x284 && count < 100);*/

	return ret;
}

int df_ver_get(struct bq27x00_device_info *di)
{

	int ret;
	unsigned char data[2]={0};

	data[0]=0x1F;
	data[1]=0x00;
	ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);
	if(ret <0){
		printk("%s: status cmd write error\n",__func__);
	}

	mdelay(50);
	ret = bq27x00_read(di,bq27xxx_REG_CNTL, false);
	printk("%s: DF ver ret is 0x%x\n",__func__,ret);

	return ret;
}

static int fg_version_get(struct bq27x00_device_info *di)
{

	int ret;
	unsigned char data[2]={0};

    data[0]=0x01;
    data[1]=0x00;

    ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);
    if(ret <0){
        printk("%s: status cmd write error %d\n",__func__,ret);
    }

    mdelay(50);
    ret = bq27x00_read(di,bq27xxx_REG_CNTL, false);
    /*printk("%s: FG ver ret is 0x%x\n",__func__,ret);*/
    
    return ret;

}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = container_of(psy, struct bq27x00_device_info,
								bat_psy);
	union power_supply_propval ret = {0,};

	if(!di->usb_psy)
	{
		di->usb_psy = power_supply_get_by_name("usb");
		if (!di->usb_psy) {
			pr_err("usb supply not found deferring probe\n");
			return;
		}
	}
	di->usb_psy->get_property(di->usb_psy,POWER_SUPPLY_PROP_CURRENT_MAX, &ret);

	if (di->prev_usb_max_ma == ret.intval)
		goto skip_set_iusb_max;

	di->prev_usb_max_ma = ret.intval / 1000;
//	charger_set_iusb_max(di->prev_usb_max_ma);
skip_set_iusb_max:
	pr_debug("end of power supply changed\n");
}
/*(static char *batt_supplied_to[] = {
	"usb",
};*/
static int bq27x00_power_supply_init(struct bq27x00_device_info *di)
{
	int ret;
	di->bat_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat_psy.properties = bq27x00_battery_props;
	di->bat_psy.num_properties = ARRAY_SIZE(bq27x00_battery_props);
	di->bat_psy.get_property = bq27x00_battery_get_property;
	di->bat_psy.set_property = bq27x00_battery_set_property;
    di->bat_psy.property_is_writeable = bq27x00_batt_property_is_writeable;
	di->bat_psy.external_power_changed = bq27x00_external_power_changed;
//	di->bat_psy.supplied_to = batt_supplied_to;

	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat_psy);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}


	return 0;
}


static u32 bq275xx_read_i2c_4byte(struct bq27x00_device_info *di, u8 reg,int num)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[4]={0};
	u32 ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = 0x0B;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = 0x0B;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	msg[1].len = num;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;
	ret = get_unaligned_le32(data);

	return ret;
}
int firmware_write(struct bq27x00_device_info *di)
{
	int i,j=0,t,offset,size;
	unsigned char data[32]={0};
	u32 ret;
	u32 fw_data;
	unsigned char reg;
	int bqfs_size = 0;
	int retry=0;
	int fw_update_err=0;
	struct bqfs *bq;
	BQFS *bqfs_index = NULL;
	unsigned char *firmware_data = NULL;

	if(di->df_ver == data_flash_ver_1)
	{
		bqfs_index = bq275xx_bqfs_index;
		firmware_data = bq275xx_firmware_data;
	}else
	{
		bqfs_index = bq275xx_bqfs_index_2;
		firmware_data = bq275xx_firmware_data_2;
	}

	bqfs_size = sizeof(bq275xx_bqfs_index)/sizeof(*bq);
/*write:	for(i=0;i<(sizeof(bqfs_index)/sizeof(*bq));i++)*/
	for(retry=0;retry<3;retry++){
		data[0]=0x00;
		data[1]=0x0f;
		ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);//Enter ROM Mode
		if(ret <0){
			printk("%s: reg 0x00 write error\n",__func__);
			return -1;
		}else{
			printk("%s: reg 0x00 enter rom mode whitout com error\n",__func__);
		}
		msleep(1000);
		fw_update_err = 0;
		for(i=0;i< bqfs_size;i++){
			for(j=0;j<32;j++)
				data[j]=0;
			j=0;
			t=0;
			offset = bqfs_index[i].data_offset;
			size = bqfs_index[i].data_size;

			if(bqfs_index[i].i2c_cmd == 'W')//write reg
			{
				reg = firmware_data[offset];
				j = offset;
				while(j<(offset+size-1)){
				//	printk("%s:The reg= 0x%x data= 0x%x.\n",__func__,reg,firmware_data[j+1]);
					ret = bq27x00_write_i2c(di,reg,1,&firmware_data[j+1],1);
									if(ret < 0)
											printk("fw ret %d.\n",ret);
					reg+=1;
					j++;
				}
			}else if(bqfs_index[i].i2c_cmd == 'X')//delay
			{

				if(bqfs_index[i].i2c_addr < 10)
					msleep(bqfs_index[i].i2c_addr+fw_delay);
				else if(bqfs_index[i].i2c_addr == 170)
					msleep(bqfs_index[i].i2c_addr+fw_long_delay);
				else
					msleep(bqfs_index[i].i2c_addr+fw_more_delay);

			}
			else if(bqfs_index[i].i2c_cmd == 'C')//compare data 
			{
				reg = firmware_data[offset];
				j = offset;
				ret = bq275xx_read_i2c_4byte(di,reg,size-1);
						//	printk("%s: Reg= 0x%x,mult_data= 0x%x\n",__func__,reg,ret);
				while(j<(offset+size-1)){
					data[t] = firmware_data[j+1];
					j++;
					t++;
				}
				fw_data = get_unaligned_le32(data);
			//	printk("%s: Reg= 0x%x,fw_data= 0x%x\n",__func__,reg,fw_data);
				if(ret != fw_data){
					fw_update_err = 1;
					printk("%s:Error The reg= 0x%x data=0x%x fw_data=0x%x compare fault.\n",__func__,reg,ret,fw_data);
					break;
				}
			}
		}
		if(!fw_update_err)
			break;
	}
		data[0]=0x0f; 
		ret=bq27x00_write_i2c(di,0x00,1,data,1);//Exit ROM Mode
		data[0]=0x0f;
		ret=bq27x00_write_i2c(di,0x64,1,data,1);//Exit ROM Mode
		if(ret <0)
			printk("%s: Reg 0x64 write error\n",__func__);
		data[0]=0x00;
		ret=bq27x00_write_i2c(di,0x65,1,data,1);//Exit ROM Mode
		if(ret <0)
			printk("%s: Reg 0x65 write error\n",__func__);
		data[0]=0x41;
		data[1]=0x00;
		ret=bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,data,0);/*Reset*/
		if(ret <0){
			pr_err("%s: reset cmd write error\n",__func__);
			return -1;
		}
	di->df_ver = df_ver_get(di);
	msleep(1000);
	return 0;
}

int bq27xxx_set_temp(int temp)
{
	int ret = 0;
	char data[2]={0};
	temp = temp + 2731;
	data[0] = temp & 0xFF;
	data[1] = (temp >> 8) & 0xFF;
//	printk("temp %d is %x:%x\n",temp,data[0],data[1]);
	if(bqdi == NULL)
		return 0;
	ret = bq27x00_write_i2c(bqdi,BQ27x00_REG_TEMP,2,data,0);
	if(ret < 0)
		pr_err("set battery temp error\n");
	return ret;
}
int bq27xxx_get_temp(void)
{
	int temp;

	if(bqdi == NULL)
		return 250;
	temp = bq27x00_read(bqdi, BQ27x00_REG_TEMP, false);
	temp = temp - 2731;

	pr_info("bq27xx temp %d\n",temp);
	
	return temp;
}

static void bq27xxx_soft_reset(void)
{
	char data[2]={0};
	int ret = 0;
	int batt_voltage,soc,batt_temp;

	if(bqdi == NULL)
		return;

	batt_temp = bq27xxx_get_temp();
	batt_voltage = bq27x00_read(bqdi, BQ27x00_REG_VOLT, false);
	soc = bq27x00_read(bqdi, BQ27500_REG_SOC, false);
	if(batt_temp < -100 && batt_voltage > 3700 && soc <= 1)
	{
		data[0]=0x41;
		data[1]=0x00;
		printk("phone power on with a very low temperature, do soft reset\n");
		ret=bq27x00_write_i2c(bqdi,bq27xxx_REG_CNTL,2,data,0);/*Reset*/
		if(ret <0){
			pr_err("%s: reset cmd write error\n",__func__);
			return;
		}
	}
}

static int fw_write(const char *val, struct kernel_param *kp)
{
	int ret;

	param_set_int(val, kp);

	if(bc_fw == 1){
		ret = firmware_write(bqdi);
		if (!ret){
			dev_info(bqdi->dev, "FG FW download complete\n");
			fw_stat = 1;
		}
		else{
			dev_err(bqdi->dev, "failed to download FW: %d\n", ret);
			fw_stat = 0;
		}
	}
	else{
		dev_err(bqdi->dev, "Error!Only write 1 to download: %d\n", bc_fw);
	}

    printk("bq27xxx %s end.\n",__func__);

	return 0;
}

module_param_call(bc_fw,fw_write,NULL,&bc_fw,0644);
MODULE_PARM_DESC(bc_fw, "Download FW in FG and Charger");

static ssize_t bc_fw_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;

	sscanf(buf, "%x", &cmd);
	if(cmd == 1){
		ret = firmware_write(bqdi);
		if (!ret){
			dev_info(bqdi->dev, "FG FW download complete\n");
			fw_stat = 1;
		}
		else{
			dev_err(bqdi->dev, "failed to download FW: %d\n", (int) ret);
			fw_stat = 0;
		}
	}
	else{
		dev_err(bqdi->dev, "Error!Only write 1 to download: %d\n", bc_fw);
	}

	return ret;
}


ssize_t  fg_ver_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    int ret;
    ret = fg_version_get(bqdi);

    if(ret == 0x0531){
	    ret = sprintf(buf, "%s\n", "bq27531");
    }else if(ret == 0x0532){
	    ret = sprintf(buf, "%s\n", "bq27532");
    }else{
	    ret = sprintf(buf, "%s\n", "bq275xx");
	}
    
    return ret;
}

static ssize_t is_in_call_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);

	if(bqdi == NULL)
		return ret;
	sscanf(buf, "%x", &is_in_call);
	printk("phone call status is %d\n",is_in_call);
	power_supply_changed(&bqdi->bat_psy);
	return ret;
}

ssize_t  is_in_call_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    int ret;
	ret = sprintf(buf, "%d\n", is_in_call);    
    return ret;
}
static DEVICE_ATTR(fw_write, S_IRUGO|S_IWUSR, NULL,bc_fw_write);
static DEVICE_ATTR(is_in_call, S_IRUGO|S_IWUSR, is_in_call_get,is_in_call_set);
static DEVICE_ATTR(fg_version, S_IRUGO|S_IWUSR, fg_ver_get,NULL);
static struct attribute *fs_attrs[] = {
	&dev_attr_fw_write.attr,
	&dev_attr_fg_version.attr,
	&dev_attr_is_in_call.attr,
	NULL,
};

static struct attribute_group fs_attr_group = {
	.attrs = fs_attrs,
};

static unsigned int ic_reg = 0;
static int bc_reg_show(const char *val, struct kernel_param *kp)
{
	int ret;
	param_set_int(val, kp);

	if(ic_reg < 0)
		return -1;

	if(ic_reg <= 0x73)
	{
		ret = bq27x00_read(bqdi,ic_reg,false);
		dev_info(bqdi->dev, "FG reg: 0x%x = 0x%x\n",ic_reg,ret);
		return 0;
	}
	else
	{
		ret = bq27x00_read(bqdi,ic_reg,true);
		dev_info(bqdi->dev, "CHRG reg: 0x%x = 0x%x\n",ic_reg,ret);
		return 0;
	}
}
module_param_call(ic_reg,bc_reg_show,NULL,&ic_reg,0644);
MODULE_PARM_DESC(ic_reg, "Download FW in FG and Charger");

static unsigned int fuel_gauge_regs = 0;
static int fuel_gauge_regs_show(const char *val, struct kernel_param *kp)
{
	fg_reg_show(bqdi);
	fuel_gauge_regs = 0;
	return 0;
}
module_param_call(fuel_gauge_regs,NULL,fuel_gauge_regs_show,NULL,0644);
static unsigned int fw_version = 0;
static int fw_reg_show(const char *val, struct kernel_param *kp)
{

	param_set_int(val, kp);
	fw_ver_get(bqdi);
	df_ver_get(bqdi);

	return 0;
}
module_param_call(fw_version,NULL,fw_reg_show,&fw_version,0644);
MODULE_PARM_DESC(fw_version, "Download FW in FG and Charger");

static int bq27xxx_parse_dt(struct device *dev,
                struct bq27xxx_platform_data *bq27xxx_pdata)
{
	struct device_node *np = dev->of_node;

	/* reset, irq gpio info */
	bq27xxx_pdata->fg_int_gpio = of_get_named_gpio_flags(np,
			"bq27530,fg-int-gpio", 0, 0);
	return 0;
}
static struct batt_id_inf batt_id_array[] = {
	{true,1800,BATT_2},
	{true,0,BATT_1}
};
extern int get_batt_id_volt(void);
struct batt_id_inf* get_batt_id_inf(void)
{
	int batt_id_volt = get_batt_id_volt()/1000;
	int batt_id = 0;
	if(batt_id_volt < 0 || batt_id_volt > (1800+100))
	{
		pr_err("batt id voltage = %d out of range\n",batt_id_volt);
		return NULL;
	}
	pr_info("battery id voltage is %d\n",batt_id_volt);
	for(batt_id = 0; batt_id < ARRAY_SIZE(batt_id_array);batt_id++)
	{
		if(abs(batt_id_volt - batt_id_array[batt_id].batt_id_volt) < 900)
			return &batt_id_array[batt_id];	
	}
	return NULL;
}
void firmware_up_worker(struct work_struct *work)
{
	struct bq27x00_device_info *di =
	    container_of(work, struct bq27x00_device_info, firmware_up_work.work);
	int ret = 0;
	printk("in testmode, update fuel gauge firmware in worker\n");
	ret = firmware_write(di);
	if (!ret){
		dev_info(di->dev, "FG FW update work complete,ver is 0x:%x\n",di->df_ver);
	}
	else{
		dev_err(di->dev, "step 2 failed to update in worker,df_ver:0x%x\n",di->df_ver);
	}
}

#ifdef BQ27XXX_REG_DUMP
#define reg_debug_timer 5000

void reg_dump_worker(struct work_struct *work)
{
	struct bq27x00_device_info *di =
	    container_of(work, struct bq27x00_device_info, reg_dump_work.work);
	int buf[16] = {0};
	char status_ctrl_data[2] = {0x00, 0x00};
	struct timespec ts;
	struct rtc_time tm;
	int hour;
	
	buf[0] = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	buf[1] = bq27x00_read(di, BQ27x00_REG_AI, false);
	buf[2] = bq27x00_read(di, BQ27x00_REG_TEMP, false);
	buf[3] = bq27x00_read(di, BQ27500_REG_SOC, false);
	buf[4] = bq27x00_read(di, bq27xxx_REG_RM, false);
	buf[5] = bq27x00_read(di, bq27xxx_REG_FCC, false);
	buf[6] = bq27x00_read(di, BQ27500_REG_TRC, false);
	buf[7] = bq27x00_read(di, BQ27500_REG_TFCC, false);
	buf[8] = bq27x00_read(di, BQ27x00_REG_FLAGS, false);
	bq27x00_write_i2c(di,bq27xxx_REG_CNTL,2,status_ctrl_data,0);
	buf[9] = bq27x00_read(di, bq27xxx_REG_CNTL, false);
	buf[10] = bq27x00_read(di, BQ27x00_REG_QMAX, false);
	buf[11] = bq27x00_read(di, 0x64, false);
	buf[12] = bq27x00_read(di, 0x66, false);
	buf[13] = bq27x00_read(di, 0x68, false);
	buf[14] = bq27x00_read(di, 0x6a, false);

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	hour = tm.tm_hour+8;
	if(hour>24)
		hour -=24;
	
	pr_err("bq27xxx dump: %02d:%02d:%02d, %d, %d, %d, %d, %d, %d, %d, %d, 0x%x, 0x%x, %d, 0x%x, 0x%x, 0x%x, 0x%x\n", 
		hour, tm.tm_min, tm.tm_sec,
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12],buf[13], buf[14]);
	
	schedule_delayed_work(&di->reg_dump_work, msecs_to_jiffies(reg_debug_timer));
}
#endif

static int bq27x00_battery_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq27x00_device_info *di;
	int retval = 0;
	int ret;
	int fw_ver;
	int bat_flag;
    struct bq27xxx_platform_data *platform_data = NULL;
	struct power_supply *qpnp_psy;

	qpnp_psy = power_supply_get_by_name("battery_qpnp");
	if (!qpnp_psy) {
		pr_err("qpnp supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}

    printk("%s %s.\n",__func__,client->name);

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(*platform_data),
			GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		retval = bq27xxx_parse_dt(&client->dev, platform_data);
		if (retval)
			return retval;
	} else {
		platform_data = client->dev.platform_data;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->dev = &client->dev;
	di->bat_psy.name = "battery";
	di->bus.read = &bq27x00_read_i2c;
	di->board = platform_data;

	i2c_set_clientdata(client, di);

	if (gpio_is_valid(platform_data->fg_int_gpio)) {
        ret = gpio_request(platform_data->fg_int_gpio, "FG_INT");
        if (ret) {
            dev_err(di->dev,
                "Failed to request gpio %d with error %d\n",
                platform_data->fg_int_gpio, ret);
        }else{
            di->fg_irq_n = gpio_to_irq(platform_data->fg_int_gpio);
            ret = request_irq(di->fg_irq_n,fg_thread_handler,IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND, "bq27531",di);
            if (ret) {
                dev_err(di->dev, "cannot get IRQ:%d\n", di->fg_irq_n);
            } else {
                dev_err(di->dev, "Chrg IRQ No:%d\n", di->fg_irq_n);
                enable_irq_wake(di->fg_irq_n);
            }
        }
    }

	mdelay(100);
	bat_flag = bq27x00_read(di, BQ27x00_REG_FLAGS, false);
	dev_info(di->dev, "0x0A REG ret = 0x%x\n",bat_flag);

	fw_ver = fw_ver_get(di);
	mdelay(100);

	/*check battery id to choose the suitable firmware*/
	di->batt_id = get_batt_id_inf();
	if(di->batt_id == NULL)
		di->df_ver = data_flash_ver_1;
	else if(di->batt_id->batt_type==BATT_2)
		di->df_ver = data_flash_ver_2;
	else
		di->df_ver = data_flash_ver_1;

	if(di->df_ver != df_ver_get(di))
	{
		if(is_testmode == 0)
		{
			ret = firmware_write(di);
			if (!ret){
				dev_info(di->dev, "FG FW update step2 complete,ver is 0x:%x\n",di->df_ver);
			}
			else{
				dev_err(di->dev, "step 2 failed to update,df_ver:0x%x\n",di->df_ver);
			}
		}else
			dev_info(di->dev, "in testmode do not update firmware\n");
	}

    dev_info(di->dev, "power supply_init\n");
    if(bq27x00_power_supply_init(di))
		goto batt_failed_3;
	di->charger_en = 1;				
	retval = sysfs_create_group(&di->bat_psy.dev->kobj,&fs_attr_group);
	if (retval) {
		dev_err(di->dev, "failed to setup sysfs ret = %d\n", retval);
	}
	di->qpnp_bat_psy = qpnp_psy;
	bqdi= di;
	bq27xxx_soft_reset();

#ifdef BQ27XXX_REG_DUMP
	if(is_testmode == 0)
	{
		INIT_DELAYED_WORK(&di->reg_dump_work,  reg_dump_worker);
		schedule_delayed_work(&di->reg_dump_work, msecs_to_jiffies(1000));
	}else 
		dev_info(di->dev, "in testmode do not run reg dump\n");
#endif

	return 0;

batt_failed_3:
	kfree(di);
batt_failed_2:
    kfree(platform_data);

	return retval;
}

#ifdef CONFIG_PM
static int bq27x00_battery_suspend(struct device *dev)
{

	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	/*disable_irq(di->chrg_irq_n);*/

	dev_info(di->dev, "Bq27531 suspend\n");

	return 0;

}
static int bq27x00_battery_resume(struct device *dev)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	/*enable_irq(di->chrg_irq_n);*/

	dev_info(di->dev, "Bq27531 resume\n");

	return 0;
}
#else
#define bq27x00_battery_suspend NULL
#define bq27x00_battery_resume NULL
#endif
static const struct dev_pm_ops bq27x00_pm_ops = {
	.suspend		= bq27x00_battery_suspend,
	.resume			= bq27x00_battery_resume,
};
/*
static struct notifier_block fg_reboot_notifier = {
	.notifier_call = fg_reboot_notifier_call,
};
*/
static const struct of_device_id bq27xxx_match[] = {
	{ .compatible = "ti,bq27xxx-battery" },
	{ },
};
static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27xxx", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27x00_id);

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
		.owner	= THIS_MODULE,
		.pm = &bq27x00_pm_ops,
		.of_match_table = of_match_ptr(bq27xxx_match),
	},
	.probe = bq27x00_battery_probe,
	.id_table = bq27x00_id,
};

static inline int bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	printk("%s:bq27xxx register_i2c driver\n",__func__);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");

	return ret;
}

static inline void bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

/*
 * Module stuff
 */

static int __init bq27x00_battery_init(void)
{
	int ret;
	ret = bq27x00_battery_i2c_init();

	return ret;
}
module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_i2c_exit();
}
module_exit(bq27x00_battery_exit);

static int __init early_testmode(char *p)
{
	is_testmode = simple_strtoul(p,NULL,0);
	return 0;
}
early_param("testmode",early_testmode);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
