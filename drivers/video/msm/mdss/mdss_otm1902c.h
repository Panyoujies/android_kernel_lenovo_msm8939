#ifndef MDSS_otm1902c_H
#define MDSS_otm1902c_H
#include "lcd_interface.h"
#include "mdss_panel.h"

static char led_cmd1_head[] = {0x99, 0x95,0x27};	
static char led_cmd1_end[] = {0x99, 0x00,0x00};

/*********************************** ce *************************************/
static struct dsi_cmd_desc otm1902c_effect_ce_cmd1_head0[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
};
char *otm1902c_ceA_cmd1_head0[] = {
	led_cmd1_head,
};
static struct dsi_cmd_desc otm1902c_effect_ce_cmd1_end0[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
};
char *otm1902c_ceA_cmd1_end0[] = {
	led_cmd1_end,
};
static char otm1902c_ce_head0[]={0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_head0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
};
char *otm1902c_ceA_head0[] = {
	otm1902c_ce_head0,
};

static char otm1902c_ce_head1[]={0xFF,0x19,0x06,0x01};
static struct dsi_cmd_desc otm1902c_effect_ce_head1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
};
char *otm1902c_ceA_head1[] = {
	otm1902c_ce_head1,
};

static char otm1902c_ce_head2[]={0x00,0x80};
static struct dsi_cmd_desc otm1902c_effect_ce_head2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
};
char *otm1902c_ceA_head2[] = {
	otm1902c_ce_head2,
};
static char otm1902c_ce_head3[]={0xFF,0x19,0x06};
static struct dsi_cmd_desc otm1902c_effect_ce_head3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
};
char *otm1902c_ceA_head3[] = {
	otm1902c_ce_head3,
};

static char otm1902c_ce_head7[]={0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_head7[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head7)}, otm1902c_ce_head7},
};
char *otm1902c_ceA_head7[] = {
	otm1902c_ce_head7,
};

static char otm1902c_ce_head8[]={0x81,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_head8[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head8)}, otm1902c_ce_head8},
};
char *otm1902c_ceA_head8[] = {
	otm1902c_ce_head8,
};
static char otm1902c_ce_head4[]={0x00,0x82};
static struct dsi_cmd_desc otm1902c_effect_ce_head4[] = {
       {{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
};
char *otm1902c_ceA_head4[] = {
       otm1902c_ce_head4,
};
 
static char otm1902c_ce_head5[]={0xD6,0x02};
static struct dsi_cmd_desc otm1902c_effect_ce_head5[] = {
       {{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
};
char *otm1902c_ceA_head5[] = {
       otm1902c_ce_head5,
};
static char otm1902c_ce_head6[]={0x00,0xA0};
static struct dsi_cmd_desc otm1902c_effect_ce_head6[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head6)}, otm1902c_ce_head6},
};
char *otm1902c_ceA_head6[] = {
	otm1902c_ce_head6,
};

static char otm1902c_ce0A[]={0xD6,0x02,0x00,0x02,0x02,0xfb,0xf6,0xfe,0x06,0x00,0xf9,0xf9,0x05};
static char otm1902c_ce1A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf4,0xfb,0x06,0x00,0xf9,0xf9,0x02};
static char otm1902c_ce2A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf4,0xfd,0x06,0xff,0xf5,0xf9,0x04};
static char otm1902c_ce3A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf5,0xfa,0x06,0x00,0xf5,0xf7,0x04};
static char otm1902c_ce4A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf5,0xfb,0x08,0x00,0xf3,0xf5,0x05};
static char otm1902c_ce5A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf5,0xfb,0x09,0x00,0xf5,0xf4,0x05};
static char otm1902c_ce6A[]={0xD6,0x02,0xfe,0x04,0x03,0xfb,0xf5,0xfc,0x0a,0x02,0xf4,0xf3,0x05};
static char otm1902c_ce7A[]={0xD6,0x02,0xfe,0x03,0x02,0xfc,0xf5,0xfc,0x0c,0x02,0xf4,0xf1,0x05};
static char otm1902c_ce8A[]={0xD6,0x02,0xfe,0x03,0x02,0xfc,0xf5,0xfc,0x0c,0x02,0xf4,0xf1,0x05};
static char otm1902c_ce9A[]={0xD6,0x02,0xfe,0x03,0x02,0xfc,0xf5,0xfc,0x0c,0x02,0xf3,0xf1,0x05};

static char otm1902c_ceB_head0[]={0x00,0xB0};
static struct dsi_cmd_desc otm1902c_effect_ceB_head[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ceB_head0)}, otm1902c_ceB_head0},
};
char *otm1902c_ceB_head[] = {
	otm1902c_ceB_head0,
};

static char otm1902c_ce0B[]={0xD6,0x00,0x00,0x26,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce1B[]={0xD6,0x00,0x00,0x33,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce2B[]={0xD6,0x00,0x00,0x3a,0x80,0x6c,0x00,0x00,0x80,0x66,0xcd,0x00,0x00};
static char otm1902c_ce3B[]={0xD6,0x00,0x00,0x46,0xb3,0x6c,0x66,0x33,0x9a,0xb3,0xe6,0x80,0x00};
static char otm1902c_ce4B[]={0xD6,0x00,0x00,0x5a,0xcd,0xa6,0x80,0x33,0xda,0xfd,0xfd,0x9a,0x00};
static char otm1902c_ce5B[]={0xD6,0x00,0x00,0x5a,0xcd,0xa6,0x80,0x33,0xfd,0xfd,0xfd,0x9a,0x00};
static char otm1902c_ce6B[]={0xD6,0x00,0x00,0x5a,0xcd,0xa6,0x80,0x66,0xfd,0xfd,0xfd,0x9a,0x00};
static char otm1902c_ce7B[]={0xD6,0x00,0x00,0xcd,0xda,0xcd,0x80,0x66,0xfd,0xfd,0xfd,0xda,0x00};
static char otm1902c_ce8B[]={0xD6,0x00,0x00,0xcd,0xe6,0xcd,0xb3,0x66,0xfd,0xfd,0xfd,0xda,0x00};
static char otm1902c_ce9B[]={0xD6,0x00,0x00,0xcd,0xe6,0xcd,0xb3,0x66,0xfd,0xfd,0xfd,0xda,0x00};

static char otm1902c_ceC_head0[]={0x00,0xC0};
static struct dsi_cmd_desc otm1902c_effect_ceC_head[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ceC_head0)}, otm1902c_ceC_head0},
};
char *otm1902c_ceC_head[] = {
	otm1902c_ceC_head0,
};

