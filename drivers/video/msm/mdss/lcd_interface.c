#include "lcd_interface.h"

#define TAG "[LCD_EFFECT: ]"
//#define LCDDEBUG
#ifdef LCDDEBUG
#define lcd_effect_info(fmt, ...) printk(TAG fmt, ##__VA_ARGS__);
#else
#define lcd_effect_info(fmt, ...) do {} while (0)
#endif

#define READ_LCD_PARAM
static DEFINE_MUTEX(lcd_effect_mtx);
int current_brightness = 0xA3;
int last_mode = 2;
extern struct i2c_client *dp_client;
static int is_show_lcd_param = 0;
static int read_dsi = 0;
static int current_bl;
extern int LCD_ID;
extern void max77819_update_brightness(int brightness);

static int show_lcd_param(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	int i, j;

	printk("======================================= cmds_cnt %d =========================================\n", cmd_cnt);
	for (i = 0; i < cmd_cnt; i++) {
		printk("%2x %2x %2x %2x %2x %2x ", cmds[i].dchdr.dtype, 
				cmds[i].dchdr.last, 
				cmds[i].dchdr.vc, 
				cmds[i].dchdr.ack, 
				cmds[i].dchdr.wait, 
				cmds[i].dchdr.dlen);
		for (j = 0; j < cmds[i].dchdr.dlen; j++) {
			printk("%2x ", cmds[i].payload[j]);
		}	
		printk("\n");
	}
	pr_debug("===========================================================================================\n");
	return 0;
}

#ifdef READ_LCD_PARAM
static int read_lcd_file = 0;
struct lcd_cmds lcd_txt_cmds;

#define BUFSIZE 3
#define LENGHT 256
#define LENGHT_POS	0
#define TYPE_POS	1
#define DELAY_POS	2
#define DATA_POS	3
#define FILEPATH	"/data/lcd.txt"
static char data_buf[LENGHT] = {0};
static char *data_pos[LENGHT];

static int get_data(char *buf)
{
	char a, b;

	if (buf[0] >= 'A' && buf[0] <= 'F') {
		a = buf[0] - 55;
	} else if (buf[0] >= 'a' && buf[0] <= 'f') {
		a = buf[0] - 87;
	} else {
		a = buf[0] - 48;
	}
	if (buf[1] >= 'A' && buf[1] <= 'F') {
		b = buf[1] - 55;
	} else if (buf[1] >= 'a' && buf[1] <= 'f') {
		b = buf[1] - 87;
	} else {
		b = buf[1] - 48;
	}
	return a * 16 + b;
}
static void clear_data_and_mem(void)
{
	int i;

	if (lcd_txt_cmds.cnt) {
		pr_info("Free mem for lcd txt file's cmds buf.\n");
		for (i = 0; i < lcd_txt_cmds.cnt; i++) {
			if (data_pos[i])
				kfree(data_pos[i]);
		}
		if (lcd_txt_cmds.cmd)
			kfree(lcd_txt_cmds.cmd);
		lcd_txt_cmds.cnt = 0;
	}
}
static int open_lcd_and_get_data(void)
{
	struct file *fp;
	int ret, lenght = 0;
	int i = 0, j = 0;
	char buf[BUFSIZE] = {0};
	char *filepath = FILEPATH;
	mm_segment_t old_fs;

	lcd_txt_cmds.cnt = 0;
	//清空BUF
	for (i = 0; i < LENGHT; i++) {
		data_buf[i] = 0;
	}
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_info("open file failure\n");
		return -1;
	} else {
		pr_info("open file success, begin to read\n");
		old_fs = get_fs();
		set_fs(KERNEL_DS);	
		do {
			ret = vfs_read(fp, buf, BUFSIZE, &fp->f_pos);
			if (ret == -1) {
				pr_info("read file failure\n");
				goto read_err;
			}
			if (ret == BUFSIZE) {
				lenght++;
				//buf里留一个位置给lenght
				data_buf[lenght] = get_data(buf); //数据格式为:长度＋格式 + 延时＋数据,现在长度data_buf[0]为空

				if (buf[2] == '\n') { //标记一行结束
					if (lenght > LENGHT) { //如果一行的数据超过buf的大小
						pr_info("data too long !!!!!!!!!!!!!!!\n");
						goto large_err;
					}

					//把data_buf[0]赋值为长度
					data_buf[LENGHT_POS] = lenght + 1;
					data_pos[lcd_txt_cmds.cnt] = kmalloc(sizeof(char) * data_buf[LENGHT_POS], GFP_KERNEL);
					if (!data_pos[lcd_txt_cmds.cnt]) {
						pr_info("kmalloc failure !!!!\n");
						goto mem_err;
					}

					memcpy(data_pos[lcd_txt_cmds.cnt], data_buf, data_buf[0]); //把数据拷贝到新分配的空间

					//pr_info("buf[%d] lenght = %d\n", lcd_txt_cmds.cnt, lenght);
					lcd_txt_cmds.cnt++; //数据的行号＋1
					lenght = 0;
				} 
			}
		} while (ret != 0);
		j = 0;

		lcd_txt_cmds.cmd = kmalloc(sizeof(struct dsi_cmd_desc) * lcd_txt_cmds.cnt, GFP_KERNEL);
		if (!lcd_txt_cmds.cmd) {
			pr_info("kmalloc failure !!!!\n");
			goto cmds_err;
		}

		for (i = 0; i < lcd_txt_cmds.cnt; i++) {
			lcd_txt_cmds.cmd[i].dchdr.dtype = data_pos[i][TYPE_POS];
			lcd_txt_cmds.cmd[i].dchdr.last = 1;
			lcd_txt_cmds.cmd[i].dchdr.vc = 0;
			lcd_txt_cmds.cmd[i].dchdr.ack = 0;
			lcd_txt_cmds.cmd[i].dchdr.wait = data_pos[i][DELAY_POS];
			lcd_txt_cmds.cmd[i].dchdr.dlen = data_pos[i][LENGHT_POS] - DATA_POS;
			lcd_txt_cmds.cmd[i].payload = &data_pos[i][DATA_POS];
		}

		filp_close(fp, NULL);
	}
	return 0;
