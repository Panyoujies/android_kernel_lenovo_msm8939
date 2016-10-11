/*
 * Maxim MAX77819 Charger Driver
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

//#define DEBUG
//#define VERBOSE_DEBUG
#define log_level  0
#define log_worker 0

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include <linux/wakelock.h>

#include <linux/power_supply.h>
#include <linux/mfd/max77819.h>
#include <linux/mfd/max77819-charger.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define DRIVER_DESC    "MAX77819 Charger Driver"
#define DRIVER_NAME    MAX77819_CHARGER_NAME
#define DRIVER_VERSION MAX77819_DRIVER_VERSION".3-rc"
#define DRIVER_AUTHOR  "Gyungoh Yoo <jack.yoo@maximintegrated.com>"

#define IRQ_WORK_DELAY              0
#define MONITOR_WORK_DELAY			msecs_to_jiffies(30*1000)
#define IRQ_WORK_INTERVAL           msecs_to_jiffies(5000)
#define LOG_WORK_INTERVAL           msecs_to_jiffies(30000)
#define AICL_WORK_DELAY			msecs_to_jiffies(10)

#define VERIFICATION_UNLOCK         0
#if 0
#define INPUT_SRC_LOW_VBAT_LIMIT               3700000
#define INPUT_SRC_MIDL_VBAT_LIMIT              3900000
#endif
#define INPUT_SRC_MIDH_VBAT_LIMIT              4100000
#define INPUT_SRC_HIGH_VBAT_LIMIT              4350000
#if 0
#define INPUT_SRC_VOLT_LMT_395                 3950000
#define INPUT_SRC_VOLT_LMT_415                 4150000 
#endif
#define INPUT_SRC_VOLT_LMT_440                 4400000
#define INPUT_SRC_VOLT_LMT_450                 4500000
#define INPUT_SRC_VOLT_LMT_DEF                 4500000
#define INPUT_SRC_VOLT_LMT_470                 4700000
/* Register map */
#define PMIC_ID                     0x20
#define CHGINT                      0x30
#define CHGINTM1                    0x31
#define CHGINT1_AICLOTG             BIT (7)
#define CHGINT1_TOPOFF              BIT (6)
#define CHGINT1_OVP                 BIT (5)
#define CHGINT1_DC_UVP              BIT (4)
#define CHGINT1_CHG                 BIT (3)
#define CHGINT1_BAT                 BIT (2)
#define CHGINT1_THM                 BIT (1)
#define CHG_STAT                    0x32
#define DC_BATT_DTLS                0x33
#define DC_BATT_DTLS_DC_AICL        BIT (7)
#define DC_BATT_DTLS_DC_I           BIT (6)
#define DC_BATT_DTLS_DC_OVP         BIT (5)
#define DC_BATT_DTLS_DC_UVP         BIT (4)
#define DC_BATT_DTLS_BAT_DTLS       BITS(3,2)
#define DC_BATT_DTLS_BATDET_DTLS    BITS(1,0)
#define CHG_DTLS                    0x34
#define CHG_DTLS_THM_DTLS           BITS(7,5)
#define CHG_DTLS_TOPOFF_DTLS        BIT (4)
#define CHG_DTLS_CHG_DTLS           BITS(3,0)
#define BAT2SYS_DTLS                0x35
#define BAT2SOC_CTL                 0x36
#define CHGCTL1                     0x37
#define CHGCTL1_SFO_DEBOUNCE_TMR    BITS(7,6)
#define CHGCTL1_SFO_DEBOUNCE_EN     BIT (5)
#define CHGCTL1_THM_DIS             BIT (4)
#define CHGCTL1_JEITA_EN            BIT (3)
#define CHGCTL1_BUCK_EN             BIT (2)
#define CHGCTL1_CHGPROT             BITS(1,0)
#define FCHGCRNT                    0x38
#define FCHGCRNT_FCHGTIME           BITS(7,5)
#define FCHGCRNT_CHGCC              BITS(4,0)
#define TOPOFF                      0x39
#define TOPOFF_TOPOFFTIME           BITS(7,5)
#define TOPOFF_IFST2P8              BIT (4)
#define TOPOFF_ITOPOFF              BITS(2,0)
#define BATREG                      0x3A
#define BATREG_REGTEMP              BITS(7,6)
#define BATREG_CHGRSTRT             BIT (5)
#define BATREG_MBATREG              BITS(4,1)
#define BATREG_VICHG_GAIN           BIT (0)
#define DCCRNT                      0x3B
#define DCCRNT_DCILMT               BITS(5,0)
#define AICLCNTL                    0x3C
#define AICLCNTL_AICL_RESET         BIT (5)
#define AICLCNTL_AICL               BITS(4,1)
#define AICLCNTL_DCMON_DIS          BIT (0)
#define RBOOST_CTL1                 0x3D
#define CHGCTL2                     0x3E
#define CHGCTL2_DCILIM_EN           BIT (7)
#define CHGCTL2_PREQCUR             BITS(6,5)
#define CHGCTL2_CEN                 BIT (4)
#define CHGCTL2_QBATEN              BIT (3)
#define CHGCTL2_VSYSREG             BITS(2,0)
#define BATDET                      0x3F
#define USBCHGCTL                   0x40
#define MBATREGMAX                  0x41
#define CHGCCMAX                    0x42
#define RBOOST_CTL2                 0x43
#define CHGINT2                     0x44
#define CHGINTMSK2                  0x45
#define CHGINT2_DC_V                BIT (7)
#define CHGINT2_CHG_WDT             BIT (4)
#define CHGINT2_CHG_WDT_WRN         BIT (0)
#define CHG_WDTC                    0x46
#define CHG_WDT_CTL                 0x47
#define CHG_WDT_DTLS                0x48

struct max77819_charger *g_charger;
int max77819_cc = -255;
void max77819_charger_otg(bool enable);
int charger_is_done = 0;
static int aicl_count = 0;
static int aicl_update = false;
extern int max77819_otg_mode;
extern int get_vbus(void);
int limit_level=0;
static void max77819_set_appropriate_cv(struct max77819_charger *chip);
static void max77819_set_appropriate_cc(struct max77819_charger *chip);
static int max77819_charger_reinit_dev (struct max77819_charger *me);
struct max77819_charger {
    struct mutex                           lock;
    struct max77819_dev                   *chip;
    struct max77819_io                    *io;
    struct device                         *dev;
    struct kobject                        *kobj;
    struct attribute_group                *attr_grp;
    struct max77819_charger_platform_data *pdata;
	struct wake_lock					  chg_wake_lock;
	struct wake_lock					  chg_update_wk;
    int                                    irq;
    u8                                     irq1_saved;
    u8                                     irq2_saved;
    spinlock_t                             irq_lock;
    struct delayed_work                    irq_work;
    struct delayed_work                    log_work;
    struct delayed_work                    monitor_work;
    struct delayed_work                    aicl_work;
    struct power_supply                    psy;
    struct power_supply                   *psy_this;
    struct power_supply                   *psy_ext;
    struct power_supply                   *psy_coop; /* cooperating charger */
    struct power_supply                   *psy_fg; /*battery */
    bool                                   dev_enabled;
    bool                                   dev_initialized;
    int                                    current_limit_volatile;
    int                                    current_limit_permanent;
    int                                    charge_current_volatile;
    int                                    charge_current_permanent;
    int                                    present;
    int                                    health;
    int                                    status;
    int                                    charge_type;
    int                                    thm_gpio;
	int                                    batt_temp;
	int                                    max_temp;
	int                                    vbat_cool;
	int                                    vbat_warm;
	int                                    ibat_warm;
	int                                    ibat_cool;
	int                                    min_temp;
	int	                                   warm_temp;
	int                                    cool_temp;
	int                                    batt_volt;
	int                                    batt_curr;
	int                                    batt_status;
	int                                    batt_soc;
	bool                                   bat_is_warm;
	bool                                   bat_is_cool;
	bool                                   dec_cur_bat_cool;
	bool                                   in_otg;
	bool                                   chg_done;
    u8                                    pmic_id;
};
int pmic_id = 0;
module_param(pmic_id, int, 0644);
#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

enum {
    BATDET_DTLS_CONTACT_BREAK       = 0b00,
    BATDET_DTLS_BATTERY_DETECTED_01 = 0b01,
    BATDET_DTLS_BATTERY_DETECTED_10 = 0b10,
    BATDET_DTLS_BATTERY_REMOVED     = 0b11,
};

static char *max77819_charger_batdet_details[] = {
    [BATDET_DTLS_CONTACT_BREAK]       = "contact break",
    [BATDET_DTLS_BATTERY_DETECTED_01] = "battery detected (01)",
    [BATDET_DTLS_BATTERY_DETECTED_10] = "battery detected (10)",
    [BATDET_DTLS_BATTERY_REMOVED]     = "battery removed",
};

enum {
    DC_UVP_INVALID = 0,
    DC_UVP_VALID   = 1,
};

static char *max77819_charger_dcuvp_details[] = {
    [DC_UVP_INVALID] = "VDC is invalid; VDC < VDC_UVLO",
    [DC_UVP_VALID]   = "VDC is valid; VDC > VDC_UVLO",
};

enum {
    DC_OVP_VALID   = 0,
    DC_OVP_INVALID = 1,
};

static char *max77819_charger_dcovp_details[] = {
    [DC_OVP_VALID]   = "VDC is valid; VDC < VDC_OVLO",
    [DC_OVP_INVALID] = "VDC is invalid; VDC > VDC_OVLO",
};

enum {
    DC_I_VALID   = 0,
    DC_I_INVALID = 1,
};

static char *max77819_charger_dci_details[] = {
    [DC_I_VALID]   = "IDC is valid; IDC < DCILMT",
    [DC_I_INVALID] = "IDC is invalid; IDC > DCILMT",
};

enum {
    DC_AICL_OK  = 0,
    DC_AICL_NOK = 1,
};

static char *max77819_charger_aicl_details[] = {
    [DC_AICL_OK]  = "VDC > AICL threshold",
    [DC_AICL_NOK] = "VDC < AICL threshold",
};

enum {
    BAT_DTLS_UVP     = 0b00,
    BAT_DTLS_TIMEOUT = 0b01,
    BAT_DTLS_OK      = 0b10,
    BAT_DTLS_OVP     = 0b11,
};

static char *max77819_charger_bat_details[] = {
    [BAT_DTLS_UVP]     = "battery voltage < 2.1V",
    [BAT_DTLS_TIMEOUT] = "timer fault",
    [BAT_DTLS_OK]      = "battery okay",
    [BAT_DTLS_OVP]     = "battery overvoltage",
};

