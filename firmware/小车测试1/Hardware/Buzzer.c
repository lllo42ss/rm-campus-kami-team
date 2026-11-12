#include "stm32f10x.h"                  // Device header

/**
  * @brief  蜂鸣器初始化（Buzzer = PB12）
  * @param  无
  * @retval 无
  */
void Buzzer_Init(void)
{
	/* 开启GPIOB的时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE );

	/* 配置PB12为推挽输出模式 */
	GPIO_InitTypeDef  GPIO_InitStracture;
	GPIO_InitStracture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStracture.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStracture.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStracture);

	/* 初始置高电平，蜂鸣器默认不响（低电平触发） */
	GPIO_SetBits(GPIOB, GPIO_Pin_12);

}

/**
  * @brief  蜂鸣器响（PB12输出低电平）
  * @param  无
  * @retval 无
  */
void Buzzer_ON(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
}

/**
  * @brief  蜂鸣器停（PB12输出高电平）
  * @param  无
  * @retval 无
  */
void Buzzer_OFF(void)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

/**
  * @brief  蜂鸣器状态翻转（响变停、停变响）
  * @param  无
  * @retval 无
  */
void Buzzer_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_12) == 0)	/* 当前为低电平(响) */
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_12);					/* 置高电平，停止发声 */
	}
	else												/* 当前为高电平(停) */
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_12);				/* 置低电平，开始发声 */
	}
}

