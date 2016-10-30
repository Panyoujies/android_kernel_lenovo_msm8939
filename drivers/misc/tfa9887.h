/*
 * tfa9887.h
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */
 
#ifndef _NXP_TFA9887_I2C_H_
#define _NXP_TFA9887_I2C_H_

#include <linux/version.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38))
#define KERNEL_ABOVE_2_6_38
#endif

#ifdef KERNEL_ABOVE_2_6_38
#define sstrtoul(...) kstrtoul(__VA_ARGS__)
#else
#define sstrtoul(...) strict_strtoul(__VA_ARGS__)
#endif

#define TFA9887_VTG_MIN_UV		1800000
#define TFA9887_VTG_MAX_UV		1800000
#define TFA9887_ACTIVE_LOAD_UA	10000 //15000
#define TFA9887_LPM_LOAD_UA	10

#define TFA9887_I2C_VTG_MIN_UV	1800000
#define TFA9887_I2C_VTG_MAX_UV	1800000
#define TFA9887_I2C_LOAD_UA	10000
#define TFA9887_I2C_LPM_LOAD_UA	10

extern int msm8x16_quat_mi2s_clk_ctl(bool enable);

struct tfa9887_i2c_platform_data {
	struct i2c_client *i2c_client;
	bool i2c_pull_up;
	bool regulator_en;
	int (*driver_opened)(void);
	void (*driver_closed)(void);
};

#endif
