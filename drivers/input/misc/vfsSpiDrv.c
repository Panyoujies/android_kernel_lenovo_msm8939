/*! @file vfsSpiDrv.c
*******************************************************************************
**  SPI Driver Interface Functions
**
**  This file contains the SPI driver interface functions.
**
**  Copyright (C) 2011-2013 Validity Sensors, Inc.
**  This program is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License
**  as published by the Free Software Foundation; either version 2
**  of the License, or (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
*/
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mutex.h>

#ifndef CONFIG_FINGERPRINT_IN_QSEE
#include <linux/spi/spi.h>
#else
#include <linux/platform_device.h>
#include <linux/clk.h>
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/compat.h>
#include <asm-generic/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <dt-bindings/msm/msm-bus-ids.h>
#include "vfsSpiDrv.h"

#define VALIDITY_PART_NAME "validity_fingerprint"
#define VFSSPI_WAKE_TIME   (5 * HZ)

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_mutex);
static struct class *vfsspi_device_class;
static int gpio_irq;

#ifdef CONFIG_OF
static struct of_device_id validity_metallica_table[] = {
#ifndef CONFIG_FINGERPRINT_IN_QSEE
    { .compatible = "validity,metallica",},
#else
    { .compatible = "validity,metallicatee",},
#endif
    { },
};
#else
#define validity_metallica_table NULL
#endif


/*
 * vfsspi_devData - The spi driver private structure
 * @devt:Device ID
 * @vfs_spi_lock:The lock for the spi device
 * @spi:The spi device
 * @device_entry:Device entry list
 * @buffer_mutex:The lock for the transfer buffer
 * @is_opened:Indicates that driver is opened
 * @buffer:buffer for transmitting data
 * @null_buffer:buffer for transmitting zeros
 * @stream_buffer:buffer for transmitting data stream
 * @stream_buffer_size:streaming buffer size
 * @drdy_pin:DRDY GPIO pin number
 * @sleep_pin:Sleep GPIO pin number
 * @user_pid:User process ID, to which the kernel signal
 *    indicating DRDY event is to be sent
 * @signal_id:Signal ID which kernel uses to indicating
 *    user mode driver that DRDY is asserted
 * @current_spi_speed:Current baud rate of SPI master clock
 */
struct vfsspi_device_data {
    dev_t devt;
    struct cdev cdev;
    spinlock_t vfs_spi_lock;
#ifndef CONFIG_FINGERPRINT_IN_QSEE
    struct spi_device *spi;
#else
    struct platform_device *spi;
#endif
    struct list_head device_entry;
    struct mutex buffer_mutex;
    unsigned int is_opened;
    unsigned char *buffer;
    unsigned char *null_buffer;
    unsigned char *stream_buffer;
    size_t stream_buffer_size;
    unsigned int drdy_pin;
    unsigned int sleep_pin;
#if DO_CHIP_SELECT
    unsigned int cs_pin;
#endif
    int user_pid;
    int signal_id;
    unsigned int current_spi_speed;
    unsigned int is_drdy_irq_enabled;
    unsigned int drdy_ntf_type;
    struct mutex kernel_lock;

    /* clocks */
    struct clk           *core_clk;
    struct clk           *iface_clk;
    int                  clk_enabled;
    struct qup_i2c_clk_path_vote clk_path_vote;
    int                  master_id;
    int                  active_only;

    /* power */
    struct regulator     *vdd_reg;
    unsigned int         power_3p3v;
    int                  power_enabled;

    /* irq wake */
    int                  irq_wake_enabled;

    /* wake lock to ensoure fingerprint handled */
    struct wake_lock         wake_lock;
    int                      wake_lock_acquired;
    struct timer_list        wake_unlock_timer;

    /* work queue & worker */
    struct work_struct       irq_worker;
};

#ifdef VFSSPI_32BIT
/*
 * Used by IOCTL compat command:
 *         VFSSPI_IOCTL_RW_SPI_MESSAGE
 *
 * @rx_buffer:pointer to retrieved data
 * @tx_buffer:pointer to transmitted data
 * @len:transmitted/retrieved data size
 */
struct vfsspi_compat_ioctl_transfer {
    compat_uptr_t rx_buffer;
    compat_uptr_t tx_buffer;
    unsigned int len;
};
#endif

static int vfsspi_sendDrdyEventFd(struct vfsspi_device_data *vfsSpiDev);
static int vfsspi_sendDrdyNotify(struct vfsspi_device_data *vfsSpiDev);
static int vfsspi_ioctl_disable_irq_wake(struct vfsspi_device_data *data);
static void vfsspi_hardReset(struct vfsspi_device_data *vfsspi_device);

static void vfsspi_wake_unlock(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    if (data->wake_lock_acquired) {
        wake_unlock(&data->wake_lock);
        data->wake_lock_acquired = 0;
    }
}

static void vfsspi_wake_unlock_timer_handler(unsigned long ptr)
{
    struct vfsspi_device_data *data = (struct vfsspi_device_data*)ptr;

    pr_debug("%s: enter\n", __func__);
    vfsspi_wake_unlock(data);
}

static void vfsspi_wake_lock_delayed_unlock(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    if (!data->wake_lock_acquired) {
        wake_lock(&data->wake_lock);
        data->wake_lock_acquired = 1;
    }
    mod_timer(&data->wake_unlock_timer, jiffies + VFSSPI_WAKE_TIME);
}

#ifdef CONFIG_FINGERPRINT_IN_QSEE
static long vfsspi_clk_max_rate(struct clk *clk, unsigned long rate)
{
    long lowest_available, nearest_low, step_size, cur;
    long step_direction = -1;
    long guess = rate;
    int  max_steps = 10;

    pr_debug("%s: enter\n", __func__);

    cur =  clk_round_rate(clk, rate);
    if (cur == rate)
        return rate;

    /* if we got here then: cur > rate */
    lowest_available =  clk_round_rate(clk, 0);
    if (lowest_available > rate)
        return -EINVAL;

    step_size = (rate - lowest_available) >> 1;
    nearest_low = lowest_available;

    while (max_steps-- && step_size) {
        guess += step_size * step_direction;

        cur =  clk_round_rate(clk, guess);

        if ((cur < rate) && (cur > nearest_low))
            nearest_low = cur;

        /*
         * if we stepped too far, then start stepping in the other
         * direction with half the step size
         */
        if (((cur > rate) && (step_direction > 0))
         || ((cur < rate) && (step_direction < 0))) {
            step_direction = -step_direction;
            step_size >>= 1;
         }
    }
    return nearest_low;
}

static void vfsspi_clock_set(struct clk *clk, int speed)
{
    long rate;
    int rc;

    pr_debug("%s: enter\n", __func__);

    rate = vfsspi_clk_max_rate(clk, speed);
    if (rate < 0) {
        pr_err("%s: no match found for requested clock frequency:%d",
            __func__, speed);
        return;
    }

    printk("%s rate=%d\n", __func__, (int)rate);
    rc = clk_set_rate(clk, rate);
}

static void vfsspi_clk_path_vote(struct vfsspi_device_data *dd)
{
    pr_debug("%s: enter\n", __func__);

    if (dd->clk_path_vote.client_hdl)
        msm_bus_scale_client_update_request(
                        dd->clk_path_vote.client_hdl,
                        MSM_SPI_CLK_PATH_RESUME_VEC);
}

