#include "Referee.h"
#include "Config.h"
#include "Tick.h"
#include <string.h>

Referee_t kb;

static uint8_t rx_buf[KB_LEN];
static uint8_t rx_cnt = 0;

/**
  * @brief  裁判系统串口初始化
  * @param  无
  * @retval 无
  * @note   用 USART3：PB10 = TX，PB11 = RX，115200-8-N-1
  *         用接收中断逐字节接收，收到 10 个字节解析一次
  */
void Referee_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/* PB10 = USART3_TX，复用推挽 */
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* PB11 = USART3_RX，上拉输入 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate            = 115200;
	USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits            = USART_StopBits_1;
	USART_InitStructure.USART_Parity              = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	/* 打开接收中断 */
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART3, ENABLE);

	memset(&kb, 0, sizeof(kb));
}

/**
  * @brief  把 2 个字节拼成有符号 16 位（小端：低字节在前）
  */
static int16_t Referee_ToInt16(const uint8_t *p)
{
	return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
  * @brief  解析 10 字节键鼠包
  * @param  d 指向 10 字节数据
  * @note   字节分配（低字节在前）：
  *           0~1  鼠标 X     2~3  鼠标 Y     4~5  滚轮
  *           6    左键 bit0  7    右键 bit0
  *           8    键盘位图   bit0=W bit1=S bit2=A bit3=D
  *                            bit4=Q bit5=E bit6=Shift bit7=Ctrl
  *           9    链路状态   bit0 = 1 表示已连接
  */
static void Referee_Parse(const uint8_t *d)
{
	uint8_t i;

	/* 先把原始字节存下来，方便在 OLED 上核对协议 */
	for (i = 0; i < KB_LEN; i++) kb.raw[i] = d[i];

	kb.mouse_x = Referee_ToInt16(&d[0]) / 32768.0f;
	kb.mouse_y = Referee_ToInt16(&d[2]) / 32768.0f;
	kb.mouse_z = Referee_ToInt16(&d[4]) / 32768.0f;

	kb.left_button  = d[6] & 0x01;
	kb.right_button = d[7] & 0x01;

	kb.w     = (d[8] >> 0) & 0x01;
	kb.s     = (d[8] >> 1) & 0x01;
	kb.a     = (d[8] >> 2) & 0x01;
	kb.d     = (d[8] >> 3) & 0x01;
	kb.q     = (d[8] >> 4) & 0x01;
	kb.e     = (d[8] >> 5) & 0x01;
	kb.shift = (d[8] >> 6) & 0x01;
	kb.ctrl  = (d[8] >> 7) & 0x01;

	kb.connected  = d[9] & 0x01;
	kb.last_rx_ms = now_ms();
}

/**
  * @brief  收到一个字节
  * @param  b 收到的字节
  * @note   定长 10 字节拆包：收满就解析，然后从头再来
  */
void Referee_OnByte(uint8_t b)
{
	if (rx_cnt < KB_LEN) rx_buf[rx_cnt++] = b;

	if (rx_cnt >= KB_LEN)
	{
		Referee_Parse(rx_buf);
		rx_cnt = 0;
	}
}

/**
  * @brief  串口接收中断
  * @note   函数名必须是 USART3_IRQHandler，这是启动文件里写死的
  */
void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		Referee_OnByte((uint8_t)USART_ReceiveData(USART3));
	}
}

/**
  * @brief  超时管理，1ms 调用一次
  * @param  无
  * @retval 无
  * @note   100ms 收不到新包就认为掉线：
  *         所有按键清零 + connected = 0，底盘会自然减速停下
  */
void Referee_Update(void)
{
	if (kb.last_rx_ms == 0) return;			/* 从来没收到过 */

	if ((now_ms() - kb.last_rx_ms) > KB_TIMEOUT_MS)
	{
		memset(&kb, 0, sizeof(kb));
	}
}
