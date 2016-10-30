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

#define CMDS_LAST_CNT 2

int g_lcd_effect_log_on=1;



extern int lcd_get_effect_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int lcd_get_effect_max_level(struct lenovo_lcd_effect *lcdEffect, int index);
extern int lcd_get_effect_level(struct lenovo_lcd_effect *lcdEffect, int index);
extern int lcd_set_effect_level(struct mdss_dsi_ctrl_pdata *ctrl_data,int index,int level);
extern int lenovo_lcd_effect_color_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
extern int lenovo_lcd_effect_mode_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
extern int lcd_set_mode(struct mdss_dsi_ctrl_pdata *ctrl_data,int level);
extern int lcd_get_mode_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int lcd_get_mode_level(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int is_custom_mode(struct lenovo_lcd_effect *lcdEffect);

#if 0
static int show_lcd_param(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	int i, j;

	printk("======================================= cmds_cnt %d =========================================\n", cmd_cnt);
	for (i = 0; i < cmd_cnt; i++) {
		printk("%02x %02x %02x %02x %02x %02x ", cmds[i].dchdr.dtype, 
				cmds[i].dchdr.last, 
				cmds[i].dchdr.vc, 
				cmds[i].dchdr.ack, 
				cmds[i].dchdr.wait, 
				cmds[i].dchdr.dlen);
		for (j = 0; j < cmds[i].dchdr.dlen; j++) {
			printk("%02x ", cmds[i].payload[j]);
		}	
		printk("\n");
	}
	pr_debug("===========================================================================================\n");
	return 0;
}
#endif

static int get_mode_max_cnt(struct lenovo_lcd_effect *pLcdEffect)
{
	int i;
	int temp;
	int cnt = 0;
	struct lenovo_lcd_mode_data *pModeData = pLcdEffect->pModeData;

	for (i = 0; i < pLcdEffect->modeDataCount; i++) {
		if(pModeData->is_support){
			temp = pModeData->cmds_cnt;
			cnt = (cnt > temp) ? cnt : temp;
			LCD_EFFECT_LOG("%s cnt = %d temp = %d\n", __func__, cnt, temp);
		}
		pModeData ++;
	}

	return cnt;
}

static int get_effect_max_cnt(struct lenovo_lcd_effect *pLcdEffect )
{
	int cnt = 0;
	int temp;
	int i;
	struct lenovo_lcd_effect_data *pEffectData=pLcdEffect->pEffectData;

	for (i = 0; i < pLcdEffect->effectDataCount; i++) {
		if(pEffectData->is_support){
			temp =pEffectData->cmds_cnt;
			cnt = cnt + temp;
			LCD_EFFECT_LOG("%s cnt = %d temp = %d\n", __func__, cnt, temp);
		}
		pEffectData ++;
	}

	return cnt;
}

static int get_init_code_max_cnt(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_data->lenovoLcdEffect);


	int cnt =ctrl_data->on_cmds.cmd_cnt;
	cnt += get_mode_max_cnt(pLcdEffect);
	cnt += get_effect_max_cnt(pLcdEffect);
	LCD_EFFECT_LOG("%s cnt: %d\n", __func__, cnt);
	return cnt;
}

static int lenovo_lcd_effect_code_buf_init(struct mdss_dsi_ctrl_pdata *ctrl_data)
{

	int count=0;
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_data->lenovoLcdEffect);

	count = get_init_code_max_cnt(ctrl_data);
	LCD_EFFECT_LOG("%s:count=%d\n",__func__,count);
	if(count)  pLcdEffect->pSaveOnCmds = kmalloc(sizeof(struct dsi_cmd_desc) * count, GFP_KERNEL);
	if(!pLcdEffect->pSaveOnCmds ) 
	{
		pr_err("%s:kmalloc fail\n",__func__);
		return -ENOMEM;
	}
	return 0;
}