static char otm1902c_ce0C[]={0xD6,0x00,0x00,0x33,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce1C[]={0xD6,0x00,0x00,0x44,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce2C[]={0xD6,0x00,0x00,0x4d,0x55,0x48,0x00,0x00,0x55,0x44,0x89,0x00,0x00};
static char otm1902c_ce3C[]={0xD6,0x00,0x00,0x5e,0x77,0x48,0x44,0x22,0x66,0x77,0x9a,0x55,0x00};
static char otm1902c_ce4C[]={0xD6,0x00,0x00,0x77,0x89,0x6f,0x55,0x22,0x91,0xa9,0xa9,0x66,0x00};
static char otm1902c_ce5C[]={0xD6,0x00,0x00,0x77,0x89,0x6f,0x55,0x22,0xa9,0xa9,0xa9,0x66,0x00};
static char otm1902c_ce6C[]={0xD6,0x00,0x00,0x77,0x89,0x6f,0x55,0x44,0xa9,0xa9,0xa9,0x66,0x00};
static char otm1902c_ce7C[]={0xD6,0x00,0x00,0x89,0x91,0x89,0x55,0x44,0xa9,0xa9,0xa9,0x91,0x00};
static char otm1902c_ce8C[]={0xD6,0x00,0x00,0x89,0x9a,0x89,0x77,0x44,0xa9,0xa9,0xa9,0x91,0x00};
static char otm1902c_ce9C[]={0xD6,0x00,0x00,0x89,0x9a,0x89,0x77,0x44,0xa9,0xa9,0xa9,0x91,0x00};

static char otm1902c_ceD_head0[]={0x00,0xD0};
static struct dsi_cmd_desc otm1902c_effect_ceD_head[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ceD_head0)}, otm1902c_ceD_head0},
};
char *otm1902c_ceD_head[] = {
	otm1902c_ceD_head0,
};

static char otm1902c_ce0D[]={0xD6,0x00,0x00,0x1a,0x1a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce1D[]={0xD6,0x00,0x09,0x22,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char otm1902c_ce2D[]={0xD6,0x00,0x09,0x26,0x2b,0x24,0x00,0x00,0x2b,0x22,0x44,0x00,0x00};
static char otm1902c_ce3D[]={0xD6,0x00,0x09,0x2f,0x3c,0x24,0x22,0x11,0x33,0x3c,0x4d,0x2b,0x00};
static char otm1902c_ce4D[]={0xD6,0x00,0x09,0x3c,0x44,0x37,0x2b,0x11,0x49,0x54,0x54,0x33,0x00};
static char otm1902c_ce5D[]={0xD6,0x00,0x09,0x3c,0x44,0x37,0x2b,0x11,0x54,0x54,0x54,0x33,0x00};
static char otm1902c_ce6D[]={0xD6,0x00,0x09,0x3c,0x44,0x37,0x2b,0x22,0x54,0x54,0x54,0x33,0x00};
static char otm1902c_ce7D[]={0xD6,0x00,0x09,0x44,0x49,0x44,0x2b,0x22,0x54,0x54,0x54,0x49,0x00};
static char otm1902c_ce8D[]={0xD6,0x00,0x09,0x44,0x4d,0x44,0x3c,0x22,0x54,0x54,0x54,0x49,0x00};
static char otm1902c_ce9D[]={0xD6,0x00,0x09,0x44,0x4d,0x44,0x3c,0x22,0x54,0x54,0x54,0x49,0x00};

static char otm1902c_ce_end0[]={0x00,0x82};
static struct dsi_cmd_desc otm1902c_effect_ce_end0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end0)}, otm1902c_ce_end0},
};
char *otm1902c_ce0_end[] = {
	otm1902c_ce_end0,
};

static char otm1902c_ce_end1[]={0xD6,0x12};
static struct dsi_cmd_desc otm1902c_effect_ce_end1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end1)}, otm1902c_ce_end1},
};
char *otm1902c_ce1_end[] = {
	otm1902c_ce_end1,
};

static char otm1902c_ce_end2[]={0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_end2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end2)}, otm1902c_ce_end2},
};
char *otm1902c_ce2_end[] = {
	otm1902c_ce_end2,
};

static char otm1902c_ce_end3[]={0x81,0x80};
static struct dsi_cmd_desc otm1902c_effect_ce_end3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end3)}, otm1902c_ce_end3},
};
char *otm1902c_ce3_end[] = {
	otm1902c_ce_end3,
};

static char otm1902c_ce_end4[]={0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_end4[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
};
char *otm1902c_ce4_end[] = {
	otm1902c_ce_end4,
};

static char otm1902c_ce_end5[]={0xFB,0x01};
static struct dsi_cmd_desc otm1902c_effect_ce_end5[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
};
char *otm1902c_ce5_end[] = {
	otm1902c_ce_end5,
};

static char otm1902c_ce_end6[]={0x00,0x80};
static struct dsi_cmd_desc otm1902c_effect_ce_end6[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
};
char *otm1902c_ce6_end[] = {
	otm1902c_ce_end6,
};

static char otm1902c_ce_end7[]={0xFF,0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_end7[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
};
char *otm1902c_ce7_end[] = {
	otm1902c_ce_end7,
};

static char otm1902c_ce_end8[]={0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_end8[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
};
char *otm1902c_ce8_end[] = {
	otm1902c_ce_end8,
};

static char otm1902c_ce_end9[]={0xFF,0x00,0x00,0x00};
static struct dsi_cmd_desc otm1902c_effect_ce_end9[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
};
char *otm1902c_ce9_end[] = {
	otm1902c_ce_end9,
};


char *otm1902c_ceA[] = {
	otm1902c_ce0A,
	otm1902c_ce1A,
	otm1902c_ce2A,
	otm1902c_ce3A,
	otm1902c_ce4A,
	otm1902c_ce5A,
	otm1902c_ce6A,
	otm1902c_ce7A,
	otm1902c_ce8A,
	otm1902c_ce9A,
};

char *otm1902c_ceB[] = {
	otm1902c_ce0B,
	otm1902c_ce1B,
	otm1902c_ce2B,
	otm1902c_ce3B,
	otm1902c_ce4B,
	otm1902c_ce5B,
	otm1902c_ce6B,
	otm1902c_ce7B,
	otm1902c_ce8B,
	otm1902c_ce9B,
};

char *otm1902c_ceC[] = {
	otm1902c_ce0C,
	otm1902c_ce1C,
	otm1902c_ce2C,
	otm1902c_ce3C,
	otm1902c_ce4C,
	otm1902c_ce5C,
	otm1902c_ce6C,
	otm1902c_ce7C,
	otm1902c_ce8C,
	otm1902c_ce9C,
};

char *otm1902c_ceD[] = {
	otm1902c_ce0D,
	otm1902c_ce1D,
	otm1902c_ce2D,
	otm1902c_ce3D,
	otm1902c_ce4D,
	otm1902c_ce5D,
	otm1902c_ce6D,
	otm1902c_ce7D,
	otm1902c_ce8D,
	otm1902c_ce9D,
};

static struct dsi_cmd_desc otm1902c_effect_ce_A[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce0A)}, otm1902c_ce0A},
};
static struct dsi_cmd_desc otm1902c_effect_ce_B[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce0B)}, otm1902c_ce0B},
};
static struct dsi_cmd_desc otm1902c_effect_ce_C[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce0C)}, otm1902c_ce0C},
};
static struct dsi_cmd_desc otm1902c_effect_ce_D[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce0D)}, otm1902c_ce0D},
};