static void vfsspi_clk_path_unvote(struct vfsspi_device_data *dd)
{
    pr_debug("%s: enter\n", __func__);

    if (dd->clk_path_vote.client_hdl)
        msm_bus_scale_client_update_request(
                        dd->clk_path_vote.client_hdl,
                        MSM_SPI_CLK_PATH_SUSPEND_VEC);
}

static void vfsspi_clk_path_teardown(struct vfsspi_device_data *dd)
{
    pr_debug("%s: enter\n", __func__);

    if (dd->active_only)
        vfsspi_clk_path_unvote(dd);

    if (dd->clk_path_vote.client_hdl) {
        msm_bus_scale_unregister_client(dd->clk_path_vote.client_hdl);
        dd->clk_path_vote.client_hdl = 0;
    }
}

static int vfsspi_clk_path_init_structs(struct vfsspi_device_data *dd)
{
    struct msm_bus_vectors *paths    = NULL;
    struct msm_bus_paths   *usecases = NULL;

    pr_debug("%s: enter\n", __func__);

    paths = devm_kzalloc(&dd->spi->dev, sizeof(*paths) * 2, GFP_KERNEL);
    if (!paths) {
        pr_err("%s: msm_bus_paths.paths memory allocation failed\n", __func__);
        return -ENOMEM;
    }

    usecases = devm_kzalloc(&dd->spi->dev, sizeof(*usecases) * 2, GFP_KERNEL);
    if (!usecases) {
        pr_err("%s: msm_bus_scale_pdata.usecases memory allocation failed\n", __func__);
        goto path_init_err;
    }

    dd->clk_path_vote.pdata = devm_kzalloc(&dd->spi->dev,
                        sizeof(*dd->clk_path_vote.pdata),
                        GFP_KERNEL);
    if (!dd->clk_path_vote.pdata) {
        pr_err("%s: msm_bus_scale_pdata memory allocation failed\n", __func__);
        goto path_init_err;
    }

    paths[MSM_SPI_CLK_PATH_SUSPEND_VEC] = (struct msm_bus_vectors) {
        .src = dd->master_id,
        .dst = MSM_BUS_SLAVE_EBI_CH0,
        .ab  = 0,
        .ib  = 0,
    };

    paths[MSM_SPI_CLK_PATH_RESUME_VEC]  = (struct msm_bus_vectors) {
        .src = dd->master_id,
        .dst = MSM_BUS_SLAVE_EBI_CH0,
        .ab  = MSM_SPI_CLK_PATH_AVRG_BW(dd),
        .ib  = MSM_SPI_CLK_PATH_BRST_BW(dd),
    };

    usecases[MSM_SPI_CLK_PATH_SUSPEND_VEC] = (struct msm_bus_paths) {
        .num_paths = 1,
        .vectors   = &paths[MSM_SPI_CLK_PATH_SUSPEND_VEC],
    };

    usecases[MSM_SPI_CLK_PATH_RESUME_VEC] = (struct msm_bus_paths) {
        .num_paths = 1,
        .vectors   = &paths[MSM_SPI_CLK_PATH_RESUME_VEC],
    };

    *dd->clk_path_vote.pdata = (struct msm_bus_scale_pdata) {
        .active_only  = dd->active_only,
        .name         = dev_name(&dd->spi->dev),
        .num_usecases = 2,
        .usecase      = usecases,
    };

    return 0;

path_init_err:
    devm_kfree(&dd->spi->dev, paths);
    devm_kfree(&dd->spi->dev, usecases);
    devm_kfree(&dd->spi->dev, dd->clk_path_vote.pdata);
    dd->clk_path_vote.pdata = NULL;
    return -ENOMEM;
}

static int vfsspi_clk_path_postponed_register(struct vfsspi_device_data *dd)
{
    pr_debug("%s: enter\n", __func__);

    dd->clk_path_vote.client_hdl = msm_bus_scale_register_client(
                        dd->clk_path_vote.pdata);

    if (dd->clk_path_vote.client_hdl) {
        if (dd->clk_path_vote.reg_err) {
            /* log a success message if an error msg was logged */
            dd->clk_path_vote.reg_err = false;
            pr_info("%s: msm_bus_scale_register_client(mstr-id:%d "
                "actv-only:%d):0x%x\n",
                __func__,
                dd->master_id, dd->active_only,
                dd->clk_path_vote.client_hdl);
        }

        if (dd->active_only)
            vfsspi_clk_path_vote(dd);
    } else {
        /* guard to log only one error on multiple failure */
        if (!dd->clk_path_vote.reg_err) {
            dd->clk_path_vote.reg_err = true;

            pr_info("%s: msm_bus_scale_register_client(mstr-id:%d "
                "actv-only:%d):0",
                __func__,
                dd->master_id, dd->active_only);
        }
    }

    return dd->clk_path_vote.client_hdl ? 0 : -EAGAIN;
}

static void vfsspi_clk_path_init(struct vfsspi_device_data *dd)
{
    pr_debug("%s: enter\n", __func__);

    /*
     * bail out if path voting is diabled (master_id == 0) or if it is
     * already registered (client_hdl != 0)
     */
    if (!dd->master_id || dd->clk_path_vote.client_hdl)
        return;

    /* if fail once then try no more */
    if (!dd->clk_path_vote.pdata && vfsspi_clk_path_init_structs(dd)) {
        dd->master_id = 0;
        return;
    };

    /* on failure try again later */
    if (vfsspi_clk_path_postponed_register(dd))
        return;

    if (dd->active_only)
        vfsspi_clk_path_vote(dd);
}

static int vfsspi_ioctl_clk_init(struct platform_device *spi, struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    data->clk_enabled = 0;
    data->core_clk = clk_get(&spi->dev, "core_clk");
    if (IS_ERR_OR_NULL(data->core_clk)) {
        pr_err("%s: fail to get core_clk\n", __func__);
        return -1;
    }
    data->iface_clk = clk_get(&spi->dev, "iface_clk");
    if (IS_ERR_OR_NULL(data->iface_clk)) {
        pr_err("%s: fail to get iface_clk\n", __func__);
        clk_put(data->core_clk);
        data->core_clk = NULL;
        return -2;
    }
    vfsspi_clk_path_init(data);
    return 0;
}

static int vfsspi_ioctl_clk_enable(struct vfsspi_device_data *data)
{
    int err;

    pr_debug("%s: enter\n", __func__);

    if (data->clk_enabled) {
        return 0;
    }

    vfsspi_clk_path_vote(data);
    err = clk_prepare_enable(data->core_clk);
    if (err) {
        pr_err("%s: fail to enable core_clk\n", __func__);
        return -1;
    }

    err = clk_prepare_enable(data->iface_clk);
    if (err) {
        pr_err("%s: fail to enable iface_clk\n", __func__);
        clk_disable_unprepare(data->core_clk);
        return -2;
    }

    data->clk_enabled = 1;

    return 0;
}

static int vfsspi_ioctl_clk_disable(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    if (!data->clk_enabled) {
        return 0;
    }

    clk_disable_unprepare(data->core_clk);
    clk_disable_unprepare(data->iface_clk);
    data->clk_enabled = 0;
    vfsspi_clk_path_unvote(data);

    return 0;
}

