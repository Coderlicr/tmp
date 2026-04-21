/*
 * top_module.c -- Fruit-Ninja DE1-SoC driver (ultrasonic version)
 *
 * Exposes /dev/top_module with ioctls:
 *   READ_ULTRA       - latch (status, last_echo_cnt, sample_id), clear new bit
 *   WRITE_FRUIT      - push fruit position/radius/visible to FPGA
 *   WRITE_SCORE      - set score register (drives VGA score bar)
 *   WRITE_GAME_STATE - set game-state register
 *
 * Platform device is bound via device tree:
 *   compatible = "csee4840,top_module-1.0";
 *
 * Register map in FPGA (byte offsets from base):
 *   0x00 ULTRA_STATUS (R/W: bit0 new, bit1 echo, bit2 trig; write 1 to bit0 clears new)
 *   0x04 LAST_ECHO_CNT (R)
 *   0x08 CURR_ECHO_CNT (R)
 *   0x0C SAMPLE_ID     (R)
 *   0x10 FRUIT_X       (W)
 *   0x14 FRUIT_Y       (W)
 *   0x18 FRUIT_RADIUS  (W)
 *   0x1C FRUIT_CTRL    (W, bit0 visible)
 *   0x20 SCORE         (W)
 *   0x24 GAME_STATE    (W)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "top_module.h"

#define DRIVER_NAME "top_module"

struct top_dev {
    void __iomem *regs;
    struct miscdevice misc;
};

static struct top_dev *g_dev;  /* single instance */

/* ---- ioctl ---- */
static long top_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct top_dev *d = g_dev;
    if (!d || !d->regs)
        return -ENODEV;

    switch (cmd) {
    case READ_ULTRA: {
        struct ultra_sample s;
        s.status        = ioread32(d->regs + ULTRA_STATUS_OFF);
        s.last_echo_cnt = ioread32(d->regs + LAST_ECHO_CNT_OFF);
        s.sample_id     = ioread32(d->regs + SAMPLE_ID_OFF);
        /* clear new_sample bit (write-1-to-clear) */
        iowrite32(ULTRA_STATUS_NEW_BIT, d->regs + ULTRA_STATUS_OFF);
        if (copy_to_user((void __user *)arg, &s, sizeof(s)))
            return -EFAULT;
        return 0;
    }
    case WRITE_FRUIT: {
        struct fruit_cmd f;
        if (copy_from_user(&f, (void __user *)arg, sizeof(f)))
            return -EFAULT;
        iowrite32(f.x,             d->regs + FRUIT_X_OFF);
        iowrite32(f.y,             d->regs + FRUIT_Y_OFF);
        iowrite32(f.radius,        d->regs + FRUIT_RADIUS_OFF);
        iowrite32(f.visible & 0x1, d->regs + FRUIT_CTRL_OFF);
        return 0;
    }
    case WRITE_SCORE: {
        unsigned int v;
        if (copy_from_user(&v, (void __user *)arg, sizeof(v)))
            return -EFAULT;
        iowrite32(v, d->regs + SCORE_OFF);
        return 0;
    }
    case WRITE_GAME_STATE: {
        unsigned int v;
        if (copy_from_user(&v, (void __user *)arg, sizeof(v)))
            return -EFAULT;
        iowrite32(v, d->regs + GAME_STATE_OFF);
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

static const struct file_operations top_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = top_ioctl,
};

/* ---- platform driver ---- */
static int top_probe(struct platform_device *pdev)
{
    struct top_dev *d;
    struct resource *r;
    int ret;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d) return -ENOMEM;

    r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    d->regs = devm_ioremap_resource(&pdev->dev, r);
    if (IS_ERR(d->regs))
        return PTR_ERR(d->regs);

    d->misc.minor  = MISC_DYNAMIC_MINOR;
    d->misc.name   = DRIVER_NAME;
    d->misc.fops   = &top_fops;
    ret = misc_register(&d->misc);
    if (ret) return ret;

    platform_set_drvdata(pdev, d);
    g_dev = d;

    /* initial state: fruit hidden, score 0, game_state 0 */
    iowrite32(0, d->regs + FRUIT_CTRL_OFF);
    iowrite32(0, d->regs + SCORE_OFF);
    iowrite32(0, d->regs + GAME_STATE_OFF);

    dev_info(&pdev->dev, "top_module loaded, regs @ %p\n", d->regs);
    return 0;
}

static int top_remove(struct platform_device *pdev)
{
    struct top_dev *d = platform_get_drvdata(pdev);
    misc_deregister(&d->misc);
    g_dev = NULL;
    return 0;
}

static const struct of_device_id top_of_match[] = {
    { .compatible = "csee4840,top_module-1.0" },
    { }
};
MODULE_DEVICE_TABLE(of, top_of_match);

static struct platform_driver top_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = top_of_match,
        .owner = THIS_MODULE,
    },
    .probe  = top_probe,
    .remove = top_remove,
};

module_platform_driver(top_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fruit-Ninja DE1-SoC ultrasonic + VGA driver");
