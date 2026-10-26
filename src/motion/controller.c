/**
 ******************************************************************************
 * @file        controller.c
 * @brief       闭环运动控制器实现
 ******************************************************************************
 */

#include "controller.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ======================== 内部辅助 ======================== */

static void send_torque_to_motor(motion_controller_t *ctrl, uint8_t addr, uint8_t dir, uint16_t current)
{
    if (ctrl == NULL || ctrl->serial == NULL) return;

    uint8_t frame[16];
    uint8_t len = protocol_build_torque_cmd(addr, dir, current, frame);

    int sent = serial_send(ctrl->serial, frame, len);
    if (sent < 0) {
        ctrl->comm_errors++;
    }
}

static bool read_motor_position(motion_controller_t *ctrl, uint8_t addr, int32_t *out_pos)
{
    if (ctrl == NULL || ctrl->serial == NULL || out_pos == NULL) return false;

    uint8_t frame[16];
    uint8_t len = protocol_build_read_pos_cmd(addr, frame);

    /* 发送读取指令 */
    int sent = serial_send(ctrl->serial, frame, len);
    if (sent < 0) {
        ctrl->comm_errors++;
        return false;
    }

    /* 等待并接收响应 (超时50ms) */
    uint8_t rx_buf[64];
    int rx_len = serial_recv(ctrl->serial, rx_buf, sizeof(rx_buf), 50);

    if (rx_len < 6) {
        /* 无响应或帧太短 */
        ctrl->comm_errors++;
        return false;
    }

    /* 解析响应帧 */
    serial_frame_t resp;
    if (!protocol_parse_frame(rx_buf, (uint8_t)rx_len, &resp)) {
        ctrl->comm_errors++;
        return false;
    }

    /* 检查是否是位置响应 */
    if (resp.function_code != FCT_READ_POS) {
        return false;
    }

    /* 提取位置值 */
    *out_pos = protocol_extract_position(&resp);
    return true;
}

static void compute_torque_for_motor(float delta_pulses, uint8_t *dir, uint16_t *current)
{
    /*
     * 将脉冲差值转换为力矩指令:
     * - delta_pulses > 0: 需要放绳 → CCW(逆时针)
     * - delta_pulses < 0: 需要收绳 → CW(顺时针)
     * - |delta_pulses| 越大 → 力矩越大
     */

    float abs_delta = fabsf(delta_pulses);

    if (abs_delta < CTRL_DEADZONE_MM * PULSES_PER_MM) {
        /* 死区: 保持当前位置, 施加最小力矩 */
        *dir     = 0;
        *current = TORQUE_MIN_MA;
        return;
    }

    /* 方向 */
    *dir = (delta_pulses > 0) ? 1 : 0;  /* 正=放绳=CCW, 负=收绳=CW */

    /* 力矩大小: P控制器 */
    float torque = CTRL_KP * abs_delta;
    if (torque < TORQUE_MIN_MA) torque = TORQUE_MIN_MA;
    if (torque > TORQUE_MAX_MA) torque = TORQUE_MAX_MA;

    *current = (uint16_t)torque;
}

/* ======================== 公开函数 ======================== */

void motion_ctrl_init(motion_controller_t *ctrl, serial_handle_t serial)
{
    if (ctrl == NULL) return;

    memset(ctrl, 0, sizeof(motion_controller_t));
    ctrl->serial = serial;
    ctrl->state  = MOTION_STATE_IDLE;

    kinematics_init(&ctrl->kinematics);
    ctrl->joystick_target.x = 0.0f;
    ctrl->joystick_target.y = 0.0f;

    /* 默认假设所有电机在线 */
    for (int i = 0; i < 4; i++) {
        ctrl->motor_online[i] = 0;
    }

    printf("[CTRL] 运动控制器初始化完成\r\n");
}

