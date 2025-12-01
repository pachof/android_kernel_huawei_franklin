/*
 * power_event_ne.h
 *
 * notifier event for power module
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

#ifndef _POWER_EVENT_NE_H_
#define _POWER_EVENT_NE_H_

#define POWER_EVENT_NOTIFY_SIZE 1024
#define POWER_EVENT_NOTIFY_NUM  2

/*
 * define notifier event for power module
 * NE is simplified identifier with notifier event
 */
enum power_event_ne_list {
	POWER_EVENT_NE_BEGIN = 0,
	/* section: for connect & disconnect */
	POWER_EVENT_NE_USB_DISCONNECT = POWER_EVENT_NE_BEGIN,
	POWER_EVENT_NE_USB_CONNECT,
	POWER_EVENT_NE_WIRELESS_DISCONNECT,
	POWER_EVENT_NE_WIRELESS_CONNECT,
	/* section: for start & stop charging */
	POWER_EVENT_NE_START_CHARGING,
	POWER_EVENT_NE_STOP_CHARGING,
	POWER_EVENT_NE_SUSPEND_CHARGING,
	/* section: for soc decimal */
	POWER_EVENT_NE_SOC_DECIMAL_DC,
	POWER_EVENT_NE_SOC_DECIMAL_WL_DC,
	/* section: for water detect */
	POWER_EVENT_NE_WD_REPORT_DMD,
	POWER_EVENT_NE_WD_REPORT_UEVENT,
	POWER_EVENT_NE_WD_DETECT_BY_USB_DP_DN,
	POWER_EVENT_NE_WD_DETECT_BY_USB_ID,
	POWER_EVENT_NE_WD_DETECT_BY_USB_GPIO,
	POWER_EVENT_NE_WD_DETECT_BY_AUDIO_DP_DN,
	/* section: for power supply */
	POWER_EVENT_NE_PSY_UPDATE_BATTERY,
	POWER_EVENT_NE_PSY_UPDATE_RAW_BATTERY,
	POWER_EVENT_NE_PSY_UPDATE_ASSITST_BATTERY,
	POWER_EVENT_NE_PSY_UPDATE_MAINS,
	POWER_EVENT_NE_PSY_UPDATE_USB,
	POWER_EVENT_NE_PSY_UPDATE_WIRELESS,
	/* section: for uvdm */
	POWER_EVENT_NE_UVDM_RECEIVE,
	POWER_EVENT_NE_END,
};

struct power_event_notify_data {
	const char *event;
	int event_len;
};

int power_event_notifier_chain_register(struct notifier_block *nb);
int power_event_notifier_chain_unregister(struct notifier_block *nb);
void power_event_notify(unsigned long event, void *data);
void power_event_report_uevent(const struct power_event_notify_data *n_data);

#endif /* _POWER_EVENT_NE_H_ */