enum {
    CHG_DTLS_DEAD_BATTERY     = 0b0000,
    CHG_DTLS_PRECHARGE        = 0b0001,
    CHG_DTLS_FASTCHARGE_CC    = 0b0010,
    CHG_DTLS_FASTCHARGE_CV    = 0b0011,
    CHG_DTLS_TOPOFF           = 0b0100,
    CHG_DTLS_DONE             = 0b0101,
    CHG_DTLS_TIMER_FAULT      = 0b0110,
    CHG_DTLS_TEMP_SUSPEND     = 0b0111,
    CHG_DTLS_OFF              = 0b1000,
    CHG_DTLS_THM_LOOP         = 0b1001,
    CHG_DTLS_TEMP_SHUTDOWN    = 0b1010,
    CHG_DTLS_BUCK             = 0b1011,
    CHG_DTLS_OTG_OVER_CURRENT = 0b1100,
    CHG_DTLS_USB_SUSPEND      = 0b1101,
};

static char *max77819_charger_chg_details[] = {
    [CHG_DTLS_DEAD_BATTERY] =
        "charger is in dead-battery region",
    [CHG_DTLS_PRECHARGE] =
        "charger is in precharge mode",
    [CHG_DTLS_FASTCHARGE_CC] =
        "charger is in fast-charge constant current mode",
    [CHG_DTLS_FASTCHARGE_CV] =
        "charger is in fast-charge constant voltage mode",
    [CHG_DTLS_TOPOFF] =
        "charger is in top-off mode",
    [CHG_DTLS_DONE] =
        "charger is in done mode",
    [CHG_DTLS_TIMER_FAULT] =
        "charger is in timer fault mode",
    [CHG_DTLS_TEMP_SUSPEND] =
        "charger is in temperature suspend mode",
    [CHG_DTLS_OFF] =
        "buck off, charger off",
    [CHG_DTLS_THM_LOOP] =
        "charger is operating with its thermal loop active",
    [CHG_DTLS_TEMP_SHUTDOWN] =
        "charger is off and junction temperature is > TSHDN",
    [CHG_DTLS_BUCK] =
        "buck on, charger off",
    [CHG_DTLS_OTG_OVER_CURRENT] =
        "charger OTG current limit is exceeded longer than debounce time",
    [CHG_DTLS_USB_SUSPEND] =
        "USB suspend",
};

enum {
    TOPOFF_NOT_REACHED = 0,
    TOPOFF_REACHED     = 1,
};

static char *max77819_charger_topoff_details[] = {
    [TOPOFF_NOT_REACHED] = "topoff is not reached",
    [TOPOFF_REACHED]     = "topoff is reached",
};

enum {
    THM_DTLS_LOW_TEMP_SUSPEND   = 0b001,
    THM_DTLS_LOW_TEMP_CHARGING  = 0b010,
    THM_DTLS_STD_TEMP_CHARGING  = 0b011,
    THM_DTLS_HIGH_TEMP_CHARGING = 0b100,
    THM_DTLS_HIGH_TEMP_SUSPEND  = 0b101,
};

static char *max77819_charger_thm_details[] = {
    [THM_DTLS_LOW_TEMP_SUSPEND]   = "cold; T < T1",
    [THM_DTLS_LOW_TEMP_CHARGING]  = "cool; T1 < T < T2",
    [THM_DTLS_STD_TEMP_CHARGING]  = "normal; T2 < T < T3",
    [THM_DTLS_HIGH_TEMP_CHARGING] = "warm; T3 < T < T4",
    [THM_DTLS_HIGH_TEMP_SUSPEND]  = "hot; T4 < T",
};

#define CHGINT1 CHGINT
#define max77819_charger_read_irq_status(_me, _irq_reg) \
        ({\
            u8 __irq_current = 0;\
            int __rc = max77819_read((_me)->io, _irq_reg, &__irq_current);\
            if (unlikely(IS_ERR_VALUE(__rc))) {\
                log_err(#_irq_reg" read error [%d]\n", __rc);\
                __irq_current = 0;\
            }\
            __irq_current;\
        })

enum {
    CFG_CHGPROT = 0,
    CFG_SFO_DEBOUNCE_TMR,
    CFG_SFO_DEBOUNCE_EN,
    CFG_THM_DIS,
    CFG_JEITA_EN,
    CFG_BUCK_EN,
    CFG_DCILIM_EN,
    CFG_PREQCUR,
    CFG_CEN,
    CFG_QBATEN,
    CFG_VSYSREG,
    CFG_DCILMT,
    CFG_FCHGTIME,
    CFG_CHGCC,
    CFG_AICL_RESET,
    CFG_AICL,
    CFG_DCMON_DIS,
    CFG_MBATREG,
    CFG_CHGRSTRT,
    CFG_TOPOFFTIME,
    CFG_ITOPOFF,
};

static struct max77819_bitdesc max77819_charger_cfg_bitdesc[] = {
    #define CFG_BITDESC(_cfg_bit, _cfg_reg) \
            [CFG_##_cfg_bit] = MAX77819_BITDESC(_cfg_reg, _cfg_reg##_##_cfg_bit)

    CFG_BITDESC(CHGPROT         , CHGCTL1 ),
    CFG_BITDESC(SFO_DEBOUNCE_TMR, CHGCTL1 ),
    CFG_BITDESC(SFO_DEBOUNCE_EN , CHGCTL1 ),
    CFG_BITDESC(THM_DIS         , CHGCTL1 ),
    CFG_BITDESC(JEITA_EN        , CHGCTL1 ),
    CFG_BITDESC(BUCK_EN         , CHGCTL1 ),
    CFG_BITDESC(DCILIM_EN       , CHGCTL2 ),
    CFG_BITDESC(PREQCUR         , CHGCTL2 ),
    CFG_BITDESC(CEN             , CHGCTL2 ),
    CFG_BITDESC(QBATEN          , CHGCTL2 ),
    CFG_BITDESC(VSYSREG         , CHGCTL2 ),
    CFG_BITDESC(DCILMT          , DCCRNT  ),
    CFG_BITDESC(FCHGTIME        , FCHGCRNT),
    CFG_BITDESC(CHGCC           , FCHGCRNT),
    CFG_BITDESC(AICL_RESET      , AICLCNTL),
    CFG_BITDESC(AICL            , AICLCNTL),
    CFG_BITDESC(DCMON_DIS       , AICLCNTL),
    CFG_BITDESC(MBATREG         , BATREG  ),
    CFG_BITDESC(CHGRSTRT        , BATREG  ),
    CFG_BITDESC(TOPOFFTIME      , TOPOFF  ),
    CFG_BITDESC(ITOPOFF         , TOPOFF  ),
};
#define __cfg_bitdesc(_cfg) (&max77819_charger_cfg_bitdesc[CFG_##_cfg])

#define PROTCMD_UNLOCK  3
#define PROTCMD_LOCK    0

/**
 * fg_chip_get_property - read a power supply property from Fuel Gauge driver
 * @psp : Power Supply property
 *
 * Return power supply property value
 *
 */
int fg_chip_get_property(struct max77819_charger *me,enum power_supply_property psp)
{
	union power_supply_propval val;
	int ret = -ENODEV;

	if (!me->psy_fg)
		me->psy_fg = power_supply_get_by_name("battery");
	if (me->psy_fg) {
		ret = me->psy_fg->get_property(me->psy_fg, psp, &val);
		if (!ret)
			return val.intval;
	}
	return ret;
}
static __always_inline int max77819_charger_unlock (struct max77819_charger *me)
{
    int rc;

    rc = max77819_write_bitdesc(me->io, __cfg_bitdesc(CHGPROT), PROTCMD_UNLOCK);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to unlock [%d]\n", rc);
        goto out;
    }

#if VERIFICATION_UNLOCK
    do {
        u8 chgprot = 0;

        rc = max77819_read_bitdesc(me->io, __cfg_bitdesc(CHGPROT), &chgprot);
        if (unlikely(IS_ERR_VALUE(rc) || chgprot != PROTCMD_UNLOCK)) {
            log_err("access denied - CHGPROT %X [%d]\n", chgprot, rc);
            rc = -EACCES;
            goto out;
        }
    } while (0);
#endif /* VERIFICATION_UNLOCK */

out:
    return rc;
}

static __always_inline int max77819_charger_lock (struct max77819_charger *me)
{
    int rc;

    rc = max77819_write_bitdesc(me->io, __cfg_bitdesc(CHGPROT), PROTCMD_LOCK);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to lock [%d]\n", rc);
    }

    return rc;
}

