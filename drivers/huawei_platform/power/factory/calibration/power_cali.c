/*
 * power_cali.c
 *
 * calibration for power module
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

#include <chipset_common/hwpower/power_sysfs.h>
#include <chipset_common/hwpower/power_event_ne.h>
#include <chipset_common/hwpower/power_cmdline.h>
#include <huawei_platform/log/hw_log.h>
#include <huawei_platform/power/power_cali.h>
#include <mt-plat/mtk_battery.h>
#ifdef CONFIG_HUAWEI_OEMINFO
#include <huawei_platform/oeminfo/oeminfo_def.h>
#define OFFSET_COUNT 4
#endif

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif

#define HWLOG_TAG batt_cali
HWLOG_REGIST();

#define TBATICAL_MIN_A 650000
#define TBATICAL_MAX_A 1300000

static struct cali_para_dev *g_cali_para_dev;
static int g_valid_data_size;

static const char * const g_power_cali_device_id_table[] = {
	[POWER_CALI_TYPE_LOCAL_FG] = "mt6359",
	[POWER_CALI_TYPE_OTHERF_FG] = "rt9426_ti",
};

static int power_cali_get_device_id(const char *str)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(g_power_cali_device_id_table); i++) {
		if (!strncmp(str, g_power_cali_device_id_table[i], strlen(str)))
			return i;
	}

	return -1;
}

static int power_cali_check_type(int type)
{
	if ((type >= POWER_CALI_TYPE_BEGIN) && (type < POWER_CALI_TYPE_END))
		return 0;

	hwlog_err("invalid type=%d\n", type);
	return -1;
}

int power_cali_ops_register(struct power_bat_cali_ops *ops)
{
	struct cali_para_dev *info = g_cali_para_dev;
	int dev_id;

	if (!info || !ops || !ops->dev_name) {
		hwlog_err("info or ops is null\n");
		return -EINVAL;
	}

	dev_id = power_cali_get_device_id(ops->dev_name);
	if (dev_id < 0) {
		hwlog_err("%s ops register fail\n", ops->dev_name);
		return -EINVAL;
	}

	info->ops[dev_id] = ops;
	info->total_ops++;

	hwlog_info("total_ops=%d %d:%s ops register ok\n",
	info->total_ops, dev_id, ops->dev_name);
	return 0;
}

void cali_para_dump(void)
{
	return;
}
struct power_cali_para_save {
	int main_cur_a;
	int main_cur_b;
	int main_vol_a;
	int main_vol_b;
	int aux_cur_a;
	int aux_cur_b;
	int aux_vol_a;
	int aux_vol_b;
};
static int cali_bat_do_save(struct cali_para_dev *dev, int mode)
{
	int ret = -1;
	struct power_cali_para_save ptemp = {0};
	int i = 0;
#ifdef CONFIG_HUAWEI_OEMINFO
	struct oeminfo_info_user *nv_offset_info = NULL;

	nv_offset_info = kzalloc(sizeof(*nv_offset_info), GFP_KERNEL);
	if (!nv_offset_info) {
		hwlog_err("%s offsetinfo alloc fail\n", __func__);
		return ret;
	}

	nv_offset_info->oeminfo_operation = OEMINFO_WRITE;
	nv_offset_info->oeminfo_id = OEMINFO_CUR_OFFSET;
	nv_offset_info->valid_size = sizeof(struct power_cali_para_save);

	ptemp.main_cur_a = dev->para[POWER_CALI_TYPE_LOCAL_FG].para[POWER_CALI_PARA_CUR_A];
	ptemp.main_cur_b = dev->para[POWER_CALI_TYPE_LOCAL_FG].para[POWER_CALI_PARA_CUR_B];
	ptemp.main_vol_a = dev->para[POWER_CALI_TYPE_LOCAL_FG].para[POWER_CALI_PARA_VOL_A];
	ptemp.main_vol_b = dev->para[POWER_CALI_TYPE_LOCAL_FG].para[POWER_CALI_PARA_VOL_B];

	ptemp.aux_cur_a = dev->para[POWER_CALI_TYPE_OTHERF_FG].para[POWER_CALI_PARA_CUR_A];
	ptemp.aux_cur_b = dev->para[POWER_CALI_TYPE_OTHERF_FG].para[POWER_CALI_PARA_CUR_B];
	ptemp.aux_vol_a = dev->para[POWER_CALI_TYPE_OTHERF_FG].para[POWER_CALI_PARA_VOL_A];
	ptemp.aux_vol_b = dev->para[POWER_CALI_TYPE_OTHERF_FG].para[POWER_CALI_PARA_VOL_B];

	memcpy(nv_offset_info->oeminfo_data, &ptemp, sizeof(ptemp));

	ret = oeminfo_direct_access(nv_offset_info);
	if (ret != 0) {
		hwlog_err("%s write oeminfo fail\n", __func__);
		kfree(nv_offset_info);
		return ret;
	}
	kfree(nv_offset_info);
#endif /* CONFIG_HUAWEI_OEMINFO */

	return ret;
}

