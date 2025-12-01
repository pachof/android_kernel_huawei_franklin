/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * File name: hw_himos_aweme_detect.h
 * Description: This file do the detect function of himos
 * Author: lengjing@huawei.com
 * Version: 0.1
 * Date: 2020/03/14
 */

#ifndef     HW_HIMOS_AWEME_DETECT_H
#define     HW_HIMOS_AWEME_DETECT_H

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/genetlink.h>

#include <net/genetlink.h>

#include "hw_himos_stats_report.h"
#include "hw_himos_stats_common.h"

struct himos_sport_key_dict
{
	unsigned short sport;
	char key[STALL_KEY_MAX];
};

struct himos_aweme_detect_info
{
	//this is must be the first member
	struct himos_stats_common comm;

	__u32 interval;
	int keepalive;

	int detecting, cur_detect_index;
	struct stall_info stalls[AWEME_STALL_WINDOW];
	int cur_index, valid_num;

	unsigned short local_sport;
	unsigned int local_inbytes;

	struct himos_aweme_search_key keys;

	int cur_dict_index;
	int dict_num;
	struct himos_sport_key_dict sport_key_dict[AWEME_STALL_WINDOW];
};

int himos_start_aweme_detect(struct genl_info *info);
int himos_stop_aweme_detect(struct genl_info *info);
int himos_keepalive_aweme_detect(struct genl_info *info);
int himos_update_aweme_stall_info(struct sk_buff *skb, struct genl_info *info);
void himos_aweme_tcp_stats(struct sock *sk, struct msghdr *old, struct msghdr *new, int inbytes, int outbytes);

int himos_aweme_init(void) __init;

#endif
