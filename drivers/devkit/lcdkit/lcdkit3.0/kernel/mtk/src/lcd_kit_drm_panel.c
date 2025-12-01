/*
 * lcd_kit_drm_panel.c
 *
 * lcdkit display function for lcd driver
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
#include "lcd_kit_drm_panel.h"
#include "lcd_kit_utils.h"
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_log.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_disp_ccorr.h"
#endif

#if defined CONFIG_HUAWEI_DSM
static struct dsm_dev dsm_lcd = {
	.name = "dsm_lcd",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = 1024,
};

unsigned int esd_recovery_level = 0;
struct dsm_client *lcd_dclient = NULL;
struct dsm_lcd_record lcd_record;
#endif
#define TE_60_TIME 16667
#define TE_90_TIME 11111
static struct mipi_dsi_device *g_dsi = NULL;
static struct timer_list g_backlight_timer;
static const unsigned int g_fps_90hz = 90;
static struct mtk_panel_info lcd_kit_info = {0};
static struct drm_display_mode default_mode = {0};
static struct drm_display_mode mode_list[LCD_FPS_SCENCE_MAX];
#ifdef CONFIG_MTK_PANEL_EXT
static struct mtk_panel_params ext_params = { 0 };
static struct mtk_panel_params ext_params_list[LCD_FPS_SCENCE_MAX];
static struct mtk_panel_funcs g_ext_funcs;
#endif
static struct lcd_kit_disp_info g_lcd_kit_disp_info;

struct lcd_kit_disp_info *lcd_kit_get_disp_info(void)
{
	return &g_lcd_kit_disp_info;
}

#if defined CONFIG_HUAWEI_DSM
struct dsm_client *lcd_kit_get_lcd_dsm_client(void)
{
	return lcd_dclient;
}
#endif

struct mtk_panel_info *lcm_get_panel_info(void)
{
	return &lcd_kit_info;
}

int is_mipi_cmd_panel(void)
{
	if (lcd_kit_info.panel_dsi_mode == 0)
		return 1;
	return 0;
}

void  lcd_kit_bl_ic_set_backlight(unsigned int bl_level)
{
	struct lcd_kit_bl_ops *bl_ops = NULL;

	esd_recovery_level = bl_level;
	if (lcd_kit_info.bl_ic_ctrl_mode) {
		bl_ops = lcd_kit_get_bl_ops();
		if (!bl_ops) {
			LCD_KIT_INFO("bl_ops is null!\n");
			return;
		}
		if (bl_ops->set_backlight)
			bl_ops->set_backlight(bl_level);
	}
}

static void lcd_kit_set_thp_proximity_state(int power_state)
{
	if (!common_info->thp_proximity.support) {
		LCD_KIT_INFO("thp_proximity not support!\n");
		return;
	}
	common_info->thp_proximity.panel_power_state = power_state;
}

static void lcd_kit_set_thp_proximity_sem(bool sem_lock)
{
	if (!common_info->thp_proximity.support) {
		LCD_KIT_INFO("thp_proximity not support!\n");
		return;
	}
	if (sem_lock == true)
		down(&disp_info->thp_second_poweroff_sem);
	else
		up(&disp_info->thp_second_poweroff_sem);
}

void lcm_set_panel_state(unsigned int state)
{
	lcd_kit_info.panel_state = state;
}

unsigned int lcm_get_panel_state(void)
{
	return lcd_kit_info.panel_state;
}

unsigned int lcm_get_panel_backlight_max_level(void)
{
	return lcd_kit_info.bl_max;
}

int lcm_rgbw_mode_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = LCD_KIT_OK;
	struct display_engine_ddic_rgbw_param *param = NULL;

	if (dev == NULL) {
		LCD_KIT_ERR("dev is null\n");
		return LCD_KIT_FAIL;
	}
	if (data == NULL) {
		LCD_KIT_ERR("data is null\n");
		return LCD_KIT_FAIL;
	}
	param = (struct display_engine_ddic_rgbw_param *)data;
	memcpy(&g_lcd_kit_disp_info.ddic_rgbw_param, param, sizeof(*param));
	ret = lcd_kit_rgbw_set_handle();
	if (ret < 0)
		LCD_KIT_ERR("set rgbw fail\n");
	return ret;
}

int lcm_rgbw_mode_get_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = LCD_KIT_OK;
	struct display_engine_ddic_rgbw_param *param = NULL;

	if (dev == NULL) {
		LCD_KIT_ERR("dev is null\n");
		return LCD_KIT_FAIL;
	}
	if (data == NULL) {
		LCD_KIT_ERR("data is null\n");
		return LCD_KIT_FAIL;
	}
	param = (struct display_engine_ddic_rgbw_param *)data;
	param->ddic_panel_id = g_lcd_kit_disp_info.ddic_rgbw_param.ddic_panel_id;
	param->ddic_rgbw_mode = g_lcd_kit_disp_info.ddic_rgbw_param.ddic_rgbw_mode;
	param->ddic_rgbw_backlight = g_lcd_kit_disp_info.ddic_rgbw_param.ddic_rgbw_backlight;
	param->pixel_gain_limit = g_lcd_kit_disp_info.ddic_rgbw_param.pixel_gain_limit;

	LCD_KIT_INFO("get RGBW parameters success\n");
	return ret;
}

int lcm_display_engine_get_panel_info(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = LCD_KIT_OK;
	struct display_engine_panel_info_param *param = NULL;

	if (dev == NULL) {
		LCD_KIT_ERR("dev is null\n");
		return LCD_KIT_FAIL;
	}
	if (data == NULL) {
		LCD_KIT_ERR("data is null\n");
		return LCD_KIT_FAIL;
	}
	param = (struct display_engine_panel_info_param *)data;
	param->width = lcd_kit_info.xres;
	param->height = lcd_kit_info.yres;
	param->maxluminance = lcd_kit_info.maxluminance;
	param->minluminance = lcd_kit_info.minluminance;
	param->maxbacklight = lcd_kit_info.bl_max;
	param->minbacklight = lcd_kit_info.bl_min;

	LCD_KIT_INFO("get panel info parameters success\n");
	return ret;
}

int lcm_display_engine_init(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = LCD_KIT_OK;
	struct display_engine *param = NULL;

	if (dev == NULL) {
		LCD_KIT_ERR("dev is null\n");
		return LCD_KIT_FAIL;
	}
	if (data == NULL) {
		LCD_KIT_ERR("data is null\n");
		return LCD_KIT_FAIL;
	}
	param = (struct display_engine *)data;
	/* 0:no support  1:support */
	if (g_lcd_kit_disp_info.rgbw.support == 0)
		param->ddic_rgbw_support = 0;
	else
		param->ddic_rgbw_support = 1;

	LCD_KIT_INFO("display engine init success\n");
	return ret;
}

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcd_kit_timerout_function(unsigned long arg)
{
	if (disp_info->bl_is_shield_backlight)
		disp_info->bl_is_shield_backlight = false;
	del_timer(&g_backlight_timer);
	disp_info->bl_is_start_timer = false;
	LCD_KIT_INFO("Sheild backlight 1.2s timeout, remove the bl sheild\n");
}

