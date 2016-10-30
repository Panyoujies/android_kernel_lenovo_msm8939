/*
 * bq24192_charger.h - Charger driver for TI BQ24190/191/192/192I
 *
 * Copyright (C) 2012 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/power_supply.h>

#ifndef __BQ2589X_CHARGER_H_
#define __BQ2589X_CHARGER_H_


#define BQ2589X_REG_00		0x00
#define BQ2589X_REG_01		0x01
#define BQ2589X_REG_02		0x02
#define BQ2589X_REG_03		0x03
#define BQ2589X_REG_04		0x04
#define BQ2589X_REG_05		0x05
#define BQ2589X_REG_06		0x06
#define BQ2589X_REG_07		0x07
#define BQ2589X_REG_08		0x08
#define BQ2589X_REG_09		0x09
#define BQ2589X_REG_0A		0x0a
#define BQ2589X_REG_0B		0x0b
#define BQ2589X_REG_0C		0x0c
#define BQ2589X_REG_0D		0x0d
#define BQ2589X_REG_0E		0x0e
#define BQ2589X_REG_0F		0x0f
#define BQ2589X_REG_10		0x10
#define BQ2589X_REG_11		0x11
#define BQ2589X_REG_12		0x12
#define BQ2589X_REG_13		0x13
#define BQ2589X_REG_14		0x14

//---------------------------reg00--------------------------
#define BQ2589X_REG_00_EN_HIZ			(7)
#define BQ2589X_REG_00_EN_ILIM			(6)
#define BQ2589X_REG_00_ILIM				(0)

#define BQ2589X_REG_00_EN_HIZ_MASK			0x1
#define BQ2589X_REG_00_EN_ILIM_MASK		0x1
#define BQ2589X_REG_00_ILIM_MASK			0x3f

//---------------------------reg01--------------------------
#define BQ2589X_REG_01_BHOT				(6)
#define BQ2589X_REG_01_BCOLD			( 5)
#define BQ2589X_REG_01_VINDPM			(0)

#define BQ2589X_REG_01_BHOT_MASK			0x7
#define BQ2589X_REG_01_BCOLD_MASK			0x1
#define BQ2589X_REG_01_VINDPM_MASK		0x1f

//---------------------------reg02--------------------------
#define BQ2589X_REG_02_CONV_START		(7)
#define BQ2589X_REG_02_CONV_RATE		(6)
#define BQ2589X_REG_02_BOOST_FREQ		(5)
#define BQ2589X_REG_02_ICO_EN			(4)
#define BQ2589X_REG_02_HVDCP_EN		(3)
#define BQ2589X_REG_02_MAXC_EN			(2)
#define BQ2589X_REG_02_FORCE_DPDM		(1)
#define BQ2589X_REG_02_AUTO_DPDM_EN	(0)

#define BQ2589X_REG_02_CONV_START_MASK		0x1
#define BQ2589X_REG_02_CONV_RATE_MASK			0x1
#define BQ2589X_REG_02_BOOST_FREQ_MASK		0x1
#define BQ2589X_REG_02_ICO_EN_MASK				0x1
#define BQ2589X_REG_02_HVDCP_EN_MASK			0x1
#define BQ2589X_REG_02_MAXC_EN_MASK			0x1
#define BQ2589X_REG_02_FORCE_DPDM_MASK		0x1
#define BQ2589X_REG_02_AUTO_DPDM_EN_MASK		0x1
//---------------------------reg03--------------------------
#define BQ2589X_REG_03_BAT_LOADEN		(7)
#define BQ2589X_REG_03_WD_RST			(6)
#define BQ2589X_REG_03_OTG_CONFIG		(5)
#define BQ2589X_REG_03_CHG_CONFIG		(4)
#define BQ2589X_REG_03_SYS_MIN			(1)

#define BQ2589X_REG_03_BAT_LOADEN_MASK		0x1
#define BQ2589X_REG_03_WD_RST_MASK			0x1
#define BQ2589X_REG_03_OTG_CONFIG_MASK		0x1
#define BQ2589X_REG_03_CHG_CONFIG_MASK		0x1
#define BQ2589X_REG_03_SYS_MIN_MASK			0x7
//---------------------------reg04--------------------------
#define BQ2589X_REG_04_ICHG				(0)

#define BQ2589X_REG_04_ICHG_MASK				0x7f
//---------------------------reg05--------------------------
#define BQ2589X_REG_05_IPRECHG			(4)
#define BQ2589X_REG_05_ITERM			(0)

#define BQ2589X_REG_05_IPRECHG_MASK			0xf
#define BQ2589X_REG_05_ITERM_MASK				0xf
//---------------------------reg06--------------------------
#define BQ2589X_REG_06_VREG				(2)
#define BQ2589X_REG_06_BATLOWV			(1)
#define BQ2589X_REG_06_VRECHG			(0)

#define BQ2589X_REG_06_VREG_MASK				0x3f
#define BQ2589X_REG_06_BATLOWV_MASK			0x1
#define BQ2589X_REG_06_VRECHG_MASK			0x1
//---------------------------reg07--------------------------
#define BQ2589X_REG_07_EN_TERM			(7)
#define BQ2589X_REG_07_WATCHDOG		(4)
#define BQ2589X_REG_07_EN_TIMER			(3)
#define BQ2589X_REG_07_CHG_TIMER		(1)
#define BQ2589X_REG_07_JEITA_ISET		(0)

#define BQ2589X_REG_07_EN_TERM_MAS_M_MASKASK	0x1
#define BQ2589X_REG_07_WATCHDOG_MAS_MASK		0x3
#define BQ2589X_REG_07_EN_TIMER_MAS_MASK			0x1
#define BQ2589X_REG_07_CHG_TIMER_MAS_MASK		0x1
#define BQ2589X_REG_07_JEITA_ISET_MAS_MASK		0x1
//---------------------------reg08--------------------------
#define BQ2589X_REG_08_BAT_COMP		(5)
#define BQ2589X_REG_08_VCLAMP			(2)
#define BQ2589X_REG_08_TREG				(0)

#define BQ2589X_REG_08_BAT_COMP_MASK			0x7
#define BQ2589X_REG_08_VCLAMP_MASK			0x7
#define BQ2589X_REG_08_TREG_MASK				0x3
//---------------------------reg09--------------------------
#define BQ2589X_REG_09_FORCE_ICO		(7)
#define BQ2589X_REG_09_TMR2X_EN		(6)
#define BQ2589X_REG_09_BATFET_DIS		(5)
#define BQ2589X_REG_09_JEITA_VSET		(4)
#define BQ2589X_REG_09_BATFET_RST_EN	(2)

#define BQ2589X_REG_09_FORCE_ICO_MASK			0x1
#define BQ2589X_REG_09_TMR2X_EN_MASK			0x1
#define BQ2589X_REG_09_BATFET_DIS_MASK			0x1
#define BQ2589X_REG_09_JEITA_VSET_MASK			0x1
#define BQ2589X_REG_09_BATFET_RST_EN_MASK		0x1
//---------------------------reg0a--------------------------
#define BQ2589X_REG_0A_BOOSTV			(4)
#define BQ2589X_REG_0A_BOOST_LIM		(0)

#define BQ2589X_REG_0A_BOOSTV_MASK			0xf
#define BQ2589X_REG_0A_BOOST_LIM_MASK			0x7
//---------------------------reg0b--------------------------
#define BQ2589X_REG_0B_VBUS_STAT		(5)
#define BQ2589X_REG_0B_CHG_STAT		(3)
#define BQ2589X_REG_0B_PG_STAT			(2)
#define BQ2589X_REG_0B_SDP_STAT		(1)
#define BQ2589X_REG_0B_VSYS_STAT		(0)

#define BQ2589X_REG_0B_VBUS_STAT_MASK			0x7
#define BQ2589X_REG_0B_CHG_STAT_MASK			0x3
#define BQ2589X_REG_0B_PG_STAT_MASK			0x1
#define BQ2589X_REG_0B_SDP_STAT_MASK			0x1
#define BQ2589X_REG_0B_VSYS_STAT_MASK			0x1
//---------------------------reg0c--------------------------
#define BQ2589X_REG_0C_WATCHDOG_FULAT	(7)
#define BQ2589X_REG_0C_BOOST_FULAT		(6)
#define BQ2589X_REG_0C_CHR_FULAT			(4)
#define BQ2589X_REG_0C_BAT_FULAT			(3)
#define BQ2589X_REG_0C_NTC_FULAT			(0)

#define BQ2589X_REG_0C_WATCHDOG_FULAT_MASK	0x1
#define BQ2589X_REG_0C_BOOST_FULAT_MASK		0x1
#define BQ2589X_REG_0C_CHR_FULAT_MASK			0x3
#define BQ2589X_REG_0C_BAT_FULAT_MASK			0x1
#define BQ2589X_REG_0C_NTC_FULAT_MASK			0x7
//---------------------------reg0d--------------------------
#define BQ2589X_REG_0D_FORCE_VINDPM	(7)
#define BQ2589X_REG_0D_VINDPM			(0)

#define BQ2589X_REG_0D_FORCE_VINDPM_MASK		0x1
#define BQ2589X_REG_0D_VINDPM_MASK			0x7f
//---------------------------reg0e--------------------------
#define BQ2589X_REG_0E_THERM_STAT		(7)
#define BQ2589X_REG_0E_BATV				(0)

#define BQ2589X_REG_0E_THERM_STAT_MASK		0x1
#define BQ2589X_REG_0E_BATV_MASK				0x7f
//---------------------------reg0f--------------------------
#define BQ2589X_REG_0F_SYSV				(0)

#define BQ2589X_REG_0F_SYSV_MASK				0x7f
//---------------------------reg10--------------------------
#define BQ2589X_REG_10_TSPCT			(0)

#define BQ2589X_REG_10_TSPCT_MASK				0x7f
//---------------------------reg11--------------------------
#define BQ2589X_REG_11_VBUS_GD			(7)
#define BQ2589X_REG_11_VBUSV			(0)

#define BQ2589X_REG_11_VBUS_GD_MASK			0x1
#define BQ2589X_REG_11_VBUSV_MASK				0x7f
//---------------------------reg12--------------------------
#define BQ2589X_REG_12_ICHGR			(0)

#define BQ2589X_REG_12_ICHGR_MASK				0x7f
//---------------------------reg13--------------------------
#define BQ2589X_REG_13_VDPM_STAT		(7)
#define BQ2589X_REG_13_IDPM_STAT		(6)
#define BQ2589X_REG_13_IDPM_LIM			(0)

#define BQ2589X_REG_13_VDPM_STAT_MASK		0x1
#define BQ2589X_REG_13_IDPM_STAT_MASK			0x1
#define BQ2589X_REG_13_IDPM_LIM_MASK			0x3f
//---------------------------reg14---------------------------
#define BQ2589X_REG_14_REG_RST			(7)
#define BQ2589X_REG_14_ICO_OPTIMIZED	(6)
#define BQ2589X_REG_14_PN				(3)
#define BQ2589X_REG_14_TS_PROFILE		(2)
#define BQ2589X_REG_14_DEV_REV			(0)

#define BQ2589X_REG_14_REG_RST_MASK			0x1
#define BQ2589X_REG_14_ICO_OPTIMIZED_MASK		0x1
#define BQ2589X_REG_14_PN_MASK					0x7
#define BQ2589X_REG_14_TS_PROFILE_MASK			0x1
#define BQ2589X_REG_14_DEV_REV_MASK			0x3

#define TEMP_NR_RNG	4
#define BATTID_STR_LEN	8
#define RANGE	25
/* User limits for sysfs charge enable/disable */
#define USER_SET_CHRG_DISABLE	0
#define USER_SET_CHRG_LMT1	1
#define USER_SET_CHRG_LMT2	2
#define USER_SET_CHRG_LMT3	3
#define USER_SET_CHRG_NOLMT	4