motion_state_t motion_ctrl_tick(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return MOTION_STATE_ERROR;

    /* 急停状态不做任何操作 */
    if (ctrl->state == MOTION_STATE_ESTOP) {
        return MOTION_STATE_ESTOP;
    }

    ctrl->loop_count++;

    /* ===== 步骤1: 读取所有电机编码器位置 ===== */
    encoder_values_t encoders;
    int online_count = 0;

    const uint8_t *addrs = protocol_get_motor_addrs();
    for (int i = 0; i < MOTOR_NUM; i++) {
        int32_t pos = 0;
        if (read_motor_position(ctrl, addrs[i], &pos)) {
            ctrl->motor_online[i] = 1;
            online_count++;
        } else {
            ctrl->motor_online[i] = 0;
            /* 使用该电机上次的值作为fallback */
            switch (i) {
                case 0: pos = ctrl->last_encoders.m1; break;
                case 1: pos = ctrl->last_encoders.m2; break;
                case 2: pos = ctrl->last_encoders.m3; break;
                case 3: pos = ctrl->last_encoders.m4; break;
            }
        }

        /* 存储编码器值 */
        switch (i) {
            case 0: encoders.m1 = pos; break;
            case 1: encoders.m2 = pos; break;
            case 2: encoders.m3 = pos; break;
            case 3: encoders.m4 = pos; break;
        }
    }

    /* 如果所有电机都离线, 报错 */
    if (online_count == 0 && ctrl->loop_count > 5) {
        /* 允许前几个周期离线 (初始化阶段) */
    }

    /* ===== 步骤2: 正运动学计算当前位置 ===== */
    if (ctrl->kinematics.is_zeroed) {
        point2d_t current_pos;
        if (kinematics_forward(&ctrl->kinematics, &encoders, &current_pos)) {
            /* 位置更新成功 */
        }
    }

    ctrl->last_encoders = encoders;

    /* ===== 步骤3: 计算力矩指令 ===== */
    point2d_t target = ctrl->joystick_target;

    /* 裁剪目标到控制范围 */
    kinematics_clamp_to_range(&target);

    /* 处理归零请求 */
    if (ctrl->zero_triggered) {
        ctrl->zero_triggered = false;

        /* 软件层面归零 */
        kinematics_set_zero(&ctrl->kinematics, &encoders);

        /* 向每个电机发送当前位置清零指令 */
        for (int i = 0; i < MOTOR_NUM; i++) {
            uint8_t frame[16];
            uint8_t len = protocol_build_angle_zero_cmd(addrs[i], frame);
            serial_send(ctrl->serial, frame, len);
        }

        /* 清除残余力矩 */
        for (int i = 0; i < MOTOR_NUM; i++) {
            send_torque_to_motor(ctrl, addrs[i], 0, 0);
        }

        target.x = 0.0f;
        target.y = 0.0f;
        ctrl->joystick_target = target;
    }

    /* 处理抓取请求 */
    if (ctrl->grab_triggered) {
        ctrl->grab_triggered = false;
        ctrl->state = MOTION_STATE_GRABBING;

        /*
         * 抓取动作序列 (简化版):
         * 1. 当前位置已经是目标上方
         * 2. 增加力矩向下 (收绳/放绳组合实现Z轴运动)
         * 注: 实际抓取需要Z轴控制, 这里简化为短暂增大力矩
         */
        for (int i = 0; i < MOTOR_NUM; i++) {
            send_torque_to_motor(ctrl, addrs[i], 0, 800);
        }

        /* 抓取后返回IDLE */
        ctrl->state = MOTION_STATE_IDLE;
    }

    /* ===== 步骤4: 计算并发送各电机力矩指令 ===== */
    if (ctrl->kinematics.is_zeroed && ctrl->state != MOTION_STATE_GRABBING) {
        /* 计算各电机所需的绳长变化 */
        int32_t deltas[4];
        kinematics_compute_deltas(&ctrl->kinematics, &target, deltas);

        /* 更新运动学目标 */
        ctrl->kinematics.target_pos = target;
        rope_lengths_t temp_len;
        kinematics_inverse(&target, &temp_len);
        ctrl->kinematics.target_lengths = temp_len;

        /* 判断是否在移动 */
        float total_error = fabsf(target.x - ctrl->kinematics.current_pos.x) +
                           fabsf(target.y - ctrl->kinematics.current_pos.y);

        if (total_error > CTRL_DEADZONE_MM) {
            ctrl->state = MOTION_STATE_MOVING;

            /* 为每个电机计算力矩 */
            for (int i = 0; i < MOTOR_NUM; i++) {
                if (ctrl->motor_online[i]) {
                    uint8_t  dir;
                    uint16_t current;
                    compute_torque_for_motor((float)deltas[i], &dir, &current);
                    send_torque_to_motor(ctrl, addrs[i], dir, current);
                }
            }
        } else {
            /* 到达目标, 保持位置 (施加最小力矩) */
            ctrl->state = MOTION_STATE_HOLDING;
            for (int i = 0; i < MOTOR_NUM; i++) {
                if (ctrl->motor_online[i]) {
                    send_torque_to_motor(ctrl, addrs[i], 0, TORQUE_MIN_MA);
                }
            }
        }
    }

    return ctrl->state;
}

