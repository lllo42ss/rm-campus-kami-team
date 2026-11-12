#include "PS2.h"
#include "Config.h"
#include "Tick.h"
#include "Delay.h"

/* ==================== 引脚定义（换脚只改这里） ==================== */
#define PS2_CLK_PORT    GPIOB
#define PS2_CLK_PIN     GPIO_Pin_3

#define PS2_CS_PORT     GPIOA
#define PS2_CS_PIN      GPIO_Pin_11

#define PS2_CMD_PORT    GPIOA
#define PS2_CMD_PIN     GPIO_Pin_12

#define PS2_DAT_PORT    GPIOA
#define PS2_DAT_PIN     GPIO_Pin_15

/* ==================== 时序与策略参数 ==================== */
/* 半个时钟周期的循环次数。24 次约 2us -> 约 250kHz
 * PS2 规范：时钟最高 500kHz，推荐 250kHz。
 * 如果还是不稳，先试 32（更慢）；如果确认接收器很快，可以试 16。 */
#define PS2_DELAY_LOOPS     32

/* 两次读取的最小间隔（ms）。太频繁会把便宜的兼容接收器问死 */
#define PS2_POLL_MS         10

/* 单次读取失败后重试次数 */
#define PS2_RETRY           3

/* 连续失败多少次才判定"掉线"（防止显示/灯光乱跳） */
#define PS2_FAIL_LIMIT      5

/* ==================== 内部状态 ==================== */
static uint32_t   s_last_poll_ms;
static uint8_t    s_connected;
static uint8_t    s_fail_streak;
static uint16_t   s_fail_total;
static PS2_Data_t s_cache;			/* 最后一次有效数据 */
static uint8_t    s_raw[9];			/* 最近一帧原始字节（诊断用） */
static uint8_t    s_loops = PS2_DELAY_LOOPS;	/* 当前使用的时钟延时档位 */
static uint32_t   s_last_scan_ms;
static uint8_t    s_edge;			/* 0 = 上升沿之后采样，1 = 下降沿之后采样 */

/* ==================== 时序延时 ==================== */
static void PS2_Delay(void)
{
	volatile uint8_t i;

	for (i = 0; i < s_loops; i++)
	{
		__NOP();
	}
}

/* ==================== 初始化 ==================== */
void PS2_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_AFIO, ENABLE);

	/* PA15 / PB3 是 JTAG 脚，必须关掉 JTAG（保留 SWD） */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	/* 先把 ODR 置高，再配成输出，避免上电瞬间出现低电平毛刺 */
	GPIO_SetBits(PS2_CLK_PORT, PS2_CLK_PIN);
	GPIO_SetBits(PS2_CS_PORT,  PS2_CS_PIN);
	GPIO_SetBits(PS2_CMD_PORT, PS2_CMD_PIN);

	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = PS2_CLK_PIN;
	GPIO_Init(PS2_CLK_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PS2_CS_PIN;
	GPIO_Init(PS2_CS_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PS2_CMD_PIN;
	GPIO_Init(PS2_CMD_PORT, &GPIO_InitStructure);

	/* DAT 上拉输入：内部上拉很弱（约 40k），
	 * 线长超过 10cm 时建议外部再加 4.7k 上拉到 3.3V */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin  = PS2_DAT_PIN;
	GPIO_Init(PS2_DAT_PORT, &GPIO_InitStructure);

	s_last_poll_ms = 0;
	s_connected    = 0;
	s_fail_streak  = 0;
	s_fail_total   = 0;

	/* 等接收器上电稳定（PS2 接收器上电约需 100~500ms） */
	Delay_ms(300);
}

/* ==================== 收发一个字节（LSB 先出） ==================== */
static uint8_t PS2_Transfer(uint8_t tx)
{
	uint8_t rx = 0;
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		/* 1. 准备命令位 */
		if (tx & 0x01) GPIO_SetBits(PS2_CMD_PORT, PS2_CMD_PIN);
		else           GPIO_ResetBits(PS2_CMD_PORT, PS2_CMD_PIN);
		tx >>= 1;

		PS2_Delay();

		/* 2. 时钟下降沿：接收器在这里采样 CMD、准备数据 */
		GPIO_ResetBits(PS2_CLK_PORT, PS2_CLK_PIN);
		PS2_Delay();

		if (s_edge)						/* 早采样：在低电平中间读 */
		{
			PS2_Delay();
			if (GPIO_ReadInputDataBit(PS2_DAT_PORT, PS2_DAT_PIN))
				rx |= (uint8_t)(1u << i);
		}

		/* 3. 时钟上升沿 */
		GPIO_SetBits(PS2_CLK_PORT, PS2_CLK_PIN);
		PS2_Delay();

		if (!s_edge)					/* 晚采样：在高电平中间读（PS2 标准做法） */
		{
			PS2_Delay();
			if (GPIO_ReadInputDataBit(PS2_DAT_PORT, PS2_DAT_PIN))
				rx |= (uint8_t)(1u << i);
		}
	}

	/* 字节之间留一点空隙，慢速接收器更容易跟上 */
	PS2_Delay();

	return rx;
}