#define INPUT_CHRG_CURR_0	0
#define INPUT_CHRG_CURR_100	100
#define INPUT_CHRG_CURR_500	500
#define INPUT_CHRG_CURR_950	950
#define INPUT_CHRG_CURR_1500	1500
/* Charger Master Temperature Control Register */
#define MSIC_CHRTMPCTRL         0x18E
/* Higher Temprature Values*/
#define CHRTMPCTRL_TMPH_60      (3 << 6)
#define CHRTMPCTRL_TMPH_55      (2 << 6)
#define CHRTMPCTRL_TMPH_50      (1 << 6)
#define CHRTMPCTRL_TMPH_45      (0 << 6)

/* Lower Temprature Values*/
#define CHRTMPCTRL_TMPL_15      (3 << 4)
#define CHRTMPCTRL_TMPL_10      (2 << 4)
#define CHRTMPCTRL_TMPL_05      (1 << 4)
#define CHRTMPCTRL_TMPL_00      (0 << 4)

enum bq22589x_bat_chrg_mode {
	BATT_CHRG_FULL = 0,
	BATT_CHRG_NORMAL = 1,
	BATT_CHRG_MAINT = 2,
	BATT_CHRG_NONE = 3
};


/*********************************************************************
 * SFI table entries Structures
 ********************************************************************/
