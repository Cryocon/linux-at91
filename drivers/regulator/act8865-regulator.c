/*
 * act8865-regulator.c - Voltage regulation for the active-semi ACT8865
 * http://www.active-semi.com/sheets/ACT8865_Datasheet.pdf
 *
 * Copyright (C) 2013 Atmel Corporation
 * Wenyou Yang <wenyou.yang@atmel.com>
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/act8865.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regmap.h>

/*
 * ACT8865 Global Register Map.
 */
#define	ACT8865_SYS_MODE	0x00
#define ACT8865_SYS_CTRL	0x01
#define ACT8865_DCDC1_VSET1	0x20
#define	ACT8865_DCDC1_VSET2	0x21
#define	ACT8865_DCDC1_CTRL	0x22
#define	ACT8865_DCDC2_VSET1	0x30
#define	ACT8865_DCDC2_VSET2	0x31
#define	ACT8865_DCDC2_CTRL	0x32
#define	ACT8865_DCDC3_VSET1	0x40
#define	ACT8865_DCDC3_VSET2	0x41
#define	ACT8865_DCDC3_CTRL	0x42
#define	ACT8865_LDO1_VSET	0x50
#define	ACT8865_LDO1_CTRL	0x51
#define	ACT8865_LDO2_VSET	0x54
#define	ACT8865_LDO2_CTRL	0x55
#define	ACT8865_LDO3_VSET	0x60
#define	ACT8865_LDO3_CTRL	0x61
#define	ACT8865_LDO4_VSET	0x64
#define	ACT8865_LDO4_CTRL	0x65

/*
 * Field Definitions.
 */
#define ACT8865_ENA		0x80  /* ON - [7] */
#define ACT8865_VSEL_MASK	0x3F  /* VSET - [5:0] */

/*
 * DC/DC MODE bit
 */
#define ACT8865_MODE_MASK	0x20
#define ACT8865_MODE_NORMAL	0x20
#define ACT8865_MODE_IDLE	0x00


struct act8865 {
	struct regulator_dev	*rdev[ACT8865_REG_NUM];
	struct regmap		*regmap;
};

static const struct regmap_config act8865_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/* ACt8865 voltage table */
static const u32 act8865_voltages_table[] = {
	600000,		625000,		650000,		675000,
	700000,		725000,		750000,		775000,
	800000,		825000,		850000,		875000,
	900000,		925000,		950000,		975000,
	1000000,	1025000,	1050000,	1075000,
	1100000,	1125000,	1150000,	1175000,
	1200000,	1250000,	1300000,	1350000,
	1400000,	1450000,	1500000,	1550000,
	1600000,	1650000,	1700000,	1750000,
	1800000,	1850000,	1900000,	1950000,
	2000000,	2050000,	2010000,	2150000,
	2200000,	2250000,	2300000,	2350000,
	2400000,	2500000,	2600000,	2700000,
	2800000,	2900000,	3000000,	3100000,
	3200000,	3300000,	3400000,	3500000,
	3600000,	3700000,	3800000,	3900000,
};

static int act8865_read_byte(struct act8865 *act8865, u8 addr, u8 *data)
{
	int ret;
	unsigned int val;

	ret = regmap_read(act8865->regmap, addr, &val);
	if (ret < 0)
		return ret;

	*data = (u8)val;
	return 0;
}

static inline int act8865_update_bits(struct act8865 *act8865, u8 addr,
				unsigned int mask, u8 data)
{
	return regmap_update_bits(act8865->regmap, addr, mask, data);
}

static int act8865_set_mode(struct regulator_dev *rdev, unsigned mode)
{
	u8 addr, value, mask = ACT8865_MODE_MASK;
	int id = rdev_get_id(rdev);
	struct act8865 *act8865 = rdev_get_drvdata(rdev);

	switch (id) {
	case ACT8865_DCDC1:
		addr = ACT8865_DCDC1_CTRL;
		break;
	case ACT8865_DCDC2:
		addr = ACT8865_DCDC2_CTRL;
		break;
	case ACT8865_DCDC3:
		addr = ACT8865_DCDC3_CTRL;
		break;
	default:
		return -EINVAL;
	}

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		value = ACT8865_MODE_NORMAL;
		break;
	case REGULATOR_MODE_IDLE:
		value = ACT8865_MODE_IDLE;
		break;
	default:
		return -EINVAL;
	}

	return act8865_update_bits(act8865, addr, mask, value);
}