/* ==================== 读一帧（9 字节） ==================== */
static uint8_t PS2_Frame(PS2_Data_t *out)
{
	/* 0x01 开始，0x42 请求数据，其余填 0 */
	uint8_t  cmd[9] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t  temp[9];
	uint8_t  i;
	uint32_t primask;

	/* 一帧约 300us，期间不能让 SysTick 等中断插进来，
	 * 否则时钟高/低电平被拉长，接收器会认错位 */
	primask = __get_PRIMASK();
	__disable_irq();

	GPIO_ResetBits(PS2_CS_PORT, PS2_CS_PIN);
	PS2_Delay();

	for (i = 0; i < 9; i++)
		temp[i] = PS2_Transfer(cmd[i]);

	/* 保存原始字节，方便在 OLED 上看接收器到底回了什么 */
	for (i = 0; i < 9; i++)
		s_raw[i] = temp[i];

	PS2_Delay();
	GPIO_SetBits(PS2_CS_PORT, PS2_CS_PIN);

	__set_PRIMASK(primask);

	/* ---------- 应答校验 ---------- */
	/* 第 2 字节：0x41 原装、0x73 部分兼容手柄；其他都视为没连上 */
	if (temp[1] != 0x41 && temp[1] != 0x73)
		return 0;

	/* 两根线都悬空时 DAT 被上拉成 0xFF，这种"假数据"要挡掉 */
	if (temp[0] == 0xFF && temp[1] == 0xFF)
		return 0;

	/* 两个按键字节同时为 0 表示所有按键都被判定为按下，这是异常帧。
	 * 只检查其中一个字节会把部分兼容接收器的正常数据误判成掉线。 */
	if (temp[3] == 0x00 && temp[4] == 0x00)
		return 0;

	out->buttons1 = temp[3];
	out->buttons2 = temp[4];

	out->rx = temp[5];
	out->ry = temp[6];
	out->lx = temp[7];
	out->ly = temp[8];

	/* 按键位（PS2 标准） */
	/* ★ PS2 按键是"低电平有效"：位 = 0 表示按下，位 = 1 表示松开。
	 * 所以要先取反再判断。
	 * 不取反的话：没按键时全是 1，会被当成"所有键都按下"，
	 * 十字键的上+下互相抵消，VX 永远是 0，车就不会动。 */
	{
		uint8_t btn = (uint8_t)(~temp[3]);

		out->select = (btn & 0x01) ? 1 : 0;
		out->l3     = (btn & 0x02) ? 1 : 0;
		out->r3     = (btn & 0x04) ? 1 : 0;
		out->start  = (btn & 0x08) ? 1 : 0;
		out->up     = (btn & 0x10) ? 1 : 0;
		out->right  = (btn & 0x20) ? 1 : 0;
		out->down   = (btn & 0x40) ? 1 : 0;
		out->left   = (btn & 0x80) ? 1 : 0;
	}

	/* 面键在按键字节 2 里，同样低电平有效 */
	{
		uint8_t btn2 = (uint8_t)(~temp[4]);

		out->triangle = (btn2 & 0x10) ? 1 : 0;	/* △ 升档 */
		out->circle   = (btn2 & 0x20) ? 1 : 0;
		out->cross    = (btn2 & 0x40) ? 1 : 0;	/* × 降档 */
		out->square   = (btn2 & 0x80) ? 1 : 0;

		out->l1       = (btn2 & 0x04) ? 1 : 0;	/* L1 原地左转 */
		out->r1       = (btn2 & 0x08) ? 1 : 0;	/* R1 原地右转 */
		out->l2       = (btn2 & 0x01) ? 1 : 0;	/* L2 备用左转 */
		out->r2       = (btn2 & 0x02) ? 1 : 0;	/* R2 备用右转 */
	}

	return 1;
}

