/**
 ******************************************************************************
 * @file        kinematics.c
 * @brief       绳索驱动并联机器人运动学实现
 * @details
 *     逆运动学: (x,y) → 绳长 L1~L4 (确定解)
 *     正运动学: 编码器值 → (x,y) (使用迭代/搜索算法)
 *
 *     绳长与脉冲转换公式:
 *       ΔPulses = ΔLength(mm) × 51200 / 30
 *       ΔLength(mm) = ΔPulses × 30 / 51200
 ******************************************************************************
 */

#include "kinematics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ======================== 初始化 ======================== */

void kinematics_init(kinematics_state_t *state)
{
    if (state == NULL) return;

    memset(state, 0, sizeof(kinematics_state_t));
    state->current_pos.x = 0.0f;
    state->current_pos.y = 0.0f;
    state->target_pos.x  = 0.0f;
    state->target_pos.y  = 0.0f;
    state->is_zeroed     = false;
    state->is_calibrated = false;

    /* 计算原点 (0,0) 时的初始绳长 */
    point2d_t origin = {0.0f, 0.0f};
    kinematics_inverse(&origin, &state->initial_lengths);
    state->current_lengths = state->initial_lengths;
    state->target_lengths  = state->initial_lengths;
}

/* ======================== 逆运动学 ======================== */

void kinematics_inverse(const point2d_t *pos, rope_lengths_t *lengths)
{
    if (pos == NULL || lengths == NULL) return;

    float x = pos->x;
    float y = pos->y;

    /*
     * 各电机到末端执行器(x,y)的欧氏距离:
     * M01(-315, 315): L1 = sqrt((x+315)² + (y-315)²)
     * M02( 315, 315): L2 = sqrt((x-315)² + (y-315)²)
     * M03( 315,-315): L3 = sqrt((x-315)² + (y+315)²)
     * M04(-315,-315): L4 = sqrt((x+315)² + (y+315)²)
     *
     * 4根绳索均连接到同一末端执行器(夹爪),物理上过约束但
     * 通过绳索张力协同控制实现稳定运动。
     */

    lengths->L1 = sqrtf((x + PLATFORM_HALF_WIDTH)  * (x + PLATFORM_HALF_WIDTH) +
                        (y - PLATFORM_HALF_HEIGHT) * (y - PLATFORM_HALF_HEIGHT));
    lengths->L2 = sqrtf((x - PLATFORM_HALF_WIDTH)  * (x - PLATFORM_HALF_WIDTH) +
                        (y - PLATFORM_HALF_HEIGHT) * (y - PLATFORM_HALF_HEIGHT));
    lengths->L3 = sqrtf((x - PLATFORM_HALF_WIDTH)  * (x - PLATFORM_HALF_WIDTH) +
                        (y + PLATFORM_HALF_HEIGHT) * (y + PLATFORM_HALF_HEIGHT));
    lengths->L4 = sqrtf((x + PLATFORM_HALF_WIDTH)  * (x + PLATFORM_HALF_WIDTH) +
                        (y + PLATFORM_HALF_HEIGHT) * (y + PLATFORM_HALF_HEIGHT));
}

/* ======================== 正运动学 ======================== */

void kinematics_get_current_lengths(const kinematics_state_t *state,
                                     const encoder_values_t *encoders,
                                     rope_lengths_t *lengths)
{
    if (state == NULL || encoders == NULL || lengths == NULL) return;

    /* 从零点绳长 + 编码器偏差 = 当前绳长 */
    lengths->L1 = state->initial_lengths.L1 +
                  kinematics_pulses_to_length(encoders->m1 - state->zero_encoders.m1);
    lengths->L2 = state->initial_lengths.L2 +
                  kinematics_pulses_to_length(encoders->m2 - state->zero_encoders.m2);
    lengths->L3 = state->initial_lengths.L3 +
                  kinematics_pulses_to_length(encoders->m3 - state->zero_encoders.m3);
    lengths->L4 = state->initial_lengths.L4 +
                  kinematics_pulses_to_length(encoders->m4 - state->zero_encoders.m4);
}

