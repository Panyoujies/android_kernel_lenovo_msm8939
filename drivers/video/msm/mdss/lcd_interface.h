#ifndef __LCD_EFFECT_H__
#define __LCD_EFFECT_H__

#include <linux/msm_mdp.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "mdss_dsi_cmd.h"

#define CMDS_LAST_CNT 4

struct lcd_cmds {
	struct dsi_cmd_desc *cmd;
	int cnt;
};

struct lcd_effect_code {
	char **code;
	const int cnt;
};

struct lcd_effect_cmds {
	struct lcd_effect_code effect_code;
	struct lcd_cmds lcd_cmd;
};

struct lcd_effect_cmd_data {
	struct lcd_effect_cmds *effect_cmd;
	const int cnt;
};

struct lcd_effect {
	//char *aliases;
	const char *name;
	const int max_level;
	int level;
	struct lcd_effect_cmd_data effect_cmd_data;
};

struct lcd_mode {
	//char *aliases;
	const char *name;
	int default_level[3];
	int level[3];
	const int bl_ctrl;
	const int effect_hold;
	struct lcd_cmds mode_cmd;
};

typedef enum {
	EFFECT = 0,
	MODE = 1,
}control_mode;

struct lcd_effect_data {
	struct lcd_effect *effect;
	struct lcd_cmds *head_cmd;
	const int supported_effect;
};

struct lcd_mode_data {
	struct lcd_mode *mode;
	struct lcd_cmds *head_cmd;
	const int supported_mode;
	int current_mode;
};

struct effect_policy {
	const int effect_enabled;
	const int low_bl_disable_cabc;
};
struct panel_effect_data {
	struct effect_policy *policy;
	struct lcd_effect_data *effect_data;
	struct lcd_mode_data *mode_data;
	struct lcd_mode *mode;
	struct lcd_cmds save_cmd;
	struct dsi_cmd_desc *buf;
	int buf_size;
	int mode_have_inited;
};

int malloc_lcd_effect_code_buf(struct panel_effect_data *panel_data);
void lcd_cabc_ctrl(struct msm_fb_data_type *mfd, struct panel_effect_data * panel_data, int level);
int update_init_code(
		struct mdss_dsi_ctrl_pdata*, 
		struct panel_effect_data *, 
		void (*)(struct mdss_dsi_ctrl_pdata *ctrl,struct dsi_panel_cmds *pcmds));
int handle_lcd_effect_data(struct msm_fb_data_type *, struct panel_effect_data *, struct hal_panel_ctrl_data *);
int get_effect_index_by_name(char *, struct panel_effect_data *);
int get_mode_index_by_name(char *, struct panel_effect_data *);
#endif
