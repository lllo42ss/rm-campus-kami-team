#include "stm32f10x.h"                  // Device header

/**
  * @brief  LED初始化（LED1 = PA1，LED2 = PA2，低电平点亮）
  * @param  无
  * @retval 无
  */
void LED_Init(void)
{
	/* 开启GPIOA的时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE );

	/* 配置PA1、PA2为推挽输出模式 */
	GPIO_InitTypeDef  GPIO_InitStracture;
	GPIO_InitStracture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStracture.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStracture.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStracture);

	/* 初始置高电平，两个LED默认熄灭 */
	GPIO_SetBits(GPIOA, GPIO_Pin_1 | GPIO_Pin_2);

}

/**
  * @brief  点亮LED1（PA1输出低电平）
  * @param  无
  * @retval 无
  */
void LED1_ON(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_1);
}

/**
  * @brief  熄灭LED1（PA1输出高电平）
  * @param  无
  * @retval 无
  */
void LED1_OFF(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_1);
}

/**
  * @brief  LED1状态翻转（亮变灭、灭变亮）
  * @param  无
  * @retval 无
  */
void LED1_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 0)	/* 当前为低电平(亮) */
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_1);					/* 置高电平，熄灭 */
	}
	else												/* 当前为高电平(灭) */
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_1);				/* 置低电平，点亮 */
	}
}

/**
  * @brief  点亮LED2（PA2输出低电平）
  * @param  无
  * @retval 无
  */
void LED2_ON(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_2);
}

/**
  * @brief  熄灭LED2（PA2输出高电平）
  * @param  无
  * @retval 无
  */
void LED2_OFF(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_2);
}

/**
  * @brief  LED2状态翻转（亮变灭、灭变亮）
  * @param  无
  * @retval 无
  */
void LED2_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_2) == 0)	/* 当前为低电平(亮) */
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_2);					/* 置高电平，熄灭 */
	}
	else												/* 当前为高电平(灭) */
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_2);				/* 置低电平，点亮 */
	}
}
