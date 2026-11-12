#ifndef __KEY_H
#define __KEY_H

void Key_Init(void);		// 按键初始化（Key1=PB1，Key2=PB11）
uint8_t Key_GetNum(void);	// 获取按键键码：1=Key1按下，2=Key2按下，0=无按键

#endif
