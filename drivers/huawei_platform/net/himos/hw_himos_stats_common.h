/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * File name: hw_himos_aweme_detect.c
 * Description: This file matain the status of himos
 * Author: lengjing@huawei.com
 * Version: 0.1
 * Date: 2020/03/14
 */

#ifndef HW_HIMOS_STATS_COMMON_H
#define HW_HIMOS_STATS_COMMON_H

#include <linux/types.h>
#include <linux/list.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/ktime.h>


struct himos_stats_common
{
	__u32 portid;   //user portid
	possible_net_t net;  //for reply

	struct list_head node;

	//the value of 'enum HIMOS_STATS_TYPE'
	int type;
	__s32 uid;
};

int himos_stats_common_init(void) __init;

struct himos_stats_common *himos_get_stats_info_by_uid(__s32 uid);
void himos_stats_common_reset(struct himos_stats_common *info);

#endif
