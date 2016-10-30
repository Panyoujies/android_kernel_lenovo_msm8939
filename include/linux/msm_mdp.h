/* include/linux/msm_mdp.h
 *
 * Copyright (C) 2007 Google Incorporated
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MSM_MDP_H_
#define _MSM_MDP_H_

#include <uapi/linux/msm_mdp.h>

/*lenovo.sw2 houdz1 add for lcd effect begin*/
#ifdef CONFIG_FB_LENOVO_LCD_EFFECT
#define MSMFB_PANEL_EFFECT	 _IOW(MSMFB_IOCTL_MAGIC, 180, struct hal_panel_ctrl_data)
#define EFFECT_COUNT 16
#define MODE_COUNT  8
#define NAME_SIZE 16

typedef enum {
	GET_EFFECT_NUM = 1,
	GET_EFFECT_LEVEL,
	GET_EFFECT,
	GET_MODE_NUM,
	GET_MODE,
	SET_EFFECT,
	SET_MODE,
	SET_BL_LEVEL,
	GET_BL_LEVEL,
} ctrl_id;

struct hal_lcd_effect {
	char name[NAME_SIZE];
	int max_level;
	int level;
};

struct hal_lcd_mode {
	char name[NAME_SIZE];
};

struct hal_panel_data {
	struct hal_lcd_effect effect[EFFECT_COUNT];
	struct hal_lcd_mode mode[MODE_COUNT];
	int effect_cnt;
	int mode_cnt;
	int current_mode;
};

struct hal_panel_ctrl_data {
	struct hal_panel_data panel_data;
	int level;
	int mode;
	int index;
	ctrl_id id;
};
#endif
/*lenovo.sw2 add for lcd effect end*/


int msm_fb_get_iommu_domain(struct fb_info *info, int domain);
/* get the framebuffer physical address information */
int get_fb_phys_info(unsigned long *start, unsigned long *len, int fb_num,
	int subsys_id);
struct fb_info *msm_fb_get_writeback_fb(void);
int msm_fb_writeback_init(struct fb_info *info);
int msm_fb_writeback_start(struct fb_info *info);
int msm_fb_writeback_queue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_dequeue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_stop(struct fb_info *info);
int msm_fb_writeback_terminate(struct fb_info *info);
int msm_fb_writeback_set_secure(struct fb_info *info, int enable);
int msm_fb_writeback_iommu_ref(struct fb_info *info, int enable);
bool msm_fb_get_cont_splash(void);

#endif /*_MSM_MDP_H_*/