void motion_ctrl_set_target(motion_controller_t *ctrl, float x, float y)
{
    if (ctrl == NULL) return;

    ctrl->joystick_target.x = x;
    ctrl->joystick_target.y = y;

    /* 裁剪范围 */
    kinematics_clamp_to_range(&ctrl->joystick_target);
}

void motion_ctrl_trigger_grab(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return;
    ctrl->grab_triggered = true;

    /* 先停止运动 */
    if (ctrl->serial) {
        const uint8_t *addrs = protocol_get_motor_addrs();
        for (int i = 0; i < MOTOR_NUM; i++) {
            uint8_t frame[16];
            uint8_t len = protocol_build_stop_cmd(addrs[i], frame);
            serial_send(ctrl->serial, frame, len);
        }
    }
}

void motion_ctrl_trigger_zero(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return;
    ctrl->zero_triggered = true;
}

void motion_ctrl_emergency_stop(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return;

    ctrl->state = MOTION_STATE_ESTOP;

    /* 向所有电机发送停止指令 */
    if (ctrl->serial) {
        const uint8_t *addrs = protocol_get_motor_addrs();
        for (int i = 0; i < MOTOR_NUM; i++) {
            uint8_t frame[16];
            uint8_t len = protocol_build_stop_cmd(addrs[i], frame);
            serial_send(ctrl->serial, frame, len);
        }
    }

    printf("[CTRL] 紧急停止!\r\n");
}

void motion_ctrl_enable_motors(motion_controller_t *ctrl)
{
    if (ctrl == NULL || ctrl->serial == NULL) return;

    const uint8_t *addrs = protocol_get_motor_addrs();
    for (int i = 0; i < MOTOR_NUM; i++) {
        /* 先清除可能存在的错误状态 */
        uint8_t frame[16];
        uint8_t len = protocol_build_clear_state_cmd(addrs[i], frame);
        serial_send(ctrl->serial, frame, len);

        /* 使能电机 */
        len = protocol_build_enable_cmd(addrs[i], 0, frame);  /* 0=使能 */
        serial_send(ctrl->serial, frame, len);
    }

    printf("[CTRL] 所有电机已使能\r\n");
}

void motion_ctrl_disable_motors(motion_controller_t *ctrl)
{
    if (ctrl == NULL || ctrl->serial == NULL) return;

    const uint8_t *addrs = protocol_get_motor_addrs();
    for (int i = 0; i < MOTOR_NUM; i++) {
        /* 先停止 */
        uint8_t frame[16];
        uint8_t len = protocol_build_stop_cmd(addrs[i], frame);
        serial_send(ctrl->serial, frame, len);

        /* 失能 */
        len = protocol_build_enable_cmd(addrs[i], 1, frame);  /* 1=失能 */
        serial_send(ctrl->serial, frame, len);
    }

    printf("[CTRL] 所有电机已失能\r\n");
}

void motion_ctrl_get_position(motion_controller_t *ctrl, float *x, float *y)
{
    if (ctrl == NULL) return;
    if (x) *x = ctrl->kinematics.current_pos.x;
    if (y) *y = ctrl->kinematics.current_pos.y;
}

motion_state_t motion_ctrl_get_state(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return MOTION_STATE_ERROR;
    return ctrl->state;
}

void motion_ctrl_get_motor_online(motion_controller_t *ctrl, uint8_t online[4])
{
    if (ctrl == NULL || online == NULL) return;
    memcpy(online, ctrl->motor_online, sizeof(uint8_t) * 4);
}

uint32_t motion_ctrl_get_comm_errors(motion_controller_t *ctrl)
{
    if (ctrl == NULL) return 0;
    return ctrl->comm_errors;
}
