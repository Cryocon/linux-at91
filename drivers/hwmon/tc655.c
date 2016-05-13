/*
 * tc655 - Driver for TODO: description
 *
 * Copyright (C) 2016, Cryogenic Control Systems Inc
 * Author: Nicholas Walton <nicholas.walton@gmail.com>
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
 * Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>

static const struct i2c_device_id tc655_id[] = {
	{ "tc655", 0 },
	{ "tc654", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tc655_id);

enum tc655_regs {
	TC655_REG_RPM1 = 0x00,
	TC655_REG_RPM2 = 0x01,
	TC655_REG_FAULT1 = 0x02,
	TC655_REG_FAULT2 = 0x03,
	TC655_REG_CONFIG     = 0x04,
	TC655_REG_STATUS = 0x05,
	TC655_REG_DUTY_CYCLE = 0x06,
	TC655_REG_MFR_ID = 0x07,
	TC655_REG_VER_ID = 0x08,
};

#define TC655_RPM_CONFIG_RES_8BIT 50

#define TC655_REG_CONFIG_DUTYC   0x20
#define TC655_REG_CONFIG_F1PPR_8 0x06
#define TC655_REG_CONFIG_F1PPR_4 0x04
#define TC655_REG_CONFIG_F1PPR_2 0x02
#define TC655_REG_CONFIG_F1PPR_1 0x00
#define TC655_REG_CONFIG_SDM     0x01

struct tc655_data {
	struct device *hwmon_dev;
	struct i2c_client *client;
	u8 fan1_pwm;
};

#ifdef CONFIG_OF
static const struct of_device_id tc655_dt_match[] = {
	{ .compatible = "microchip,tc655" },
	{ .compatible = "microchip,tc654" },
	{ },
};
MODULE_DEVICE_TABLE(of, tc655_dt_match);
#endif

static int do_set_pwm(struct tc655_data *data, int val)
{
	int ret;

	val = clamp_val(val, 0x0, 0xf);
	ret = i2c_smbus_write_byte_data(data->client, TC655_REG_DUTY_CYCLE, val);
	if (ret >= 0)
		data->fan1_pwm = val;
	return ret;
}

static ssize_t get_fan_rpm(struct device *dev, struct device_attribute *da,
			   char *buf)
{
	unsigned int rpm;
	struct tc655_data *data = dev_get_drvdata(dev);
	if (IS_ERR(data))
		return PTR_ERR(data);

	rpm = TC655_RPM_CONFIG_RES_8BIT * i2c_smbus_read_byte_data(data->client, TC655_REG_RPM1);
	return sprintf(buf, "%u\n", rpm);
}

static int do_get_fan_enable(struct device *dev)
{
	unsigned int res;
	unsigned int config;
	struct tc655_data *data = dev_get_drvdata(dev);
	if (IS_ERR(data))
		return PTR_ERR(data);

	config = i2c_smbus_read_byte_data(data->client, TC655_REG_CONFIG);
	if ((config & (TC655_REG_CONFIG_DUTYC | TC655_REG_CONFIG_SDM))
			== TC655_REG_CONFIG_DUTYC)
		res = 1;
	else
		res = 0;
	return res;
}

static ssize_t get_fan_enable(struct device *dev, struct device_attribute *da,
			   char *buf)
{
	int res = do_get_fan_enable(dev);
	if (res < 0)
		return res;
	return sprintf(buf, "%u\n", res);
}

static int do_set_fan_enable(struct device *dev, unsigned long val) {
	int ret;
	int config;
	struct tc655_data *data = dev_get_drvdata(dev);
	if (IS_ERR(data))
		return PTR_ERR(data);

	config = i2c_smbus_read_byte_data(data->client, TC655_REG_CONFIG);
	switch (val) {
	default:
		return -EINVAL;
	case 0:
		config = (config & ~TC655_REG_CONFIG_DUTYC) | TC655_REG_CONFIG_SDM;
		break;
	case 1:
		config = (config | TC655_REG_CONFIG_DUTYC) & ~TC655_REG_CONFIG_SDM;
		break;
	}

	ret = i2c_smbus_write_byte_data(data->client, TC655_REG_CONFIG, config);
	if (ret < 0)
		return ret;

	if (val == 1)
		ret = do_set_pwm(data, data->fan1_pwm);

	return ret;
}

static ssize_t set_fan_enable(struct device *dev, struct device_attribute *da,
			    const char *buf, size_t count)
{
	int ret;
	unsigned long val;
	ret = kstrtoul(buf, 10, &val);
	if (ret == 0) {
		ret = do_set_fan_enable(dev, val);
	}
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t get_pwm(struct device *dev, struct device_attribute *da,
			   char *buf)
{
	struct tc655_data *data = dev_get_drvdata(dev);
	if (IS_ERR(data))
		return PTR_ERR(data);

	if (do_get_fan_enable(dev) != 0) {
		/* REG_DUTY_CYCLE only valid when fan is enabled */
		data->fan1_pwm = i2c_smbus_read_byte_data(data->client, TC655_REG_DUTY_CYCLE);
	}
	return sprintf(buf, "%u\n", data->fan1_pwm);
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *da,
			    const char *buf, size_t count)
{
	int ret;
	int enable;
	unsigned long val;
	struct tc655_data *data = dev_get_drvdata(dev);
	if (IS_ERR(data))
		return PTR_ERR(data);
	ret = kstrtoul(buf, 10, &val);
	if (ret == 0) {
		enable = do_get_fan_enable(dev);
		if (enable > 0) {
			/* Retrigger fan start-up circuit */
			do_set_fan_enable(dev, 0);
			ret = do_set_fan_enable(dev, 1);
			if (ret < 0)
				return ret;
		}
		do_set_pwm(data, val);
	}
	if (ret < 0)
		return ret;
	return count;
}


