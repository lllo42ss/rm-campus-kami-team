#include "stm32f10x.h"
#include "Tick.h"

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	uint32_t i;

	/* 粗略微秒延时：72MHz 下一次循环约 6~12 个周期 */
	for (i = 0; i < xus * 6; i++)
	{
		__NOP();
	}
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	uint32_t start = now_ms();

	/* 基于系统时基等待，用减法判断，计数器溢出也不会出错 */
	while ((now_ms() - start) < xms);
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	Delay_ms(xs * 1000);
}