static void lcd_kit_enable_hbm_timer(void)
{
	if (disp_info->bl_is_start_timer == true) {
		// if timer is not timeout, restart timer
		mod_timer(&g_backlight_timer, (jiffies + 12 * HZ / 10)); // 1.2s
		return;
	}
	init_timer(&g_backlight_timer);
	g_backlight_timer.expires = jiffies + 12 * HZ / 10; // 1.2s
	g_backlight_timer.data = 0;
	g_backlight_timer.function = lcd_kit_timerout_function;
	add_timer(&g_backlight_timer);
	disp_info->bl_is_start_timer = true;
}

static void lcd_kit_disable_hbm_timer(void)
{
	if (disp_info->bl_is_start_timer == false)
		return;

	del_timer(&g_backlight_timer);
	disp_info->bl_is_start_timer = false;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = NULL;

	LCD_KIT_INFO("enter\n");
	if (!ctx->enabled)
		return LCD_KIT_OK;
	dsi = to_mipi_dsi_device(ctx->dev);
	if (dsi == NULL) {
		LCD_KIT_ERR("dsi is null\n");
		return LCD_KIT_OK;
	}

	ctx->enabled = false;
	if (!disp_info->pre_power_off) {
		LCD_KIT_INFO("exit\n");
		return LCD_KIT_OK;
	}
	lcm_set_panel_state(LCD_POWER_STATE_OFF);
	if (common_ops->panel_power_off)
		common_ops->panel_power_off(dsi);
	LCD_KIT_INFO("exit\n");
	return LCD_KIT_OK;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = NULL;

	LCD_KIT_INFO("enter\n");
	if (!ctx) {
		LCD_KIT_ERR("ctx is null\n");
		return LCD_KIT_OK;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi) {
		LCD_KIT_ERR("dsi is null\n");
		return LCD_KIT_OK;
	}
	if (!ctx->prepared)
		return LCD_KIT_OK;
	if (common_info->hbm.support)
		common_info->hbm.hbm_level_current = 0;

	if (disp_info->bl_is_shield_backlight == true)
		disp_info->bl_is_shield_backlight = false;
	lcd_kit_disable_hbm_timer();
	lcm_set_panel_state(LCD_POWER_STATE_OFF);
	lcd_kit_set_thp_proximity_sem(true);
	if (common_ops == NULL){
		LCD_KIT_ERR("common_ops NULL\n");
		return LCD_KIT_FAIL;
	}
	if (!disp_info->pre_power_off && common_ops->panel_power_off)
		common_ops->panel_power_off(dsi);

	lcd_kit_set_thp_proximity_state(POWER_OFF);
	disp_info->alpm_state = ALPM_OUT;

	ctx->error = 0;
	ctx->prepared = false;
	ctx->hbm_en = false;

	lcd_kit_set_thp_proximity_sem(false);
	LCD_KIT_INFO("exit\n");
	return LCD_KIT_OK;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = NULL;
	int ret;

	LCD_KIT_INFO("enter\n");
	if (ctx == NULL) {
		LCD_KIT_ERR("ctx is null\n");
		return LCD_KIT_OK;
	}
	dsi = to_mipi_dsi_device(ctx->dev);
	if (dsi == NULL) {
		LCD_KIT_ERR("dsi is null\n");
		return LCD_KIT_OK;
	}
	if (ctx->prepared)
		return LCD_KIT_OK;

	lcd_kit_set_thp_proximity_sem(true);
	if (common_ops == NULL){
		LCD_KIT_ERR("common_ops NULL\n");
		return LCD_KIT_FAIL;
	}
	if (common_ops->panel_power_on)
		common_ops->panel_power_on(dsi);
	/* record panel on time */
	if (disp_info->quickly_sleep_out.support)
		lcd_kit_disp_on_record_time();
	lcm_set_panel_state(LCD_POWER_STATE_ON);
	lcd_kit_set_thp_proximity_state(POWER_ON);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	lcd_kit_set_thp_proximity_sem(false);
	LCD_KIT_INFO("exit\n");
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	LCD_KIT_INFO("enter\n");
	if (ctx->enabled)
		return LCD_KIT_OK;

	ctx->enabled = true;

	LCD_KIT_INFO("exit\n");
	return LCD_KIT_OK;
}

static void lcm_set_drm_mode(struct mtk_panel_info *pinfo, int index,
	struct lcd_kit_array_data *timing)
{
	mode_list[index].hdisplay = pinfo->xres;
	mode_list[index].hsync_start = pinfo->xres + timing->buf[FPS_HFP_INDEX];
	mode_list[index].hsync_end = pinfo->xres + timing->buf[FPS_HFP_INDEX] +
		timing->buf[FPS_HS_INDEX];
	mode_list[index].htotal = pinfo->xres + timing->buf[FPS_HFP_INDEX] +
		timing->buf[FPS_HS_INDEX] + timing->buf[FPS_HBP_INDEX];

	mode_list[index].vdisplay = pinfo->yres;
	mode_list[index].vsync_start = pinfo->yres + timing->buf[FPS_VFP_INDEX];
	mode_list[index].vsync_end = pinfo->yres + timing->buf[FPS_VFP_INDEX] +
		timing->buf[FPS_VS_INDEX];
	mode_list[index].vtotal = pinfo->yres + timing->buf[FPS_VFP_INDEX] +
		timing->buf[FPS_VS_INDEX] + timing->buf[FPS_VBP_INDEX];

	mode_list[index].vrefresh = timing->buf[FPS_VRE_INDEX];
	mode_list[index].clock = mode_list[index].htotal * mode_list[index].vtotal *
		mode_list[index].vrefresh / 1000;
}

static void lcm_init_drm_mode(void)
{
	struct mtk_panel_info *pinfo = &lcd_kit_info;
	int index;
	int i;
	struct lcd_kit_array_data *timing = NULL;

	if (disp_info->fps.support) {
		for (i = 0; i < disp_info->fps.panel_support_fps_list.cnt; i++) {
			index = disp_info->fps.panel_support_fps_list.buf[i];
			timing = &disp_info->fps.fps_dsi_timming[index];

			if (timing == NULL || timing->buf == NULL ||
				timing->cnt < FPS_DSI_TIMMING_PARA_NUM) {
				LCD_KIT_ERR("Timing %d is NULL\n", index);
				return;
			} else {
				lcm_set_drm_mode(pinfo, index, timing);
			}
		}
	} else {
		default_mode.hdisplay = pinfo->xres;
		default_mode.hsync_start = pinfo->xres + pinfo->ldi.h_front_porch;
		default_mode.hsync_end = pinfo->xres + pinfo->ldi.h_front_porch +
			pinfo->ldi.h_pulse_width;
		default_mode.htotal = pinfo->xres + pinfo->ldi.h_front_porch +
			pinfo->ldi.h_pulse_width + pinfo->ldi.h_back_porch;

		default_mode.vdisplay = pinfo->yres;
		default_mode.vsync_start = pinfo->yres + pinfo->ldi.v_front_porch;
		default_mode.vsync_end = pinfo->yres + pinfo->ldi.v_front_porch +
			pinfo->ldi.v_pulse_width;
		default_mode.vtotal = pinfo->yres + pinfo->ldi.v_front_porch +
			pinfo->ldi.v_pulse_width + pinfo->ldi.v_back_porch;

		default_mode.vrefresh = pinfo->vrefresh;
		default_mode.clock = default_mode.htotal * default_mode.vtotal *  default_mode.vrefresh / 1000;
	}
}