static int vfsspi_ioctl_clk_uninit(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    if (data->clk_enabled) {
        vfsspi_ioctl_clk_disable(data);
    }

    if (!IS_ERR_OR_NULL(data->core_clk)) {
        clk_put(data->core_clk);
        data->core_clk = NULL;
    }

    if (!IS_ERR_OR_NULL(data->iface_clk)) {
        clk_put(data->iface_clk);
        data->iface_clk = NULL;
    }
    vfsspi_clk_path_teardown(data);

    return 0;
}
#endif

static int vfsspi_ioctl_power_init(struct platform_device *spi, struct vfsspi_device_data *data)
{
    int err = 0;

    pr_debug("%s: enter\n", __func__);

    data->power_enabled = 0;
    data->power_3p3v = VFSSPI_PW3P3V_PIN;
    data->power_3p3v = of_get_named_gpio(spi->dev.of_node, "synaptics,gpio_3p3v", 0);
    if (data->power_3p3v == 0) {
        pr_err("%s: fail to get power_3p3v\n", __func__);
        return -1;
    }

    err = gpio_direction_output(data->power_3p3v, 1);
    if (err < 0) {
        pr_err("%s: fail to set output for power_3p3v\n", __func__);
        return -2;
    }

    data->vdd_reg = regulator_get(&spi->dev, "vdd");
    if (IS_ERR_OR_NULL(data->vdd_reg)) {
        pr_err("%s: fail to get 1.8v LDO\n", __func__);
        return -3;
    }

    if (regulator_count_voltages(data->vdd_reg) > 0) {
        err = regulator_set_voltage(data->vdd_reg, 1800000, 1800000);
        if (err) {
            pr_err("%s: regulator set_vtg vdd_reg failed rc=%d\n", __func__, err);
            regulator_put(data->vdd_reg);
            return -4;
        }
    }

    err = regulator_set_optimum_mode(data->vdd_reg, 20000);
    if (err < 0) {
        pr_err("%s: set_optimum_mode vdd_reg failed, rc=%d\n", __func__, err);
        regulator_put(data->vdd_reg);
        return -5;
    }

    return 0;
}

static int vfsspi_ioctl_power_on(struct vfsspi_device_data *data)
{
    int err;

    pr_debug("%s: enter\n", __func__);

    if (data->power_enabled) {
        return 0;
    }

    gpio_set_value(data->power_3p3v, 1);
    err = regulator_enable(data->vdd_reg);
    if (err) {
        pr_err("%s: Regulator vdd_reg enable failed rc=%d\n", __func__, err);
        gpio_set_value(data->power_3p3v, 0);
        return -1;
    }

    data->power_enabled = 1;

    return 0;
}

static int vfsspi_ioctl_power_off(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    // DO NOT enable the irq wake, otherwise system can't suspend
    vfsspi_ioctl_disable_irq_wake(data);

    if (!data->power_enabled) {
        return 0;
    }

    regulator_disable(data->vdd_reg);
    gpio_set_value(data->power_3p3v, 0);
    data->power_enabled = 0;

    return 0;
}

static int vfsspi_ioctl_power_uninit(struct vfsspi_device_data *data)
{
    if (data->power_enabled) {
        vfsspi_ioctl_power_off(data);
    }

    if (!IS_ERR_OR_NULL(data->vdd_reg)) {
        regulator_put(data->vdd_reg);
        data->vdd_reg = NULL;
    }

    return 0;
}

static int vfsspi_ioctl_enable_irq_wake(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    // make sure that power is enabled
    if (!data->power_enabled) {
        pr_err("%s: power is off, DO NOT enable irq wake.\n", __func__);
        vfsspi_ioctl_disable_irq_wake(data);
        return -1;
    }

    // make sure that interrupt is enabled
    if (data->is_drdy_irq_enabled != DRDY_IRQ_ENABLE) {
        pr_err("%s: drdy irq is disabled, DO NOT enable irq wake.\n", __func__);
        vfsspi_ioctl_disable_irq_wake(data);
        return -2;
    }

    if (data->irq_wake_enabled) {
        return 0;
    }

    if (enable_irq_wake(gpio_irq)) {
        pr_err("%s: fail to enable_irq_wake\n", __func__);
        return -3;
    }

    data->irq_wake_enabled = 1;

    return 0;
}

static int vfsspi_ioctl_disable_irq_wake(struct vfsspi_device_data *data)
{
    pr_debug("%s: enter\n", __func__);

    if (!data->irq_wake_enabled) {
        return 0;
    }

    if (disable_irq_wake(gpio_irq)) {
        pr_err("%s: fail to disable_irq_wake\n", __func__);
        return -1;
    }

    data->irq_wake_enabled = 0;

    return 0;
}

#ifndef CONFIG_FINGERPRINT_IN_QSEE
static int vfsspi_drv_suspend(struct spi_device* spi, pm_message_t msg)
#else
static int vfsspi_drv_suspend(struct platform_device* spi, pm_message_t msg)
#endif
{
    struct vfsspi_device_data *vfsspi_device;

    pr_debug("%s: enter\n", __func__);

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    vfsspi_device = spi_get_drvdata(spi);
#else
    vfsspi_device = platform_get_drvdata(spi);
#endif
    vfsspi_ioctl_clk_disable(vfsspi_device);

    return 0;
}

#ifndef CONFIG_FINGERPRINT_IN_QSEE
static int vfsspi_drv_resume(struct spi_device* spi)
#else
static int vfsspi_drv_resume(struct platform_device* spi)
#endif
{
    struct vfsspi_device_data *vfsspi_device;

    pr_debug("%s: enter\n", __func__);

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    vfsspi_device = spi_get_drvdata(spi);
#else
    vfsspi_device = platform_get_drvdata(spi);
#endif
    vfsspi_ioctl_clk_enable(vfsspi_device);
    if (!vfsspi_device->power_enabled) {
        vfsspi_ioctl_power_on(vfsspi_device);
        mdelay(15);
        vfsspi_hardReset(vfsspi_device);
    }

    return 0;
}

static void vfsspi_irq_worker(struct work_struct *arg)
{
    struct vfsspi_device_data *vfsspi_device = container_of(arg, struct vfsspi_device_data, irq_worker);

    vfsspi_wake_lock_delayed_unlock(vfsspi_device);
    vfsspi_sendDrdyNotify(vfsspi_device);
    printk("%s: exit\n", __func__);
}

static int vfsspi_send_drdy_signal(struct vfsspi_device_data *vfsspi_device)
{
    struct task_struct *t;
    int ret = 0;

    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device->user_pid != 0) {
        rcu_read_lock();
        /* find the task_struct associated with userpid */
        pr_debug("%s: Searching task with PID=%08x\n",
            __func__, vfsspi_device->user_pid);
        t = pid_task(find_pid_ns(vfsspi_device->user_pid, &init_pid_ns),
                 PIDTYPE_PID);
        if (t == NULL) {
            pr_debug("%s: No such pid\n", __func__);
            rcu_read_unlock();
            return -ENODEV;
        }
        rcu_read_unlock();
        /* notify DRDY signal to user process */
        ret = send_sig_info(vfsspi_device->signal_id,
                    (struct siginfo *)1, t);
        if (ret < 0)
            pr_err("%s: Error sending signal\n", __func__);

    } else {
        pr_err("%s: pid not received yet\n", __func__);
    }

    return ret;
}

