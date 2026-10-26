/**
 ******************************************************************************
 * @file        protocol.h
 * @brief       电机通信协议层 - 帧构建、校验、解析
 * @details     基于正点原子SMD电机自定义协议
 *              RS485网络，4个电机地址 0x01~0x04
 *              帧格式: HEAD(0xC5) + ADDR(1B) + CMD(1B) + DATA(0~8B) + CHK(1B) + TAIL(0x5C)
 ******************************************************************************
 */

#ifndef __MOTOR_PROTOCOL_H
#define __MOTOR_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 常量定义 ======================== */

#define FRAME_HEAD              0xC5        /* 帧头 */
#define FRAME_TAIL              0x5C        /* 帧尾 */
#define FRAME_MIN_LEN           5           /* 最小帧长 */
#define FRAME_MAX_DATA          64          /* 最大数据段 */
#define FRAME_MAX_LEN           (FRAME_MAX_DATA + 5)  /* 最大帧长 */

#define MOTOR_ADDR_M01          0x01        /* 电机1地址 */
#define MOTOR_ADDR_M02          0x02        /* 电机2地址 */
#define MOTOR_ADDR_M03          0x03        /* 电机3地址 */
#define MOTOR_ADDR_M04          0x04        /* 电机4地址 */
#define MOTOR_ADDR_BROADCAST    0x00        /* 广播地址 */

#define MOTOR_NUM               4           /* 电机数量 */

/* 编码器分辨率: 51200脉冲/圈, 1圈=30mm绳长变化 */
#define ENCODER_PULSES_PER_REV  51200
#define ROPE_LENGTH_PER_REV_MM  30.0f
#define PULSES_PER_MM           (ENCODER_PULSES_PER_REV / ROPE_LENGTH_PER_REV_MM)

/* 力矩范围 */
#define TORQUE_MIN_MA           150
#define TORQUE_MAX_MA           1500

/* ======================== 功能码枚举 ======================== */