static void lcm_init_drm_mipi(struct mipi_dsi_device *dsi)
{
	struct mtk_panel_info *pinfo = &lcd_kit_info;

	if (dsi == NULL) {
		LCD_KIT_INFO("dsi is null\n");
		return;
	}
	dsi->lanes = pinfo->mipi.lane_nums;
	switch (pinfo->panel_ps) {
	case LCM_PIXEL_16BIT_RGB565:
		dsi->format = MIPI_DSI_FMT_RGB565;
		break;
	case LCM_LOOSELY_PIXEL_18BIT_RGB666:
		dsi->format = MIPI_DSI_FMT_RGB666;
		break;
	case LCM_PIXEL_24BIT_RGB888:
		dsi->format = MIPI_DSI_FMT_RGB888;
		break;
	case LCM_PACKED_PIXEL_18BIT_RGB666:
		dsi->format = MIPI_DSI_FMT_RGB666_PACKED;
		break;
	default:
		dsi->format = MIPI_DSI_FMT_RGB888;
		break;
	}
	switch (pinfo->panel_dsi_mode) {
	case DSI_CMD_MODE:
		break;
	case DSI_SYNC_PULSE_VDO_MODE:
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
		break;
	case DSI_SYNC_EVENT_VDO_MODE:
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
		break;
	case DSI_BURST_VDO_MODE:
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
		break;
	default:
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
		break;
	}
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	dsi->mode_flags |= MIPI_DSI_MODE_EOT_PACKET;
	if (pinfo->mipi.non_continue_en)
		dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;
}

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *support_mode = NULL;
	struct drm_display_mode *mode = NULL;
	struct mtk_panel_info *pinfo = &lcd_kit_info;
	int index;
	int i;

	if (disp_info->fps.support) {
		for (i = 0; i < disp_info->fps.panel_support_fps_list.cnt; i++) {
			index = disp_info->fps.panel_support_fps_list.buf[i];
			support_mode = drm_mode_duplicate(panel->drm, &mode_list[index]);
			if (!support_mode) {
				dev_err(panel->drm->dev, "failed to add mode %ux%ux:%u\n",
					mode_list[index].hdisplay, mode_list[index].vdisplay,
					mode_list[index].vrefresh);
				return -ENOMEM;
			}
			drm_mode_set_name(support_mode);
			/* first is default mode */
			if (i == 0)
				support_mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
			else
				support_mode->type = DRM_MODE_TYPE_DRIVER;

			drm_mode_probed_add(panel->connector, support_mode);
		}
	} else {
		mode = drm_mode_duplicate(panel->drm, &default_mode);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux:%u\n",
				default_mode.hdisplay, default_mode.vdisplay,
				default_mode.vrefresh);
			return -ENOMEM;
		}

		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(panel->connector, mode);
	}

	panel->connector->display_info.width_mm = pinfo->width;
	panel->connector->display_info.height_mm = pinfo->height;

	return 1;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	LCD_KIT_INFO("enter\n");
	return LCD_KIT_OK;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb_short[] = { 0x51, 0xFF };
	char bl_tb_long[] = { 0x51, 0xFF, 0xFF };

	if (!cb) {
		LCD_KIT_ERR("cb is null\n");
		return 0;
	}
	if (disp_info->alpm.support && level)
		lcd_kit_info.bl_current = level;
	if (disp_info->bl_is_shield_backlight) {
		LCD_KIT_ERR("bl_is_shield_backlight\n");
		return 0;
	}
	LCD_KIT_DEBUG("bl level:%u\n", level);
	lcd_kit_set_bl_cmd(level);
	if(common_info->backlight.bl_max <= 0xFF) {
		bl_tb_short[0] = common_info->backlight.bl_cmd.cmds[0].payload[0];
		bl_tb_short[1] = common_info->backlight.bl_cmd.cmds[0].payload[1];
		cb(dsi, handle, bl_tb_short, ARRAY_SIZE(bl_tb_short));
	} else {
		bl_tb_long[0] = common_info->backlight.bl_cmd.cmds[0].payload[0];
		bl_tb_long[1] = common_info->backlight.bl_cmd.cmds[0].payload[1];
		bl_tb_long[2] = common_info->backlight.bl_cmd.cmds[0].payload[2];
		cb(dsi, handle, bl_tb_long, ARRAY_SIZE(bl_tb_long));
	}
	lcd_kit_info.bl_current = level;
	if (disp_info->hbm_entry_delay > 0)
		disp_info->hbm_blcode_ts = ktime_get();
	return LCD_KIT_OK;
}

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m = NULL;
	unsigned int i = 0;

	if (panel == NULL) {
		LCD_KIT_ERR("panel is NULL\n");
		return NULL;
	}
	if (panel->connector == NULL) {
		LCD_KIT_ERR("connector is NULL\n");
		return NULL;
	}
	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = MTK_MODE_STATE_OK;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (ext == NULL){
		LCD_KIT_ERR("ext NULL\n");
		return MTK_MODE_STATE_FAIL;
	}
	if (m == NULL) {
		LCD_KIT_ERR("get mode is NULL\n");
		return MTK_MODE_STATE_FAIL;
	}
	LCD_KIT_INFO("set ext params mode = %u vrefresh = %d\n", mode, m->vrefresh);
	switch (m->vrefresh) {
	case LCD_KIT_FPS_60:
		ext->params = &ext_params_list[LCD_FPS_SCENCE_H60];
		disp_info->fps.current_fps = LCD_KIT_FPS_60;
		break;
	case LCD_KIT_FPS_90:
		ext->params = &ext_params_list[LCD_FPS_SCENCE_90];
		disp_info->fps.current_fps = LCD_KIT_FPS_90;
		break;
	case LCD_KIT_FPS_120:
		ext->params = &ext_params_list[LCD_FPS_SCENCE_120];
		disp_info->fps.current_fps = LCD_KIT_FPS_120;
		break;
	default:
		LCD_KIT_ERR("fps:%d is not support\n", m->vrefresh);
		ret = MTK_MODE_STATE_FAIL;
		break;
	}

	return ret;
}

static unsigned long get_real_te_interval(void)
{
	if (disp_info->fps.current_fps == g_fps_90hz) {
		LCD_KIT_INFO("current_fps: 90Hz\n");
		return (unsigned long)TE_90_TIME;
	}

	LCD_KIT_INFO("current_fps: 60Hz\n");
	return (unsigned long)TE_60_TIME;
}

static void lcd_kit_fphbm_entry_delay(void)
{
	ktime_t cur_timestamp;
	unsigned long diff;
	unsigned long te_time;
	unsigned long delay_time;
	unsigned long delay_threshold;
	unsigned long real_te_interval;

	if (!disp_info->hbm_entry_delay || !disp_info->te_interval_us)
		return;
	real_te_interval = get_real_te_interval();
	delay_threshold = real_te_interval * disp_info->hbm_entry_delay /
		TE_60_TIME;
	LCD_KIT_INFO("delay_threshold = %lu us", delay_threshold);
	if (delay_threshold <= 0)
		return;

	cur_timestamp = ktime_get();
	diff = ktime_to_us(cur_timestamp) -
		ktime_to_us(disp_info->hbm_blcode_ts);
	if (diff >= delay_threshold)
		return;

	te_time = real_te_interval * disp_info->te_interval_us /
		TE_60_TIME;
	delay_time = (delay_threshold - diff) / te_time * te_time + te_time;
	LCD_KIT_INFO("diff = %lu us, TE time = %lu us, delay_time = %lu us",
		diff, te_time, delay_time);
	udelay(delay_time);
}

