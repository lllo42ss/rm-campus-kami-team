#include "stm32f10x.h"
#include "Tick.h"
#include "Delay.h"
#include "Config.h"
#include "Motor.h"
#include "PS2.h"
#include "OLED.h"
#include "Servo.h"
#include "Encoder.h"
#include "Shooter.h"

/* ============================================================
 *  只用十字键控制小车
 *
 *      START             启动 / 停止
 *      十字键 ↑↓         前进 / 后退
 *      十字键 ←→         左 / 右平移
 *      □                 按一下单发水弹
 *      L1+R1             启动/停止10秒连续发射
 *
 *  左摇杆不参与控制；右摇杆上下控制舵机俯仰。
 *
 *  升档：按下后 0.35 秒平滑加到满，松手 0.35 秒平滑减到 0。
 *
 *  接线：
 *      PS2  CLK->PB3  CS->PA11  CMD->PA12  DAT->PA15  VCC->3.3V  GND->GND
 *      OLED SCL->PB8  SDA->PB9  VCC->3.3V  GND->GND
 *      电机驱动见 README
 * ============================================================ */

#define LED_PORT    GPIOC
#define LED_PIN     GPIO_Pin_13

/* 起步：从 0 加到满占空比需要多少毫秒 */
#define RAMP_UP_MS    120

/* 停车：从当前值降到 0 需要多少毫秒（比起步更快） */
#define                                                                                                                                                 RAMP_DOWN_MS   60

/* 屏幕刷新间隔 ms */
#define UI_MS       100

/* 开机默认档位：G03 = 30%，避免第一次启动过快。
 * 运行中用 △ 升档、× 降档，1~10 档都会保持。 */
#define START_GEAR   3

/* 原地旋转需要克服四个轮胎的侧向摩擦，低速档容易只抖不转。
 * 旋转输出至少使用 65%，前进档位仍按 G01~G10 控制。 */
#define ROTATE_MIN_LEVEL  0.55f

/* 右摇杆控制舵机：当前实车安全角度范围 */
#define SERVO_MIN_ANGLE      55.0f
#define SERVO_MAX_ANGLE      80.0f
#define SERVO_TEST_MIN       55.0f
#define SERVO_TEST_MAX       70.0f
#define SERVO_AXIS_SPEED     60.0f
#define SERVO_AXIS_DIR       -1.0f

/* 左摇杆：上推前进，右推右移。若方向相反，只改这两个符号。 */
#define JOY_FWD_DIR      -1.0f
#define JOY_STRAFE_DIR    1.0f

/* 按前进方向校准后的旋转方向：
 * 左转时左侧轮向前、右侧轮向后。
 * 顺序：A 右后、B 左后、C 左前、D 右前。 */
static const float rotate_dir[4] = {-1.0f, 1.0f, -1.0f, 1.0f};

/* 评分项：连续发射 10 秒 */
#define BURST_FIRE_MS  10000u

/* 当前只接了两路编码器：
 *   Encoder 0 -> C 左前轮
 *   Encoder 1 -> D 右前轮
 * 如果实际装在别的轮子上，只改这两个编号：
 *   0=A右后  1=B左后  2=C左前  3=D右前
 */
#define ENC0_WHEEL  2
#define ENC1_WHEEL  3

static float cur[4] = {0.0f, 0.0f, 0.0f, 0.0f};	/* 当前实际占空比 */

/* 按时间计算的斜坡状态：目标变化后从当前值线性走到新目标 */
static void LED_Init(void)
{
	GPIO_InitTypeDef g;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	g.GPIO_Pin   = LED_PIN;
	g.GPIO_Mode  = GPIO_Mode_Out_PP;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_PORT, &g);
	GPIO_SetBits(LED_PORT, LED_PIN);
}

/* 舵机使用更小的3%死区，轻微推动摇杆也能响应 */
static float Servo_Axis(uint8_t raw)
{
	float v = ((float)raw - 128.0f) / 127.0f;

	if (v >  1.0f) v =  1.0f;
	if (v < -1.0f) v = -1.0f;
	if (v > -0.03f && v < 0.03f) v = 0.0f;
	return v;
}