struct lcd_effect_cmds otm1902c_effect_ce_cmd[] = {
	{{otm1902c_ceA_cmd1_head0, ARRAY_SIZE(otm1902c_ceA_cmd1_head0)}, {otm1902c_effect_ce_cmd1_head0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_head0)}},	
	{{otm1902c_ceA_head0, ARRAY_SIZE(otm1902c_ceA_head0)}, {otm1902c_effect_ce_head0, ARRAY_SIZE(otm1902c_effect_ce_head0)}},
	{{otm1902c_ceA_head1, ARRAY_SIZE(otm1902c_ceA_head1)}, {otm1902c_effect_ce_head1, ARRAY_SIZE(otm1902c_effect_ce_head1)}},
	{{otm1902c_ceA_head2, ARRAY_SIZE(otm1902c_ceA_head2)}, {otm1902c_effect_ce_head2, ARRAY_SIZE(otm1902c_effect_ce_head2)}},
	{{otm1902c_ceA_head3, ARRAY_SIZE(otm1902c_ceA_head3)}, {otm1902c_effect_ce_head3, ARRAY_SIZE(otm1902c_effect_ce_head3)}},
         {{otm1902c_ceA_head7, ARRAY_SIZE(otm1902c_ceA_head7)}, {otm1902c_effect_ce_head7, ARRAY_SIZE(otm1902c_effect_ce_head7)}},
         {{otm1902c_ceA_head8, ARRAY_SIZE(otm1902c_ceA_head8)}, {otm1902c_effect_ce_head8, ARRAY_SIZE(otm1902c_effect_ce_head8)}},
         {{otm1902c_ceA_head4, ARRAY_SIZE(otm1902c_ceA_head4)}, {otm1902c_effect_ce_head4, ARRAY_SIZE(otm1902c_effect_ce_head4)}},
         {{otm1902c_ceA_head5, ARRAY_SIZE(otm1902c_ceA_head5)}, {otm1902c_effect_ce_head5, ARRAY_SIZE(otm1902c_effect_ce_head5)}},
	{{otm1902c_ceA_head6, ARRAY_SIZE(otm1902c_ceA_head6)}, {otm1902c_effect_ce_head6, ARRAY_SIZE(otm1902c_effect_ce_head6)}},
	{{otm1902c_ceA, ARRAY_SIZE(otm1902c_ceA)}, {otm1902c_effect_ce_A, ARRAY_SIZE(otm1902c_effect_ce_A)}},
	{{otm1902c_ceB_head, ARRAY_SIZE(otm1902c_ceB_head)}, {otm1902c_effect_ceB_head, ARRAY_SIZE(otm1902c_effect_ceB_head)}},	
	{{otm1902c_ceB, ARRAY_SIZE(otm1902c_ceB)}, {otm1902c_effect_ce_B, ARRAY_SIZE(otm1902c_effect_ce_B)}},
         {{otm1902c_ceC_head, ARRAY_SIZE(otm1902c_ceC_head)}, {otm1902c_effect_ceC_head, ARRAY_SIZE(otm1902c_effect_ceC_head)}},	
	{{otm1902c_ceC, ARRAY_SIZE(otm1902c_ceC)}, {otm1902c_effect_ce_C, ARRAY_SIZE(otm1902c_effect_ce_C)}},
         {{otm1902c_ceD_head, ARRAY_SIZE(otm1902c_ceD_head)}, {otm1902c_effect_ceD_head, ARRAY_SIZE(otm1902c_effect_ceD_head)}},	
	{{otm1902c_ceD, ARRAY_SIZE(otm1902c_ceD)}, {otm1902c_effect_ce_D, ARRAY_SIZE(otm1902c_effect_ce_D)}},
	{{otm1902c_ce0_end, ARRAY_SIZE(otm1902c_ce0_end)}, {otm1902c_effect_ce_end0, ARRAY_SIZE(otm1902c_effect_ce_end0)}},
	{{otm1902c_ce1_end, ARRAY_SIZE(otm1902c_ce1_end)}, {otm1902c_effect_ce_end1, ARRAY_SIZE(otm1902c_effect_ce_end1)}},
	{{otm1902c_ce4_end, ARRAY_SIZE(otm1902c_ce4_end)}, {otm1902c_effect_ce_end4, ARRAY_SIZE(otm1902c_effect_ce_end4)}},
	{{otm1902c_ce5_end, ARRAY_SIZE(otm1902c_ce5_end)}, {otm1902c_effect_ce_end5, ARRAY_SIZE(otm1902c_effect_ce_end5)}},
	{{otm1902c_ce2_end, ARRAY_SIZE(otm1902c_ce2_end)}, {otm1902c_effect_ce_end2, ARRAY_SIZE(otm1902c_effect_ce_end2)}},
	{{otm1902c_ce3_end, ARRAY_SIZE(otm1902c_ce3_end)}, {otm1902c_effect_ce_end3, ARRAY_SIZE(otm1902c_effect_ce_end3)}},
	{{otm1902c_ce6_end, ARRAY_SIZE(otm1902c_ce6_end)}, {otm1902c_effect_ce_end6, ARRAY_SIZE(otm1902c_effect_ce_end6)}},
	{{otm1902c_ce7_end, ARRAY_SIZE(otm1902c_ce7_end)}, {otm1902c_effect_ce_end7, ARRAY_SIZE(otm1902c_effect_ce_end7)}},
	{{otm1902c_ce8_end, ARRAY_SIZE(otm1902c_ce8_end)}, {otm1902c_effect_ce_end8, ARRAY_SIZE(otm1902c_effect_ce_end8)}},
	{{otm1902c_ce9_end, ARRAY_SIZE(otm1902c_ce9_end)}, {otm1902c_effect_ce_end9, ARRAY_SIZE(otm1902c_effect_ce_end0)}},
	{{otm1902c_ceA_cmd1_end0, ARRAY_SIZE(otm1902c_ceA_cmd1_end0)}, {otm1902c_effect_ce_cmd1_end0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_end0)}},	
};
/*********************************** cta *************************************/
static char otm1902c_cta0[]  = {0x84,0xff};
static char otm1902c_cta1[]  = {0x84,0xff};
static char otm1902c_cta2[]  = {0x84,0xf6};
static char otm1902c_cta3[]  = {0x84,0xf6};
static char otm1902c_cta4[]  = {0x84,0xed};
static char otm1902c_cta5[]  = {0x84,0xed};
static char otm1902c_cta6[]  = {0x84,0xe4};
static char otm1902c_cta7[]  = {0x84,0xe4};
static char otm1902c_cta8[]  = {0x84,0xdb};
static char otm1902c_cta9[]  = {0x84,0xdb};
static char otm1902c_cta10[]  = {0x84,0xd2};
static char otm1902c_cta11[]  = {0x84,0xd2};
static char otm1902c_cta12[]  = {0x84,0xc9};
static char otm1902c_cta13[]  = {0x84,0xc9};
static char otm1902c_cta14[]  = {0x84,0x80};
static char otm1902c_cta15[]  = {0x84,0x80};
static char otm1902c_cta16[]  = {0x84,0x80};
static char otm1902c_cta17[]  = {0x84,0x88};
static char otm1902c_cta18[]  = {0x84,0x88};
static char otm1902c_cta19[]  = {0x84,0x90};
static char otm1902c_cta20[]  = {0x84,0x90};
static char otm1902c_cta21[] = {0x84,0x98};
static char otm1902c_cta22[] = {0x84,0x98};
static char otm1902c_cta23[] = {0x84,0xa0};
static char otm1902c_cta24[] = {0x84,0xa0};
static char otm1902c_cta25[] = {0x84,0xa8};
static char otm1902c_cta26[] = {0x84,0xa8};
static char otm1902c_cta27[] = {0x84,0xb0};
static char otm1902c_cta28[] = {0x84,0xb0};
static char otm1902c_cta29[] = {0x84,0xb8};
static char otm1902c_cta30[] = {0x84,0xb8};
static char otm1902c_cta31[] = {0x84,0xc0};
static char otm1902c_cta32[] = {0x84,0xc0};