#ifdef CONFIG_SYSFS
static ssize_t cali_para_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t cali_para_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static struct power_sysfs_attr_info cali_para_sysfs_field_tbl[] = {
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_BAT_CUR_A, bat_cur_a),
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_BAT_CUR_B, bat_cur_b),
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_BAT_VOL_A, bat_vol_a),
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_BAT_VOL_B, bat_vol_b),
	power_sysfs_attr_rw(cali_para, 0440, CALI_PARA_SYSFS_BAT_CUR, bat_cur),
	power_sysfs_attr_rw(cali_para, 0440, CALI_PARA_SYSFS_BAT_VOL, bat_vol),
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_CALI_MODE, cali_mode),
	power_sysfs_attr_rw(cali_para, 0664, CALI_PARA_SYSFS_BAT_DO_SAVE, bat_do_save),
};

#define CALI_PARA_SYSFS_ATTRS_SIZE  ARRAY_SIZE(cali_para_sysfs_field_tbl)

static struct attribute *cali_para_sysfs_attrs[CALI_PARA_SYSFS_ATTRS_SIZE + 1];

static const struct attribute_group cali_para_sysfs_attr_group = {
	.attrs = cali_para_sysfs_attrs,
};
static int power_cali_get_bat_current(struct cali_para_dev *l_dev)
{
	struct power_bat_cali_ops *ops = NULL;
	int val = 0;
	int mode = 0;

	if (!l_dev || (l_dev->cali_mode < POWER_CALI_TYPE_BEGIN) ||
		(l_dev->cali_mode > POWER_CALI_TYPE_END) || !l_dev->ops[l_dev->cali_mode])
		return 0;
	mode = l_dev->cali_mode;
	ops = l_dev->ops[mode];

	if ((!ops->get_current) || ops->get_current(&val) != 0)
		return 0;

	return val;
}
static int power_cali_get_bat_voltage(struct cali_para_dev *l_dev)
{
	struct power_bat_cali_ops *ops = NULL;
	int val = 0;
	int mode = 0;

	if (!l_dev || (l_dev->cali_mode < POWER_CALI_TYPE_BEGIN) ||
		(l_dev->cali_mode > POWER_CALI_TYPE_END) || !l_dev->ops[l_dev->cali_mode])
		return 0;
	mode = l_dev->cali_mode;
	ops = l_dev->ops[mode];

	if ((!ops->get_voltage)  || ops->get_voltage(&val) != 0)
		return 0;

	return val * 1000;
}