#define max77819_charger_read_config(_me, _cfg, _val_ptr) \
        ({\
            int __rc = max77819_read_bitdesc((_me)->io, __cfg_bitdesc(_cfg),\
                _val_ptr);\
            if (unlikely(IS_ERR_VALUE(__rc))) {\
                log_err("read config "#_cfg" error [%d]\n", __rc);\
            } else {\
                log_vdbg("read config "#_cfg": %Xh\n", *(_val_ptr));\
            }\
            __rc;\
        })
#define max77819_charger_write_config(_me, _cfg, _val) \
        ({\
            int __rc = max77819_charger_unlock(_me);\
            if (likely(!IS_ERR_VALUE(__rc))) {\
                __rc = max77819_write_bitdesc((_me)->io, __cfg_bitdesc(_cfg),\
                    _val);\
                if (unlikely(IS_ERR_VALUE(__rc))) {\
                    log_err("write config "#_cfg" error [%d]\n", __rc);\
                } else {\
                    log_vdbg("write config "#_cfg": %Xh\n", _val);\
                }\
                max77819_charger_lock(_me);\
            }\
            __rc;\
        })

static __inline int max77819_charger_get_dcilmt (struct max77819_charger *me,
    int *uA)
{
    int rc;
    u8 dcilmt = 0;

    rc = max77819_charger_read_config(me, DCILMT, &dcilmt);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    if (unlikely(dcilmt >= 0x3F)) {
        *uA = MAX77819_CHARGER_CURRENT_UNLIMIT;
        log_vdbg("<get_dcilmt> no limit\n");
        goto out;
    }

    *uA = dcilmt < 0x03 ? 100000 :
          dcilmt < 0x35 ? (int)(dcilmt - 0x03) * 25000 +  275000 :
                          (int)(dcilmt - 0x35) * 37500 + 1537500;
    log_vdbg("<get_dcilmt> %Xh -> %duA\n", dcilmt, *uA);

out:
    return rc;
}

static int max77819_charger_set_dcilmt (struct max77819_charger *me, int uA)
{
    u8 dcilmt;

    if (unlikely(uA == MAX77819_CHARGER_CURRENT_UNLIMIT)) {
        dcilmt = 0x3F;
        log_vdbg("<set_dcilmt> no limit\n");
        goto out;
    }

    dcilmt = uA <  275000 ? 0x00 :
             uA < 1537500 ? DIV_ROUND_UP(uA -  275000, 25000) + 0x03 :
             uA < 1875000 ? DIV_ROUND_UP(uA - 1537500, 37500) + 0x35 : 0x3F;
    log_vdbg("<set_dcilmt> %duA -> %Xh\n", uA, dcilmt);

out:
    return max77819_charger_write_config(me, DCILMT, dcilmt);
}

static __inline int max77819_charger_get_enable (struct max77819_charger *me,
    int *en)
{
    int rc;
    u8 cen = 0;

    rc = max77819_charger_read_config(me, CEN, &cen);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    *en = !!cen;
    log_dbg("<get_enable> %s\n", en ? "enabled" : "disabled");

out:
    return rc;
}

int max77819_charger_enable (int en)
{
	if(g_charger == NULL)
		return 0;
    log_dbg("<charger_enable> %s\n", en ? "enabling" : "disabling");
    return max77819_charger_write_config(g_charger, BUCK_EN, !!en);
}
static int max77819_charger_set_enable (struct max77819_charger *me, int en)
{
    log_dbg("<set_enable> %s\n", en ? "enabling" : "disabling");
    return max77819_charger_write_config(me, CEN, !!en);
}

int max77819_set_charging_enable (int en)
{
	if(g_charger == NULL)
		return 0;
    log_dbg("<charging_enable> %s\n", en ? "enabling" : "disabling");
    return max77819_charger_set_enable(g_charger,!!en);
}
static __inline int max77819_charger_get_chgcc (struct max77819_charger *me,
    int *uA)
{
    int rc;
    u8 dcilmt = 0;

    rc = max77819_charger_read_config(me, CHGCC, &dcilmt);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    *uA = dcilmt < 0x01 ? 0 :
          dcilmt < 0x1C ? (int)(dcilmt - 0x01) * 50000 +  250000 :
          dcilmt < 0x1F ? (int)(dcilmt - 0x1C) * 62500 + 1620000 : 1800000;
    log_vdbg("<get_chgcc> %Xh -> %duA\n", dcilmt, *uA);

out:
    return rc;
}

static int max77819_charger_set_chgcc (struct max77819_charger *me, int uA)
{
    u8 chgcc;

    chgcc = uA <  250000 ? 0x00 :
            uA < 1620000 ? DIV_ROUND_UP(uA -  250000, 50000) + 0x01 :
            uA < 1800000 ? DIV_ROUND_UP(uA - 1620000, 62500) + 0x1C : 0x1F;
    log_vdbg("<set_chgcc> %duA -> %Xh\n", uA, chgcc);

    return max77819_charger_write_config(me, CHGCC, chgcc);
}

static int max77819_charger_set_charge_current (struct max77819_charger *me,
    int limit_uA, int charge_uA)
{
    int rc;

    #define DCILMT_MIN  100000
    #define DCILMT_MAX  1875000
    #define CHGCC_MIN   0
    #define CHGCC_MAX   1800000
//  max77819_charger_set_chgcc(me, CHGCC_MIN);

    if (limit_uA == MAX77819_CHARGER_CURRENT_MAXIMUM) {
        limit_uA = DCILMT_MAX;
    } else if (limit_uA == MAX77819_CHARGER_CURRENT_MINIMUM) {
        limit_uA = DCILMT_MIN;
    } else if (limit_uA != MAX77819_CHARGER_CURRENT_UNLIMIT) {
        limit_uA  = max(DCILMT_MIN, limit_uA );
    }

    if (charge_uA == MAX77819_CHARGER_CURRENT_UNLIMIT ||
        charge_uA == MAX77819_CHARGER_CURRENT_MAXIMUM) {
        charge_uA = CHGCC_MAX;
    } else if (limit_uA == MAX77819_CHARGER_CURRENT_MINIMUM) {
        charge_uA = CHGCC_MIN;
    } else {
        charge_uA = max(CHGCC_MIN , charge_uA);
    }

	/*limit_uA control the vbus current;charger_uA control the battery charging current;
	 *when limit_uA < charger_uA, we will use limit_uA instead of charger_uA
	 * */
    if (likely(limit_uA == MAX77819_CHARGER_CURRENT_UNLIMIT ||
               limit_uA >= charge_uA)) {
        rc = max77819_charger_set_dcilmt(me, limit_uA);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        if (likely(me->dev_enabled)) {
            rc = max77819_charger_set_chgcc(me, charge_uA);
        }

        goto out;
    }

    if (likely(me->dev_enabled)) {
        log_dbg("setting current %duA but limited up to %duA\n", charge_uA,
            limit_uA);
        if (likely(limit_uA >= CHGCC_MIN)) {
            rc = max77819_charger_set_chgcc(me, limit_uA);
        } else {
            log_warn("disabling charger ; charging current < %uA\n", CHGCC_MIN);
            rc = max77819_charger_set_enable(me, false);
        }

        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
    }

    rc = max77819_charger_set_dcilmt(me, limit_uA);

out:
    return rc;
}

static bool max77819_charger_present_input (struct max77819_charger *me)
{
    u8 dc_uvp = 0;
    int rc;

    rc = max77819_read_reg_bit(me->io, DC_BATT_DTLS, DC_UVP, &dc_uvp);
    if (unlikely(IS_ERR_VALUE(rc))) {
        return false;
    }

    return (dc_uvp == DC_UVP_VALID);
}

static void max77819_charger_thm_control(struct max77819_charger *me)
{
	int rc;
    u8 chg_dtls,chg_cc,chg_cv;

    if ((!gpio_is_valid(me->thm_gpio)))
		return;

    rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);

    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("thm control: CHG_DTLS read error [%d]\n", rc);
        goto out;
    }


    if (chg_dtls == 0x27) {
		printk("thm control: initial CHG_DTLS %Xh\n", chg_dtls);
		gpio_set_value_cansleep(me->thm_gpio,0);
		udelay(200);
		rc = max77819_charger_write_config(me, THM_DIS, 0);
		udelay(200);
		rc |= max77819_charger_write_config(me, THM_DIS, 1);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("thm control: thm register write error [%d]\n", rc);
			goto out;
		}
		rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("CHG_DTLS read error [%d]\n", rc);
			goto out;
		} else
			 printk("after thm handle: CHG_DTLS %Xh\n", chg_dtls);
	
       max77819_read(me->io, FCHGCRNT, &chg_cc);
       max77819_read(me->io, BATREG, &chg_cv);
	   printk("%s,before reinit charge cc,cv %Xh,%Xh\n",__func__,chg_cc,chg_cv);
		//reinit dev
		rc = max77819_charger_reinit_dev(me);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("%s,reinit_dev_err\n",__func__);
		}
       max77819_read(me->io, FCHGCRNT, &chg_cc);
       max77819_read(me->io, BATREG, &chg_cv);
	   printk("%s,after reinit charge cc,cv:%Xh,%Xh\n",__func__,chg_cc,chg_cv);
    } else if (chg_dtls == 0xa7) {
		printk("thm control: initial CHG_DTLS %Xh\n", chg_dtls);
		gpio_set_value_cansleep(me->thm_gpio,1);
		udelay(200);
		rc = max77819_charger_write_config(me, THM_DIS, 0);
		udelay(200);
		rc |= max77819_charger_write_config(me, THM_DIS, 1);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("thm control: thm register write error [%d]\n", rc);
			goto out;
		}
		rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("CHG_DTLS read error [%d]\n", rc);
			goto out;
		} else
			 printk("after thm handle: CHG_DTLS %Xh\n", chg_dtls);
       max77819_read(me->io, FCHGCRNT, &chg_cc);
       max77819_read(me->io, BATREG, &chg_cv);
	   printk("%s,before reinit charge cc,cv %Xh,%Xh\n",__func__,chg_cc,chg_cv);
		//reinit charger cc and cv
		rc = max77819_charger_reinit_dev(me);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("%s,reinit_dev_err\n",__func__);
		}
       max77819_read(me->io, FCHGCRNT, &chg_cc);
       max77819_read(me->io, BATREG, &chg_cv);
	   printk("%s,after reinit charge cc,cv:%Xh,%Xh\n",__func__,chg_cc,chg_cv);
	}
out:
    log_vdbg("thm control error !!! \n");
}

bool is_charger_enabled(void)
{
	if(g_charger == NULL)
		return false;
	return g_charger->dev_enabled;
}
bool is_in_otg_mode(void)
{
	if(g_charger == NULL)
		return false;
	return g_charger->in_otg;
}
void max77819_charger_otg(bool enable)
{
	int rc;
	if(g_charger == NULL)
	{
	    log_vdbg("g_charger is not initied  !!! \n");
		return;
	}
	if(enable){
		g_charger->in_otg = true;
		rc = max77819_write(g_charger->io, RBOOST_CTL2, 0x50);
		rc |= max77819_write(g_charger->io, RBOOST_CTL1, 0x21);
		rc |= max77819_write(g_charger->io, BAT2SOC_CTL, 0x20);
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("enabling otg was failed [%d]\n", rc);
		}
		else
			log_vdbg("enabled otg successfully !!! \n");
	}
	else{
		g_charger->in_otg = false;
		rc |= max77819_write(g_charger->io, BAT2SOC_CTL, 0x00);
		rc = max77819_write(g_charger->io, RBOOST_CTL2, 0x00);
		rc |= max77819_write(g_charger->io, RBOOST_CTL1, 0x20);
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("disabling otg was failed [%d]\n", rc);
		}
		else
			log_vdbg("disabled otg successfully !!! \n");

	}

}
static void max77819_set_appropriate_cv(struct max77819_charger *chip)
{
	int cv = 0;
	u8 val = 0;
	int rc = 0;
	if(chip->bat_is_cool)
		cv = chip->vbat_cool;
	else if(chip->bat_is_warm)
		cv = chip->vbat_warm;
	else
		cv = chip->pdata->charge_termination_voltage;
    val = cv < 3700000 ? 0x00 :
        (int)DIV_ROUND_UP(cv - 3700000, 50000)
        + 0x01;
    rc = max77819_charger_write_config(chip, MBATREG, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        pr_err("set cv error %d\n",rc);
    }
}

static void max77819_set_appropriate_cc(struct max77819_charger *chip)
{
	int cc = 0;
	int rc = 0;
	if(chip->bat_is_cool)
	{
		cc = chip->ibat_cool;
		if(chip->dec_cur_bat_cool)
			cc = cc/2;
	}else if(chip->bat_is_warm)
		cc = chip->ibat_warm;
	else
		cc = chip->charge_current_volatile;
    rc = max77819_charger_set_charge_current(chip, chip->current_limit_volatile,cc);
    if (unlikely(IS_ERR_VALUE(rc))) {
        pr_err("set cc error %d\n",rc);
    }
}
static int max77819_charger_exit_dev (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;

    max77819_charger_set_charge_current(me, me->current_limit_permanent, 0);
    max77819_charger_set_enable(me, false);

    me->current_limit_volatile  = me->current_limit_permanent;
    me->charge_current_volatile = me->charge_current_permanent;
    me->dev_enabled     = (!pdata->enable_coop || pdata->coop_psy_name);
    me->dev_initialized = false;
    return 0;
}