char *otm1902c_cta[] = {
	otm1902c_cta0,
	otm1902c_cta1,
	otm1902c_cta2,
	otm1902c_cta3,
	otm1902c_cta4,
	otm1902c_cta5,
	otm1902c_cta6,
	otm1902c_cta7,
	otm1902c_cta8,
	otm1902c_cta9,
	otm1902c_cta10,
	otm1902c_cta11,
	otm1902c_cta12,
	otm1902c_cta13,
	otm1902c_cta14,
	otm1902c_cta15,
	otm1902c_cta16,
	otm1902c_cta17,
	otm1902c_cta18,
	otm1902c_cta19,
	otm1902c_cta20,
	otm1902c_cta21,
	otm1902c_cta22,
	otm1902c_cta23,
	otm1902c_cta24,
	otm1902c_cta25,
	otm1902c_cta26,
	otm1902c_cta27,
	otm1902c_cta28,
	otm1902c_cta29,
	otm1902c_cta30,
	otm1902c_cta31,
	otm1902c_cta32,
};

static struct dsi_cmd_desc otm1902c_effect_cta[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_cta0)}, otm1902c_cta0},
};
struct lcd_effect_cmds otm1902c_effect_cta_cmd[] = {
	{{otm1902c_ceA_cmd1_head0, ARRAY_SIZE(otm1902c_ceA_cmd1_head0)}, {otm1902c_effect_ce_cmd1_head0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_head0)}},	
	{{otm1902c_cta, ARRAY_SIZE(otm1902c_cta)}, {otm1902c_effect_cta, ARRAY_SIZE(otm1902c_effect_cta)}},
	{{otm1902c_ceA_cmd1_end0, ARRAY_SIZE(otm1902c_ceA_cmd1_end0)}, {otm1902c_effect_ce_cmd1_end0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_end0)}},	
};
/************************************ cabc ***********************************/
static char otm1902c_cabc0[] = {0x55, 0x00};
static char otm1902c_cabc1[] = {0x55, 0x01};
static char otm1902c_cabc2[] = {0x55, 0x02};
static char otm1902c_cabc3[] = {0x55, 0x03};
static char otm1902c_cabc4[] = {0x55, 0x03};

char *otm1902c_cabc[] = {
	otm1902c_cabc0,
	otm1902c_cabc1,
	otm1902c_cabc2,
	otm1902c_cabc3,
	otm1902c_cabc4,
};

