/*
 * drivers/leds/sn3191_leds.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * sn3191 leds driver
 *
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/debugfs.h>




static int  i2c_device_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int  i2c_device_remove(struct i2c_client *client);



static struct i2c_client *i2c_device_cilent = NULL;
static struct regulator *reg_i2c = NULL;




static const struct i2c_device_id i2c_device_id[] = {
	{"lm36923",0},
	{}
};
static struct of_device_id lenovo_match_table[] = {
	{.compatible = "lenovo,lm36923"},
	{}
};

static struct i2c_driver i2c_device_driver = {
    .id_table = i2c_device_id,
    .probe = i2c_device_probe,
    .remove = i2c_device_remove,
    .driver = {
		.owner = THIS_MODULE,
		.name = "lm36923",
		.of_match_table = lenovo_match_table,
	},
};


static int i2c_write_reg(u8 addr, u8 para)
{
	struct i2c_msg msg[1];
	u8 data[2];
	int ret;

	data[0] = addr;
	data[1] = para;
	if(i2c_device_cilent != NULL){
		msg[0].addr = i2c_device_cilent->addr;
		msg[0].flags = 0;
		msg[0].buf = data;
		msg[0].len = ARRAY_SIZE(data);

		ret = i2c_transfer(i2c_device_cilent->adapter, msg, ARRAY_SIZE(msg));
		pr_debug("%s,slave=0x%x,add=0x%x,para=0x%x\n",__func__,msg[0].addr,data[0],data[1]);
		return ret;
	}else
		pr_err("%s no client\n",__func__);
    return 0;
}



int i2c_read_reg(u8 reg_offset, u8 *read_buf)
{
	struct i2c_msg msgs[2];
	int ret = -1;

	pr_debug("%s: reading from slave_addr=[%x] and offset=[%x]\n",
		 __func__, i2c_device_cilent->addr, reg_offset);
	
	if(i2c_device_cilent != NULL){

		msgs[0].addr = i2c_device_cilent->addr;
		msgs[0].flags = 0;
		msgs[0].buf = &reg_offset;
		msgs[0].len = 1;

		msgs[1].addr = i2c_device_cilent->addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].buf = read_buf;
		msgs[1].len = 1;

		ret = i2c_transfer(i2c_device_cilent->adapter, msgs, 2);
		if (ret < 1) {
			pr_err("%s: I2C READ FAILED=[%d]\n", __func__, ret);
			return -EACCES;
		}
		pr_debug("%s: i2c buf is [%x]\n", __func__, *read_buf);
		return 0;
	}
	return -1;
}


#define L5_VTG_MIN_UV	1750000
#define L5_VTG_MAX_UV 	1950000

static int i2c_device_regulator_configure(bool on)
{
	int ret;

	if (!on)
		goto pwr_deinit;
	
	if (IS_ERR(reg_i2c )) {
		ret = PTR_ERR(reg_i2c );
		pr_err("%s: Regulator get failed vdd ret=%d\n",__func__,ret);
	} else if (regulator_count_voltages(reg_i2c) > 0) {
		ret = regulator_set_voltage(reg_i2c, L5_VTG_MIN_UV,L5_VTG_MAX_UV);
		if (ret) {
                         	pr_err("%s: Regulator set failed vdd ret=%d\n",__func__,ret);
				goto err_vdd_put;
		}
	}

	return 0;
	
err_vdd_put:
	regulator_put(reg_i2c);
	return ret;

pwr_deinit:

	if ((!IS_ERR(reg_i2c)) &&
		(regulator_count_voltages(reg_i2c) > 0))
		regulator_set_voltage(reg_i2c, 0, L5_VTG_MAX_UV);
	return 0;
}

static int i2c_device_regulator_power_on( bool on)
{
	int rc = 0;

	if (!on) {
		rc = regulator_disable(reg_i2c);
		if (rc) {
			pr_err("bcm2079x_regulator_power_on: power vdd disable failed rc=%d\n", rc);
			return rc;
		}

	} else {
		rc = regulator_enable(reg_i2c);
		if (rc) {
			pr_err("bcm2079x_regulator_power_on: power vdd enable failed rc=%d\n", rc);
			return rc;
		}	
	}
	return 0;
}

static int i2c_device_setup_regulator(void)
{
        int  err =0;

        reg_i2c = regulator_get(&(i2c_device_cilent->dev), "vcc_i2c");
	if (!IS_ERR(reg_i2c)) {
		pr_debug("%s: regulator gte vdd ok\n",__func__);	
	   	err = i2c_device_regulator_configure(true);
		if(err){
			pr_err("%s: unable to configure regulator\n",__func__);			
		}

		err = i2c_device_regulator_power_on( true);
		if (err){
			pr_err( "%s: Can't configure regulator on\n",__func__);			
		}	
	}
	return 0;
}

static u8 level_lsb = 0x06;
static u8 level_msb = 0xcc;
static u8 dimming_step = 0;

int lm36923_config_20ma(void)
{
	int retry_time=3;
	u8 read_data[3]={0,0,0};
	if(i2c_device_regulator_power_on(1) == 0){
		while(retry_time--){
			//if(dimming_step == 8) i2c_write_reg(0x11,0x41);
			//else i2c_write_reg(0x11,0x51|((dimming_step&0x07)<<1));
			i2c_write_reg(0x11,0x51);
			i2c_write_reg(0x12,0x61);
			i2c_write_reg(0x18,level_lsb);
			i2c_write_reg(0x19,level_msb);
			i2c_write_reg(0x13,0x4F);
			i2c_read_reg(0x11,&read_data[0]);
			i2c_read_reg(0x18,&read_data[1]);
			i2c_read_reg(0x19,&read_data[2]);
			pr_debug("%s:i2c,0x%x -0x%x -0x%x\n",__func__,read_data[0],read_data[1],read_data[2]);
			if((read_data[1] == level_lsb) && (read_data[2] == level_msb))
			{
				printk("[houdz1]%s:set step = %d, level_lsb = 0x%x,level_msb=0x%x\n",__func__,dimming_step,level_lsb,level_msb);
				break;
			}
		}
		if(retry_time == 0){
			pr_err("%s[houdz1]:set backlight current fail!write(0x%x,0x%x),read(0x%x,0x%x)\n",__func__, level_msb,level_lsb,read_data[2],read_data[1]);
			return -1;
		}else return 0;
	}else return -1;
}
EXPORT_SYMBOL_GPL(lm36923_config_20ma);


static ssize_t set_dimming_step_time(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)/*value = 0....255*/
{
	unsigned int value = 0;
	unsigned int dimming_level;

	u8 read_value;
	int retry_time=3;

	if(kstrtouint(buf, 10, &value)) return -1;
	value &= 0x0f;
	if(value >8) value = 8;

	dimming_level = value;
	if(dimming_level == dimming_step) return size;

	if(value == 8) value = 0x41;
	else{
		value &=0x07;
		value <<= 1;
		value |= 0x51;
	}
	while(retry_time--){
		i2c_write_reg(0x11,value);
		i2c_read_reg(0x11,&read_value);
		if(read_value == value) break;
	}
	if(retry_time == 0)
	{
		pr_err("%s[houdz1]:set_dimming_step_time fail,read 0x%x write 0x%x\n",__func__,read_value,value);
		return -1;
	}

	dimming_step = dimming_level;
	printk("[houdz1]%s:set step = %d, ok!\n",__func__,dimming_step);
	return size;
}