/**
 * max77819_get_battery_health - to get the battery health status
 *
 * Returns battery health status
 */
int max77819_get_battery_health(void)
{
	int  temp,vnow;
	if(g_charger == NULL)
	{
        log_err("g_charger init err,null\n");
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	temp = fg_chip_get_property(g_charger,POWER_SUPPLY_PROP_TEMP);
	if (temp == -ENODEV || temp == -EINVAL) {
        log_err("fg get battery temp err [%d]\n",temp);
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}

	if (temp <= g_charger->min_temp) 
		return POWER_SUPPLY_HEALTH_COLD;
	if (temp > g_charger->max_temp)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	/* read the battery voltage */
	vnow = fg_chip_get_property(g_charger,POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (vnow == -ENODEV || vnow == -EINVAL) {
		log_err("Can't read voltage from FG\n");
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	vnow = vnow/1000;
	if (vnow > 4400)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	return POWER_SUPPLY_HEALTH_GOOD;
}
EXPORT_SYMBOL(max77819_get_battery_health);
static int max77819_charger_reinit_dev (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;
    unsigned long irq_flags;
    int rc;
    u8 irq1_current, irq2_current, val;

    val  = 0;
    val |= CHGINT1_DC_UVP;
    val |= CHGINT1_CHG;
    rc = max77819_write(me->io, CHGINTM1, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTM1 write error [%d]\n", rc);
        goto out;
    }

    val  = 0;

    rc = max77819_write(me->io, CHGINTMSK2, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTMSK2 write error [%d]\n", rc);
        goto out;
    }

    irq1_current = max77819_charger_read_irq_status(me, CHGINT1);
    irq2_current = max77819_charger_read_irq_status(me, CHGINT2);

    spin_lock_irqsave(&me->irq_lock, irq_flags);
    me->irq1_saved |= irq1_current;
    me->irq2_saved |= irq2_current;
    spin_unlock_irqrestore(&me->irq_lock, irq_flags);

    log_dbg("CHGINT1 CURR %02Xh SAVED %02Xh\n", irq1_current, me->irq1_saved);
    log_dbg("CHGINT2 CURR %02Xh SAVED %02Xh\n", irq2_current, me->irq2_saved);

    /* charger enable */
    rc = max77819_charger_set_enable(me, me->dev_enabled);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* DCILMT enable */
    rc = max77819_charger_write_config(me, DCILIM_EN, true);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

	//set suitlable current
	max77819_set_appropriate_cc(me);
    /* topoff timer */
    val = pdata->topoff_timer <=  0 ? 0x00 :
          pdata->topoff_timer <= 60 ?
            (int)DIV_ROUND_UP(pdata->topoff_timer, 10) : 0x07;
    rc = max77819_charger_write_config(me, TOPOFFTIME, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* topoff current */
    val = pdata->topoff_current <  50000 ? 0x00 :
          pdata->topoff_current < 400000 ?
            (int)DIV_ROUND_UP(pdata->topoff_current - 50000, 50000) : 0x07;
    rc = max77819_charger_write_config(me, ITOPOFF, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge restart threshold */
    val = (pdata->charge_restart_threshold > 150000);
    rc = max77819_charger_write_config(me, CHGRSTRT, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }


	//set suiltable voltage
	max77819_set_appropriate_cv(me);
    /* thermistor and jeita control */
    val = (pdata->enable_thermistor == false);
    rc  = max77819_charger_write_config(me, THM_DIS, val);
	rc |= max77819_charger_write_config(me, JEITA_EN, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* AICL control */
    val = (pdata->enable_aicl == false);
    rc = max77819_charger_write_config(me, DCMON_DIS, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    if (likely(pdata->enable_aicl)) {
        int uV;

        /* AICL detection voltage selection */

        uV = pdata->aicl_detection_voltage;
        val = uV < 3900000 ? 0x00 :
              uV < 4800000 ? (int)DIV_ROUND_UP(uV - 3900000, 100000) : 0x09;
        log_dbg("AICL detection voltage %uV (%Xh)\n", uV, val);

        rc = max77819_charger_write_config(me, AICL, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        /* AICL reset threshold */

        uV = (int)pdata->aicl_reset_threshold;
        val = (uV > 100000);
        log_dbg("AICL reset threshold %uV (%Xh)\n", uV, val);

        rc = max77819_charger_write_config(me, AICL_RESET, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
    }
	//disable precharge timer
    rc = max77819_read(me->io, USBCHGCTL, &val);
      if (unlikely(IS_ERR_VALUE(rc))) {
           log_err("max77819:read precharge timer err\n");
		   goto out;
        }else
		log_dbg("charger 0x40 before set= 0x%02x \n",val);
	rc = max77819_write(me->io,USBCHGCTL,val | 0x80);//disable charger timer
      if (unlikely(IS_ERR_VALUE(rc))) {
           printk("max77819:disable precharge timer err\n");
		   goto out;
        }else
		{
    	rc = max77819_read(me->io, USBCHGCTL, &val);
		log_dbg("charger 0x40 after set= 0x%02x \n",val);
		}
	//set fastcharge timer to 8h
    max77819_charger_write_config(me, FCHGTIME,0x05);
    rc = max77819_read(me->io, FCHGCRNT, &val);
	log_dbg("charger 0x38 after set= 0x%02x \n",val);
    log_dbg("device initialized\n");

out:
    return rc;
}
static int max77819_charger_init_dev (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;
    unsigned long irq_flags;
    int rc;
    u8 irq1_current, irq2_current, val;

    val  = 0;
    /*val |= CHGINT1_AICLOTG;*/
    /*val |= CHGINT1_TOPOFF;*/
//  val |= CHGINT1_OVP;
    val |= CHGINT1_DC_UVP;
    val |= CHGINT1_CHG;
//  val |= CHGINT1_BAT;
//  val |= CHGINT1_THM;

    rc = max77819_write(me->io, CHGINTM1, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTM1 write error [%d]\n", rc);
        goto out;
    }

    val  = 0;
//  val |= CHGINT2_DC_V;
//  val |= CHGINT2_CHG_WDT;
//  val |= CHGINT2_CHG_WDT_WRN;

    rc = max77819_write(me->io, CHGINTMSK2, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTMSK2 write error [%d]\n", rc);
        goto out;
    }

#if 0
    do {
        static u8 dump_reg[32];
        int i;

        rc = max77819_bulk_read(me->io, 0x30, dump_reg, 32);
        for (i = 0; i < 32; i++) {
            log_vdbg("BURST DUMP REG %02Xh VALUE %02Xh [%d]\n", i+0x30, dump_reg[i], rc);
        }

        for (i = 0; i < 32; i++) {
            val = 0;
            rc = max77819_read(me->io, i+0x30, &val);
            log_vdbg("SINGLE DUMP REG %02Xh VALUE %02Xh [%d]\n", i+0x30, val, rc);
        }

        val = 0;
        rc = max77819_read(me->io, 0x37, &val);
        log_vdbg("DUMP REG %02Xh VALUE %02Xh [%d]\n", 0x37, val, rc);
    } while (0);
#endif

    irq1_current = max77819_charger_read_irq_status(me, CHGINT1);
    irq2_current = max77819_charger_read_irq_status(me, CHGINT2);

    spin_lock_irqsave(&me->irq_lock, irq_flags);
    me->irq1_saved |= irq1_current;
    me->irq2_saved |= irq2_current;
    spin_unlock_irqrestore(&me->irq_lock, irq_flags);

    log_dbg("CHGINT1 CURR %02Xh SAVED %02Xh\n", irq1_current, me->irq1_saved);
    log_dbg("CHGINT2 CURR %02Xh SAVED %02Xh\n", irq2_current, me->irq2_saved);

    /* charger enable */
    rc = max77819_charger_set_enable(me, me->dev_enabled);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* DCILMT enable */
    rc = max77819_charger_write_config(me, DCILIM_EN, true);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge current 
    rc = max77819_charger_set_charge_current(me, me->current_limit_volatile,
        me->charge_current_volatile);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }
	*/
	//set suitlable current
	max77819_set_appropriate_cc(me);
    /* topoff timer */
    val = pdata->topoff_timer <=  0 ? 0x00 :
          pdata->topoff_timer <= 60 ?
            (int)DIV_ROUND_UP(pdata->topoff_timer, 10) : 0x07;
    rc = max77819_charger_write_config(me, TOPOFFTIME, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* topoff current */
    val = pdata->topoff_current <  50000 ? 0x00 :
          pdata->topoff_current < 400000 ?
            (int)DIV_ROUND_UP(pdata->topoff_current - 50000, 50000) : 0x07;
    rc = max77819_charger_write_config(me, ITOPOFF, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge restart threshold */
    val = (pdata->charge_restart_threshold > 150000);
    rc = max77819_charger_write_config(me, CHGRSTRT, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge termination voltage 
    val = pdata->charge_termination_voltage < 3700000 ? 0x00 :
        (int)DIV_ROUND_UP(pdata->charge_termination_voltage - 3700000, 50000)
        + 0x01;
    rc = max77819_charger_write_config(me, MBATREG, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }*/

	//set suiltable voltage
	max77819_set_appropriate_cv(me);
    /* thermistor and jeita control */
    val = (pdata->enable_thermistor == false);
    rc  = max77819_charger_write_config(me, THM_DIS, val);
	rc |= max77819_charger_write_config(me, JEITA_EN, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* AICL control */
    val = (pdata->enable_aicl == false);
    rc = max77819_charger_write_config(me, DCMON_DIS, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    if (likely(pdata->enable_aicl)) {
        int uV;

        /* AICL detection voltage selection */

        uV = pdata->aicl_detection_voltage;
        val = uV < 3900000 ? 0x00 :
              uV < 4800000 ? (int)DIV_ROUND_UP(uV - 3900000, 100000) : 0x09;
        log_dbg("AICL detection voltage %uV (%Xh)\n", uV, val);

        rc = max77819_charger_write_config(me, AICL, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        /* AICL reset threshold */

        uV = (int)pdata->aicl_reset_threshold;
        val = (uV > 100000);
        log_dbg("AICL reset threshold %uV (%Xh)\n", uV, val);

        rc = max77819_charger_write_config(me, AICL_RESET, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
    }
	//disable precharge timer
    rc = max77819_read(me->io, USBCHGCTL, &val);
      if (unlikely(IS_ERR_VALUE(rc))) {
           log_err("max77819:read precharge timer err\n");
		   goto out;
        }else
		log_dbg("charger 0x40 before set= 0x%02x \n",val);
	rc = max77819_write(me->io,USBCHGCTL,val | 0x80);//disable charger timer
      if (unlikely(IS_ERR_VALUE(rc))) {
           printk("max77819:disable precharge timer err\n");
		   goto out;
        }else
		{
    	rc = max77819_read(me->io, USBCHGCTL, &val);
		log_dbg("charger 0x40 after set= 0x%02x \n",val);
		}
	//set fastcharge timer to 8h
    max77819_charger_write_config(me, FCHGTIME,0x05);
    rc = max77819_read(me->io, FCHGCRNT, &val);
	log_dbg("charger 0x38 after set= 0x%02x \n",val);
       /*thm handle*/
    max77819_charger_thm_control(me);
    me->dev_initialized = true;
    log_dbg("device initialized\n");

out:
    return rc;
}

#define max77819_charger_psy_setprop(_me, _psy, _psp, _val) \
        ({\
            struct power_supply *__psy = _me->_psy;\
            union power_supply_propval __propval = { .intval = _val };\
            int __rc = -ENXIO;\
            if (likely(__psy && __psy->set_property)) {\
                __rc = __psy->set_property(__psy, POWER_SUPPLY_PROP_##_psp,\
                    &__propval);\
            }\
            __rc;\
        })

static void max77819_charger_psy_init (struct max77819_charger *me)
{
    if (unlikely(!me->psy_this)) {
        me->psy_this = &me->psy;
    }

    if (unlikely(!me->psy_ext && me->pdata->ext_psy_name)) {
        me->psy_ext = power_supply_get_by_name(me->pdata->ext_psy_name);
        if (likely(me->psy_ext)) {
            log_dbg("psy %s found\n", me->pdata->ext_psy_name);
            /*max77819_charger_psy_setprop(me, psy_ext, PRESENT, false);*/
        }
    }

    if (unlikely(!me->psy_coop && me->pdata->coop_psy_name)) {
        me->psy_coop = power_supply_get_by_name(me->pdata->coop_psy_name);
        if (likely(me->psy_coop)) {
            log_dbg("psy %s found\n", me->pdata->coop_psy_name);
        }
    }
	me->psy_fg = power_supply_get_by_name("battery");
}

static void max77819_charger_psy_changed (struct max77819_charger *me)
{
    max77819_charger_psy_init(me);

    if (likely(me->psy_this)) {
        power_supply_changed(me->psy_this);
    }

    if (likely(me->psy_ext)) {
        power_supply_changed(me->psy_ext);
    }

    if (likely(me->psy_coop)) {
        power_supply_changed(me->psy_coop);
    }
}

struct max77819_charger_status_map {
    int health, status, charge_type;
};

static struct max77819_charger_status_map max77819_charger_status_map[] = {
    #define STATUS_MAP(_chg_dtls, _health, _status, _charge_type) \
            [CHG_DTLS_##_chg_dtls] = {\
                .health = POWER_SUPPLY_HEALTH_##_health,\
                .status = POWER_SUPPLY_STATUS_##_status,\
                .charge_type = POWER_SUPPLY_CHARGE_TYPE_##_charge_type,\
            }
    //                           health               status        charge_type
    STATUS_MAP(DEAD_BATTERY,     DEAD,                NOT_CHARGING, NONE),
    STATUS_MAP(PRECHARGE,        UNKNOWN,             CHARGING,     TRICKLE),
    STATUS_MAP(FASTCHARGE_CC,    UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(FASTCHARGE_CV,    UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(TOPOFF,           UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(DONE,             UNKNOWN,             FULL,         NONE),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
    STATUS_MAP(TIMER_FAULT,      SAFETY_TIMER_EXPIRE, NOT_CHARGING, NONE),
#else /* LINUX_VERSION_CODE ... */
    STATUS_MAP(TIMER_FAULT,      UNKNOWN,             NOT_CHARGING, NONE),
#endif /* LINUX_VERSION_CODE ... */
    STATUS_MAP(TEMP_SUSPEND,     UNKNOWN,             NOT_CHARGING, NONE),
    STATUS_MAP(OFF,              UNKNOWN,             NOT_CHARGING, NONE),
    STATUS_MAP(THM_LOOP,         UNKNOWN,             CHARGING,     NONE),
    STATUS_MAP(TEMP_SHUTDOWN,    OVERHEAT,            NOT_CHARGING, NONE),
    STATUS_MAP(BUCK,             UNKNOWN,             NOT_CHARGING, UNKNOWN),
    STATUS_MAP(OTG_OVER_CURRENT, UNKNOWN,             NOT_CHARGING, UNKNOWN),
    STATUS_MAP(USB_SUSPEND,      UNKNOWN,             NOT_CHARGING, NONE),
};

static int max77819_charger_update (struct max77819_charger *me)
{
    int rc;
    u8 dc_batt_dtls, chg_dtls;
    u8 batdet, bat, dcuvp, dcovp, dci, aicl;
    u8 chg, topoff, thm;

    me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
    me->status      = POWER_SUPPLY_STATUS_UNKNOWN;
    me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

    rc = max77819_read(me->io, DC_BATT_DTLS, &dc_batt_dtls);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("DC_BATT_DTLS read error [%d]\n", rc);
        goto out;
    }

    rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHG_DTLS read error [%d]\n", rc);
        goto out;
    }

    log_vdbg("DC_BATT_DTLS %Xh CHG_DTLS %Xh\n", dc_batt_dtls, chg_dtls);

    batdet = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_BATDET_DTLS);
    log_vdbg("*** BATDET %s\n", max77819_charger_batdet_details[batdet]);

    bat = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_BAT_DTLS);
    log_vdbg("*** BAT    %s\n", max77819_charger_bat_details[bat]);

    dcuvp = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_UVP);
    log_vdbg("*** DC_UVP %s\n", max77819_charger_dcuvp_details[dcuvp]);

    dcovp = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_OVP);
    log_vdbg("*** DC_OVP %s\n", max77819_charger_dcovp_details[dcovp]);

    dci = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_I);
    log_vdbg("*** DC_I   %s\n", max77819_charger_dci_details[dci]);

    aicl = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_AICL);
    log_vdbg("*** AICL   %s\n", max77819_charger_aicl_details[aicl]);

    chg = BITS_GET(chg_dtls, CHG_DTLS_CHG_DTLS);
    log_vdbg("*** CHG    %s\n", max77819_charger_chg_details[chg]);

    topoff = BITS_GET(chg_dtls, CHG_DTLS_TOPOFF_DTLS);
    log_vdbg("*** TOPOFF %s\n", max77819_charger_topoff_details[topoff]);

    thm = BITS_GET(chg_dtls, CHG_DTLS_THM_DTLS);
    log_vdbg("*** THM    %s\n", max77819_charger_thm_details[thm]);

    me->present = (dcuvp == DC_UVP_VALID);
    if (unlikely(!me->present)) {
        /* no charger present */
        me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
        me->status      = POWER_SUPPLY_STATUS_DISCHARGING;
        me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        goto out;
    }

    if (unlikely(dcovp != DC_OVP_VALID)) {
        me->status      = POWER_SUPPLY_STATUS_NOT_CHARGING;
        me->health      = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        me->charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
        goto out;
    }

    me->health      = max77819_charger_status_map[chg].health;
    me->status      = max77819_charger_status_map[chg].status;
    me->charge_type = max77819_charger_status_map[chg].charge_type;

    if (likely(me->health != POWER_SUPPLY_HEALTH_UNKNOWN)) {
        goto out;
    }

    /* override health by THM_DTLS */
    switch (thm) {
    case THM_DTLS_LOW_TEMP_SUSPEND:
        me->health = POWER_SUPPLY_HEALTH_COLD;
        break;
    case THM_DTLS_HIGH_TEMP_SUSPEND:
        me->health = POWER_SUPPLY_HEALTH_OVERHEAT;
        break;
    default:
        me->health = POWER_SUPPLY_HEALTH_GOOD;
        break;
    }

out:
    log_vdbg("PRESENT %d HEALTH %d STATUS %d CHARGE_TYPE %d\n",
        me->present, me->health, me->status, me->charge_type);
    return rc;
}

#define max77819_charger_resume_log_work(_me) \
        do {\
            if (likely(log_worker)) {\
                if (likely(!delayed_work_pending(&(_me)->log_work))) {\
                    schedule_delayed_work(&(_me)->log_work, LOG_WORK_INTERVAL);\
                }\
            }\
        } while (0)

#define max77819_charger_suspend_log_work(_me) \
        cancel_delayed_work_sync(&(_me)->log_work)
extern bool bl_for_charge;
bool bat_current_1a = false;
static void charger_temp_monitor_func(struct max77819_charger *chip) 
{
	bool bat_cool = false;
	bool bat_warm = false;
	bool dec_cur_bat_cool = false;
	int rc;
    int val = 0;

	if(chip->batt_temp > (chip->max_temp + 5) || chip->batt_temp < (chip->min_temp - 5))
	{
	   //disable charging
	   if(chip->dev_enabled == true)
	   {
		   printk("Battery temperature is abnormal,stop charging\n");
		   chip->dev_enabled = false;
		   max77819_charger_set_enable(chip, chip->dev_enabled);
	   }
   }else if((chip->min_temp + 5) < chip->batt_temp && chip->batt_temp < chip->max_temp)
   {
	   if(chip->dev_enabled == false)
	   {
		   printk("Battery temperature is normal again,start charging\n");
		   chip->dev_enabled = true;
		   //enable charging
		   max77819_charger_set_enable(chip, chip->dev_enabled);
	   }
   }
   if(chip->batt_temp  < chip->cool_temp){
	   bat_cool = true;
	   bat_warm = false;
	   if((chip->batt_volt/1000) > 4450)//in zoom, we don't need divide the charging time into tow stage during cool temp,so make a invalid value
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
		   max77819_set_appropriate_cc(chip);
		   max77819_set_appropriate_cv(chip);
   }
   //temp >40,set current to 1A.
#if 1

   max77819_charger_get_chgcc(chip, &val);
   if(bl_for_charge)
   {
		if(chip->batt_temp > 400 && (val > 950000))
		{
			printk("battery temp is over 40,set current to 0.95A\n");
			bat_current_1a = true;
    		rc = max77819_charger_set_charge_current(chip, chip->current_limit_volatile,950000);
    		if (unlikely(IS_ERR_VALUE(rc))) {
        	pr_err("set cc error %d\n",rc);
    		}
   		}else if(chip->batt_temp < 380 && bat_current_1a)
   		{
			printk("battery temp is below 38,set current to 1.5A\n");
			bat_current_1a = false;
    		rc = max77819_charger_set_charge_current(chip, chip->current_limit_volatile,chip->charge_current_volatile);
    		if (unlikely(IS_ERR_VALUE(rc))) {
        	pr_err("set cc error %d\n",rc);
    		}
			schedule_delayed_work(&chip->aicl_work,AICL_WORK_DELAY);
   		}
   }else
	{
	//	printk("backlight is 0,bat_current_1a is %d\n",bat_current_1a);
		if(bat_current_1a)
		{
			printk("before current is 1a,set current to 1.5A\n");
			bat_current_1a = false;
    		rc = max77819_charger_set_charge_current(chip, chip->current_limit_volatile,chip->charge_current_volatile);
    		if (unlikely(IS_ERR_VALUE(rc))) {
        	pr_err("set cc error %d\n",rc);
    		}

		}
	}
#endif
#if 0
   if(chip->bat_is_warm)
   {

	   if(!chip->warm_chg_done)
	   {
		   if(batt_voltage > chip->cfg_warm_bat_mv)
		   {
			   warm_chg_done_count++;
		   }else
			   warm_chg_done_count = 0;
		   if(warm_chg_done_count > 3)
		   {
			   chip->warm_chg_done = true;
			   printk("battery is warm,set warm_chg_done %d\n",warm_chg_done_count);
		   }
	   }
   }else
   {
	   chip->warm_chg_done = false;
   }
#endif
}
/*max77816 detect the vbus current to terminate the charger,so sometimes 
 * when system load is larger than the terminate current, charger can't 
 * be terminated.we terminate the charger here when battery is full*/
static int rty_chg_c = 0;
static int warm_chg_c = 0;
extern int is_charger_plug_in(void);
static void max77819_charger_done_check(struct max77819_charger *me)
{
	if((me->batt_soc == 100) && is_charger_plug_in())
		rty_chg_c++;	
	else
		rty_chg_c = 0;
	if(rty_chg_c > 60)
	{
		printk("soc keep in 100 longger than 30 min, disable charger\n");
		me->chg_done = true;
		max77819_set_charging_enable(0);
	}else if(me->chg_done){
		me->chg_done = false;
		if(me->dev_enabled)
		{
		printk("enable charger again\n");
		max77819_set_charging_enable(1);
		}
	}
	if((me->bat_is_warm && (me->batt_volt/1000) > 4050) && is_charger_plug_in())
		warm_chg_c++;
	else
		warm_chg_c = 0;
	if(warm_chg_c > 60)
	{
		printk("temp is warm and vreg>4100 for 30 min\n");
		charger_is_done = 1;
	}else if(charger_is_done){
		charger_is_done = 0;
		printk("temp is normall or vreg<4100 again\n");
	}
}
/***********************adjust chgcc dynamically according to the AICL flag Jelphi****************/
static void max77819_charger_aicl_dynamic_control(struct max77819_charger *me)
{
	  int rc;
    int val = 0;
    u8  chg_sts;
    rc = max77819_read(me->io, CHG_STAT, &chg_sts); //read 0x32 register
    if(chg_sts & 0x80){ //judge if charger works in AICL mode
    	max77819_charger_get_chgcc(me, &val);
    	printk("max77819:current chgcc = %d,set current to =%d\n",val,val - 50000);
    	if(val > 550000){ //don't adjust curent anymore if it is below 500mA
    		val = val - 50000;
    		max77819_charger_set_charge_current(me,val,val);//set dclim and chgcc to same value 
			schedule_delayed_work(&me->aicl_work,AICL_WORK_DELAY);
			aicl_count = 0;
    	}
    }else
	{
		aicl_count++;
    	max77819_charger_get_chgcc(me, &val);
	//	printk("max77819:not works in aicl mode for %d time\n",aicl_count);
		if(aicl_count <= 10)
			schedule_delayed_work(&me->aicl_work,AICL_WORK_DELAY);
	}

}
static void max77819_aicl_monitor_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, aicl_work.work);
	max77819_charger_aicl_dynamic_control(me);//check AICL jelphi
}
static void max77819_aicl_voltage_set(struct max77819_charger *me,int aicl_val)
{
	static int uV,val,rc;
	if(me->pdata->aicl_detection_voltage != aicl_val)
	{
		me->pdata->aicl_detection_voltage = aicl_val;
		uV = me->pdata->aicl_detection_voltage;
		val = uV < 3900000 ? 0x00 :
		 uV < 4800000 ? (int)DIV_ROUND_UP(uV - 3900000, 100000) : 0x09;
		rc = max77819_charger_write_config(me, AICL, val);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("%s,set aicl voltage err\n",__func__);
		}else
		{
			printk("%s:AICL detection voltage %uV (%Xh)\n", __func__,uV, me->pdata->aicl_detection_voltage);
		}
	}
}
static void max77819_charger_dtls_check(struct max77819_charger *me)
{
	int rc;
    u8  chg_dtls,chg_status,chg_sts;
    rc = max77819_read(me->io, CHG_STAT, &chg_sts); //read 0x32 register
    if((chg_sts & 0x80) && !aicl_update){ //judge if charger works in AICL mode
		aicl_update = true;
		schedule_delayed_work(&me->aicl_work,AICL_WORK_DELAY);
	}
	rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
	if (unlikely(IS_ERR_VALUE(rc))) {
		printk("max77819:CHG_DTLS read error [%d]\n", rc);
	}
    chg_status = BITS_GET(chg_dtls, CHG_DTLS_CHG_DTLS);
	/*
	if(chg_status >4)
	{
		printk("%s,chg_status is %d,reinit dev\n",__func__,chg_status);
		if (likely(max77819_charger_present_input(me))) {
			rc = max77819_charger_init_dev(me);
			if (unlikely(IS_ERR_VALUE(rc))) {
				printk("%s,init_dev_err\n",__func__);
			}
		}
	}else */
	if(chg_status == 4)
	{
		if(me->batt_temp > me->min_temp && me->batt_temp < me->warm_temp && me->batt_soc != 100)
		{
			if(me->bat_is_warm)
				me->bat_is_warm = false;
			printk("%s,charge terminate unexpectly,reinit dev\n",__func__);
			if (likely(max77819_charger_present_input(me))) {
				rc = max77819_charger_reinit_dev(me);
				if (unlikely(IS_ERR_VALUE(rc))) {
					printk("%s,reinit_dev_err\n",__func__);
				}
			}
		}
	}
    max77819_charger_thm_control(me);
}
static void max77819_charger_monitor_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, monitor_work.work);
	static int vindpm = INPUT_SRC_VOLT_LMT_450;
    u8 dc_batt_dtls, chg_dtls,fchgcrnt;
	static u8 log_count = 1;
	int rc;
	me->batt_volt = fg_chip_get_property(me,POWER_SUPPLY_PROP_VOLTAGE_NOW);
	me->batt_curr = fg_chip_get_property(me,POWER_SUPPLY_PROP_CURRENT_NOW);
	me->batt_temp = fg_chip_get_property(me,POWER_SUPPLY_PROP_TEMP);
	me->batt_soc = fg_chip_get_property(me,POWER_SUPPLY_PROP_CAPACITY);
	me->batt_status = fg_chip_get_property(me,POWER_SUPPLY_PROP_STATUS);
	max77819_charger_psy_changed(me);
	max77819_charger_done_check(me);
	//change aicl detection voltage dynamicly
	if(me->batt_status == POWER_SUPPLY_STATUS_CHARGING)
	{
#if 0
		if (me->batt_volt <= INPUT_SRC_LOW_VBAT_LIMIT)
			vindpm = INPUT_SRC_VOLT_LMT_395;
		if (me->batt_volt > INPUT_SRC_LOW_VBAT_LIMIT &&
			me->batt_volt < INPUT_SRC_MIDL_VBAT_LIMIT)
			vindpm = INPUT_SRC_VOLT_LMT_415;
		else if (me->batt_volt > INPUT_SRC_MIDL_VBAT_LIMIT &&
			me->batt_volt < INPUT_SRC_MIDH_VBAT_LIMIT)
			vindpm = INPUT_SRC_VOLT_LMT_435;
#endif
		if(me->batt_volt < INPUT_SRC_MIDH_VBAT_LIMIT)
			vindpm = INPUT_SRC_VOLT_LMT_440;
		else if (me->batt_volt > INPUT_SRC_MIDH_VBAT_LIMIT &&
			me->batt_volt < INPUT_SRC_HIGH_VBAT_LIMIT)
			vindpm = INPUT_SRC_VOLT_LMT_450;
		max77819_aicl_voltage_set(me,vindpm);
	}
	if(me->batt_status == POWER_SUPPLY_STATUS_FULL && me->batt_volt >= INPUT_SRC_HIGH_VBAT_LIMIT)
	{
		vindpm = INPUT_SRC_VOLT_LMT_470;
		max77819_aicl_voltage_set(me,vindpm);
	}
//	printk("battery voltage is %d,capacity is %d,current now is %d,temp is %d,status is %d\n",me->batt_volt,me->batt_soc,me->batt_curr,me->batt_temp,me->batt_status);
	charger_temp_monitor_func(me);
	if((log_count++)%4 == 0)
	{
		rc = max77819_read(me->io, DC_BATT_DTLS, &dc_batt_dtls);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("max77819:DC_BATT_DTLS read error [%d]\n", rc);
		}

		rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("max77819:CHG_DTLS read error [%d]\n", rc);
		}
		rc = max77819_read(me->io, FCHGCRNT, &fchgcrnt);
		printk("max77819:DC_BATT_DTLS %Xh CHG_DTLS %Xh,FCHGCRNT %X\n", dc_batt_dtls, chg_dtls,fchgcrnt);
		log_count = 1;
	}
	if(is_charger_plug_in())
		max77819_charger_dtls_check(me);
	schedule_delayed_work(&me->monitor_work, MONITOR_WORK_DELAY);
}
static void max77819_charger_log_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, log_work.work);
    int val = 0;
    u8 regval = 0;

    __lock(me);

    max77819_charger_update(me);

    max77819_charger_get_enable(me, &val);
    log_info("charger = %s\n", val ? "on" : "off");

    max77819_charger_get_dcilmt(me, &val);

    if (me->current_limit_volatile == MAX77819_CHARGER_CURRENT_UNLIMIT) {
        log_info("current limit expected = unlimited\n");
    } else {
        log_info("current limit expected = %duA\n", me->current_limit_volatile);
    }

    if (val == MAX77819_CHARGER_CURRENT_UNLIMIT) {
        log_info("current limit set      = unlimited\n");
    } else {
        log_info("current limit set      = %duA\n", val);
    }

    max77819_charger_get_chgcc(me, &val);

    log_info("charge current expected = %duA\n", me->charge_current_volatile);
    log_info("charge current set      = %duA\n", val);

    max77819_read(me->io, TOPOFF, &regval);
    log_info("TOPOFF register %02Xh\n", regval);

    max77819_read(me->io, BATREG, &regval);
    log_info("BATREG register %02Xh\n", regval);

    max77819_read(me->io, 0x34, &regval);
    log_info("charging status[0x34]: %02Xh\n", regval);

    max77819_read(me->io, 0x3e, &regval);
    log_info("charger control2 %02Xh\n", regval);

    max77819_charger_resume_log_work(me);

    __unlock(me);
}

static void max77819_charger_irq_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, irq_work.work);
    unsigned long irq_flags;
    u8 irq1_current, irq2_current;
    bool present_input;
	int ilimt_uA = 0;

    __lock(me);

    irq1_current = max77819_charger_read_irq_status(me, CHGINT1);
    irq2_current = max77819_charger_read_irq_status(me, CHGINT2);

    spin_lock_irqsave(&me->irq_lock, irq_flags);
    irq1_current |= me->irq1_saved;
    me->irq1_saved = 0;
    irq2_current |= me->irq2_saved;
    me->irq2_saved = 0;
    spin_unlock_irqrestore(&me->irq_lock, irq_flags);

    if (irq1_current || irq2_current)
		log_dbg("<IRQ_WORK> CHGINT1 %02Xh CHGINT2 %02Xh\n", irq1_current,
        irq2_current);