cmds_err:
large_err:
read_err:
mem_err:
	for (i = 0; i < lcd_txt_cmds.cnt; i++) {
		kfree(data_pos[i]);
	}
	lcd_txt_cmds.cnt = 0;
	return -1;
}
#endif
static struct mdss_dsi_ctrl_pdata *  mfd_to_ctrl(struct msm_fb_data_type *mfd)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	return ctrl_pdata;
}

int get_effect_index_by_name(char *name, struct panel_effect_data *lcd_data)
{
	int i;
	struct lcd_effect *effect = lcd_data->effect_data->effect;

	for (i = 0; i < lcd_data->effect_data->supported_effect; i++) {
		if (!strcmp(name, effect[i].name))
			return i;
	}
	return -EINVAL;
}
static int is_effect_hold(struct panel_effect_data *panel_data, int index)
{
	return panel_data->mode_data->mode[index].effect_hold;
}
int get_mode_index_by_name(char *name, struct panel_effect_data *lcd_data)
{
	int i;
	struct lcd_mode *mode= lcd_data->mode_data->mode;

	for (i = 0; i < lcd_data->mode_data->supported_mode; i++) {
		if (!strcmp(name, mode[i].name))
			return i;
	}
	return -EINVAL;
}
static void update_effect_cmds(struct lcd_effect *effect, int level)
{
	struct lcd_effect_cmd_data *effect_cmd_data = &effect->effect_cmd_data;
	struct lcd_effect_cmds *effect_cmd = effect_cmd_data->effect_cmd;
	int cmd_cnt = effect_cmd_data->cnt;
	int code_cnt ;
	int i;

	for (i = 0; i < cmd_cnt; i++) {
		code_cnt = effect_cmd[i].effect_code.cnt;
		effect_cmd[i].lcd_cmd.cmd->payload = effect_cmd[i].effect_code.code[level >= code_cnt ? code_cnt -1 : level];
	}
}