/*********************************************************************
 *		Platform Data Section
 *********************************************************************/
/* Battery Thresholds info which need to get from SMIP area */
struct platform_batt_safety_param {
	u8 smip_rev;
	u8 fpo;		/* fixed implementation options */
	u8 fpo1;	/* fixed implementation options1 */
	u8 rsys;	/* System Resistance for Fuel gauging */

	/* Minimum voltage necessary to
	 * be able to safely shut down */
	short int vbatt_sh_min;

	/* Voltage at which the battery driver
	 * should report the LEVEL as CRITICAL */
	short int vbatt_crit;

	short int itc;		/* Charge termination current */
	short int temp_high;	/* Safe Temp Upper Limit */
	short int temp_low;	/* Safe Temp lower Limit */
	u8 brd_id;		/* Unique Board ID */
} __packed;

/* Parameters defining the range */
struct platform_temp_mon_table {
	short int temp_up_lim;
	short int temp_low_lim;
	short int rbatt;
	short int full_chrg_vol;
	short int full_chrg_cur;
	short int maint_chrg_vol_ll;
	short int maint_chrg_vol_ul;
	short int maint_chrg_cur;
} __packed;

struct platform_batt_profile {
	char batt_id[BATTID_STR_LEN];
	unsigned short int voltage_max;
	unsigned int capacity;
	u8 battery_type;
	u8 temp_mon_ranges;
	struct platform_temp_mon_table temp_mon_range[TEMP_NR_RNG];

} __packed;

