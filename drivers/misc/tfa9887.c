/*
 * tfa9887.c
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include "tfa9887.h"
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif
 /* lenovo-sw zhangrc2 add for smartpa begin */
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
 /* lenovo-sw zhangrc2 add for smartpa end */
#define DRIVER_NAME 		"tfa9887"
#define MAX_BUFFER_SIZE 	512

#define TFA9887_ID_REG		0x03
#define TFA9887_CHIPID		0x0080

struct tfa9887_dev	{
	struct mutex		read_mutex;
	struct device	*dev;
	struct i2c_client	*i2c_client;
	struct miscdevice	tfa9887_device;
	struct regulator    *vdd;
	struct regulator    *vcc_i2c;
};

static int tfa9887_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);

	return ret;
}

static ssize_t tfa9887_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct tfa9887_dev *tfa9887_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&tfa9887_dev->read_mutex);

	/* Read data */
	ret = i2c_master_recv(tfa9887_dev->i2c_client, tmp, count);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		mutex_unlock(&tfa9887_dev->read_mutex);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		mutex_unlock(&tfa9887_dev->read_mutex);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		mutex_unlock(&tfa9887_dev->read_mutex);
		return -EFAULT;
	}
	mutex_unlock(&tfa9887_dev->read_mutex);
	return ret;

}

static ssize_t tfa9887_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct tfa9887_dev  *tfa9887_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	/* Write data */
	ret = i2c_master_send(tfa9887_dev->i2c_client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return ret;
}

static int tfa9887_dev_open(struct inode *inode, struct file *filp)
{
	struct tfa9887_dev *tfa9887_dev = container_of(filp->private_data,
						struct tfa9887_dev,
						tfa9887_device);

	filp->private_data = tfa9887_dev;
	pr_err("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static long tfa9887_dev_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct tfa9887_dev *tfa9887_dev = filp->private_data;

	//pr_err("%s : cmd = 0x%x\n", __func__, cmd);
	switch(cmd)
	{
		case I2C_SLAVE:
			tfa9887_dev->i2c_client->addr = arg;
			break;
		case ENABLE_MI2S_CLK:
			msm8x16_quat_mi2s_clk_ctl(arg);
			break;
		default:
			break;
	}
	return 0;
}

static const struct file_operations tfa9887_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= tfa9887_dev_read,
	.write	= tfa9887_dev_write,
	.open	= tfa9887_dev_open,
	.unlocked_ioctl	= tfa9887_dev_ioctl,
};

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int tfa9887_power_on(struct tfa9887_dev *tfa9887_dev,
					bool on) {
	int retval;
	return 0;
	if (on == false)
		goto power_off;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	retval = reg_set_optimum_mode_check(tfa9887_dev->vdd,
		TFA9887_ACTIVE_LOAD_UA);
	if (retval < 0) {
		dev_err(&tfa9887_dev->i2c_client->dev, "Regulator vdd set_opt failed rc=%d\n", retval);
		return retval;
	}

	retval = regulator_enable(tfa9887_dev->vdd);
	if (retval) {
		dev_err(&tfa9887_dev->i2c_client->dev, "Regulator vdd enable failed rc=%d\n", retval);
		goto error_reg_en_vdd;
	}

	retval = reg_set_optimum_mode_check(tfa9887_dev->vcc_i2c,
		TFA9887_I2C_LOAD_UA);
	if (retval < 0) {
		dev_err(&tfa9887_dev->i2c_client->dev, "Regulator vcc_i2c set_opt failed rc=%d\n", retval);
		goto error_reg_opt_i2c;
	}

	retval = regulator_enable(tfa9887_dev->vcc_i2c);
	if (retval) {
		dev_err(&tfa9887_dev->i2c_client->dev, "Regulator vcc_i2c enable failed rc=%d\n", retval);
		goto error_reg_en_vcc_i2c;
	}
	return 0;

error_reg_en_vcc_i2c:
	reg_set_optimum_mode_check(tfa9887_dev->vcc_i2c, 0);
error_reg_opt_i2c:
	regulator_disable(tfa9887_dev->vdd);
error_reg_en_vdd:
	reg_set_optimum_mode_check(tfa9887_dev->vdd, 0);
	return retval;

power_off:
	reg_set_optimum_mode_check(tfa9887_dev->vdd, 0);
	regulator_disable(tfa9887_dev->vdd);
	reg_set_optimum_mode_check(tfa9887_dev->vcc_i2c, 0);
	regulator_disable(tfa9887_dev->vcc_i2c);
	return 0;
}

