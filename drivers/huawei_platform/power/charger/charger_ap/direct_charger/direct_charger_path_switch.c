/*
 * direct_charger_path_switch.c
 *
 * path switch for direct charger
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

#include <huawei_platform/power/direct_charger/direct_charger.h>
#include <huawei_platform/power/huawei_charger_common.h>
#include <chipset_common/hwpower/wired_channel_switch.h>
#include <huawei_platform/log/hw_log.h>

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif

#define HWLOG_TAG direct_charge_path
HWLOG_REGIST();

static const char * const g_dc_charging_path[PATH_END] = {
	[PATH_NORMAL] = "path_normal",
	[PATH_LVC] = "path_lvc",
	[PATH_SC] = "path_sc",
};

static const char *direct_charge_get_charging_path_string(unsigned int path)
{
	if ((path >= PATH_BEGIN) && (path < PATH_END))
		return g_dc_charging_path[path];

	return "illegal charging_path";
}

static int direct_charge_open_wired_channel(struct direct_charge_device *di)
{
	if (di->need_wired_sw_off)
		return wired_chsw_set_wired_channel(WIRED_CHANNEL_RESTORE);

	return 0;
}

static int direct_charge_close_wired_channel(struct direct_charge_device *di)
{
	if (di->need_wired_sw_off)
		return wired_chsw_set_wired_channel(WIRED_CHANNEL_CUTOFF);

	return 0;
}

/* switch charging path to normal charging path */
static int direct_charge_switch_path_to_normal_charging(void)
{
	int ret;
	struct direct_charge_device *l_di = direct_charge_get_di();

	if (!l_di)
		return -1;

	msleep(WAIT_LS_DISCHARGE); /* need to wait device discharge */

	ret = direct_charge_open_wired_channel(l_di);

	return ret;
}

/* switch charging path to lvc or sc charging path */
static int direct_charge_switch_path_to_dc_charging(void)
{
	struct direct_charge_device *l_di = direct_charge_get_di();

	if (!l_di)
		return -1;

	if (l_di->scp_work_on_charger) {
		direct_charge_adapter_protocol_power_supply(ENABLE);
		charge_enable_eoc(EOC_DISABLE);
		reset_cur_delay_eoc_count();
		if (l_di->use_force_sleep)
			charge_enable_force_sleep(HIZ_MODE_ENABLE);
		else
			charge_enable_hz(HIZ_MODE_ENABLE);
	}

	msleep(100); /* delay 100ms */
	return direct_charge_close_wired_channel(l_di);
}

int direct_charge_switch_charging_path(unsigned int path)
{
	hwlog_info("switch to %d,%s charging path\n",
		path, direct_charge_get_charging_path_string(path));

	switch (path) {
	case PATH_NORMAL:
		return direct_charge_switch_path_to_normal_charging();
	case PATH_LVC:
	case PATH_SC:
		return direct_charge_switch_path_to_dc_charging();
	default:
		return -1;
	}
}