//static char normal_otm1902c_mode_ce_off0[] = {0x00,0x83};
//static char normal_otm1902c_mode_ce_off1[] = {0xD6,0x02};
static char normal_otm1902c_mode_ce_off2[] = {0x00,0x00};
static char normal_otm1902c_mode_ce_off3[] = {0xD4,0xff,0x33,0xff,0x34,0xff,0x35,0xff,0x36,0xff,0x37,0xff,0x38,0xff,0x38,0xff,0x39,0xff,0x3a,0xff,0x3b,0xff,0x3c,0xff,0x3d,0xff,0x3d,0xff,0x3e,0xff,0x3f,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,
0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0xff,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,
0x40,0x00,0x40,0x00,0x40,0x01,0x40,0x01,0x40,0x01,0x40,0x01,0x40,0x02,0x40,0x03,0x40,0x04,0x40,0x05,0x40,0x06,0x40,0x07,0x40,0x08,0x40,0x08,0x3f,0x09,0x3f,0x0b,0x3f,0x0b,0x3f,0x0c,0x3f,0x0d,0x3f,0x0e,
0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0f,0x3e,0x0e,0x3e,0x0d,0x3f,0x0c,0x3f,0x0b,
0x3f,0x0a,0x3f,0x09,0x3f,0x08,0x3f,0x07,0x40,0x07,0x40,0x06,0x40,0x04,0x40,0x04,0x40,0x03,0x40,0x02,0x40,0x01,0x40,0x01,0x40,0x00,0x40,0xff,0x40,0xfe,0x40,0xfe,0x40,0xfd,0x40,0xfd,0x40,0xfc,0x40,0xfb,
0x40,0xfa,0x40,0xfa,0x40,0xf9,0x40,0xf8,0x40,0xf8,0x3f,0xf7,0x3f,0xf7,0x3f,0xf7,0x3f,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf8,0x40,0xf9,0x40,0xf9,0x40,0xf9,0x40,0xf9,
0x40,0xf9,0x40,0xfa,0x40,0xfa,0x40,0xfc,0x40,0xfc,0x40,0xfd,0x40,0xfd,0x40,0xfe,0x40,0xfe,0x40,0xff,0x40,0xff,0x40,0x00,0x40,0x01,0x40,0x01,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,
0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x3f,0x02,0x3e,0x02,0x3e,0x01,0x3d,0x01,0x3c,0x01,0x3c,0x01,0x3b,0x01,0x3a,0x00,
0x3a,0x00,0x39,0x00,0x38,0x00,0x37,0x00,0x37,0xff,0x36,0xff,0x36,0xff,0x35,0xff,0x35,0xff,0x35,0xff,0x35,0xff,0x35,0xff,0x35,0xff,0x34,0xff,0x34,0xff,0x34,0xff,0x34,0xff,0x34,0xff,0x34,0xff,0x34,0xff,
0x33,0x00,0x34,0x00,0x34,0x00,0x34,0xff,0x35,0xff,0x35,0xff,0x35,0xfe,0x35,0xfe,0x35,0xfd,0x35,0xfd,0x35,0xfd,0x35,0xfc,0x35,0xfc,0x36,0xfb,0x36,0xfb,0x36,0xfb,0x36,0xfb,0x37,0xfb,0x39,0xfb,0x3a,0xfb,
0x3b,0xfb,0x3d,0xfb,0x3e,0xfc,0x3f,0xfb,0x41,0xfb,0x42,0xfb,0x43,0xfb,0x44,0xfb,0x46,0xfc,0x47,0xfc,0x48,0xfc,0x49,0xfc,0x4a,0xfc,0x4b,0xfc,0x4c,0xfc,0x4d,0xfd,0x4d,0xfd,0x4e,0xfd,0x4f,0xfd,0x50,0xfd,
0x51,0xfd,0x52,0xfd,0x52,0xff,0x53,0xff,0x54,0xff,0x55,0xfe,0x55,0xff,0x54,0x00,0x53,0x00,0x53,0x01,0x52,0x01,0x51,0x03,0x50,0x03,0x4f,0x03,0x4e,0x04,0x4e,0x04,0x4d,0x05,0x4c,0x05,0x4b,0x06,0x4a,0x06,
0x49,0x07,0x48,0x06,0x48,0x06,0x48,0x08,0x48,0x08,0x47,0x07,0x47,0x07,0x47,0x07,0x47,0x07,0x46,0x07,0x46,0x07,0x46,0x07,0x46,0x07,0x45,0x07,0x45,0x07,0x45,0x07,0x45,0x07,0x45,0x06,0x45,0x06,0x45,0x05,
0x45,0x05,0x44,0x05,0x44,0x04,0x44,0x04,0x44,0x04,0x44,0x02,0x44,0x02,0x44,0x02,0x44,0x01,0x44,0x01,0x44,0x00,0x44,0x00,0x46,0x00,0x48,0xff,0x4a,0xff,0x4c,0xff,0x4e,0xff,0x50,0xfd,0x52,0xfd,0x54,0xfd,
0x56,0xfc,0x58,0xfb,0x5a,0xfb,0x5c,0xfa,0x5e,0xf9,0x60,0xf9,0x62,0xf9,0x61,0xf9,0x61,0xfa,0x60,0xfb,0x60,0xfb,0x5f,0xfb,0x5e,0xfb,0x5e,0xfb,0x5d,0xfc,0x5d,0xfd,0x5c,0xfd,0x5c,0xfd,0x5b,0xfd,0x5b,0xfd,
0x5a,0xfe,0x59,0xfe,0x59,0xfe,0x58,0xfe,0x58,0xfe,0x57,0xfe,0x57,0xfe,0x56,0xff,0x55,0xff,0x55,0xff,0x54,0xff,0x54,0x00,0x53,0x00,0x53,0x00,0x52,0x00,0x51,0x00,0x51,0x00,0x51,0x01,0x50,0x01,0x50,0x01,
0x50,0x01,0x4f,0x01,0x4f,0x03,0x4e,0x03,0x4e,0x03,0x4e,0x03,0x4d,0x04,0x4d,0x04,0x4c,0x04,0x4c,0x04,0x4c,0x04,0x4b,0x04,0x4a,0x04,0x49,0x04,0x48,0x04,0x46,0x04,0x45,0x04,0x44,0x03,0x43,0x03,0x41,0x03,
0x40,0x03,0x3f,0x03,0x3e,0x03,0x3d,0x03,0x3b,0x03,0x3a,0x02,0x39,0x02,0x39,0x02,0x38,0x02,0x38,0x02,0x38,0x02,0x37,0x02,0x37,0x02,0x37,0x01,0x36,0x01,0x36,0x01,0x36,0x01,0x35,0x01,0x35,0x01,0x35,0x01,
0x34};
static char normal_otm1902c_mode_ce_off4[] = {0x00,0x00};
static char normal_otm1902c_mode_ce_off5[] = {0xD5,0x00,0x3b,0xff,0x3b,0xff,0x3c,0xff,0x3c,0xff,0x3c,0xff,0x3c,0xff,0x3d,0xfe,0x3d,0xfe,0x3d,0xfe,0x3d,0xfe,0x3e,0xfe,0x3e,0xfe,0x3e,0xfd,0x3f,0xfd,0x3f,0xfd,0x3f,0xfd,0x40,0xfd,0x40,0xfd,0x40,0xfd,0x41,
0xfd,0x41,0xfd,0x41,0xfd,0x42,0xfd,0x42,0xfd,0x42,0xfc,0x43,0xfc,0x43,0xfd,0x44,0xfe,0x44,0xfe,0x44,0xfd,0x45,0xfe,0x45,0xfe,0x45,0xff,0x46,0xff,0x46,0xff,0x46,0xff,0x46,0x00,0x47,0x00,0x47,0x01,0x47,
0x01,0x47,0x01,0x48,0x02,0x48,0x03,0x48,0x03,0x48,0x03,0x48,0x04,0x48,0x05,0x47,0x06,0x46,0x07,0x46,0x07,0x45,0x08,0x44,0x09,0x44,0x09,0x43,0x0b,0x42,0x0b,0x42,0x0c,0x41,0x0c,0x40,0x0d,0x3f,0x0e,0x3f,
0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0e,0x3e,0x0d,0x3e,0x0d,0x3e,0x0d,0x3e,0x0d,0x3e,0x0d,0x3e,0x0d,0x3e,0x0c,0x3f,0x0c,0x3f,0x0c,0x3f,0x0c,0x3f,0x0c,0x3f,0x0b,0x3f,0x0b,0x3f,0x0a,0x3f,0x09,0x3f,0x09,0x3f,
0x08,0x3f,0x07,0x3f,0x07,0x40,0x06,0x40,0x05,0x40,0x04,0x40,0x03,0x40,0x03,0x40,0x02,0x40,0x01,0x40,0x00,0x40,0x00,0x41,0x00,0x41,0xff,0x42,0xff,0x42,0xfe,0x43,0xfe,0x44,0xfc,0x44,0xfc,0x45,0xfb,0x45,
0xfb,0x46,0xfa,0x46,0xfa,0x47,0xf9,0x47,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,0xf8,0x48,
0xf8,0x48,0xf9,0x48,0xfa,0x48,0xfa,0x48,0xfb,0x48,0xfb,0x48,0xfc,0x48,0xfc,0x48,0xfc,0x48,0xfd,0x48,0xfe,0x47,0xfe,0x47,0xff,0x47,0xff,0x47,0xff,0x47,0x00,0x47,0x00,0x47,0x00,0x46,0x00,0x46,0x01,0x46,
0x01,0x46,0x01,0x46,0x01,0x45,0x01,0x45,0x01,0x45,0x02,0x45,0x02,0x44,0x02,0x44,0x02,0x44,0x02,0x44,0x03,0x43,0x02,0x43,0x02,0x42,0x02,0x42,0x02,0x41,0x02,0x41,0x02,0x40,0x02,0x40,0x02,0x3f,0x02,0x3f,
0x01,0x3e,0x01,0x3e,0x01,0x3d,0x01,0x3d,0x01,0x3c,0x01,0x3c,0x01,0x3c,0x01,0x3c,0x01,0x3c,0x01,0x3c,0x01,0x3c,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,0x00,0x3b,
0x00,0x40,0xff,0x40,0xff,0x40,0xff,0x41,0xfe,0x41,0xfe,0x41,0xfe,0x41,0xfd,0x41,0xfd,0x41,0xfc,0x42,0xfb,0x42,0xfb,0x42,0xfa,0x42,0xfa,0x42,0xfa,0x42,0xf9,0x42,0xfa,0x43,0xfa,0x44,0xfa,0x45,0xfa,0x46,
0xfa,0x46,0xfa,0x47,0xfa,0x48,0xfb,0x49,0xfb,0x4a,0xfb,0x4a,0xfb,0x4b,0xfb,0x4c,0xfb,0x4d,0xfc,0x4e,0xfc,0x4e,0xfc,0x4f,0xfc,0x50,0xfc,0x51,0xfc,0x52,0xfc,0x52,0xfc,0x53,0xfc,0x54,0xfc,0x55,0xfc,0x56,
0xfb,0x56,0xfb,0x57,0xfb,0x58,0xfb,0x59,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfd,0x5a,0xfd,0x5a,0xfe,0x5a,0xfe,0x5a,0xfe,0x5a,0x00,0x5a,0x00,0x5a,0x01,0x59,0x02,0x59,0x02,0x59,0x03,0x59,0x03,0x59,0x04,0x59,
0x04,0x59,0x05,0x59,0x05,0x58,0x05,0x58,0x05,0x58,0x05,0x58,0x05,0x57,0x05,0x57,0x05,0x57,0x05,0x57,0x05,0x56,0x05,0x56,0x04,0x56,0x04,0x56,0x04,0x55,0x05,0x55,0x04,0x55,0x04,0x55,0x04,0x55,0x03,0x56,
0x03,0x56,0x03,0x56,0x03,0x56,0x01,0x56,0x02,0x56,0x02,0x56,0x02,0x56,0x02,0x56,0x00,0x56,0x00,0x56,0x00,0x57,0x00,0x57,0xff,0x57,0xfe,0x57,0xfe,0x58,0xfe,0x58,0xfe,0x58,0xfd,0x58,0xfd,0x58,0xfd,0x59,
0xfd,0x59,0xfb,0x59,0xfb,0x59,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfc,0x5a,0xfd,0x5a,0xfd,0x5a,0xfd,0x5a,0xfe,0x5b,0xfe,0x5b,0xfe,0x5b,0xfe,0x5b,0xfe,0x5b,0x00,0x5b,0x00,0x5b,
0x00,0x5b,0xff,0x5b,0xfe,0x5b,0xfe,0x5b,0xfe,0x5a,0xfd,0x5a,0xfd,0x5a,0xfc,0x5a,0xfb,0x5a,0xfb,0x5a,0xfb,0x5a,0xfa,0x59,0xfa,0x59,0xf9,0x59,0xf8,0x59,0xf8,0x59,0xfa,0x59,0xfb,0x58,0xfb,0x58,0xfd,0x58,
0xfe,0x57,0xfe,0x57,0x00,0x57,0x01,0x56,0x02,0x56,0x02,0x56,0x03,0x55,0x04,0x55,0x05,0x54,0x06,0x54,0x07,0x53,0x07,0x52,0x06,0x51,0x06,0x50,0x05,0x4e,0x05,0x4d,0x05,0x4c,0x05,0x4a,0x05,0x49,0x05,0x48,
0x05,0x46,0x05,0x45,0x04,0x44,0x03,0x43,0x03,0x41,0x04,0x40,0x03,0x40,0x03,0x40,0x03,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x01,0x40,0x01,0x40,0x01,0x40,0x01,0x40,0x00,0x40,0x00,0x40};
static char normal_otm1902c_mode_ce_off6[] = {0x81,0x81};

