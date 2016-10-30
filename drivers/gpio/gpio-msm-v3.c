/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <mach/msm_iomap.h>
#include <mach/gpiomux.h>
#include "gpio-msm-common.h"

/* Bits of interest in the GPIO_IN_OUT register.
 */
enum {
	GPIO_IN_BIT  = 0,
	GPIO_OUT_BIT = 1
};

/* Bits of interest in the GPIO_INTR_STATUS register.
 */
enum {
	INTR_STATUS_BIT = 0,
};

/* Bits of interest in the GPIO_CFG register.
 */
enum {
	GPIO_OE_BIT = 9,
};

/* Bits of interest in the GPIO_INTR_CFG register.
 */
enum {
	INTR_ENABLE_BIT        = 0,
	INTR_POL_CTL_BIT       = 1,
	INTR_DECT_CTL_BIT      = 2,
	INTR_RAW_STATUS_EN_BIT = 4,
	INTR_TARGET_PROC_BIT   = 5,
	INTR_DIR_CONN_EN_BIT   = 8,
};

/*
 * There is no 'DC_POLARITY_LO' because the GIC is incapable
 * of asserting on falling edge or level-low conditions.  Even though
 * the registers allow for low-polarity inputs, the case can never arise.
 */
enum {
	DC_GPIO_SEL_BIT = 0,
	DC_POLARITY_BIT	= 8,
};

/* Default id of application processor subsystem */
static unsigned subsys_id = 4;
/*
 * When a GPIO triggers, two separate decisions are made, controlled
 * by two separate flags.
 *
 * - First, INTR_RAW_STATUS_EN controls whether or not the GPIO_INTR_STATUS
 * register for that GPIO will be updated to reflect the triggering of that
 * gpio.  If this bit is 0, this register will not be updated.
 * - Second, INTR_ENABLE controls whether an interrupt is triggered.
 *
 * If INTR_ENABLE is set and INTR_RAW_STATUS_EN is NOT set, an interrupt
 * can be triggered but the status register will not reflect it.
 */
#define INTR_RAW_STATUS_EN BIT(INTR_RAW_STATUS_EN_BIT)
#define INTR_ENABLE        BIT(INTR_ENABLE_BIT)
#define INTR_POL_CTL_HI    BIT(INTR_POL_CTL_BIT)
#define INTR_DIR_CONN_EN   BIT(INTR_DIR_CONN_EN_BIT)
#define DC_POLARITY_HI     BIT(DC_POLARITY_BIT)

#define INTR_TARGET_PROC_APPS    (subsys_id << INTR_TARGET_PROC_BIT)
#define INTR_TARGET_PROC_NONE    (7 << INTR_TARGET_PROC_BIT)

#define INTR_DECT_CTL_LEVEL      (0 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_POS_EDGE   (1 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_NEG_EDGE   (2 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_DUAL_EDGE  (3 << INTR_DECT_CTL_BIT)
#define INTR_DECT_CTL_MASK       (3 << INTR_DECT_CTL_BIT)

#define GPIO_CONFIG(gpio)        (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#define GPIO_IN_OUT(gpio)        (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))
#define GPIO_INTR_CFG(gpio)      (MSM_TLMM_BASE + 0x1008 + (0x10 * (gpio)))
#define GPIO_INTR_STATUS(gpio)   (MSM_TLMM_BASE + 0x100c + (0x10 * (gpio)))
#define GPIO_DIR_CONN_INTR(intr) (MSM_TLMM_BASE + 0x2800 + (0x04 * (intr)))

static inline void set_gpio_bits(unsigned n, void __iomem *reg)
{
	__raw_writel_no_log(__raw_readl_no_log(reg) | n, reg);
}

static inline void clr_gpio_bits(unsigned n, void __iomem *reg)
{
	__raw_writel_no_log(__raw_readl_no_log(reg) & ~n, reg);
}

unsigned __msm_gpio_get_inout(unsigned gpio)
{
	return __raw_readl_no_log(GPIO_IN_OUT(gpio)) & BIT(GPIO_IN_BIT);
}

void __msm_gpio_set_inout(unsigned gpio, unsigned val)
{
	__raw_writel_no_log(val ? BIT(GPIO_OUT_BIT) : 0, GPIO_IN_OUT(gpio));
}

void __msm_gpio_set_config_direction(unsigned gpio, int input, int val)
{
	if (input) {
		clr_gpio_bits(BIT(GPIO_OE_BIT), GPIO_CONFIG(gpio));
	} else {
		__msm_gpio_set_inout(gpio, val);
		set_gpio_bits(BIT(GPIO_OE_BIT), GPIO_CONFIG(gpio));
	}
}

