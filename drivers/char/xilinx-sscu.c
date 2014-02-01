/*
 *  linux/drivers/char/xilinx-sscu.c
 *
 *  Copyright (C) 2011 Andrew 'Necromant' Andrianov <contact@necromant.ath.cx>
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

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/types.h>
#include <linux/cdev.h>

#include <linux/xilinx-sscu.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define DRVNAME "xilinx-sscu"
#define DEVNAME "fpga"
#define DRVVER	"0.1"

static int g_debug;
module_param(g_debug, int, 0);	/* and these 2 lines */
MODULE_PARM_DESC(g_debug, "Print lots of useless debug info.");

static struct miscdevice *mdev = 0;

/* This delay is system specific. In my case (200Mhz ARM) I can safely
   define it to nothing to speed things up. But on a faster system you
   may want to define it to something, e.g. udelay(100) if the clk will
   get too fast and crew things up. I do not have a chance to check if
   it's needed on a faster system, so I left it here to be 100% sure.
   Have fun
*/

#define DELAY

#define DBG(fmt, ...)	if (g_debug) \
    printk(KERN_DEBUG "%s/%s: " fmt " \n", DRVNAME, __FUNCTION__, ##__VA_ARGS__)
#define INF(fmt, ...)	printk(KERN_INFO "%s: " fmt " \n", DRVNAME, ##__VA_ARGS__)
#define ERR(fmt, ...)	printk(KERN_ERR "%s: " fmt " \n", DRVNAME, ##__VA_ARGS__)

static inline char *xsscu_state2char(struct xsscu_device_data *dev_data)
{
	switch (dev_data->state) {
	case XSSCU_STATE_UPLOAD_DONE:
	case XSSCU_STATE_IDLE:
		if (gpio_get_value(dev_data->pdata->done))
			return "Online";
		else
			return "Unprogrammed/Error";
	case XSSCU_STATE_DISABLED:
		return "Offline";
	case XSSCU_STATE_PROG_ERROR:
		return "Bitstream error";
	default:
		return "Bug!";
	}
}

static int xsscu_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc;
	struct xsscu_device_data *dev_data;
	misc = file->private_data;
	dev_data = misc->this_device->platform_data;
	if (dev_data->open)
		return -EBUSY;
	int err = gpio_request(dev_data->pdata->clk,"clk") ||
	    gpio_request(dev_data->pdata->done, "done") ||
	    gpio_request(dev_data->pdata->init_b, "init") ||
	    gpio_request(dev_data->pdata->prog_b, "prog") ||
	    gpio_request(dev_data->pdata->sout, "sout");
	if (err) {
		ERR("Failed to claim required GPIOs");
		err = -EBUSY;
		goto err_request_pins;
	}
	dev_data->open++;
	DBG("Device %s opened", dev_data->pdata->name);
	sprintf(dev_data->msg_buffer,
		"DEVICE:\t%s\nINIT_B:\t%d\nDONE:\t%d\nSTATE:\t%s\n",
		dev_data->pdata->name,
		gpio_get_value(dev_data->pdata->init_b),
		gpio_get_value(dev_data->pdata->done),
		xsscu_state2char(dev_data)
	    );
	dev_data->read_ptr = dev_data->msg_buffer;
	return 0;
err_request_pins:
	gpio_free(dev_data->pdata->clk);
	gpio_free(dev_data->pdata->done);
	gpio_free(dev_data->pdata->init_b);
	gpio_free(dev_data->pdata->prog_b);
	gpio_free(dev_data->pdata->sout);
	return err;
}

static int send_clocks(struct xsscu_data *p, int c)
{

	while (c--) {
		gpio_direction_output(p->clk, 0);
		DELAY;
		gpio_direction_output(p->clk, 1);
		DELAY;
		if (1 == gpio_get_value(p->done))
			return 0;
	}
	return 1;
}

static inline void xsscu_dbg_state(struct xsscu_data *p)
{
	DBG("INIT_B: %d | DONE: %d",
	    gpio_get_value(p->init_b), gpio_get_value(p->done));
}