static void disable_elvss_dim_write(void)
{
	common_info->hbm.elvss_write_cmds.cmds[0].wait =
		common_info->hbm.hbm_fp_elvss_cmd_delay;
	common_info->hbm.elvss_write_cmds.cmds[0].payload[1] =
		common_info->hbm.ori_elvss_val & LCD_KIT_DISABLE_ELVSSDIM_MASK;
}

static void enable_elvss_dim_write(void)
{
	common_info->hbm.elvss_write_cmds.cmds[0].wait = LCD_KIT_ELVSSDIM_NO_WAIT;
	common_info->hbm.elvss_write_cmds.cmds[0].payload[1] =
		common_info->hbm.ori_elvss_val | LCD_KIT_ENABLE_ELVSSDIM_MASK;
}

static void hbm_cmd_tx(struct hbm_type_cfg hbm_source,
	struct lcd_kit_dsi_panel_cmds *pcmds)
{
	if (hbm_source.source == HBM_FOR_FP)
		(void)cb_tx(hbm_source.dsi, hbm_source.cb,
			hbm_source.handle, pcmds);
	else
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(pcmds);
}

static void lcd_kit_set_elvss_dim_fp(int disable_elvss_dim,
	struct hbm_type_cfg hbm_source)
{
	if (!common_info->hbm.hbm_fp_elvss_support) {
		LCD_KIT_INFO("Do not support ELVSS Dim, just return\n");
		return;
	}

	if (common_info->hbm.elvss_prepare_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.elvss_prepare_cmds);
	if (common_info->hbm.elvss_write_cmds.cmds) {
		if (disable_elvss_dim)
			disable_elvss_dim_write();
		else
			enable_elvss_dim_write();
		hbm_cmd_tx(hbm_source, &common_info->hbm.elvss_write_cmds);
		LCD_KIT_INFO("[fp set elvss dim] send:0x%x\n",
			common_info->hbm.elvss_write_cmds.cmds[0].payload[1]);
	}
	if (common_info->hbm.elvss_post_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.elvss_post_cmds);
}

static void lcd_kit_fill_hbm_cmd(unsigned int level)
{
	if (common_info->hbm.hbm_special_bit_ctrl ==
		LCD_KIT_HIGH_12BIT_CTL_HBM_SUPPORT) {
		/* Set high 12bit hbm level, low 4bit set 0 */
		common_info->hbm.hbm_cmds.cmds[0].payload[1] =
			(level >> LCD_KIT_SHIFT_FOUR_BIT) & 0xff;
		common_info->hbm.hbm_cmds.cmds[0].payload[2] =
			(level << LCD_KIT_SHIFT_FOUR_BIT) & 0xf0;
	} else if (common_info->hbm.hbm_special_bit_ctrl ==
		LCD_KIT_8BIT_CTL_HBM_SUPPORT) {
		common_info->hbm.hbm_cmds.cmds[0].payload[1] = level & 0xff;
	} else {
		/* change bl level to dsi cmds */
		common_info->hbm.hbm_cmds.cmds[0].payload[1] =
			(level >> 8) & 0xf;
		common_info->hbm.hbm_cmds.cmds[0].payload[2] = level & 0xff;
	}
}

static void lcd_kit_set_hbm_level_fp(unsigned int level,
	struct hbm_type_cfg hbm_source)
{
	/* prepare */
	if (common_info->hbm.hbm_prepare_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.hbm_prepare_cmds);
	/* set hbm level */
	if (common_info->hbm.hbm_cmds.cmds) {
		lcd_kit_fill_hbm_cmd(level);
		hbm_cmd_tx(hbm_source, &common_info->hbm.hbm_cmds);
	}
	/* post */
	if (common_info->hbm.hbm_post_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.hbm_post_cmds);
}

static void lcd_kit_disable_hbm_fp(struct hbm_type_cfg hbm_source)
{
	if (common_info->hbm.exit_dim_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.exit_dim_cmds);
	if (common_info->hbm.exit_cmds.cmds)
		hbm_cmd_tx(hbm_source, &common_info->hbm.exit_cmds);
}

static void lcd_kit_hbm_set_func_by_level(uint32_t level, int type,
	struct hbm_type_cfg hbm_source)
{
	if (type == LCD_KIT_FP_HBM_ENTER) {
		lcd_kit_set_elvss_dim_fp(LCD_KIT_DISABLE_ELVSSDIM, hbm_source);
		if (common_info->hbm.fp_enter_cmds.cmds)
			hbm_cmd_tx(hbm_source, &common_info->hbm.fp_enter_cmds);
		lcd_kit_set_hbm_level_fp(level, hbm_source);
	} else if (type == LCD_KIT_FP_HBM_EXIT) {
		if (level > 0)
			lcd_kit_set_hbm_level_fp(level, hbm_source);
		else
			lcd_kit_disable_hbm_fp(hbm_source);

		lcd_kit_set_elvss_dim_fp(LCD_KIT_ENABLE_ELVSSDIM, hbm_source);
	} else {
		LCD_KIT_ERR("unknown case!\n");
	}
}

static void lcd_kit_fp_hbm_extern(int type, struct hbm_type_cfg hbm_source)
{
	if (type == LCD_KIT_FP_HBM_ENTER) {
		if (common_info->hbm.fp_enter_extern_cmds.cmds)
			hbm_cmd_tx(hbm_source,
				&common_info->hbm.fp_enter_extern_cmds);
	} else if (type == LCD_KIT_FP_HBM_EXIT) {
		if (common_info->hbm.fp_exit_extern_cmds.cmds)
			hbm_cmd_tx(hbm_source,
				&common_info->hbm.fp_exit_extern_cmds);
	} else {
		LCD_KIT_ERR("unknown case!\n");
	}
}

static void lcd_kit_restore_hbm_level(struct hbm_type_cfg hbm_source)
{
	mutex_lock(&common_info->hbm.hbm_lock);
	lcd_kit_hbm_set_func_by_level(
		common_info->hbm.hbm_level_current,
		LCD_KIT_FP_HBM_EXIT, hbm_source);
	mutex_unlock(&common_info->hbm.hbm_lock);
}

static void lcd_kit_hbm_enter(int level, struct hbm_type_cfg hbm_source)
{
	int hbm_level;
	struct mtk_panel_info *pinfo = &lcd_kit_info;

	if (!common_info->hbm.hbm_fp_support && hbm_source.source == HBM_FOR_FP)
		hbm_level = pinfo->bl_max;
	else
		hbm_level = level;

	if (common_info->hbm.hbm_fp_support && hbm_source.source == HBM_FOR_MMI)
		lcd_kit_mipi_set_backlight(hbm_source, pinfo->bl_max);

	if (common_info->hbm.hbm_fp_support) {
		mutex_lock(&common_info->hbm.hbm_lock);
		if (hbm_source.source == HBM_FOR_FP)
			lcd_kit_fphbm_entry_delay();
		common_info->hbm.hbm_if_fp_is_using = 1;
		lcd_kit_hbm_set_func_by_level(hbm_level,
			LCD_KIT_FP_HBM_ENTER, hbm_source);
		mutex_unlock(&common_info->hbm.hbm_lock);
	} else {
		lcd_kit_mipi_set_backlight(hbm_source, hbm_level);
		lcd_kit_fp_hbm_extern(LCD_KIT_FP_HBM_ENTER, hbm_source);
	}
}