void __msm_gpio_set_polarity(unsigned gpio, unsigned val)
{
	if (val)
		clr_gpio_bits(INTR_POL_CTL_HI, GPIO_INTR_CFG(gpio));
	else
		set_gpio_bits(INTR_POL_CTL_HI, GPIO_INTR_CFG(gpio));
}

unsigned __msm_gpio_get_intr_status(unsigned gpio)
{
	return __raw_readl_no_log(GPIO_INTR_STATUS(gpio)) &
					BIT(INTR_STATUS_BIT);
}

void __msm_gpio_set_intr_status(unsigned gpio)
{
	__raw_writel_no_log(0, GPIO_INTR_STATUS(gpio));
}

unsigned __msm_gpio_get_intr_config(unsigned gpio)
{
	return __raw_readl_no_log(GPIO_INTR_CFG(gpio));
}

void __msm_gpio_set_intr_cfg_enable(unsigned gpio, unsigned val)
{
	unsigned cfg;

	cfg = __raw_readl_no_log(GPIO_INTR_CFG(gpio));
	if (val) {
		cfg &= ~INTR_DIR_CONN_EN;
		cfg |= INTR_ENABLE;
	} else {
		cfg &= ~INTR_ENABLE;
	}
	__raw_writel_no_log(cfg, GPIO_INTR_CFG(gpio));
}

unsigned  __msm_gpio_get_intr_cfg_enable(unsigned gpio)
{
	return __msm_gpio_get_intr_config(gpio) & INTR_ENABLE;
}

void __msm_gpio_set_intr_cfg_type(unsigned gpio, unsigned type)
{
	unsigned cfg;

	/* RAW_STATUS_EN is left on for all gpio irqs. Due to the
	 * internal circuitry of TLMM, toggling the RAW_STATUS
	 * could cause the INTR_STATUS to be set for EDGE interrupts.
	 */
	cfg = INTR_RAW_STATUS_EN | INTR_TARGET_PROC_APPS;
	__raw_writel_no_log(cfg, GPIO_INTR_CFG(gpio));
	cfg &= ~INTR_DECT_CTL_MASK;
	if (type == IRQ_TYPE_EDGE_RISING)
		cfg |= INTR_DECT_CTL_POS_EDGE;
	else if (type == IRQ_TYPE_EDGE_FALLING)
		cfg |= INTR_DECT_CTL_NEG_EDGE;
	else if (type == IRQ_TYPE_EDGE_BOTH)
		cfg |= INTR_DECT_CTL_DUAL_EDGE;
	else
		cfg |= INTR_DECT_CTL_LEVEL;

	if (type & IRQ_TYPE_LEVEL_LOW)
		cfg &= ~INTR_POL_CTL_HI;
	else
		cfg |= INTR_POL_CTL_HI;

	__raw_writel_no_log(cfg, GPIO_INTR_CFG(gpio));
	/* Sometimes it might take a little while to update
	 * the interrupt status after the RAW_STATUS is enabled
	 * We clear the interrupt status before enabling the
	 * interrupt in the unmask call-back.
	 */
	udelay(5);
}

void __msm_gpio_set_subsys_id(unsigned id)
{
	subsys_id = id;
}

void __gpio_tlmm_config(unsigned config)
{
	unsigned flags;
	unsigned gpio = GPIO_PIN(config);

	flags = ((GPIO_DIR(config) << 9) & (0x1 << 9)) |
		((GPIO_DRVSTR(config) << 6) & (0x7 << 6)) |
		((GPIO_FUNC(config) << 2) & (0xf << 2)) |
		((GPIO_PULL(config) & 0x3));
	__raw_writel_no_log(flags, GPIO_CONFIG(gpio));
}

void __msm_gpio_install_direct_irq(unsigned gpio, unsigned irq,
					unsigned int input_polarity)
{
	unsigned cfg;

	set_gpio_bits(BIT(GPIO_OE_BIT), GPIO_CONFIG(gpio));
	cfg = __raw_readl_no_log(GPIO_INTR_CFG(gpio));
	cfg &= ~(INTR_TARGET_PROC_NONE | INTR_RAW_STATUS_EN | INTR_ENABLE);
	cfg |= INTR_TARGET_PROC_APPS | INTR_DIR_CONN_EN;
	__raw_writel_no_log(cfg, GPIO_INTR_CFG(gpio));

	cfg = gpio;
	if (input_polarity)
		cfg |= DC_POLARITY_HI;
	__raw_writel_no_log(cfg, GPIO_DIR_CONN_INTR(irq));
}

/* yangjq, 20130515, Add sysfs for gpio's debug, START */
#define TLMM_NUM_GPIO 146

