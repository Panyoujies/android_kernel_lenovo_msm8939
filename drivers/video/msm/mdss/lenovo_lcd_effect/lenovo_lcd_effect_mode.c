#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include <linux/msm_mdp.h>

#include "../mdss.h"
#include "../mdss_panel.h"
#include "../mdss_dsi.h"
#include "../mdss_debug.h"
#include "lenovo_lcd_effect.h"

#define CUSTOM_MODE_INDEX 0

extern  void lenovo_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl_pdata, struct dsi_cmd_desc *cmds,int cmds_cnt,int hs_mode);
extern int lenovo_lcd_effect_set_custom_mode(struct mdss_dsi_ctrl_pdata *ctrl_data);

static struct lenovo_lcd_mode_data lcdModeData[]={
	{"custom_mode",0,NULL,0,0},
	{"auto_mode",0,NULL,0,0},
	{"normal_mode",0,NULL,0,0},
	{"comfort_mode",0,NULL,0,0},
	{"outside_mode",0,NULL,0,0},
	{"ultra_mode", 0,NULL,0,0},
	{"camera_mode",0,NULL,0,0},
};


int lenovo_lcd_effect_mode_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct lenovo_lcd_effect *pLcdEffect=&(ctrl_pdata->lenovoLcdEffect);
	
	pLcdEffect->current_mode_num = 2;//default mode

	if(ctrl_pdata->custom_mode_cmds.cmd_cnt)
	{
		lcdModeData[0].is_support = 1;
		lcdModeData[0].cmds = ctrl_pdata->custom_mode_cmds.cmds;
		lcdModeData[0].cmds_cnt = ctrl_pdata->custom_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->default_mode_cmds.cmd_cnt)
	{
		lcdModeData[2].is_support = 1;
		lcdModeData[2].cmds = ctrl_pdata->default_mode_cmds.cmds;
		lcdModeData[2].cmds_cnt = ctrl_pdata->default_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->comfort_mode_cmds.cmd_cnt)
	{
		lcdModeData[3].is_support = 1;
		lcdModeData[3].cmds = ctrl_pdata->comfort_mode_cmds.cmds;
		lcdModeData[3].cmds_cnt = ctrl_pdata->comfort_mode_cmds.cmd_cnt;
	}
		if(ctrl_pdata->outside_mode_cmds.cmd_cnt)
	{
		lcdModeData[4].is_support = 1;
		lcdModeData[4].cmds = ctrl_pdata->outside_mode_cmds.cmds;
		lcdModeData[4].cmds_cnt = ctrl_pdata->outside_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->ultra_mode_cmds.cmd_cnt)
	{
		lcdModeData[5].is_support = 1;
		lcdModeData[5].cmds = ctrl_pdata->ultra_mode_cmds.cmds;
		lcdModeData[5].cmds_cnt = ctrl_pdata->ultra_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->camera_mode_cmds.cmd_cnt)
	{
		lcdModeData[6].is_support = 1;
		lcdModeData[6].cmds = ctrl_pdata->camera_mode_cmds.cmds;
		lcdModeData[6].cmds_cnt = ctrl_pdata->camera_mode_cmds.cmd_cnt;
	}
	pLcdEffect->modeDataCount = ARRAY_SIZE(lcdModeData);
	pLcdEffect->pModeData = &lcdModeData[0];
	return 0;
}

int lcd_set_mode(struct mdss_dsi_ctrl_pdata *ctrl_data,int level)
{
	struct lenovo_lcd_effect *lcdEffect = NULL;
	struct lenovo_lcd_mode_data *pModeData = NULL;
	struct dsi_cmd_desc *cmds = NULL;
	struct dsi_cmd_desc *last_cmds = NULL;
	static int save_level = -1;
	int cmds_cnt = 0;

	LCD_EFFECT_LOG("%s:level=%d\n",__func__,level);
	
	lcdEffect = &(ctrl_data->lenovoLcdEffect);
	if(lcdEffect == NULL) return -1;
	
	pModeData =  lcdEffect->pModeData;
	if(pModeData == NULL) return -1;
		
	if(level >9) 
	{
		pr_err("[houdz1]%s:level (%d) is larger than 9 \n",__func__,level);
		return -1;
	}
	if(level == save_level) return 0;

	pModeData += level;	
	
	if(pModeData->is_support != 1)
	{
		pr_err("[houdz1]%s:the mode(%s) is not support\n",__func__,pModeData->mode_name);
		return -1;
	}	

	cmds = pModeData->cmds;
	cmds_cnt = pModeData->cmds_cnt;
	if(level !=  0){ //custom mode do not download command
		if(level != 6){
			last_cmds = cmds+cmds_cnt-1 ;
			last_cmds->payload[1] = ((last_cmds->payload[1]) & 0xF0) |(lcdEffect->current_effect_num[0]);
		}
		lenovo_dsi_panel_cmds_send(ctrl_data,cmds,cmds_cnt,1);
	}
	else{
		lenovo_lcd_effect_set_custom_mode(ctrl_data);
	}
	lcdEffect->current_mode_num = level;
	save_level = level;
	return 0;
}


int lcd_get_mode_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	int i;
	struct lenovo_lcd_mode_data *pModeData = lcdEffect->pModeData;

	for (i = 0; i < lcdEffect->modeDataCount; i++) 
	{
		if(pModeData->is_support == 1)
		{
			memcpy(panel_data->mode[i].name, pModeData->mode_name, strlen(pModeData->mode_name));
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->mode[i].name);
		}
		else 	
		{
			memcpy(panel_data->mode[i].name, "null",5);
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->mode[i].name);
		}		
		
		pModeData++;
	}
	panel_data->mode_cnt = lcdEffect->modeDataCount;
	LCD_EFFECT_LOG("%s: mode_cnt=%d\n", __func__,panel_data->mode_cnt);
	return panel_data->mode_cnt;
}


int lcd_get_mode_level(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	struct lenovo_lcd_mode_data *pModeData = lcdEffect->pModeData;
	int currentMode = lcdEffect->current_mode_num ;

	pModeData += currentMode  ;
	LCD_EFFECT_LOG("%s: name: [%s]  mode: [%d]\n", __func__,pModeData->mode_name,currentMode);
	memcpy(panel_data->mode[currentMode].name,pModeData->mode_name,strlen(pModeData->mode_name));
	return currentMode;
}

int is_custom_mode(struct lenovo_lcd_effect *lcdEffect)
{
	return ((lcdEffect->current_mode_num) ==CUSTOM_MODE_INDEX);
}