static void lcd_kit_hbm_exit(int level, struct hbm_type_cfg hbm_source)
{
	if (common_info->hbm.hbm_fp_support) {
		lcd_kit_restore_hbm_level(hbm_source);
		mutex_lock(&common_info->hbm.hbm_lock);
		common_info->hbm.hbm_if_fp_is_using = 0;
		mutex_unlock(&common_info->hbm.hbm_lock);
	}
	lcd_kit_mipi_set_backlight(hbm_source, level);
	lcd_kit_fp_hbm_extern(LCD_KIT_FP_HBM_EXIT, hbm_source);
}

static int lcd_kit_set_backlight_by_type(int backlight_type,
	struct hbm_type_cfg hbm_source)
{
	struct mtk_panel_info *pinfo = &lcd_kit_info;

	LCD_KIT_INFO("backlight_type is %d\n", backlight_type);
	if (backlight_type == BACKLIGHT_HIGH_LEVEL) {
		lcd_kit_hbm_enter(common_info->hbm.hbm_level_max, hbm_source);
		disp_info->bl_is_shield_backlight = true;
		lcd_kit_enable_hbm_timer();
		LCD_KIT_INFO("set_bl is max_%d : pre_%d\n",
			pinfo->bl_max, lcd_kit_info.bl_current);
	} else if (backlight_type == BACKLIGHT_LOW_LEVEL) {
		lcd_kit_disable_hbm_timer();
		disp_info->bl_is_shield_backlight = false;
		lcd_kit_hbm_exit(lcd_kit_info.bl_current, hbm_source);
		LCD_KIT_INFO("set_bl is pre_%d : cur_%d\n",
			pinfo->bl_min, lcd_kit_info.bl_current);
	} else {
		LCD_KIT_ERR("bl_type is not define\n");
		return LCD_KIT_FAIL;
	}
	return LCD_KIT_OK;
}

static int lcd_kit_set_hbm_for_mmi(int level, struct hbm_type_cfg hbm_source)
{
	struct mtk_panel_info *pinfo = &lcd_kit_info;

	LCD_KIT_INFO("level = %d\n", level);
	if (level == 0) {
		disp_info->bl_is_shield_backlight = false;
		lcd_kit_hbm_exit(lcd_kit_info.bl_current, hbm_source);
		return LCD_KIT_OK;
	}
	lcd_kit_hbm_enter(level, hbm_source);
	disp_info->bl_is_shield_backlight = true;
	return LCD_KIT_OK;
}

static void lcd_kit_hbm_disable(void)
{
	if (common_info->hbm.exit_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.exit_cmds);
}

static void lcd_kit_hbm_dim_enable(void)
{
	if (common_info->hbm.enter_dim_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.enter_dim_cmds);
}

static void lcd_kit_hbm_dim_disable(void)
{
	if (common_info->hbm.exit_dim_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.exit_dim_cmds);
}

static void lcd_kit_hbm_set_level(int level)
{
	/* prepare */
	if (common_info->hbm.hbm_prepare_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(
			&common_info->hbm.hbm_prepare_cmds);
	/* set hbm level */
	if (common_info->hbm.hbm_cmds.cmds != NULL) {
		if (common_info->hbm.hbm_special_bit_ctrl ==
			LCD_KIT_HIGH_12BIT_CTL_HBM_SUPPORT) {
			/* Set high 12bit hbm level, low 4bit set zero */
			common_info->hbm.hbm_cmds.cmds[0].payload[1] =
				(level >> LCD_KIT_SHIFT_FOUR_BIT) & 0xff;
			common_info->hbm.hbm_cmds.cmds[0].payload[2] =
				(level << LCD_KIT_SHIFT_FOUR_BIT) & 0xf0;
		} else if (common_info->hbm.hbm_special_bit_ctrl ==
					LCD_KIT_8BIT_CTL_HBM_SUPPORT) {
			common_info->hbm.hbm_cmds.cmds[0].payload[1] = level & 0xff;
		} else {
			/* change bl level to dsi cmds */
			common_info->hbm.hbm_cmds.cmds[0].payload[1] =
				(level >> 8) & 0xf;
			common_info->hbm.hbm_cmds.cmds[0].payload[2] =
				level & 0xff;
		}
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.hbm_cmds);
	}
	/* post */
	if (common_info->hbm.hbm_post_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.hbm_post_cmds);
}

static void lcd_kit_hbm_print_count(int last_hbm_level, int hbm_level)
{
	static int count;
	int level_delta = 60;

	if (abs(hbm_level - last_hbm_level) > level_delta) {
		if (count == 0)
			LCD_KIT_INFO("last hbm_level=%d!\n", last_hbm_level);
		count = 5;
	}
	if (count > 0) {
		count--;
		LCD_KIT_INFO("hbm_level=%d!\n", hbm_level);
	} else {
		LCD_KIT_DEBUG("hbm_level=%d!\n", hbm_level);
	}
}

