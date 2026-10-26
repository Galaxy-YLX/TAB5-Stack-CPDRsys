/**
 ******************************************************************************
 * @file        protocol.c
 * @brief       电机通信协议层实现
 ******************************************************************************
 */

#include "protocol.h"
#include <string.h>
#include <stdio.h>

/* -------------------------- 电机地址列表 -------------------------- */

static const uint8_t motor_addrs[MOTOR_NUM] = {
    MOTOR_ADDR_M01, MOTOR_ADDR_M02, MOTOR_ADDR_M03, MOTOR_ADDR_M04
};

const uint8_t* protocol_get_motor_addrs(void)
{
    return motor_addrs;
}

/* -------------------------- 校验计算 -------------------------- */

uint8_t protocol_checksum(const uint8_t *data, uint8_t length)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

/* -------------------------- 指令构建 -------------------------- */

uint8_t protocol_build_torque_cmd(uint8_t addr, uint8_t dir, uint16_t current, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_TORQUE_MODE;                   /* 功能码 */
    out_buf[3] = dir;                               /* 方向 */
    out_buf[4] = (uint8_t)((current >> 8) & 0xFF);  /* 电流高字节 */
    out_buf[5] = (uint8_t)((current >> 0) & 0xFF);  /* 电流低字节 */
    out_buf[6] = protocol_checksum(out_buf, 6);     /* 校验 */
    out_buf[7] = FRAME_TAIL;                        /* 帧尾 */
    return 8;
}

uint8_t protocol_build_read_pos_cmd(uint8_t addr, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_READ_POS;                      /* 功能码 */
    out_buf[3] = protocol_checksum(out_buf, 3);     /* 校验 */
    out_buf[4] = FRAME_TAIL;                        /* 帧尾 */
    return 5;
}

uint8_t protocol_build_enable_cmd(uint8_t addr, uint8_t en, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_MOTOR_ENABLE;                  /* 功能码 */
    out_buf[3] = en;                                /* 使能标志 */
    out_buf[4] = protocol_checksum(out_buf, 4);     /* 校验 */
    out_buf[5] = FRAME_TAIL;                        /* 帧尾 */
    return 6;
}

uint8_t protocol_build_stop_cmd(uint8_t addr, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_STOP_NOW;                      /* 功能码 */
    out_buf[3] = protocol_checksum(out_buf, 3);     /* 校验 */
    out_buf[4] = FRAME_TAIL;                        /* 帧尾 */
    return 5;
}

uint8_t protocol_build_angle_zero_cmd(uint8_t addr, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_ANGLE_ZERO;                    /* 功能码 */
    out_buf[3] = protocol_checksum(out_buf, 3);     /* 校验 */
    out_buf[4] = FRAME_TAIL;                        /* 帧尾 */
    return 5;
}

uint8_t protocol_build_clear_state_cmd(uint8_t addr, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_CLEAR_STATE;                   /* 功能码 */
    out_buf[3] = protocol_checksum(out_buf, 3);     /* 校验 */
    out_buf[4] = FRAME_TAIL;                        /* 帧尾 */
    return 5;
}

uint8_t protocol_build_read_motor_sta_cmd(uint8_t addr, uint8_t *out_buf)
{
    out_buf[0] = FRAME_HEAD;                        /* 帧头 */
    out_buf[1] = addr;                              /* 地址 */
    out_buf[2] = FCT_READ_MOTOR_STA;                /* 功能码 */
    out_buf[3] = protocol_checksum(out_buf, 3);     /* 校验 */
    out_buf[4] = FRAME_TAIL;                        /* 帧尾 */
    return 5;
}

/* -------------------------- 帧解析 -------------------------- */

bool protocol_parse_frame(const uint8_t *buffer, uint8_t len, serial_frame_t *frame)
{
    if (frame == NULL || buffer == NULL) return false;

    memset(frame, 0, sizeof(serial_frame_t));

    /* 最小帧长检查: HEAD(1) + ADDR(1) + CMD(1) + ERR(1) + CHK(1) + TAIL(1) = 6 */
    if (len < 6) {
        printf("[PROTOCOL] 帧太短: %d字节\r\n", len);
        return false;
    }

    /* 帧头检查 */
    if (buffer[0] != FRAME_HEAD) {
        printf("[PROTOCOL] 帧头错误: 0x%02X\r\n", buffer[0]);
        return false;
    }

    /* 帧尾检查 */
    if (buffer[len - 1] != FRAME_TAIL) {
        printf("[PROTOCOL] 帧尾错误: 0x%02X\r\n", buffer[len - 1]);
        return false;
    }

    /* 校验和检查 */
    uint8_t calc_sum = protocol_checksum(buffer, len - 2);
    if (calc_sum != buffer[len - 2]) {
        printf("[PROTOCOL] 校验错误: 计算=0x%02X, 接收=0x%02X\r\n", calc_sum, buffer[len - 2]);
        return false;
    }

    /* 提取帧内容 */
    frame->slave_addr   = buffer[1];
    frame->function_code = buffer[2];
    frame->error_code   = buffer[3];
    frame->checksum     = buffer[len - 2];
    frame->data_len     = len - 6;  /* 减去 HEAD(1)+ADDR(1)+CMD(1)+ERR(1)+CHK(1)+TAIL(1) */

    if (frame->data_len > 0 && frame->data_len <= sizeof(frame->data)) {
        memcpy(frame->data, &buffer[4], frame->data_len);
    } else {
        frame->data_len = 0;
    }

    return true;
}

int32_t protocol_extract_position(const serial_frame_t *frame)
{
    if (frame == NULL || frame->data_len < 4) return 0;

    /* 大端序 4字节 → int32_t */
    int32_t pos = (int32_t)(
        ((uint32_t)frame->data[0] << 24) |
        ((uint32_t)frame->data[1] << 16) |
        ((uint32_t)frame->data[2] << 8)  |
        ((uint32_t)frame->data[3] << 0)
    );
    return pos;
}

/* -------------------------- 默认发送回调 (由HAL层重写) -------------------------- */

/*
 * 跨平台弱符号定义:
 * - GCC/Clang: __attribute__((weak))
 * - MSVC: 需要链接器 /ALTERNATENAME 或使用条件编译
 *
 * 简化方案: 默认空实现, 由实际HAL层在链接时覆盖 (需要确保HAL的.o先于protocol.o链接)
 * 更好的做法是使用函数指针回调, 这里为保留与原始STM32代码兼容而使用弱符号
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void protocol_send_raw(const uint8_t *data, uint8_t len)
{
    /* 默认空实现, 由串口HAL层重写 */
    (void)data;
    (void)len;
}
