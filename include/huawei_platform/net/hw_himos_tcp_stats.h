/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * File name: hw_himos_aweme_detect.c
 * Description: This file matain the states of tcp of himos
 * Author: lengjing@huawei.com
 * Version: 0.1
 * Date: 2020/03/14
 */

#ifndef		HW_HIMOS_TCP_STATS_H
#define		HW_HIMOS_TCP_STATS_H

#include <net/sock.h>

#include <linux/socket.h>

void himos_tcp_stats(struct sock *sk, struct msghdr *old, struct msghdr *new,
	int inbytes, int outbytes);

#endif
