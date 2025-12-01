/*
 * power_event.c
 *
 * event for power module
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

#include "power_event.h"
#include <chipset_common/hwpower/power_event_ne.h>
#include <chipset_common/hwpower/power_sysfs.h>
#include <chipset_common/hwpower/power_interface.h>
#include <huawei_platform/log/hw_log.h>

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif
#define HWLOG_TAG power_event
HWLOG_REGIST();

struct power_event_dev *g_power_event_dev;
static BLOCKING_NOTIFIER_HEAD(g_power_event_nh);

static const char * const g_power_event_ne_table[POWER_EVENT_NE_END] = {
	/* section: for connect & disconnect */
	[POWER_EVENT_NE_USB_DISCONNECT] = "usb_disconnect",
	[POWER_EVENT_NE_USB_CONNECT] = "usb_connect",
	[POWER_EVENT_NE_WIRELESS_DISCONNECT] = "wireless_disconnect",
	[POWER_EVENT_NE_WIRELESS_CONNECT] = "wireless_connect",
	/* section: for start & stop charging */
	[POWER_EVENT_NE_START_CHARGING] = "start_charging",
	[POWER_EVENT_NE_STOP_CHARGING] = "stop_charging",
	[POWER_EVENT_NE_SUSPEND_CHARGING] = "suspend_charging",
	/* section: for soc decimal */
	[POWER_EVENT_NE_SOC_DECIMAL_DC] = "soc_decimal_dc",
	[POWER_EVENT_NE_SOC_DECIMAL_WL_DC] = "soc_decimal_wl_dc",
	/* section: for water detect */
	[POWER_EVENT_NE_WD_REPORT_DMD] = "wd_report_dmd",
	[POWER_EVENT_NE_WD_REPORT_UEVENT] = "wd_report_uevent",
	[POWER_EVENT_NE_WD_DETECT_BY_USB_DP_DN] = "wd_detect_by_usb_dp_dn",
	[POWER_EVENT_NE_WD_DETECT_BY_USB_ID] = "wd_detect_by_usb_id",
	[POWER_EVENT_NE_WD_DETECT_BY_USB_GPIO] = "wd_detect_by_usb_gpio",
	[POWER_EVENT_NE_WD_DETECT_BY_AUDIO_DP_DN] = "wd_detect_by_audio_dp_dn",
	/* section: for power supply */
	[POWER_EVENT_NE_PSY_UPDATE_BATTERY] = "psy_update_battery",
	[POWER_EVENT_NE_PSY_UPDATE_RAW_BATTERY] = "psy_update_raw_battery",
	[POWER_EVENT_NE_PSY_UPDATE_ASSITST_BATTERY] = "psy_update_assitst_battery",
	[POWER_EVENT_NE_PSY_UPDATE_MAINS] = "psy_update_mains",
	[POWER_EVENT_NE_PSY_UPDATE_USB] = "psy_update_usb",
	[POWER_EVENT_NE_PSY_UPDATE_WIRELESS] = "psy_update_wireless",
	/* section: for uvdm */
	[POWER_EVENT_NE_UVDM_RECEIVE] = "uvdm_receive",
};

static const char *power_event_get_ne_name(unsigned int event)
{
	if ((event >= POWER_EVENT_NE_BEGIN) && (event < POWER_EVENT_NE_END))
		return g_power_event_ne_table[event];

	return "illegal ne";
}

static struct power_event_dev *power_event_get_dev(void)
{
	if (!g_power_event_dev) {
		hwlog_err("g_power_event_dev is null\n");
		return NULL;
	}

	return g_power_event_dev;
}

void power_event_report_uevent(const struct power_event_notify_data *n_data)
{
	char uevent_buf[POWER_EVENT_NOTIFY_SIZE] = { 0 };
	char *envp[POWER_EVENT_NOTIFY_NUM] = { uevent_buf, NULL };
	int i, ret;
	struct power_event_dev *l_dev = power_event_get_dev();

	if (!l_dev || !l_dev->sysfs_ne)
		return;

	if (!n_data || !n_data->event) {
		hwlog_err("n_data or event is null\n");
		return;
	}

	if (n_data->event_len >= POWER_EVENT_NOTIFY_SIZE) {
		hwlog_err("event_len is invalid\n");
		return;
	}

	for (i = 0; i < n_data->event_len; i++)
		uevent_buf[i] = n_data->event[i];
	hwlog_info("receive uevent_buf %s\n", uevent_buf);

	ret = kobject_uevent_env(l_dev->sysfs_ne, KOBJ_CHANGE, envp);
	if (ret < 0)
		hwlog_err("notify uevent fail, ret=%d\n", ret);
}

static void power_event_vbus_connect(struct power_event_dev *l_dev)
{
	struct power_event_notify_data n_data;

	/* ignore repeat event */
	if (l_dev->connect_state == POWER_EVENT_CONNECT)
		return;
	l_dev->connect_state = POWER_EVENT_CONNECT;

	n_data.event = "VBUS_CONNECT=";
	n_data.event_len = 13; /* length of VBUS_CONNECT= */
	power_event_report_uevent(&n_data);
}