static void inline update_level(struct lcd_effect *effect, int level)
{
	effect->level = level;
}

static inline void update_mode(struct lcd_mode_data *mode_data, int index)
{
	mode_data->current_mode = index;
}

static inline int get_level(struct lcd_effect *effect)
{
	return effect->level;
}

static inline int get_effect_cmd_cnt(struct lcd_effect *effect)
{
	return effect->effect_cmd_data.cnt;
}

static inline int get_head_cmd_cnt(struct lcd_cmds *head_cmd)
{
	return head_cmd->cnt;
}

static inline struct dsi_cmd_desc *get_head_cmd(struct lcd_cmds *head_cmd)
{
	return head_cmd->cmd;
}
static inline struct lcd_cmds *get_lcd_cmd(struct lcd_effect_cmds *effect_cmd)
{
	return &effect_cmd->lcd_cmd;
}
static inline struct lcd_effect_cmds * get_effect_cmd(struct lcd_effect *effect)
{
	return effect->effect_cmd_data.effect_cmd;
}

static inline struct dsi_cmd_desc *get_effect_cmd_desc(struct lcd_effect_cmds *effect_cmd)
{
	return effect_cmd->lcd_cmd.cmd;
}
static inline struct dsi_cmd_desc * get_mode_cmd(struct lcd_mode *mode)
{
	return mode->mode_cmd.cmd;
}
static inline int get_mode_cmd_cnt(struct lcd_mode *mode)
{
	return mode->mode_cmd.cnt;
}

static int get_mode_max_cnt(struct lcd_mode_data *mode_data)
{
	int i;
	int temp;
	int cnt = 0;

	for (i = 0; i < mode_data->supported_mode; i++) {
		temp = mode_data->mode[i].mode_cmd.cnt;
		cnt = (cnt > temp) ? cnt : temp;
		lcd_effect_info("%s cnt = %d temp = %d\n", __func__, cnt, temp);
	}

	return cnt;
}

static int get_effect_max_cnt(struct lcd_effect_data *effect_data)
{
	int cnt = 0;
	int temp;
	int i;

	for (i = 0; i < effect_data->supported_effect; i++) {
		temp = effect_data->effect[i].effect_cmd_data.cnt;
		cnt = cnt + temp;
		lcd_effect_info("%s cnt = %d temp = %d\n", __func__, cnt, temp);
	}

	return cnt;
}

static int get_init_code_max_cnt(struct panel_effect_data *panel_data, struct lcd_cmds *save_cmd)
{
	int cnt = save_cmd->cnt;

	cnt += get_mode_max_cnt(panel_data->mode_data);
	cnt += get_effect_max_cnt(panel_data->effect_data);
	lcd_effect_info("%s cnt: %d\n", __func__, cnt);
	return cnt;
}

static int panel_is_power_on(struct msm_fb_data_type *mfd)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (mfd->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("%s: LCD panel have powered off\n", __func__);
		return false;
	}
	return true;
}