#ifndef CONFIG_FINGERPRINT_IN_QSEE
/* Return no. of bytes written to device. Negative number for errors */
static inline ssize_t vfsspi_writeSync(struct vfsspi_device_data *vfsspi_device,
                    size_t len)
{
    int    status = 0;
    struct spi_message m;
    struct spi_transfer t;

    pr_debug("%s: enter\n", __func__);

    spi_message_init(&m);
    memset(&t, 0, sizeof(t));

    t.rx_buf = vfsspi_device->null_buffer;
    t.tx_buf = vfsspi_device->buffer;
    t.len = len;
    t.speed_hz = vfsspi_device->current_spi_speed;

    spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
    status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
    if (status == 0)
        status = m.actual_length;
    pr_debug("%s: length=%d\n", __func__, (int)m.actual_length);
    return status;
}

/* Return no. of bytes read > 0. negative integer incase of error. */
static inline ssize_t vfsspi_readSync(struct vfsspi_device_data *vfsspi_device,
                    size_t len)
{
    int    status = 0;
    struct spi_message m;
    struct spi_transfer t;

    pr_debug("%s: enter\n", __func__);

    spi_message_init(&m);
    memset(&t, 0x0, sizeof(t));

    memset(vfsspi_device->null_buffer, 0x0, len);
    t.tx_buf = vfsspi_device->null_buffer;
    t.rx_buf = vfsspi_device->buffer;
    t.len = len;
    t.speed_hz = vfsspi_device->current_spi_speed;

    spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
    status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
    if (status == 0)
        status = len;

    pr_debug("%s: length=%d\n", __func__, (int)len);

    return status;
}

static ssize_t vfsspi_write(struct file *filp, const char __user *buf,
            size_t count, loff_t *fPos)
{
    struct vfsspi_device_data *vfsspi_device = NULL;
    ssize_t               status = 0;

    pr_debug("%s: enter\n", __func__);

    if (count > DEFAULT_BUFFER_SIZE || count <= 0)
        return -EMSGSIZE;

    vfsspi_device = filp->private_data;

    mutex_lock(&vfsspi_device->buffer_mutex);

    if (vfsspi_device->buffer) {
        unsigned long missing = 0;

        missing = copy_from_user(vfsspi_device->buffer, buf, count);

        if (missing == 0)
            status = vfsspi_writeSync(vfsspi_device, count);
        else
            status = -EFAULT;
    }

    mutex_unlock(&vfsspi_device->buffer_mutex);

    return status;
}

static ssize_t vfsspi_read(struct file *filp, char __user *buf,
            size_t count, loff_t *fPos)
{
    struct vfsspi_device_data *vfsspi_device = NULL;
    ssize_t                status    = 0;

    pr_debug("%s: enter\n", __func__);

    if (count > DEFAULT_BUFFER_SIZE || count <= 0)
        return -EMSGSIZE;
    if (buf == NULL)
        return -EFAULT;


    vfsspi_device = filp->private_data;

    mutex_lock(&vfsspi_device->buffer_mutex);

    status  = vfsspi_readSync(vfsspi_device, count);


    if (status > 0) {
        unsigned long missing = 0;
        /* data read. Copy to user buffer.*/
        missing = copy_to_user(buf, vfsspi_device->buffer, status);

        if (missing == status) {
            pr_err("%s: copy_to_user failed\n", __func__);
            /* Nothing was copied to user space buffer. */
            status = -EFAULT;
        } else {
            status = status - missing;
        }
    }

    mutex_unlock(&vfsspi_device->buffer_mutex);

    return status;
}

static int vfsspi_xfer(struct vfsspi_device_data *vfsspi_device,
            struct vfsspi_ioctl_transfer *tr)
{
    int status = 0;
    struct spi_message m;
    struct spi_transfer t;

    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device == NULL || tr == NULL)
        return -EFAULT;

    if (tr->len > DEFAULT_BUFFER_SIZE || tr->len <= 0)
        return -EMSGSIZE;

    if (tr->tx_buffer != NULL) {

        if (copy_from_user(vfsspi_device->null_buffer,
                tr->tx_buffer, tr->len) != 0)
            return -EFAULT;
    }

    spi_message_init(&m);
    memset(&t, 0, sizeof(t));

    t.tx_buf = vfsspi_device->null_buffer;
    t.rx_buf = vfsspi_device->buffer;
    t.len = tr->len;
    t.speed_hz = vfsspi_device->current_spi_speed;

    spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
    status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
    if (status == 0) {
        if (tr->rx_buffer != NULL) {
            unsigned missing = 0;

            missing = copy_to_user(tr->rx_buffer,
                           vfsspi_device->buffer, tr->len);

            if (missing != 0)
                tr->len = tr->len - missing;
        }
    }
    pr_debug("%s: length=%d\n", __func__, (int)tr->len);
    return status;

} /* vfsspi_xfer */

