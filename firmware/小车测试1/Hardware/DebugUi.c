#include "DebugUi.h"
#include "Config.h"
#include "Tick.h"
#include "OLED.h"
#include "Referee.h"
#include "RC.h"
#include "Chassis.h"
#include "Encoder.h"
#include "Shooter.h"
#include "TestMode.h"

#if DEBUG_OLED

#define DEBUGUI_PERIOD_MS  200		/* 5Hz 刷新就够看，刷太快会拖慢控制循环 */

static uint32_t AbsU32(float v)
{
	if (v < 0.0f) v = -v;
	return (uint32_t)v;
}

void DebugUi_Init(void)
{
	OLED_Init();
	OLED_Clear();
}

/**
  * @brief  刷新调试页
  * @note
  *   第1行  LNK1 T0 S0    链路 / 考察模式 / 正在发射
  *   第2行  A 40 B 40     轮子A、B 的占空比(%)
  *   第3行  A 120 B -118  轮子A、B 的实测转速(RPM)，没接编码器就是 0
  *   第4行  D 502 G 720   车体位移(cm) / 累计转角(度)
  */
void DebugUi_Update(void)
{
	static uint32_t last = 0;

	if ((now_ms() - last) < DEBUGUI_PERIOD_MS) return;
	last = now_ms();

	/* ---------- 第 1 行：L=键鼠链路  R=遥控  T=考察模式  S=正在发射 ---------- */
	OLED_ShowString(1, 1, "L");
	OLED_ShowNum   (1, 2, kb.connected, 1);
	OLED_ShowString(1, 3, " R");
	OLED_ShowNum   (1, 5, RC_IsConnected(), 1);
	OLED_ShowString(1, 6, " T");
	OLED_ShowNum   (1, 8, TestMode_IsActive(), 1);
	OLED_ShowString(1, 9, " S");
	OLED_ShowNum   (1, 11, Shooter_IsFiring(), 1);

	/* ---------- 第 2 行：A / B 占空比 ---------- */
	OLED_ShowString(2, 1, "A");
	OLED_ShowSignedNum(2, 2, (int32_t)(Chassis_GetDuty(0) * 100.0f), 3);
	OLED_ShowString(2, 6, "B");
	OLED_ShowSignedNum(2, 7, (int32_t)(Chassis_GetDuty(1) * 100.0f), 3);

	/* ---------- 第 3 行：A / B 实测转速 ---------- */
	OLED_ShowString(3, 1, "A");
	OLED_ShowSignedNum(3, 2, (int32_t)Encoder_GetRpm(0), 4);
	OLED_ShowString(3, 7, "B");
	OLED_ShowSignedNum(3, 8, (int32_t)Encoder_GetRpm(1), 4);

	/* ---------- 第 4 行：里程 / 转角 ---------- */
	OLED_ShowString(4, 1, "D");
	OLED_ShowNum(4, 2, AbsU32(Encoder_GetCarTravelM() * 100.0f), 4);
	OLED_ShowString(4, 7, "G");
	OLED_ShowNum(4, 8, AbsU32(Encoder_GetCarAngleDeg()), 4);
}

#else

void DebugUi_Init(void)   { }
void DebugUi_Update(void) { }

#endif