static struct dsi_cmd_desc otm1902c_effect_cabc[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(otm1902c_cabc0)}, otm1902c_cabc0},
};
struct lcd_effect_cmds otm1902c_effect_cabc_cmd[] = {
	{{otm1902c_ceA_cmd1_head0, ARRAY_SIZE(otm1902c_ceA_cmd1_head0)}, {otm1902c_effect_ce_cmd1_head0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_head0)}},		
	{{otm1902c_cabc, ARRAY_SIZE(otm1902c_cabc)}, {otm1902c_effect_cabc, ARRAY_SIZE(otm1902c_effect_cabc)}},
	{{otm1902c_ceA_cmd1_end0, ARRAY_SIZE(otm1902c_ceA_cmd1_end0)}, {otm1902c_effect_ce_cmd1_end0, ARRAY_SIZE(otm1902c_effect_ce_cmd1_end0)}},	
};

/************************************** comfort mode **************************************/
static char comfort_otm1902c_mode_cmd0[]=  {0x00,0x00};
static char comfort_otm1902c_mode_cmd1[] = {0xE1,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5c};
static char comfort_otm1902c_mode_cmd2[] = {0x00,0x00};
static char comfort_otm1902c_mode_cmd3[] = {0xE2,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5C};
static char comfort_otm1902c_mode_cmd4[]=  {0x00,0x00};
static char comfort_otm1902c_mode_cmd5[] = {0xE3,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5C};
static char comfort_otm1902c_mode_cmd6[] = {0x00,0x00};
static char comfort_otm1902c_mode_cmd7[] = {0xE4,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5C};
static char comfort_otm1902c_mode_cmd8[]=  {0x00,0x00};
static char comfort_otm1902c_mode_cmd9[] = {0xE5,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5C};
static char comfort_otm1902c_mode_cmd10[] = {0x00,0x00};
static char comfort_otm1902c_mode_cmd11[] = {0xE6,0x80,0x81,0x82,0x82,0x83,0x84,0x86,0x8a,0x88,0x8a,0x88,0x88,0x77,0x74,0x71,0x6b,0x63,0x5f,0x5e,0x5e,0x5d,0x5d,0x5d,0x5C};
static struct dsi_cmd_desc comfort_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_cabc3)}, otm1902c_cabc3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1)}, comfort_otm1902c_mode_cmd1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3)}, comfort_otm1902c_mode_cmd3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5)}, comfort_otm1902c_mode_cmd5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7)}, comfort_otm1902c_mode_cmd7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9)}, comfort_otm1902c_mode_cmd9},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11)}, comfort_otm1902c_mode_cmd11},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_cta0)}, otm1902c_cta0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off0)}, normal_otm1902c_mode_ce_off0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off1)}, normal_otm1902c_mode_ce_off1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off2)}, normal_otm1902c_mode_ce_off2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off3)}, normal_otm1902c_mode_ce_off3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off4)}, normal_otm1902c_mode_ce_off4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off5)}, normal_otm1902c_mode_ce_off5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off6)}, normal_otm1902c_mode_ce_off6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},	
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
};
static char comfort_otm1902c_mode_cmd1_off[] = {0xE1,0x5e,0x5f,0x60,0x62,0x64,0x66,0x6a,0x71,0x73,0x7a,0x7d,0x7f,0x7d,0x79,0x72,0x62,0x50,0x3c,0x30,0x28,0x1e,0x0f,0x03,0x00};
static char comfort_otm1902c_mode_cmd3_off[] = {0xE2,0x5e,0x5f,0x60,0x62,0x64,0x66,0x6a,0x71,0x73,0x7a,0x7d,0x7f,0x7d,0x79,0x72,0x62,0x50,0x3c,0x30,0x28,0x1e,0x0f,0x03,0x00};
static char comfort_otm1902c_mode_cmd5_off[] = {0xE3,0x34,0x37,0x3a,0x3f,0x43,0x47,0x4f,0x5b,0x61,0x6d,0x74,0x79,0x82,0x7e,0x76,0x66,0x52,0x3f,0x32,0x2a,0x20,0x12,0x04,0x00};
static char comfort_otm1902c_mode_cmd7_off[] = {0xE4,0x34,0x37,0x3a,0x3f,0x43,0x47,0x4f,0x5b,0x61,0x6d,0x74,0x79,0x82,0x7e,0x76,0x66,0x52,0x3f,0x32,0x2a,0x20,0x12,0x04,0x00};
static char comfort_otm1902c_mode_cmd9_off[] = {0xE5,0x00,0x0a,0x10,0x1a,0x22,0x2a,0x36,0x4a,0x55,0x66,0x70,0x76,0x84,0x80,0x78,0x68,0x54,0x41,0x33,0x2b,0x21,0x14,0x04,0x00};
static char comfort_otm1902c_mode_cmd11_off[] = {0xE6,0x00,0x0a,0x10,0x1a,0x22,0x2a,0x36,0x4a,0x55,0x66,0x70,0x76,0x84,0x80,0x78,0x68,0x54,0x41,0x33,0x2b,0x21,0x14,0x04,0x00};
//static char otm1902c_cta0_off[]  = {0x84,0x00};
static char outside_otm1902c_mode_cmd1_off[] = {0x82,0x00};

