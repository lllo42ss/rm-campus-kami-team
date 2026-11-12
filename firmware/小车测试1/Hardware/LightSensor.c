#include "stm32f10x.h"                  // Device header

/**
  * @brief  光敏传感器初始化（LightSensor = PB13）
  * @param  无
  * @retval 无
  * @note   上拉输入模式下：光照强时光敏电阻阻值变小，PB13被拉低（读0）；
  *         光照弱/遮光时阻值变大，PB13被上拉为高电平（读1）
  */
void LightSensor_Init(void)
{
	/* 开启GPIOB的时钟 */
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);

	/* 配置PB13为上拉输入模式，用于读取光敏传感器的电平 */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  获取光敏传感器状态
  * @param  无
  * @retval 1 = 光照弱（遮光，PB13为高电平），0 = 光照强（PB13为低电平）
  */
uint8_t LightSensor_Get(void)
{
	/* 读取PB13的输入电平并返回 */
	return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);
}