/*
    if (unlikely(!irq1_current && !irq2_current)) {
        goto done;
    }
	*/
    max77819_charger_get_dcilmt(me, &ilimt_uA);

    present_input = max77819_charger_present_input(me);
    if ((present_input ^ me->dev_initialized) || ((me->current_limit_volatile != ilimt_uA)&& present_input)) {
        max77819_charger_psy_init(me);
        if (likely(present_input)) {
            max77819_charger_init_dev(me);
        } else {
            max77819_charger_exit_dev(me);
        }

        max77819_charger_psy_setprop(me, psy_coop, CHARGING_ENABLED,
            present_input);
        max77819_charger_psy_setprop(me, psy_ext, PRESENT, present_input);

  //      schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
		log_dbg("DC input %s\n", present_input ? "inserted" : "removed");
		printk("max77819:voltage is %d,vbus is %d,current is %d,temp is %d\n",fg_chip_get_property(me,POWER_SUPPLY_PROP_VOLTAGE_NOW),get_vbus(),fg_chip_get_property(me,POWER_SUPPLY_PROP_CURRENT_NOW),fg_chip_get_property(me,POWER_SUPPLY_PROP_TEMP));
        goto out;
    }

    /* notify psy changed */
    max77819_charger_psy_changed(me);

//done:
    if (unlikely(me->irq <= 0)) {
        if (likely(!delayed_work_pending(&me->irq_work))) {
            schedule_delayed_work(&me->irq_work, IRQ_WORK_INTERVAL);
        }
    }