static ssize_t cali_para_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct power_sysfs_attr_info *info = NULL;
	struct cali_para_dev *l_dev = g_cali_para_dev;
	int mode;
	struct power_bat_cali_ops *ops = NULL;
	int val = 0;

	if (!l_dev)
		return -EINVAL;

	info = power_sysfs_lookup_attr(attr->attr.name,
		cali_para_sysfs_field_tbl, CALI_PARA_SYSFS_ATTRS_SIZE);
	if (!info)
		return -EINVAL;

	mode = l_dev->cali_mode;

	switch (info->name) {
	case CALI_PARA_SYSFS_BAT_CUR_A:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->para[mode].para[POWER_CALI_PARA_CUR_A]);
	case CALI_PARA_SYSFS_BAT_CUR_B:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->para[mode].para[POWER_CALI_PARA_CUR_B]);
	case CALI_PARA_SYSFS_BAT_VOL_A:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->para[mode].para[POWER_CALI_PARA_VOL_A]);
	case CALI_PARA_SYSFS_BAT_VOL_B:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->para[mode].para[POWER_CALI_PARA_VOL_B]);
	case CALI_PARA_SYSFS_BAT_CUR:
		val = power_cali_get_bat_current(l_dev);
		l_dev->para[mode].bat_cur = val;
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n", val);
	case CALI_PARA_SYSFS_BAT_VOL:
		val = power_cali_get_bat_voltage(l_dev);
		l_dev->para[mode].bat_vol = val;
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n", val);
	case CALI_PARA_SYSFS_CALI_MODE:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->cali_mode);
	case CALI_PARA_SYSFS_BAT_DO_SAVE:
		return scnprintf(buf, CALI_PARA_WR_BUF_SIZE, "%d\n",
			l_dev->para[mode].do_save);
	default:
		return 0;
	}
}

static ssize_t cali_para_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	struct power_sysfs_attr_info *info = NULL;
	struct cali_para_dev *l_dev = g_cali_para_dev;
	int mode = 0;

	if (!power_cmdline_is_factory_mode()) {
		hwlog_info("only factory version can set\n");
		return -EINVAL;
	}

	if (!l_dev)
		return -EINVAL;

	info = power_sysfs_lookup_attr(attr->attr.name,
		cali_para_sysfs_field_tbl, CALI_PARA_SYSFS_ATTRS_SIZE);
	if (!info)
		return -EINVAL;

	if (kstrtoint(buf, 10, &value) != 0) {
		hwlog_err("unable to parse input:%s\n", buf);
		return -EINVAL;
	}
	hwlog_info("info:%d set: value=%d\n", info->name, value);
	mode = l_dev->cali_mode;
	switch (info->name) {
	case CALI_PARA_SYSFS_BAT_CUR_A:
		l_dev->para[mode].para[POWER_CALI_PARA_CUR_A] = value;
		if (l_dev->ops[mode] && l_dev->ops[mode]->set_current_gain)
			l_dev->ops[mode]->set_current_gain(value);
		break;
	case CALI_PARA_SYSFS_BAT_CUR_B:
		l_dev->para[mode].para[POWER_CALI_PARA_CUR_B] = value;
		if (l_dev->ops[mode] && l_dev->ops[mode]->set_current_offset)
			l_dev->ops[mode]->set_current_offset(value);
		break;
	case CALI_PARA_SYSFS_BAT_VOL_A:
		l_dev->para[mode].para[POWER_CALI_PARA_VOL_A] = value;
		if (l_dev->ops[mode] && l_dev->ops[mode]->set_voltage_gain)
			l_dev->ops[mode]->set_voltage_gain(value);
		break;
	case CALI_PARA_SYSFS_BAT_VOL_B:
		l_dev->para[mode].para[POWER_CALI_PARA_VOL_B] = value;
		if (l_dev->ops[mode] && l_dev->ops[mode]->set_voltage_offset)
			l_dev->ops[mode]->set_voltage_offset(value);
		break;
	case CALI_PARA_SYSFS_CALI_MODE:
		if (value >= POWER_CALI_TYPE_END) {
			hwlog_err("only support local cali %s\n", buf);
			return -EINVAL;
		}
		l_dev->cali_mode = value;
	case CALI_PARA_SYSFS_BAT_DO_SAVE:
		l_dev->para[mode].do_save = cali_bat_do_save(l_dev, mode);
		break;
	default:
		break;
	}

	return count;
}

static struct device *cali_para_sysfs_create_group(void)
{
	power_sysfs_init_attrs(cali_para_sysfs_attrs,
		cali_para_sysfs_field_tbl, CALI_PARA_SYSFS_ATTRS_SIZE);
	return power_sysfs_create_group("hw_power", "factory",
		&cali_para_sysfs_attr_group);
}

