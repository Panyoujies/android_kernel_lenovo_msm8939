/*
 * bq2589x_charger.c - Charger driver for TI bq2589x,BQ24191 and BQ24190
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 * Author: Raj Pandey <raj.pandey@intel.com>
 */
//#define DEBUG

#define EXT_CHARGER_POWER_SUPPLY
#define LENOVO_OTG_USB_SHORT

#include <linux/module.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/sfi.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/version.h>
#include <linux/usb/otg.h>
#include <linux/rpmsg.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/power/bq2589x_charger.h>

#define DRV_NAME "bq2589x_charger"
#define DEV_NAME "bq2589x"

/*
 * D0, D1, D2 can be used to set current limits
 * and D3, D4, D5, D6 can be used to voltage limits
 */
#define bq2589x_INPUT_SRC_CNTL_REG		0x0
#define INPUT_SRC_CNTL_EN_HIZ			(1 << 7)
#define BATTERY_NEAR_FULL(a)			((a * 98)/100)
/*
 * set input voltage lim to 4.68V. This will help in charger
 * instability issue when duty cycle reaches 100%.
 */
#define INPUT_SRC_VOLT_LMT_DEF                 (3 << 4)
#define INPUT_SRC_VOLT_LMT_444                 (7 << 3)
#define INPUT_SRC_VOLT_LMT_468                 (5 << 4)
#define INPUT_SRC_VOLT_LMT_476                 (0xB << 3)

#define INPUT_SRC_VINDPM_MASK                  (0xF << 3)
#define INPUT_SRC_LOW_VBAT_LIMIT               3600
#define INPUT_SRC_MIDL_VBAT_LIMIT              4200
#define INPUT_SRC_MIDH_VBAT_LIMIT              4300
#define INPUT_SRC_HIGH_VBAT_LIMIT              4350

/* D0, D1, D2 represent the input current limit */
#define INPUT_SRC_CUR_LMT0		0x0	/* 100mA */
#define INPUT_SRC_CUR_LMT1		0x1	/* 150mA */
#define INPUT_SRC_CUR_LMT2		0x2	/* 500mA */
#define INPUT_SRC_CUR_LMT3		0x3	/* 900mA */
#define INPUT_SRC_CUR_LMT4		0x4	/* 1200mA */
#define INPUT_SRC_CUR_LMT5		0x5	/* 1500mA */
#define INPUT_SRC_CUR_LMT6		0x6	/* 2000mA */
#define INPUT_SRC_CUR_LMT7		0x7	/* 3000mA */

/*
 * D1, D2, D3 can be used to set min sys voltage limit
 * and D4, D5 can be used to control the charger
 */
#define bq2589x_POWER_ON_CFG_REG		0x1
#define POWER_ON_CFG_RESET			(1 << 7)
#define POWER_ON_CFG_I2C_WDTTMR_RESET		(1 << 6)
/* BQ2419X series charger and OTG enable bits */
#define CHR_CFG_BIT_POS				4
#define CHR_CFG_BIT_LEN				2
#define CHR_CFG_CHRG_MASK			3
#define POWER_ON_CFG_CHRG_CFG_DIS		(0 << 4)
#define POWER_ON_CFG_CHRG_CFG_EN		(1 << 4)
#define POWER_ON_CFG_CHRG_CFG_OTG		(3 << 4)
/* BQ2429X series charger and OTG enable bits */
#define POWER_ON_CFG_BQ29X_OTG_EN		(1 << 5)
#define POWER_ON_CFG_BQ29X_CHRG_EN		(1 << 4)
#define POWER_ON_CFG_BOOST_LIM			(1 << 0)

/*
 * Charge Current control register
 * with range from 500 - 4532mA
 */
#define bq2589x_CHRG_CUR_CNTL_REG		0x2
#define bq2589x_CHRG_CUR_OFFSET		500	/* 500 mA */
#define bq2589x_CHRG_CUR_LSB_TO_CUR	64	/* 64 mA */
#define bq2589x_GET_CHRG_CUR(reg) ((reg>>2)*bq2589x_CHRG_CUR_LSB_TO_CUR\
			+ bq2589x_CHRG_CUR_OFFSET) /* in mA */
#define bq2589x_CHRG_ITERM_OFFSET       128
#define bq2589x_CHRG_CUR_LSB_TO_ITERM   128

/* Pre charge and termination current limit reg */
#define bq2589x_PRECHRG_TERM_CUR_CNTL_REG	0x3
#define bq2589x_TERM_CURR_LIMIT_128		0	/* 128mA */
#define bq2589x_PRE_CHRG_CURR_256		(1 << 4)  /* 256mA */

/* Charge voltage control reg */
#define bq2589x_CHRG_VOLT_CNTL_REG	0x6
#define bq2589x_CHRG_VOLT_OFFSET	3504	/* 3504 mV */
#define bq2589x_CHRG_VOLT_LSB_TO_VOLT	16	/* 16 mV */
/* Low voltage setting 0 - 2.8V and 1 - 3.0V */
#define CHRG_VOLT_CNTL_BATTLOWV		(1 << 1)
/* Battery Recharge threshold 0 - 100mV and 1 - 300mV */
#define CHRG_VOLT_CNTL_VRECHRG		(1 << 0)
#define bq2589x_GET_CHRG_VOLT(reg) ((reg>>2)*bq2589x_CHRG_VOLT_LSB_TO_VOLT\
			+ bq2589x_CHRG_VOLT_OFFSET) /* in mV */

/* Charge termination and Timer control reg */
#define bq2589x_CHRG_TIMER_EXP_CNTL_REG		0x5
#define CHRG_TIMER_EXP_CNTL_EN_TERM		(1 << 7)
#define CHRG_TIMER_EXP_CNTL_TERM_STAT		(1 << 6)
/* WDT Timer uses 2 bits */
#define WDT_TIMER_BIT_POS			4
#define WDT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_WDTDISABLE		(0 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT40SEC		(1 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT80SEC		(2 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT160SEC		(3 << 4)
#define WDTIMER_RESET_MASK			0x40
/* Safety Timer Enable bit */
#define CHRG_TIMER_EXP_CNTL_EN_TIMER		(1 << 3)
/* Charge Timer uses 2bits(20 hrs) */
#define SFT_TIMER_BIT_POS			1
#define SFT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_SFT_TIMER		(3 << 1)

#define bq2589x_CHRG_THRM_REGL_REG		0x6

#define bq2589x_MISC_OP_CNTL_REG		0x7
#define MISC_OP_CNTL_DPDM_EN			(1 << 7)
#define MISC_OP_CNTL_TMR2X_EN			(1 << 6)
#define MISC_OP_CNTL_BATFET_DIS			(1 << 5)
#define MISC_OP_CNTL_BATGOOD_EN			(1 << 4)
/* To mask INT's write 0 to the bit */
#define MISC_OP_CNTL_MINT_CHRG			(1 << 1)
#define MISC_OP_CNTL_MINT_BATT			(1 << 0)

#define bq2589x_SYSTEM_STAT_REG			0x8
/* D6, D7 show VBUS status */
#define SYSTEM_STAT_VBUS_BITS			(3 << 6)
#define SYSTEM_STAT_VBUS_UNKNOWN		0
#define SYSTEM_STAT_VBUS_HOST			(1 << 5)
#define SYSTEM_STAT_VBUS_CDP			(2 << 5)
#define SYSTEM_STAT_VBUS_DCP			(3 <<5)
#define SYSTEM_STAT_VBUS_HVDCP		(4<< 5)
#define SYSTEM_STAT_VBUS_UNSTD		(5<< 5)
#define SYSTEM_STAT_VBUS_NONSTD		(6<< 5)
#define SYSTEM_STAT_VBUS_OTG			(3 << 6)
/* D4, D5 show charger status */
#define SYSTEM_STAT_NOT_CHRG			(0 << 3)
#define SYSTEM_STAT_PRE_CHRG			(1 << 3)
#define SYSTEM_STAT_FAST_CHRG			(2 << 3)
#define SYSTEM_STAT_CHRG_DONE			(3 << 3)
#define SYSTEM_STAT_DPM				(1 << 3)
#define SYSTEM_STAT_PWR_GOOD			(1 << 2)
#define SYSTEM_STAT_THERM_REG			(1 << 1)
#define SYSTEM_STAT_VSYS_LOW			(1 << 0)
#define SYSTEM_STAT_CHRG_MASK			(3 << 3)

#define bq2589x_FAULT_STAT_REG			0x9
#define FAULT_STAT_WDT_TMR_EXP			(1 << 7)
#define FAULT_STAT_OTG_FLT			(1 << 6)
/* D4, D5 show charger fault status */
#define FAULT_STAT_CHRG_BITS			(3 << 4)
#define FAULT_STAT_CHRG_NORMAL			(0 << 4)
#define FAULT_STAT_CHRG_IN_FLT			(1 << 4)
#define FAULT_STAT_CHRG_THRM_FLT		(2 << 4)
#define FAULT_STAT_CHRG_TMR_FLT			(3 << 4)
#define FAULT_STAT_BATT_FLT			(1 << 3)
#define FAULT_STAT_BATT_TEMP_BITS		(3 << 0)

#define bq2589x_VENDER_REV_REG			0xA
/* D3, D4, D5 indicates the chip model number */
#define BQ25852_IC_VERSION			0x0
#define BQ25890_IC_VERSION			0x3
#define bq25895_IC_VERSION			0x7

#define bq2589x_MAX_MEM		12
#define NR_RETRY_CNT		3

#define CHARGER_PS_NAME				"bq2589x_charger"

#define CHARGER_TASK_JIFFIES		(HZ * 30)/* 150sec */
#define CHARGER_HOST_JIFFIES		(HZ * 30) /* 60sec */

#define BATT_TEMP_MAX_DEF	60	/* 60 degrees */
#define BATT_TEMP_MIN_DEF	0
/*GPADC*/
#define MANCONV0	0x72
#define MANCONV1	0x73
#define BPTEMP0_RSLTH	0x7a
#define BPTEMP0_RSLTL	0x7b
#define THERM_ENABLE	0x90
#define ADCIRQ0		0x08

/* Max no. of tries to clear the charger from Hi-Z mode */
#define MAX_TRY		3

/* Max no. of tries to reset the bq2589xi WDT */
#define MAX_RESET_WDT_RETRY 8

#define FAST_CHARGER_VBUS_THRESHOLD 6000
#define FAST_CHARGER_VBUS_OVER_VOLTAGE_THRESHOLD 14000

volatile int charger_enabled = 1;
bool otg_stat = false;

static struct power_supply *fg_psy = NULL;
static struct power_supply *qpnp_psy = NULL;

struct bq2589x_otg_event {
	struct list_head node;
	bool is_enable;
};

enum bq2589x_chrgr_stat {
	bq2589x_CHRGR_STAT_UNKNOWN,
	bq2589x_CHRGR_STAT_CHARGING,
	bq2589x_CHRGR_STAT_BAT_FULL,
	bq2589x_CHRGR_STAT_FAULT,
	bq2589x_CHRGR_STAT_LOW_SUPPLY_FAULT
};

