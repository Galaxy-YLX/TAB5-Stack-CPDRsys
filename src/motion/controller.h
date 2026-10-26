/**
 ******************************************************************************
 * @file        controller.h
 * @brief       闭环运动控制器
 * @details
 *     控制架构:
 *       1. 位置检测: 读取4个电机编码器 → 正运动学计算当前位置
 *       2. 力矩移动: 向目标位置移动,使用P控制器将位置误差转换为力矩指令
 *
 *     控制参数:
 *       力矩范围: 150~1500 mA
 *       控制频率: ~50Hz (20ms周期)
 *
 *     安全特性:
 *       - 位置边界保护 (±200mm)
 *       - 力矩限幅
 *       - 通信失败检测
 *       - 紧急停止支持
 ******************************************************************************
 */

#ifndef __MOTION_CONTROLLER_H
#define __MOTION_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "../motor/protocol.h"
#include "../motor/serial.h"
#include "kinematics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 控制参数 ======================== */

#define CTRL_LOOP_PERIOD_MS     20          /* 控制周期 (ms) */
#define CTRL_KP                 8.0f        /* 位置误差→力矩增益 */
#define CTRL_DEADZONE_MM        0.5f        /* 位置死区 (mm) */
#define CTRL_MAX_SPEED_PPS      20000       /* 最大脉冲速率 (pulses/s) */

/* ======================== 状态枚举 ======================== */

typedef enum {
    MOTION_STATE_IDLE,          /* 空闲 */
    MOTION_STATE_MOVING,        /* 运动中 */
    MOTION_STATE_HOLDING,       /* 保持位置 */
    MOTION_STATE_GRABBING,      /* 抓取中 */
    MOTION_STATE_ERROR,         /* 错误 */
    MOTION_STATE_ESTOP,         /* 急停 */
} motion_state_t;

/* ======================== 数据结构 ======================== */

/* 单个电机的力矩指令 */
typedef struct {
    uint8_t  dir;       /* 0=CW(顺时针/收绳), 1=CCW(逆时针/放绳) */
    uint16_t current;   /* 力矩电流 (mA), 范围 150~1500 */
} motor_torque_cmd_t;

/* 运动控制器 */
typedef struct {
    motion_state_t      state;              /* 当前状态 */
    kinematics_state_t  kinematics;         /* 运动学状态 */
    motor_status_t      motor_status[4];    /* 4个电机状态 */
    encoder_values_t    last_encoders;      /* 上一周期编码器值 */
    serial_handle_t     serial;             /* 串口句柄 */
    point2d_t           joystick_target;    /* 摇杆目标位置 */
    bool                grab_triggered;     /* 抓取触发标志 */
    bool                zero_triggered;     /* 归零触发标志 */
    uint32_t            loop_count;         /* 控制循环计数 */
    uint32_t            comm_errors;        /* 通信错误计数 */
    uint8_t             motor_online[4];    /* 电机在线标志 */
} motion_controller_t;

/* ======================== 函数声明 ======================== */

/**
 * @brief   初始化运动控制器
 * @param   ctrl    控制器指针
 * @param   serial  已打开的串口句柄
 */
void motion_ctrl_init(motion_controller_t *ctrl, serial_handle_t serial);

/**
 * @brief   执行一次控制循环
 * @details 1) 读取所有电机编码器位置
 *          2) 正运动学计算当前位置
 *          3) 根据摇杆目标或抓取状态计算力矩指令
 *          4) 发送力矩指令到各电机
 * @param   ctrl    控制器指针
 * @return  控制器当前状态
 */
motion_state_t motion_ctrl_tick(motion_controller_t *ctrl);

/**
 * @brief   设置摇杆目标位置
 * @param   ctrl    控制器指针
 * @param   x       X轴目标 (-200 ~ +200 mm)
 * @param   y       Y轴目标 (-200 ~ +200 mm)
 */
void motion_ctrl_set_target(motion_controller_t *ctrl, float x, float y);

/**
 * @brief   触发抓取操作
 * @param   ctrl    控制器指针
 */
void motion_ctrl_trigger_grab(motion_controller_t *ctrl);

/**
 * @brief   触发归零操作 (设置当前位置为0,0)
 * @param   ctrl    控制器指针
 */
void motion_ctrl_trigger_zero(motion_controller_t *ctrl);

/**
 * @brief   紧急停止所有电机
 * @param   ctrl    控制器指针
 */
void motion_ctrl_emergency_stop(motion_controller_t *ctrl);

/**
 * @brief   使能所有电机
 * @param   ctrl    控制器指针
 */
void motion_ctrl_enable_motors(motion_controller_t *ctrl);

/**
 * @brief   失能所有电机
 * @param   ctrl    控制器指针
 */
void motion_ctrl_disable_motors(motion_controller_t *ctrl);

/**
 * @brief   获取当前位置
 * @param   ctrl    控制器指针
 * @param   x       输出X坐标
 * @param   y       输出Y坐标
 */
void motion_ctrl_get_position(motion_controller_t *ctrl, float *x, float *y);

/**
 * @brief   获取控制器状态
 * @param   ctrl    控制器指针
 * @return  状态枚举值
 */
motion_state_t motion_ctrl_get_state(motion_controller_t *ctrl);

/**
 * @brief   获取电机在线状态
 * @param   ctrl    控制器指针
 * @param   online  输出: 4个电机的在线状态
 */
void motion_ctrl_get_motor_online(motion_controller_t *ctrl, uint8_t online[4]);

/**
 * @brief   获取通信错误计数
 * @param   ctrl    控制器指针
 * @return  错误计数
 */
uint32_t motion_ctrl_get_comm_errors(motion_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* __MOTION_CONTROLLER_H */