static void send_lcd_cmds(struct mdss_dsi_ctrl_pdata *ctrl, struct lcd_cmds *cmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.cmds = cmds->cmd;
	cmdreq.cmds_cnt = cmds->cnt;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static int prepare_to_send_cmds(struct msm_fb_data_type *mfd, struct lcd_cmds *cmds)
{
	if (panel_is_power_on(mfd)) {
		send_lcd_cmds(mfd_to_ctrl(mfd), cmds);
		return 0;
	} else {
		return -EPERM;
	}
}

static struct dsi_cmd_desc *copy_init_code(struct panel_effect_data *panel_data, int *cnt)
{
	int init_cnt = panel_data->save_cmd.cnt;

	memcpy(panel_data->buf, panel_data->save_cmd.cmd, (init_cnt - CMDS_LAST_CNT) * sizeof (struct dsi_cmd_desc));
	*cnt += (init_cnt - CMDS_LAST_CNT);
	return (panel_data->buf + (init_cnt - CMDS_LAST_CNT));
}

static struct dsi_cmd_desc *copy_sleep_out_code(
		struct panel_effect_data *panel_data, 
		struct dsi_cmd_desc *buf, 
		int *cnt)
{
	memcpy(buf, panel_data->save_cmd.cmd + panel_data->save_cmd.cnt - CMDS_LAST_CNT, CMDS_LAST_CNT * sizeof (struct dsi_cmd_desc));
	*cnt += CMDS_LAST_CNT;
	return (buf + CMDS_LAST_CNT);
}
static struct dsi_cmd_desc *copy_head_code(struct panel_effect_data *panel_data, struct dsi_cmd_desc *buf, int *cnt)
{
	struct lcd_cmds *head_cmds = panel_data->effect_data->head_cmd;
	int head_cnt = get_head_cmd_cnt(head_cmds); 

	memcpy(buf, head_cmds->cmd, head_cnt * sizeof (struct dsi_cmd_desc));
	*cnt +=  head_cnt;

	return (buf + head_cnt);
}

static struct dsi_cmd_desc * copy_single_effect_code(
		struct panel_effect_data *panel_data, 
		struct dsi_cmd_desc *buf, 
		int index, 
		int level,
		int *cnt)
{
	struct lcd_effect_data *effect_data = panel_data->effect_data;
	struct lcd_effect *effect = &effect_data->effect[index];
	struct dsi_cmd_desc *temp = buf;
	struct lcd_effect_cmds *effect_cmd;
	int cmd_cnt;
	int i;

	update_effect_cmds(effect, level);
	cmd_cnt = get_effect_cmd_cnt(effect);
	effect_cmd = get_effect_cmd(effect);
	*cnt += cmd_cnt;
	for (i = 0; i < cmd_cnt; i++)
		memcpy(temp++, get_effect_cmd_desc(&effect_cmd[i]), sizeof (struct dsi_cmd_desc));

	return (buf + cmd_cnt);
}

static void copy_all_effect_code(struct panel_effect_data *panel_data, struct dsi_cmd_desc *buf, int *cnt)
{
	struct dsi_cmd_desc *temp;
	struct lcd_effect_data *effect_data = panel_data->effect_data;
	struct lcd_effect *effect = effect_data->effect;
	struct lcd_effect_cmds *effect_cmd;
	int i, j;
	int cmd_cnt;

	temp = buf;
	for (i = 1; i < effect_data->supported_effect; i++) {
		update_effect_cmds(&effect[i], effect[i].level);
		cmd_cnt = get_effect_cmd_cnt(&effect[i]);
		effect_cmd = get_effect_cmd(&effect[i]);
		lcd_effect_info("%s name: [%s] level: [%d]\n", __func__, effect[i].name, effect[i].level);
		*cnt += cmd_cnt;
		for (j = 0; j < cmd_cnt; j++)
			memcpy(temp++, get_effect_cmd_desc(&effect_cmd[j]), sizeof (struct dsi_cmd_desc));
	}
}
static void copy_mode_code(
		struct panel_effect_data *panel_data, 
		struct dsi_cmd_desc *buf, 
		int mode_index, 
		int *cnt)
{
	struct lcd_mode *mode = &panel_data->mode_data->mode[mode_index];
	struct lcd_cmds *mode_cmds = &mode->mode_cmd; 
	int count = 0;
	int mode_cnt = get_mode_cmd_cnt(mode);
	if (mode_index == 0) {
		lcd_effect_info("%s: current is custom mode\n", __func__);
		copy_all_effect_code(panel_data, buf, &count);
		memcpy(buf + count, mode_cmds->cmd, mode_cnt * sizeof (struct dsi_cmd_desc));
		*cnt += (count + mode_cnt);
	} else {
		lcd_effect_info("%s: current is %s\n", __func__, mode->name);
		memcpy(buf, mode_cmds->cmd, mode_cnt * sizeof (struct dsi_cmd_desc));
		*cnt += mode_cnt;
	}
}
#if 1
static int lcd_get_bl_outdoor(struct msm_fb_data_type *mfd)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (mfd->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("%s: LCD panel have powered off\n", __func__);
		return -EPERM;
	}
	return 0;
}