out:
    __unlock(me);
    return;
}

static irqreturn_t max77819_charger_isr (int irq, void *data)
{
    struct max77819_charger *me = data;
    u8 irq_current;

    irq_current = max77819_charger_read_irq_status(me, CHGINT1);
	log_dbg("<ISR> CHGINT1 CURR %02Xh SAVED %02Xh\n", irq_current, me->irq1_saved);
    me->irq1_saved |= irq_current;

    irq_current = max77819_charger_read_irq_status(me, CHGINT2);
	log_dbg("<ISR> CHGINT2 CURR %02Xh SAVED %02Xh\n", irq_current, me->irq2_saved);
    me->irq2_saved |= irq_current;

   // schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    return IRQ_HANDLED;
}

static int max77819_charger_get_property (struct power_supply *psy,
        enum power_supply_property psp, union power_supply_propval *val)
{
    struct max77819_charger *me =
        container_of(psy, struct max77819_charger, psy);
    int rc = 0;

    __lock(me);

    rc = max77819_charger_update(me);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = me->present;
        break;
/*
#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
    case POWER_SUPPLY_PROP_ONLINE:
#endif  !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        val->intval = me->dev_enabled;
        break;

    case POWER_SUPPLY_PROP_HEALTH:
        val->intval = me->health;
        break;

    case POWER_SUPPLY_PROP_STATUS:
        val->intval = me->status;
        break;

    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        val->intval = me->charge_type;
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        val->intval = me->charge_current_volatile;
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
        val->intval = me->current_limit_volatile;
        break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	   val->intval = limit_level;
   	   break;	   

    default:
        rc = -EINVAL;
        goto out;
    }

out:
    log_vdbg("<get_property> psp %d val %d [%d]\n", psp, val->intval, rc);
    __unlock(me);
    return rc;
}


static int max77819_charger_set_property (struct power_supply *psy,
        enum power_supply_property psp, const union power_supply_propval *val)
{
    struct max77819_charger *me =
        container_of(psy, struct max77819_charger, psy);
    int uA, rc = 0;

    __lock(me);

    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        rc = max77819_charger_set_enable(me, val->intval);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        me->dev_enabled = val->intval;

        /* apply charge current */
        rc = max77819_charger_set_charge_current(me, me->current_limit_volatile,
            me->charge_current_volatile);
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        uA = abs(val->intval);
        rc = max77819_charger_set_charge_current(me, me->current_limit_volatile,
            uA);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
        me->charge_current_volatile  = uA;
        me->charge_current_permanent =
            val->intval > 0 ? uA : me->charge_current_permanent;
        break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		limit_level =  abs(val->intval);
		if(limit_level == 0)
		{
			uA = 1500000;//back
		}
		else
		{
			uA = 900000; //0.3c
		}

