#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/* 电机编号：0=A(左前)  1=B(右前)  2=C(右后)  3=D(左后) */
/* 引脚分配：
 *   PWM  : A=PA1(TIM2_CH2)  B=PA0(TIM2_CH1)  C=PA2(TIM2_CH3)  D=PA3(TIM2_CH4)
 *   方向 : A=PB12/PB13  B=PB14/PB15  C=PB0/PB1  D=PB4/PB5
 *   使能 : STBY = PA4（高电平才允许驱动）
 *   注意 : 用到 PB4，所以初始化里关掉了 JTAG（保留 SWD）
 *
 * 和卖家接线图的差异（详见工程根目录 WIRING.md）：
 *   · 图上 BIN 用 PC13/PC14 -> 这里用 PB14/PB15
 *     （PC13/PC14 是 RTC 备份脚，驱动弱；PB10/PB11 是裁判系统串口，也不能占）
 *   · 图上 CIN 用 PC1/PC2 -> 这里用 PB4/PB5（PC1/PC2 在 C8T6 上不存在）
 *   · 图上 E2 用 PA15/PB3 -> 这里用 PB6/PB7
 *     （PA15/PB3 是 TIM2 重映射，和 PA0~PA3 的 PWM 是同一个定时器，二选一）
 */

void Motor_Init(void);                                        /* 初始化，上电默认不使能 */
void Motor_Enable(uint8_t on);                                /* 0=关闭驱动(急停)  1=允许驱动 */
void Motor_Set(uint8_t ch, float duty);                       /* duty: -1.0 ~ +1.0，负数反转 */
void Motor_SetAll(float a, float b, float c, float d);
void Motor_Brake(void);                                       /* PWM 全部置 0，立即停 */

/* 调试用：单独设置某一轮的方向引脚（排查接线时用） */
void Motor_SetDir(uint8_t ch, uint8_t forward);

#endif
