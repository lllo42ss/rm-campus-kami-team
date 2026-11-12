#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

/* ============================================================
 *  舵机（底层） + 云台俯仰（上层）
 *
 *  硬件：MG996R，50Hz PWM，脉宽 0.5ms=0度，1.5ms=90度，2.5ms=180度
 *  引脚：PA8 = TIM1_CH1
 * ============================================================ */

void Servo_Init(void);                  /* TIM1_CH1 输出 50Hz PWM */
void Servo_SetAngle(float deg);         /* 0 ~ 180 度，内部限幅 */
void Servo_Disable(void);               /* 关掉 PWM 输出 */

/* ---------------- 云台俯仰 ---------------- */

void  Gimbal_Init(void);                /* 初始化并把枪口回到中位 */
void  Gimbal_SetEnable(uint8_t on);     /* 0 = 失能（不输出），1 = 使能 */
void  Gimbal_SetSpeed(float dps);       /* 设定俯仰角速度，度/秒（鼠标控制用） */
void  Gimbal_SetAngle(float deg);       /* 直接指定目标角度（考察模式用） */
float Gimbal_GetAngle(void);            /* 当前枪口角度 */
void  Gimbal_Update(float dt);          /* 1ms 调用：积分 + 限位 + 输出 */

#endif