static int tfa9887_regulator_configure(struct tfa9887_dev *tfa9887_dev, bool on)
{
	int retval;
	return 0;
	if (on == false)
		goto hw_shutdown;

	tfa9887_dev->vdd = regulator_get(&tfa9887_dev->i2c_client->dev, "vdd");
	if (IS_ERR(tfa9887_dev->vdd)) {
		dev_err(&tfa9887_dev->i2c_client->dev, "%s: Failed to get vdd regulator\n", __func__);
		return PTR_ERR(tfa9887_dev->vdd);
	}

	if (regulator_count_voltages(tfa9887_dev->vdd) > 0) {
		retval = regulator_set_voltage(tfa9887_dev->vdd,
			TFA9887_VTG_MIN_UV, TFA9887_VTG_MAX_UV);
		if (retval) {
			dev_err(&tfa9887_dev->i2c_client->dev, "regulator set_vtg failed retval =%d\n", retval);
			goto err_set_vtg_vdd;
		}
	}

	tfa9887_dev->vcc_i2c = regulator_get(&tfa9887_dev->i2c_client->dev,
					"vcc_i2c");
	if (IS_ERR(tfa9887_dev->vcc_i2c)) {
		dev_err(&tfa9887_dev->i2c_client->dev, "%s: Failed to get i2c regulator\n", __func__);
		retval = PTR_ERR(tfa9887_dev->vcc_i2c);
		goto err_get_vtg_i2c;
	}

	if (regulator_count_voltages(tfa9887_dev->vcc_i2c) > 0) {
		retval = regulator_set_voltage(tfa9887_dev->vcc_i2c,
			TFA9887_I2C_VTG_MIN_UV, TFA9887_I2C_VTG_MAX_UV);
		if (retval) {
			dev_err(&tfa9887_dev->i2c_client->dev, "reg set i2c vtg failed retval =%d\n", retval);
		goto err_set_vtg_i2c;
		}
	}
	return 0;

err_set_vtg_i2c:
	regulator_put(tfa9887_dev->vcc_i2c);
err_get_vtg_i2c:
	if (regulator_count_voltages(tfa9887_dev->vdd) > 0)
		regulator_set_voltage(tfa9887_dev->vdd, 0, TFA9887_VTG_MAX_UV);
err_set_vtg_vdd:
	regulator_put(tfa9887_dev->vdd);
	return retval;

hw_shutdown:
	if (regulator_count_voltages(tfa9887_dev->vdd) > 0)
		regulator_set_voltage(tfa9887_dev->vdd, 0, TFA9887_VTG_MAX_UV);
	regulator_put(tfa9887_dev->vdd);
	if (regulator_count_voltages(tfa9887_dev->vcc_i2c) > 0)
		regulator_set_voltage(tfa9887_dev->vcc_i2c, 0, TFA9887_I2C_VTG_MAX_UV);
	regulator_put(tfa9887_dev->vcc_i2c);
	return 0;
}

static int tfa9887_get_id(struct i2c_client *client)
{
	u8 i = 0;
	int ret = 0;
	int chip_version = 0;

	for (i = 0; i < 5; i++)
	{
		ret = tfa9887_read_reg(client, TFA9887_ID_REG);
		if(ret < 0) {
			i++;
		}
		else {
			break;
		}
	}

	if (i == 5) {
		return -1;
 	}
	chip_version = ((u16)(ret & 0xFF)<<8) + ((u16)(ret & 0xFF00)>>8); 
	if (chip_version == TFA9887_CHIPID) {
		pr_info("%s: read success, NXP TFA9887 ID = 0x%04x\n", __func__, chip_version);
		return 1;
	}
	else {
		pr_info("%s: read chipID fail!\n", __func__);
		return -1;
	}
}

 /**
 * nxp_tfa9887_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 */
