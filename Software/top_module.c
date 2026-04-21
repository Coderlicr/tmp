/*
 * top_module.c  --  Linux platform/misc driver for Fruit-Ninja FPGA peripheral
 *
 * Exposes /dev/top_module with ioctl interface for:
 *   - Reading motion detection status   (MOTION_READ)
 *   - Writing fruit display parameters  (FRUIT_WRITE)
 *   - Writing score                     (SCORE_WRITE)
 *   - Writing game state                (GAME_ST_WRITE)
 *
 * Register bus: Avalon-MM, 32-bit data, word-addressed.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "top_module.h"

#define DRIVER_NAME "top_module"

/* ---- device state ---- */
struct fruit_ninja_dev {
	struct resource res;
	void __iomem   *virtbase;
} dev;

/* ---- low-level register helpers ---- */
static inline u32 reg_read(unsigned int offset)
{
	return ioread32((u8 *)dev.virtbase + offset);
}

static inline void reg_write(unsigned int offset, u32 value)
{
	iowrite32(value, (u8 *)dev.virtbase + offset);
}

/* ---- ioctl ---- */
static long top_module_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	motion_status_t ms;
	fruit_params_t  fp;
	unsigned int    val;

	switch (cmd) {

	case MOTION_READ:
		ms.motion_detected    = reg_read(REG_MOTION_STATUS) & 1;
		ms.changed_pixel_count = reg_read(REG_CHANGED_COUNT);
		ms.frame_counter       = reg_read(REG_FRAME_COUNTER);
		if (copy_to_user((motion_status_t __user *)arg, &ms, sizeof(ms)))
			return -EACCES;
		break;

	case FRUIT_WRITE:
		if (copy_from_user(&fp, (fruit_params_t __user *)arg, sizeof(fp)))
			return -EACCES;
		reg_write(REG_FRUIT_X,    fp.x      & 0x3FF);
		reg_write(REG_FRUIT_Y,    fp.y      & 0x3FF);
		reg_write(REG_FRUIT_RADIUS, fp.radius & 0x3FF);
		reg_write(REG_FRUIT_CTRL, fp.visible & 1);
		break;

	case SCORE_WRITE:
		if (copy_from_user(&val, (unsigned int __user *)arg, sizeof(val)))
			return -EACCES;
		reg_write(REG_SCORE, val);
		break;

	case GAME_ST_WRITE:
		if (copy_from_user(&val, (unsigned int __user *)arg, sizeof(val)))
			return -EACCES;
		reg_write(REG_GAME_STATE, val);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

/* ---- file ops ---- */
static const struct file_operations top_module_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = top_module_ioctl,
};

static struct miscdevice top_module_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DRIVER_NAME,
	.fops  = &top_module_fops,
};

/* ---- probe / remove ---- */
static int __init top_module_probe(struct platform_device *pdev)
{
	int ret;

	ret = misc_register(&top_module_misc_device);
	if (ret)
		return ret;

	ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
	if (ret) {
		ret = -ENOENT;
		goto out_deregister;
	}

	if (!request_mem_region(dev.res.start, resource_size(&dev.res),
				DRIVER_NAME)) {
		ret = -EBUSY;
		goto out_deregister;
	}

	dev.virtbase = of_iomap(pdev->dev.of_node, 0);
	if (!dev.virtbase) {
		ret = -ENOMEM;
		goto out_release;
	}

	pr_info(DRIVER_NAME ": mapped to %p, size %u\n",
		dev.virtbase, (unsigned)resource_size(&dev.res));
	return 0;

out_release:
	release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
	misc_deregister(&top_module_misc_device);
	return ret;
}

static int top_module_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&top_module_misc_device);
	return 0;
}

/* ---- device-tree match ---- */
#ifdef CONFIG_OF
static const struct of_device_id top_module_of_match[] = {
	{ .compatible = "csee4840,top_module-1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, top_module_of_match);
#endif

static struct platform_driver top_module_driver = {
	.driver = {
		.name           = DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(top_module_of_match),
	},
	.remove = __exit_p(top_module_remove),
};

static int __init top_module_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&top_module_driver, top_module_probe);
}

static void __exit top_module_exit(void)
{
	platform_driver_unregister(&top_module_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(top_module_init);
module_exit(top_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peiheng Li, Chengrui Li, Yitong Bai");
MODULE_DESCRIPTION("Fruit Ninja - DE1-SoC FPGA driver");