static int bl_set_level(int level)
{
	//sil98611_set_led(dp_client, level);
	if(level == 1)
	{
	         if(LCD_ID == 1){
	        		max77819_update_brightness(0xCC);
			current_brightness = 0xCC;
			}
		else{
	        		max77819_update_brightness(0xEB);
			current_brightness = 0xEB;
		}
	}else{
		if(LCD_ID == 1){
	        		max77819_update_brightness(0xA3);
			current_brightness = 0xA3;
		}
		else{
	        		max77819_update_brightness(0xB8);
			current_brightness = 0xB8;
		}
	
	}
	lcd_effect_info("%s %s success\n", __func__, level ? "on" : "off");
	return 0;
}
static int lcd_set_bl_outdoor(struct msm_fb_data_type *mfd, int level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;

	
	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (mfd->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("%s: LCD panel have powered off\n", __func__);
		return -EPERM;
	}
	return bl_set_level(level);
}
#endif

static int set_mode(struct msm_fb_data_type *mfd, struct panel_effect_data *panel_data, int index)
{
	struct lcd_cmds lcd_cmd;
	int cnt = 0;
	int ret = 0;

	lcd_cmd.cmd = NULL;
	lcd_cmd.cnt = 0;

    mutex_lock(&lcd_effect_mtx);
	printk("%s index %d last_mode %d \n", __func__, index, last_mode);
	lcd_cmd.cmd = panel_data->buf;
	copy_head_code(panel_data, panel_data->buf, &cnt);
	if (is_effect_hold(panel_data, index)) {
		copy_mode_code(panel_data, panel_data->buf + cnt, last_mode, &cnt);
	}
	copy_mode_code(panel_data, panel_data->buf + cnt, index, &cnt);

	lcd_cmd.cnt = cnt;

	if (index != 0 || panel_data->mode_have_inited)
		ret = prepare_to_send_cmds(mfd, &lcd_cmd);
	if (!ret || ret == -EPERM) {
		panel_data->mode_data->current_mode = index;
		if (index <= 3)
			last_mode = index;
		panel_data->mode = &panel_data->mode_data->mode[index];
		lcd_effect_info("%s %s success, index %d flag %d\n", __func__, panel_data->mode_data->mode[index].name, index, panel_data->mode_have_inited);
		lcd_set_bl_outdoor(mfd, panel_data->mode_data->mode[index].bl_ctrl);
		ret = 0;
	}
	panel_data->mode_have_inited = 1;
	if (is_show_lcd_param)
		show_lcd_param(lcd_cmd.cmd, lcd_cmd.cnt);

	lcd_cabc_ctrl(mfd, panel_data, current_bl);
	mutex_unlock(&lcd_effect_mtx);

	return ret;
}

static int set_effect(struct msm_fb_data_type *mfd, struct panel_effect_data *panel_data, int index, int level)
{
	struct lcd_cmds lcd_cmd;
	struct dsi_cmd_desc *temp;
	int mode_index = panel_data->mode_data->current_mode;
	struct lcd_mode *mode= &panel_data->mode_data->mode[mode_index];
	int cnt = 0;
	int ret;

	lcd_cmd.cmd = panel_data->buf;

	temp = copy_head_code(panel_data, panel_data->buf, &cnt);
	copy_single_effect_code(panel_data, temp, index, level, &cnt);

	lcd_cmd.cnt = cnt;

	ret = prepare_to_send_cmds(mfd, &lcd_cmd);
	if (!ret || ret == -EPERM) {
		panel_data->effect_data->effect[index].level = level;
		mode->level[index] = level;
		lcd_effect_info("%s name: [%s] level: [%d] success\n", __func__, panel_data->effect_data->effect[index].name, level);
		ret = 0;
	} 	
	if (is_show_lcd_param)
		show_lcd_param(lcd_cmd.cmd, lcd_cmd.cnt);

	return ret;
}

static int lcd_get_mode(struct lcd_mode_data *mode_data)
{
	if (mode_data == NULL)
		return -EINVAL;

	lcd_effect_info("%s name: [%s] index: [%d]\n", __func__, mode_data->mode[mode_data->current_mode].name, mode_data->current_mode);
	return mode_data->current_mode;
}