static int vfsspi_rw_spi_message(struct vfsspi_device_data *vfsspi_device,
                 unsigned long arg)
{
    struct vfsspi_ioctl_transfer   *dup  = NULL;
#ifdef VFSSPI_32BIT
    struct vfsspi_compat_ioctl_transfer   dup_compat;
#endif

    pr_debug("%s: enter\n", __func__);

    dup = kmalloc(sizeof(struct vfsspi_ioctl_transfer), GFP_KERNEL);
    if (dup == NULL)
        return -ENOMEM;
#ifdef VFSSPI_32BIT
    if (copy_from_user(&dup_compat, (void __user *)arg,
               sizeof(struct vfsspi_compat_ioctl_transfer)) != 0)  {
#else
    if (copy_from_user(dup, (void __user *)arg,
               sizeof(struct vfsspi_ioctl_transfer)) != 0)  {
#endif
        return -EFAULT;
    } else {
        int err;
#ifdef VFSSPI_32BIT
        dup->rx_buffer = (unsigned char *)(unsigned long)dup_compat.rx_buffer;
        dup->tx_buffer = (unsigned char *)(unsigned long)dup_compat.tx_buffer;
        dup->len = dup_compat.len;
#endif
        err = vfsspi_xfer(vfsspi_device, dup);
        if (err != 0) {
            kfree(dup);
            return err;
        }
    }
#ifdef VFSSPI_32BIT
    dup_compat.len = dup->len;
    if (copy_to_user((void __user *)arg, &dup_compat,
             sizeof(struct vfsspi_compat_ioctl_transfer)) != 0){
#else
    if (copy_to_user((void __user *)arg, dup,
             sizeof(struct vfsspi_ioctl_transfer)) != 0){
#endif
        kfree(dup);
        return -EFAULT;
    }
    kfree(dup);
    return 0;
}
#endif

static int vfsspi_set_clk(struct vfsspi_device_data *vfsspi_device,
              unsigned long arg)
{
    unsigned short clock = 0;

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    struct spi_device *spidev = NULL;

    pr_debug("%s: enter\n", __func__);

    if (copy_from_user(&clock, (void __user *)arg,
               sizeof(unsigned short)) != 0)
        return -EFAULT;

    spin_lock_irq(&vfsspi_device->vfs_spi_lock);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
    spidev = spi_dev_get(vfsspi_device->spi);
#if DO_CHIP_SELECT
    gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
    spin_unlock_irq(&vfsspi_device->vfs_spi_lock);
    if (spidev != NULL) {
        switch (clock) {
        case 0:    /* Running baud rate. */
            pr_debug("%s: Running baud rate.\n", __func__);
            spidev->max_speed_hz = MAX_BAUD_RATE;
            vfsspi_device->current_spi_speed = MAX_BAUD_RATE;
            break;
        case 0xFFFF: /* Slow baud rate */
            pr_debug("%s: slow baud rate.\n", __func__);
            spidev->max_speed_hz = SLOW_BAUD_RATE;
            vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
            break;
        default:
            pr_debug("%s: baud rate is %d.\n", __func__, (int)clock);
            vfsspi_device->current_spi_speed =
                clock * BAUD_RATE_COEF;
            if (vfsspi_device->current_spi_speed > MAX_BAUD_RATE)
                vfsspi_device->current_spi_speed =
                    MAX_BAUD_RATE;
            spidev->max_speed_hz = vfsspi_device->current_spi_speed;
            break;
        }
        spi_dev_put(spidev);
    }
#else
    pr_debug("%s: enter\n", __func__);

    if (copy_from_user(&clock, (void __user *)arg,
               sizeof(unsigned short)) != 0)
        return -EFAULT;

    if (vfsspi_device->clk_enabled) {
        switch (clock) {
        case 0:    /* Running baud rate. */
            pr_debug("%s: Running baud rate.\n", __func__);
            vfsspi_device->current_spi_speed = MAX_BAUD_RATE;
            break;
        case 0xFFFF: /* Slow baud rate */
            pr_debug("%s: slow baud rate.\n", __func__);
            vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
            break;
        default:
            pr_debug("%s: baud rate is %d.\n", __func__, (int)clock);
            vfsspi_device->current_spi_speed =
                clock * BAUD_RATE_COEF;
            if (vfsspi_device->current_spi_speed > MAX_BAUD_RATE)
                vfsspi_device->current_spi_speed =
                    MAX_BAUD_RATE;
            break;
        }
        vfsspi_clock_set(vfsspi_device->core_clk, vfsspi_device->current_spi_speed);
    }
#endif
    return 0;
}

static int vfsspi_register_drdy_signal(struct vfsspi_device_data *vfsspi_device,
                       unsigned long arg)
{
    struct vfsspi_ioctl_register_signal usr_signal;
    if (copy_from_user(&usr_signal, (void __user *)arg, sizeof(usr_signal)) != 0) {
        pr_err("%s: Failed copy from user.\n", __func__);
        return -EFAULT;
    } else {
        vfsspi_device->user_pid = usr_signal.user_pid;
        vfsspi_device->signal_id = usr_signal.signal_id;
    }
    return 0;
}

static irqreturn_t vfsspi_irq(int irq, void *context)
{
    struct vfsspi_device_data *vfsspi_device = context;

    pr_debug("%s: enter\n", __func__);

    /* Linux kernel is designed so that when you disable
    an edge-triggered interrupt, and the edge happens while
    the interrupt is disabled, the system will re-play the
    interrupt at enable time.
    Therefore, we are checking DRDY GPIO pin state to make sure
    if the interrupt handler has been called actually by DRDY
    interrupt and it's not a previous interrupt re-play */
    if (gpio_get_value(vfsspi_device->drdy_pin) == DRDY_ACTIVE_STATUS) {
        schedule_work(&vfsspi_device->irq_worker);
    }

    return IRQ_HANDLED;
}

static int vfsspi_sendDrdyEventFd(struct vfsspi_device_data *vfsSpiDev)
{
    struct task_struct *t;
    struct file *efd_file = NULL;
    struct eventfd_ctx *efd_ctx = NULL;    int ret = 0;

    pr_debug("%s: enter\n", __func__);

    if (vfsSpiDev->user_pid != 0) {
        rcu_read_lock();
        /* find the task_struct associated with userpid */
        pr_debug("%s: Searching task with PID=%08x\n", __func__, vfsSpiDev->user_pid);
        t = pid_task(find_pid_ns(vfsSpiDev->user_pid, &init_pid_ns),
            PIDTYPE_PID);
        if (t == NULL) {
            pr_debug("%s: No such pid\n", __func__);
            rcu_read_unlock();
            return -ENODEV;
        }
        efd_file = fcheck_files(t->files, vfsSpiDev->signal_id);
        rcu_read_unlock();

        if (efd_file == NULL) {
            pr_debug("%s: No such efd_file\n", __func__);
            return -ENODEV;
        }

        efd_ctx = eventfd_ctx_fileget(efd_file);
        if (efd_ctx == NULL) {
            pr_debug("%s: eventfd_ctx_fileget is failed\n", __func__);
            return -ENODEV;
        }

        /* notify DRDY eventfd to user process */
        eventfd_signal(efd_ctx, 1);

        /* Release eventfd context */
        eventfd_ctx_put(efd_ctx);
    }

    return ret;
}

static int vfsspi_sendDrdyNotify(struct vfsspi_device_data *vfsSpiDev)
{
    int ret = 0;

    pr_debug("%s: enter\n", __func__);

    if (vfsSpiDev->drdy_ntf_type == VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD) {
        ret = vfsspi_sendDrdyEventFd(vfsSpiDev);
    } else {
        ret = vfsspi_send_drdy_signal(vfsSpiDev);
    }

    return ret;
}

static int vfsspi_enableIrq(struct vfsspi_device_data *vfsspi_device)
{
    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device->is_drdy_irq_enabled == DRDY_IRQ_ENABLE) {
        pr_debug("%s: DRDY irq already enabled\n", __func__);
        return -EINVAL;
    }

    enable_irq(gpio_irq);
    vfsspi_device->is_drdy_irq_enabled = DRDY_IRQ_ENABLE;
    vfsspi_ioctl_enable_irq_wake(vfsspi_device);

    return 0;
}

static int vfsspi_disableIrq(struct vfsspi_device_data *vfsspi_device)
{
    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device->is_drdy_irq_enabled == DRDY_IRQ_DISABLE) {
        pr_debug("%s: DRDY irq already disabled\n", __func__);
        return -EINVAL;
    }

    disable_irq_nosync(gpio_irq);
    vfsspi_ioctl_disable_irq_wake(vfsspi_device);
    vfsspi_device->is_drdy_irq_enabled = DRDY_IRQ_DISABLE;

    return 0;
}
static int vfsspi_set_drdy_int(struct vfsspi_device_data *vfsspi_device,
                   unsigned long arg)
{
    unsigned short drdy_enable_flag;
    if (copy_from_user(&drdy_enable_flag, (void __user *)arg,
               sizeof(drdy_enable_flag)) != 0) {
        pr_err("%s: Failed copy from user.\n", __func__);
        return -EFAULT;
    }
    if (drdy_enable_flag == 0) {
        vfsspi_wake_lock_delayed_unlock(vfsspi_device);
        vfsspi_disableIrq(vfsspi_device);
    } else {
        vfsspi_enableIrq(vfsspi_device);
        /* Workaround the issue where the system
          misses DRDY notification to host when
          DRDY pin was asserted before enabling
          device.*/
        if (gpio_get_value(vfsspi_device->drdy_pin) ==
            DRDY_ACTIVE_STATUS) {
            vfsspi_sendDrdyNotify(vfsspi_device);
        }
        //vfsspi_wake_unlock(vfsspi_device);
    }
    return 0;
}

static void vfsspi_hardReset(struct vfsspi_device_data *vfsspi_device)
{
    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device != NULL) {
        gpio_set_value(vfsspi_device->sleep_pin, 0);
        mdelay(1);
        gpio_set_value(vfsspi_device->sleep_pin, 1);
        mdelay(5);
    }
}