enum bq2589x_chip_type {
	BQ25852, 
	BQ25890, 
	BQ25895,
};

struct bq2589x_otg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};
struct bq2589x_chip {
	struct i2c_client *client;
	struct bq2589x_platform_data *pdata;
	enum bq2589x_chip_type chip_type;
#ifdef EXT_CHARGER_POWER_SUPPLY
	struct power_supply charger_psy;
	int temp_debug_flag;
#endif	
	struct power_supply *usb_psy;
	struct delayed_work chrg_task_wrkr;
	struct delayed_work fault_work;
	struct work_struct otg_evt_work;
	struct notifier_block	otg_nb;
	struct list_head	otg_queue;
	struct mutex event_lock;
	struct usb_phy *transceiver;
	struct bq2589x_otg_regulator	otg_vreg;
	/* Wake lock to prevent platform from going to S3 when charging */
	struct wake_lock wakelock;
	/* timeout Wake lock when charger status changed*/
	struct wake_lock irq_wk;
	spinlock_t otg_queue_lock;
   	 struct regulator *vdd;
	
	/*
	 * regulator v3p3s used by display driver to save 7mW in
	 * S3 for USB Host
	 */


	enum bq2589x_chrgr_stat chgr_stat;
	int cc;
	int cv;
	int ichg_max;
	int max_temp;
	int vbat_cool;
	int vbat_warm;
	int ibat_warm;
	int ibat_cool;
	int min_temp;
	int warm_temp;
	int cool_temp;
	int max_cv;
	int iterm;
	int batt_temp;
	int board_temp;	
	int batt_voltage;
	int batt_status;
	int cntl_state;
	int irq;
	int chg_en_gpio;
	int fast_chg_en_gpio;
	int ext_charger_gpio;
	int otg_en_gpio;
#ifdef LENOVO_OTG_USB_SHORT	
	int otg_usb_short_gpio;
	bool otg_usb_short_state;
#endif
	char ic_name[10];
	bool bat_is_warm;
	bool bat_is_cool;
	bool is_charging_enabled;
	bool is_fast_charging_started;
	bool is_fast_charging_need_change;
	bool a_bus_enable;
	bool is_pwr_good;
	bool boost_mode;
	bool online;
	bool present;
	bool sfttmr_expired;
	bool sfi_tabl_present;
	bool dec_cur_bat_cool;
	bool dec_cur_bat_hot;
	bool charging_en_flags;
	bool fast_charging_shutdown_flag;	
	int vbus;
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *bq2589x_dbgfs_root;
static char bq2589x_dbg_regs[bq2589x_MAX_MEM][4];
#endif

static struct i2c_client *bq2589x_client = NULL;
static int bq2589x_get_chip_version(struct bq2589x_chip *chip);

static bool blk_dec = false;

/*-------------------------------------------------------------------------*/


/*
 * Genenric register read/write interfaces to access registers in charger ic
 */

static int bq2589x_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, value);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
		{//sleep 100ms to wait device and master ready
			pr_err("bq2589x i2c write reg 0x%x to 0x%x error\n",reg,value);
			msleep(100);
			continue;
		}
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Write error:%d\n", ret);

	return ret;
}

static int bq2589x_read_reg(struct i2c_client *client, u8 reg)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
		{//sleep 100ms to wait device and master ready
			pr_err("bq2589x i2c read reg 0x%x error\n",reg);
			msleep(100);
			continue;
		}
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Read error:%d\n", ret);

	return ret;
}

/*
 * If the bit_set is TRUE then val 1s will be SET in the reg else val 1s will
 * be CLEARED
 */
static int bq2589x_reg_read_modify(struct i2c_client *client, u8 reg,
							u8 val, bool bit_set)
{
	int ret;

	ret = bq2589x_read_reg(client, reg);

	if (bit_set)
		ret |= val;
	else
		ret &= (~val);

	ret = bq2589x_write_reg(client, reg, ret);

	return ret;
}

static int bq2589x_reg_multi_bitset(struct i2c_client *client, u8 reg,
						u8 val, u8 pos, u8 len)
{
	int ret;
	u8 data;

	ret = bq2589x_read_reg(client, reg);
	if (ret < 0) {
		dev_warn(&client->dev, "I2C SMbus Read error:%d\n", ret);
		return ret;
	}

	data = (1 << len) - 1;
	ret = (ret & ~(data << pos)) | val;
	ret = bq2589x_write_reg(client, reg, ret);

	return ret;
}

static int bq2589x_reg_read_mask(struct i2c_client *client, u8 reg,
				 u8 mask, u8 shift)
{
	int ret;

	if (shift > 8)
		return -EINVAL;

	ret = bq2589x_read_reg(client, reg);
	if (ret < 0)
		return ret;
	
	return (ret & (mask<<shift)) >> shift;
}

static int bq2589x_reg_write_mask(struct i2c_client *client, u8 reg, u8 val,
				 u8 shift,  u8 mask)
{
	int ret;

	if (shift > 8)
	{
		pr_err("%s shift out range %d\n", __func__, shift);
		return -EINVAL;
	}

	ret = bq2589x_read_reg(client, reg);
	if (ret < 0)
	{
		pr_err("%s read err %d\n", __func__, ret);	
		return ret;
	}
	
	ret &= ~(mask<<shift);
	ret |= val << shift;

	if((reg==BQ2589X_REG_14)&&(shift!=BQ2589X_REG_14_REG_RST))
		ret &= 0x7f;

	return bq2589x_write_reg(client, reg, ret);
}


/*
 * This function dumps the bq2589x registers
 */
static void bq2589x_dump_registers(struct bq2589x_chip *chip)
{
	int ret;
	
//	dev_info(&chip->client->dev, "%s\n", __func__);
#ifndef DEBUG
	dev_info(&chip->client->dev, "%s :", __func__);

	/* Input Src Ctrl register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_00);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Input Src Ctrl reg read fail\n");
	pr_warn( "00 %x; ", ret);

	/* Pwr On Cfg register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_01);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Pwr On Cfg reg read fail\n");
	pr_warn( "01 %x;  ", ret);

	/* Chrg Curr Ctrl register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_03);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Chrg Curr Ctrl reg read fail\n");
	pr_warn( "03 %x;  ", ret);

	/* Pre-Chrg Term register */
	ret = bq2589x_read_reg(chip->client,
					BQ2589X_REG_04);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Pre-Chrg Term reg read fail\n");
	pr_warn( "04 %x;  ", ret);

	/* Chrg Volt Ctrl register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_05);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Chrg Volt Ctrl reg read fail\n");
	pr_warn( "05 %x;  ", ret);

	/* Thermal Regulation register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_06);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
				"Thermal Regulation reg read fail\n");
	}
	pr_warn( "06 %x;  ", ret);

	/* Vendor Revision register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_09);
	if (ret < 0)
		dev_warn(&chip->client->dev, "09 reg read fail\n");
	pr_warn( "09 %x;  ", ret);
	
	/* Vendor Revision register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0A);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	pr_warn( "0A %x;  ", ret);

	/* Vendor Revision register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	pr_warn( "0B %x;  ", ret);

	/* Vendor Revision register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	pr_warn( "0C %x;  ", ret);

	//bq2589x_reg_write_mask(chip->client, BQ2589X_REG_02, 0x3, 6, 0x3);

	/* Vendor Revision register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0D);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	pr_warn( "0D %x \n", ret);
/*
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0E);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	pr_warn( "0E %x \n", ret);
	
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_11);
	if (ret < 0)
		dev_warn(&chip->client->dev, "11 reg read fail\n");
	pr_warn( "11 %x \n", ret);

	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_12);
	if (ret < 0)
		dev_warn(&chip->client->dev, "12 reg read fail\n");
	pr_warn( "12 %x \n", ret);
*/
#else
	int i;

	for(i=0;i<0x14;i++)
	{
		ret = bq2589x_read_reg(chip->client, BQ2589X_REG_00+i);
		if (ret < 0)
			dev_warn(&chip->client->dev, "reg %x read fail\n", i);
		pr_warn("bq25 REG%x %x\n", i, ret);		
	}

	ret = gpio_get_value(chip->chg_en_gpio);
	dev_info(&chip->client->dev, "en io %d\n", ret);
#endif


}

/*
 * This function verifies if the bq2589xi charger chip is in Hi-Z
 * If yes, then clear the Hi-Z to resume the charger operations
 */
static int bq2589x_clear_hiz(struct bq2589x_chip *chip)
{
	int ret, count;

	pr_debug("%s\n", __func__);

	for (count = 0; count < MAX_TRY; count++) {
		/*
		 * Read the bq2589xi REG00 register for charger Hi-Z mode.
		 * If it is in Hi-Z, then clear the Hi-Z to resume the charging
		 * operations.
		 */
		ret = bq2589x_read_reg(chip->client,
				BQ2589X_REG_00);
		if (ret < 0) {
			dev_warn(&chip->client->dev,
					"Input src cntl read failed\n");
			goto i2c_error;
		}

		if (ret & (1<<BQ2589X_REG_00_EN_HIZ)) {
			dev_warn(&chip->client->dev,
						"Charger IC in Hi-Z mode\n");

			/* Clear the Charger from Hi-Z mode */
			ret &= ~(1<<BQ2589X_REG_00_EN_HIZ);

			/* Write the values back */
			ret = bq2589x_write_reg(chip->client,
					BQ2589X_REG_00, ret);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"Input src cntl write failed\n");
				goto i2c_error;
			}
			msleep(150);
		} else {
			pr_debug("Charger is not in Hi-Z\n");
			break;
		}
	}
	return ret;
i2c_error:
	dev_err(&chip->client->dev, "%s\n", __func__);
	return ret;
}

static int bq2589x_reset_regs(struct bq2589x_chip *chip)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_14, 1, BQ2589X_REG_14_REG_RST, BQ2589X_REG_14_REG_RST_MASK);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
				"Input src cntl read failed\n");
		goto i2c_error;
	}

	//bq2589x_dump_registers(chip);
	return ret;
i2c_error:
	dev_err(&chip->client->dev, "%s\n", __func__);
	return ret;
}

static struct power_supply *get_fg_chip_psy(void)
{
	if (fg_psy)
		return fg_psy;

	fg_psy = power_supply_get_by_name("battery");
	return fg_psy;
}

static struct power_supply *get_qpnp_chip_psy(void)
{
	if (qpnp_psy)
		return qpnp_psy;

	qpnp_psy = power_supply_get_by_name("battery_qpnp");
	return qpnp_psy;
}

/**
 * fg_chip_get_property - read a power supply property from Fuel Gauge driver
 * @psp : Power Supply property
 *
 * Return power supply property value
 *
 */
int fg_chip_get_property(enum power_supply_property psp)
{
	union power_supply_propval val;
	int ret = -ENODEV;

	if (!fg_psy)
		fg_psy = get_fg_chip_psy();
	if (fg_psy) {
		ret = fg_psy->get_property(fg_psy, psp, &val);
		if (!ret)
			return val.intval;
	}
	return ret;
}