static int lcd_get_supported_effect_level(struct lcd_effect_data *effect_data, int index)
{
	if (effect_data == NULL || index < 0) {
	         lcd_effect_info("%s index: %d invalid, max index is: 0 - %d\n", __func__, index, effect_data->supported_effect - 1);
		return -EINVAL;
	}

	lcd_effect_info("%s name: [%s] index: [%d] max_level: [%d]\n", __func__, effect_data->effect[index].name, index, effect_data->effect[index].max_level);
	return effect_data->effect[index].max_level;
}
static int lcd_get_effect_level(struct panel_effect_data *panel_data, int index)
{
	struct lcd_effect_data *effect_data = panel_data->effect_data;
	if (effect_data == NULL || index < 0) {
		lcd_effect_info("%s index: %d invalid, max index is: 0 - %d\n", __func__, index, effect_data->supported_effect - 1);
		return -EINVAL;
	}

	if (panel_data->mode) {
		lcd_effect_info("%s name: [%s] index: [%d] level: [%d]\n", __func__, effect_data->effect[index].name, index, panel_data->mode->level[index]);
		return panel_data->mode->level[index];
	}
	lcd_effect_info("%s name: [%s] index: [%d] level: [%d]\n", __func__, effect_data->effect[index].name, index, effect_data->effect[index].level);
	return effect_data->effect[index].level;
}

static int lcd_get_supported_effect(struct lcd_effect_data *effect_data, struct hal_panel_data *data)
{
	int i;

	if (effect_data == NULL || data == NULL)
		return -EINVAL;

	if (EFFECT_COUNT < effect_data->supported_effect)
		return -ENOMEM;

	for (i = 0; i < effect_data->supported_effect; i++) {
		memcpy(data->effect[i].name, effect_data->effect[i].name, strlen(effect_data->effect[i].name));
	}
	data->effect_cnt = effect_data->supported_effect;

	return data->effect_cnt;
}

static int lcd_get_supported_mode(struct lcd_mode_data *mode_data, struct hal_panel_data *data)
{
	int i;

	if (mode_data == NULL || data == NULL)
		return -EINVAL;

	if (MODE_COUNT < mode_data->supported_mode)
		return -ENOMEM;

	for (i = 0; i < mode_data->supported_mode; i++) {
		memcpy(data->mode[i].name, mode_data->mode[i].name, strlen(mode_data->mode[i].name));
	}
	data->mode_cnt = mode_data->supported_mode;

	return data->mode_cnt;
}

static int lcd_set_mode(struct msm_fb_data_type *mfd, struct panel_effect_data *panel_data, int mode)
{
	int ret;
	struct lcd_mode_data *mode_data = panel_data->mode_data;

	if (mode >= mode_data->supported_mode || mode < 0) {
		lcd_effect_info("%s mode invalid, max mode is: 0 - %d\n", __func__, mode_data->supported_mode - 1);
		return -EINVAL;
	}

	ret = set_mode(mfd, panel_data, mode);

	return ret;
}

static int lcd_set_effect(struct msm_fb_data_type *mfd, struct panel_effect_data *panel_data, int index, int level)
{
	int ret;
	struct lcd_effect_data *effect_data = panel_data->effect_data;

	if (index >= effect_data->supported_effect || index < 0) {
		lcd_effect_info("%s index invalid, max index is: 0 - %d\n", __func__, effect_data->supported_effect - 1);
		return -EINVAL;
	}

	if (level >= effect_data->effect[index].max_level || level < 0) {
		lcd_effect_info("%s level invalid, max level is: 0 - %d\n", __func__, effect_data->effect[index].max_level - 1);
		return -EINVAL;
	}

	ret = set_effect(mfd, panel_data, index, level);

	return ret;
}