static struct dsi_cmd_desc otm1902c_packet_head_cmds[] = {
};

/************************************** picture mode **************************************/
static char picture_otm1902c_mode_cmd0[] = {0x55,0x00};
static char picture_otm1902c_mode_cta_nal[] = {0x84,0x80};

//static char picture_otm1902c_mode_cmd2[]=  {0xCA,0x01,0x80,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0x0B,0x60,0xCE,0x80,0xD1,0x42,0x44,0x80,0x69,0xDA,0x08,0x08,0x08,0x06,0x08,0x04,0x00,0x00,0x10,0x10,0x3F,0x3F,0x3F,0x3F};

//static char picture_otm1902c_mode_cmd3[] = {0xc8,0x01,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0xfc,0x00};
static struct dsi_cmd_desc picture_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cmd0)}, picture_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1_off)}, outside_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off0)}, normal_otm1902c_mode_ce_off0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off1)}, normal_otm1902c_mode_ce_off1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off2)}, normal_otm1902c_mode_ce_off2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off3)}, normal_otm1902c_mode_ce_off3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off4)}, normal_otm1902c_mode_ce_off4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off5)}, normal_otm1902c_mode_ce_off5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off6)}, normal_otm1902c_mode_ce_off6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
//	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cta_off)}, picture_otm1902c_mode_cta_off},
//	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cmd3)}, picture_otm1902c_mode_cmd3},
};

static struct dsi_cmd_desc camera_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cmd0)}, picture_otm1902c_mode_cmd0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off0)}, normal_otm1902c_mode_ce_off0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off1)}, normal_otm1902c_mode_ce_off1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off2)}, normal_otm1902c_mode_ce_off2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off3)}, normal_otm1902c_mode_ce_off3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off4)}, normal_otm1902c_mode_ce_off4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off5)}, normal_otm1902c_mode_ce_off5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off6)}, normal_otm1902c_mode_ce_off6},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1_off)}, outside_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
//	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cta_off)}, picture_otm1902c_mode_cta_off},
//	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cmd3)}, picture_otm1902c_mode_cmd3},
};

/************************************** outside mode **************************************/
static char outside_otm1902c_mode_cmd0[] = {0x55,0x03};
static char outside_otm1902c_mode_cmd1[] = {0x82,0x82};
//static char outside_otm1902c_mode_cmd1[] = {0xc8,0x01,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0xfc,0x00};
/*
static char outside_otm1902c_mode_cmd2[] = {0xca,0x01,0xC8,0x80,0x80,0x80,0x80,0x80,0x80,0x12,0x29,0xAC,0x80,0x00,0x4A,0x37,0x80,0x55,0xF8,0x08,0x08,0x08,0x08,0x10,0x10,0x00,0x00,0x10,0x10,0x10,0x10,0x10,0x10};
*/
//static char outside_otm1902c_mode_cmd3[] = {0xba,0x05,0x3c,0xff,0x00,0x00,0x00};
//static char outside_otm1902c_mode_cmd4[] = {0xbd,0x01,0x1e,0x14};
static struct dsi_cmd_desc outside_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd0)}, outside_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1)}, outside_otm1902c_mode_cmd1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},

	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1)}, outside_otm1902c_mode_cmd1},
	/*
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd2)}, outside_otm1902c_mode_cmd2},
	*/
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd3)}, outside_otm1902c_mode_cmd3},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd4)}, outside_otm1902c_mode_cmd4},
};

/************************************** custom mode **************************************/
static char custom_otm1902c_mode_cmd0[] = {0x55,0x03};
//static char custom_otm1902c_mode_cmd1[] = {0xba,0x07,0x75,0x61,0x20,0x16,0x87};

static struct dsi_cmd_desc custom_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(custom_otm1902c_mode_cmd0)}, custom_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1_off)}, outside_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(custom_otm1902c_mode_cmd1)}, custom_otm1902c_mode_cmd1},
};
/************************************** normal mode **************************************/
static char normal_otm1902c_mode_cmd0[] = {0x55,0x03};
//static char normal_otm1902c_mode_cmd1[] = {0xba,0x07,0x75,0x61,0x20,0x16,0x87};
//static char normal_otm1902c_mode_cmd2[]=  {0xCA,0x01,0x80,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0x0B,0x60,0xCE,0x80,0xD1,0x42,0x44,0x80,0x69,0xDA,0x08,0x08,0x08,0x06,0x08,0x04,0x00,0x00,0x10,0x10,0x3F,0x3F,0x3F,0x3F};
//static char normal_otm1902c_mode_cmd3[] = {0xC8,0x01,0x00,0x00,0x00,0x00,0xD5,0x00,0x00,0x00,0x00,0x00,0xEA,0x00,0x00,0x00,0x00,0x00,0xFC,0x00};