int main(void)
{
	PS2_Data_t ps2;

	uint8_t  ok;
	uint8_t  running     = 0;
	uint8_t  last_start  = 0;
	uint8_t  led_level   = 1;
	uint8_t  i;

	uint32_t ui_tick = 0;
	uint32_t led_tick = 0;
	uint32_t toggle_tick = 0;
	uint32_t enc_tick = 0;
	uint32_t burst_tick = 0;
	uint32_t servo_tick = 0;

	/* 速度档位：1~10 档，每档 10%，低档起步 */
	uint8_t  gear       = START_GEAR;
	uint8_t  last_tri   = 0;
	uint8_t  last_cross = 0;
	uint8_t  last_circle = 0;
	uint8_t  last_square = 0;
	uint8_t  last_fire_combo = 0;
	uint8_t  burst_active = 0;

	/* 舵机：右摇杆上下连续控制，○ 键切换55°/80° */
	float    servo_angle = (SERVO_MIN_ANGLE + SERVO_MAX_ANGLE) * 0.5f;
	uint8_t  servo_btn_high = 0;
	float    servo_axis, servo_dt;
	uint32_t servo_now;

	float vx, vy;
	float fwd, lft;			/* 物理坐标：前进 / 左移 */
	float joy_fwd, joy_lft;
	float wz;				/* 原地旋转量 */
	float out[4];
	float rpm[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float target;
	float adjust, control_dt, control_step;
	uint32_t control_tick = 0;
	uint32_t control_now;
	float level;
	float rotate_level;

	/* ---------- 初始化 ---------- */
	Tick_Init();
	Shooter_Init();					/* PA5 尽早上电为不发射状态 */
	LED_Init();

	OLED_Init();
	OLED_Clear();
	OLED_ShowString(1, 1,  "A*");		/* 估算 RPM */
	OLED_ShowString(1, 8,  "B*");		/* 估算 RPM */
	OLED_ShowString(2, 1,  "C ");		/* 实测 RPM */
	OLED_ShowString(2, 8,  "D ");		/* 实测 RPM */
	OLED_ShowString(3, 1,  "RPM");
	OLED_ShowString(4, 1,  "S");		/* 舵机角度 */

	Motor_Init();
	Encoder_Init();
	PS2_Init();
	Servo_Init();					/* PA8 = TIM1_CH1，50Hz */

	/* 舵机自检限制在55°～70°，最后回到控制范围中间67.5° */
	Servo_SetAngle(SERVO_TEST_MIN);
	Delay_ms(350);
	Servo_SetAngle(SERVO_TEST_MAX);
	Delay_ms(350);
	Servo_SetAngle(servo_angle);

	Delay_ms(300);
	Motor_Enable(1);

	/* 开机自检：四个轮子转 0.4 秒（和手柄无关） */
	Motor_SetAll(0.35f, 0.35f, 0.35f, 0.35f);
	Delay_ms(250);
	Motor_SetAll(0.0f, 0.0f, 0.0f, 0.0f);
	Delay_ms(200);

	control_tick = now_ms();
	servo_tick   = now_ms();

	/* ---------- 主循环 ---------- */
	while (1)
	{
		ok = PS2_Read(&ps2);			/* 10ms 限频 + 3 次重试 */

		/* 编码器固定每 20ms 更新一次，避免主循环速度变化影响测速 */
		if ((now_ms() - enc_tick) >= 20)
		{
			enc_tick = now_ms();
			Encoder_Update(0.02f);
		}

		/* ---- 启动 / 停止（250ms 去抖） ---- */
		if (ok)
		{
			if (ps2.start && !last_start && ((now_ms() - toggle_tick) > 250))
			{
				running = (uint8_t)(!running);
				toggle_tick = now_ms();
			}
			last_start = ps2.start;

			/* △ 升档、× 降档：按一下切一档，档位会保持 */
			if (ps2.triangle && !last_tri)
			{
				if (gear < 10) gear++;
			}
			last_tri = ps2.triangle;

			if (ps2.cross && !last_cross)
			{
				if (gear > 1) gear--;
			}
			last_cross = ps2.cross;

			/* ○ 键：在55°和80°之间切换，右摇杆仍可继续调节 */
			if (ps2.circle && !last_circle)
			{
				servo_btn_high = (uint8_t)(!servo_btn_high);
				servo_angle = servo_btn_high ? 55.0f : 80.0f;
				Servo_SetAngle(servo_angle);
			}
			last_circle = ps2.circle;

			/* □ 键：只在按下瞬间触发一次，按住不会连续发射 */
			if (running && ps2.square && !last_square)
			{
				Shooter_TriggerSingle();
			}
			if (!ps2.square && last_square)
			{
				Shooter_CancelSingle();
			}
			last_square = ps2.square;

			/* L1+R1：启动/停止连续发射。两者同时按只切换一次。
			 * 启动后允许松开按键，波箱持续工作 10 秒后自动停止。 */
			if (running && ps2.l1 && ps2.r1 && !last_fire_combo)
			{
				burst_active = (uint8_t)(!burst_active);
				Shooter_SetContinous(burst_active);
				burst_tick = now_ms();
			}
			last_fire_combo = (ps2.l1 && ps2.r1) ? 1 : 0;
		}

		/* 连续发射到时自动关闭 */
		if (burst_active && ((now_ms() - burst_tick) >= BURST_FIRE_MS))
		{
			burst_active = 0;
			Shooter_SetContinous(0);
		}

		/* 手柄掉线时立即结束连续发射，避免继电器一直吸合 */
		if (!ok && burst_active)
		{
			burst_active = 0;
			Shooter_SetContinous(0);
		}

		/* 未按 START 进入运行状态时，禁止发射和连续发射 */
		if (!running)
		{
			burst_active = 0;
			Shooter_SetContinous(0);
		}

		/* 发射状态机必须持续调用 */
		Shooter_Update();

		/* 右摇杆 Y 轴连续调节舵机，中位时角度保持不变 */
		servo_now = now_ms();
		servo_dt = (float)(servo_now - servo_tick) * 0.001f;
		servo_tick = servo_now;
		if (servo_dt > 0.05f) servo_dt = 0.05f;

		if (ok)
		{
			servo_axis = Servo_Axis(ps2.ry) * SERVO_AXIS_DIR;
			servo_angle += servo_axis * SERVO_AXIS_SPEED * servo_dt;

			if (servo_angle < SERVO_MIN_ANGLE) servo_angle = SERVO_MIN_ANGLE;
			if (servo_angle > SERVO_MAX_ANGLE) servo_angle = SERVO_MAX_ANGLE;
			Servo_SetAngle(servo_angle);
		}

		/* ---- 十字键 -> 物理方向（前进 / 左移） ---- */
		fwd = 0.0f;
		lft = 0.0f;
		wz  = 0.0f;

		if (running && ok)
		{
			joy_fwd = JOY_FWD_DIR * PS2_Axis(ps2.ly);
			joy_lft = JOY_STRAFE_DIR * PS2_Axis(ps2.lx);

			if (joy_fwd != 0.0f || joy_lft != 0.0f)
			{
				/* 左摇杆有输入时使用模拟量 */
				fwd = joy_fwd;
				lft = joy_lft;
			}
			else
			{
				/* 摇杆回中时保留十字键作为备用控制 */
				if (ps2.up)    fwd += 1.0f;
				if (ps2.down)  fwd -= 1.0f;
				if (ps2.left)  lft -= 1.0f;
				if (ps2.right) lft += 1.0f;
			}

			/* L2 左转；R2 右转。右摇杆专用于舵机，R3 和 SELECT 不参与旋转。 */
			if (ps2.l2) wz += 1.0f;
			if (ps2.r2) wz -= 1.0f;
		}

		/* ---- 坐标系旋转 90° ----
		 * 实车观察：按 → 才前进、按 ↑ 反而右移，
		 * 说明车体前进方向对应公式里的 -vy、左移对应 -vx。
		 * 不做这一步，UP 会变成向右平移。 */
		vx = -lft;
		vy = -fwd;

		/* 档位系数：5 档 = 0.5，10 档 = 1.0 */
		level = (float)gear / 10.0f;
		rotate_level = (level < ROTATE_MIN_LEVEL) ? ROTATE_MIN_LEVEL : level;

		/* ---- 麦轮解算，按实车电机位置 ----
		 *   A = 右后   B = 左后   C = 左前   D = 右前
		 *
		 * 左右两侧轮子是镜像安装的，所以同一台车要"往前"，
		 * 左右两侧的电机符号必须相反。 */
		out[0] = -vx - vy;	/* A 右后 */
		out[1] =  vx - vy;	/* B 左后 */
		out[2] =  vx + vy;	/* C 左前 */
		out[3] = -vx + vy;	/* D 右前 */

		/* ---- 按实际时间限速 + 输出 ----
		 * 适合左摇杆连续变化的模拟量：每个循环只允许输出变化固定步长，
		 * 满量程加速约 RAMP_UP_MS，满量程减速约 RAMP_DOWN_MS。 */
		control_now = now_ms();
		control_dt = (float)(control_now - control_tick) * 0.001f;
		control_tick = control_now;
		if (control_dt > 0.05f) control_dt = 0.05f;

		for (i = 0; i < 4; i++)
		{
			/* 平移部分使用当前档位；旋转按每个轮子方向单独叠加 */
			target = out[i] * level + wz * rotate_dir[i] * rotate_level;
			if (target >  1.0f) target =  1.0f;
			if (target < -1.0f) target = -1.0f;

			adjust = target - cur[i];
			control_step = control_dt * 1000.0f /
			               ((adjust >= 0.0f) ? RAMP_UP_MS : RAMP_DOWN_MS);

			if (adjust >  control_step) adjust =  control_step;
			if (adjust < -control_step) adjust = -control_step;
			cur[i] += adjust;

			Motor_Set(i, cur[i]);
		}

		/* ---- OLED 状态 ---- */
		if ((now_ms() - ui_tick) >= UI_MS)
		{
			ui_tick = now_ms();

			/* 没接编码器的两个轮子先按“占空比 × 最大转速”估算；
			 * 接了编码器的 C、D 用实测值覆盖。 */
			for (i = 0; i < 4; i++)
			{
				rpm[i] = cur[i] * MOTOR_RPM_MAX;
			}
			rpm[ENC0_WHEEL] = Encoder_GetRpm(0);
			rpm[ENC1_WHEEL] = Encoder_GetRpm(1);

			OLED_ShowSignedNum(1, 3,  (int32_t)rpm[0], 4);	/* A */
			OLED_ShowSignedNum(1, 10, (int32_t)rpm[1], 4);	/* B */
			OLED_ShowSignedNum(2, 3,  (int32_t)rpm[2], 4);	/* C */
			OLED_ShowSignedNum(2, 10, (int32_t)rpm[3], 4);	/* D */

			OLED_ShowString(3, 5, ok ? "OK " : "-- ");
			OLED_ShowString(3, 8, running ? "RUN" : "STP");
			OLED_ShowString(3, 11, "G");
			OLED_ShowNum   (3, 12, gear, 2);

			OLED_ShowNum   (4, 2, (uint32_t)servo_angle, 3);
			OLED_ShowString(4, 6, "F");
			OLED_ShowNum   (4, 7, Shooter_IsFiring(), 1);
			OLED_ShowString(4, 9, "A");
			OLED_ShowNum   (4, 10, burst_active, 1);
		}

		/* ---- LED：常亮=运行，慢闪=停止 ---- */
		if (running && ok)
		{
			GPIO_ResetBits(LED_PORT, LED_PIN);
			led_level = 1;
		}
		else if ((now_ms() - led_tick) >= 500)
		{
			led_tick = now_ms();
			led_level = (uint8_t)(!led_level);
			if (led_level) GPIO_ResetBits(LED_PORT, LED_PIN);
			else           GPIO_SetBits(LED_PORT, LED_PIN);
		}

		Delay_ms(1);
	}
}