#define HAL_OUTPUT_VAL(config)    \
         (((config)&0x40000000)>>30)

static int tlmm_get_cfg(unsigned gpio, unsigned* cfg)
{
	unsigned flags;

	BUG_ON(gpio >= TLMM_NUM_GPIO);
	//printk("%s(), gpio=%d, addr=0x%08x\n", __func__, gpio, (unsigned int)GPIO_CONFIG(gpio));

#if 0
	flags = ((GPIO_DIR(config) << 9) & (0x1 << 9)) |
		((GPIO_DRVSTR(config) << 6) & (0x7 << 6)) |
		((GPIO_FUNC(config) << 2) & (0xf << 2)) |
		((GPIO_PULL(config) & 0x3));
#else
	flags = __raw_readl(GPIO_CONFIG(gpio));
#endif
	*cfg = GPIO_CFG(gpio, (flags >> 2) & 0xf, (flags >> 9) & 0x1, flags & 0x3, (flags >> 6) & 0x7);

	return 0;
}

static int tlmm_dump_cfg(char* buf,unsigned gpio, unsigned cfg, int output_val)
{
	static char* drvstr_str[] = { "2", "4", "6", "8", "10", "12", "14", "16" }; // mA
	static char*   pull_str[] = { "N", "D", "K", "U" };	 // "NO_PULL", "PULL_DOWN", "KEEPER", "PULL_UP"
	static char*    dir_str[] = { "I", "O" }; // "Input", "Output"	 
	char func_str[20];
	
	char* p = buf;

	int drvstr   = GPIO_DRVSTR(cfg);
	int pull     = GPIO_PULL(cfg);
	int dir      = GPIO_DIR(cfg);
	int func     = GPIO_FUNC(cfg);

	//printk("%s(), drvstr=%d, pull=%d, dir=%d, func=%d\n", __func__, drvstr, pull, dir, func);
	sprintf(func_str, "%d", func);

	p += sprintf(p, "%d:0x%x %s%s%s%s", gpio, cfg,
			func_str, pull_str[pull], dir_str[dir], drvstr_str[drvstr]);

	p += sprintf(p, " = %d", output_val);

	p += sprintf(p, "\n");	
			
	return p - buf;		
}

static int tlmm_dump_header(char* buf)
{
	char* p = buf;
	p += sprintf(p, "bit   0~3: function. (0 is GPIO)\n");
	p += sprintf(p, "bit  4~13: gpio number\n");
	p += sprintf(p, "bit    14: 0: input, 1: output\n");
	p += sprintf(p, "bit 15~16: pull: NO_PULL, PULL_DOWN, KEEPER, PULL_UP\n");
	p += sprintf(p, "bit 17~20: driver strength. \n");
	p += sprintf(p, "0:GPIO\n");
	p += sprintf(p, "N:NO_PULL  D:PULL_DOWN  K:KEEPER  U:PULL_UP\n");
	p += sprintf(p, "I:Input  O:Output\n");
	p += sprintf(p, "2:2, 4, 6, 8, 10, 12, 14, 16 mA (driver strength)\n\n");
	return p - buf;
}

int tlmm_dump_info(char* buf, int tlmm_num)
{
	unsigned i;
	char* p = buf;
	unsigned cfg;
	int output_val = 0;

	if(tlmm_num >= 0 && tlmm_num < TLMM_NUM_GPIO) {
		tlmm_get_cfg(tlmm_num, &cfg);
		output_val = __msm_gpio_get_inout(tlmm_num);
			
		p += tlmm_dump_cfg(p, tlmm_num, cfg, output_val);
	} else {
		p += tlmm_dump_header(p);
		p += sprintf(p, "Standard Format: gpio_num  function  pull  direction  strength [output_value]\n");
		p += sprintf(p, "Shortcut Format: gpio_num  output_value\n");
		p += sprintf(p, " e.g.  'echo  20 0 D O 2 1'  ==> set pin 20 as GPIO output and the output = 1 \n");
		p += sprintf(p, " e.g.  'echo  20 1'  ==> set output gpio pin 20 output = 1 \n");
	
		printk("%s(), %d\n", __func__, __LINE__);
		for(i = 0; i < TLMM_NUM_GPIO; ++i) {
			tlmm_get_cfg(i, &cfg);
			output_val = __msm_gpio_get_inout(i);
			
			p += tlmm_dump_cfg(p, i, cfg, output_val);
		}
		printk("%s(), %d\n", __func__, __LINE__);
		p+= sprintf(p, "(%d)\n", p - buf); // only for debug reference	
	}
	return p - buf;	
}