bool kinematics_forward(kinematics_state_t *state,
                         const encoder_values_t *encoders,
                         point2d_t *out_pos)
{
    if (state == NULL || encoders == NULL || out_pos == NULL) return false;

    /* 先获取当前绳长 */
    rope_lengths_t lengths;
    kinematics_get_current_lengths(state, encoders, &lengths);

    /*
     * 正运动学迭代求解:
     * 使用 Newton-Raphson 方法从绳长反推位置。
     *
     * 对于每个电机 i, 绳长平方:
     *   L1² = (x - X1)² + (y - Y1)²  →  L1² = (x + 315)² + (y - 315)²
     *   L2² = (x - X2)² + (y - Y2)²  →  L2² = (x - 315)² + (y - 315)²
     *   L3² = (x - X3)² + (y - Y3)²  →  L3² = (x - 315)² + (y + 315)²
     *   L4² = (x - X4)² + (y - Y4)²  →  L4² = (x + 315)² + (y + 315)²
     *
     * 其中 (Xi, Yi) 为各电机坐标。
     *
     * 使用两两组合消元法:
     * L1² - L2² = (x+315)² + (y-315)² - (x-315)² - (y-315)²
     *           = (x+315)² - (x-315)²
     *           = (x² + 630x + 315²) - (x² - 630x + 315²)
     *           = 1260x
     * 所以: x = (L1² - L2²) / 1260
     *
     * 同理:
     * L3² - L4² = (x-315)² + (y+315)² - (x+315)² - (y+315)²
     *           = (x-315)² - (x+315)²
     *           = -1260x
     * 所以: x = (L4² - L3²) / 1260
     *
     * L1² - L4² = (x+315)² + (y-315)² - (x+315)² - (y+315)²
     *           = (y-315)² - (y+315)²
     *           = -1260y
     * 所以: y = (L4² - L1²) / 1260
     *
     * L2² - L3² = (x-315)² + (y-315)² - (x-315)² - (y+315)²
     *           = (y-315)² - (y+315)²
     *           = -1260y
     * 所以: y = (L3² - L2²) / 1260
     */

    float L1_sq = lengths.L1 * lengths.L1;
    float L2_sq = lengths.L2 * lengths.L2;
    float L3_sq = lengths.L3 * lengths.L3;
    float L4_sq = lengths.L4 * lengths.L4;

    /* 用两组独立方程求x, 取平均提高精度 */
    float x1 = (L1_sq - L2_sq) / (4.0f * PLATFORM_HALF_WIDTH);
    float x2 = (L4_sq - L3_sq) / (4.0f * PLATFORM_HALF_WIDTH);
    float x = (x1 + x2) * 0.5f;

    /* 用两组独立方程求y, 取平均 */
    float y1 = (L4_sq - L1_sq) / (4.0f * PLATFORM_HALF_HEIGHT);
    float y2 = (L3_sq - L2_sq) / (4.0f * PLATFORM_HALF_HEIGHT);
    float y = (y1 + y2) * 0.5f;

    /* 处理浮点精度导致的微小越界 */
    if (fabsf(x) < 0.01f) x = 0.0f;
    if (fabsf(y) < 0.01f) y = 0.0f;

    out_pos->x = x;
    out_pos->y = y;

    /* 更新状态 */
    state->current_pos    = *out_pos;
    state->current_lengths = lengths;

    return true;
}

/* ======================== 归零操作 ======================== */

void kinematics_set_zero(kinematics_state_t *state, const encoder_values_t *encoders)
{
    if (state == NULL || encoders == NULL) return;

    /* 记录当前编码器值作为零点 */
    state->zero_encoders = *encoders;

    /* 零点位置为 (0, 0) */
    state->current_pos.x = 0.0f;
    state->current_pos.y = 0.0f;
    state->target_pos.x  = 0.0f;
    state->target_pos.y  = 0.0f;

    /* 零点绳长即为 (0,0) 处的绳长 */
    point2d_t origin = {0.0f, 0.0f};
    kinematics_inverse(&origin, &state->initial_lengths);
    state->current_lengths = state->initial_lengths;
    state->target_lengths  = state->initial_lengths;

    state->is_zeroed = true;

    printf("[KINEMATICS] 归零完成\r\n");
    printf("  零点编码器: M1=%d M2=%d M3=%d M4=%d\r\n",
           encoders->m1, encoders->m2, encoders->m3, encoders->m4);
    printf("  原点绳长: L1=%.1f L2=%.1f L3=%.1f L4=%.1f mm\r\n",
           state->initial_lengths.L1, state->initial_lengths.L2,
           state->initial_lengths.L3, state->initial_lengths.L4);
}

/* ======================== 转换函数 ======================== */

int32_t kinematics_length_to_pulses(float delta_length_mm)
{
    /* ΔPulses = ΔLength(mm) × 51200 / 30 */
    return (int32_t)roundf(delta_length_mm * PULSES_PER_MM);
}

float kinematics_pulses_to_length(int32_t delta_pulses)
{
    /* ΔLength(mm) = ΔPulses × 30 / 51200 */
    return (float)delta_pulses / PULSES_PER_MM;
}

/* ======================== 范围检查 ======================== */

bool kinematics_is_in_range(const point2d_t *pos)
{
    if (pos == NULL) return false;
    return (fabsf(pos->x) <= CONTROL_RANGE_MAX && fabsf(pos->y) <= CONTROL_RANGE_MAX);
}

void kinematics_clamp_to_range(point2d_t *pos)
{
    if (pos == NULL) return;

    if (pos->x > CONTROL_RANGE_MAX)  pos->x = CONTROL_RANGE_MAX;
    if (pos->x < -CONTROL_RANGE_MAX) pos->x = -CONTROL_RANGE_MAX;
    if (pos->y > CONTROL_RANGE_MAX)  pos->y = CONTROL_RANGE_MAX;
    if (pos->y < -CONTROL_RANGE_MAX) pos->y = -CONTROL_RANGE_MAX;
}

/* ======================== 绳长变化量计算 ======================== */

void kinematics_compute_deltas(const kinematics_state_t *state,
                                const point2d_t *target_pos,
                                int32_t delta_pulses[4])
{
    if (state == NULL || target_pos == NULL || delta_pulses == NULL) return;

    /* 计算目标绳长 */
    rope_lengths_t target_lengths;
    kinematics_inverse(target_pos, &target_lengths);

    /* 计算绳长差值 → 脉冲差值 (正=需要放绳, 负=需要收绳) */
    float dL1 = target_lengths.L1 - state->current_lengths.L1;
    float dL2 = target_lengths.L2 - state->current_lengths.L2;
    float dL3 = target_lengths.L3 - state->current_lengths.L3;
    float dL4 = target_lengths.L4 - state->current_lengths.L4;

    delta_pulses[0] = kinematics_length_to_pulses(dL1);  /* M01 */
    delta_pulses[1] = kinematics_length_to_pulses(dL2);  /* M02 */
    delta_pulses[2] = kinematics_length_to_pulses(dL3);  /* M03 */
    delta_pulses[3] = kinematics_length_to_pulses(dL4);  /* M04 */
}