static void vfsspi_suspend(struct vfsspi_device_data *vfsspi_device)
{
    pr_debug("%s: enter\n", __func__);

    if (vfsspi_device != NULL) {
        spin_lock(&vfsspi_device->vfs_spi_lock);
        gpio_set_value(vfsspi_device->sleep_pin, 0);
        spin_unlock(&vfsspi_device->vfs_spi_lock);
    }
}

static long vfsspi_ioctl(struct file *filp, unsigned int cmd,
            unsigned long arg)
{
    int ret_val = 0;
    struct vfsspi_device_data *vfsspi_device = NULL;

    pr_debug("%s: enter\n", __func__);

    if (_IOC_TYPE(cmd) != VFSSPI_IOCTL_MAGIC) {
        pr_err("%s: invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
            __func__, cmd, _IOC_TYPE(cmd), VFSSPI_IOCTL_MAGIC);
        return -ENOTTY;
    }

    vfsspi_device = filp->private_data;
    mutex_lock(&vfsspi_device->buffer_mutex);
    switch (cmd) {
    case VFSSPI_IOCTL_POWER_ON:
        printk("%s: VFSSPI_IOCTL_POWER_ON\n", __func__);
        ret_val = 0;
        if (!vfsspi_device->power_enabled) {
            ret_val = vfsspi_ioctl_power_on(vfsspi_device);
            mdelay(15);
            vfsspi_hardReset(vfsspi_device);
        }
        break;
    case VFSSPI_IOCTL_POWER_OFF:
        // keep wake lock for some special senario
        vfsspi_wake_lock_delayed_unlock(vfsspi_device);
        printk("%s: VFSSPI_IOCTL_POWER_OFF\n", __func__);
        ret_val = vfsspi_ioctl_power_off(vfsspi_device);
        break;
    case VFSSPI_IOCTL_SET_SPI_CONFIGURATION:
        printk("%s: VFSSPI_IOCTL_SET_SPI_CONFIGURATION\n", __func__);
#ifdef CONFIG_FINGERPRINT_IN_QSEE
        ret_val = vfsspi_ioctl_clk_enable(vfsspi_device);
#endif
        break;
    case VFSSPI_IOCTL_RESET_SPI_CONFIGURATION:
        // keep wake lock for some special senario
        vfsspi_wake_lock_delayed_unlock(vfsspi_device);
        printk("%s: VFSSPI_IOCTL_RESET_SPI_CONFIGURATION\n", __func__);
#if 0
#ifdef CONFIG_FINGERPRINT_IN_QSEE
        ret_val = vfsspi_ioctl_clk_disable(vfsspi_device);
#endif
#endif
        break;
    case VFSSPI_IOCTL_DEVICE_RESET:
        printk("%s: VFSSPI_IOCTL_DEVICE_RESET:\n", __func__);
        vfsspi_hardReset(vfsspi_device);
        ret_val = 0;
        break;
    case VFSSPI_IOCTL_DEVICE_SUSPEND:
    {
        printk("%s: VFSSPI_IOCTL_DEVICE_SUSPEND:\n", __func__);
        vfsspi_suspend(vfsspi_device);
        ret_val = 0;
        break;
    }
#ifndef CONFIG_FINGERPRINT_IN_QSEE
    case VFSSPI_IOCTL_RW_SPI_MESSAGE:
        pr_debug("VFSSPI_IOCTL_RW_SPI_MESSAGE");
        ret_val = vfsspi_rw_spi_message(vfsspi_device, arg);
        break;
#endif
    case VFSSPI_IOCTL_SET_CLK:
        printk("%s: VFSSPI_IOCTL_SET_CLK\n", __func__);
        ret_val = vfsspi_set_clk(vfsspi_device, arg);
        break;
    case VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL:
        printk("%s: VFSSPI_IOCTL_REGISTER_DRDY_SIGNAL, %08x\n", __func__, (unsigned int)arg);
        ret_val = vfsspi_register_drdy_signal(vfsspi_device, arg);
        break;
    case VFSSPI_IOCTL_SET_DRDY_INT:
        printk("%s: VFSSPI_IOCTL_SET_DRDY_INT, %08x\n", __func__, (unsigned int)arg);
        ret_val = vfsspi_set_drdy_int(vfsspi_device, arg);
        break;
    case VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE:
        {
            vfsspi_iocSelectDrdyNtfType_t drdyTypes;

            printk("%s: VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE\n", __func__);

            if (copy_from_user(&drdyTypes, (void __user *)arg,
                sizeof(vfsspi_iocSelectDrdyNtfType_t)) != 0) {
                    pr_debug("%s: copy from user failed.\n", __func__);
                    ret_val = -EFAULT;
            } else {
                if (0 != (drdyTypes.supportedTypes & VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD)) {
                    vfsspi_device->drdy_ntf_type = VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD;
                } else {
                    vfsspi_device->drdy_ntf_type = VFSSPI_DRDY_NOTIFY_TYPE_SIGNAL;
                }
                drdyTypes.selectedType = vfsspi_device->drdy_ntf_type;
                if (copy_to_user((void __user *)arg, &(drdyTypes),
                    sizeof(vfsspi_iocSelectDrdyNtfType_t)) == 0) {
                        ret_val = 0;
                } else {
                    pr_debug("%s: copy to user failed\n", __func__);
                }
            }
            break;
        }
    default:
        ret_val = -EFAULT;
        break;
    }
    mutex_unlock(&vfsspi_device->buffer_mutex);
    return ret_val;
}

static int vfsspi_open(struct inode *inode, struct file *filp)
{
    struct vfsspi_device_data *vfsspi_device = NULL;
    int status = -ENXIO;

    pr_debug("%s: enter\n", __func__);

    mutex_lock(&device_list_mutex);

    list_for_each_entry(vfsspi_device, &device_list, device_entry) {
        if (vfsspi_device->devt == inode->i_rdev) {
            status = 0;
            break;
        }
    }

    if (status == 0) {
        mutex_lock(&vfsspi_device->kernel_lock);
        if (vfsspi_device->is_opened != 0) {
            status = -EBUSY;
            pr_err("%s: is_opened != 0, -EBUSY\n", __func__);
            goto vfsspi_open_out;
        }
        vfsspi_device->user_pid = 0;
        vfsspi_device->drdy_ntf_type = VFSSPI_DRDY_NOTIFY_TYPE_SIGNAL;
        if (vfsspi_device->buffer != NULL) {
            pr_err("%s: buffer != NULL\n", __func__);
            goto vfsspi_open_out;
        }
        vfsspi_device->null_buffer =
            kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
        if (vfsspi_device->null_buffer == NULL) {
            status = -ENOMEM;
            pr_err("%s: null_buffer == NULL, -ENOMEM\n", __func__);
            goto vfsspi_open_out;
        }
        vfsspi_device->buffer =
            kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
        if (vfsspi_device->buffer == NULL) {
            status = -ENOMEM;
            kfree(vfsspi_device->null_buffer);
            pr_err("%s: buffer == NULL, -ENOMEM\n", __func__);
            goto vfsspi_open_out;
        }
        vfsspi_device->is_opened = 1;
        filp->private_data = vfsspi_device;
        nonseekable_open(inode, filp);

vfsspi_open_out:
        mutex_unlock(&vfsspi_device->kernel_lock);
    }
    mutex_unlock(&device_list_mutex);
    return status;
}