int qpnp_chip_get_property(enum power_supply_property psp)
{
	union power_supply_propval val;
	int ret = -ENODEV;

	if (!qpnp_psy)
		qpnp_psy = get_qpnp_chip_psy();

	if (qpnp_psy) {
		ret = qpnp_psy->get_property(qpnp_psy, psp, &val);
		if (!ret)
			return val.intval;
	}
	return ret;
}
/**
 * bq2589x_get_battery_health - to get the battery health status
 *
 * Returns battery health status
 */
int bq2589x_get_battery_health(void)
{
	int  temp,vnow;
	struct bq2589x_chip *chip;
	if (!bq2589x_client)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	chip = i2c_get_clientdata(bq2589x_client);

	/* If power supply is emulating as battery, return health as good */
	if (!chip->sfi_tabl_present)
		return POWER_SUPPLY_HEALTH_GOOD;

	/* Report the battery health w.r.t battery temperature from FG */
	temp = fg_chip_get_property(POWER_SUPPLY_PROP_TEMP);
	if (temp == -ENODEV || temp == -EINVAL || chip->dec_cur_bat_hot || blk_dec) {
		dev_err(&chip->client->dev,
				"Failed to read batt profile or board over temp\n");
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	if(chip->is_charging_enabled==0)
	{
		temp = (temp/10)*10;	
		if (temp > chip->max_temp)
			return POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (temp <= chip->min_temp)
			return POWER_SUPPLY_HEALTH_COLD;
	}
	
	/* read the battery voltage */
	vnow = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (vnow == -ENODEV || vnow == -EINVAL) {
		dev_err(&chip->client->dev, "Can't read voltage from FG %d\n",__LINE__);
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}

	/* convert voltage into millivolts */
/*	vnow /= 1000;

	if (vnow > (chip->max_cv+150))
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;*/
	if(chip->vbus>FAST_CHARGER_VBUS_OVER_VOLTAGE_THRESHOLD)
	{
		dev_err(&chip->client->dev, "vbus over voltage %d\n", chip->vbus);
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
		

	return POWER_SUPPLY_HEALTH_GOOD;
}
EXPORT_SYMBOL(bq2589x_get_battery_health);

int charger_get_battery_health(void)
{
	return bq2589x_get_battery_health();
}
EXPORT_SYMBOL(charger_get_battery_health);

/***********************************************************************/

/* convert the input current limit value
 * into equivalent register setting.
 * Note: ilim must be in mA.
 */
static int chrg_ilim_to_reg(int ilim)
{
	int reg;
	struct bq2589x_chip *chip;

	if (!bq2589x_client)
		return -ENODEV;

	chip = i2c_get_clientdata(bq2589x_client);

	//enable ilim
	bq2589x_reg_write_mask(chip->client, BQ2589X_REG_00, 1, BQ2589X_REG_00_EN_ILIM, BQ2589X_REG_00_EN_ILIM_MASK);

	if(ilim<=100)
		reg = 0;
	else if(ilim>=3250)
		reg = BQ2589X_REG_00_ILIM_MASK;
	else
		reg = (ilim - 100)/50;

	return reg;
}

static u8 chrg_iterm_to_reg(int iterm)
{
	u8 reg;

	if (iterm <= 64)
		reg = 0;
	else if(iterm>=1024)
		reg = BQ2589X_REG_05_ITERM_MASK;
	else
		reg = (iterm - 64) /64;
	
	return reg;
}

/* convert the charge current value
 * into equivalent register setting
 */
static u8 chrg_cur_to_reg(int cur)
{
	u8 reg;

	if (cur <= 0)
		reg = 0;
	else if(cur>=5056)
		reg = BQ2589X_REG_04_ICHG_MASK;
	else
		reg = cur /64;
	
	return reg;
}

/* convert the charge voltage value
 * into equivalent register setting
 */
static u8 chrg_volt_to_reg(int volt)
{
	u8 reg;

	if (volt <= 3840)
		reg = 0;
	else if(volt>=4608)
		reg = BQ2589X_REG_06_VREG_MASK;
	else
		reg = (volt - 3840) /16;

	return reg;
}

static u8 vindpm_to_reg(int vindpm)
{
	u8 reg;

	if (vindpm <= 3900)
		reg = 0;
	else if(vindpm>=7000)
		reg = BQ2589X_REG_01_VINDPM_MASK;
	else
		reg = (vindpm - 3900) /100;

	return reg;
}
#if 0
static int bq2589x_enable_hw_term(struct bq2589x_chip *chip, bool hw_term_en)
{
	int ret = 0;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Disable and enable charging to restart the charging */
	ret = bq2589x_reg_multi_bitset(chip->client,
					bq2589x_POWER_ON_CFG_REG,
					POWER_ON_CFG_CHRG_CFG_DIS,
					CHR_CFG_BIT_POS,
					CHR_CFG_BIT_LEN);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
			"i2c reg write failed: reg: %d, ret: %d\n",
			bq2589x_POWER_ON_CFG_REG, ret);
		return ret;
	}

	/* Read the timer control register */
	ret = bq2589x_read_reg(chip->client, bq2589x_CHRG_TIMER_EXP_CNTL_REG);
	if (ret < 0) {
		dev_warn(&chip->client->dev, "TIMER CTRL reg read failed\n");
		return ret;
	}

	/*
	 * Enable the HW termination. When disabled the HW termination, battery
	 * was taking too long to go from charging to full state. HW based
	 * termination could cause the battery capacity to drop but it would
	 * result in good battery life.
	 */
	if (hw_term_en)
		ret |= CHRG_TIMER_EXP_CNTL_EN_TERM;
	else
		ret &= ~CHRG_TIMER_EXP_CNTL_EN_TERM;

	/* Program the TIMER CTRL register */
	ret = bq2589x_write_reg(chip->client,
				bq2589x_CHRG_TIMER_EXP_CNTL_REG,
				ret);
	if (ret < 0)
		dev_warn(&chip->client->dev, "TIMER CTRL I2C write failed\n");

	return ret;
}
#endif

/*
 * chip->event_lock need to be acquired before calling this function
 * to avoid the race condition
 */
static int program_timers(struct bq2589x_chip *chip, int wdt_duration,
				bool sfttmr_enable)
{
	int ret;

	/* Read the timer control register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_07);
	if (ret < 0) {
		dev_warn(&chip->client->dev, "TIMER CTRL reg read failed\n");
		return ret;
	}

	/* Program the time with duration passed */
	ret |=  wdt_duration;

	/* Enable/Disable the safety timer */
	if (sfttmr_enable)
		ret |= CHRG_TIMER_EXP_CNTL_EN_TIMER;
	else
		ret &= ~CHRG_TIMER_EXP_CNTL_EN_TIMER;

	/* Program the TIMER CTRL register */
	ret = bq2589x_write_reg(chip->client,
				BQ2589X_REG_07,
				ret);
	if (ret < 0)
		dev_warn(&chip->client->dev, "TIMER CTRL I2C write failed\n");

	return ret;
}

/* This function should be called with the mutex held */
static int reset_wdt_timer(struct bq2589x_chip *chip)
{
	int ret = 0, i;

	/* reset WDT timer */
	for (i = 0; i < MAX_RESET_WDT_RETRY; i++) {
		ret = bq2589x_reg_read_modify(chip->client,
						BQ2589X_REG_03,
						WDTIMER_RESET_MASK, true);
		if (ret < 0)
			dev_warn(&chip->client->dev, "I2C write failed:%s\n",
							__func__);
	}
	return ret;
}

/*
 *This function will modify the VINDPM as per the battery voltage
 */
static int bq2589x_modify_vindpm(u8 vindpm)
{
	int ret;
	u8 vindpm_prev;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	/* Get the input src ctrl values programmed */
	ret = bq2589x_read_reg(chip->client,
				BQ2589X_REG_01);

	if (ret < 0) {
		dev_warn(&chip->client->dev, "INPUT CTRL reg read failed\n");
		return ret;
	}

	/* Assign the return value of REG00 to vindpm_prev */
	vindpm_prev = (ret & (BQ2589X_REG_01_VINDPM_MASK<<BQ2589X_REG_01_VINDPM));
	ret &= ~(BQ2589X_REG_01_VINDPM_MASK<<BQ2589X_REG_01_VINDPM);

	/*
	 * If both the previous and current values are same do not program
	 * the register.
	*/
	if (vindpm_prev != vindpm) {
		vindpm |= ret;
		ret = bq2589x_write_reg(chip->client,
					BQ2589X_REG_01, vindpm);
		if (ret < 0) {
			dev_info(&chip->client->dev, "VINDPM failed\n");
			return ret;
		}
	}
	return ret;
}

#ifdef CONFIG_DEBUG_FS
#define DBGFS_REG_BUF_LEN	3

static int bq2589x_show(struct seq_file *seq, void *unused)
{
	u16 val;
	long addr;

	if (kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

	val = bq2589x_read_reg(bq2589x_client, addr);
	seq_printf(seq, "%x\n", val);

	return 0;
}

static int bq2589x_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq2589x_show, inode->i_private);
}

static ssize_t bq2589x_dbgfs_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[DBGFS_REG_BUF_LEN];
	long addr;
	unsigned long value;
	int ret;
	struct seq_file *seq = file->private_data;

	if (!seq || kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

//	if (copy_from_user(buf, user_buf, DBGFS_REG_BUF_LEN-1))
//		return -EFAULT;

	buf[DBGFS_REG_BUF_LEN-1] = '\0';
	if (kstrtoul(buf, 16, &value))
		return -EINVAL;

	dev_info(&bq2589x_client->dev,
			"[dbgfs write] Addr:0x%x Val:0x%x\n",
			(u32)addr, (u32)value);


	ret = bq2589x_write_reg(bq2589x_client, addr, value);
	if (ret < 0)
		dev_warn(&bq2589x_client->dev, "I2C write failed\n");

	return count;
}

