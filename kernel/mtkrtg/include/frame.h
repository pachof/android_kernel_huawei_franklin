/*
 * frame.h
 *
 * Frame declaration
 *
 * Copyright (c) 2019-2020 Huawei Technologies Co., Ltd.
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

#ifndef FRAME_EXTERN_H
#define FRAME_EXTERN_H

bool is_frame_task(struct task_struct *task);
int set_frame_rate(int rate);
int set_frame_margin(int margin);
int set_frame_status(unsigned long status);
int set_frame_max_util(int max_util);
void set_frame_sched_state(bool enable);
int set_frame_timestamp(unsigned long timestamp);
int set_frame_min_util(int min_util);
void update_frame_thread(int pid, int tid);
int update_frame_isolation(void);

#endif // FRAME_EXTERN_H
