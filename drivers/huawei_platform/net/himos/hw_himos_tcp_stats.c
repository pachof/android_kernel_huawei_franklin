/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * File name: hw_himos_aweme_detect.c
 * Description: This file matain the states of tcp of himos
 * Author: lengjing@huawei.com
 * Version: 0.1
 * Date: 2020/03/14
 */

#include "hw_himos_stats_common.h"
#include "hw_himos_aweme_detect.h"
#include "hw_himos_stats_report.h"
#include <huawei_platform/net/hw_himos_tcp_stats.h>

void himos_tcp_stats(struct sock *sk, struct msghdr *old, struct msghdr *new,
	int inbytes, int outbytes)

{
	struct himos_stats_common *info = NULL;
	__s32 uid;
	int type;

	if (!sk)
		return;

	spin_lock_bh(&stats_info_lock);

	uid = (__s32)(sk->sk_uid.val);
	info = himos_get_stats_info_by_uid(uid);
	if (info == NULL) {
		spin_unlock_bh(&stats_info_lock);
		return;
	}
	type = info->type;
	spin_unlock_bh(&stats_info_lock);

	switch(type) {
	case HIMOS_STATS_TYPE_AWEME:
	case HIMOS_STATS_TYPE_KWAI:
		himos_aweme_tcp_stats(sk, old, new, inbytes, outbytes);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(himos_tcp_stats);