extern int value_reg_55;
static int copy_init_code(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_data->lenovoLcdEffect);
	struct lenovo_lcd_effect_data *pEffectData=pLcdEffect->pEffectData;
	struct lenovo_lcd_mode_data *pModeData=pLcdEffect->pModeData;
	struct dsi_cmd_desc *pSaveCmds=NULL;
	struct dsi_cmd_desc *pTempCmds=NULL;
	struct dsi_cmd_desc *pSrcCmds;
	int ret = 0;
	int i,init_cnt;

	pSaveCmds= pLcdEffect->pSaveOnCmds;
	if(pSaveCmds ==NULL){
		pr_err("%s:pLcdEffect->pSaveOnCmds =NULL\n",__func__);
		return -1;
	}

	init_cnt = ctrl_data->on_cmds.cmd_cnt;
	 /******copy init code *******/
	pSrcCmds =  ctrl_data->on_cmds.cmds;
	memcpy(pSaveCmds,pSrcCmds, (init_cnt- CMDS_LAST_CNT) * sizeof (struct dsi_cmd_desc));
	pSaveCmds +=  (init_cnt - CMDS_LAST_CNT);
	ret += (init_cnt - CMDS_LAST_CNT);
	LCD_EFFECT_LOG("%s:count = 0x%x \n",__func__,ret);

	if(pLcdEffect->current_mode_num != 0){
	//if(pLcdEffect->current_mode_num == 3){ //comfort mode
		 /******copy mode code *******/
		LCD_EFFECT_LOG("%s:current_mode_num = 0x%x \n",__func__,pLcdEffect->current_mode_num);
		pModeData += pLcdEffect->current_mode_num;
		pSrcCmds = pModeData->cmds;
		if(pModeData->is_support) {
				memcpy(pSaveCmds,pSrcCmds,(pModeData->cmds_cnt) * sizeof (struct dsi_cmd_desc));
				pSaveCmds +=(pModeData->cmds_cnt);
				ret += (pModeData->cmds_cnt);
		}
		LCD_EFFECT_LOG("%s:count = 0x%x \n",__func__,ret);
	}

	 /******copy effect code *******/
	for (i = 0; i < pLcdEffect->effectDataCount; i++) {
		pSrcCmds = pEffectData->cmds;
		pSrcCmds += (pLcdEffect->current_effect_num[i])*(pEffectData->cmds_cnt);
		if(pEffectData->is_support) {
			memcpy(pSaveCmds,pSrcCmds,(pEffectData->cmds_cnt) * sizeof (struct dsi_cmd_desc));
			pSaveCmds +=(pEffectData->cmds_cnt);
			if((i==0)||(i==1)){
				pTempCmds= pSaveCmds -1;
				pTempCmds->payload[1] = value_reg_55;
			}
			ret += (pEffectData->cmds_cnt);
			LCD_EFFECT_LOG("%s:i= 0x%x, current_level = 0x%x ,current_count=0x%x,total_count = 0x%x,value_reg_55=0x%x \n",__func__,i,pLcdEffect->current_effect_num[i],pEffectData->cmds_cnt,ret,value_reg_55);
		}
		if(pLcdEffect->current_mode_num != 0) break;//if it is not custom mode .it will only set cabc level
		else pEffectData ++ ;
	}
	LCD_EFFECT_LOG("%s:i=%d,count = 0x%x \n",__func__,i,ret);

	 /******copy seleep out code *******/
	
	pSrcCmds = ctrl_data->on_cmds.cmds + (init_cnt - CMDS_LAST_CNT);
	memcpy(pSaveCmds,pSrcCmds,CMDS_LAST_CNT * sizeof (struct dsi_cmd_desc));
	ret += CMDS_LAST_CNT;

	LCD_EFFECT_LOG("%s:count = 0x%x \n",__func__,ret);
	return ret;
}




int lenovo_updata_effect_code(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_data->lenovoLcdEffect);
	struct dsi_cmd_desc *pSaveCmds= pLcdEffect->pSaveOnCmds;
	int count = 0;
	
	if(pLcdEffect->pSaveOnCmds == NULL)  return -1;
	
	count = copy_init_code(ctrl_data);
	if(count >(ctrl_data->on_cmds.cmd_cnt)){
		ctrl_data->on_cmds.cmds = pSaveCmds;
		ctrl_data->on_cmds.cmd_cnt = count;
	}

	//show_lcd_param(ctrl_data->on_cmds.cmds,ctrl_data->on_cmds.cmd_cnt );
	return 0;
}