static unsigned act8865_get_mode(struct regulator_dev *rdev)
{
	u8 addr, value, mask = ACT8865_MODE_MASK;
	int id = rdev_get_id(rdev);
	struct act8865 *act8865 = rdev_get_drvdata(rdev);
	int ret;

	switch (id) {
	case ACT8865_DCDC1:
		addr = ACT8865_DCDC1_CTRL;
		break;
	case ACT8865_DCDC2:
		addr = ACT8865_DCDC2_CTRL;
		break;
	case ACT8865_DCDC3:
		addr = ACT8865_DCDC3_CTRL;
		break;
	default:
		return -EINVAL;
	}

	ret = act8865_read_byte(act8865, addr, &value);
	if (ret)
		return ret;

	return value & mask ? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int act8865_get_status(struct regulator_dev *rdev)
{
	int ret = regulator_is_enabled_regmap(rdev);

	if (ret == 0) {
		ret = REGULATOR_STATUS_OFF;
	} else if (ret > 0) {
		ret = act8865_get_mode(rdev);
		if (ret > 0)
			ret = regulator_mode_to_status(ret);
		else if (ret == 0)
			ret = -EIO;
	}

	return ret;
}

static int act8865_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	u32	selector;

	selector = regulator_map_voltage_iterate(rdev, uV, uV);

	return regulator_set_voltage_sel_regmap(rdev, selector);
}

static int act8865_set_suspend_mode(struct regulator_dev *rdev,
							unsigned mode)
{
	return act8865_set_mode(rdev, mode);
}

static struct regulator_ops act8865_ops = {
	.list_voltage		= regulator_list_voltage_table,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_mode		= act8865_set_mode,
	.get_mode		= act8865_get_mode,
	.get_status		= act8865_get_status,
	.set_suspend_voltage	= act8865_set_suspend_voltage,
	.set_suspend_enable	= regulator_enable_regmap,
	.set_suspend_disable	= regulator_disable_regmap,
	.set_suspend_mode	= act8865_set_suspend_mode,
};

