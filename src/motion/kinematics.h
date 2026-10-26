/**
 ******************************************************************************
 * @file        kinematics.h
 * @brief       绳索驱动并联机器人运动学
 * @details
 *     电机布局 (平台尺寸 630mm × 630mm, 电机位于四角):
 *
 *          M01(-315, 315)          M02(315, 315)
 *               ★───────────────────────★
 *               │                        │
 *               │      有效范围±200mm     │
 *               │                        │
 *               ★───────────────────────★
 *           M04(-315,-315)          M03(315,-315)
 *
 *     电机坐标 (固定平台):
 *       M01: (-315,  315) mm
 *       M02: ( 315,  315) mm
 *       M03: ( 315, -315) mm
 *       M04: (-315, -315) mm
 *
 *     绳索长度计算: 4根绳索均连接到末端执行器(夹爪)同一点,
 *     各电机到末端执行器(x,y)的欧氏距离:
 *       L1 = sqrt((x+315)² + (y-315)²)    (M01→夹爪)
 *       L2 = sqrt((x-315)² + (y-315)²)    (M02→夹爪)
 *       L3 = sqrt((x-315)² + (y+315)²)    (M03→夹爪)
 *       L4 = sqrt((x+315)² + (y+315)²)    (M04→夹爪)
 *
 *     编码器: 51200脉冲/圈, 1圈=30mm绳长变化
 *     ΔPulses = ΔLength(mm) × 51200 / 30
 ******************************************************************************
 */

#ifndef __KINEMATICS_H
#define __KINEMATICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 几何参数 ======================== */

#define PLATFORM_HALF_WIDTH     315.0f       /* 平台半宽 (mm) */
#define PLATFORM_HALF_HEIGHT    315.0f       /* 平台半高 (mm) */
#define CONTROL_RANGE_MAX       200.0f       /* 有效控制范围 (mm) */

#define ENCODER_PULSES_PER_REV  51200        /* 编码器脉冲/圈 */
#define ROPE_PER_REV_MM         30.0f        /* 每圈绳长变化 (mm) */
#define PULSES_PER_MM           (ENCODER_PULSES_PER_REV / ROPE_PER_REV_MM)

/* ======================== 数据结构 ======================== */

/* 二维坐标 */
typedef struct {
    float x;    /* X坐标 (mm) */
    float y;    /* Y坐标 (mm) */
} point2d_t;

/* 四根绳索长度 */
typedef struct {
    float L1;   /* M01 → 平台上角点 (mm) */
    float L2;   /* M02 → 平台上角点 (mm) */
    float L3;   /* M03 → 平台下角点 (mm) */
    float L4;   /* M04 → 平台下角点 (mm) */
} rope_lengths_t;

/* 四个电机的编码器值 */
typedef struct {
    int32_t m1;     /* M01 编码器位置 (脉冲数) */
    int32_t m2;     /* M02 编码器位置 (脉冲数) */
    int32_t m3;     /* M03 编码器位置 (脉冲数) */
    int32_t m4;     /* M04 编码器位置 (脉冲数) */
} encoder_values_t;

/* 运动学状态 */
typedef struct {
    point2d_t       current_pos;        /* 当前位置 (mm) */
    point2d_t       target_pos;         /* 目标位置 (mm) */
    rope_lengths_t  current_lengths;    /* 当前绳长 (mm) */
    rope_lengths_t  target_lengths;     /* 目标绳长 (mm) */
    rope_lengths_t  initial_lengths;    /* 零点绳长 (mm) */
    encoder_values_t zero_encoders;     /* 零点编码器值 */
    encoder_values_t current_encoders;  /* 当前编码器值 */
    bool            is_zeroed;          /* 是否已完成归零 */
    bool            is_calibrated;      /* 是否已标定 */
} kinematics_state_t;

/* ======================== 函数声明 ======================== */

/**
 * @brief   初始化运动学状态
 * @param   state   运动学状态指针
 */
void kinematics_init(kinematics_state_t *state);

/**
 * @brief   设置当前位置为零点 (归零操作)
 * @param   state       运动学状态指针
 * @param   encoders    当前各电机编码器值
 */
void kinematics_set_zero(kinematics_state_t *state, const encoder_values_t *encoders);

/**
 * @brief   逆运动学: 由目标位置(x,y)计算各绳索长度
 * @param   pos     目标位置 (mm)
 * @param   lengths 输出: 各绳索长度 (mm)
 */
void kinematics_inverse(const point2d_t *pos, rope_lengths_t *lengths);

/**
 * @brief   正运动学: 由编码器值估算当前位置
 * @details 使用绳长差值和几何约束进行迭代求解
 *          实际实现使用近似线性化方法
 * @param   state       运动学状态
 * @param   encoders    当前编码器值
 * @param   out_pos     输出: 估算的当前位置 (mm)
 * @return  true=求解成功
 */
bool kinematics_forward(kinematics_state_t *state, const encoder_values_t *encoders, point2d_t *out_pos);

/**
 * @brief   由编码器差值计算当前绳长
 * @param   state       运动学状态
 * @param   encoders    当前编码器值
 * @param   lengths     输出: 当前绳长
 */
void kinematics_get_current_lengths(const kinematics_state_t *state, const encoder_values_t *encoders, rope_lengths_t *lengths);

/**
 * @brief   绳长变化量 → 所需脉冲变化量
 * @param   delta_length_mm  绳长变化 (mm)
 * @return  脉冲变化量
 */
int32_t kinematics_length_to_pulses(float delta_length_mm);

/**
 * @brief   脉冲变化量 → 绳长变化量
 * @param   delta_pulses  脉冲变化量
 * @return  绳长变化 (mm)
 */
float kinematics_pulses_to_length(int32_t delta_pulses);

/**
 * @brief   检查位置是否在有效控制范围内
 * @param   pos     位置坐标
 * @return  true=在范围内
 */
bool kinematics_is_in_range(const point2d_t *pos);

/**
 * @brief   裁剪位置到有效范围内
 * @param   pos     输入/输出: 位置坐标
 */
void kinematics_clamp_to_range(point2d_t *pos);

/**
 * @brief   计算到达目标位置每个电机需要的绳长变化
 * @param   state           运动学状态
 * @param   target_pos      目标位置
 * @param   delta_pulses    输出: 各电机需要的脉冲变化量 (正=放绳, 负=收绳)
 */
void kinematics_compute_deltas(const kinematics_state_t *state, const point2d_t *target_pos, int32_t delta_pulses[4]);

#ifdef __cplusplus
}
#endif

#endif /* __KINEMATICS_H */
