

#include "lenovo_lcd_effect_passion.h"

int lenovo_lcd_effect_color_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);
	int ret = 0;
	if(ctrl_pdata->panel_id == 0)
	{
		pLcdEffect->effectDataCount = ARRAY_SIZE(lcd_effect_data_passion_tm);
		pLcdEffect->pEffectData = &lcd_effect_data_passion_tm[0];
	}
	else 
	{
		pLcdEffect->effectDataCount = ARRAY_SIZE(lcd_effect_data_passion_yassy);
		pLcdEffect->pEffectData = &lcd_effect_data_passion_yassy[0];

	}

	return ret;
}

	