static void lcd_kit_hbm_enable_no_dimming(void)
{
	if (common_info->hbm.enter_no_dim_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(
			&common_info->hbm.enter_no_dim_cmds);
}

static void lcd_kit_hbm_enable(void)
{
	if (common_info->hbm.enter_cmds.cmds != NULL)
		(void)lcd_kit_dsi_cmds_extern_tx_nolock(&common_info->hbm.enter_cmds);
}

static int check_if_fp_using_hbm(void)
{
	int ret = LCD_KIT_OK;

	if (common_info->hbm.hbm_fp_support) {
		if (common_info->hbm.hbm_if_fp_is_using)
			ret = LCD_KIT_FAIL;
	}

	return ret;
}

static int lcd_kit_hbm_set_handle(int last_hbm_level,
	int hbm_dimming, int hbm_level)
{
	int ret = LCD_KIT_OK;

	if ((hbm_level < 0) || (hbm_level > HBM_SET_MAX_LEVEL)) {
		LCD_KIT_ERR("input param invalid, hbm_level %d!\n", hbm_level);
		return LCD_KIT_FAIL;
	}
	if (!common_info->hbm.support) {
		LCD_KIT_DEBUG("not support hbm\n");
		return ret;
	}
	mutex_lock(&common_info->hbm.hbm_lock);
	common_info->hbm.hbm_level_current = hbm_level;
	if (check_if_fp_using_hbm() < 0) {
		LCD_KIT_INFO("fp is using, exit!\n");
		mutex_unlock(&common_info->hbm.hbm_lock);
		return ret;
	}
	if (hbm_level > 0) {
		if (last_hbm_level == 0) {
			/* enable hbm */
			lcd_kit_hbm_enable();
			if (!hbm_dimming)
				lcd_kit_hbm_enable_no_dimming();
		} else {
			lcd_kit_hbm_print_count(last_hbm_level, hbm_level);
		}
		 /* set hbm level */
		lcd_kit_hbm_set_level(hbm_level);
	} else {
		if (last_hbm_level == 0) {
			/* disable dimming */
			lcd_kit_hbm_dim_disable();
		} else {
			/* exit hbm */
			if (hbm_dimming)
				lcd_kit_hbm_dim_enable();
			else
				lcd_kit_hbm_dim_disable();
			lcd_kit_hbm_disable();
		}
	}
	mutex_unlock(&common_info->hbm.hbm_lock);
	return ret;
}

static int lcd_kit_hbm_set_func(int level, bool dimming)
{
	static int last_hbm_level;
	int ret;

	ret = lcd_kit_hbm_set_handle(last_hbm_level,
		dimming, level);
	last_hbm_level = level;
	return ret;
}


static void handle_ccorr_for_hbm(struct drm_device *dev, int level,
	struct drm_file *file_priv)
{
	if (level == 0)
	{
		mtk_drm_ccorr_restore(dev, NULL, file_priv);
	} else {
		mtk_drm_ccorr_clear(dev, NULL, file_priv);
	}
}

int panel_drm_hbm_set(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = LCD_KIT_FAIL;
	struct display_engine_ddic_hbm_param *hbm_cfg =
		(struct display_engine_ddic_hbm_param *)data;
	struct hbm_type_cfg hbm_source = {
		.source = HBM_FOR_MMI,
	};

	if (!data) {
		LCD_KIT_ERR("data is null\n");
		return LCD_KIT_FAIL;
	}

	if (disp_info->alpm_state != ALPM_OUT) {
		LCD_KIT_ERR("in alpm state, return!\n");
		return LCD_KIT_FAIL;
	}

	LCD_KIT_INFO("hbm tpye:%d, level:%d, dimming:%d\n", hbm_cfg->type,
		hbm_cfg->level, hbm_cfg->dimming);
	if (hbm_cfg->type == HBM_FOR_MMI)
		handle_ccorr_for_hbm(dev, hbm_cfg->level, file_priv);
	mtk_get_crtc_lock_ex();
	if (hbm_cfg->type == HBM_FOR_MMI)
		ret = lcd_kit_set_hbm_for_mmi(hbm_cfg->level, hbm_source);

	if (hbm_cfg->type == HBM_FOR_LIGHT)
		ret = lcd_kit_hbm_set_func(hbm_cfg->level, hbm_cfg->dimming);
	mtk_get_crtc_unlock_ex();

	return ret;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
	dcs_write_gce cb, void *handle, bool en)
{
	int ret;
	struct lcm *ctx = panel_to_lcm(panel);
	struct hbm_type_cfg hbm_source = {
		HBM_FOR_FP, dsi, (void *)cb, handle };

	LCD_KIT_INFO(" in\n");
	if (disp_info->alpm_state != ALPM_OUT) {
		LCD_KIT_ERR("in alpm state, return!\n");
		return LCD_KIT_FAIL;
	}

	if (en)
		ret = lcd_kit_set_backlight_by_type(BACKLIGHT_HIGH_LEVEL,
			hbm_source);
	else
		ret = lcd_kit_set_backlight_by_type(BACKLIGHT_LOW_LEVEL,
			hbm_source);
	ctx->hbm_en = en;
	ctx->hbm_wait = true;
	return ret;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->hbm_en;
}

static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;

	ctx->hbm_wait = wait;
	return old;
}

static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	LCD_KIT_INFO(" in\n");
	disp_info->alpm_state = ALPM_START;
	disp_info->bl_is_shield_backlight = true;
	if (disp_info->alpm.need_reset) {
		/* reset pull low */
		lcd_kit_reset_power_ctrl(0);
		msleep(1);
		/* reset pull high */
		lcd_kit_reset_power_ctrl(1);
		msleep(6);
	}
	(void)cb_tx(dsi_drv, (void *)cb, handle, &disp_info->alpm.off_cmds);
	return LCD_KIT_OK;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	LCD_KIT_INFO(" in\n");
	return LCD_KIT_OK;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	LCD_KIT_INFO(" in\n");
	if (disp_info->alpm_state == ALPM_OUT) {
		LCD_KIT_ERR("not in alpm state, return!\n");
		return LCD_KIT_FAIL;
	}
	if (disp_info->alpm_state == ALPM_START)
		(void)cb_tx(dsi_drv, (void *)cb, handle,
			&disp_info->alpm.high_light_cmds);
	(void)cb_tx(dsi_drv, (void *)cb, handle, &disp_info->alpm.exit_cmds);
	disp_info->bl_is_shield_backlight = false;
	disp_info->alpm_state = ALPM_OUT;
	return LCD_KIT_OK;
}

static int panel_doze_post_disp_on(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	LCD_KIT_INFO(" in\n");
	if (disp_info->alpm_state == ALPM_START)
		disp_info->alpm_start_time = ktime_get();
	return LCD_KIT_OK;
}

static int panel_doze_area(struct drm_panel *panel,
	void *dsi_drv, dcs_write_gce cb, void *handle)
{
	LCD_KIT_INFO(" in\n");
	return LCD_KIT_OK;
}

static unsigned long panel_doze_get_mode_flags(
	struct drm_panel *panel, int aod_en)
{
	unsigned long mode_flags = 0;

	LCD_KIT_INFO(" in\n");
	if (is_mipi_cmd_panel()) {
		g_ext_funcs.doze_get_mode_flags = NULL;
		return mode_flags;
	}

	if (aod_en)
		mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;
	else
		mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;
	return mode_flags;
}

static struct mtk_panel_funcs g_ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	.doze_enable_start = panel_doze_enable_start,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
//	.doze_post_disp_on = panel_doze_post_disp_on,
	.doze_area = panel_doze_area,
	.doze_get_mode_flags = panel_doze_get_mode_flags,
};

static void lcm_set_esd_config(struct mtk_panel_params *panel_params)
{
	int i;
	int count = 0;
	struct lcd_kit_dsi_cmd_desc *esd_cmds = NULL;

	panel_params->cust_esd_check = common_info->esd.support;
	panel_params->esd_check_enable = common_info->esd.support;

	if (common_info->esd.cmds.cmds != NULL) {
		LCD_KIT_INFO("set esd config\n");
		esd_cmds = common_info->esd.cmds.cmds;
		for (i = 0; i < common_info->esd.cmds.cmd_cnt; i++) {
			panel_params->lcm_esd_check_table[i].cmd = esd_cmds->payload[0];
			panel_params->lcm_esd_check_table[i].count = esd_cmds->dlen;
			LCD_KIT_INFO("dsi.lcm_esd_check_table[%d].cmd = 0x%x\n",
				i, panel_params->lcm_esd_check_table[i].cmd);
			LCD_KIT_INFO("dsi.lcm_esd_check_table[%d].count = %d\n",
				i, panel_params->lcm_esd_check_table[i].count);
			panel_params->lcm_esd_check_table[i].mask_list[0] =
				(common_info->esd.value.buf[i] >> 8) & 0xFF;
			panel_params->lcm_esd_check_table[i].para_list[0] =
				common_info->esd.value.buf[i] & 0xFF;
			LCD_KIT_INFO("lcm_esd_check_table[%d].para_list[0] = %d\n",
				i, panel_params->lcm_esd_check_table[i].para_list[0]);
			LCD_KIT_INFO("lcm_esd_check_table[%d].mask_list[0] = %d\n",
				i, panel_params->lcm_esd_check_table[i].mask_list[0]);
			esd_cmds++;
			count++;
			if (count == ESD_CHECK_NUM)
				break;
		}
	} else {
		LCD_KIT_INFO("esd not config, use default\n");
		panel_params->lcm_esd_check_table[0].cmd = 0x0A;
		panel_params->lcm_esd_check_table[0].count = 1;
		panel_params->lcm_esd_check_table[0].para_list[0] = 0x9C;
		panel_params->lcm_esd_check_table[0].mask_list[0] = 0;
	}
}

