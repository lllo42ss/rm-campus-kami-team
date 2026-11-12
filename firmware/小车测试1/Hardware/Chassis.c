#include "Chassis.h"
#include "Config.h"
#include "Motor.h"
#include "Referee.h"
#include "RC.h"

/* 当前速度（经过加减速斜坡）与目标速度 */
static float vx_now = 0, vy_now = 0, wz_now = 0;
static float vx_tgt = 0, vy_tgt = 0, wz_tgt = 0;

/* 最近一次算出来的 4 个轮子占空比，调试显示用 */
static float s_duty[4] = {0, 0, 0, 0};

/* 1 = 按裁判系统按键算目标速度，0 = 目标速度由外面给（考察模式） */
static uint8_t s_use_kb = 1;

#define SQRT2_2   0.70710678f          /* sqrt(2)/2 */

/**
  * @brief  底盘初始化
  */
void Chassis_Init(void)
{
	vx_now = vy_now = wz_now = 0;
	vx_tgt = vy_tgt = wz_tgt = 0;
}

/**
  * @brief  麦轮逆解：车体速度 -> 四个轮子的线速度
  * @param  vx  前进速度 m/s
  * @param  vy  左移速度 m/s
  * @param  wz  逆时针角速度 度/秒
  * @param  out 输出 4 个轮子的线速度 m/s，顺序：左前 / 右前 / 右后 / 左后
  * @note   轮子布局为标准 X 型：
  *           左前 = 左旋轮(L)   右前 = 右旋轮(R)
  *           左后 = 右旋轮(R)   右后 = 左旋轮(L)
  *         如果实车跑起来"前进变后退"或"平移变旋转"，就是这里的符号问题，
  *         按 README 里的四个方向测试逐条标定
  */
void Chassis_Solve(float vx, float vy, float wz, float out[4])
{
	/* 旋转角速度 度/秒 -> 轮子切向速度 m/s */
	float vz = wz / 360.0f * (2.0f * 3.14159265f * CHASSIS_R);

	out[0] = SQRT2_2 * ( vx + vy) + vz;		/* 左前 */
	out[1] = SQRT2_2 * (-vx + vy) + vz;		/* 右前 */
	out[2] = SQRT2_2 * (-vx - vy) + vz;		/* 右后 */
	out[3] = SQRT2_2 * ( vx - vy) + vz;		/* 左后 */
}

/**
  * @brief  设置目标速度
  */
void Chassis_SetSpeed(float vx, float vy, float wz)
{
	vx_tgt = vx;
	vy_tgt = vy;
	wz_tgt = wz;
}

/**
  * @brief  加速度限幅：让速度值按最大加速度慢慢逼近目标值
  * @note   为什么必须做：
  *         1. 四个电机同时满占空比起步，瞬间电流会把 12V 电池的
  *            8A 过流保护顶掉，整车直接断电；
  *         2. 麦轮瞬间打滑，直线 5 米会跑偏。
  */
static float Chassis_Ramp(float now, float target, float acc, float dt)
{
	float delta = target - now;
	float step  = acc * dt;

	if (delta >  step) delta =  step;
	if (delta < -step) delta = -step;

	return now + delta;
}

/**
  * @brief  底盘主循环，1ms 调用一次
  * @param  dt 距上次调用的时间，单位秒（1ms 就是 0.001f）
  */
void Chassis_Update(float dt)
{
	float wheel[4];
	float mps_to_rpm = 60.0f / (2.0f * 3.14159265f * WHEEL_R);
	uint8_t i;

	/* ---------- 第一步：输入 -> 车体目标速度 ----------
	 * 优先级：遥控器 > 裁判系统键鼠。
	 * 遥控器关机（接收机 50ms 收不到脉冲）会自动让位给键鼠 */
	if (s_use_kb)
	{
		float vx = 0.0f, vy = 0.0f, wz = 0.0f;
		uint8_t has_input = 0;

		if (RC_IsConnected())
		{
			if (RC_CH_FWD != RC_NO_CH)
			{
				vx += RC_GetNorm(RC_CH_FWD) * VXY_MAX;
				has_input = 1;
			}
			if (RC_CH_STRAFE != RC_NO_CH)
			{
				vy += RC_GetNorm(RC_CH_STRAFE) * VXY_MAX;
				has_input = 1;
			}
			if (RC_CH_STEER != RC_NO_CH)
			{
				wz += RC_GetNorm(RC_CH_STEER) * VW_MAX;
				has_input = 1;
			}
		}
		else if (kb.connected)
		{
			if (kb.w) vx += VXY_MAX;			/* W 前进 */
			if (kb.s) vx -= VXY_MAX;			/* S 后退 */
			if (kb.a) vy += VXY_MAX;			/* A 左平移 */
			if (kb.d) vy -= VXY_MAX;			/* D 右平移 */
			if (kb.q) wz += VW_MAX;				/* Q 原地左转（逆时针） */
			if (kb.e) wz -= VW_MAX;				/* E 原地右转（顺时针） */
			has_input = 1;
		}

		if (has_input) Chassis_SetSpeed(vx, vy, wz);
	}

	/* ---------- 第二步：加减速限幅 ---------- */
	vx_now = Chassis_Ramp(vx_now, vx_tgt, ACC_XY, dt);
	vy_now = Chassis_Ramp(vy_now, vy_tgt, ACC_XY, dt);
	wz_now = Chassis_Ramp(wz_now, wz_tgt, ACC_W,  dt);

	/* ---------- 第三步：麦轮逆解 ---------- */
	Chassis_Solve(vx_now, vy_now, wz_now, wheel);

	/* ---------- 第四步：换算成占空比，输出给电机 ---------- */
	for (i = 0; i < 4; i++)
	{
		float rpm  = wheel[i] * mps_to_rpm;
		float duty = rpm / MOTOR_RPM_MAX;

		if (duty >  1.0f) duty =  1.0f;
		if (duty < -1.0f) duty = -1.0f;

		s_duty[i] = duty;
		Motor_Set(i, duty);
	}
}

/**
  * @brief  读取某个轮子当前的占空比
  * @param  i 轮子编号 0=A(左前) 1=B(右前) 2=C(右后) 3=D(左后)
  * @retval -1.0 ~ +1.0
  */
float Chassis_GetDuty(uint8_t i)
{
	if (i > 3) return 0.0f;
	return s_duty[i];
}

/**
  * @brief  是否按裁判系统按键算目标速度
  * @param  on 1 = 按键控制（默认），0 = 目标速度由外面设置
  */
void Chassis_UseKeyboard(uint8_t on)
{
	s_use_kb = on ? 1 : 0;
}

/**
  * @brief  立即停车
  */
void Chassis_Stop(void)
{
	vx_now = vy_now = wz_now = 0;
	vx_tgt = vy_tgt = wz_tgt = 0;
	s_duty[0] = s_duty[1] = s_duty[2] = s_duty[3] = 0;
	Motor_Brake();
}