static const struct file_operations bq2589x_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= bq2589x_dbgfs_open,
	.read		= seq_read,
	.write		= bq2589x_dbgfs_reg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int bq2589x_create_debugfs(struct bq2589x_chip *chip)
{
	int i;
	struct dentry *entry;

	bq2589x_dbgfs_root = debugfs_create_dir(DEV_NAME, NULL);
	if (IS_ERR(bq2589x_dbgfs_root)) {
		dev_warn(&chip->client->dev, "DEBUGFS DIR create failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < bq2589x_MAX_MEM; i++) {
		sprintf((char *)&bq2589x_dbg_regs[i], "%x", i);
		entry = debugfs_create_file(
					(const char *)&bq2589x_dbg_regs[i],
					S_IRUGO,
					bq2589x_dbgfs_root,
					&bq2589x_dbg_regs[i],
					&bq2589x_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(bq2589x_dbgfs_root);
			bq2589x_dbgfs_root = NULL;
			dev_warn(&chip->client->dev,
					"DEBUGFS entry Create failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}
static inline void bq2589x_remove_debugfs(struct bq2589x_chip *chip)
{
	if (bq2589x_dbgfs_root)
		debugfs_remove_recursive(bq2589x_dbgfs_root);
}
#else
static inline int bq2589x_create_debugfs(struct bq2589x_chip *chip)
{
	return 0;
}
static inline void bq2589x_remove_debugfs(struct bq2589x_chip *chip)
{
}
#endif

static inline int bq2589x_set_cc(struct bq2589x_chip *chip, int cc)
{
	u8 regval;

	dev_info(&chip->client->dev, "%s: %d\n", __func__,  cc);
	regval = chrg_cur_to_reg(cc);

	return bq2589x_reg_write_mask(chip->client, BQ2589X_REG_04,regval, BQ2589X_REG_04_ICHG, BQ2589X_REG_04_ICHG_MASK);
}

static inline int bq2589x_set_cv(struct bq2589x_chip *chip, int cv)
{
	u8 regval;

	dev_info(&chip->client->dev, "%s: %d\n", __func__,  cv);
	regval = chrg_volt_to_reg(cv);

//	return bq2589x_write_reg(chip->client, bq2589x_CHRG_VOLT_CNTL_REG, regval & ~CHRG_VOLT_CNTL_VRECHRG);
	return bq2589x_reg_write_mask(chip->client, BQ2589X_REG_06, regval, BQ2589X_REG_06_VREG, BQ2589X_REG_06_VREG_MASK);	
}
static int bq2589x_set_appropriate_cv(struct bq2589x_chip *chip)
{
	int cv = 0;
	if(chip->bat_is_cool)
		cv = chip->vbat_cool;
	else if(chip->bat_is_warm)
		cv = chip->vbat_warm;
	else
		cv = chip->cv;
	return bq2589x_set_cv(chip,cv);
}
static int bq2589x_set_appropriate_cc(struct bq2589x_chip *chip)
{
	int cc = 0;

	cc = chip->cc;

	return bq2589x_set_cc(chip,cc);
}
void charger_set_ibat_max(int ibat_max)
{
	struct bq2589x_chip *chip = NULL;
	static int pre_val = -1;
	int vbus;
	
	printk("fast_charger: %s, %d\n", __func__, ibat_max);

	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return;

	if(pre_val!=ibat_max)
	{
		pre_val = ibat_max;
		chip->cc = ibat_max;
		if(chip->cc==0)
			chip->is_charging_enabled = false;
		else
			chip->is_charging_enabled = true;
		
		bq2589x_set_appropriate_cc(chip);
	}else
		pr_info("%s ignore same val(%d)\n", __func__, pre_val);

	
	vbus = qpnp_chip_get_property(POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION)/1000;
	//when iusb max is 4000, it indicat fast charging handshake is ok and started
	if(vbus > FAST_CHARGER_VBUS_THRESHOLD)
	{
		int ret = 0x80;

		chip->is_fast_charging_started = true;

		if(chip->is_fast_charging_need_change==0)
		{
			if(vbus<7500)
				ret |= 0x22;//vindpm 7V
			else if(vbus <9500)
				ret  |= 0x36 ;//vindpm 9V
			else
				ret |= 0x54 ;//vindpm 11V

			bq2589x_write_reg(bq2589x_client,BQ2589X_REG_0D,ret);

			//pr_err("ww_debug dpm 0x%x\n", ret);
		}else
			bq2589x_write_reg(bq2589x_client,BQ2589X_REG_0D, 0x93);//vindpm 4.5V			

		if(fg_psy != NULL)
			power_supply_changed(fg_psy);		
	}else
	{
		bq2589x_write_reg(bq2589x_client,BQ2589X_REG_0D, 0x93);//vindpm 4.5V
		chip->is_fast_charging_started = false;
	}
}
static inline int bq2589x_set_ichg_max(struct bq2589x_chip *chip, int ichg_max)
{
	int regval;

	chip->ichg_max = ichg_max;
	regval = chrg_ilim_to_reg(ichg_max);

	pr_info("fast_charger: %s:%d %x\n", __func__,  ichg_max,regval);
	if (regval < 0)
		return regval;

	return bq2589x_reg_write_mask(chip->client, BQ2589X_REG_00, regval, BQ2589X_REG_00_ILIM, BQ2589X_REG_00_ILIM_MASK);	

}
bool is_charging_enabled(void)
{
	struct bq2589x_chip *chip = NULL;
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return 1;
	return chip->is_charging_enabled;
}
bool is_in_otg_mode(void)
{
	struct bq2589x_chip *chip = NULL;
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return 0;
	return chip->boost_mode;
}
void charger_set_iusb_max(int iusb_max)
{
	struct bq2589x_chip *chip = NULL;	
	int reg_status;
	int i = 0;
	static int pre_val = -1;
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return ;

	if(chip->fast_charging_shutdown_flag==1)
	{
		pr_info("%s fast_charging_shutdown_flag is true, return\n", __func__);
		return;		
	}
	
	if(pre_val==iusb_max)
	{
		pr_info("%s ignore same val(%d)\n", __func__, pre_val);
		return;
	}
	
	pre_val = iusb_max;
	
	chip->ichg_max = iusb_max;
	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	while(i++ < 6)
	{
		/*ilimt will be reset to 500mA,when bq2589x detect charger plug in. so we 
		 * need set the ilimt after the bq2589x has completed the charger detection.
		 * so we wait 3s here
		 * */
		if(((reg_status & (1<<BQ2589X_REG_0B_VBUS_STAT)) !=  (1<<BQ2589X_REG_0B_VBUS_STAT)))
		{
			reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
			msleep(500);
		}else
			break;
	}
	bq2589x_set_ichg_max(chip, chip->ichg_max);
}
static inline int bq2589x_enable_charging(
			struct bq2589x_chip *chip, bool val)
{
	int ret, regval;

	dev_info(&chip->client->dev, "%s: %d\n", __func__, val);

	ret = program_timers(chip, CHRG_TIMER_EXP_CNTL_WDT160SEC, true);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"program_timers failed: %d\n", ret);
		return ret;
	}


	/*
	 * Program the ichg_max here in case we are asked to resume the charging
	 * framework would send only set CC/CV commands and not the ichg_max. This
	 * would make sure that we program the last set ichg_max into the register
	 * in case for some reasons WDT expires
	 */
	regval = chrg_ilim_to_reg(chip->ichg_max);

	if (regval < 0) {
		dev_err(&chip->client->dev,
			"read ilim failed: %d\n", regval);
		return regval;
	}

	printk("ichg max is %d,reg is %x\n",chip->ichg_max,regval);
	ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_00, regval, BQ2589X_REG_00_ILIM, BQ2589X_REG_00_ILIM_MASK);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"ichg_max programming failed: %d\n", ret);
		return ret;
	}

	/*
	 * check if we have the battery emulator connected. We do not start
	 * charging if the emulator is connected. Disable the charging
	 * explicitly.
	 */
	if (!chip->sfi_tabl_present) {
		//ret = bq2589x_reg_multi_bitset(chip->client, bq2589x_POWER_ON_CFG_REG, POWER_ON_CFG_CHRG_CFG_DIS, CHR_CFG_BIT_POS, 	CHR_CFG_BIT_LEN);
		bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_CHG_CONFIG, BQ2589X_REG_03_CHG_CONFIG_MASK);
		return ret;
	}

	if (chip->sfttmr_expired)
		return ret;

/*	ret = bq2589x_read_reg(chip->client, bq2589x_POWER_ON_CFG_REG);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"pwr cfg read failed: %d\n", ret);
		return ret;
	}

	if ((chip->chip_type == BQ24296) || (chip->chip_type == BQ24297)) {
		if (val)
			regval = ret | POWER_ON_CFG_BQ29X_CHRG_EN;
		else
			regval = ret & ~POWER_ON_CFG_BQ29X_CHRG_EN;
	} else {
		ret &= ~(CHR_CFG_CHRG_MASK << CHR_CFG_BIT_POS);
		if (val)
			regval = ret | POWER_ON_CFG_CHRG_CFG_EN;
		else
			regval = ret | POWER_ON_CFG_CHRG_CFG_DIS;
	}


	ret = bq2589x_write_reg(chip->client, bq2589x_POWER_ON_CFG_REG, regval);
*/
	if(val)
		ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 1, BQ2589X_REG_03_CHG_CONFIG, BQ2589X_REG_03_CHG_CONFIG_MASK);
	else
		ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_CHG_CONFIG, BQ2589X_REG_03_CHG_CONFIG_MASK);
		
	if (ret < 0)
		dev_warn(&chip->client->dev, "charger enable/disable failed\n");
	else {
		if (val)
			chip->online = true;
		else
			chip->online = false;

	}

	return ret;
}

int bq2589x_enable_charger(int val)
{
	int ret = 0;

	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	charger_enabled = val;
	/*stop charger, by putting it in HiZ mode*/
	if (val == 0) {
		//ret = bq2589x_reg_read_modify(chip->client, bq2589x_INPUT_SRC_CNTL_REG, INPUT_SRC_CNTL_EN_HIZ, true);
		ret =  bq2589x_reg_write_mask(chip->client, BQ2589X_REG_00, 1, BQ2589X_REG_00_EN_HIZ, BQ2589X_REG_00_EN_HIZ_MASK);
		if (ret < 0)
			dev_warn(&chip->client->dev,
				"Input src cntl write failed\n");
		else
			chip->charging_en_flags = false;			
	}else
	{
		bq2589x_clear_hiz(chip);
		bq2589x_enable_charging(chip, true);
		chip->charging_en_flags = true;	
	}

	dev_warn(&chip->client->dev, "%s: %d, %d\n", __func__,  val, chip->charging_en_flags);

	return ret;
}

int charger_enable_charger(int val)
{
	return bq2589x_enable_charger(val);
}

static inline int bq2589x_set_iterm(struct bq2589x_chip *chip, int iterm)
{
	u8 reg_val;

	if (iterm > bq2589x_CHRG_ITERM_OFFSET)
		dev_info(&chip->client->dev,
			"%s ITERM set for%d >128mA", __func__, iterm);

	reg_val = chrg_iterm_to_reg(iterm);
	msleep(500);

	//return bq2589x_write_reg(chip->client, bq2589x_PRECHRG_TERM_CUR_CNTL_REG, (bq2589x_PRE_CHRG_CURR_256 | reg_val));
	return bq2589x_reg_write_mask(chip->client, BQ2589X_REG_05, reg_val, BQ2589X_REG_05_ITERM, BQ2589X_REG_05_ITERM_MASK);	
}

static enum bq2589x_chrgr_stat bq2589x_is_charging(struct bq2589x_chip *chip)
{
	int ret;
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (ret < 0)
		dev_err(&chip->client->dev, "STATUS register read failed\n");

	ret &= SYSTEM_STAT_CHRG_MASK;