/* save tlmm config before sleep */
static unsigned before_sleep_fetched;
static unsigned before_sleep_configs[TLMM_NUM_GPIO];
void tlmm_before_sleep_save_configs(void)
{
	unsigned i;

	//only save tlmm configs when it has been fetched
	if (!before_sleep_fetched)
		return;

	printk("%s(), before_sleep_fetched=%d\n", __func__, before_sleep_fetched);
	before_sleep_fetched = false;
	for(i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int output_val = 0;

		tlmm_get_cfg(i, &cfg);
		output_val = __msm_gpio_get_inout(i);

		before_sleep_configs[i] = cfg | (output_val << 30);
	}
}

int tlmm_before_sleep_dump_info(char* buf)
{
	unsigned i;
	char* p = buf;

	p += sprintf(p, "tlmm_before_sleep:\n");
	if (!before_sleep_fetched) {
		before_sleep_fetched = true;

		p += tlmm_dump_header(p);
		
		for(i = 0; i < TLMM_NUM_GPIO; ++i) {
			unsigned cfg;
			int output_val = 0;

			cfg = before_sleep_configs[i];
			output_val = HAL_OUTPUT_VAL(cfg);
			//cfg &= ~0x40000000;
			p += tlmm_dump_cfg(p, i, cfg, output_val);
		}
		p+= sprintf(p, "(%d)\n", p - buf); // only for debug reference
	}
	return p - buf;	
}

/* set tlmms config before sleep */
static unsigned before_sleep_table_enabled;
static unsigned before_sleep_table_configs[TLMM_NUM_GPIO];
void tlmm_before_sleep_set_configs(void)
{
	int res;
	unsigned i;

	//only set tlmms before sleep when it's enabled
	if (!before_sleep_table_enabled)
		return;

	printk("%s(), before_sleep_table_enabled=%d\n", __func__, before_sleep_table_enabled);
	for(i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int gpio;
		int dir;
		int func;
		int output_val = 0;

		cfg = before_sleep_table_configs[i];

		gpio = GPIO_PIN(cfg);
		if(gpio != i)//(cfg & ~0x20000000) == 0 || 
			continue;

		output_val = HAL_OUTPUT_VAL(cfg);
		//Clear the output value
		//cfg &= ~0x40000000;
		dir = GPIO_DIR(cfg);
		func = GPIO_FUNC(cfg);

		printk("%s(), [%d]: 0x%x\n", __func__, i, cfg);
		res = gpio_tlmm_config((cfg & ~0x40000000), GPIO_CFG_ENABLE);
		if(res < 0) {
			printk("Error: Config failed.\n");
		}
		
		if((func == 0) && (dir == 1)) // gpio output
			__msm_gpio_set_inout(i, output_val);
	}
}

int tlmm_before_sleep_table_set_cfg(unsigned gpio, unsigned cfg)
{
	//BUG_ON(gpio >= TLMM_NUM_GPIO && GPIO_PIN(cfg) != 0xff);
	if (gpio >= TLMM_NUM_GPIO && gpio != 255 && gpio != 256) {
		printk("gpio >= TLMM_NUM_GPIO && gpio != 255 && gpio != 256!\n");
		return -1;
	}

	if(gpio < TLMM_NUM_GPIO)
	{
		before_sleep_table_configs[gpio] = cfg;// | 0x20000000
		before_sleep_table_enabled = true;
	}
	else if(gpio == 255)
		before_sleep_table_enabled = true;
	else if(gpio == 256)
		before_sleep_table_enabled = false;

	return 0;
}

int tlmm_before_sleep_table_dump_info(char* buf)
{
	unsigned i;
	char* p = buf;

	p += tlmm_dump_header(p);
	p += sprintf(p, "Format: gpio_num  function  pull  direction  strength [output_value]\n");
	p += sprintf(p, " e.g.  'echo  20 0 D O 2 1'  ==> set pin 20 as GPIO output and the output = 1 \n");
	p += sprintf(p, " e.g.  'echo  20'  ==> disable pin 20's setting \n");
	p += sprintf(p, " e.g.  'echo  255'  ==> enable sleep table's setting \n");
	p += sprintf(p, " e.g.  'echo  256'  ==> disable sleep table's setting \n");

	for(i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int output_val = 0;

		cfg = before_sleep_table_configs[i];
		output_val = HAL_OUTPUT_VAL(cfg);
		//cfg &= ~0x40000000;
		p += tlmm_dump_cfg(p, i, cfg, output_val);
	}
	p+= sprintf(p, "(%d)\n", p - buf); // only for debug reference
	return p - buf;
}
/* yangjq, 20130515, Add sysfs for gpio's debug, END */