static DEVICE_ATTR(fan1_input, S_IRUGO, get_fan_rpm, NULL);
static DEVICE_ATTR(fan_enable, S_IWUSR | S_IRUGO, get_fan_enable, set_fan_enable);
static DEVICE_ATTR(pwm, S_IWUSR | S_IRUGO, get_pwm, set_pwm);
/* Driver data */
static struct attribute *tc655_attrs[] = {
	&dev_attr_fan1_input.attr,
	&dev_attr_fan_enable.attr,
	&dev_attr_pwm.attr,
	NULL
};

ATTRIBUTE_GROUPS(tc655);

static int tc655_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tc655_data *data;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(struct tc655_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	data->client = client;

	ret = i2c_smbus_write_byte_data(client, TC655_REG_DUTY_CYCLE, 0xf);
	if (ret)
		return ret;

	ret = i2c_smbus_write_byte_data(client, TC655_REG_CONFIG,
			TC655_REG_CONFIG_DUTYC | TC655_REG_CONFIG_F1PPR_4);
	if (ret)
		return ret;

	data->hwmon_dev = hwmon_device_register_with_groups(dev, client->name,
							    data, tc655_groups);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
	}

	return ret;
}

static int tc655_remove(struct i2c_client *client)
{
	int ret;
	struct tc655_data *data = i2c_get_clientdata(client);
	hwmon_device_unregister(data->hwmon_dev);

	ret = i2c_smbus_write_byte_data(client, TC655_REG_CONFIG, 0x0a);
	ret = i2c_smbus_write_byte_data(client, TC655_REG_DUTY_CYCLE, 0x2);
	return ret;
}

static void tc655_shutdown(struct i2c_client *client) {
	tc655_remove(client);
}

static struct i2c_driver tc655_driver = {
	.driver = {
		.name = "tc655",
		.of_match_table = of_match_ptr(tc655_dt_match),
	},
	.probe = tc655_probe,
	.remove = tc655_remove,
	.shutdown = tc655_shutdown,
	.id_table = tc655_id,
};

module_i2c_driver(tc655_driver);

MODULE_AUTHOR("Nicholas Walton <nicholas.walton@gmail.com>");
MODULE_DESCRIPTION("Microchip TC654/TC655 driver");
MODULE_LICENSE("GPL");
