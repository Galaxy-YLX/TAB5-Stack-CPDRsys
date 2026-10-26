/**
 ******************************************************************************
 * @file        serial.h
 * @brief       Windows串口通信抽象层
 * @details     通过USB转RS485连接4个电机, 波特率115200, 8N1
 ******************************************************************************
 */

#ifndef __SERIAL_HAL_H
#define __SERIAL_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 串口配置 */
#define SERIAL_DEFAULT_BAUD     115200
#define SERIAL_DEFAULT_PORT     "COM3"      /* 默认串口号 */
#define SERIAL_RX_BUF_SIZE      512         /* 接收缓冲区 */

/* 串口句柄 (Win32 HANDLE) */
typedef void* serial_handle_t;

/**
 * @brief   初始化并打开串口
 * @param   port_name   串口名称, 如 "COM3"
 * @param   baud_rate   波特率
 * @return  串口句柄, NULL表示失败
 */
serial_handle_t serial_open(const char *port_name, uint32_t baud_rate);

/**
 * @brief   关闭串口
 * @param   handle  串口句柄
 */
void serial_close(serial_handle_t handle);

/**
 * @brief   发送数据
 * @param   handle  串口句柄
 * @param   data    数据缓冲区
 * @param   len     数据长度
 * @return  实际发送的字节数, 负数表示错误
 */
int serial_send(serial_handle_t handle, const uint8_t *data, uint8_t len);

/**
 * @brief   接收数据 (非阻塞)
 * @param   handle      串口句柄
 * @param   buffer      接收缓冲区
 * @param   max_len     最大接收长度
 * @param   timeout_ms  超时时间(ms)
 * @return  实际接收的字节数, 负数表示错误
 */
int serial_recv(serial_handle_t handle, uint8_t *buffer, int max_len, int timeout_ms);

/**
 * @brief   清空串口接收缓冲区
 * @param   handle  串口句柄
 */
void serial_flush_rx(serial_handle_t handle);

/**
 * @brief   检查串口是否已打开
 * @param   handle  串口句柄
 * @return  true=已打开
 */
bool serial_is_open(serial_handle_t handle);

/**
 * @brief   列出可用串口 (调试用)
 */
void serial_list_ports(void);

#ifdef __cplusplus
}
#endif

#endif /* __SERIAL_HAL_H */