		rc = max77819_charger_set_charge_current(me, me->current_limit_volatile, uA);
		if (unlikely(IS_ERR_VALUE(rc))) 
		{
			goto out;
		}

		me->charge_current_volatile  = uA;
		me->charge_current_permanent = uA;
		printk("%s,limit_level is :%d,charge_current_volatile is :%d,charge_current_permanent is :%d\n",__func__,limit_level,me->charge_current_volatile,me->charge_current_permanent);
		break;


    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
        uA = abs(val->intval);
        rc = max77819_charger_set_charge_current(me, uA,
            me->charge_current_volatile);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        me->current_limit_volatile  = uA;
        me->current_limit_permanent =
            val->intval > 0 ? uA : me->current_limit_permanent;
        break;

    default:
        rc = -EINVAL;
        goto out;
    }

out:
    log_vdbg("<set_property> psp %d val %d [%d]\n", psp, val->intval, rc);
    __unlock(me);
    return rc;
}

static int max77819_charger_property_is_writeable (struct power_supply *psy,
    enum power_supply_property psp)
{
    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
        return 1;

    default:
        break;
    }

    return -EINVAL;
}
void notify_max77819_charger_current(int cc_uA)
{
    struct power_supply *max77819_psy;
    struct max77819_charger *me ;
    max77819_psy = power_supply_get_by_name("ac");
	max77819_cc = cc_uA;
	if(likely(max77819_psy != NULL))
	{
		me = container_of(max77819_psy, struct max77819_charger, psy);
		wake_lock_timeout(&me->chg_update_wk,HZ);
		if(cc_uA > 0)
		{
			/*wake lock during charing*/
			if (!wake_lock_active(&me->chg_wake_lock))
				wake_lock(&me->chg_wake_lock);
		}else{
			if (wake_lock_active(&me->chg_wake_lock))
				wake_unlock(&me->chg_wake_lock);
		}
		if(me->current_limit_volatile != cc_uA)
		{
			if(!in_interrupt())
				__lock(me);
			me->current_limit_volatile = cc_uA;
			if(!in_interrupt())
				__unlock(me);
			schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
		}
	if(cc_uA == 1500000)//charger type is not usb
		schedule_delayed_work(&me->aicl_work,AICL_WORK_DELAY);
	if(cc_uA == 0)//charger removed
	{
		cancel_delayed_work_sync(&me->aicl_work);
		aicl_count = 0;
		aicl_update = false;
		max77819_aicl_voltage_set(me,INPUT_SRC_VOLT_LMT_DEF);
	}
	}
}
static void max77819_charger_external_power_changed (struct power_supply *psy)
{
    struct max77819_charger *me =
        container_of(psy, struct max77819_charger, psy);
    struct power_supply *supplicant;
    int i;

    __lock(me);

    for (i = 0; i < me->pdata->num_supplicants; i++) {
        supplicant = power_supply_get_by_name(me->pdata->supplied_to[i]);
        if (likely(supplicant)) {
            power_supply_changed(supplicant);
        }
    }

    __unlock(me);
}

static enum power_supply_property max77819_charger_psy_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CHARGE_TYPE,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
/*#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
    POWER_SUPPLY_PROP_ONLINE,
#endif  !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,     /* charging current */
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, /* input current limit */
    POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
};

