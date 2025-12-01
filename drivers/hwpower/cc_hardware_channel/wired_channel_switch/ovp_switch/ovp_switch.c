/*
 * ovp_switch.c
 *
 * ovp switch to control wired channel
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <chipset_common/hwpower/wired_channel_switch.h>
#include <chipset_common/hwpower/power_dts.h>
#include <chipset_common/hwpower/power_gpio.h>
#include <huawei_platform/log/hw_log.h>

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif
#define HWLOG_TAG ovp_chsw
HWLOG_REGIST();

static u32 g_chsw_by_ovp;
static int g_ovp_chsw_count;
static int g_ovp_chsw_en[WIRED_CHANNEL_TOTAL];
static int g_ovp_chsw_initialized;
static int g_ovp_chsw_status = WIRED_CHANNEL_RESTORE;
static u32 g_gpio_low_by_set_input = 1;

static int ovp_chsw_get_wired_channel(void)
{
	return g_ovp_chsw_status;
}

static void ovp_chsw_gpio_free(void)
{
	int i;

	for (i = 0; i < g_ovp_chsw_count; i++) {
		if (gpio_is_valid(g_ovp_chsw_en[i]))
			gpio_free(g_ovp_chsw_en[i]);
	}
}

static int ovp_chsw_set_gpio_input(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < g_ovp_chsw_count; i++)
		ret |= gpio_direction_input(g_ovp_chsw_en[i]);

	return ret;
}

static int ovp_chsw_set_gpio_output(unsigned int value)
{
	int i;
	int ret = 0;

	for (i = 0; i < g_ovp_chsw_count; i++)
		ret |= gpio_direction_output(g_ovp_chsw_en[i], value);

	return ret;
}

static int ovp_chsw_set_gpio_low(void)
{
	if (g_gpio_low_by_set_input)
		return ovp_chsw_set_gpio_input(); /* restore */

	return ovp_chsw_set_gpio_output(0); /* restore */
}

static int ovp_chsw_set_wired_channel(int flag)
{
	int ret;

	if (!g_ovp_chsw_initialized) {
		hwlog_err("ovp channel switch not initialized\n");
		return -ENODEV;
	}

	if (flag == WIRED_CHANNEL_CUTOFF)
		ret = ovp_chsw_set_gpio_output(1); /* cutoff */
	else
		ret = ovp_chsw_set_gpio_low(); /* restore */

	hwlog_info("ovp channel switch set en=%d\n",
		(flag == WIRED_CHANNEL_CUTOFF) ? 1 : 0);

	g_ovp_chsw_status = flag;
	return ret;
}

static struct wired_chsw_device_ops ovp_chsw_ops = {
	.get_wired_channel = ovp_chsw_get_wired_channel,
	.set_wired_channel = ovp_chsw_set_wired_channel,
};

static void ovp_chsw_parse_dts(struct device_node *np)
{
	(void)power_dts_read_u32_compatible(power_dts_tag(HWLOG_TAG),
		"huawei,wired_channel_switch", "use_ovp_cutoff_wired_channel",
		&g_chsw_by_ovp, 0);
	(void)power_dts_read_u32_compatible(power_dts_tag(HWLOG_TAG),
		"huawei,wired_channel_switch", "wired_channel_count",
		&g_ovp_chsw_count, 1);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"gpio_low_by_set_input", &g_gpio_low_by_set_input, 1);
}

static int ovp_chsw_gpio_init(struct device_node *np)
{
	int ret;

	if (power_gpio_request(np, "gpio_ovp_chsw_en", "gpio_ovp_chsw_en",
		&g_ovp_chsw_en[WIRED_CHANNEL_MAIN]))
		return -1;

	if ((g_ovp_chsw_count == WIRED_CHANNEL_TOTAL) &&
		(power_gpio_request(np, "gpio_ovp_chsw2_en", "gpio_ovp_chsw2_en",
		&g_ovp_chsw_en[WIRED_CHANNEL_AUX])))
		return -1;

	ret = ovp_chsw_set_gpio_low();
	if (ret) {
		hwlog_err("gpio set input fail\n");
		ovp_chsw_gpio_free();
		return -1;
	}

	g_ovp_chsw_initialized = 1;
	return 0;
}

static int ovp_chsw_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = NULL;

	if (!pdev || !pdev->dev.of_node)
		return -ENODEV;

	np = pdev->dev.of_node;
	ovp_chsw_parse_dts(np);

	if (g_chsw_by_ovp) {
		ret = ovp_chsw_gpio_init(np);
		if (ret)
			return -1;

		ret = wired_chsw_ops_register(&ovp_chsw_ops);
		if (ret) {
			ovp_chsw_gpio_free();
			return -1;
		}
	}

	return 0;
}

static int ovp_chsw_remove(struct platform_device *pdev)
{
	ovp_chsw_gpio_free();
	return 0;
}

static const struct of_device_id ovp_chsw_match_table[] = {
	{
		.compatible = "huawei,ovp_channel_switch",
		.data = NULL,
	},
	{},
};

static struct platform_driver ovp_chsw_driver = {
	.probe = ovp_chsw_probe,
	.remove = ovp_chsw_remove,
	.driver = {
		.name = "huawei,ovp_channel_switch",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ovp_chsw_match_table),
	},
};

static int __init ovp_chsw_init(void)
{
	return platform_driver_register(&ovp_chsw_driver);
}

static void __exit ovp_chsw_exit(void)
{
	platform_driver_unregister(&ovp_chsw_driver);
}

fs_initcall_sync(ovp_chsw_init);
module_exit(ovp_chsw_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ovp switch module driver");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
