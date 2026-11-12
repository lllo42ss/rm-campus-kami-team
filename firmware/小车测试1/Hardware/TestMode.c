#include "TestMode.h"
#include "Config.h"
#include "Tick.h"
#include "Chassis.h"
#include "Encoder.h"
#include "Servo.h"
#include "Shooter.h"

#define BTN_PORT      GPIOB
#define BTN_PIN       GPIO_Pin_1			/* 江协板的 Key1 */
#define BTN_LONG_MS   800					/* 按住多久算长按 */

#define TM_ITEM_COUNT 5

static uint8_t  s_active = 0;
static uint8_t  s_item   = 0;
static uint32_t s_item_ms = 0;

/* ---- 按键消抖用的状态 ---- */
static uint8_t  s_raw = 1;					/* 原始电平，1 = 没按 */
static uint8_t  s_level = 1;				/* 消抖后的电平 */
static uint8_t  s_cnt = 0;
static uint32_t s_down_ms = 0;
static uint8_t  s_long_fired = 0;
static uint8_t  s_ev_short = 0;
static uint8_t  s_ev_long = 0;

void TestMode_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin   = BTN_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;		/* 按键另一端接地，按下为低 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BTN_PORT, &GPIO_InitStructure);
}

uint8_t TestMode_IsActive(void) { return s_active; }
uint8_t TestMode_GetItem(void)  { return s_item; }

/**
  * @brief  按键扫描：20ms 消抖，区分短按和长按
  */
static void Button_Scan(void)
{
	uint8_t raw = (uint8_t)GPIO_ReadInputDataBit(BTN_PORT, BTN_PIN);

	s_ev_short = 0;
	s_ev_long  = 0;

	/* 电平必须稳定 20ms 才认 */
	if (raw == s_raw) { if (s_cnt < 25) s_cnt++; }
	else              { s_raw = raw; s_cnt = 0; }

	if (s_cnt == 20 && raw != s_level)
	{
		s_level = raw;

		if (s_level == 0)						/* 按下 */
		{
			s_down_ms    = now_ms();
			s_long_fired = 0;
		}
		else									/* 松开 */
		{
			if (!s_long_fired && (now_ms() - s_down_ms) > 50)
				s_ev_short = 1;
			s_long_fired = 0;
		}
	}

	/* 一直按着超过 800ms -> 长按（只报一次） */
	if (s_level == 0 && !s_long_fired && (now_ms() - s_down_ms) > BTN_LONG_MS)
	{
		s_ev_long    = 1;
		s_long_fired = 1;
	}
}

/**
  * @brief  开始（或重开）当前考察项
  */
static void StartItem(void)
{
	Encoder_ResetOdom();		/* 里程清零，否则会累加上一项的成绩 */
	Chassis_UseKeyboard(0);		/* 接管底盘，别让裁判系统按键插一脚 */
	Gimbal_SetEnable(1);
	Shooter_SetContinous(0);
	s_item_ms = now_ms();
}

/**
  * @brief  退出考察模式，把所有输出交回去
  */
static void StopAll(void)
{
	Chassis_SetSpeed(0.0f, 0.0f, 0.0f);
	Shooter_SetContinous(0);
	Chassis_UseKeyboard(1);
}

/**
  * @brief  跑当前考察项
  */
static void RunItem(void)
{
	uint32_t el = now_ms() - s_item_ms;

	/* 超时保护：编码器没接（里程永远是 0）或者机械卡住时，
	 * 靠这一条兜底，否则车会一直往前开 */
	if (el > TEST_TIMEOUT_MS)
	{
		Chassis_SetSpeed(0.0f, 0.0f, 0.0f);
		return;
	}

	switch (s_item)
	{
	case 0:		/* 1 直线移动 */
		if (Encoder_GetCarTravelM() >= TEST_DIST_M)
			Chassis_SetSpeed(0.0f, 0.0f, 0.0f);			/* 走够了，停 */
		else
			Chassis_SetSpeed(TEST_DIST_SPEED, 0.0f, 0.0f);
		break;

	case 1:		/* 2 原地旋转 */
		if (Encoder_GetCarAngleDeg() >= TEST_TURN_DEG)
			Chassis_SetSpeed(0.0f, 0.0f, 0.0f);
		else
			Chassis_SetSpeed(0.0f, 0.0f, TEST_TURN_SPEED);
		break;

	case 2:		/* 3 全向平移 */
		if (Encoder_GetCarTravelM() >= TEST_STRAFE_M)
			Chassis_SetSpeed(0.0f, 0.0f, 0.0f);
		else
			Chassis_SetSpeed(0.0f, TEST_STRAFE_SPEED, 0.0f);
		break;

	case 3:		/* 4 枪口俯仰：45 度 <-> 135 度来回摆 */
		if (((el / 2000u) % 2u) == 0u) Gimbal_SetAngle(PITCH_MAX);
		else                           Gimbal_SetAngle(PITCH_MIN);
		break;

	case 4:		/* 5 波箱连发 10 秒 */
		if (el < TEST_SHOOT_MS) Shooter_SetContinous(1);
		else                    Shooter_SetContinous(0);
		break;

	default:
		break;
	}
}

/**
  * @brief  考察模式更新，1ms 调用
  */
void TestMode_Update(void)
{
#if TESTMODE_ENABLE
	Button_Scan();

	if (!s_active)
	{
		if (s_ev_short)						/* 没在考察模式：短按进入 */
		{
			s_active = 1;
			s_item   = 0;
			StartItem();
		}
		return;
	}

	if (s_ev_long)							/* 长按退出 */
	{
		StopAll();
		s_active = 0;
		return;
	}

	if (s_ev_short)							/* 短按切下一项 */
	{
		s_item = (uint8_t)((s_item + 1u) % TM_ITEM_COUNT);
		StartItem();
		return;
	}

	RunItem();
#endif
}