static void cali_para_sysfs_remove_group(struct device *dev)
{
	power_sysfs_remove_group(dev, &cali_para_sysfs_attr_group);
}
#else
static inline struct device *cali_para_sysfs_create_group(void)
{
	return NULL;
}

static inline void cali_para_sysfs_remove_group(struct device *dev)
{
}
#endif /* CONFIG_SYSFS */

static void cali_para_get(void)
{
	const char *p = NULL;
	int para[POWER_CALI_PARA_END * POWER_CALI_TYPE_END] = {0};
	int i;
	int j;

	p = strstr(saved_command_line, "f0_ca=");
	if (p)
		sscanf(p, "f0_ca=0x%x f0_cb=0x%x f0_va=0x%x f0_vb=0x%x f1_ca=0x%x f1_cb=0x%x f1_va=0x%x f1_vb=0x%x",
			&para[POWER_CALI_PARA_CUR_A], &para[POWER_CALI_PARA_CUR_B], &para[POWER_CALI_PARA_VOL_A], &para[POWER_CALI_PARA_VOL_B],
			&para[POWER_CALI_PARA_CUR_A + POWER_CALI_PARA_END], &para[POWER_CALI_PARA_CUR_B + POWER_CALI_PARA_END],
			&para[POWER_CALI_PARA_VOL_A + POWER_CALI_PARA_END], &para[POWER_CALI_PARA_VOL_B + POWER_CALI_PARA_END]);

	for (i = 0; i < POWER_CALI_TYPE_END; i++) {
		for (j = 0; j < POWER_CALI_PARA_END; j++) {
			if (para[i] >=0 && para[i] < TBATICAL_MAX_A)
				g_cali_para_dev->para[i].para[j] = para[i * POWER_CALI_PARA_END + j];
		}
	}

	cali_para_dump();
}

static int __init power_cali_init(void)
{
	struct cali_para_dev *cali_dev = NULL;

	cali_dev = kzalloc(sizeof(*cali_dev), GFP_KERNEL);
	if (!cali_dev)
		return -ENOMEM;

	g_cali_para_dev = cali_dev;
	cali_dev->dev = cali_para_sysfs_create_group();
	cali_para_get();
	return 0;
}

struct power_cali_para* power_cali_get_para(int mode)
{
	if (!g_cali_para_dev || mode < POWER_CALI_TYPE_BEGIN || mode >= POWER_CALI_TYPE_END)
		return NULL;

	return &g_cali_para_dev->para[mode];
}

int power_cali_cur_tune_update(int cur_tune)
{
	int mode = POWER_CALI_TYPE_LOCAL_FG;
	if (!g_cali_para_dev)
		return cur_tune;

	int cur_a =g_cali_para_dev->para[mode].para[POWER_CALI_PARA_CUR_A];
	if ((cur_a > TBATICAL_MIN_A) && (cur_a < TBATICAL_MAX_A))
		cur_tune = ((s64)cur_a * cur_tune) / OFFSET_DEFAULT_VAL;

	return cur_tune;
}

int power_cali_batt_vol_update(int vol)
{
	int mode = POWER_CALI_TYPE_LOCAL_FG;

	if (!g_cali_para_dev)
		return vol;

	int vol_a = g_cali_para_dev->para[mode].para[POWER_CALI_PARA_VOL_A];
	int vol_b = g_cali_para_dev->para[mode].para[POWER_CALI_PARA_VOL_B];
	if ((vol_a > TBATICAL_MIN_A) && (vol_a < TBATICAL_MAX_A)) {
		vol = ((s64)vol_a * vol) / OFFSET_DEFAULT_VAL;
		vol = ((s64)vol * 1000 + vol_b) / 1000; /* vol_b is uv, vol is mv,1000 for cali */
	}

	return vol;
}

static void __exit power_cali_exit(void)
{
	struct cali_para_dev *cali_dev = g_cali_para_dev;

	if (!cali_dev)
		return;

	cali_para_sysfs_remove_group(cali_dev->dev);
	kfree(cali_dev);
	g_cali_para_dev = NULL;
}

subsys_initcall_sync(power_cali_init);
module_exit(power_cali_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("battery calibration module driver");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