/* ==================== 时钟档位自动扫描 ==================== */
/* 不同批次的接收器对时钟快慢容忍度不一样。
 * 这里从慢到快逐个试，谁先正确应答 0x41/0x73 就用谁。 */
static const uint8_t kScanLoops[] = {64, 48, 40, 32, 28, 24, 20, 16, 12};

static void PS2_AutoScan(void)
{
	uint8_t    k, e;
	PS2_Data_t tmp;

	for (e = 0; e < 2; e++)				/* 两种采样边沿都试 */
	{
		s_edge = e;

		for (k = 0; k < (uint8_t)sizeof(kScanLoops); k++)
		{
			s_loops = kScanLoops[k];

			if (PS2_Frame(&tmp))
			{
				s_cache       = tmp;
				s_connected   = 1;
				s_fail_streak = 0;
				return;					/* 找到就锁定这套参数 */
			}
		}
	}

	s_loops = PS2_DELAY_LOOPS;			/* 全部失败，恢复默认 */
	s_edge  = 0;
}

/* ==================== 对外读取接口 ==================== */
uint8_t PS2_Read(PS2_Data_t *out)
{
	uint8_t i;

	/* 限频：两次读至少隔 PS2_POLL_MS */
	if ((now_ms() - s_last_poll_ms) < PS2_POLL_MS)
	{
		*out = s_cache;
		return s_connected;
	}
	s_last_poll_ms = now_ms();

	/* 失败重试，减少偶发误判 */
	for (i = 0; i < PS2_RETRY; i++)
	{
		if (PS2_Frame(&s_cache))
		{
			s_fail_streak = 0;
			s_connected   = 1;
			*out = s_cache;
			return 1;
		}
	}

	/* 从没连上时，每 3 秒自动扫一次时钟档位 */
	if (!s_connected && ((now_ms() - s_last_scan_ms) > 3000))
	{
		s_last_scan_ms = now_ms();
		PS2_AutoScan();

		if (s_connected)
		{
			*out = s_cache;
			return 1;
		}
	}

	/* 连续失败才判定掉线，避免数据显示和灯光来回跳 */
	s_fail_total++;
	if (s_fail_streak < 255) s_fail_streak++;
	if (s_fail_streak >= PS2_FAIL_LIMIT) s_connected = 0;

	*out = s_cache;			/* 输出最后一次有效数据 */
	return s_connected;
}

/* ==================== 摇杆归一化 ==================== */
float PS2_Axis(uint8_t raw)
{
	float v = ((float)raw - 128.0f) / 127.0f;

	if (v >  1.0f) v =  1.0f;
	if (v < -1.0f) v = -1.0f;

	/* 8% 死区：松手时不会让车自己慢慢爬 */
	if (v > -0.08f && v < 0.08f) v = 0.0f;

	return v;
}

uint8_t PS2_IsConnected(void)
{
	return s_connected;
}

uint16_t PS2_GetFailCount(void)
{
	return s_fail_total;
}

const uint8_t *PS2_GetRawFrame(void)
{
	return s_raw;
}
