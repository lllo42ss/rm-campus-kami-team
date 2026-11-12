#include "Shooter.h"
#include "Config.h"
#include "Tick.h"

#define SHOOT_PORT   GPIOA
#define SHOOT_PIN    GPIO_Pin_5

static uint8_t  s_cont      = 0;	/* 连发请求 */
static uint16_t s_single_ms = 0;	/* 单发剩余通电时间 */
static uint8_t  s_on        = 0;	/* 当前是否通电（用于检测上升沿） */

#if !SHOOT_USE_RELAY
static uint32_t s_start_ms  = 0;	/* 本次通电开始时刻 */
static uint8_t  s_phase     = 0;	/* 软启动用的软件 PWM 相位 */
#endif

/* 兼容高电平触发和低电平触发继电器模块 */
static void Shooter_Write(uint8_t on)
{
	if (on)
	{
		if (SHOOT_ACTIVE_HIGH) GPIO_SetBits(SHOOT_PORT, SHOOT_PIN);
		else                   GPIO_ResetBits(SHOOT_PORT, SHOOT_PIN);
	}
	else
	{
		if (SHOOT_ACTIVE_HIGH) GPIO_ResetBits(SHOOT_PORT, SHOOT_PIN);
		else                   GPIO_SetBits(SHOOT_PORT, SHOOT_PIN);
	}
}

void Shooter_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	/* 切为输出前先写入“不发射”电平，避免上电瞬间产生低电平毛刺。
	 * 低电平触发模块的“不发射”电平是高电平。 */
	Shooter_Write(0);

	GPIO_InitStructure.GPIO_Pin   = SHOOT_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SHOOT_PORT, &GPIO_InitStructure);

	Shooter_Write(0);							/* 上电默认不发射 */
}

/**
  * @brief  连发开关
  * @param  on 1 = 持续通电发射，0 = 停
  */
void Shooter_SetContinous(uint8_t on)
{
	s_cont = on ? 1 : 0;
}

/**
  * @brief  单发
  * @note   必须在按键的"上升沿"调用一次。
  *         写成 if(按键) Shooter_TriggerSingle(); 会导致按住时每毫秒触发一次，
  *         一秒几百发，供弹机构直接废掉
  */
void Shooter_TriggerSingle(void)
{
	if (s_cont) return;						/* 连发中忽略单发 */
	s_single_ms = SINGLE_SHOT_MS;
}

void Shooter_CancelSingle(void)
{
	s_single_ms = 0;
}

uint8_t Shooter_IsFiring(void)
{
	return s_on;
}

/**
  * @brief  发射控制更新，1ms 调用
  * @note   通电瞬间走 SHOOT_SOFT_MS 的软件 PWM 斜坡（500Hz），
  *         把 6A 的启动冲击摊平，之后全开
  */
void Shooter_Update(void)
{
	uint8_t  want;

#if !SHOOT_USE_RELAY
	uint32_t t;
	uint8_t  level;
#endif

	want = (s_cont || s_single_ms > 0) ? 1 : 0;

	if (s_single_ms > 0) s_single_ms--;

	if (!want)
	{
		s_on = 0;
		Shooter_Write(0);
		return;
	}

	if (!s_on)								/* 上升沿：记下开始时刻 */
	{
		s_on       = 1;
#if !SHOOT_USE_RELAY
		s_start_ms = now_ms();
#endif
	}

#if SHOOT_USE_RELAY
	/* 机械继电器不能高频通断：整个单发周期保持吸合 */
	Shooter_Write(1);
#else
	t = now_ms() - s_start_ms;

	if (t < SHOOT_SOFT_MS)
	{
		/* 软启动：占空比从 0/10 逐级抬到 9/10 */
		level   = (uint8_t)(t * 10u / SHOOT_SOFT_MS);
		s_phase = (uint8_t)((s_phase + 1u) % 10u);

		Shooter_Write(s_phase < level);
	}
	else
	{
		Shooter_Write(1);						/* 全速 */
	}
#endif
}
