#ifndef __REFEREE_H
#define __REFEREE_H

#include "stm32f10x.h"
#include "Config.h"

/* ============================================================
 *  裁判系统键鼠包
 *  链路：键盘鼠标 -> 裁判系统客户端 -> 车上装甲板 -> 串口 10 字节/100Hz -> 这里
 *  ============================================================ */

typedef struct
{
	float   mouse_x;       /* 鼠标左右移动，-1.0 ~ +1.0 */
	float   mouse_y;       /* 鼠标前后移动，-1.0 ~ +1.0 */
	float   mouse_z;       /* 滚轮，-1.0 ~ +1.0 */

	uint8_t left_button;   /* 鼠标左键：1 = 按下（波箱发射水弹） */
	uint8_t right_button;  /* 鼠标右键 */

	uint8_t w;             /* 前进 */
	uint8_t s;             /* 后退 */
	uint8_t a;             /* 左平移 */
	uint8_t d;             /* 右平移 */
	uint8_t q;             /* 原地左转 */
	uint8_t e;             /* 原地右转 */
	uint8_t shift;
	uint8_t ctrl;

	uint8_t  connected;    /* 1 = 链路正常，0 = 掉线 */
	uint32_t last_rx_ms;   /* 最后一次收到完整包的时刻 */

	uint8_t  raw[KB_LEN];  /* 原始 10 个字节，调试用：
	                        * 在 OLED 上看 raw[8] 就知道键盘位图的格式对不对 */
} Referee_t;

extern Referee_t kb;        /* 全车共用这一份输入 */

void Referee_Init(void);            /* 初始化 USART3（PB10/PB11，115200） */
void Referee_OnByte(uint8_t b);     /* 串口中断里调用，负责拆包 */
void Referee_Update(void);          /* 1ms 调用一次，负责超时清零 */

#endif
