#ifndef __CONFIG_H
#define __CONFIG_H

/* ============================================================
 *  整车参数表
 *  所有可调数字都放这里，现场标定只改这一个文件
 * ============================================================ */

/* ---------- 机械参数（按实车量完再改） ---------- */
#define WHEEL_R        0.040f    /* 80mm 麦轮半径，单位 m */
#define CHASSIS_R      0.150f    /* 底盘中心到轮中心的距离，单位 m */
#define GEAR_RATIO     34.0f     /* MG370 减速比 1:34 */
#define ENC_PPR        500.0f    /* 编码器每转脉冲数（电机轴端） */

/* 输出轴转一圈的四倍频计数 = 500 * 4 * 34 = 68000 */
#define ENC_CNT_PER_REV  ((int32_t)(ENC_PPR * 4.0f * GEAR_RATIO))

/* ---------- 速度与加速度上限 ---------- */
/* 加速度受电池 8A 过流保护约束，不要为了"起步猛"调大 */
#define VXY_MAX        1.0f      /* 最大平移速度，m/s */
#define VW_MAX         180.0f    /* 最大旋转角速度，度/秒 */
#define ACC_XY         2.0f      /* 平移加速度，m/s^2 */
#define ACC_W          360.0f    /* 旋转角加速度，度/秒^2 */

/* 标定量：满占空比时轮子的实测转速（RPM）。先填 300，实测后改 */
#define MOTOR_RPM_MAX  300.0f

/* ---------- 云台 ---------- */
#define PITCH_MIN       45.0f    /* 枪口最低角，度 */
#define PITCH_MID       90.0f    /* 中位，度 */
#define PITCH_MAX      135.0f    /* 枪口最高角，度 */
#define PITCH_SPEED_MAX 120.0f   /* 俯仰速度，度/秒（鼠标控制时） */

/* 俯仰方向：如果鼠标往上推、枪口却往下走，把这里改成 -1.0f */
#define PITCH_DIR       1.0f

/* ---------- 发射 ---------- */
#define SHOOT_USE_RELAY 1       /* 1 = 机械继电器：单次持续吸合，不做高频PWM */
#define SHOOT_ACTIVE_HIGH 1     /* 1 = IN高电平吸合；空闲低电平关闭 */
#define SINGLE_SHOT_MS  200     /* 每按一次□键，波箱通电时长，ms */
#define SHOOT_SOFT_MS  180       /* 波箱软启动时长，ms（保护电池，别改小） */

/* ---------- 编码器与里程计 ---------- */
/* 车体前进距离(m) = 轮子平均行程(m) × ODOM_TRANS_K
 * 理论值 1.414。实测"命令走 5m 实际走了多少"，按比例修正 */
#define ODOM_TRANS_K    1.414f
#define ODOM_ROT_K      1.0f     /* 旋转角度系数，理论 1.0，一般不用改 */

/* ---------- 考察模式（按 PB1 按钮触发） ---------- */
#define TESTMODE_ENABLE     1        /* 1 = 允许进考察模式；正式比赛可以改 0 */
#define TEST_DIST_M         5.0f     /* 直线考察：走多远 */
#define TEST_DIST_SPEED     0.5f     /* 直线速度 m/s */
#define TEST_TURN_DEG       720.0f   /* 旋转考察：转多少度 */
#define TEST_TURN_SPEED     90.0f    /* 旋转速度 度/秒 */
#define TEST_STRAFE_M       2.0f     /* 平移考察：平移多远 */
#define TEST_STRAFE_SPEED   0.4f
#define TEST_SHOOT_MS       10000    /* 连发考察：持续多久 */
#define TEST_TIMEOUT_MS     25000    /* 每一项的超时保护，超时强制停 */

/* ---------- 裁判系统 ---------- */
#define KB_TIMEOUT_MS  100       /* 多久收不到键鼠包算掉线，ms */
#define KB_LEN          10       /* 键鼠包长度，字节 */

/* ---------- 遥控接收机（PWM 通道型） ---------- */
/* 1 = 启用。遥控器一开就用遥控开车；遥控器关掉自动回到裁判系统键鼠 */
#define RC_ENABLE       1
/* 用了几个通道，最多 4 个。用不上的通道对应的引脚就空着 */
#define RC_CH_COUNT     4
#define RC_TIMEOUT_MS   50       /* 多久收不到脉冲算失控 */
#define RC_DEADZONE     0.05f    /* 摇杆死区 */

/* 通道映射：0 = 第 1 通道，1 = 第 2 通道……  设成 255 表示这个功能不用遥控
 * 常见 4 通道接收机：ch1 右摇杆左右、ch2 右摇杆上下、ch3 左摇杆上下、ch4 左摇杆左右/开关 */
#define RC_CH_STEER     0        /* 转向 -> wz */
#define RC_CH_FWD       1        /* 前进 -> vx */
#define RC_CH_STRAFE    2        /* 平移 -> vy */
#define RC_CH_SHOOT     3        /* 发射开关，推过一半就开火 */
#define RC_CH_PITCH     255      /* 云台俯仰用哪个通道；不用遥控控制俯仰就保持 255 */
#define RC_NO_CH        255      /* "这个功能不用遥控" */

/* ---------- 调试开关 ---------- */
/* 1 = 没接到裁判系统时自动跑一段动作（前进/后退/平移/旋转），方便台架测试
 * 0 = 只接受裁判系统的键鼠指令（正式比赛用这个）
 * 注意：开着自检时上电，车会自己动，测试请把轮子架空 */
#define SELF_TEST          1

/* 自检时每个动作持续多久，毫秒 */
#define SELF_TEST_STEP_MS  2000

/* 1 = 打开 OLED 调试页（车上的小屏幕显示链路、按键、轮速）
 * 0 = 关掉。软件 I2C 刷一屏要几毫秒，正式比赛建议关掉 */
#define DEBUG_OLED         1

#endif
