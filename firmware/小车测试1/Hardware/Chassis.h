#ifndef __CHASSIS_H
#define __CHASSIS_H

#include "stm32f10x.h"

/* ============================================================
 *  底盘：把"车体速度"翻译成"四个轮子各转多快"
 *
 *  车体速度用三个量描述（右手系，车头方向为 +X，左侧为 +Y）：
 *     vx  前进速度，单位 m/s      正 = 前进
 *     vy  左移速度，单位 m/s      正 = 向左（不转头）
 *     wz  旋转角速度，单位 度/秒  正 = 逆时针（原地左转）
 * ============================================================ */

void Chassis_Init(void);

/* 麦轮逆解：车体速度 -> 4 个轮子的线速度(m/s)，顺序 左前/右前/右后/左后 */
void Chassis_Solve(float vx, float vy, float wz, float out[4]);

/* 设置目标速度（真正的加减速在 Chassis_Update 里做） */
void Chassis_SetSpeed(float vx, float vy, float wz);

/* 1ms 调用一次：读按键 -> 加减速 -> 逆解 -> 输出给 Motor */
void Chassis_Update(float dt);

/* 立即停车（掉线、急停时用） */
void Chassis_Stop(void);

/* 读取某个轮子当前的占空比（-1.0 ~ +1.0），调试显示用 */
float Chassis_GetDuty(uint8_t i);

/* 1 = Chassis_Update 里按裁判系统按键算目标速度（默认）
 * 0 = 不理会按键，目标速度完全由外面设置（考察模式用） */
void Chassis_UseKeyboard(uint8_t on);

#endif
