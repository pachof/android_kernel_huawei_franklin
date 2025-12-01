/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * Description: This file is the internal header file for the
 *              app accelerator module.
 * Author: youpeigang@huawei.com
 * Create: 2020-01-10
 */

#ifndef APP_ACCELERATOR_H
#define APP_ACCELERATOR_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/netfilter.h>
#include <linux/timer.h>
#include <uapi/linux/netfilter.h>
#include "netlink_handle.h"
#include "ip_para_collec.h"

#define UID_INVALID (-1)
#define MAX_TCP_ACC_LEN 12

#define BITS_PER_SECONDE_DEFAULT (2048 * 1024 * 8)
#define STATICS_PACKET_NUM_INTERVAL 50
#define SPEED_TEST_UID 0

/* Notification request issued by the upper layer is defined as: */
struct app_acc_req_msg {
	/* Event enumeration values */
	u16 type;
	/* The length behind this field, the limit lower 8 */
	u16 len;
	u32 uid;
	u32 threshold;
	u32 packet_threshold;
};

msg_process *app_acc_init(notify_event *fn);
void app_acc_process(struct req_msg_head *msg);
int app_acc_get_uid(void);
u32 app_acc_get_theshold(void);
void app_acc_report(u32 disorder_packets, u32 total_packets,
	u32 dicard_bytes, u32 total_bytes);
bool app_acc_start_check(struct sock *sk);
void app_sock_acc_start_check(struct sock *sk, struct sk_buff *skb);
#endif
