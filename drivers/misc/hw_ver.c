//
// hw_ver.c
//
// Drivers for hw version detected.
//

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/err.h>

struct hw_ver_pdata {
	unsigned int	ver_gpio0;
	unsigned int	ver_gpio1;
	unsigned int	ver_gpio2;
};

static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hw_ver_pdata *pdata;
	unsigned int ver_gpio0, ver_gpio1, ver_gpio2;
	unsigned int hw_ver = 0;

	pdata = dev_get_drvdata(dev);

	ver_gpio0 = gpio_get_value(pdata->ver_gpio0);
	ver_gpio1 = gpio_get_value(pdata->ver_gpio1);
	ver_gpio2 = gpio_get_value(pdata->ver_gpio2);
	pr_info("ver_gpio0 = %d, ver_gpio1 = %d, ver_gpio2 = %d\n", ver_gpio0, ver_gpio1, ver_gpio2);

	hw_ver = (ver_gpio0 & 0x1);
	hw_ver |= (ver_gpio1 & 0x1) << 4;
	hw_ver |= (ver_gpio2 & 0x1) << 8;

	return sprintf(buf, "%04x\n", hw_ver);
}


static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

static struct attribute *hw_ver_attrs[] = {
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group hw_ver_attr_group = {
	.attrs = hw_ver_attrs,
};

static int  hw_ver_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct hw_ver_pdata *pdata;
	int rc = 0;

	node = pdev->dev.of_node;

	pdata = kzalloc(sizeof(struct hw_ver_pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err( "%s: Can't allocate hw_version_chip\n", __func__);
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, pdata);

	if (of_gpio_count(node) < 3)
		goto free_data;

	pdata->ver_gpio0 = of_get_gpio(node, 0);
	pdata->ver_gpio1 = of_get_gpio(node, 1);
	pdata->ver_gpio2 = of_get_gpio(node, 2);

	if (!gpio_is_valid(pdata->ver_gpio0) || !gpio_is_valid(pdata->ver_gpio1)|| !gpio_is_valid(pdata->ver_gpio2)) {
		pr_err("%s: invalid GPIO pins, ver_gpio0=%d/ver_gpio1=%d/ver_gpio2=%d\n",
		       node->full_name, pdata->ver_gpio0, pdata->ver_gpio1,pdata->ver_gpio2);
		goto free_data;
	}
	pr_info("hw_ver: ver_gpio0 = %d, ver_gpio1 = %d, ver_gpio2 = %d\n", pdata->ver_gpio0, pdata->ver_gpio1, pdata->ver_gpio2);

	rc = gpio_request(pdata->ver_gpio0, "ver_gpio0");
	if (rc)
		goto err_request_gpio0;
	rc = gpio_request(pdata->ver_gpio1, "ver_gpio1");
	if (rc)
		goto err_request_gpio1;
	rc = gpio_request(pdata->ver_gpio2, "ver_gpio2");
	if (rc)
		goto err_sysfs_create;

	gpio_direction_input(pdata->ver_gpio0);
	gpio_direction_input(pdata->ver_gpio1);
	gpio_direction_input(pdata->ver_gpio2);

	rc = sysfs_create_group(&pdev->dev.kobj, &hw_ver_attr_group);
	if (rc) {
		pr_err("Unable to create sysfs for hw_ver, errors: %d\n", rc);
		goto err_sysfs_create;
	}

	return 0;

err_sysfs_create:
	gpio_free(pdata->ver_gpio2);
err_request_gpio1:
	gpio_free(pdata->ver_gpio1);
err_request_gpio0:
	gpio_free(pdata->ver_gpio0);
free_data:
	kfree(pdata);
	return rc;
}

static int hw_ver_remove(struct platform_device *pdev)
{
	struct hw_ver_pdata *pdata;

	pdata = dev_get_drvdata(&pdev->dev);

	gpio_free(pdata->ver_gpio0);
	gpio_free(pdata->ver_gpio1);
	gpio_free(pdata->ver_gpio2);

	sysfs_remove_group(&pdev->dev.kobj, &hw_ver_attr_group);

	if(pdata != NULL)
		kfree(pdata);

	return 0;
}

static struct of_device_id hw_ver_of_match[] = {
	{ .compatible = "hw-version", },
	{ },
};

static struct platform_driver hw_ver_device_driver = {
	.probe		= hw_ver_probe,
	.remove		= hw_ver_remove,
	.driver		= {
		.name	= "hw-version",
		.owner	= THIS_MODULE,
		.of_match_table = hw_ver_of_match,
	}
};

static int __init hw_ver_init(void)
{
	return platform_driver_register(&hw_ver_device_driver);
}

static void __exit hw_ver_exit(void)
{
	platform_driver_unregister(&hw_ver_device_driver);
}

late_initcall(hw_ver_init);
module_exit(hw_ver_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Cheng Xuetao <chengxta@lenovo.com>");
MODULE_DESCRIPTION("Drivers for HW Version detected");
MODULE_ALIAS("platform:hw-version");

// end of file