struct bq2589x_platform_data {
	bool slave_mode;
	short int temp_low_lim;
	bool sfi_tabl_present;
	short int safetemp;
	struct platform_batt_profile batt_profile;
	struct platform_batt_safety_param safety_param;
	struct power_supply_throttle *throttle_states;
	struct ps_batt_chg_prof *chg_profile;

	char **supplied_to;
	size_t	num_supplicants;
	size_t num_throttle_states;
	unsigned long supported_cables;

	/* safety charegr setting */
	int max_cc;
	int max_cv;
	int ichg_max;
	int ibat_max;
	int max_temp;
	int min_temp;
	
	/*gpio config*/
	int chg_irq_gpio;
	int chg_en_gpio;

	/* Function pointers for platform specific initialization */
	int (*init_platform_data)(void);
	int (*get_irq_number)(void);
	int (*query_otg)(void *, void *);
	int (*drive_vbus)(bool);
	int (*get_battery_pack_temp)(int *);
	void (*free_platform_data)(void);
};

#ifdef CONFIG_CHARGER_BQ2589X
extern int bq2589x_slave_mode_enable_charging(int volt, int cur, int ilim);
extern int bq2589x_slave_mode_disable_charging(void);
extern int bq2589x_query_battery_status(void);
extern int bq2589x_get_battery_pack_temp(int *temp);
extern int bq2589x_get_battery_health(void);
extern bool bq2589x_is_volt_shutdown_enabled(void);
#else
static int bq2589x_get_battery_health(void)
{
	return 0;
}
#endif
#endif /* __BQ2589X_CHARGER_H_ */