static int xsscu_release(struct inode *inode, struct file *file)
{
	struct miscdevice *misc;
	struct xsscu_device_data *dev_data;
	int err = 0;
	misc = file->private_data;
	dev_data = misc->this_device->platform_data;
	dev_data->open--;
	switch (dev_data->state) {
	case XSSCU_STATE_UPLOADING:
		err = send_clocks(dev_data->pdata, 10000);
		dev_data->state = XSSCU_STATE_UPLOAD_DONE;
		break;
	case XSSCU_STATE_DISABLED:
		err = 0;
		break;
	}

	if (err) {
		ERR("DONE not HIGH or other programming error");
		dev_data->state = XSSCU_STATE_PROG_ERROR;
	}
	xsscu_dbg_state(dev_data->pdata);
	gpio_free(dev_data->pdata->clk);
	gpio_free(dev_data->pdata->done);
	gpio_free(dev_data->pdata->init_b);
	gpio_free(dev_data->pdata->prog_b);
	gpio_free(dev_data->pdata->sout);
	DBG("Device closed");
	/* We must still close the device, hence return ok */
	if (err) {
		return -EIO;
	} else {
		return 0;
	}
}

static ssize_t xsscu_read(struct file *filp, char *buffer,
			  size_t length,
			  loff_t *offset)
{
	struct miscdevice *misc;
	struct xsscu_device_data *dev_data;
	int bytes_read = 0;
	misc = filp->private_data;
	dev_data = misc->this_device->platform_data;

	if (*dev_data->read_ptr == 0)
		return 0;
	while (length && *dev_data->read_ptr) {
		put_user(*(dev_data->read_ptr++), buffer++);
		length--;
		bytes_read++;
	}
	return bytes_read;
}

static int xsscu_reset_fpga(struct xsscu_data *p)
{
	int i = 50;
	DBG("Resetting FPGA...");
	gpio_direction_output(p->prog_b, 0);
	mdelay(1);
	gpio_direction_output(p->prog_b, 1);
	while (i--) {
		xsscu_dbg_state(p);
		if (gpio_get_value(p->init_b) == 1)
			return 0;
		mdelay(1);
	}
	ERR("FPGA reset failed");
	return 1;
}

static ssize_t xsscu_write(struct file *filp,
			   const char *buff, size_t len, loff_t * off)
{
	struct miscdevice *misc;
	struct xsscu_device_data *dev_data;
	int i;
	int k;
	i = 0;
	misc = filp->private_data;
	dev_data = misc->this_device->platform_data;

	if ((*off == 0)) {
		if (strncmp(buff, "disable", 7) == 0) {
			DBG("Disabling FPGA");
			gpio_direction_output(dev_data->pdata->prog_b, 0);
			dev_data->state = XSSCU_STATE_DISABLED;
			goto all_written;
		} else if (xsscu_reset_fpga(dev_data->pdata) != 0)
			return -EIO;
		/*Wait a little bit, before starting to clock the fpga,
		as the datasheet suggests */
		mdelay(1);
		gpio_direction_output(dev_data->pdata->clk, 0);
		dev_data->state = XSSCU_STATE_UPLOADING;
	}
	/* bitbang data */
	while (i < len) {
		for (k = 7; k >= 0; k--) {
			gpio_direction_output(dev_data->pdata->sout,
					      (buff[i] & (1 << k)));
			gpio_direction_output(dev_data->pdata->clk, 1);
			DELAY;
			gpio_direction_output(dev_data->pdata->clk, 0);
			DELAY;
		}
		i++;
	}
all_written:
	*off += len;
	return len;
}

static const struct file_operations xsscu_fileops = {
	.owner = THIS_MODULE,
	.write = xsscu_write,
	.read = xsscu_read,
	.open = xsscu_open,
	.release = xsscu_release,
	.llseek = no_llseek,
};

static int xsscu_create_miscdevice(struct platform_device *p, int id)
{
	struct xsscu_device_data *dev_data;
	char *nm;
	int err;
	mdev = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!mdev) {
		ERR("Misc device allocation failed");
		return -ENOMEM;
	}
	nm = kzalloc(64, GFP_KERNEL);
	if (!nm) {
		err = -ENOMEM;
		ERR("Device name allocation failed");
		goto freemisc;
	}
	dev_data = kzalloc(sizeof(struct xsscu_device_data), GFP_KERNEL);
	if (!dev_data) {
		ERR("Device data allocation failed");
		err = -ENOMEM;
		goto freenm;
	}

	snprintf(nm, 64, "fpga%d", id);
	mdev->name = nm;
	mdev->fops = &xsscu_fileops;
	mdev->minor = MISC_DYNAMIC_MINOR;
	err = misc_register(mdev);
	if (!err) {
		mdev->this_device->platform_data = dev_data;
		dev_data->pdata = p->dev.platform_data;
	}

	return err;