int update_init_code(
		struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		struct panel_effect_data *panel_data,
		void (*mdss_dsi_panel_cmds_send)(struct mdss_dsi_ctrl_pdata *ctrl,struct dsi_panel_cmds *pcmds))
{
	struct lcd_cmds lcd_cmd;
	struct lcd_cmds *save_cmd = &panel_data->save_cmd;
	int mode_index = panel_data->mode_data->current_mode;
	int cnt = 0;
	int ret = 0;
    mutex_lock(&lcd_effect_mtx);
	if (panel_data->policy->effect_enabled) {
		lcd_cmd.cmd = panel_data->buf;

		copy_init_code(panel_data, &cnt);
		if (is_effect_hold(panel_data, mode_index)) {
			copy_mode_code(panel_data, panel_data->buf + cnt, last_mode, &cnt);
		}
		copy_mode_code(panel_data, panel_data->buf + cnt, mode_index, &cnt);
		copy_sleep_out_code(panel_data, panel_data->buf + cnt, &cnt);

		lcd_cmd.cnt = cnt;
	}

#ifdef READ_LCD_PARAM
	if (read_lcd_file) 
	{
		if (!open_lcd_and_get_data()) {
			ctrl_pdata->on_cmds.cmds = lcd_txt_cmds.cmd;
			ctrl_pdata->on_cmds.cmd_cnt = lcd_txt_cmds.cnt;
			lcd_effect_info("%s Use Txt file param\n", __func__);
		}
	} else 
#endif
	{
		if (panel_data->policy->effect_enabled) {
			ctrl_pdata->on_cmds.cmds = lcd_cmd.cmd;
			ctrl_pdata->on_cmds.cmd_cnt = lcd_cmd.cnt;
			lcd_effect_info("%s Use system param\n", __func__);
		}
	}

	mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->on_cmds);
	if (is_show_lcd_param)
		show_lcd_param(ctrl_pdata->on_cmds.cmds, ctrl_pdata->on_cmds.cmd_cnt);

	ctrl_pdata->on_cmds.cmds = save_cmd->cmd;
	ctrl_pdata->on_cmds.cmd_cnt = save_cmd->cnt;
	mutex_unlock(&lcd_effect_mtx);
#ifdef READ_LCD_PARAM
	clear_data_and_mem();
#endif
#if 1
	bl_set_level(panel_data->mode_data->mode[mode_index].bl_ctrl);
#endif
	return ret;
}

static int low_bl_value_end = 30;
static int low_bl_value_begin = 20;
module_param(low_bl_value_end, int, 0644);
module_param(low_bl_value_begin, int, 0644);
void lcd_cabc_ctrl(struct msm_fb_data_type *mfd, struct panel_effect_data * panel_data, int level)
{
	int cabc_index = 0;

	current_bl = level;
	if (panel_data->policy->low_bl_disable_cabc) {
		cabc_index = get_effect_index_by_name("cabc", panel_data);
		if (level > low_bl_value_end) {
			//enable cabc
			if (panel_data->mode->level[cabc_index] != panel_data->mode->default_level[cabc_index])
				lcd_set_effect(mfd, panel_data, cabc_index, panel_data->mode->default_level[cabc_index]);
		} else if (level < low_bl_value_begin){
			//disable cabc
			if (panel_data->mode->level[cabc_index])
				lcd_set_effect(mfd, panel_data, cabc_index, 0);
		}
	}
}

int handle_lcd_effect_data(
		struct msm_fb_data_type *mfd, 
		struct panel_effect_data *panel_data, 
		struct hal_panel_ctrl_data *ctrl_data)
{
	struct lcd_effect_data *effect_data = panel_data->effect_data;
	struct lcd_mode_data *mode_data = panel_data->mode_data;
	int ret = 0;

	switch(ctrl_data->id) {
		case GET_EFFECT_NUM:
			ret = lcd_get_supported_effect(effect_data, &ctrl_data->panel_data);
			break;
		case GET_EFFECT_LEVEL:
			ret = lcd_get_supported_effect_level(effect_data, ctrl_data->index);
			break;
		case GET_EFFECT:
			ret = lcd_get_effect_level(panel_data, ctrl_data->index);
			break;
		case GET_MODE_NUM:
			ret = lcd_get_supported_mode(mode_data, &ctrl_data->panel_data);
			break;
		case GET_MODE:
			ret = lcd_get_mode(mode_data);
			break;
		case SET_EFFECT:
			ret = lcd_set_effect(mfd, panel_data, ctrl_data->index, ctrl_data->level);
			break;
		case SET_MODE:
			ret = lcd_set_mode(mfd, panel_data, ctrl_data->mode);
			break;
		case SET_BL_LEVEL:
			ret = lcd_set_bl_outdoor(mfd, ctrl_data->level);
			break;
		case GET_BL_LEVEL:
			ret = lcd_get_bl_outdoor(mfd);
			break;
		default:
			break;
	}