static int vfsspi_release(struct inode *inode, struct file *filp)
{
    struct vfsspi_device_data *vfsspi_device = NULL;
    int                   status     = 0;

    pr_debug("%s: enter\n", __func__);

    mutex_lock(&device_list_mutex);
    vfsspi_device = filp->private_data;
    filp->private_data = NULL;
    vfsspi_device->is_opened = 0;
    if (vfsspi_device->buffer != NULL) {
        kfree(vfsspi_device->buffer);
        vfsspi_device->buffer = NULL;
    }

    if (vfsspi_device->null_buffer != NULL) {
        kfree(vfsspi_device->null_buffer);
        vfsspi_device->null_buffer = NULL;
    }

    mutex_unlock(&device_list_mutex);
    return status;
}

/* file operations associated with device */
static const struct file_operations vfsspi_fops = {
    .owner   = THIS_MODULE,
#ifndef CONFIG_FINGERPRINT_IN_QSEE
    .write   = vfsspi_write,
    .read    = vfsspi_read,
#endif
    .unlocked_ioctl   = vfsspi_ioctl,
    .open    = vfsspi_open,
    .release = vfsspi_release,
};

#ifndef CONFIG_FINGERPRINT_IN_QSEE
static int vfsspi_probe(struct spi_device *spi)
#else
static int vfsspi_probe(struct platform_device *spi)
#endif
{
    int status = 0;
    struct vfsspi_device_data *vfsspi_device;
    struct device *dev;

    pr_info("%s: enter\n", __func__);

    vfsspi_device = kzalloc(sizeof(*vfsspi_device), GFP_KERNEL);
    if (vfsspi_device == NULL) {
        pr_err("%s: no memory to init driver data.\n", __func__);
        return -ENOMEM;
    }

    /* Initialize driver data. */
    vfsspi_device->irq_wake_enabled = 0;
    vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
    vfsspi_device->spi = spi;
    spin_lock_init(&vfsspi_device->vfs_spi_lock);
    mutex_init(&vfsspi_device->buffer_mutex);
    mutex_init(&vfsspi_device->kernel_lock);
    INIT_LIST_HEAD(&vfsspi_device->device_entry);
    vfsspi_device->master_id = 86;
    vfsspi_device->active_only = 0;

    /* init wake lock */
    vfsspi_device->wake_lock_acquired = 0;
    wake_lock_init(&vfsspi_device->wake_lock, WAKE_LOCK_SUSPEND, "fingerprint_wakelock");

    /* init work queue for interrupt */
    INIT_WORK(&vfsspi_device->irq_worker, vfsspi_irq_worker);
    init_timer(&vfsspi_device->wake_unlock_timer);
    vfsspi_device->wake_unlock_timer.expires = jiffies - 1;
    vfsspi_device->wake_unlock_timer.function = vfsspi_wake_unlock_timer_handler;
    vfsspi_device->wake_unlock_timer.data = (unsigned long)vfsspi_device;
    add_timer(&vfsspi_device->wake_unlock_timer);

#ifdef CONFIG_FINGERPRINT_IN_QSEE
    /* Enable spi clock */
    if (vfsspi_ioctl_clk_init(spi, vfsspi_device)) {
        goto vfsspi_probe_clk_init_failed;
    }
    if (vfsspi_ioctl_clk_enable(vfsspi_device)) {
        goto vfsspi_probe_clk_enable_failed;
    }
    vfsspi_clock_set(vfsspi_device->core_clk, MAX_BAUD_RATE);
#endif

    /* Power up for the sensor */
    if (vfsspi_ioctl_power_init(spi, vfsspi_device)) {
        goto vfsspi_probe_power_init_failed;

    }
    if (vfsspi_ioctl_power_on(vfsspi_device)) {
        goto vfsspi_probe_power_on_failed;
    }

    vfsspi_device->drdy_pin  = VFSSPI_DRDY_PIN;
    vfsspi_device->sleep_pin = VFSSPI_SLEEP_PIN;

#ifndef CONFIG_OF
    if (gpio_request(vfsspi_device->drdy_pin, "vfsspi_drdy") < 0) {
        status = -EBUSY;
        goto vfsspi_probe_drdy_failed;
    }

    if (gpio_request(vfsspi_device->sleep_pin, "vfsspi_sleep")) {
        status = -EBUSY;
        goto vfsspi_probe_sleep_failed;
    }
#else
    if(spi->dev.of_node){
        vfsspi_device->drdy_pin = of_get_named_gpio(spi->dev.of_node, "synaptics,gpio_drdy", 0);
        if(vfsspi_device->drdy_pin == 0) {
            pr_err("%s: Failed to get drdy_pin gpio.\n", __func__);
            return 0;
        }
        vfsspi_device->sleep_pin = of_get_named_gpio(spi->dev.of_node, "synaptics,gpio_sleep", 0);
        if(vfsspi_device->sleep_pin == 0) {
            pr_err("%s: Failed to get sleep_pin gpio.\n", __func__);
            return 0;
        }
    } else {
        pr_err("%s: init gpio failed.\n", __func__);
    }
#endif

#if DO_CHIP_SELECT
    pr_debug("%s: HANDLING CHIP SELECT\n", __func__);
    vfsspi_device->cs_pin  = VFSSPI_CS_PIN;
#ifndef CONFIG_OF
    if (gpio_request(vfsspi_device->cs_pin, "vfsspi_cs") < 0) {
        status = -EBUSY;
        goto vfsspi_probe_cs_failed;
    }
#else
    vfsspi_device->cs_pin = of_get_named_gpio(spi->dev.of_node, "synaptics,gpio_cs", 0);
    pr_info("%s: vfsspi_device->cs_pin = %d\n", __func__, (int)vfsspi_device->cs_pin);
    if(vfsspi_device->cs_pin == 0) {
        pr_err("%s: Failed to get cs_pin gpio.\n", __func__);
        return 0;
    }
#endif
    status = gpio_direction_output(vfsspi_device->cs_pin, 1);
    if (status < 0) {
        pr_err("%s: gpio_direction_input CS failed\n", __func__);
        status = -EBUSY;
        goto vfsspi_probe_gpio_init_failed;
    }
    gpio_set_value(vfsspi_device->cs_pin, 1);
#endif // end DO_CHIP_SELECT

    status = gpio_direction_output(vfsspi_device->sleep_pin, 1);
    if (status < 0) {
        pr_err("%s: gpio_direction_output SLEEP failed\n", __func__);
        status = -EBUSY;
        goto vfsspi_probe_gpio_init_failed;
    }

    status = gpio_direction_input(vfsspi_device->drdy_pin);
    if (status < 0) {
        pr_err("%s: gpio_direction_input DRDY failed\n", __func__);
        status = -EBUSY;
        goto vfsspi_probe_gpio_init_failed;
    }

    /* register interrupt */
    gpio_irq = gpio_to_irq(vfsspi_device->drdy_pin);
    if (gpio_irq < 0) {
        pr_err("%s: gpio_to_irq failed\n", __func__);
        status = -EBUSY;
        goto vfsspi_probe_gpio_init_failed;
    }

    if (request_irq(gpio_irq, vfsspi_irq, IRQF_TRIGGER_RISING,
            "vfsspi_irq", vfsspi_device) < 0) {
        pr_err("%s: request_irq failed\n", __func__);
        status = -EBUSY;
        goto vfsspi_probe_irq_failed;
    }

    vfsspi_device->is_drdy_irq_enabled = DRDY_IRQ_ENABLE;
#ifndef CONFIG_FINGERPRINT_IN_QSEE
    spi->bits_per_word = BITS_PER_WORD;
    spi->max_speed_hz = MAX_BAUD_RATE;
    spi->mode = SPI_MODE_0;

    status = spi_setup(spi);
    if (status != 0) {
        pr_err("%s: failed to setup spi\n", __func__);
        goto vfsspi_probe_failed;
    }
#endif

    mutex_lock(&device_list_mutex);
    /* Create device node */
    /* register major number for character device */
    status = alloc_chrdev_region(&(vfsspi_device->devt),
                     0, 1, VALIDITY_PART_NAME);
    if (status < 0) {
        pr_err("%s: alloc_chrdev_region failed\n", __func__);
        goto vfsspi_probe_alloc_chardev_failed;
    }

    cdev_init(&(vfsspi_device->cdev), &vfsspi_fops);
    vfsspi_device->cdev.owner = THIS_MODULE;
    status = cdev_add(&(vfsspi_device->cdev), vfsspi_device->devt, 1);
    if (status < 0) {
        pr_err("%s: cdev_add failed\n", __func__);
        unregister_chrdev_region(vfsspi_device->devt, 1);
        goto vfsspi_probe_cdev_add_failed;
    }

    vfsspi_device_class = class_create(THIS_MODULE, VALIDITY_PART_NAME);

    if (IS_ERR(vfsspi_device_class)) {
        pr_err("%s: class_create() is failed - unregister chrdev.\n", __func__);
        cdev_del(&(vfsspi_device->cdev));
        unregister_chrdev_region(vfsspi_device->devt, 1);
        status = PTR_ERR(vfsspi_device_class);
        goto vfsspi_probe_class_create_failed;
    }

    dev = device_create(vfsspi_device_class, &spi->dev,
                vfsspi_device->devt, vfsspi_device, "vfsspi");
    status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    if (status == 0)
        list_add(&vfsspi_device->device_entry, &device_list);
    mutex_unlock(&device_list_mutex);

    if (status != 0)  {
        pr_err("%s: vfsspi create status: %d\n", __func__, (int)status);
        goto vfsspi_probe_failed;
    }

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    spi_set_drvdata(spi, vfsspi_device);
#else
    platform_set_drvdata(spi, vfsspi_device);
#endif
    mdelay(15);
    vfsspi_hardReset(vfsspi_device);

    pr_info("%s: vfsspi_probe successful\n", __func__);

    return 0;

vfsspi_probe_failed:
vfsspi_probe_class_create_failed:
    cdev_del(&(vfsspi_device->cdev));
vfsspi_probe_cdev_add_failed:
    unregister_chrdev_region(vfsspi_device->devt, 1);
vfsspi_probe_alloc_chardev_failed:
vfsspi_probe_irq_failed:
    free_irq(gpio_irq, vfsspi_device);
vfsspi_probe_gpio_init_failed:
#if DO_CHIP_SELECT
#ifndef CONFIG_OF
        gpio_free(vfsspi_device->cs_pin);
vfsspi_probe_cs_failed:
#endif
#endif

#ifndef CONFIG_OF
    gpio_free(vfsspi_device->sleep_pin);
vfsspi_probe_sleep_failed:
    gpio_free(vfsspi_device->drdy_pin);
vfsspi_probe_drdy_failed:
#endif

vfsspi_probe_power_on_failed:
    vfsspi_ioctl_power_uninit(vfsspi_device);
vfsspi_probe_power_init_failed:
#ifdef CONFIG_FINGERPRINT_IN_QSEE
vfsspi_probe_clk_enable_failed:
    vfsspi_ioctl_clk_uninit(vfsspi_device);
vfsspi_probe_clk_init_failed:
#endif
    kfree(vfsspi_device);
    mutex_destroy(&vfsspi_device->buffer_mutex);
    mutex_destroy(&vfsspi_device->kernel_lock);
    pr_err("%s: vfsspi_probe failed!!\n", __func__);
    return status;
}