	switch (ret) {
	case SYSTEM_STAT_NOT_CHRG:
		chip->chgr_stat = bq2589x_CHRGR_STAT_FAULT;
		break;
	case SYSTEM_STAT_CHRG_DONE:
		chip->chgr_stat = bq2589x_CHRGR_STAT_BAT_FULL;
		break;
	case SYSTEM_STAT_PRE_CHRG:
	case SYSTEM_STAT_FAST_CHRG:
		chip->chgr_stat = bq2589x_CHRGR_STAT_CHARGING;
		break;
	default:
		break;
	}

	return chip->chgr_stat;
}

bool bq2589x_charger_done(void)
{
	struct bq2589x_chip *chip = NULL;
	int batt_voltage = 0;
	int current_now = 0;
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return false;
	current_now = fg_chip_get_property(POWER_SUPPLY_PROP_CURRENT_NOW);
	if(current_now > -300000) {
		batt_voltage = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW)/1000;
		if (batt_voltage < 0) {
			dev_err(&chip->client->dev, "Can't read voltage from FG\n");
			return false;
		}
		chip->batt_voltage = batt_voltage;
		if(chip->batt_voltage > 4300)
			return true;
		if((bq2589x_CHRGR_STAT_BAT_FULL == bq2589x_is_charging(chip)) && (chip->batt_voltage > 4250))
			return true;
		if(chip->bat_is_warm && chip->batt_voltage > 4050)
			return true;
	}
	return false;
}

bool charger_charging_done(void)
{
	return  bq2589x_charger_done();
}

/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq2589x_irq_isr(int irq, void *devid)
{
	struct bq2589x_chip *chip = (struct bq2589x_chip *)devid;


	/**TODO: This hanlder will be used for charger Interrupts */
	dev_info(&chip->client->dev,
		"IRQ Handled for charger interrupt: %d\n", irq);

	return IRQ_WAKE_THREAD;
}

static void bq2589x_hw_init(struct bq2589x_chip *chip)
{
	int ret = 0;
	/* Enable the WDT and enable Safety timer */
	ret = program_timers(chip, CHRG_TIMER_EXP_CNTL_WDT160SEC,true);
	if (ret < 0)
		dev_warn(&chip->client->dev, "TIMER enable failed\n");

	if (chip->ichg_max >= 0)
		bq2589x_set_ichg_max(chip, chip->ichg_max);

	bq2589x_set_appropriate_cc(chip);

	bq2589x_set_appropriate_cv(chip);
	
	if( (chip->is_charging_enabled)&&(chip->charging_en_flags))
		bq2589x_enable_charging(chip, true);
	if(chip->iterm)
		bq2589x_set_iterm(chip,chip->iterm);
	//set bat_comp = 40moh,vclamp=32mv
	bq2589x_write_reg(bq2589x_client,BQ2589X_REG_0D,0x93);

//	bq2589x_dump_registers(chip);
}
static int bq2589x_get_appropriate_cv(struct bq2589x_chip *chip)
{
	if(chip->bat_is_cool)
		return chip->vbat_cool;
	else if(chip->bat_is_warm)
		return chip->vbat_warm;
	else
		return chip->cv;
}

static void bq2589x_cv_check_and_set(struct bq2589x_chip *chip)
{
	int ret,cv,cv_reg;
	ret = bq2589x_read_reg(chip->client, bq2589x_CHRG_VOLT_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Chrg Volt Ctrl reg read fail\n");
	ret = (ret>>2);
	
	cv = bq2589x_get_appropriate_cv(chip);
	cv_reg = chrg_volt_to_reg(cv);
	if(ret != cv_reg)
	{
		printk("Charger:cv value 0x%x get from charger chip is not correct :0x%x(%d)\n",ret,cv_reg, cv);
		bq2589x_hw_init(chip);
	}
}
static void bq2589x_fault_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip =
	    container_of(work, struct bq2589x_chip, fault_work.work);
	int reg_fault;

	/* Check if battery fault condition occured. Reading the register
	   value two times to get reliable reg value, recommended by vendor*/
/*	reg_fault = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");*/

	reg_fault = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");

	if(reg_fault!=0x00)
		dev_info(&chip->client->dev, "FAULT reg %x\n", reg_fault);
	
	if (reg_fault & FAULT_STAT_WDT_TMR_EXP) {
		dev_warn(&chip->client->dev, "WDT expiration fault\n");
		if (chip->is_charging_enabled) {
			/*when WDT expiration ,bq2589x register will be reset,so we need reconfig it*/
			bq2589x_hw_init(chip);
		} else
			dev_info(&chip->client->dev, "No charger connected\n");
	}
	if ((reg_fault & FAULT_STAT_CHRG_TMR_FLT) == FAULT_STAT_CHRG_TMR_FLT) {
		chip->sfttmr_expired = true;
		dev_info(&chip->client->dev, "Safety timer expired\n");
	}
/*	if (reg_fault & FAULT_STAT_BATT_TEMP_BITS) {
		dev_info(&chip->client->dev,
			"%s:Battery over temp occured!!!!\n", __func__);
	}*/
}
extern void do_double_check_for_usb_unplug(void);
/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq2589x_irq_thread(int irq, void *devid)
{
	struct bq2589x_chip *chip = (struct bq2589x_chip *)devid;
	int reg_status;

	msleep(100);
	
	wake_lock_timeout(&chip->irq_wk,HZ);
	/*
	 * check the bq2589x status/fault registers to see what is the
	 * source of the interrupt
	 */
	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	//dev_info(&chip->client->dev, "STATUS reg %x\n", reg_status);


	if(((reg_status & SYSTEM_STAT_VBUS_HOST) == SYSTEM_STAT_VBUS_HOST) ||
		((reg_status & SYSTEM_STAT_VBUS_CDP) == SYSTEM_STAT_VBUS_CDP)/*||
		((reg_status & SYSTEM_STAT_VBUS_DCP) == SYSTEM_STAT_VBUS_DCP)||
		((reg_status & SYSTEM_STAT_VBUS_HVDCP) == SYSTEM_STAT_VBUS_HVDCP)||
		((reg_status & SYSTEM_STAT_VBUS_UNSTD) == SYSTEM_STAT_VBUS_UNSTD)||
		((reg_status & SYSTEM_STAT_VBUS_NONSTD) == SYSTEM_STAT_VBUS_NONSTD)*/)

	{
		/*
		 * Prevent system from entering suspend while charger is connected
		 */
		if (!wake_lock_active(&chip->wakelock))
			wake_lock(&chip->wakelock);
	}else{
		msleep(150);
		do_double_check_for_usb_unplug();
		/* Release the wake lock */
		if (wake_lock_active(&chip->wakelock))
			wake_unlock(&chip->wakelock);
	}

	reg_status &= SYSTEM_STAT_CHRG_DONE;

	if (reg_status == SYSTEM_STAT_CHRG_DONE) {
		//dev_warn(&chip->client->dev, "HW termination happened\n");
/*		mutex_lock(&chip->event_lock);
		bq2589x_enable_hw_term(chip, false);
		mutex_unlock(&chip->event_lock);
		bq2589x_hw_init(chip);
		*/
		/* schedule the thread to let the framework know about FULL */
	}
	if(fg_psy != NULL)
		power_supply_changed(fg_psy);

	schedule_delayed_work(&chip->fault_work, 0);
	return IRQ_HANDLED;
}
static void charger_temp_monitor_func(struct bq2589x_chip *chip) 
{
	bool bat_cool = false;
	bool bat_warm = false;
	bool dec_cur_bat_cool = false;

   if(chip->batt_temp  < chip->cool_temp){
	   bat_cool = true;
	   bat_warm = false;
	   if(chip->batt_voltage > 4150)
	   {
		   dec_cur_bat_cool = true;
	   }else
	   {
		   dec_cur_bat_cool = false;
	   }
   }else if(chip->batt_temp > chip->cool_temp && chip->batt_temp < chip->warm_temp){
	   bat_cool = false;
	   bat_warm = false;
   }else if(chip->batt_temp > chip->warm_temp){
	   bat_warm = true;
	   bat_cool = false;
   }

   if (chip->bat_is_cool ^ bat_cool || chip->bat_is_warm ^ bat_warm || chip->dec_cur_bat_cool ^ dec_cur_bat_cool) {
		   chip->bat_is_cool = bat_cool;
		   chip->bat_is_warm = bat_warm;
		   chip->dec_cur_bat_cool = dec_cur_bat_cool;      
		   //bq2589x_set_appropriate_cc(chip);
		   bq2589x_set_appropriate_cv(chip);
   }
}
extern int bq27xxx_set_temp(int temp);
extern int bq27xxx_get_temp(void);
extern int is_charger_plug_in(void);
extern bool need_dec_chg_current(void);
extern int get_board_temp(void);
static int poweroff_charging_flag = 0;
#ifdef CONFIG_LEDS_SN3193		
extern void SN3193_PowerOff_Charging_RGB_LED(unsigned int level);	
#endif

int bq2589x_led_opt(int soc, int en)
{
	int val;
	
#ifdef CONFIG_LEDS_SN3193
	if(en==0)
		val = 0x00;
	else
	{
		if(soc==100)
			val = 0x00;
		else
			val = 0xff;
	}
	
	SN3193_PowerOff_Charging_RGB_LED(val);	
#endif

	return 0;
}

static void bq2589x_task_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip =
	    container_of(work, struct bq2589x_chip, chrg_task_wrkr.work);
	int ret, jiffy = CHARGER_TASK_JIFFIES;
	int soc = fg_chip_get_property(POWER_SUPPLY_PROP_CAPACITY);
	int current_now = fg_chip_get_property(POWER_SUPPLY_PROP_CURRENT_NOW);
	
	chip->vbus = qpnp_chip_get_property(POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION)/1000;
	//int batt_voltage = 0;
	//u8 vindpm = INPUT_SRC_VOLT_LMT_DEF;

	if(fg_psy != NULL)
		power_supply_changed(fg_psy);
	
	/* Reset the WDT */
	mutex_lock(&chip->event_lock);
	ret = reset_wdt_timer(chip);
	mutex_unlock(&chip->event_lock);
	if (ret < 0)
		dev_warn(&chip->client->dev, "WDT reset failed:\n");

	/*
	 * If we have an OTG device connected, no need to modify the VINDPM
	 * check for Hi-Z
	 */
	if (chip->boost_mode) {
		jiffy = CHARGER_HOST_JIFFIES;
		goto sched_task_work;
	}

	if (charger_enabled == 1) {
		/* Clear the charger from Hi-Z */
		ret = bq2589x_clear_hiz(chip);
		if (ret < 0)
			dev_warn(&chip->client->dev, "HiZ clear failed:\n");
	}
	
	/*sometimes WDT interrupt won't assert, so charger parameters will be reseted to 
	 * default value. we do check here, if the CV value is not correct, we reinit the
	 * charger parameters*/
	bq2589x_cv_check_and_set(chip);
	/* Modify the VINDPM */

