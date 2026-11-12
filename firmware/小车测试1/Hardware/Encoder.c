#include "Encoder.h"
#include "Config.h"

#define ENC_PI      3.14159265f
#define ENC_SQR2_2  0.70710678f

static TIM_TypeDef * const s_tim[2] = {TIM3, TIM4};

/* 编码器方向统一：
 *   Encoder 0 -> C 左前轮，保持正方向；
 *   Encoder 1 -> D 右前轮，保持正方向。
 * 如果以后把编码器换到别的轮子，需要重新确认这两个符号。 */
static const float s_direction[2] = {1.0f, 1.0f};

static int16_t s_last[2];			/* 上次读到的计数 */
static float   s_rpm[2];			/* 轮子转速 */
static float   s_travel[2];			/* 轮子累计行程 m */
static float   s_car_dist;			/* 车体累计位移 m */
static float   s_car_angle;			/* 车体累计转角 度 */

/**
  * @brief  编码器初始化
  * @note   TIM3 <- PA6(A) / PA7(B)
  *         TIM4 <- PB6(A) / PB7(B)
  *         用 TI12 模式（A、B 两相都计数，四倍频）
  */
void Encoder_Init(void)
{
	GPIO_InitTypeDef        GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef       TIM_ICInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

	/* 编码器多为开漏输出，用上拉输入 */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_Init(GPIOA, &GPIO_InitStructure);		/* TIM3_CH1/CH2 */
	GPIO_Init(GPIOB, &GPIO_InitStructure);		/* TIM4_CH1/CH2 */

	/* 16 位满量程，让计数器自然翻转，靠增量法处理溢出 */
	TIM_TimeBaseStructure.TIM_Prescaler         = 1 - 1;
	TIM_TimeBaseStructure.TIM_Period            = 65536 - 1;
	TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12,
	                           TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12,
	                           TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

	/* 输入滤波：编码器脉冲很密（68000 计数/圈），滤太狠会丢计数 */
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 0x4;
	TIM_ICInitStructure.TIM_Channel  = TIM_Channel_1; TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel  = TIM_Channel_2; TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel  = TIM_Channel_1; TIM_ICInit(TIM4, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel  = TIM_Channel_2; TIM_ICInit(TIM4, &TIM_ICInitStructure);

	TIM_SetCounter(TIM3, 0);
	TIM_SetCounter(TIM4, 0);
	TIM_Cmd(TIM3, ENABLE);
	TIM_Cmd(TIM4, ENABLE);

	Encoder_ResetOdom();
}

/**
  * @brief  编码器更新，1ms 调用
  * @param  dt 距上次调用的秒数
  * @note   用无符号差值再转 int16 读增量，16 位溢出会被自动处理。
  *         必须固定周期调用，间隔越长增量越大、越容易真的溢出
  */
void Encoder_Update(float dt)
{
	uint8_t i;
	float   d[2];

	for (i = 0; i < 2; i++)
	{
		int16_t now = (int16_t)TIM_GetCounter(s_tim[i]);
		int16_t cnt = (int16_t)(now - s_last[i]);	/* 本次增量（计数） */
		float dir_cnt = (float)cnt * s_direction[i];

		s_last[i] = now;

		/* 计数 -> 输出轴圈数 -> 轮面行程 */
		d[i] = (dir_cnt / (float)ENC_CNT_PER_REV) * (2.0f * ENC_PI * WHEEL_R);
		s_travel[i] += d[i];

		if (dt > 0.0f)
		{
			s_rpm[i] = (dir_cnt / (float)ENC_CNT_PER_REV) / dt * 60.0f;
		}
	}

	/* ---- 车体位移：左右两侧轮子行程的平均值换算 ----
	 * ODOM_TRANS_K 理论 1.414（45 度麦轮），实测后按比例修正 */
	s_car_dist += (d[0] + d[1]) * 0.5f * ODOM_TRANS_K;

	/* ---- 车体转角：(左侧 - 右侧) / (2 * 底盘半径) 就是弧度 ----
	 * 纯旋转时左侧轮往前、右侧轮往后，这个公式不需要标定 */
	s_car_angle += (d[0] - d[1]) / (2.0f * CHASSIS_R) * (180.0f / ENC_PI) * ODOM_ROT_K;
}

float Encoder_GetRpm(uint8_t i)
{
	if (i > 1) return 0.0f;
	return s_rpm[i];
}

float Encoder_GetTravelM(uint8_t i)
{
	if (i > 1) return 0.0f;
	return s_travel[i];
}

/**
  * @brief  里程清零
  * @note   每次开始一个新动作（走 5m、转 720 度）之前调一次，
  *         否则会累加上一次的成绩
  */
void Encoder_ResetOdom(void)
{
	uint8_t i;

	for (i = 0; i < 2; i++)
	{
		s_travel[i] = 0.0f;
		s_rpm[i]    = 0.0f;
		s_last[i]   = (int16_t)TIM_GetCounter(s_tim[i]);
	}

	s_car_dist  = 0.0f;
	s_car_angle = 0.0f;
}

float Encoder_GetCarTravelM(void)
{
	return s_car_dist;
}

float Encoder_GetCarAngleDeg(void)
{
	return s_car_angle;
}
