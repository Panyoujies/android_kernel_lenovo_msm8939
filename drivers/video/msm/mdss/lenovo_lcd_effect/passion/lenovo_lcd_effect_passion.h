#include "../../mdss_panel.h"
#include "../../mdss_dsi_cmd.h"
#include "../lenovo_lcd_effect.h"
#include "../../mdss_dsi.h"


static char lcd_cabc_tm[4][2] = {{0x55,0x00},{0x55,0x01},{0x55,0x02},{0x55,0x03}};
static char nt35596_page_00[2] = {0xFF,0x00};

static struct dsi_cmd_desc cabc_cmd_tm[] =
{
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_cabc_tm[0])},(char *)(&lcd_cabc_tm[0])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_cabc_tm[1])},(char *)(&lcd_cabc_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_cabc_tm[2])},(char *)(&lcd_cabc_tm[2])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_cabc_tm[3])},(char *)(&lcd_cabc_tm[3])},
};



static char lcd_ce_tm[2][2] = {{0x55,0x00},{0x55,0xB0}};
static char lcd_ce_tm_4F[9][2] = {{0x4F,0x00},{0x4F,0x02},{0x4F,0x04},{0x4F,0x06},{0x4F,0x08},{0x4F,0x0A},{0x4F,0x0C},{0x4F,0x0D},{0x4F,0x0F}};
static char nt35596_page_03[2][2] = {{0xFF,0x03},{0xFB,0x01}};
static char lcd_ce_yassy_4F[9][2] = {{0x4F,0x00},{0x4F,0x04},{0x4F,0x07},{0x4F,0x0A},{0x4F,0x0D},{0x4F,0x13},{0x4F,0x19},{0x4F,0x1C},{0x4F,0x1E}};

static struct dsi_cmd_desc ce_cmd_tm[] =
{
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[0])},(char *)(&lcd_ce_tm_4F[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[0])},(char *)(&lcd_ce_tm[0])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[1])},(char *)(&lcd_ce_tm_4F[1])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[2])},(char *)(&lcd_ce_tm_4F[2])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[3])},(char *)(&lcd_ce_tm_4F[3])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[4])},(char *)(&lcd_ce_tm_4F[4])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[5])},(char *)(&lcd_ce_tm_4F[5])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[6])},(char *)(&lcd_ce_tm_4F[6])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[7])},(char *)(&lcd_ce_tm_4F[7])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_tm_4F[8])},(char *)(&lcd_ce_tm_4F[8])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

};

static struct dsi_cmd_desc ce_cmd_yassy[] =
{
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[0])},(char *)(&lcd_ce_yassy_4F[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[0])},(char *)(&lcd_ce_tm[0])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[1])},(char *)(&lcd_ce_yassy_4F[1])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[2])},(char *)(&lcd_ce_yassy_4F[2])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[3])},(char *)(&lcd_ce_yassy_4F[3])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[4])},(char *)(&lcd_ce_yassy_4F[4])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[5])},(char *)(&lcd_ce_yassy_4F[5])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[6])},(char *)(&lcd_ce_yassy_4F[6])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[7])},(char *)(&lcd_ce_yassy_4F[7])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_03[0])},(char *)(&nt35596_page_03[0])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt35596_page_03[1])},(char *)(&nt35596_page_03[1])},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(lcd_ce_yassy_4F[8])},(char *)(&lcd_ce_yassy_4F[8])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(nt35596_page_00)},(char *)(&nt35596_page_00[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},

};

#if 0
static struct dsi_cmd_desc ce_cmd_tm[] =
{
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[0])},(char *)(&lcd_ce_tm[0])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[1])},(char *)(&lcd_ce_tm[1])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[2])},(char *)(&lcd_ce_tm[2])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[3])},(char *)(&lcd_ce_tm[3])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[4])},(char *)(&lcd_ce_tm[4])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[5])},(char *)(&lcd_ce_tm[5])},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(lcd_ce_tm[6])},(char *)(&lcd_ce_tm[6])},
};
#endif

struct lenovo_lcd_effect_data lcd_effect_data_passion_tm[]=
{
	{"cabc",1,4,cabc_cmd_tm,2},
	{"ce",1,9,&ce_cmd_tm[0],5},
	{"cta",1,0,NULL,0},//gamma_init to set "pLcdEffectData_cta" and "count"
	{"aco",0,0,NULL,0},
	{"gamma",0,0,NULL,0},
	{"inverse",0,0,NULL,0},
	{"sre",0,0,NULL,0},		
};


struct lenovo_lcd_effect_data lcd_effect_data_passion_yassy[]=
{
	{"cabc",1,4,cabc_cmd_tm,2},
	{"ce",1,9,&ce_cmd_yassy[0],5},
	{"cta",1,0,NULL,0},//gamma_init to set "pLcdEffectData_cta" and "count"
	{"aco",0,0,NULL,0},
	{"gamma",0,0,NULL,0},
	{"inverse",0,0,NULL,0},
	{"sre",0,0,NULL,0},	
};
