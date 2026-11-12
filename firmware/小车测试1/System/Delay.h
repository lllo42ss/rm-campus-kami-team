#ifndef __DELAY_H
#define __DELAY_H

void Delay_us(uint32_t us);	// 微秒延时，us:0~233015
void Delay_ms(uint32_t ms);	// 毫秒延时，ms:0~4294967295
void Delay_s(uint32_t s);	// 秒延时，s:0~4294967295

#endif
