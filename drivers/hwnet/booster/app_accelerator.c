/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * Description: This module is for app accelerator
 * Author: youpeigang@huawei.com
 * Create: 2020-01-10
 */

#include "app_accelerator.h"

#include <linux/net.h>
#include <net/sock.h>
#include <linux/list.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/tcp.h>

int g_acc_uid = UID_INVALID;
/* default 2048 * 1024 * 8 bps */
u32 g_acc_threshold = BITS_PER_SECONDE_DEFAULT;
u32 g_acc_packet_threshold = 50;

msg_process *app_acc_init(notify_event *fn)
{
	if (!fn) {
		pr_info("app_acc_init null parameter");
		return NULL;
	}
	pr_info("app_acc_init");
	g_acc_uid = UID_INVALID;
	return app_acc_process;
}

void app_acc_process(struct req_msg_head *msg)
{
	struct app_acc_req_msg *req = (struct app_acc_req_msg *)msg;
	if (!msg)
		return;
	if (msg->len != sizeof(struct app_acc_req_msg))
		return;
	pr_info("[APP_ACC] type :%d", msg->type);
	if (msg->type == APP_ACCELARTER_CMD) {
		g_acc_uid = req->uid;
		if (req->threshold < BITS_PER_SECONDE_DEFAULT)
			g_acc_threshold = BITS_PER_SECONDE_DEFAULT;
		else
			g_acc_threshold = req->threshold;
		if (req->packet_threshold < STATICS_PACKET_NUM_INTERVAL)
			g_acc_packet_threshold = STATICS_PACKET_NUM_INTERVAL;
		else
			g_acc_packet_threshold = req->packet_threshold;

		pr_info("[APP_ACC] final threshold:%d final packet threshold:%d",
			g_acc_threshold, g_acc_packet_threshold);
	}
}

int app_acc_get_uid()
{
	return g_acc_uid;
}

u32 app_acc_get_theshold()
{
	return g_acc_threshold;
}

bool app_acc_start_check(struct sock *sk)
{
	if (sk->start_acc_flag && (sk->sk_state != TCP_TIME_WAIT))
		return true; //lint !e1564
	else
		return false; //lint !e1564
}

void app_sock_acc_start_check(struct sock *sk, struct sk_buff *skb)
{
	unsigned long time_long;
	unsigned long bytes_speed;
	int uid;
	struct inet_sock *inet = NULL;

	if (!sk || (sk->sk_state == TCP_TIME_WAIT) || !skb)
		return;

	uid = (int)sk->sk_uid.val;
	if (app_acc_get_uid() != uid)
		return;

	if (sk->start_acc_flag)
		return;

	sk->packet_num++;
	if (sk->packet_num % STATICS_PACKET_NUM_INTERVAL)
		return;

	time_long = jiffies - sk->sk_born_stamp;
	/* zero indicates no time difference */
	if (time_long == 0)
		return;

	bytes_speed = (tcp_sk(sk)->bytes_received * HZ) / time_long;
	/* bps unit */
	bytes_speed = bytes_speed * 8;
	if (bytes_speed > app_acc_get_theshold()) {
		inet = inet_sk(sk);
		sk->start_acc_flag = true; //lint !e1564
		pr_info("[APP_ACC] start_acc_flag port:%d", htons(inet->inet_sport));
	}
	pr_info("[APP_ACC] bytes_speed:%ld", bytes_speed);
}
