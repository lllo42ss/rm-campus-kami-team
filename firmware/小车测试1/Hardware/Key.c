#include "stm32f10x.h"   // Device header
#include "Delay.h"

/**
  * @brief  按键初始化（Key1 = PB1，Key2 = PB11）
  * @param  无
  * @retval 无
  */
void Key_Init(void)
{
	/* 开启GPIOB的时钟 */
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);

	/* 配置PB1、PB11为上拉输入模式（按键另一端接地，按下为低电平） */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU ;
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  获取按下的按键键码（阻塞式，含消抖）
  * @param  无
  * @retval 按键键码：1 = Key1(PB1)按下，2 = Key2(PB11)按下，0 = 无按键按下
  * @note   内部流程：检测到低电平后延时20ms消抖，等待按键松开后
  *         再延时20ms消除松抖动，最后返回对应键码。该函数是阻塞式的，
  *         按住不放时程序会停在while循环中等待松手
  */
uint8_t Key_GetNum(void)
{
	uint8_t KeyNum = 0;

	/* Key1检测：PB1为低电平说明按下 */
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
	{
		Delay_ms(20);										/* 按下消抖 */
		while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0);	/* 等待松手 */
		Delay_ms (20);										/* 松手消抖 */
		KeyNum = 1;
	}

	/* Key2检测：PB11为低电平说明按下 */
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
	{
		Delay_ms(20);											/* 按下消抖 */
		while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0);	/* 等待松手 */
		Delay_ms (20);											/* 松手消抖 */
		KeyNum = 2;
	}

	return KeyNum;
}