typedef enum {
    /* 系统指令 */
    FCT_IDLE                    = 0x00,
    FCT_CAL_ENCODER             = 0x01,
    FCT_RESTART                 = 0x02,
    FCT_RESET_FACTORY           = 0x03,
    FCT_PARAM_SAVE              = 0x04,

    /* 查询类指令 */
    FCT_READ_SOFT_HARD_VER      = 0x20,
    FCT_READ_PSI                = 0x21,
    FCT_READ_PHASE_RES_IND      = 0x22,
    FCT_READ_PHASE_MA           = 0x23,
    FCT_READ_VOL                = 0x24,
    FCT_READ_MA_PID             = 0x25,
    FCT_READ_SPEED_PID          = 0x26,
    FCT_READ_POS_PID            = 0x27,
    FCT_READ_TOTAL_PULSE        = 0x28,
    FCT_READ_ROTATE_SPEED       = 0x29,
    FCT_READ_POS                = 0x2A,     /* 读取实时位置 */
    FCT_READ_POS_ERROR          = 0x2B,
    FCT_READ_MOTOR_STA          = 0x2C,     /* 读取电机状态 */
    FCT_READ_CLOG_FLAG          = 0x2D,
    FCT_READ_CLOG_CUR           = 0x2E,
    FCT_READ_ENABLE_STA         = 0x2F,
    FCT_READ_ARRIVED_STA        = 0x30,
    FCT_READ_SYS_PARAM          = 0x31,
    FCT_READ_DRIVE_PARAMS       = 0x32,

    /* 设置参数指令 */
    FCT_SET_SLAVE_ADD           = 0x60,
    FCT_SET_GROUP_ADD           = 0x61,
    FCT_SET_MODE                = 0x62,
    FCT_SET_POS_PID             = 0x63,
    FCT_SET_POS_TORQUE          = 0x64,
    FCT_SET_STEP                = 0x65,
    FCT_SET_MA                  = 0x66,
    FCT_SET_UART_BAUD           = 0x67,
    FCT_SET_CAN_BAUD            = 0x68,
    FCT_SET_MODBUS              = 0x69,
    FCT_SET_CLOG_PRO            = 0x6A,
    FCT_SET_CLOG_CUR            = 0x6B,
    FCT_SET_CAN_ID              = 0x6C,
    FCT_SET_DIR_LEVEL           = 0x6D,
    FCT_SET_EN_LEVEL            = 0x6E,
    FCT_SET_CMD_ECHO            = 0x6F,
    FCT_SET_KEY_LOCK            = 0x70,
    FCT_SET_AUTO_NOT_DISPLAY    = 0x71,
    FCT_SET_IO_START_LEVEL      = 0x72,
    FCT_SET_SPEED_PID           = 0x73,

    /* 回零/原点指令 */
    FCT_ORIGIN_SET_LEFT_POS     = 0x90,
    FCT_ORIGIN_LIMIT_HOME       = 0x91,
    FCT_ORIGIN_TRIG             = 0x92,
    FCT_ORIGIN_BREAK            = 0x93,
    FCT_ORIGIN_READ_PARAMS      = 0x94,
    FCT_ORIGIN_SET_PARAMS       = 0x95,
    FCT_ORIGIN_READ_STA         = 0x96,
    FCT_ORIGIN_AOTO_ZERO        = 0x97,
    FCT_ORIGIN_SET_RIGHT_POS    = 0x98,
    FCT_ORIGIN_SWITCH           = 0x99,

    /* 运动控制指令 */
    FCT_OL_SPEED_MODE           = 0xE0,
    FCT_OL_POS_MODE             = 0xE1,
    FCT_OL_POS_REL_MODE         = 0xE2,
    FCT_OL_PULSES_MODE          = 0xE3,
    FCT_IO_RUN_MODE             = 0xE4,

    FCT_TORQUE_MODE             = 0xF0,     /* 力矩模式 */
    FCT_SPEED_MODE              = 0xF1,
    FCT_POS_MODE                = 0xF2,
    FCT_POS_REL_MODE            = 0xF3,
    FCT_PULSES_MODE             = 0xF4,
    FCT_PULSE_WIDTH_POS_MODE    = 0xF5,
    FCT_PULSE_WIDTH_MA_MODE     = 0xF6,
    FCT_PULSE_WIDTH_SPEED_MODE  = 0xF7,
    FCT_ANGLE_ZERO              = 0xF8,     /* 当前角度清零 */
    FCT_CLEAR_CLOG_PRO          = 0xF9,
    FCT_MOTOR_ENABLE            = 0xFA,     /* 使能控制 */
    FCT_CLEAR_STATE             = 0xFB,     /* 清除错误状态 */
    FCT_STOP_NOW                = 0xFC,     /* 立即停止 */
} FUN_CODE_TYPE;

/* ======================== 应答状态 ======================== */

typedef enum {
    ACK_SUCCEED                 = 0x01,
    ACK_FRAME_TOO_SHORT         = 0xE1,
    ACK_INVALID_HEADER          = 0xE2,
    ACK_INVALID_FOOTER          = 0xE3,
    ACK_CHECKSUM_MISMATCH       = 0xE4,
    ACK_UNSUPPORTED_FUNCTION    = 0xE5,
    ACK_ERR_ILLEGAL_VAL         = 0xE6,
} ACK_STA;

/* ======================== 数据结构 ======================== */

/* 电机状态信息 */
typedef struct {
    int32_t  position;          /* 当前位置 (编码器脉冲数) */
    int32_t  total_pulses;      /* 累计脉冲数 */
    int16_t  speed_rpm;         /* 实时转速 (RPM) */
    int16_t  current_ma;        /* 当前电流 (mA) */
    int16_t  pos_error;         /* 位置误差 */
    uint8_t  motor_state;       /* 电机状态: 0空闲/1加速/2匀速/3减速/4堵转/5欠压 */
    uint8_t  enable_state;      /* 使能状态: 0使能/1失能 */
    uint8_t  arrived_state;     /* 到位状态: 0未到位/1到位 */
    uint8_t  clog_flag;         /* 堵转标志 */
} motor_status_t;