sched_task_work:
	chip->batt_status = fg_chip_get_property(POWER_SUPPLY_PROP_STATUS);
	chip->batt_temp = fg_chip_get_property(POWER_SUPPLY_PROP_TEMP);
	/* read the battery voltage */
	chip->batt_voltage  = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW) / 1000;
	if (chip->batt_voltage < 0) {
		dev_err(&chip->client->dev, "Can't read voltage from FG\n");
	}

	/* convert voltage into millivolts */
	if (bq2589x_CHRGR_STAT_BAT_FULL == bq2589x_is_charging(chip)) {
		if(soc != 100)
		{
		//	bq2589x_hw_init(chip);
			dev_warn(&chip->client->dev,
				"%s battery full,but soc is not 100", __func__);
		}
	}
	
	//sometime bq2589x plug in/out event will be lost,so do wakelock here to ensure system won't enter suspend when charger plug in
	if (is_charger_plug_in()) {
		if (!wake_lock_active(&chip->wakelock))
			wake_lock(&chip->wakelock);
	}else
	{
		chip->dec_cur_bat_hot = false;
		blk_dec = false;
		if (wake_lock_active(&chip->wakelock))
			wake_unlock(&chip->wakelock);
	}
//	if(fg_psy != NULL)
//		power_supply_changed(fg_psy);
	chip->board_temp = get_board_temp();

	if(poweroff_charging_flag==1)
	{
		if(chip->batt_status==1)
		{
			bq2589x_led_opt(soc, 1);			
		}else
			bq2589x_led_opt(0x0, 0);		
	}

	if(chip->vbus<FAST_CHARGER_VBUS_THRESHOLD)	
		bq2589x_write_reg(bq2589x_client,BQ2589X_REG_0D, 0x93);//vindpm 4.5V

	pr_warn("battery voltage is %d,capacity is %d,current now is %d,temp is %d,status is %d, vbus %d, fast_state %d board temp %d\n",chip->batt_voltage,soc,current_now,chip->batt_temp,chip->batt_status, chip->vbus, chip->is_fast_charging_started, chip->board_temp);
	//we don't change the charging current base on battery temperature when fast charging started now 
	//if(chip->is_fast_charging_started == false)
	if(1)
	{
		charger_temp_monitor_func(chip);
	}
	//if(abs(bq27xxx_get_temp()-chip->batt_temp) > 15)
	if(bq27xxx_get_temp()!=chip->batt_temp)		
	{
		pr_debug("battery temperature changed,update to bq275xx\n");
		bq27xxx_set_temp(chip->batt_temp);
	}

	/*lenovo-sw weiweij added tmp*/
	/*-----------------need to be removed soon-------------------*/
/*	if(1)
	{
		union power_supply_propval val;		

		if(!chip->usb_psy)
		{
			chip->usb_psy = power_supply_get_by_name("usb");
			if (!chip->usb_psy) {
				pr_err("usb supply not found return\n");
				return;
			}
		}

		chip->usb_psy->get_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPE, &val);
		pr_err("usb type %d\n", val.intval);
		if((val.intval!=POWER_SUPPLY_TYPE_USB_DCP)&&(chip->charging_en_flags))
		{
			charger_set_iusb_max(500);		
			charger_set_ibat_max(500);
		}
	}*/
	/*---------------------------------------------------------*/	
	/*lenovo-sw weiweij added tmp end*/	

	bq2589x_dump_registers(chip);

	if(soc < 5 || chip->batt_voltage < 3400)
		jiffy = jiffy /3;
	
	schedule_delayed_work(&chip->chrg_task_wrkr, jiffy);
}

static int bq2589x_get_chip_version(struct bq2589x_chip *chip)
{
	int ret;

	/* check chip model number */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_14);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c read err:%d\n", ret);
		return -EIO;
	}
	dev_info(&chip->client->dev, "version reg: 0x%x\n", ret);

	ret = (ret&0x38) >> 3;
	switch (ret) {
	case BQ25852_IC_VERSION:
		chip->chip_type = BQ25852;
		memcpy(chip->ic_name, "BQ25852", sizeof("BQ25852"));
		break;
	case BQ25890_IC_VERSION:
		chip->chip_type = BQ25890;
		memcpy(chip->ic_name, "BQ25890", sizeof("BQ25890"));
		break;
	case bq25895_IC_VERSION:
		chip->chip_type = BQ25895;
		memcpy(chip->ic_name, "BQ25895", sizeof("BQ25895"));
		break;
	default:
		dev_err(&chip->client->dev,
			"device version mismatch: 0x%x set default\n", ret);
		chip->chip_type = BQ25852;
		memcpy(chip->ic_name, "BQ25852", sizeof("BQ25852"));
		break;
	}

	dev_info(&chip->client->dev, "chip type:%x\n", chip->chip_type);
	return 0;
}

static int open_fast_charger_v1_detect = 0;
module_param(open_fast_charger_v1_detect, int, 0644);
static bool first_set = true;
static volatile int fast_charger_en = 0;
static int pre_fast_charger_en = 0;
static int fast_charger_is_engineermode = 0;
static DECLARE_WAIT_QUEUE_HEAD(fast_charger_hs_wq);
int start_fast_charger_hs(int cmd)
{
	struct bq2589x_chip *chip = NULL;
	int ret;
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return 0;

	fast_charger_en = cmd;
	if(open_fast_charger_v1_detect == 1)
	{
		if(gpio_is_valid(chip->fast_chg_en_gpio) && first_set) {
			ret = gpio_request(chip->fast_chg_en_gpio, "fast_chg_en_gpio");
			gpio_direction_output(chip->fast_chg_en_gpio,0);
			first_set = false;
		}
		gpio_set_value(chip->fast_chg_en_gpio,fast_charger_en);
	}
	wake_up_interruptible(&fast_charger_hs_wq);
	return ret;
}

int fast_charger_state(void)
{
	struct bq2589x_chip *chip = NULL;
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return -1;

	return chip->is_fast_charging_started;
}

static ssize_t fast_charger_en_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	sscanf(buf, "%x", &cmd);

	if(open_fast_charger_v1_detect == 1)
	{
		if(gpio_is_valid(chip->fast_chg_en_gpio) && first_set) {
		        ret = gpio_request(chip->fast_chg_en_gpio, "fast_chg_en_gpio");
				gpio_direction_output(chip->fast_chg_en_gpio,0);
				first_set = false;
		}
		
		if(cmd == 1){
			fast_charger_en = 1;
			gpio_set_value(chip->fast_chg_en_gpio,1);
		}
		else{
			fast_charger_en = 0;
			gpio_set_value(chip->fast_chg_en_gpio,0);
		}
	}else
	{
		if(cmd == 1){
			fast_charger_en = 1;
		}
		else{
			fast_charger_en = 0;
		}	
	}

	return ret;
}


ssize_t  fast_charger_en_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    	int ret;
	wait_event_interruptible(fast_charger_hs_wq,fast_charger_en != pre_fast_charger_en);
	
	printk("fast charger en is %d(%d)\n",fast_charger_en, pre_fast_charger_en);
	pre_fast_charger_en = fast_charger_en;

	ret = sprintf(buf, "%d\n", fast_charger_en);

   	 return ret;
}

static DEVICE_ATTR(fast_charger_en, S_IRUGO|S_IWUSR, fast_charger_en_get,fast_charger_en_set);

static ssize_t fast_charger_need_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	sscanf(buf, "%x", &cmd);

	chip->is_fast_charging_need_change = cmd;

	return ret;
}

static DEVICE_ATTR(fast_charger_need_change, S_IRUGO|S_IWUSR, NULL,fast_charger_need_change);

ssize_t  charger_ver_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int ret = 0;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	ret = sprintf(buf, "%s\n",chip->ic_name);
    
    return ret;
}

static DEVICE_ATTR(chrg_version, S_IRUGO|S_IWUSR, charger_ver_get,NULL);

static ssize_t fast_charger_is_engineermode_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;

	sscanf(buf, "%x", &cmd);
	fast_charger_is_engineermode = cmd;

	pr_info("fast charge engineer ret %d\n", fast_charger_is_engineermode);

	return ret;
}


ssize_t  fast_charger_is_engineermode_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    	int ret;

	ret = sprintf(buf, "%d\n", fast_charger_is_engineermode);

   	 return ret;
}

static DEVICE_ATTR(fast_charger_is_engineermode, S_IRUGO|S_IWUSR, fast_charger_is_engineermode_get,fast_charger_is_engineermode_set);

#ifdef LENOVO_OTG_USB_SHORT
int bq2589x_turn_otg_vbus(bool votg_on);

int bq2589x_otg_short_config( int en)
{
	struct bq2589x_chip *chip = NULL ;
	int value;
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
		return -1;

	if(en==1)
	{
		if(gpio_is_valid(chip->otg_usb_short_gpio)) {
			//ret = gpio_request(chip->otg_usb_short_gpio, "otg_usb_short_gpio");
			//bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_OTG_CONFIG, BQ2589X_REG_03_OTG_CONFIG_MASK);
			bq2589x_turn_otg_vbus(0);
			usleep(1000*1000);
			gpio_set_value(chip->otg_usb_short_gpio,1);
			usleep(1000*1000);
			//bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 1, BQ2589X_REG_03_OTG_CONFIG, BQ2589X_REG_03_OTG_CONFIG_MASK);
			bq2589x_turn_otg_vbus(1);
			
			chip->otg_usb_short_state = true;
		}else
			chip->otg_usb_short_state = false;		
	}else
	{
		if(gpio_is_valid(chip->otg_usb_short_gpio)) {
			//ret = gpio_request(chip->otg_usb_short_gpio, "otg_usb_short_gpio");
			bq2589x_turn_otg_vbus(0);
			gpio_set_value(chip->otg_usb_short_gpio,0);
			usleep(2000*1000);
			bq2589x_turn_otg_vbus(1);
			
			chip->otg_usb_short_state = false;
		}else
			chip->otg_usb_short_state = false;		
	}	

	value = gpio_get_value(chip->otg_usb_short_gpio);
		
	return value;
}

static ssize_t otg_usb_short_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;
	int value = -1;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	sscanf(buf, "%x", &cmd);

	if(chip->otg_usb_short_state==cmd)
	{
		pr_err("%s new cmd is as same as the old value(%d)\n", __func__, chip->otg_usb_short_state);
		return ret;
	}

	value = bq2589x_otg_short_config(cmd);

	printk("otg usb short state  is %d, cmd %d, ret %d\n",chip->otg_usb_short_state, cmd, value);
	
	return ret;
}


ssize_t  otg_usb_short_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
   	 int ret;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	printk("otg usb short state  is %d\n",chip->otg_usb_short_state);
	ret = sprintf(buf, "%d\n", chip->otg_usb_short_state);

    return ret;
}

static DEVICE_ATTR(otg_usb_short, S_IRUGO|S_IWUSR,otg_usb_short_get,otg_usb_short_set);
#endif

static struct attribute *fs_attrs[] = {
	&dev_attr_chrg_version.attr,
	&dev_attr_fast_charger_en.attr,
	&dev_attr_fast_charger_need_change.attr,	
	&dev_attr_fast_charger_is_engineermode.attr,		
#ifdef LENOVO_OTG_USB_SHORT
	&dev_attr_otg_usb_short.attr,
#endif
	NULL,
};