ssize_t get_dimming_step_time(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	u8 value ;

	i2c_read_reg(0x11,&value);
	if((value &0xf0) == 0x40) value = 8;
	else{
		value &= 0x0f;
		value >>= 1;
	}
	ret = snprintf(buf, PAGE_SIZE, "%d\n",value);
	return ret;
}

static DEVICE_ATTR(dimming_step_time, S_IRUGO|S_IWUSR|S_IWGRP,get_dimming_step_time, set_dimming_step_time);



static ssize_t set_backlight_current(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)/*value = 0....255*/
{
	unsigned int value = 255;
	u8 read_data[2]={0,0};
	int retry_time=3;

	if(kstrtouint(buf, 10, &value)) return -1;

	if(value >255) value = 255;
	level_lsb = (value*1638/255) & 0x07; //1638 = 2048*0.8
	level_msb = ((value*1638/255)>>3) &0xFF;

	while(retry_time--){
		i2c_write_reg(0x18,level_lsb);
		i2c_write_reg(0x19,level_msb);
		i2c_read_reg(0x18,&read_data[0]);
		i2c_read_reg(0x19,&read_data[1]);
		if((read_data[0] == level_lsb) && (read_data[1] == level_msb)) break;
		pr_err("%s[houdz1]:write (0x%x , 0x%x)  read (0x%x , 0x%x)\n",__func__,level_lsb,level_msb,read_data[0],read_data[1]);
	}
	if(retry_time>0){
		printk("%s:set backlight current = %d mA, ok!\n",__func__,20*value/255);
		return size;
	}else{
		printk("%s:set backlight current = %d mA, fail!\n",__func__,20*value/255);
		return -1;
	}
	return size;
}

static DEVICE_ATTR(backlight_current, S_IWUSR|S_IWGRP|S_IRUGO, NULL, set_backlight_current);


static int  i2c_device_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret=0;

	pr_debug("[LED]%s\n", __func__);

	i2c_device_cilent = client;
	i2c_device_setup_regulator();

	ret = device_create_file(&client->dev, &dev_attr_backlight_current);
	if (ret < 0) pr_err("failed to create backlight_current file\n");
	ret = device_create_file(&client->dev, &dev_attr_dimming_step_time);
	if (ret < 0) pr_err("failed to create dimming_step_time file\n");
	return ret;
 }

static int  i2c_device_remove(struct i2c_client *client)
{
   pr_debug("[LED]%s\n", __func__);
   device_remove_file(&client->dev, &dev_attr_backlight_current);
   return 0;
}



/***********************************************************************************
* please add platform device in mt_devs.c
*
************************************************************************************/

MODULE_DEVICE_TABLE(of, lenovo_match_table);




static int __init i2c_device_init(void)
{
	pr_debug("[LED]%s\n", __func__);
	if(i2c_add_driver(&i2c_device_driver))
	{
		pr_err("add i2c driver error %s\n",__func__);
		return -1;
	} 
	return 0;
}

static void __exit i2c_device_exit(void)
{
	i2c_del_driver(&i2c_device_driver);
}


module_init(i2c_device_init);
module_exit(i2c_device_exit);

MODULE_AUTHOR("houdz1@lenovo.com");
MODULE_DESCRIPTION("i2c device(lm36923) driver");
MODULE_LICENSE("GPL");



