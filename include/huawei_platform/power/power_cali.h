/*
 * power_ui.h
 *
 * ui display for power module
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

#ifndef _POWER_CALI_H_
#define _POWER_CALI_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#define OFFSET_DEFAULT_VAL 1000000
#define CALI_PARA_RD_BUF_SIZE  32
#define CALI_PARA_WR_BUF_SIZE  32
#define POWER_CALI_PARA_SIZE 4


enum {
	POWER_CALI_PARA_BEGIN = 0,
	POWER_CALI_PARA_CUR_A = POWER_CALI_PARA_BEGIN,
	POWER_CALI_PARA_CUR_B,
	POWER_CALI_PARA_VOL_A,
	POWER_CALI_PARA_VOL_B,
	POWER_CALI_PARA_END,
};

enum {
	POWER_CALI_TYPE_BEGIN = 0,
	POWER_CALI_TYPE_LOCAL_FG = POWER_CALI_TYPE_BEGIN, /* for fuel guage*/
	POWER_CALI_TYPE_OTHERF_FG,
	POWER_CALI_TYPE_END,
};

struct power_cali_para {
	int para[POWER_CALI_PARA_END];
	int bat_cur;
	int bat_vol;
	int do_save;
};

enum power_cali_sysfs_type {
	CALI_PARA_SYSFS_BEGIN = 0,
	CALI_PARA_SYSFS_BAT_CUR_A = CALI_PARA_SYSFS_BEGIN,
	CALI_PARA_SYSFS_BAT_CUR_B,
	CALI_PARA_SYSFS_BAT_VOL_A,
	CALI_PARA_SYSFS_BAT_VOL_B,
	CALI_PARA_SYSFS_BAT_CUR,
	CALI_PARA_SYSFS_BAT_VOL,
	CALI_PARA_SYSFS_CALI_MODE,
	CALI_PARA_SYSFS_BAT_DO_SAVE,
	POWER_CALI_SYSFS_END,
};

struct power_bat_cali_ops {
	const char *dev_name;
	void *dev_data;
	int (*get_current)(int *);
	int (*get_voltage)(int *);
	int (*get_current_offset)(int *);
	int (*get_current_gain)(unsigned int *);
	int (*get_voltage_offset)(int *);
	int (*get_voltage_gain)(unsigned int *);
	int (*set_current_offset)(int);
	int (*set_current_gain)(unsigned int);
	int (*set_voltage_offset)(int);
	int (*set_voltage_gain)(unsigned int);
};

struct cali_para_dev {
	struct device *dev;
	struct mutex cali_lock;
	unsigned int total_ops;
	int cali_mode;
	struct power_cali_para para[POWER_CALI_TYPE_END];
	struct power_bat_cali_ops *ops[POWER_CALI_TYPE_END];
};

struct power_cali_para* power_cali_get_para(int mode);
int power_cali_ops_register(struct power_bat_cali_ops *ops);
int power_cali_cur_tune_update(int cur_tune);
int power_cali_batt_vol_update(int vol);
#endif /* _POWER_CALI_H_ */
