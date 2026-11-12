#include "Motor.h"

/* ================= 引脚表 ================= */
/* 顺序 A / B / C / D
 *
 * 本副本按「轮子测试接线表」配置：
 *   PWM  : A=PA1(TIM2_CH2)  B=PA0(TIM2_CH1)  C=PA2(CH3)  D=PA3(CH4)
 *          注意 A/B 和常规写法是反的，因为这版线是这么接的
 *   方向 : A=PB12/PB13  B=PB14/PB15  C=PB0/PB1  D=PB4/PB5
 */
static GPIO_TypeDef * const in1_port[4] = {GPIOB, GPIOB, GPIOB, GPIOB};
static const uint16_t        in1_pin [4] = {GPIO_Pin_12, GPIO_Pin_14, GPIO_Pin_0, GPIO_Pin_4};
static GPIO_TypeDef * const in2_port[4] = {GPIOB, GPIOB, GPIOB, GPIOB};
static const uint16_t        in2_pin [4] = {GPIO_Pin_13, GPIO_Pin_15, GPIO_Pin_1, GPIO_Pin_5};

/* 某个轮子转反了就把对应位置改成 1，不用去拆车改线
 *
 * 实车电机位置：
 *      A = 右后    B = 左后    C = 左前    D = 右前
 *
 * 先全部设为 0 做基准。按 ↑ 前进时，如果某个轮子把车往后推，
 * 就把那一位改成 1。
 */
static const uint8_t motor_invert[4] = {
	0,	/* A 右后 */
	0,	/* B 左后 */
	1,	/* C 左前：实车唯一反转的轮子 */
	0	/* D 右前 */
};

/* TB6612 的 PWM 周期值：72MHz / 3600 = 20kHz */
#define PWM_PERIOD   3600

/**
  * @brief  电机初始化
  * @param  无
  * @retval 无
  * @note   PWM   : TIM2 的 4 个通道 -> PA0/PA1/PA2/PA3，20kHz
  *         方向  : 8 个普通推挽输出
  *         使能  : STBY = PA4，初始为低（不使能，安全）
  *         注意  : 用到 PB4（默认是 JTAG 的 NJTRST），必须先关掉 JTAG 才能当普通 IO 用；
  *                 这里只关 JTAG、保留 SWD，所以下载和调试不受影响
  */
void Motor_Init(void)
{
	GPIO_InitTypeDef        GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef       TIM_OCInitStructure;
	uint8_t i;

	/* 开启时钟：TIM2 在 APB1，GPIO 在 APB2，AFIO 用于引脚重映射 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

	/* ★释放 PB4：只关 JTAG，保留 SWD（千万不要用 SWJ_Disable，那会连下载也一起关掉） */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	/* ---------------- 1. PWM 引脚：PA0~PA3，复用推挽 ---------------- */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* ---------------- 2. TIM2 时基：20kHz ---------------- */
	TIM_InternalClockConfig(TIM2);						/* 选择内部时钟 */
	TIM_TimeBaseStructure.TIM_Prescaler     = 1 - 1;		/* 72MHz */
	TIM_TimeBaseStructure.TIM_Period        = PWM_PERIOD - 1;	/* 72M/3600 = 20kHz */
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	/* ---------------- 3. 4 个 PWM 通道 ---------------- */
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse       = 0;			/* 初始占空比 0，电机不转 */
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);			/* CH1 -> PA0 -> PWMA */
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);			/* CH2 -> PA1 -> PWMB */
	TIM_OC3Init(TIM2, &TIM_OCInitStructure);			/* CH3 -> PA2 -> PWMC */
	TIM_OC4Init(TIM2, &TIM_OCInitStructure);			/* CH4 -> PA3 -> PWMD */

	TIM_Cmd(TIM2, ENABLE);

	/* ---------------- 4. 方向引脚：普通推挽输出 ---------------- */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	for (i = 0; i < 4; i++)
	{
		GPIO_InitStructure.GPIO_Pin = in1_pin[i];
		GPIO_Init(in1_port[i], &GPIO_InitStructure);
		GPIO_InitStructure.GPIO_Pin = in2_pin[i];
		GPIO_Init(in2_port[i], &GPIO_InitStructure);
	}

	/* ---------------- 5. STBY 使能脚 ---------------- */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	Motor_Enable(0);			/* 上电默认不使能 */
	Motor_Brake();
}

/**
  * @brief  驱动使能（对应 TB6612 模块的 STBY 脚）
  * @param  on 0 = 关闭驱动（电机全部停转并断电），1 = 允许驱动
  * @retval 无
  * @note   这个脚不拉高，PWM 给多少电机都不会转——排查"电机不转"时先查它
  */
void Motor_Enable(uint8_t on)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, on ? Bit_SET : Bit_RESET);
}

/**
  * @brief  设置某个电机的转向（不改变转速）
  * @param  ch 电机编号 0~3
  * @param  forward 1 = 正转，0 = 反转
  * @retval 无
  */
void Motor_SetDir(uint8_t ch, uint8_t forward)
{
	if (ch > 3) return;
	if (motor_invert[ch]) forward = !forward;

	if (forward)
	{
		GPIO_WriteBit(in1_port[ch], in1_pin[ch], Bit_SET);
		GPIO_WriteBit(in2_port[ch], in2_pin[ch], Bit_RESET);
	}
	else
	{
		GPIO_WriteBit(in1_port[ch], in1_pin[ch], Bit_RESET);
		GPIO_WriteBit(in2_port[ch], in2_pin[ch], Bit_SET);
	}
}

/**
  * @brief  设置某个电机的转速
  * @param  ch   电机编号 0~3
  * @param  duty 占空比，范围 -1.0 ~ +1.0，正数正转、负数反转
  * @retval 无
  */
void Motor_Set(uint8_t ch, float duty)
{
	uint16_t ccr;

	if (ch > 3) return;

	/* 占空比限幅，防止外面算出来的数超范围 */
	if (duty >  1.0f) duty =  1.0f;
	if (duty < -1.0f) duty = -1.0f;

	/* 先按正负定方向 */
	Motor_SetDir(ch, (duty >= 0.0f) ? 1 : 0);
	if (duty < 0.0f) duty = -duty;

	ccr = (uint16_t)(duty * PWM_PERIOD);

	switch (ch)
	{
		case 0: TIM_SetCompare2(TIM2, ccr); break;		/* PWMA -> PA1 (TIM2_CH2) */
		case 1: TIM_SetCompare1(TIM2, ccr); break;		/* PWMB -> PA0 (TIM2_CH1) */
		case 2: TIM_SetCompare3(TIM2, ccr); break;		/* PWMC */
		case 3: TIM_SetCompare4(TIM2, ccr); break;		/* PWMD */
	}
}

/**
  * @brief  一次设置 4 个电机
  * @param  a,b,c,d 对应 4 个电机的占空比
  * @retval 无
  */
void Motor_SetAll(float a, float b, float c, float d)
{
	Motor_Set(0, a);
	Motor_Set(1, b);
	Motor_Set(2, c);
	Motor_Set(3, d);
}

/**
  * @brief  刹车：4 路 PWM 全部置 0
  * @param  无
  * @retval 无
  * @note   掉线、急停、状态切换时调用
  */
void Motor_Brake(void)
{
	TIM_SetCompare1(TIM2, 0);
	TIM_SetCompare2(TIM2, 0);
	TIM_SetCompare3(TIM2, 0);
	TIM_SetCompare4(TIM2, 0);
}
