#ifndef __SHOOTER_H
#define __SHOOTER_H

#include "stm32f10x.h"

/* ============================================================
 *  波箱发射控制
 *
 *  硬件：波箱是有刷直流电机，通电就转、断电就停
 *  引脚：PA5 -> MOS 管栅极（串 100Ω），MOS 通断电机电源
 *
 *  为什么必须有软启动：波箱启动峰值电流 6A，直接拉高 MOS，
 *  这个冲击叠在 4 个电机的加速电流上，很容易把 12V 电池的
 *  8A 过流保护顶掉 —— 整车瞬间断电
 * ============================================================ */

void    Shooter_Init(void);
void    Shooter_SetContinous(uint8_t on);   /* 连发：1 开始，0 停止 */
void    Shooter_TriggerSingle(void);        /* 单发：按键上升沿时调用一次 */
void    Shooter_CancelSingle(void);         /* 松开按键时立即结束单发 */
void    Shooter_Update(void);               /* 1ms 调用 */
uint8_t Shooter_IsFiring(void);             /* 当前是否在通电，调试显示用 */

#endif
