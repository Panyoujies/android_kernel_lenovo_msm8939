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


extern int lenovo_lcd_effect_color_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
extern  void lenovo_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl_pdata, struct dsi_cmd_desc *cmds,int cmds_cnt,int hs_mode);

int lcd_get_effect_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	int i;
	struct lenovo_lcd_effect_data *pEffectData = lcdEffect->pEffectData;
	
	if(pEffectData==NULL) return 0;
	
	panel_data->effect_cnt = lcdEffect->effectDataCount;
	for (i = 0; i < panel_data->effect_cnt; i++) 
	{
		if(pEffectData->is_support == 1)
		{
			memcpy(panel_data->effect[i].name, pEffectData->effect_name, strlen(pEffectData->effect_name));
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->effect[i].name);
		}
		else
		{
			memcpy(panel_data->effect[i].name, "null",5);
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->effect[i].name);
		}
		pEffectData ++ ;
	}
	LCD_EFFECT_LOG("%s:GET_EFFECT_NUM = 0x%x\n",__func__,panel_data->effect_cnt);
	return panel_data->effect_cnt;
}

 int lcd_get_effect_level(struct lenovo_lcd_effect *lcdEffect, int index)
{
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;
	
	if((index <0)||(pEffectData ==NULL)) return -1;
	
	pEffectData +=index;
	LCD_EFFECT_LOG("%s: name: [%s] index: [%d] level: [%d]\n", __func__,pEffectData->effect_name, index, lcdEffect->current_effect_num[index]);
	return lcdEffect->current_effect_num[index];
}


int lcd_get_effect_max_level(struct lenovo_lcd_effect *lcdEffect, int index)
{
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;
	pEffectData += index;
	LCD_EFFECT_LOG("%s: name: [%s] index: [%d] max_level: [%d]\n", __func__,pEffectData->effect_name, index, pEffectData->max_level);
	return pEffectData->max_level;
}

int value_reg_55 = 0xB3;

int lcd_set_effect_level(struct mdss_dsi_ctrl_pdata *ctrl_data,int index,int level)
{
	struct lenovo_lcd_effect *lcdEffect = NULL;
	struct lenovo_lcd_effect_data *pEffectData = NULL;
	struct dsi_cmd_desc *cmds = NULL;
	struct dsi_cmd_desc *cmds_0x55 = NULL;
	int *pEffectCurrentLevel = NULL ;
	int cmds_cnt = 0;
	int maxLevel = 0;

	LCD_EFFECT_LOG("%s:index = %d,level=%d\n",__func__,index,level);
	
	lcdEffect = &(ctrl_data->lenovoLcdEffect);
	if(lcdEffect == NULL) return -1;
	
	pEffectData =  lcdEffect->pEffectData;
	if(pEffectData == NULL) return -1;
		
	if(index > (lcdEffect->effectDataCount)) 
	{
		pr_err("[houdz1]%s:index(%d)>effectDataCount\n",__func__,index);
		return -1;
	}
	pEffectData += index;	
	
	if(pEffectData->is_support != 1)
	{
		pr_err("[houdz1]%s:the effect(%s) is not support\n",__func__,pEffectData->effect_name);
		return -1;
	}	
	maxLevel = pEffectData->max_level;
	if((level <0) ||(level > maxLevel)) 
	{
		pr_err("[houdz1]%s:level(%d) is errort\n",__func__,level);
		return -1;
	}

	cmds = pEffectData->cmds + level*(pEffectData->cmds_cnt);
	cmds_cnt = pEffectData->cmds_cnt;

	if(index == 0){
		cmds_0x55 = cmds + (pEffectData->cmds_cnt) -1;
		value_reg_55 = (value_reg_55 & 0xF0) | ((cmds_0x55->payload[1])&0x0F);
		cmds_0x55->payload[1] = value_reg_55;
	}else if(index == 1)
	{
		cmds_0x55 = cmds + (pEffectData->cmds_cnt) -1;
		value_reg_55 = (value_reg_55 &0x0F) | ((cmds_0x55->payload[1])&0xF0);
		cmds_0x55->payload[1] = value_reg_55;
	}
	lenovo_dsi_panel_cmds_send(ctrl_data,cmds,cmds_cnt,1);

	pEffectCurrentLevel = lcdEffect->current_effect_num;
	pEffectCurrentLevel +=index;
	*pEffectCurrentLevel = level;
	return 0;
}

int lenovo_lcd_effect_gamma_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int cmdCnt = ctrl_pdata->gamma_cmds.cmd_cnt;
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);
	struct lenovo_lcd_effect_data *pLcdEffectData = NULL;
	struct dsi_cmd_desc *pGammaCmds = ctrl_pdata->gamma_cmds.cmds ;

	if((pLcdEffect->pEffectData == NULL)||(pGammaCmds ==NULL)) return -1;

	pLcdEffectData = pLcdEffect->pEffectData ;
	pLcdEffectData += EFFECT_INDEX_HUE;
	pLcdEffectData->cmds = pGammaCmds;
	if(ctrl_pdata->gamma_count) 
	{
		pLcdEffectData->cmds_cnt = cmdCnt/(ctrl_pdata->gamma_count);
		pLcdEffectData->max_level = ctrl_pdata->gamma_count;
	}
	return 0;
}

int lenovo_lcd_effect_color_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);	
	
	memset(pLcdEffect->current_effect_num,0x0,sizeof(pLcdEffect->current_effect_num));
	pLcdEffect->current_effect_num[0] = 3;//cabc default moving mode
	pLcdEffect->current_effect_num[1] = 1;//ce default =0x80
	pLcdEffect->current_effect_num[2] = 8;//gamma = TC 7200
	lenovo_lcd_effect_color_data_init(ctrl_pdata);
	lenovo_lcd_effect_gamma_init(ctrl_pdata);
	return 0;
}


int lenovo_lcd_effect_set_custom_mode(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	int i;
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_data->lenovoLcdEffect);
	struct lenovo_lcd_effect_data *pEffectData=pLcdEffect->pEffectData;

	for (i = 0; i < pLcdEffect->effectDataCount; i++) {
		if(pEffectData->is_support) lcd_set_effect_level(ctrl_data,i,pLcdEffect->current_effect_num[i]);
	}
	return 0;
}