static void power_event_vbus_disconnect(struct power_event_dev *l_dev)
{
	struct power_event_notify_data n_data;

	/* ignore repeat event */
	if (l_dev->connect_state == POWER_EVENT_DISCONNECT)
		return;
	l_dev->connect_state = POWER_EVENT_DISCONNECT;

	n_data.event = "VBUS_DISCONNECT=";
	n_data.event_len = 16; /* length of VBUS_DISCONNECT= */
	power_event_report_uevent(&n_data);

	power_if_kernel_sysfs_set(POWER_IF_OP_TYPE_ALL, POWER_IF_SYSFS_READY, 0);
}

static int power_event_notifier_call(struct notifier_block *nb,
	unsigned long event, void *data)
{
	struct power_event_dev *l_dev = power_event_get_dev();

	if (!l_dev)
		return NOTIFY_OK;

	hwlog_info("receive event %lu,%s\n", event, power_event_get_ne_name(event));

	switch (event) {
	case POWER_EVENT_NE_USB_DISCONNECT:
	case POWER_EVENT_NE_WIRELESS_DISCONNECT:
		power_event_vbus_disconnect(l_dev);
		break;
	case POWER_EVENT_NE_USB_CONNECT:
	case POWER_EVENT_NE_WIRELESS_CONNECT:
		power_event_vbus_connect(l_dev);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

int power_event_notifier_chain_register(struct notifier_block *nb)
{
	if (!nb) {
		hwlog_err("nb is null\n");
		return NOTIFY_OK;
	}

	return blocking_notifier_chain_register(&g_power_event_nh, nb);
}

int power_event_notifier_chain_unregister(struct notifier_block *nb)
{
	if (!nb) {
		hwlog_err("nb is null\n");
		return NOTIFY_OK;
	}

	return blocking_notifier_chain_unregister(&g_power_event_nh, nb);
}

void power_event_notify(unsigned long event, void *data)
{
	blocking_notifier_call_chain(&g_power_event_nh, event, data);
}

#ifdef CONFIG_SYSFS
static ssize_t power_event_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);

static struct power_sysfs_attr_info power_event_sysfs_field_tbl[] = {
	power_sysfs_attr_ro(power_event, 0440, POWER_EVENT_SYSFS_CONNECT_STATE, connect_state),
};

#define POWER_EVENT_SYSFS_ATTRS_SIZE  ARRAY_SIZE(power_event_sysfs_field_tbl)

static struct attribute *power_event_sysfs_attrs[POWER_EVENT_SYSFS_ATTRS_SIZE + 1];

static const struct attribute_group power_event_sysfs_attr_group = {
	.attrs = power_event_sysfs_attrs,
};

static ssize_t power_event_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct power_sysfs_attr_info *info = NULL;
	struct power_event_dev *l_dev = power_event_get_dev();

	if (!l_dev)
		return -EINVAL;

	info = power_sysfs_lookup_attr(attr->attr.name,
		power_event_sysfs_field_tbl, POWER_EVENT_SYSFS_ATTRS_SIZE);
	if (!info)
		return -EINVAL;

	switch (info->name) {
	case POWER_EVENT_SYSFS_CONNECT_STATE:
		return scnprintf(buf, PAGE_SIZE, "%d\n", l_dev->connect_state);
	default:
		return 0;
	}
}

static struct device *power_event_sysfs_create_group(void)
{
	power_sysfs_init_attrs(power_event_sysfs_attrs,
		power_event_sysfs_field_tbl, POWER_EVENT_SYSFS_ATTRS_SIZE);
	return power_sysfs_create_group("hw_power", "power_event",
		&power_event_sysfs_attr_group);
}

static void power_event_sysfs_remove_group(struct device *dev)
{
	power_sysfs_remove_group(dev, &power_event_sysfs_attr_group);
}
#else
static inline struct device *power_event_sysfs_create_group(void)
{
	return NULL;
}

static inline void power_event_sysfs_remove_group(struct device *dev)
{
}
#endif /* CONFIG_SYSFS */

static int __init power_event_init(void)
{
	int ret;
	struct power_event_dev *l_dev = NULL;

	l_dev = kzalloc(sizeof(*l_dev), GFP_KERNEL);
	if (!l_dev)
		return -ENOMEM;

	g_power_event_dev = l_dev;
	l_dev->nb.notifier_call = power_event_notifier_call;
	ret = power_event_notifier_chain_register(&l_dev->nb);
	if (ret)
		goto fail_free_mem;

	l_dev->dev = power_event_sysfs_create_group();
	if (l_dev->dev)
		l_dev->sysfs_ne = &l_dev->dev->kobj;
	l_dev->connect_state = POWER_EVENT_INVAID;

	return 0;

fail_free_mem:
	kfree(l_dev);
	g_power_event_dev = NULL;
	return ret;
}

static void __exit power_event_exit(void)
{
	struct power_event_dev *l_dev = g_power_event_dev;

	if (!l_dev)
		return;

	power_event_notifier_chain_unregister(&l_dev->nb);
	power_event_sysfs_remove_group(l_dev->dev);
	kfree(l_dev);
	g_power_event_dev = NULL;
}

fs_initcall_sync(power_event_init);
module_exit(power_event_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("power event module driver");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