#ifndef CONFIG_FINGERPRINT_IN_QSEE
static int vfsspi_remove(struct spi_device *spi)
#else
static int vfsspi_remove(struct platform_device *spi)
#endif
{
    int status = 0;

    struct vfsspi_device_data *vfsspi_device = NULL;

    pr_debug("%s: enter\n", __func__);

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    vfsspi_device = spi_get_drvdata(spi);
#else
    vfsspi_device = platform_get_drvdata(spi);
#endif

    if (vfsspi_device != NULL) {
        spin_lock_irq(&vfsspi_device->vfs_spi_lock);
        vfsspi_device->spi = NULL;
#ifndef CONFIG_FINGERPRINT_IN_QSEE
        spi_set_drvdata(spi, NULL);
#else
        platform_set_drvdata(spi, NULL);
#endif
        spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

        mutex_lock(&device_list_mutex);

        free_irq(gpio_irq, vfsspi_device);

#ifndef CONFIG_OF
#if DO_CHIP_SELECT
        gpio_free(vfsspi_device->cs_pin);
#endif
        gpio_free(vfsspi_device->sleep_pin);
        gpio_free(vfsspi_device->drdy_pin);
#endif
        /* Remove device entry. */
        list_del(&vfsspi_device->device_entry);
        device_destroy(vfsspi_device_class, vfsspi_device->devt);
        class_destroy(vfsspi_device_class);
        cdev_del(&(vfsspi_device->cdev));
        unregister_chrdev_region(vfsspi_device->devt, 1);

        mutex_destroy(&vfsspi_device->buffer_mutex);
        mutex_destroy(&vfsspi_device->kernel_lock);

        kfree(vfsspi_device);
        mutex_unlock(&device_list_mutex);
    }

    return status;
}


#ifndef CONFIG_FINGERPRINT_IN_QSEE
struct spi_driver vfsspi_spi = {
#else
struct platform_driver vfsspi_spi = {
#endif
    .driver = {
        .name  = VALIDITY_PART_NAME,
        .owner = THIS_MODULE,
        .of_match_table = validity_metallica_table,
    },
    .probe  = vfsspi_probe,
    .remove = vfsspi_remove,
    .suspend  = vfsspi_drv_suspend,
    .resume = vfsspi_drv_resume,
};

static int __init vfsspi_init(void)
{
    int status = 0;

    pr_debug("%s: enter\n", __func__);

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    status = spi_register_driver(&vfsspi_spi);
#else
    status = platform_driver_register(&vfsspi_spi);
#endif
    if (status < 0) {
        pr_err("%s: register driver() is failed\n", __func__);
        return status;
    }
    pr_debug("%s: init is successful\n", __func__);

    return status;
}

static void __exit vfsspi_exit(void)
{
    pr_debug("%s: enter\n", __func__);

#ifndef CONFIG_FINGERPRINT_IN_QSEE
    spi_unregister_driver(&vfsspi_spi);
#else
    platform_driver_unregister(&vfsspi_spi);
#endif
}

module_init(vfsspi_init);
module_exit(vfsspi_exit);

MODULE_DESCRIPTION("Validity FPS sensor");
MODULE_LICENSE("GPL");
