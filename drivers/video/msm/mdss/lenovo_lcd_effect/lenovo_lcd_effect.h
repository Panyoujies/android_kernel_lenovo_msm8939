#ifndef __LENOVO_LCD_EFFECT_H__
#define __LENOVO_LCD_EFFECT_H__
#define MAX_SUPPORT_EFFECT_COUNT 10


extern int g_lcd_effect_log_on;




#define LCD_EFFECT_LOG_PRINT(level, sub_module, fmt, arg...)      \
	do {                                                    \
           pr_debug("LCD_EFFECT/"fmt, ##arg);  \
        }while(0)


#define LCD_EFFECT_LOG(fmt, arg...) \
       do {                                                    \
           if(g_lcd_effect_log_on) LCD_EFFECT_LOG_PRINT(ANDROID_LOG_ERROR, "LCD_EFFECT1", fmt, ##arg);  \
        }while(0)

struct lenovo_lcd_effect_data
{
	const char *effect_name;
	int is_support;
	int max_level;
	struct dsi_cmd_desc *cmds;
	int cmds_cnt;	
};
	

	
	
struct lenovo_lcd_mode_data 
{
	const char *mode_name;	
	int is_support;
	struct dsi_cmd_desc *cmds;
	int cmds_cnt;
	const int bl_ctrl;
};

struct lenovo_lcd_effect
{
	int effectDataCount;
	int modeDataCount;
	struct lenovo_lcd_effect_data *pEffectData;
	struct lenovo_lcd_mode_data  *pModeData;
	int current_mode_num;
	int current_effect_num[MAX_SUPPORT_EFFECT_COUNT];
	int (*pFuncSetEffect)(void *pData,int index,int level);
	int (*pFuncInitEffect)(void *pData);
	struct dsi_cmd_desc *pSaveOnCmds;
};

typedef enum {
	EFFECT_INDEX_CABC = 0,
	EFFECT_INDEX_SAT,
	EFFECT_INDEX_HUE,
	EFFECT_INDEX_CONTRAST,
	EFFECT_INDEX_GAMMA,
	EFFECT_INDEX_INVERSE,
	EFFECT_INDEX_SRE,  
} effect_index;



typedef enum {
	MODE_INDEX_CUSTOM = 0,
	MODE_INDEX_AUTO ,
	MODE_INDEX_NORMAL ,
	MODE_INDEX_COMFORT ,
	MODE_INDEX_OUTSIDE ,
	MODE_INDEX_ULTRA , 
} mode_index;
#endif