static void lcm_mipi_hopping_config(struct mtk_panel_params *pext_params,
	struct mipi_hopping_info *pmipi_hopping)
{
	if (pext_params == NULL ||
		pmipi_hopping == NULL) {
		LCD_KIT_ERR("input param is NULL\n");
		return;
	}
	if (pmipi_hopping->switch_en == 0) {
		pext_params->dyn.switch_en = pmipi_hopping->switch_en;
		return;
	}
	pext_params->dyn.switch_en = pmipi_hopping->switch_en;
	pext_params->dyn.data_rate = pmipi_hopping->data_rate;
	pext_params->dyn.hbp = pmipi_hopping->h_back_porch;
	pext_params->dyn.hfp = pmipi_hopping->h_front_porch;
	pext_params->dyn.hsa = pmipi_hopping->h_pulse_width;
	pext_params->dyn.vbp = pmipi_hopping->v_back_porch;
	pext_params->dyn.vfp = pmipi_hopping->v_front_porch;
	pext_params->dyn.vsa = pmipi_hopping->v_pulse_width;
	pext_params->dyn.vfp_lp_dyn = pmipi_hopping->vfp_lp_dyn;
}

static void lcm_set_dsc_params(struct mtk_panel_params *panel_params)
{
	if (panel_params == NULL) {
		LCD_KIT_ERR("panel_params is NULL\n");
		return;
	}
	panel_params->output_mode = MTK_PANEL_DSC_SINGLE_PORT;
	panel_params->dsc_params.enable = disp_info->dsc_enable;
	panel_params->dsc_params.ver = disp_info->dsc_params.ver;
	panel_params->dsc_params.slice_mode = disp_info->dsc_params.slice_mode;
	panel_params->dsc_params.rgb_swap = disp_info->dsc_params.rgb_swap;
	panel_params->dsc_params.dsc_cfg = disp_info->dsc_params.dsc_cfg;
	panel_params->dsc_params.rct_on = disp_info->dsc_params.rct_on;
	panel_params->dsc_params.bit_per_channel = disp_info->dsc_params.bit_per_channel;
	panel_params->dsc_params.dsc_line_buf_depth = disp_info->dsc_params.dsc_line_buf_depth;
	panel_params->dsc_params.bp_enable = disp_info->dsc_params.bp_enable;
	panel_params->dsc_params.bit_per_pixel = disp_info->dsc_params.bit_per_pixel;
	panel_params->dsc_params.pic_height = disp_info->dsc_params.pic_height;
	panel_params->dsc_params.pic_width = disp_info->dsc_params.pic_width;
	panel_params->dsc_params.slice_height = disp_info->dsc_params.slice_height;
	panel_params->dsc_params.slice_width = disp_info->dsc_params.slice_width;
	panel_params->dsc_params.chunk_size = disp_info->dsc_params.chunk_size;
	panel_params->dsc_params.xmit_delay = disp_info->dsc_params.xmit_delay;
	panel_params->dsc_params.dec_delay = disp_info->dsc_params.dec_delay;
	panel_params->dsc_params.scale_value = disp_info->dsc_params.scale_value;
	panel_params->dsc_params.increment_interval = disp_info->dsc_params.increment_interval;
	panel_params->dsc_params.decrement_interval = disp_info->dsc_params.decrement_interval;
	panel_params->dsc_params.line_bpg_offset = disp_info->dsc_params.line_bpg_offset;
	panel_params->dsc_params.nfl_bpg_offset = disp_info->dsc_params.nfl_bpg_offset;
	panel_params->dsc_params.slice_bpg_offset = disp_info->dsc_params.slice_bpg_offset;
	panel_params->dsc_params.initial_offset = disp_info->dsc_params.initial_offset;
	panel_params->dsc_params.final_offset = disp_info->dsc_params.final_offset;
	panel_params->dsc_params.flatness_minqp = disp_info->dsc_params.flatness_minqp;
	panel_params->dsc_params.flatness_maxqp = disp_info->dsc_params.flatness_maxqp;
	panel_params->dsc_params.rc_model_size = disp_info->dsc_params.rc_model_size;
	panel_params->dsc_params.rc_edge_factor = disp_info->dsc_params.rc_edge_factor;
	panel_params->dsc_params.rc_quant_incr_limit0 = disp_info->dsc_params.rc_quant_incr_limit0;
	panel_params->dsc_params.rc_quant_incr_limit1 = disp_info->dsc_params.rc_quant_incr_limit1;
	panel_params->dsc_params.rc_tgt_offset_hi = disp_info->dsc_params.rc_tgt_offset_hi;
	panel_params->dsc_params.rc_tgt_offset_lo = disp_info->dsc_params.rc_tgt_offset_lo;
}

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
static void lcm_set_round_corner_params(struct mtk_panel_params *panel_params,
	struct mtk_panel_info *pinfo)
{
	if (panel_params == NULL) {
		LCD_KIT_ERR("panel_params is NULL\n");
		return;
	}
	if (pinfo == NULL) {
		LCD_KIT_ERR("pinfo is NULL\n");
		return;
	}
	if (pinfo->round_corner.round_corner_en) {
		panel_params->round_corner_en = pinfo->round_corner.round_corner_en;
		panel_params->corner_pattern_height = pinfo->round_corner.h;
		panel_params->corner_pattern_height_bot = pinfo->round_corner.h_bot;
		panel_params->corner_pattern_tp_size = pinfo->round_corner.tp_size;
		panel_params->corner_pattern_lt_addr = pinfo->round_corner.lt_addr;
		LCD_KIT_INFO("corner_pattern_tp_size is %u\n", panel_params->corner_pattern_tp_size);
	}
}
#endif

