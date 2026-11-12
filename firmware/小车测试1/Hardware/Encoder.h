#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

/* ============================================================
 *  编码器测速 + 里程计
 *
 *  只有两个定时器能接编码器（TIM3、TIM4），所以只接两个轮子：
 *      编码器 0 -> 轮子 A（左前，PWMA）
 *      编码器 1 -> 轮子 B（右前，PWMB）
 *
 *  接上编码器以后，"走 5m"和"转 720 度"就不用靠掐表估算了：
 *  直接数轮子转了多少圈。电池电量下降导致速度变慢时，
 *  时间法会走不够距离，里程计法不会。
 * ============================================================ */

void  Encoder_Init(void);
void  Encoder_Update(float dt);         /* 1ms 调用，不调用就没有数据 */

float Encoder_GetRpm(uint8_t i);        /* 轮子转速，RPM，正负表示方向 */
float Encoder_GetTravelM(uint8_t i);    /* 轮子累计行程，米 */

void  Encoder_ResetOdom(void);          /* 里程清零（每次做动作前调一次） */
float Encoder_GetCarTravelM(void);      /* 车体累计位移，米 */
float Encoder_GetCarAngleDeg(void);     /* 车体累计转角，度，正 = 逆时针 */

#endif