freenm:
	kfree(nm);
freemisc:
	kfree(mdev);

	return err;
}

static int of_xilinx_sscu_get_pins(struct device_node *np,
		unsigned int *clk_pin, unsigned int *done_pin, unsigned int *init_pin, unsigned int *prog_pin, unsigned int *sout_pin)
{
	if (of_gpio_count(np) < 2)
		return -ENODEV;

	*clk_pin =  of_get_gpio(np, 0);
	*done_pin = of_get_gpio(np, 1);
	*init_pin = of_get_gpio(np, 2);
	*prog_pin = of_get_gpio(np, 3);
	*sout_pin = of_get_gpio(np, 4);

	if (!gpio_is_valid(*clk_pin) ||
			!gpio_is_valid(*done_pin) ||
			!gpio_is_valid(*init_pin) ||
			!gpio_is_valid(*prog_pin) ||
			!gpio_is_valid(*sout_pin)) {
		pr_err("%s: invalid GPIO pins\n",
		       np->full_name);
		return -ENODEV;
	}

	return 0;
}

static int xsscu_probe(struct platform_device *p)
{
	int err;
	int ret;
	int id;
	struct xsscu_data *pdata = p->dev.platform_data;
	/* some id magic */
	if (p->id == -1)
		id = 0;
	else
		id = p->id;
	DBG("Probing xsscu platform device with id %d", p->id);
	unsigned int clk_pin, done_pin, init_pin, prog_pin, sout_pin;

	if (p->dev.of_node) {
		ret = of_xilinx_sscu_get_pins(p->dev.of_node,
					   &clk_pin, &done_pin, &init_pin, &prog_pin, &sout_pin);
		if (ret)
			return ret;

	} else {
		if (!p->dev.platform_data) {
			ERR("Missing platform_data, sorry dude");
			return -ENXIO;
		}
		pdata = p->dev.platform_data;
		clk_pin = pdata->clk;
		done_pin = pdata->done;
		init_pin = pdata->init_b;
		prog_pin = pdata->prog_b;
		sout_pin = pdata->sout;
	}

	if (!p->dev.platform_data) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		p->dev.platform_data = pdata;
	}
	if (p->dev.of_node) {
		pdata->clk = clk_pin;
		pdata->done = done_pin;
		pdata->init_b = init_pin;
		pdata->prog_b = prog_pin;
		pdata->sout = sout_pin;
//		of_i2c_gpio_get_props(p->dev.of_node, pdata);
	}

	err = xsscu_create_miscdevice(p, id);

	platform_set_drvdata(p, mdev);
	if (!err)
		INF("FPGA Device %s registered as /dev/fpga%d", pdata->name,
		    id);
	return err;
}



static int xsscu_remove(struct platform_device *p) {
	struct xsscu_data *pdata = p->dev.platform_data;
	gpio_free(pdata->clk);
	gpio_free(pdata->done);
	gpio_free(pdata->init_b);
	gpio_free(pdata->prog_b);
	gpio_free(pdata->sout);
	if (mdev) {
		misc_deregister(mdev);
		mdev = 0;
	}
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id xilinx_sscu_dt_ids[] = {
	{ .compatible = "xilinx-sscu" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, xilinx_sscu_dt_ids);
#endif

static struct platform_driver xsscu_driver = {
	.probe = xsscu_probe,
	.remove = xsscu_remove,
	.driver = {
		   .name = DRVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(xilinx_sscu_dt_ids),
		   }
};
//module_platform_driver(xsscu_driver);

static int __init xsscu_init(void)
{
	INF("Xilinx Slave Serial Configuration Upload Driver " DRVVER);
	return platform_driver_register(&xsscu_driver);
}

static void __exit xsscu_cleanup(void)
{
	platform_driver_unregister(&xsscu_driver);
}

module_init(xsscu_init);
module_exit(xsscu_cleanup);

MODULE_AUTHOR("Andrew 'Necromant' Andrianov <necromant@necromant.ath.cx>");
MODULE_DESCRIPTION("Xilinx Slave Serial BitBang Uploader driver");
MODULE_LICENSE("GPL");