static void lcm_init_ext_params(void)
{
	struct mtk_panel_info *pinfo = &lcd_kit_info;
	int index;
	int i;
	struct lcd_kit_array_data *timing = NULL;
	struct lcd_kit_array_data *hop_info = NULL;

	/* set fps */
	if (disp_info->fps.support) {
		for (i = 0; i < disp_info->fps.panel_support_fps_list.cnt; i++) {
			index = disp_info->fps.panel_support_fps_list.buf[i];
			timing = &disp_info->fps.fps_dsi_timming[index];

			if (timing == NULL || timing->buf == NULL || timing->cnt < FPS_DSI_TIMMING_PARA_NUM) {
				LCD_KIT_ERR("Timing %d is NULL\n", index);
				return;
			} else {
				ext_params_list[index].data_rate = timing->buf[FPS_RATE_INDEX];
				ext_params_list[index].pll_clk = timing->buf[FPS_RATE_INDEX] / 2;
				ext_params_list[index].vfp_low_power = timing->buf[FPS_LOWER_INDEX];
				/* 0: dphy 1: cphy */
				ext_params_list[index].is_cphy = pinfo->mipi.phy_mode;
				/* change mm to um */
				ext_params_list[index].physical_width_um = pinfo->width * 1000;
				/* change mm to um */
				ext_params_list[index].physical_height_um = pinfo->height * 1000;

				/* set dsc */
				if (disp_info->dsc_enable) {
					LCD_KIT_INFO("set dsc params\n");
					lcm_set_dsc_params(&ext_params_list[index]);
				}

				/* set esd */
				lcm_set_esd_config(&ext_params_list[index]);

				/* set hopping */
				ext_params_list[index].dyn.switch_en = disp_info->fps.hop_support;
				if (disp_info->fps.hop_support) {
					hop_info = &disp_info->fps.hop_info[index];
					/* 0-7 is HFP-HBP-HS-VFP-VBP-VS-RATE-LOWER */
					ext_params_list[index].dyn.hfp = hop_info->buf[0];
					ext_params_list[index].dyn.hbp = hop_info->buf[1];
					ext_params_list[index].dyn.hsa = hop_info->buf[2];
					ext_params_list[index].dyn.vfp = hop_info->buf[3];
					ext_params_list[index].dyn.vbp = hop_info->buf[4];
					ext_params_list[index].dyn.vsa = hop_info->buf[5];
					ext_params_list[index].dyn.data_rate = hop_info->buf[6];
					ext_params_list[index].dyn.vfp_lp_dyn = hop_info->buf[7];
					LCD_KIT_INFO("%d %d %d %d %d %d", ext_params_list[index].dyn.hfp,
						ext_params_list[index].dyn.hbp, ext_params_list[index].dyn.hsa,
						ext_params_list[index].dyn.vfp, ext_params_list[index].dyn.vbp,
						ext_params_list[index].dyn.vsa);
				}
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
				/* set hw round corner param*/
				lcm_set_round_corner_params(&ext_params_list[index], pinfo);
#endif
			}
		}
	} else {
		ext_params.data_rate = pinfo->data_rate;
		ext_params.pll_clk = pinfo->data_rate / 2;
		ext_params.vfp_low_power = pinfo->ldi.v_front_porch_forlp;
		lcm_set_esd_config(&ext_params);
		lcm_mipi_hopping_config(&ext_params, &(pinfo->mipi_hopping));
		/* set dsc */
		if (disp_info->dsc_enable) {
			LCD_KIT_INFO("set dsc params\n");
			lcm_set_dsc_params(&ext_params);
		}
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
		/* set hw round corner param*/
		lcm_set_round_corner_params(&ext_params, pinfo);
#endif
	}
	ext_params.hbm_en_time = disp_info->hbm_en_times;
	ext_params.hbm_dis_time = disp_info->hbm_dis_times;
	ext_params.doze_delay = disp_info->alpm.doze_delay;
}
#endif

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

int lcd_kit_init(void)
{
	int ret = LCD_KIT_OK;
	struct device_node *np = NULL;

	LCD_KIT_INFO("enter\n");
	if (!lcd_kit_support()) {
		LCD_KIT_INFO("not lcd_kit driver and return\n");
		return ret;
	}

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_LCD_KIT_PANEL_TYPE);
	if (np == NULL) {
		LCD_KIT_ERR("not find device node %s\n", DTS_COMP_LCD_KIT_PANEL_TYPE);
		ret = -1;
		return ret;
	}

	OF_PROPERTY_READ_U32_RETURN(np, "product_id", &disp_info->product_id);
	LCD_KIT_INFO("product_id = %d\n", disp_info->product_id);
	disp_info->compatible = (char *)of_get_property(np, "lcd_panel_type", NULL);
	if (!disp_info->compatible) {
		LCD_KIT_ERR("can not get lcd kit compatible\n");
		return ret;
	}
	LCD_KIT_INFO("compatible: %s\n", disp_info->compatible);

	np = of_find_compatible_node(NULL, NULL, disp_info->compatible);
	if (!np) {
		LCD_KIT_ERR("NOT FOUND device node %s!\n", disp_info->compatible);
		ret = -1;
		return ret;
	}

#if defined CONFIG_HUAWEI_DSM
	lcd_dclient = dsm_register_client(&dsm_lcd);
#endif
	mutex_init(&disp_info->drm_hw_lock);
	/* adapt init */
	lcd_kit_adapt_init();
	bias_bl_ops_init();
	/* common init */
	if (common_ops->common_init)
		common_ops->common_init(np);
	/* utils init */
	lcd_kit_utils_init(np, &lcd_kit_info);
	/* init fnode */
	lcd_kit_sysfs_init();
	/* power init */
	lcd_kit_power_init();
	/* init panel ops */
	lcd_kit_panel_init();
	/* get lcd max brightness */
	lcd_kit_get_bl_max_nit_from_dts();

	lcm_set_panel_state(LCD_POWER_STATE_ON);
	lcd_kit_set_thp_proximity_state(POWER_ON);

	LCD_KIT_INFO("exit\n");
	return ret;
}

static void lcd_kit_keep_ldo_on(struct device *dev)
{
	struct regulator *ldo_reg = NULL;

	if (!dev) {
		LCD_KIT_ERR("dev is NULL\n");
		return;
	}

	if (of_property_read_bool(dev->of_node, "lcd_vci-supply")) {
		ldo_reg = devm_regulator_get(dev, "lcd_vci");
		if (ldo_reg)
			regulator_enable(ldo_reg);
	}
	if (of_property_read_bool(dev->of_node, "lcd_iovcc-supply")) {
		ldo_reg = devm_regulator_get(dev, "lcd_iovcc");
		if (ldo_reg)
			regulator_enable(ldo_reg);
	}
	if (of_property_read_bool(dev->of_node, "lcd_vdd-supply")) {
		ldo_reg = devm_regulator_get(dev, "lcd_vdd");
		if (ldo_reg)
			regulator_enable(ldo_reg);
	}
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx = NULL;
	int ret;
	int index;

	LCD_KIT_INFO("enter\n");
	g_dsi = dsi;
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	lcd_kit_keep_ldo_on(dev);
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	lcm_init_drm_mode();
	lcm_init_drm_mipi(dsi);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_en = false;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	lcm_init_ext_params();
	mtk_panel_tch_handle_reg(&ctx->panel);

	/* fps support */
	if (disp_info->fps.support) {
		/* set default first */
		if (disp_info->fps.panel_support_fps_list.cnt > 0) {
			index = disp_info->fps.panel_support_fps_list.buf[0];
			ret = mtk_panel_ext_create(dev, &ext_params_list[index],
				&g_ext_funcs, &ctx->panel);
			/* set default fps */
			disp_info->fps.default_fps = mode_list[index].vrefresh;
			/* set current fps */
			disp_info->fps.current_fps = mode_list[index].vrefresh;
			if (ret < 0)
				return ret;
		} else {
			LCD_KIT_ERR("Timing is NULL\n");
		}
	} else {
		ret = mtk_panel_ext_create(dev, &ext_params,
			&g_ext_funcs, &ctx->panel);
		if (ret < 0)
			return ret;
	}
#endif

	LCD_KIT_INFO("exit\n");
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return LCD_KIT_OK;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "kit_panel_common", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "kit_panel_common",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Panel driver for mtk drm arch");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