/* 解析后的帧结构 */
typedef struct {
    uint8_t  slave_addr;        /* 从机地址 */
    uint8_t  function_code;     /* 功能码 */
    uint8_t  error_code;        /* 错误码 */
    uint8_t  data[128];         /* 数据段 */
    uint8_t  data_len;          /* 数据长度 */
    uint8_t  checksum;          /* 校验和 */
} serial_frame_t;

/* ======================== 函数声明 ======================== */

/**
 * @brief   计算累加和校验
 * @param   data    数据缓冲区
 * @param   length  数据长度
 * @return  校验和 (低8位)
 */
uint8_t protocol_checksum(const uint8_t *data, uint8_t length);

/**
 * @brief   构建力矩模式指令帧
 * @param   addr    电机地址
 * @param   dir     方向: 0=CW顺时针, 非0=CCW逆时针
 * @param   current 电流值 0~3000mA (本项目使用150~1500)
 * @param   out_buf 输出缓冲区 (至少8字节)
 * @return  帧长度
 */
uint8_t protocol_build_torque_cmd(uint8_t addr, uint8_t dir, uint16_t current, uint8_t *out_buf);

/**
 * @brief   构建读取位置指令帧
 * @param   addr    电机地址
 * @param   out_buf 输出缓冲区 (至少5字节)
 * @return  帧长度
 */
uint8_t protocol_build_read_pos_cmd(uint8_t addr, uint8_t *out_buf);

/**
 * @brief   构建电机使能指令帧
 * @param   addr    电机地址
 * @param   en      0=使能, 1=失能
 * @param   out_buf 输出缓冲区 (至少6字节)
 * @return  帧长度
 */
uint8_t protocol_build_enable_cmd(uint8_t addr, uint8_t en, uint8_t *out_buf);

/**
 * @brief   构建立即停止指令帧
 * @param   addr    电机地址
 * @param   out_buf 输出缓冲区 (至少5字节)
 * @return  帧长度
 */
uint8_t protocol_build_stop_cmd(uint8_t addr, uint8_t *out_buf);

/**
 * @brief   构建当前位置清零指令帧
 * @param   addr    电机地址
 * @param   out_buf 输出缓冲区 (至少5字节)
 * @return  帧长度
 */
uint8_t protocol_build_angle_zero_cmd(uint8_t addr, uint8_t *out_buf);

/**
 * @brief   构建清除状态指令帧
 * @param   addr    电机地址
 * @param   out_buf 输出缓冲区 (至少5字节)
 * @return  帧长度
 */
uint8_t protocol_build_clear_state_cmd(uint8_t addr, uint8_t *out_buf);

/**
 * @brief   构建读取电机状态指令帧
 * @param   addr    电机地址
 * @param   out_buf 输出缓冲区 (至少5字节)
 * @return  帧长度
 */
uint8_t protocol_build_read_motor_sta_cmd(uint8_t addr, uint8_t *out_buf);

/**
 * @brief   发送原始帧数据 (由HAL层实现)
 * @param   data    帧数据
 * @param   len     数据长度
 */
void protocol_send_raw(const uint8_t *data, uint8_t len);

/**
 * @brief   解析串口接收到的帧
 * @param   buffer  原始数据缓冲区
 * @param   len     数据长度
 * @param   frame   解析结果输出
 * @return  true=解析成功, false=解析失败
 */
bool protocol_parse_frame(const uint8_t *buffer, uint8_t len, serial_frame_t *frame);

/**
 * @brief   从帧数据中提取int32_t位置值 (大端序, 4字节)
 * @param   frame   解析后的帧
 * @return  位置值 (脉冲数)
 */
int32_t protocol_extract_position(const serial_frame_t *frame);

/**
 * @brief   获取所有电机地址数组
 * @return  电机地址数组指针
 */
const uint8_t* protocol_get_motor_addrs(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_PROTOCOL_H */
