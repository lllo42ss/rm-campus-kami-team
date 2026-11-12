#ifndef __PS2_H
#define __PS2_H

#include "stm32f10x.h"

/* ============================================================
 *  PS2 无线手柄接收器驱动（软件模拟 SPI）
 *
 *  接线：
 *      CLK -> PB3    （输出）
 *      CS  -> PA11   （输出，模块上可能标 ATT）
 *      CMD -> PA12   （输出）
 *      DAT -> PA15   （输入，建议外加 4.7k 上拉到 3.3V）
 *      VCC -> 3.3V 或 5V（看模块，见 README）
 *      GND -> GND
 *
 *  本驱动相比初版修正了：
 *      1. 时钟从约 830kHz 降到约 250kHz（PS2 规范上限 500kHz）
 *      2. 一帧期间关中断，避免 SysTick 打断时序
 *      3. 两次轮询至少间隔 10ms，避免把接收器问死
 *      4. 单次读失败自动重试 3 次
 *      5. 连续失败 5 次才判定掉线，避免灯/数据显示乱跳
 *      6. 增加"悬空线"判断（全 0xFF）
 * ============================================================ */

typedef struct
{
	uint8_t buttons1;		/* 按键字节 1 */
	uint8_t buttons2;		/* 按键字节 2 */

	uint8_t rx, ry;			/* 右摇杆，0~255，中位约 128 */
	uint8_t lx, ly;			/* 左摇杆，0~255，中位约 128 */

	/* 常用按键，1 = 按下 */
	uint8_t start;
	uint8_t select;
	uint8_t up, down, left, right;

	/* 面键 / 肩键，1 = 按下 */
	uint8_t triangle, circle, cross, square;
	uint8_t l1, r1;
	uint8_t l2, r2;
	uint8_t l3, r3;
} PS2_Data_t;

void     PS2_Init(void);

/* 读一次手柄。
 * 返回 1 = 当前链路正常（数据有效）；0 = 掉线（out 里是最后一次有效数据）。
 * 内部已做限频、重试、失联计数，可以直接在 1ms 主循环里调用。 */
uint8_t  PS2_Read(PS2_Data_t *out);

/* 摇杆原始值 0~255 -> -1.0~+1.0，带 8% 死区 */
float    PS2_Axis(uint8_t raw);

/* 调试接口 */
uint8_t  PS2_IsConnected(void);
uint16_t PS2_GetFailCount(void);		/* 累计失败帧数 */

/* 最近一帧收到的 9 个原始字节（诊断用，-- 的时候看它判断问题在哪） */
const uint8_t *PS2_GetRawFrame(void);

#endif