static struct dsi_cmd_desc normal_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(normal_otm1902c_mode_cmd0)}, normal_otm1902c_mode_cmd0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off0)}, normal_otm1902c_mode_ce_off0},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off1)}, normal_otm1902c_mode_ce_off1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off2)}, normal_otm1902c_mode_ce_off2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off3)}, normal_otm1902c_mode_ce_off3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off4)}, normal_otm1902c_mode_ce_off4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off5)}, normal_otm1902c_mode_ce_off5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_ce_off6)}, normal_otm1902c_mode_ce_off6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(picture_otm1902c_mode_cta_nal)}, picture_otm1902c_mode_cta_nal},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1_off)}, outside_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_cmd1)}, normal_otm1902c_mode_cmd1},
	//{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(normal_otm1902c_mode_cmd0)}, normal_otm1902c_mode_cmd0},
};
/************************************** ultra bright mode **************************************/
static char ultra_otm1902c_mode_cmd0[] = {0x55,0x00};
//static char ultra_otm1902c_mode_cmd1[] = {0xb8,0x00,0x00,0x00,0x00,0x00,0x00};
//static char ultra_otm1902c_mode_cmd2[] = {0xC8,0x01,0x00,0x00,0x00,0x00,0xD5,0x00,0x00,0x00,0x00,0x00,0xEA,0x00,0x00,0x00,0x00,0x00,0xFC,0x00};

static struct dsi_cmd_desc ultra_otm1902c_mode_cmds[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_head)}, led_cmd1_head},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head0)}, otm1902c_ce_head0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head1)}, otm1902c_ce_head1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head2)}, otm1902c_ce_head2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head3)}, otm1902c_ce_head3},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head4)}, otm1902c_ce_head4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_head5)}, otm1902c_ce_head5},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ultra_otm1902c_mode_cmd0)}, ultra_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(picture_otm1902c_mode_cta_nal)}, picture_otm1902c_mode_cta_nal},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(outside_otm1902c_mode_cmd1_off)}, outside_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd0)}, comfort_otm1902c_mode_cmd0},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd1_off)}, comfort_otm1902c_mode_cmd1_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd2)}, comfort_otm1902c_mode_cmd2},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd3_off)}, comfort_otm1902c_mode_cmd3_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd4)}, comfort_otm1902c_mode_cmd4},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd5_off)}, comfort_otm1902c_mode_cmd5_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd6)}, comfort_otm1902c_mode_cmd6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd7_off)}, comfort_otm1902c_mode_cmd7_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd8)}, comfort_otm1902c_mode_cmd8},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd9_off)}, comfort_otm1902c_mode_cmd9_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd10)}, comfort_otm1902c_mode_cmd10},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(comfort_otm1902c_mode_cmd11_off)}, comfort_otm1902c_mode_cmd11_off},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end4)}, otm1902c_ce_end4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 2, sizeof(otm1902c_ce_end5)}, otm1902c_ce_end5},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end6)}, otm1902c_ce_end6},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end7)}, otm1902c_ce_end7},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(otm1902c_ce_end8)}, otm1902c_ce_end8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(otm1902c_ce_end9)}, otm1902c_ce_end9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(led_cmd1_end)}, led_cmd1_end},

	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ultra_otm1902c_mode_cmd1)}, ultra_otm1902c_mode_cmd1},
	//{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ultra_otm1902c_mode_cmd2)}, ultra_otm1902c_mode_cmd2},
	/*
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ultra_otm1902c_mode_cmd3)}, ultra_otm1902c_mode_cmd3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ultra_otm1902c_mode_cmd1)}, ultra_otm1902c_mode_cmd1},
	*/
};

/*********************************** all effect ************************************/

struct lcd_effect otm1902c_effect[] = {
	{"cabc", ARRAY_SIZE(otm1902c_cabc), 0, {otm1902c_effect_cabc_cmd, ARRAY_SIZE(otm1902c_effect_cabc_cmd)}},
	{"ce", ARRAY_SIZE(otm1902c_ceA), 0, {otm1902c_effect_ce_cmd, ARRAY_SIZE(otm1902c_effect_ce_cmd)}},
	{"cta", ARRAY_SIZE(otm1902c_cta), 16, {otm1902c_effect_cta_cmd, ARRAY_SIZE(otm1902c_effect_cta_cmd)}},
//	{"aco", ARRAY_SIZE(otm1902c_aco), 0, {otm1902c_effect_aco_cmd, ARRAY_SIZE(otm1902c_effect_aco_cmd)}},
	//{"gamma", ARRAY_SIZE(otm1902c_sre), 0, {otm1902c_effect_sre_cmd, ARRAY_SIZE(otm1902c_effect_sre_cmd)}},
};
/**************************************************************************************/

/************************************** all mode **************************************/
struct lcd_mode otm1902c_mode[] = {
	{"custom_mode", {3, 4, 16}, {0, 4, 16},0, 0, {custom_otm1902c_mode_cmds, ARRAY_SIZE(custom_otm1902c_mode_cmds)}},
	{"auto_mode", {0, 0, 0}, {0, 4, 16},0, 0, {normal_otm1902c_mode_cmds, ARRAY_SIZE(normal_otm1902c_mode_cmds)}},
	{"normal_mode", {3, 0, 0}, {0, 4, 16},0, 0 ,{normal_otm1902c_mode_cmds, ARRAY_SIZE(normal_otm1902c_mode_cmds)}},
	{"comfort_mode", {3, 0, 0}, {0, 4, 16},0, 0, {comfort_otm1902c_mode_cmds, ARRAY_SIZE(comfort_otm1902c_mode_cmds)}},
	{"outside_mode", {4, 0, 0}, {0, 4, 16},0, 1, {outside_otm1902c_mode_cmds, ARRAY_SIZE(outside_otm1902c_mode_cmds)}},
	{"ultra_mode", {0, 0, 0}, {0, 4, 16},1, 1, {ultra_otm1902c_mode_cmds, ARRAY_SIZE(ultra_otm1902c_mode_cmds)}},
	{"camera_mode", {0, 0, 0}, {0, 4, 16},0, 0, {camera_otm1902c_mode_cmds, ARRAY_SIZE(camera_otm1902c_mode_cmds)}},
	{"picture_mode", {0, 0, 0},{0, 4, 16}, 0, 1, {picture_otm1902c_mode_cmds, ARRAY_SIZE(picture_otm1902c_mode_cmds)}},
};
/**************************************************************************************/
struct lcd_cmds otm1902c_head_cmds = 
	{otm1902c_packet_head_cmds, ARRAY_SIZE(otm1902c_packet_head_cmds)};

struct lcd_effect_data otm1902c_effect_data = 
	{otm1902c_effect, &otm1902c_head_cmds, ARRAY_SIZE(otm1902c_effect)};

struct lcd_mode_data otm1902c_mode_data = 
	{otm1902c_mode, &otm1902c_head_cmds, ARRAY_SIZE(otm1902c_mode), 0};

/**************************************************************************************/


#endif