static struct attribute_group fs_attr_group = {
	.attrs = fs_attrs,
};
static int bq2589x_parse_dt(struct device *dev,struct bq2589x_chip *chip)
{
	struct device_node *np = dev->of_node;

	/* reset, irq gpio info */
	chip->irq = gpio_to_irq(of_get_named_gpio_flags(np,
			"charger,irq-gpio", 0, 0));
	chip->chg_en_gpio = of_get_named_gpio_flags(np,
			"charger,en-gpio", 0, 0);
	chip->fast_chg_en_gpio = of_get_named_gpio_flags(np,
			"charger,fast-charger-en-gpio", 0, 0);
	chip->ext_charger_gpio = of_get_named_gpio_flags(np,
			"charger,ext-charger-gpio", 0, 0);
	chip->otg_en_gpio = of_get_named_gpio_flags(np,
			"charger,otg-en-gpio", 0, 0);
#ifdef LENOVO_OTG_USB_SHORT	
	chip->otg_usb_short_gpio = of_get_named_gpio_flags(np,
			"charger,otg-short-gpio", 0, 0);	
#endif
	of_property_read_u32(np,"charger,max-temp" ,&chip->max_temp);	
	of_property_read_u32(np,"charger,min-temp" ,&chip->min_temp);	
	of_property_read_u32(np,"charger,ichg-max" ,&chip->ichg_max);	
	of_property_read_u32(np,"charger,ibat-max" ,&chip->cc);	
	of_property_read_u32(np,"charger,vbat-mv" ,&chip->cv);	
	of_property_read_u32(np,"charger,iterm-ma" ,&chip->iterm);	
	of_property_read_u32(np,"charger,warm-temp" ,&chip->warm_temp);	
	of_property_read_u32(np,"charger,ibat-warm" ,&chip->ibat_warm);	
	of_property_read_u32(np,"charger,ibat-cool" ,&chip->ibat_cool);	
	of_property_read_u32(np,"charger,cool-temp" ,&chip->cool_temp);	
	of_property_read_u32(np,"charger,vbat-warm" ,&chip->vbat_warm);	
	of_property_read_u32(np,"charger,vbat-cool" ,&chip->vbat_cool);	
	chip->max_cv = chip->cv;
	chip->sfi_tabl_present = 1;
	chip->is_charging_enabled = true;
	chip->bat_is_warm = false;
	chip->bat_is_cool = false;
	chip->charging_en_flags = true;
	return 0;
}

/* This function should be called with the mutex held */
int bq2589x_turn_otg_vbus(bool votg_on)
{
	int ret = 0;
	struct bq2589x_chip *chip = NULL;

	pr_info("turn on vbus %d\n", votg_on);
	
	if(bq2589x_client != NULL)
		chip = i2c_get_clientdata(bq2589x_client);
	else
	{
		pr_err("%s bq2589x_client is NULL\n", __func__);
		return -1;
	}

	//clean charging config
	//bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_CHG_CONFIG, BQ2589X_REG_03_CHG_CONFIG_MASK);
	
	if (votg_on && chip->a_bus_enable) {
			/* Program the timers */
			ret = program_timers(chip,
						CHRG_TIMER_EXP_CNTL_WDT80SEC,
						false);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"TIMER enable failed %s\n", __func__);
				goto i2c_write_fail;
			}
			/* Configure the charger in OTG mode */
			//ret = bq2589x_reg_read_modify(chip->client, BQ2589X_REG_03, POWER_ON_CFG_CHRG_CFG_OTG, true);

			/* Put the charger IC in reverse boost mode. Since
			 * SDP charger can supply max 500mA charging current
			 * Setting the boost current to 500mA
			 */
			//ret = bq2589x_reg_read_modify(chip->client, bq2589x_POWER_ON_CFG_REG, POWER_ON_CFG_BOOST_LIM, false);
			ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_0A, 0x6, BQ2589X_REG_0A_BOOST_LIM, BQ2589X_REG_0A_BOOST_LIM_MASK);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"otg boost lim write 1.3A failed\n");
				goto i2c_write_fail;
			}

			ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_0A, 0xc, BQ2589X_REG_0A_BOOSTV, BQ2589X_REG_0A_BOOSTV_MASK);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"otg boost lim write 1.3A failed\n");
				goto i2c_write_fail;
			}		
			
			ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 2, BQ2589X_REG_03_CHG_CONFIG, 0x3);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"otg set config failed\n");
				goto i2c_write_fail;
			}			
			
			//gpio_set_value(chip->otg_en_gpio,1);
			chip->boost_mode = true;
	} else {
			//ret = bq2589x_reg_read_modify(chip->client, bq2589x_POWER_ON_CFG_REG, POWER_ON_CFG_CHRG_CFG_OTG, false);
			ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_OTG_CONFIG, BQ2589X_REG_03_OTG_CONFIG_MASK);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"otg set config failed\n");
				goto i2c_write_fail;
			}

			/* Put the charger IC out of reverse boost mode 500mA */
			//ret = bq2589x_reg_read_modify(chip->client, bq2589x_POWER_ON_CFG_REG, POWER_ON_CFG_BOOST_LIM, false);
			/*ret = bq2589x_reg_write_mask(chip->client, BQ2589X_REG_0A, 0x0, BQ2589X_REG_0A_BOOST_LIM, BQ2589X_REG_0A_BOOST_LIM_MASK);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"otg boost lim write 500ma failed\n");
				goto i2c_write_fail;
			}*/
			//gpio_set_value(chip->otg_en_gpio,0);
			chip->boost_mode = false;
			//restart charger when come back from otg mode
			if( (chip->is_charging_enabled)&&(chip->charging_en_flags))
				bq2589x_enable_charging(chip, true);
	}

	return ret;
i2c_write_fail:
	dev_err(&chip->client->dev, "%s: Failed\n", __func__);
	return ret;
}
static int bq2589x_otg_regulator_enable(struct regulator_dev *rdev)
{
	return bq2589x_turn_otg_vbus(true);
}

static int bq2589x_otg_regulator_disable(struct regulator_dev *rdev)
{
#ifdef LENOVO_OTG_USB_SHORT
	{
		int val;

		val = bq2589x_otg_short_config(0);
		pr_info("bq2589x_otg_short_config = %d", val);
	}
#endif

	return bq2589x_turn_otg_vbus(false);
}

static int bq2589x_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	struct bq2589x_chip *chip = rdev_get_drvdata(rdev);
	return chip->boost_mode;
}

struct regulator_ops bq2589x_otg_reg_ops = {
	.enable		= bq2589x_otg_regulator_enable,
	.disable	= bq2589x_otg_regulator_disable,
	.is_enabled	= bq2589x_otg_regulator_is_enable,
};
static int bq2589x_regulator_init(struct device *dev,struct bq2589x_chip *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(dev, dev->of_node);
	if (!init_data) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &bq2589x_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = dev->of_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
					&chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			rc = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(dev,
					"OTG reg failed, rc=%d\n", rc);
		}
	}

	return rc;
}

//dummy function for unused static functions
void dummy_function(void)
{
	bq2589x_reg_multi_bitset(NULL, 0, 0, 0, 0);
	bq2589x_reg_read_mask(NULL, 0, 0, 0);	
	bq2589x_modify_vindpm(0);
	vindpm_to_reg(0);
	charger_temp_monitor_func(NULL);
}

#ifdef EXT_CHARGER_POWER_SUPPLY
static char *bq2589x_chip_name = "ext-charger";

static enum power_supply_property bq2589x_power_supply_props[] = {
	/* TODO: maybe add more power supply properties */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TEMP,	
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
};

static int bq2589x_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq2589x_chip *chip = container_of(psy, struct bq2589x_chip, charger_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	{
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_board_temp();//chip->batt_temp;
		break;
/*	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->model;
		break;*/
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = (chip->charging_en_flags)? 1:0;		
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq2589x_power_supply_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct bq2589x_chip *chip = container_of(psy, struct bq2589x_chip, charger_psy);
	int value;
	
	switch (psp) {
		//just for charging temp protect function debug 
		case POWER_SUPPLY_PROP_TEMP:
			pr_err("%s debug charging temp, intval = %d\n", __func__, val->intval);
			if((val->intval<-20)&&(val->intval>60))
				chip->temp_debug_flag = false;
			else
			{
				chip->temp_debug_flag = true;
				chip->batt_temp = val->intval;
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_ENABLED:
			//pr_err("%s debug charging enable, intval = %d\n", __func__, val->intval);	
			charger_enable_charger(val->intval);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
			value = val->intval;
			charger_set_iusb_max(value);
	        	break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			value = val->intval;
	//		pr_info("set ibatmax with %d\n",di->prev_ibat_max_ma);
			
	//		if(di->df_ver == data_flash_ver_n)
	//			di->prev_ibat_max_ma = 1200;
			charger_set_ibat_max(value);
			break;			
		default:
			pr_err("%s not support power_supply property cmd\n", __func__);
			return -EINVAL;
	}
	return 0;
}

static int bq2589x_power_supply_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:		
			return 1;
	        	break;
		default:
			break;
	}

	return 0;
}

static int bq2589x_power_supply_init(struct bq2589x_chip *chip)
{
	int ret;

	chip->charger_psy.name = bq2589x_chip_name;//chip->name;
	chip->charger_psy.type = POWER_SUPPLY_TYPE_USB;
	chip->charger_psy.properties = bq2589x_power_supply_props;
	chip->charger_psy.num_properties = ARRAY_SIZE(bq2589x_power_supply_props);
	chip->charger_psy.get_property = bq2589x_power_supply_get_property;
	chip->charger_psy.set_property = bq2589x_power_supply_set_property;
	chip->charger_psy.property_is_writeable = bq2589x_power_supply_property_is_writeable;	

	ret = power_supply_register(&chip->client->dev, &chip->charger_psy);
	if (ret) 
	{
		return ret;
	}

	return 0;
}


static void bq2589x_power_supply_exit(struct bq2589x_chip *chip)
{
	//cancel_delayed_work_sync(&chip->work);
	power_supply_unregister(&chip->charger_psy);

}
#endif

static int bq2589x_l6_regulator_get(struct bq2589x_chip *chip)
{
	int ret;

	chip->vdd = regulator_get(&chip->client->dev, "vdd");
	if (IS_ERR_OR_NULL(chip->vdd)) {
		pr_err("%s: fail to get 1.8v LDO\n", __func__);
		return -3;
	}

	if (regulator_count_voltages(chip->vdd) > 0) {
	ret = regulator_set_voltage(chip->vdd, 1800000, 1800000);
	if (ret) {
		pr_err("%s: regulator set_vtg vdd_reg failed rc=%d\n", __func__, ret);
		regulator_put(chip->vdd);
		return -4;
	}
	}

/*    err = regulator_set_optimum_mode(data->vdd_reg, 20000);
    if (err < 0) {
        pr_err("%s: set_optimum_mode vdd_reg failed, rc=%d\n", __func__, err);
        regulator_put(data->vdd_reg);
        return -5;
    }*/
	
	ret = regulator_enable(chip->vdd);
	if (ret) {
		pr_err("%s: Regulator vdd_reg enable failed rc=%d\n", __func__, ret);
	}
	
    return 0;
}