	return ret;
}


int malloc_lcd_effect_code_buf(struct panel_effect_data *panel_data)
{
	struct lcd_cmds *save_cmd = &panel_data->save_cmd;
	if (panel_data->buf == NULL) {
		panel_data->buf_size = get_init_code_max_cnt(panel_data, save_cmd);
		panel_data->buf  = kmalloc(sizeof(struct dsi_cmd_desc) * panel_data->buf_size, GFP_KERNEL);
		if ( !panel_data->buf)
			return -ENOMEM;
		return 0;
	}
	return 0;
}
#ifdef READ_LCD_PARAM
#if 0
static int get_lcd_param_func(const char *val, struct kernel_param *kp)
{
	int value;
	int ret = param_set_int(val, kp); 

	if(ret < 0) 
	{    
		pr_info(KERN_ERR"%s Invalid argument\n", __func__);
		return -EINVAL;
	}    
	value = *((int*)kp->arg);
	if (value) {
		read_lcd_file = 1;
		pr_info("prepare to read lcd param...\n");
	} else {
		read_lcd_file = 0;
		pr_info("show lcd param off...\n");
	}
	return 0;
}
#endif

module_param(read_lcd_file, int, 0644);
#endif
module_param(is_show_lcd_param, int, 0644);
#if 0
static int show_lcd_param_func(const char *val, struct kernel_param *kp)
{
	int value;
	int ret = param_set_int(val, kp); 

	if(ret < 0) 
	{    
		pr_info(KERN_ERR"%s Invalid argument\n", __func__);
		return -EINVAL;
	}    
	value = *((int*)kp->arg);
	if (value) {
		is_show_lcd_param = 1;
		pr_info("show lcd param on...\n");
	} else {
		is_show_lcd_param = 0;
		pr_info("show lcd param off...\n");
	}
	return 0;
}
#endif
#define DSI0_PHY_BASE	0xfd922800
#define DSI1_PHY_BASE	0xfd922e00
#define DSI_REG_SIZE	0x600
static void read_dsi_register(void)
{
	unsigned char *dsi0_base = NULL;
	unsigned char *dsi1_base = NULL;
	int i;

	dsi0_base = ioremap(DSI0_PHY_BASE, DSI_REG_SIZE);
	dsi1_base = ioremap(DSI1_PHY_BASE, DSI_REG_SIZE);

	i = 0;
	if (dsi0_base != NULL) {
		for (i = 0; i < DSI_REG_SIZE / 4; i++) {
			pr_info("DSI0: reg[%8x] 0x%8x\n", i * 4 + DSI0_PHY_BASE, MIPI_INP(dsi0_base + i * 4));
		}

		for (i = 0; i < DSI_REG_SIZE / 4; i++) {
			pr_info("DSI1: reg[%8x] 0x%8x\n", i * 4 + DSI1_PHY_BASE, MIPI_INP(dsi1_base + i * 4));
		}
	}
}
static int read_dsi_func(const char *val, struct kernel_param *kp)
{
   int value;
   int ret = param_set_int(val, kp); 

   if(ret < 0) 
   {    
       pr_info("%s Invalid argument\n", __func__);
       return -EINVAL;
   }    
   value = *((int*)kp->arg);
   if (value) {
       pr_info("prepare to read dsi register...\n");
	   read_dsi_register();
	   read_dsi = 1;
   } else {
	   read_dsi = 0;
   } 
   return 0;
}

module_param_call(read_dsi, read_dsi_func, param_get_int, &read_dsi, S_IRUSR | S_IWUSR);
