#ifndef __TICK_H
#define __TICK_H

#include "stm32f10x.h"

/* 系统毫秒时基：SysTick 每 1ms 中断一次，s_ms 自增
 * 全车的延时、超时判断、控制周期都基于它
 */

void     Tick_Init(void);      /* 初始化 1ms 时基，main 里最先调用 */
void     Tick_Inc(void);       /* 只能被 SysTick_Handler 调用 */
uint32_t now_ms(void);         /* 读取当前毫秒数，溢出（约 49.7 天）自动正确 */
uint32_t now_us(void);         /* 读取当前微秒数，遥控接收机测脉宽要用 */

#endif
