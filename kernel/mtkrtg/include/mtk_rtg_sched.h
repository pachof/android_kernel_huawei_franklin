/*
 * mtk_rtg_sched.h
 *
 * rtg sched header
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

#ifndef MTK_RTG_SCHED_H
#define MTK_RTG_SCHED_H

enum rtg_freq_update_flags {
	RTG_FREQ_FORCE_UPDATE = (1 << 0),
	RTG_FREQ_NORMAL_UPDATE = (1 << 1),
};

int alloc_related_thread_groups(void);
int sched_set_group_id(pid_t pid, unsigned int group_id);
int _sched_set_group_id(struct task_struct *p, unsigned int group_id);
struct related_thread_group *lookup_related_thread_group(unsigned int group_id);
int sched_set_group_window_size(unsigned int grp_id, unsigned int rate);
int sched_set_group_window_rollover(unsigned int grp_id);
int sched_set_group_freq_update_interval(unsigned int grp_id,
	unsigned int interval);
int sched_set_group_util_invalid_interval(unsigned int grp_id,
	unsigned int interval);
int sched_set_group_load_mode(struct rtg_load_mode *mode);
unsigned int sched_get_group_id(struct task_struct *p);
void add_new_task_to_grp(struct task_struct *new);
void sched_update_rtg_tick(struct task_struct *p);
struct group_cpu_time *group_update_cpu_time(struct rq *rq,
	struct related_thread_group *grp);
bool group_should_update_freq(struct related_thread_group *grp, int cpu,
	unsigned int flags);
void sched_set_group_min_util(struct related_thread_group *grp, int min_util);
#endif
