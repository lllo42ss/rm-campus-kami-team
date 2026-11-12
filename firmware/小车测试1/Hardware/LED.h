#ifndef __LED_H
#define __LED_H

void LED_Init(void);		// LED初始化（LED1=PA1，LED2=PA2，低电平点亮）
void LED1_ON(void);		// 点亮LED1
void LED1_OFF(void);		// 熄灭LED1
void LED1_Turn(void);	// LED1状态翻转

void LED2_ON(void);		// 点亮LED2
void LED2_OFF(void);		// 熄灭LED2
void LED2_Turn(void);	// LED2状态翻转

#endif
