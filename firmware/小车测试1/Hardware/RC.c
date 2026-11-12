#include "RC.h"
#include "Config.h"
#include "Tick.h"

#if RC_ENABLE

/* 4 个通道的引脚：ch1~ch4 对应 索引 0~3 */
static GPIO_TypeDef * const s_port[4] = {GPIOB, GPIOB, GPIOA, GPIOA};
static const uint16_t       s_pin [4] = {GPIO_Pin_0, GPIO_Pin_3, GPIO_Pin_12, GPIO_Pin_15};

static uint32_t s_rise[4];			/* 上升沿时刻 */
static uint16_t s_width[4];			/* 上一次测到的脉宽 us */
static uint32_t s_last_us = 0;		/* 最后一次收到有效脉冲的时刻 */
static uint8_t  s_connected = 0;

/**
  * @brief  边沿处理
  * @param  ch    通道号 0~3
  * @param  level 当前引脚电平：1 = 上升沿（脉冲开始），0 = 下降沿（脉冲结束）
  */
static void RC_OnEdge(uint8_t ch, uint8_t level)
{
	uint32_t t = now_us();
	uint32_t w;

	if (level)
	{
		s_rise[ch] = t;				/* 记下脉冲起点 */
	}
	else
	{
		w = t - s_rise[ch];			/* 脉冲宽度 */

		/* 只接受 500~2500us 的脉宽，把毛刺和噪声滤掉 */
		if (w > 500u && w < 2500u)
		{
			s_width[ch] = (uint16_t)w;
			s_last_us   = t;
		}
	}
}

/**
  * @brief  遥控接收机初始化
  * @note   用外部中断（EXTI）在上升沿和下降沿各进一次，
  *         时间差就是脉宽。四个脚都是"任意边沿触发"
  */
void RC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_AFIO, ENABLE);

	/* 用到 PB3、PA15（JTAG 复用脚），必须关掉 JTAG，保留 SWD */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	/* 下拉输入：接收机没通电时引脚保持低电平，不会产生假边沿 */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = s_pin[0]; GPIO_Init(s_port[0], &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = s_pin[1]; GPIO_Init(s_port[1], &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = s_pin[2]; GPIO_Init(s_port[2], &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = s_pin[3]; GPIO_Init(s_port[3], &GPIO_InitStructure);

	/* 四个脚都配成"上升沿 + 下降沿"中断 */
	EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
	EXTI_InitStructure.EXTI_Line = EXTI_Line0;  EXTI_Init(&EXTI_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource3);
	EXTI_InitStructure.EXTI_Line = EXTI_Line3;  EXTI_Init(&EXTI_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource12);
	EXTI_InitStructure.EXTI_Line = EXTI_Line12; EXTI_Init(&EXTI_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource15);
	EXTI_InitStructure.EXTI_Line = EXTI_Line15; EXTI_Init(&EXTI_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;

	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;     NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;     NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn; NVIC_Init(&NVIC_InitStructure);
}

/* ---------------- 三个中断服务函数（函数名写死在启动文件里） ---------------- */

void EXTI0_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line0) != RESET)
	{
		RC_OnEdge(0, (uint8_t)GPIO_ReadInputDataBit(s_port[0], s_pin[0]));
		EXTI_ClearITPendingBit(EXTI_Line0);
	}
}

void EXTI3_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line3) != RESET)
	{
		RC_OnEdge(1, (uint8_t)GPIO_ReadInputDataBit(s_port[1], s_pin[1]));
		EXTI_ClearITPendingBit(EXTI_Line3);
	}
}

/* PA12 和 PA15 共用这一个中断入口，要分别判断是哪一路 */
void EXTI15_10_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line12) != RESET)
	{
		RC_OnEdge(2, (uint8_t)GPIO_ReadInputDataBit(s_port[2], s_pin[2]));
		EXTI_ClearITPendingBit(EXTI_Line12);
	}

	if (EXTI_GetITStatus(EXTI_Line15) != RESET)
	{
		RC_OnEdge(3, (uint8_t)GPIO_ReadInputDataBit(s_port[3], s_pin[3]));
		EXTI_ClearITPendingBit(EXTI_Line15);
	}
}

/**
  * @brief  失控判断，1ms 调用
  * @note   遥控器关机 / 超出距离 / 接收机掉电时，
  *         50ms 收不到脉冲就认为失控，上层会据此停车
  */
void RC_Update(void)
{
	if (s_last_us == 0)
	{
		s_connected = 0;
		return;
	}

	if ((now_us() - s_last_us) > (RC_TIMEOUT_MS * 1000u))
		s_connected = 0;
	else
		s_connected = 1;
}

float RC_GetUs(uint8_t ch)
{
	if (ch > 3) return 0.0f;
	return (float)s_width[ch];
}

/**
  * @brief  把脉宽归一化到 -1.0 ~ +1.0
  * @note   1500us 是中位；按 ±500us 满量程换算，再打死区
  */
float RC_GetNorm(uint8_t ch)
{
	float us, v;

	if (ch > 3) return 0.0f;
	if (!s_connected) return 0.0f;

	us = (float)s_width[ch];
	if (us < 800.0f) return 0.0f;			/* 这个通道还没收到过 */

	v = (us - 1500.0f) / 500.0f;

	if (v >  1.0f) v =  1.0f;
	if (v < -1.0f) v = -1.0f;

	if (v > -RC_DEADZONE && v < RC_DEADZONE) v = 0.0f;

	return v;
}

uint8_t RC_IsConnected(void)
{
	return s_connected;
}

#else	/* RC_ENABLE == 0：全部做成空函数，不用遥控器时一点开销都没有 */

void    RC_Init(void)                { }
void    RC_Update(void)              { }
float   RC_GetUs(uint8_t ch)         { (void)ch; return 0.0f; }
float   RC_GetNorm(uint8_t ch)       { (void)ch; return 0.0f; }
uint8_t RC_IsConnected(void)         { return 0; }

#endif
