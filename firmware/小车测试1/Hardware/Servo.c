#include "Servo.h"
#include "Config.h"

/* 定时器时钟 1MHz（72MHz / 72），周期 20000 个计数 = 20ms = 50Hz
 * 所以 1 个计数 = 1 微秒，脉宽直接写 500 ~ 2500 就是 0.5ms ~ 2.5ms */
#define SERVO_PSC        (72 - 1)
#define SERVO_ARR        (20000 - 1)
#define SERVO_MIN_US     500
#define SERVO_MAX_US     2500

/* ==================== 底层舵机 ==================== */

/**
  * @brief  舵机 PWM 初始化（PA8 = TIM1_CH1）
  * @note   TIM1 是高级定时器，少了 TIM_CtrlPWMOutputs 这一句就没有波形，
  *         这是最高频的坑
  */
void Servo_Init(void)
{
	GPIO_InitTypeDef        GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef       TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIM_InternalClockConfig(TIM1);
	TIM_TimeBaseStructure.TIM_Prescaler         = SERVO_PSC;
	TIM_TimeBaseStructure.TIM_Period            = SERVO_ARR;
	TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse       = 0;
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);

	TIM_CtrlPWMOutputs(TIM1, ENABLE);		/* ★ 高级定时器必须加 */
	TIM_Cmd(TIM1, ENABLE);
}

/**
  * @brief  设置舵机角度
  * @param  deg 0 ~ 180 度（超出自动限幅）
  */
void Servo_SetAngle(float deg)
{
	uint16_t us;

	if (deg <   0.0f) deg =   0.0f;
	if (deg > 180.0f) deg = 180.0f;

	us = (uint16_t)(SERVO_MIN_US + deg * (SERVO_MAX_US - SERVO_MIN_US) / 180.0f);
	TIM_SetCompare1(TIM1, us);
}

/**
  * @brief  关掉舵机输出
  * @note   失能时用，避免舵机被一个奇怪的占空比推着走
  */
void Servo_Disable(void)
{
	TIM_SetCompare1(TIM1, 0);
}

/* ==================== 云台俯仰 ==================== */

static uint8_t s_enable = 0;
static float   s_angle  = PITCH_MID;		/* 当前角度 */
static float   s_speed  = 0.0f;				/* 鼠标给的角速度 */
static float   s_target = PITCH_MID;		/* 考察模式给的目标角度 */
static uint8_t s_use_target = 0;			/* 1 = 用目标角度，0 = 用角速度 */

void Gimbal_Init(void)
{
	Servo_Init();
	s_angle  = PITCH_MID;
	s_target = PITCH_MID;
	s_speed  = 0.0f;
	Servo_SetAngle(s_angle);
}

void Gimbal_SetEnable(uint8_t on)
{
	if (s_enable == on) return;
	s_enable = on;

	if (on)
	{
		s_use_target = 0;
		s_speed = 0.0f;
		Servo_SetAngle(s_angle);
	}
	else
	{
		Servo_Disable();
	}
}

/**
  * @brief  设定俯仰角速度（鼠标控制用）
  * @param  dps 度/秒，正 = 枪口往上抬
  */
void Gimbal_SetSpeed(float dps)
{
	s_use_target = 0;
	s_speed = dps;
}

/**
  * @brief  直接指定枪口角度（考察模式用）
  * @param  deg 目标角度，内部限幅到 PITCH_MIN ~ PITCH_MAX
  */
void Gimbal_SetAngle(float deg)
{
	if (deg < PITCH_MIN) deg = PITCH_MIN;
	if (deg > PITCH_MAX) deg = PITCH_MAX;
	s_target     = deg;
	s_use_target = 1;
}

float Gimbal_GetAngle(void)
{
	return s_angle;
}

/**
  * @brief  云台更新，1ms 调用
  * @param  dt 距上次调用的秒数
  * @note   两种模式：角速度模式（鼠标推）和目标角度模式（考察模式）。
  *         限位在这里做，不做限位会烧舵机
  */
void Gimbal_Update(float dt)
{
	float step;

	if (!s_enable) return;

	if (s_use_target)
	{
		/* 目标角度模式：以固定速度逼近目标，避免瞬间甩头 */
		step = PITCH_SPEED_MAX * dt;
		if (s_angle < s_target)
		{
			s_angle += step;
			if (s_angle > s_target) s_angle = s_target;
		}
		else if (s_angle > s_target)
		{
			s_angle -= step;
			if (s_angle < s_target) s_angle = s_target;
		}
	}
	else
	{
		/* 角速度模式 */
		s_angle += s_speed * dt;
	}

	/* 硬限位：这一步不能省 */
	if (s_angle < PITCH_MIN) s_angle = PITCH_MIN;
	if (s_angle > PITCH_MAX) s_angle = PITCH_MAX;

	Servo_SetAngle(s_angle);
}