static int nxp_tfa9887_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{

 /* lenovo-sw zhangrc2 add for smartpa begin */

	struct device_node *np =client->dev.of_node;
	int smartpa_ldo_num = 0;
 /* lenovo-sw zhangrc2 add for smartpa begin */
	int ret = 0;
	struct tfa9887_dev *tfa9887_dev;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check failed\n", __func__);
		ret = -ENODEV;
		goto err_exit;
	}

	tfa9887_dev = kzalloc(sizeof(*tfa9887_dev), GFP_KERNEL);
	if (tfa9887_dev == NULL) {
		pr_err("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	
	tfa9887_dev->i2c_client   = client;
	tfa9887_dev->dev = &client->dev;

	/* Initialise mutex and work queue */
	mutex_init(&tfa9887_dev->read_mutex);

	tfa9887_dev->tfa9887_device.minor = MISC_DYNAMIC_MINOR;
    /* lenovo-sw zhangrc2 change device name for smartpa begin */ 
	//tfa9887_dev->tfa9887_device.name = "tfa9887";
    tfa9887_dev->tfa9887_device.name = "i2c_smartpa"; 
    /* lenovo-sw zhangrc2 change device name for smartpa end */ 
	tfa9887_dev->tfa9887_device.fops = &tfa9887_dev_fops;
    /* lenovo-sw zhangrc2 add for smartpa begin */	
 	smartpa_ldo_num = of_get_named_gpio_flags(np,
			"qcom,smartpa-ldo", 0, 0);
	if(gpio_is_valid(smartpa_ldo_num) ) {	
		gpio_free(smartpa_ldo_num)	;
          //		ret = gpio_request(smartpa_ldo_num, "smartpa-ldo-num");
	 //        printk("shone ret   is %d\n",ret);		 
	//	gpio_direction_output(smartpa_ldo_num,1);
	//	gpio_set_value(smartpa_ldo_num,1);		
	}
   /* lenovo-sw zhangrc2 add for smartpa end */	
	ret = tfa9887_regulator_configure(tfa9887_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to configure regulators\n", __func__);
		goto err_exit;
	}

	ret = tfa9887_power_on(tfa9887_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to power on\n", __func__);
		goto err_exit;
	}
	msleep(10);

	ret = misc_register(&tfa9887_dev->tfa9887_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	ret = tfa9887_get_id(client);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to get chipID\n", __func__);
		goto err_misc_register;
	}

	pr_info("%s Done\n", __func__);
	
	return 0;

err_misc_register:
	misc_deregister(&tfa9887_dev->tfa9887_device);
	mutex_destroy(&tfa9887_dev->read_mutex);
	kfree(tfa9887_dev);
err_exit:
	return ret;
}

 /**
 * nxp_tfa9887_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 */
static int nxp_tfa9887_remove(struct i2c_client *client)
{
	struct tfa9887_dev *tfa9887_dev;

	pr_info("%s\n", __func__);
	tfa9887_dev = i2c_get_clientdata(client);
	misc_deregister(&tfa9887_dev->tfa9887_device);
	mutex_destroy(&tfa9887_dev->read_mutex);

	kfree(tfa9887_dev);

	return 0;
}

#ifdef CONFIG_PM
 /**
 * nxp_tfa9887_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 */
static int nxp_tfa9887_suspend(struct device *dev)
{
	pr_info(KERN_ALERT "-------nxp_tfa9887_suspend");

	return 0;
}

 /**
 * nxp_tfa9887_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 */
static int nxp_tfa9887_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops nxp_tfa9887_dev_pm_ops = {
	.suspend = nxp_tfa9887_suspend,
	.resume  = nxp_tfa9887_resume,
};
#endif

static const struct i2c_device_id nxp_tfa9887_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, nxp_tfa9887_id_table);
/* lenovo-sw zhangrc2 add for test i2c is ok begin */
#ifdef CONFIG_OF
static struct of_device_id tfa9887_match_table[] = {
	{ .compatible = "nxp,tfa9887",},
	{ },
};
#else
//#define tfa9887_match_table NULL

static struct of_device_id tfa9887_match_table[] = {
	{ .compatible = "nxp,tfa9887",},
	{ },
#endif
/* lenovo-sw zhangrc2 add for test i2c is ok end */
static struct i2c_driver nxp_tfa9887_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tfa9887_match_table,
#ifdef CONFIG_PM
		.pm = &nxp_tfa9887_dev_pm_ops,
#endif
	},
	.probe = nxp_tfa9887_probe,
	.remove = nxp_tfa9887_remove,
	.id_table = nxp_tfa9887_id_table,
};

 /**
 * nxp_tfa9887_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init nxp_tfa9887_init(void)
{
/* lenovo-sw zhangrc2 add for test i2c is ok begin */
    int ret; 
	pr_info("%s\n",__func__);
	ret=i2c_add_driver(&nxp_tfa9887_driver);
    if (ret!=0){
           printk("shone add driver for smartpa is error\n"); 
           return ret;
    } else {
            printk("shone add driver for smartpa is ok\n");
            return 0;
    }
}
/* lenovo-sw zhangrc2 add for test i2c is ok end */
 /**
 * nxp_tfa9887_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit nxp_tfa9887_exit(void)
{
	i2c_del_driver(&nxp_tfa9887_driver);

	return;
}

module_init(nxp_tfa9887_init);
module_exit(nxp_tfa9887_exit);

MODULE_AUTHOR("NXP, Inc.");
MODULE_DESCRIPTION("NXP TFA9885 I2C Touch Driver");
MODULE_LICENSE("GPL v2");
