#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"                  // Device header

void OLED_Init(void);												// OLED初始化（软件I2C: PB8-SCL, PB9-SDA）
void OLED_Clear(void);												// 清屏
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);		// 显示一个字符，Line:1~4, Column:1~16
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);	// 显示字符串，Line:1~4, Column:1~16
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);			// 显示无符号数，Length:显示位数
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);		// 显示有符号数，Length:显示位数(不含符号)
void OLED_ShowHex(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);			// 显示十六进制数，Length:1~8
void OLED_ShowBin(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);			// 显示二进制数，Length:1~32

#endif
