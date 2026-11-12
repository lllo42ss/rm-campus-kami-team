#include "stm32f10x.h"                  // Device header
#include "OLED_Font.h"
#include "OLED.h"

/* 软件I2C引脚定义: PB8 = SCL, PB9 = SDA */
#define OLED_SCL_PORT     GPIOB
#define OLED_SCL_PIN      GPIO_Pin_8
#define OLED_SDA_PORT     GPIOB
#define OLED_SDA_PIN      GPIO_Pin_9
#define OLED_I2C_RCC      RCC_APB2Periph_GPIOB

/* OLED I2C地址(0x78为写地址, 0x79为读地址) */
#define OLED_ADDRESS      0x78

/*---------------------- 软件I2C底层 ----------------------*/

/**
  * @brief  内部简单软件延时（约几微秒，@72MHz），用于I2C时序
  * @param  无
  * @retval 无
  */
static void OLED_Delay(void)
{
	volatile uint32_t i;
	for (i = 0; i < 10; i++);
}

/**
  * @brief  设置I2C时钟线SCL的电平
  * @param  BitValue 写入的电平，取值：0 = 低电平，1 = 高电平
  * @retval 无
  */
static void OLED_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(OLED_SCL_PORT, OLED_SCL_PIN, (BitAction)BitValue);
	OLED_Delay();
}

/**
  * @brief  设置I2C数据线SDA的电平
  * @param  BitValue 写入的电平，取值：0 = 低电平，1 = 高电平
  * @retval 无
  */
static void OLED_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(OLED_SDA_PORT, OLED_SDA_PIN, (BitAction)BitValue);
	OLED_Delay();
}

/**
  * @brief  I2C起始信号（SCL高电平期间，SDA产生下降沿）
  * @param  无
  * @retval 无
  */
static void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C终止信号（SCL高电平期间，SDA产生上升沿）
  * @param  无
  * @retval 无
  */
static void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节（高位先行）
  * @param  Byte 要发送的字节，取值范围：0x00~0xFF
  * @retval 无
  * @note   发送完8位后额外产生一个时钟用于接收从机应答位（本驱动不检查应答）
  */
static void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(Byte & (0x80 >> i));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	/* 第9个时钟, 释放SDA接收应答 */
	OLED_W_SCL(1);
	OLED_W_SCL(0);
}

/*---------------------- OLED端口层 ----------------------*/

/**
  * @brief  向OLED写入一条命令
  * @param  Command 要写入的命令字节，取值范围：0x00~0xFF，具体命令见SSD1306指令表
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(OLED_ADDRESS);
	OLED_I2C_SendByte(0x00);        /* Control Byte: 命令 */
	OLED_I2C_SendByte(Command);
	OLED_I2C_Stop();
}

/**
  * @brief  向OLED写入一个字节的显示数据（写入GDDRAM）
  * @param  Data 要写入的数据字节，取值范围：0x00~0xFF，每一位对应一个像素点
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(OLED_ADDRESS);
	OLED_I2C_SendByte(0x40);        /* Control Byte: 数据 */
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  设置光标（写入位置）
  * @param  Y 光标所在页地址，范围：0~7（每页8行像素，共8页64行）
  * @param  X 光标所在列地址，范围：0~127（横向128个像素列）
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);                    /* 设置Y位置(页地址) */
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    /* 设置X位置高4位 */
	OLED_WriteCommand(0x00 | (X & 0x0F));           /* 设置X位置低4位 */
}

/*---------------------- OLED功能层 ----------------------*/

/**
  * @brief  清屏（将全部GDDRAM写0，屏幕熄灭所有像素）
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for (i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

/**
  * @brief  在指定位置显示一个ASCII字符（8x16点阵）
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的字符，范围：ASCII可打印字符' '~'~'（0x20~0x7E）
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);      /* 上半部分 */
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);  /* 下半部分 */
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
	}
}

/**
  * @brief  在指定位置开始显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，以'\0'结尾；
  *               起始列+字符串长度不要超过16，否则超出部分不显示
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  返回X的Y次方（内部乘方运算函数）
  * @param  X 底数
  * @param  Y 指数
  * @retval X的Y次方运算结果
  */
static uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  显示无符号十进制整数
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 显示数字的长度（位数），范围：1~10，不足位高位补0
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(Line, Column + i,
			Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  显示有符号十进制整数（自动在数字前显示'+'或'-'符号）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 符号列位置，范围：1~15（数字从Column+1开始显示）
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 显示数字的长度（位数，不含符号位），范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = (uint32_t)Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = (uint32_t)(-Number);
	}
	OLED_ShowNum(Line, Column + 1, Number1, Length);
}

/**
  * @brief  显示十六进制整数
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 显示十六进制的长度（位数），范围：1~8，不足位高位补0
  * @retval 无
  */
void OLED_ShowHex(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  显示二进制整数
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 显示二进制的长度（位数），范围：1~32，不足位高位补0
  * @retval 无
  */
void OLED_ShowBin(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(Line, Column + i,
			Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/*---------------------- OLED初始化 ----------------------*/

/**
  * @brief  OLED初始化（软件I2C：PB8-SCL，PB9-SDA，SSD1306控制器）
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;

	/* GPIO初始化: 软件I2C, 推挽输出(模块一般自带/外接上拉也可用开漏) */
	RCC_APB2PeriphClockCmd(OLED_I2C_RCC, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 上电延时, 等待OLED复位完成 */
	for (i = 0; i < 1000; i++)
	{
		for (j = 0; j < 1000; j++);
	}

	OLED_I2C_Start();       /* I2C总线测试性起始 */
	OLED_I2C_Stop();

	/* SSD1306初始化命令序列 */
	OLED_WriteCommand(0xAE);    /* 关闭显示 */

	OLED_WriteCommand(0xD5);    /* 设置显示时钟分频比/振荡器频率 */
	OLED_WriteCommand(0x80);

	OLED_WriteCommand(0xA8);    /* 设置多路复用率 */
	OLED_WriteCommand(0x3F);

	OLED_WriteCommand(0xD3);    /* 设置显示偏移 */
	OLED_WriteCommand(0x00);

	OLED_WriteCommand(0x40);    /* 设置显示开始行 */

	OLED_WriteCommand(0xA1);    /* 设置左右方向, 0xA1正常 0xA0左右反置 */

	OLED_WriteCommand(0xC8);    /* 设置上下方向, 0xC8正常 0xC0上下反置 */

	OLED_WriteCommand(0xDA);    /* 设置COM引脚硬件配置 */
	OLED_WriteCommand(0x12);

	OLED_WriteCommand(0x81);    /* 设置对比度控制 */
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);    /* 设置预充电周期 */
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);    /* 设置VCOMH取消选择级别 */
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);    /* 设置整个显示打开/关闭 */

	OLED_WriteCommand(0xA6);    /* 设置正常/反色显示 */

	OLED_WriteCommand(0x8D);    /* 设置充电泵 */
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);    /* 开启显示 */

	OLED_Clear();               /* 清屏 */
}