static const struct regulator_desc act8865_reg[] = {
	{
		.name = "DCDC_REG1",
		.id = ACT8865_DCDC1,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_DCDC1_VSET1,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_DCDC1_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "DCDC_REG2",
		.id = ACT8865_DCDC2,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_DCDC2_VSET1,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_DCDC2_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "DCDC_REG3",
		.id = ACT8865_DCDC3,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_DCDC3_VSET1,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_DCDC3_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_REG4",
		.id = ACT8865_LDO1,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_LDO1_VSET,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_LDO1_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_REG5",
		.id = ACT8865_LDO2,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_LDO2_VSET,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_LDO2_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_REG6",
		.id = ACT8865_LDO3,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_LDO3_VSET,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_LDO3_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_REG7",
		.id = ACT8865_LDO4,
		.ops = &act8865_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = ARRAY_SIZE(act8865_voltages_table),
		.volt_table = act8865_voltages_table,
		.vsel_reg = ACT8865_LDO4_VSET,
		.vsel_mask = ACT8865_VSEL_MASK,
		.enable_reg = ACT8865_LDO4_CTRL,
		.enable_mask = ACT8865_ENA,
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_OF
static const struct of_device_id act8865_dt_ids[] = {
	{ .compatible = "active-semi,act8865" },
	{ }
};
MODULE_DEVICE_TABLE(of, act8865_dt_ids);

static int act8865_pdata_from_dt(struct device *dev,
				 struct device_node **of_node,
				 struct act8865_platform_data *pdata)
{
	int matched, i;
	struct device_node *np;
	struct act8865_regulator_data *regulator;
	struct of_regulator_match rmatch[ARRAY_SIZE(act8865_reg)];

	np = of_find_node_by_name(dev->of_node, "regulators");
	if (!np) {
		dev_err(dev, "missing 'regulators' subnode in DT\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rmatch); i++)
		rmatch[i].name = act8865_reg[i].name;

	matched = of_regulator_match(dev, np, rmatch, ARRAY_SIZE(rmatch));
	if (matched <= 0)
		return matched;

	pdata->regulators = devm_kzalloc(dev,
				sizeof(struct act8865_regulator_data) * matched,
				GFP_KERNEL);
	if (!pdata->regulators) {
		dev_err(dev, "%s: failed to allocate act8865 registor\n",
						__func__);
		return -ENOMEM;
	}

	pdata->num_regulators = matched;
	regulator = pdata->regulators;

	for (i = 0; i < matched; i++) {
		regulator->id = i;
		regulator->name = rmatch[i].name;
		regulator->platform_data = rmatch[i].init_data;
		of_node[i] = rmatch[i].of_node;
		regulator++;
	}

	return 0;
}

#else

static inline int act8865_pdata_from_dt(struct device *dev,
					struct device_node **of_node,
					struct act8865_platform_data *pdata)
{
	return 0;
}
#endif

static int act8865_pmic_probe(struct i2c_client *client,
			   const struct i2c_device_id *i2c_id)
{
	struct regulator_dev **rdev;
	struct device *dev = &client->dev;
	struct act8865_platform_data *pdata = dev_get_platdata(dev);
	struct regulator_config config = { };
	struct act8865 *act8865;
	struct device_node *of_node[ACT8865_REG_NUM];
	int i, id;
	int ret = -EINVAL;
	int error;

	if (dev->of_node && !pdata) {
		const struct of_device_id *id;
		struct act8865_platform_data pdata_of;

		id = of_match_device(of_match_ptr(act8865_dt_ids), dev);
		if (!id)
			return -ENODEV;

		ret = act8865_pdata_from_dt(dev, of_node, &pdata_of);
		if (ret < 0)
			return ret;

		pdata = &pdata_of;
	} else {
		memset(of_node, 0, sizeof(of_node));
	}

	if (pdata->num_regulators > ACT8865_REG_NUM) {
		dev_err(dev, "Too many regulators found!\n");
		return -EINVAL;
	}

	act8865 = devm_kzalloc(dev, sizeof(struct act8865) +
			sizeof(struct regulator_dev *) * ACT8865_REG_NUM,
			GFP_KERNEL);
	if (!act8865)
		return -ENOMEM;

	rdev = act8865->rdev;

	act8865->regmap = devm_regmap_init_i2c(client, &act8865_regmap_config);
	if (IS_ERR(act8865->regmap)) {
		error = PTR_ERR(act8865->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	/* Finally register devices */
	for (i = 0; i < pdata->num_regulators; i++) {

		id = pdata->regulators[i].id;

		config.dev = dev;
		config.init_data = pdata->regulators[i].platform_data;
		config.of_node = of_node[i];
		config.driver_data = act8865;
		config.regmap = act8865->regmap;

		rdev[i] = regulator_register(&act8865_reg[i], &config);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(dev, "failed to register %s\n",
				act8865_reg[id].name);
			goto err_unregister;
		}
	}

	i2c_set_clientdata(client, act8865);

	return 0;

err_unregister:
	while (--i >= 0)
		regulator_unregister(rdev[i]);

	return ret;
}

static int act8865_pmic_remove(struct i2c_client *client)
{
	struct act8865 *act8865 = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < ACT8865_REG_NUM; i++)
		regulator_unregister(act8865->rdev[i]);

	return 0;
}

static const struct i2c_device_id act8865_ids[] = {
	{ "act8865", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, act8865_ids);

static struct i2c_driver act8865_pmic_driver = {
	.driver	= {
		.name	= "act8865",
		.owner	= THIS_MODULE,
	},
	.probe	= act8865_pmic_probe,
	.remove	= act8865_pmic_remove,
	.id_table = act8865_ids,
};

module_i2c_driver(act8865_pmic_driver);

MODULE_DESCRIPTION("active-semi act8865 voltage regulator driver");
MODULE_AUTHOR("Wenyou Yang <wenyou.yang@atmel.com>");
MODULE_LICENSE("GPL v2");