static int bq2589x_charger_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct bq2589x_chip *chip;
	int ret;

	fg_psy = power_supply_get_by_name("battery");
	if (!fg_psy) {
		pr_err("fg supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}
	chip = kzalloc(sizeof(struct bq2589x_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "mem alloc failed\n");
		return -ENOMEM;
	}
	if (client->dev.of_node) {
		ret = bq2589x_parse_dt(&client->dev, chip);
		if (ret)
			return ret;
	}

	chip->client = client;
	chip->a_bus_enable = true;

	/*assigning default value for min and max temp*/
	i2c_set_clientdata(client, chip);
	bq2589x_client = client;
	chip->chgr_stat = bq2589x_CHRGR_STAT_UNKNOWN;
	chip->is_fast_charging_need_change = 0;
	chip->fast_charging_shutdown_flag = 0;

	bq2589x_reg_write_mask(chip->client, BQ2589X_REG_03, 0, BQ2589X_REG_03_OTG_CONFIG, BQ2589X_REG_03_OTG_CONFIG_MASK);
	
	/* check chip model number */
	ret = bq2589x_get_chip_version(chip);
	if (ret < 0) {
		dev_err(&client->dev, "i2c read err:%d\n", ret);
		i2c_set_clientdata(client, NULL);
		kfree(chip);
		return -EIO;
	}


	INIT_DELAYED_WORK(&chip->chrg_task_wrkr, bq2589x_task_worker);
	INIT_DELAYED_WORK(&chip->fault_work, bq2589x_fault_worker);
	mutex_init(&chip->event_lock);

	/* Initialize the wakelock */
	wake_lock_init(&chip->wakelock, WAKE_LOCK_SUSPEND,
						"ctp_charger_wakelock");
	wake_lock_init(&chip->irq_wk, WAKE_LOCK_SUSPEND,
						"charger_irq_wakelock");

	/* Init Runtime PM State */
	pm_runtime_put_noidle(&chip->client->dev);
	pm_schedule_suspend(&chip->client->dev, MSEC_PER_SEC);

	/* create debugfs for maxim registers */
	ret = bq2589x_create_debugfs(chip);
	if (ret < 0) {
		dev_err(&client->dev, "debugfs create failed\n");
		i2c_set_clientdata(client, NULL);
		bq2589x_client = NULL;
		kfree(chip);
		return ret;
	}

#ifdef EXT_CHARGER_POWER_SUPPLY
	ret = bq2589x_power_supply_init(chip);
	if (ret) {
		pr_err("failed to register power supply: %d\n", ret);
	}	
	ret = sysfs_create_group(&chip->charger_psy.dev->kobj,&fs_attr_group);
#else
	ret = sysfs_create_group(&client->dev.kobj,&fs_attr_group);
#endif

	if (ret) {
		dev_err(&client->dev, "failed to setup sysfs ret = %d\n", ret);
	}
	/*
	 * Request for charger chip gpio.This will be used to
	 * register for an interrupt handler for servicing charger
	 * interrupts
	 */
	if (chip->irq < 0) {
		dev_err(&chip->client->dev,
			"chgr_int_n GPIO is not available\n");
	} else {
		ret = request_threaded_irq(chip->irq,
				bq2589x_irq_isr, bq2589x_irq_thread,
				IRQF_TRIGGER_FALLING, "bq2589x", chip);
		if (ret) {
			dev_warn(&bq2589x_client->dev,
				"failed to register irq for pin %d\n",
				chip->irq);
		} else {
			dev_warn(&bq2589x_client->dev,
				"registered charger irq for pin %d\n",
				chip->irq);
		}
	}
	if(gpio_is_valid(chip->chg_en_gpio)) {
        ret = gpio_request(chip->chg_en_gpio, "chg_en_gpio");
		gpio_direction_output(chip->chg_en_gpio,0);
		gpio_set_value(chip->chg_en_gpio,0);
	}
/*	if(gpio_is_valid(chip->otg_en_gpio)) {
        ret = gpio_request(chip->otg_en_gpio, "otg_en_gpio");
		gpio_direction_output(chip->otg_en_gpio,0);
		gpio_set_value(chip->otg_en_gpio,0);
	}*/
#ifdef LENOVO_OTG_USB_SHORT	
	if(gpio_is_valid(chip->otg_usb_short_gpio)) {
	ret = gpio_request(chip->otg_usb_short_gpio, "otg_usb_short_gpio");	
		gpio_direction_output(chip->otg_usb_short_gpio,0);
		gpio_set_value(chip->otg_usb_short_gpio,0);
	}
#endif	
	bq2589x_hw_init(chip);
	//enable charging default
	bq2589x_enable_charger(1);

	bq2589x_regulator_init(&client->dev, chip);
	bq2589x_l6_regulator_get(chip);
	
	schedule_delayed_work(&chip->chrg_task_wrkr, msecs_to_jiffies(1000));

	pr_info("%s prob success\n", __func__);
	
	return 0;
}

static int bq2589x_remove(struct i2c_client *client)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(client);

	bq2589x_remove_debugfs(chip);

#ifdef EXT_CHARGER_POWER_SUPPLY	
	bq2589x_power_supply_exit(chip);
#endif

	if (chip->irq > 0)
		free_irq(chip->irq, chip);

	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->wakelock);

	kfree(chip);
	return 0;
}

static void bq2589x_shutdown(struct i2c_client *client)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(client);

	printk("bq2589x_shutdown \r\n");

	wake_lock_destroy(&chip->wakelock);

	if (chip->irq > 0) {
		free_irq(chip->irq, chip);
	}
	cancel_delayed_work_sync(&chip->chrg_task_wrkr);
	cancel_delayed_work_sync(&chip->fault_work);
	
	if(chip->is_fast_charging_started==1)
	{
		if(gpio_is_valid(chip->chg_en_gpio)) {		
			printk("disable charging for shutting down\r\n");
			gpio_set_value(chip->chg_en_gpio,1);
		}
		
		charger_set_iusb_max(100);
		
		chip->fast_charging_shutdown_flag = 1;		
		msleep(250);
		chip->fast_charging_shutdown_flag = 0;
	}
	
	//reset charger ic registers
	bq2589x_reset_regs(chip);


}

#ifdef CONFIG_PM
static int bq2589x_suspend(struct device *dev)
{
	struct bq2589x_chip *chip = dev_get_drvdata(dev);
	int ret;
	
	if (chip->irq > 0) {
		/*
		 * Once the WDT is expired all bq2589x registers gets
		 * set to default which means WDT is programmed to 40s
		 * and if there is no charger connected, no point
		 * feeding the WDT. Since reg07[1] is set to default,
		 * charger will interrupt SOC every 40s which is not
		 * good for S3. In this case we need to free chgr_int_n
		 * interrupt so that no interrupt from charger wakes
		 * up the platform in case of S3. Interrupt will be
		 * re-enabled on charger connect.
		 */
		if (chip->irq > 0)
			free_irq(chip->irq, chip);
	}
	cancel_delayed_work_sync(&chip->chrg_task_wrkr);
	cancel_delayed_work_sync(&chip->fault_work);

	ret= regulator_disable(chip->vdd);
	if (ret) {
		pr_err("%s: Regulator vdd_reg disable failed rc=%d\n", __func__, ret);
	}
	
	dev_dbg(&chip->client->dev, "bq2589x suspend,cancel charger worker\n");
	return 0;
}

static int bq2589x_resume(struct device *dev)
{
	struct bq2589x_chip *chip = dev_get_drvdata(dev);
	int ret;

	ret = regulator_enable(chip->vdd);
	if (ret) {
		pr_err("%s: Regulator vdd_reg enable failed rc=%d\n", __func__, ret);
	}
	
	if (chip->irq > 0) {
		ret = request_threaded_irq(chip->irq,
				bq2589x_irq_isr, bq2589x_irq_thread,
				IRQF_TRIGGER_FALLING, "bq2589x", chip);
		if (ret) {
			dev_warn(&bq2589x_client->dev,
				"failed to register irq for pin %d\n",
				chip->irq);
		} else {
			dev_warn(&bq2589x_client->dev,
				"registered charger irq for pin %d\n",
				chip->irq);
		}
	}
	schedule_delayed_work(&chip->fault_work, 0);
	schedule_delayed_work(&chip->chrg_task_wrkr, msecs_to_jiffies(20));

	dev_dbg(&chip->client->dev, "bq2589x resume,shecdule charger work and fault work\n");
	return 0;
}
#else
#define bq2589x_suspend NULL
#define bq2589x_resume NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int bq2589x_runtime_suspend(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq2589x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq2589x_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#else
#define bq2589x_runtime_suspend	NULL
#define bq2589x_runtime_resume		NULL
#define bq2589x_runtime_idle		NULL
#endif

MODULE_DEVICE_TABLE(i2c, bq2589x_id);

static const struct dev_pm_ops bq2589x_pm_ops = {
	.suspend		= bq2589x_suspend,
	.resume			= bq2589x_resume,
	.runtime_suspend	= bq2589x_runtime_suspend,
	.runtime_resume		= bq2589x_runtime_resume,
	.runtime_idle		= bq2589x_runtime_idle,
};


static const struct of_device_id bq2589x_match[] = {
	{ .compatible = "ti,bq2589x" },
	{ },
};
static const struct i2c_device_id bq2589x_id[] = {
	{ "bq2589x", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2589x_id);

static struct i2c_driver bq2589x_battery_driver = {
	.driver = {
		.name = "bq2589x-charger",
		.owner	= THIS_MODULE,
		.pm = &bq2589x_pm_ops,
		.of_match_table = of_match_ptr(bq2589x_match),
	},
	.probe = bq2589x_charger_probe,
	.remove = bq2589x_remove,
	.shutdown = bq2589x_shutdown,
	.id_table = bq2589x_id,
};

static inline int bq2589x_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq2589x_battery_driver);
	printk("%s:bq27xxx register_i2c driver\n",__func__);
	if (ret)
		printk(KERN_ERR "Unable to register bq2589x i2c driver\n");

	return ret;
}

static inline void bq2589x_battery_i2c_exit(void)
{
	i2c_del_driver(&bq2589x_battery_driver);
}

/*
 * Module stuff
 */

static int __init bq2589x_battery_init(void)
{
	int ret;
	ret = bq2589x_battery_i2c_init();

	return ret;
}
module_init(bq2589x_battery_init);

static void __exit bq2589x_battery_exit(void)
{
	bq2589x_battery_i2c_exit();
}
module_exit(bq2589x_battery_exit);

static int __init early_bootmode(char *p)
{
	if(strstr(p, "usb_cable")!=0)
		poweroff_charging_flag = 0;
	else
		poweroff_charging_flag = 0;

	return 0;
}
early_param("androidboot.mode",early_bootmode);