static void *max77819_charger_get_platdata (struct max77819_charger *me)
{
#ifdef CONFIG_OF
    struct device *dev = me->dev;
    struct device_node *np = dev->of_node;
    struct max77819_charger_platform_data *pdata;
    size_t sz;
    int num_supplicants, i;

    num_supplicants = of_property_count_strings(np, "supplied_to");
    num_supplicants = max(0, num_supplicants);

    sz = sizeof(*pdata) + num_supplicants * sizeof(char*);
    pdata = devm_kzalloc(dev, sz, GFP_KERNEL);
    if (unlikely(!pdata)) {
        log_err("out of memory (%luB requested)\n", sz);
        pdata = ERR_PTR(-ENOMEM);
        goto out;
    }

    //pdata->irq = irq_of_parse_and_map(np, 0);
    //log_dbg("property:IRQ             %d\n", pdata->irq);

    me->thm_gpio = of_get_named_gpio(np, "max77819,thm_gpio", 0);

    pdata->psy_name = "ac";
    of_property_read_string(np, "psy_name", (char const **)&pdata->psy_name);
    log_dbg("property:PSY NAME        %s\n", pdata->psy_name);

    of_property_read_string(np, "ext_psy_name",
        (char const **)&pdata->ext_psy_name);
    log_dbg("property:EXT PSY         %s\n",
        pdata->ext_psy_name ? pdata->ext_psy_name : "null");

    if (unlikely(num_supplicants <= 0)) {
        pdata->supplied_to     = NULL;
        pdata->num_supplicants = 0;
        log_dbg("property:SUPPLICANTS     null\n");
    } else {
        pdata->num_supplicants = (size_t)num_supplicants;
        log_dbg("property:SUPPLICANTS     %d\n", num_supplicants);
        pdata->supplied_to = (char**)(pdata + 1);
        for (i = 0; i < num_supplicants; i++) {
            of_property_read_string_index(np, "supplied_to", i,
                (char const **)&pdata->supplied_to[i]);
            log_dbg("property:SUPPLICANTS     %s\n", pdata->supplied_to[i]);
        }
    }

    pdata->fast_charge_current = 500000;
    of_property_read_u32(np, "fast_charge_current",
        &pdata->fast_charge_current);
    log_dbg("property:CHGCC           %uuA\n", pdata->fast_charge_current);

    pdata->charge_termination_voltage = 4200000;
    of_property_read_u32(np, "charge_termination_voltage",
        &pdata->charge_termination_voltage);
    log_dbg("property:MBATREG         %uuV\n",
        pdata->charge_termination_voltage);

    pdata->topoff_timer = 0;
    of_property_read_u32(np, "topoff_timer", &pdata->topoff_timer);
    log_dbg("property:TOPOFFTIME      %umin\n", pdata->topoff_timer);

    pdata->topoff_current = 200000;
    of_property_read_u32(np, "topoff_current", &pdata->topoff_current);
    log_dbg("property:ITOPOFF         %uuA\n", pdata->topoff_current);

    pdata->charge_restart_threshold = 150000;
    of_property_read_u32(np, "charge_restart_threshold",
        &pdata->charge_restart_threshold);
    log_dbg("property:CHGRSTRT        %uuV\n", pdata->charge_restart_threshold);

    pdata->enable_coop = of_property_read_bool(np, "enable_coop");
    log_dbg("property:COOP CHG        %s\n",
        pdata->enable_coop ? "enabled" : "disabled");

    if (likely(pdata->enable_coop)) {
        of_property_read_string(np, "coop_psy_name",
            (char const **)&pdata->coop_psy_name);
        log_dbg("property:COOP CHG        %s\n",
            pdata->coop_psy_name ? pdata->coop_psy_name : "null");
    }

    pdata->enable_thermistor = of_property_read_bool(np, "enable_thermistor");
    log_dbg("property:THERMISTOR      %s\n",
        pdata->enable_thermistor ? "enabled" : "disabled");

    pdata->enable_aicl = of_property_read_bool(np, "enable_aicl");
    log_dbg("property:AICL            %s\n",
        pdata->enable_aicl ? "enabled" : "disabled");

    pdata->aicl_detection_voltage = INPUT_SRC_VOLT_LMT_DEF;
    of_property_read_u32(np, "aicl_detection_voltage",
        &pdata->aicl_detection_voltage);
    log_dbg("property:AICL DETECTION  %uuV\n", pdata->aicl_detection_voltage);

    pdata->aicl_reset_threshold = 100000;
    of_property_read_u32(np, "aicl_reset_threshold",
        &pdata->aicl_reset_threshold);
    log_dbg("property:AICL RESET      %uuV\n", pdata->aicl_reset_threshold);

out:
    return pdata;
#else /* CONFIG_OF */
    return dev_get_platdata(me->dev) ?
        dev_get_platdata(me->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

static __always_inline
void max77819_charger_destroy (struct max77819_charger *me)
{
    struct device *dev = me->dev;

    cancel_delayed_work_sync(&me->log_work);

    if (likely(me->irq > 0)) {
        devm_free_irq(dev, me->irq, me);
    }

    cancel_delayed_work_sync(&me->irq_work);

    if (likely(me->attr_grp)) {
        sysfs_remove_group(me->kobj, me->attr_grp);
    }

    if (likely(me->psy_this)) {
        power_supply_unregister(me->psy_this);
    }

#ifdef CONFIG_OF
    if (likely(me->pdata)) {
        devm_kfree(dev, me->pdata);
    }
#endif /* CONFIG_OF */

	wake_lock_destroy(&me->chg_wake_lock);
	wake_lock_destroy(&me->chg_update_wk);
    mutex_destroy(&me->lock);
//  spin_lock_destroy(&me->irq_lock);

    devm_kfree(dev, me);
}

#ifdef CONFIG_OF
static struct of_device_id max77819_charger_of_ids[] = {
    { .compatible = "maxim,"MAX77819_CHARGER_NAME },
    { },
};
MODULE_DEVICE_TABLE(of, max77819_charger_of_ids);
#endif /* CONFIG_OF */

static int max77819_charger_probe (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct max77819_dev *chip = dev_get_drvdata(dev->parent);
    struct max77819_charger *me;
    int rc = 0;

    log_dbg("attached\n");

    me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
    if (unlikely(!me)) {
        log_err("out of memory (%luB requested)\n", sizeof(*me));
        return -ENOMEM;
    }

    dev_set_drvdata(dev, me);

    spin_lock_init(&me->irq_lock);
    mutex_init(&me->lock);
    me->io   = max77819_get_io(chip);
    me->dev  = dev;
    me->kobj = &dev->kobj;
    me->irq  = -1;

    INIT_DELAYED_WORK(&me->irq_work, max77819_charger_irq_work);
    INIT_DELAYED_WORK(&me->log_work, max77819_charger_log_work);
    INIT_DELAYED_WORK(&me->monitor_work, max77819_charger_monitor_work);
    INIT_DELAYED_WORK(&me->aicl_work, max77819_aicl_monitor_work);

	wake_lock_init(&me->chg_wake_lock, WAKE_LOCK_SUSPEND,"charger_wakelock");
	wake_lock_init(&me->chg_update_wk, WAKE_LOCK_SUSPEND,"charger_update_wakelock");
    me->pdata = max77819_charger_get_platdata(me);
    if (unlikely(IS_ERR(me->pdata))) {
        rc = PTR_ERR(me->pdata);
        me->pdata = NULL;

        log_err("failed to get platform data [%d]\n", rc);
        goto abort;
    }

    /* disable all IRQ */
    max77819_write(me->io, CHGINTM1,   0xFF);
    max77819_write(me->io, CHGINTMSK2, 0xFF);

    me->dev_enabled               =
        (!me->pdata->enable_coop || me->pdata->coop_psy_name);
    me->current_limit_permanent   = MAX77819_CHARGER_CURRENT_UNLIMIT;
    me->charge_current_permanent  = me->pdata->fast_charge_current;
    me->current_limit_volatile    = me->current_limit_permanent;
    me->charge_current_volatile   = me->charge_current_permanent;

    if (likely(max77819_charger_present_input(me))) {
        rc = max77819_charger_init_dev(me);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto abort;
        }
    }

    me->psy.name                   = me->pdata->psy_name;
    me->psy.type                   = POWER_SUPPLY_TYPE_MAINS;
    me->psy.supplied_to            = me->pdata->supplied_to;
    me->psy.num_supplicants        = me->pdata->num_supplicants;
    me->psy.properties             = max77819_charger_psy_props;
    me->psy.num_properties         = ARRAY_SIZE(max77819_charger_psy_props);
    me->psy.get_property           = max77819_charger_get_property;
    me->psy.set_property           = max77819_charger_set_property;
    me->psy.property_is_writeable  = max77819_charger_property_is_writeable;
    me->psy.external_power_changed = max77819_charger_external_power_changed;

	//jeita setting
	me->vbat_warm = 4100*1000;
	me->vbat_cool = 4350*1000;
	me->ibat_warm = 1500*1000;
	me->ibat_cool = 550*1000;
	me->min_temp = 0;
	me->max_temp = 500;
	me->warm_temp = 450;
	me->cool_temp = 150;
	me->chg_done = false;

    max77819_charger_psy_init(me);
    rc = power_supply_register(dev, me->psy_this);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to register power_supply class [%d]\n", rc);
        me->psy_this = NULL;
        goto abort;
    }

    me->irq = max77819_map_irq(chip, MAX77819_IRQ_CHGR);

    log_dbg("Charger irq is %d\n", me->irq);
	
    if (unlikely(me->irq <= 0)) {
        log_warn("interrupt disabled\n");
        schedule_delayed_work(&me->irq_work, IRQ_WORK_INTERVAL);
    } else {
        rc = request_threaded_irq(me->irq, NULL, max77819_charger_isr,
            		IRQF_TRIGGER_LOW|IRQF_ONESHOT, DRIVER_NAME, me);
        if (unlikely(IS_ERR_VALUE(rc))) {
            log_err("failed to request IRQ %d [%d]\n", me->irq, rc);
            me->irq = -1;
            goto abort;
        }
    }

    /* enable IRQ we want */
    max77819_write(me->io, CHGINTM1, (u8)~CHGINT1_DC_UVP);

    /* thm gpio init --Jelphi*/
    if ((!gpio_is_valid(me->thm_gpio))) {
			log_dbg("failed to get a valid thm gpio!!!\n");
	} else {
		gpio_request(me->thm_gpio, "thermal_gpio");
		gpio_direction_output(me->thm_gpio, 0);
		log_dbg("thm-gpio :%u assigned\n", me->thm_gpio);
		gpio_set_value_cansleep(me->thm_gpio,0);
	}

    log_info("driver "DRIVER_VERSION" installed\n");
	
	max77819_read(me->io,PMIC_ID, &me->pmic_id);
	pmic_id = me->pmic_id;
    schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    schedule_delayed_work(&me->monitor_work, 5000);
    max77819_charger_resume_log_work(me);
	if(is_charger_plug_in() && (max77819_cc!= -255))
	{
		printk("max77819:request wake lock,set current:%d\n",max77819_cc);
		notify_max77819_charger_current(max77819_cc);
	}
	g_charger = me;
	if(max77819_otg_mode)
	max77819_charger_otg(max77819_otg_mode);
    return 0;

abort:
    dev_set_drvdata(dev, NULL);
    max77819_charger_destroy(me);
    return rc;
}

static int max77819_charger_remove (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct max77819_charger *me = dev_get_drvdata(dev);

    dev_set_drvdata(dev, NULL);
    max77819_charger_destroy(me);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77819_charger_suspend (struct device *dev)
{
    struct max77819_charger *me = dev_get_drvdata(dev);

    __lock(me);

    log_vdbg("suspending\n");

    max77819_charger_suspend_log_work(me);
    cancel_delayed_work_sync(&me->irq_work);
    cancel_delayed_work_sync(&me->monitor_work);

    __unlock(me);
    return 0;
}

static int max77819_charger_resume (struct device *dev)
{
    struct max77819_charger *me = dev_get_drvdata(dev);

    __lock(me);

    log_vdbg("resuming\n");

//    schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    max77819_charger_resume_log_work(me);
	schedule_delayed_work(&me->monitor_work, MONITOR_WORK_DELAY);
	schedule_delayed_work(&me->irq_work, MONITOR_WORK_DELAY);

    __unlock(me);
    return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max77819_charger_pm, max77819_charger_suspend,
    max77819_charger_resume);

static struct platform_driver max77819_charger_driver =
{
    .driver.name            = DRIVER_NAME,
    .driver.owner           = THIS_MODULE,
    .driver.pm              = &max77819_charger_pm,
#ifdef CONFIG_OF
    .driver.of_match_table  = max77819_charger_of_ids,
#endif /* CONFIG_OF */
    .probe                  = max77819_charger_probe,
    .remove                 = max77819_charger_remove,
};

static __init int max77819_charger_init (void)
{
    return platform_driver_register(&max77819_charger_driver);
}
late_initcall(max77819_charger_init);

static __exit void max77819_charger_exit (void)
{
    platform_driver_unregister(&max77819_charger_driver);
}
module_exit(max77819_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
