#include "Tick.h"

static volatile uint32_t s_ms = 0;

/**
  * @brief  初始化 1ms 系统时基
  * @param  无
  * @retval 无
  * @note   配置 SysTick 每 1ms 产生一次中断。
  *         中断服务函数在 stm32f10x_it.c 里，内容只有一句 Tick_Inc()
  */
void Tick_Init(void)
{
	/* SystemCoreClock 在 system_stm32f10x.c 里，72MHz 时 = 72000000 */
	if (SysTick_Config(SystemCoreClock / 1000))
	{
		/* 配置失败（重装值太大），停在这里不要往下跑 */
		while (1);
	}
}

/**
  * @brief  毫秒计数 +1，由 SysTick_Handler 调用
  * @param  无
  * @retval 无
  */
void Tick_Inc(void)
{
	s_ms++;
}

/**
  * @brief  读取当前毫秒数
  * @param  无
  * @retval 从开机到现在的毫秒数
  * @note   判断超时请用 now_ms() - start > 100 这种写法，
  *         不要写 now_ms() > start + 100，后者在计数器溢出时会出错
  */
uint32_t now_ms(void)
{
	return s_ms;
}

/**
  * @brief  读取当前微秒数
  * @param  无
  * @retval 从开机到现在的微秒数
  * @note   SysTick 是递减计数器（每 1ms 从 71999 数到 0），
  *         所以"本毫秒已经过了多少拍" = LOAD - VAL。
  *         读 s_ms 和 VAL 之间可能被中断打断，所以读两次确认没变。
  *         用在遥控接收机测脉宽（脉宽只有 1000~2000us，必须有微秒精度）
  */
uint32_t now_us(void)
{
	uint32_t ms1, ms2, val;

	do
	{
		ms1 = s_ms;
		val = SysTick->VAL;
		ms2 = s_ms;
	} while (ms1 != ms2);

	return ms1 * 1000u + (SysTick->LOAD - val) / 72u;
}