int lenovo_get_index_by_name(struct mdss_dsi_ctrl_pdata *ctrl_pdata,char *name)
{
	struct lenovo_lcd_effect *pLcdEffect=NULL;
	struct lenovo_lcd_effect_data *pEffectData=NULL;
	int effectCount;
	int i,ret;

	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1] %s fail:(ctrl_pdata == NULL)\n",__func__);
		ret = -1;
	}

	pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);
	if (pLcdEffect == NULL) 
	{
		pr_err("[houdz1] %s fail:(pLcdEffect == NULL)\n",__func__);
		ret = -1;
	}

	pEffectData = pLcdEffect->pEffectData;
	if (pLcdEffect == NULL) 
	{
		pr_err("[houdz1] %s fail:(pEffectData == NULL)\n",__func__);
		ret = -1;
	}

	effectCount = pLcdEffect->effectDataCount;
	for(i=0;i<effectCount;i++)
	{
		if (!strcmp(name,pEffectData->effect_name)) return i;
		pEffectData++;
	}
	return -EINVAL;
}



void lenovo_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl_pdata, struct dsi_cmd_desc *cmds,int cmds_cnt,int hs_mode)
{
	struct dcs_cmd_req cmdreq;


	//printk("[houdz1]%s: cmds_cnt=%d,hs_mode=%d\n", __func__,cmds_cnt,hs_mode);//lenoco.sw2 houdz1 add	
#if 0

	{
		int i,j;
		for (i = 0; i < cmds_cnt; i++)
		 {
			printk("%2x %2x %2x %2x %2x %2x ", cmds[i].dchdr.dtype,
					cmds[i].dchdr.last,
					cmds[i].dchdr.vc,
					cmds[i].dchdr.ack,
					cmds[i].dchdr.wait,
					cmds[i].dchdr.dlen);
			for (j = 0; j < cmds[i].dchdr.dlen; j++)
			{
				printk("%2x ", cmds[i].payload[j]);
			}
			printk("\n");
		}
	}
#endif
	{
		int i,j;
		i = cmds_cnt-1;
		pr_debug("%2x %2x %2x %2x %2x %2x ", cmds[i].dchdr.dtype,
							cmds[i].dchdr.last,
							cmds[i].dchdr.vc,
							cmds[i].dchdr.ack,
							cmds[i].dchdr.wait,
							cmds[i].dchdr.dlen);
		for (j = 0; j < cmds[i].dchdr.dlen; j++)
		{
			pr_debug("%2x ", cmds[i].payload[j]);
		}
		pr_debug("\n");
	}

	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cmds_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	//if(!hs_mode)   cmdreq.flags |= CMD_REQ_LP_MODE;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);
}




int lenovo_lcd_effect_init(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	int ret = 0;

	ret = lenovo_lcd_effect_color_init(ctrl_data);
	ret = lenovo_lcd_effect_mode_data_init(ctrl_data);
	ret = lenovo_lcd_effect_code_buf_init(ctrl_data);
	return ret;

}



int lenovo_lcd_effect_handle(struct mdss_dsi_ctrl_pdata *ctrl_data,struct hal_panel_ctrl_data *hal_ctrl_data)
{
	
	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);	
	int ret = 0;
	
	if(lcdEffect == NULL)  return -1;
	LCD_EFFECT_LOG("%s:hal_ctrl_data->id = %d\n",__func__,hal_ctrl_data->id);
	

	switch(hal_ctrl_data->id){
		case GET_EFFECT_NUM:
			ret = lcd_get_effect_support(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case GET_EFFECT_LEVEL:
			ret = lcd_get_effect_max_level(lcdEffect,hal_ctrl_data->index);
			break;
		case GET_EFFECT:
			ret = lcd_get_effect_level(lcdEffect,hal_ctrl_data->index);
			break;
		case SET_EFFECT:
			if(is_custom_mode(lcdEffect) || (hal_ctrl_data->index ==EFFECT_INDEX_CABC));
				ret = lcd_set_effect_level(ctrl_data,hal_ctrl_data->index,hal_ctrl_data->level);
			break;
		case GET_MODE_NUM:
			ret = lcd_get_mode_support(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case GET_MODE:
			ret = lcd_get_mode_level(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case SET_MODE:
			ret = lcd_set_mode(ctrl_data,hal_ctrl_data->mode);
			break;
		default:

			break;
		}
	return ret;
}

