/**
 ******************************************************************************
 * @file        serial.c
 * @brief       Windows串口通信实现
 * @details     使用Win32 API操作串口, 支持USB转RS485
 ******************************************************************************
 */

#include "serial.h"
#include <windows.h>
#include <stdio.h>

/* 将端口名转为Win32设备路径 */
static void make_device_path(const char *port_name, char *device_path, int max_len)
{
    /* COM端口号超过9时需要使用 \\.\COMxx 格式 */
    if (port_name[0] == '\\') {
        /* 已经是完整路径 */
        snprintf(device_path, max_len, "%s", port_name);
    } else {
        snprintf(device_path, max_len, "\\\\.\\%s", port_name);
    }
}

serial_handle_t serial_open(const char *port_name, uint32_t baud_rate)
{
    char device_path[64];
    make_device_path(port_name, device_path, sizeof(device_path));

    /* 打开串口 */
    HANDLE h = CreateFileA(
        device_path,
        GENERIC_READ | GENERIC_WRITE,
        0,                          /* 独占访问 */
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("[SERIAL] 无法打开 %s, 错误码: %lu\r\n", device_path, err);
        return NULL;
    }

    /* 配置串口参数 */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(h, &dcb)) {
        printf("[SERIAL] 获取串口配置失败\r\n");
        CloseHandle(h);
        return NULL;
    }

    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;   /* RS485需要DTR */
    dcb.fRtsControl = RTS_CONTROL_ENABLE;   /* RS485发送使能 */

    if (!SetCommState(h, &dcb)) {
        printf("[SERIAL] 设置串口配置失败\r\n");
        CloseHandle(h);
        return NULL;
    }

    /* 设置超时 */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 5;      /* 字节间隔超时 5ms */
    timeouts.ReadTotalTimeoutMultiplier  = 1;      /* 每字节附加 1ms */
    timeouts.ReadTotalTimeoutConstant    = 20;     /* 总超时常量 20ms */
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;

    if (!SetCommTimeouts(h, &timeouts)) {
        printf("[SERIAL] 设置超时失败\r\n");
        CloseHandle(h);
        return NULL;
    }

    /* 设置缓冲区大小 */
    SetupComm(h, SERIAL_RX_BUF_SIZE, SERIAL_RX_BUF_SIZE);

    /* 清空缓冲区 */
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    printf("[SERIAL] %s 打开成功, 波特率=%d\r\n", port_name, baud_rate);
    return (serial_handle_t)h;
}

void serial_close(serial_handle_t handle)
{
    if (handle != NULL) {
        PurgeComm((HANDLE)handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        CloseHandle((HANDLE)handle);
        printf("[SERIAL] 串口已关闭\r\n");
    }
}

int serial_send(serial_handle_t handle, const uint8_t *data, uint8_t len)
{
    if (handle == NULL || data == NULL || len == 0) return -1;

    DWORD bytes_written = 0;
    if (!WriteFile((HANDLE)handle, data, len, &bytes_written, NULL)) {
        printf("[SERIAL] 发送失败, 错误码: %lu\r\n", GetLastError());
        return -1;
    }

    FlushFileBuffers((HANDLE)handle);
    return (int)bytes_written;
}

int serial_recv(serial_handle_t handle, uint8_t *buffer, int max_len, int timeout_ms)
{
    if (handle == NULL || buffer == NULL || max_len <= 0) return -1;

    /* 设置读取超时 */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 5;
    timeouts.ReadTotalTimeoutMultiplier  = 1;
    timeouts.ReadTotalTimeoutConstant    = (DWORD)timeout_ms;

    if (!SetCommTimeouts((HANDLE)handle, &timeouts)) {
        return -1;
    }

    DWORD bytes_read = 0;
    if (!ReadFile((HANDLE)handle, buffer, (DWORD)max_len, &bytes_read, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            return 0;  /* 超时无数据 */
        }
        return -1;
    }

    return (int)bytes_read;
}

void serial_flush_rx(serial_handle_t handle)
{
    if (handle != NULL) {
        PurgeComm((HANDLE)handle, PURGE_RXCLEAR);
    }
}

bool serial_is_open(serial_handle_t handle)
{
    if (handle == NULL) return false;

    DWORD errors;
    COMSTAT stat;
    return ClearCommError((HANDLE)handle, &errors, &stat) ? true : false;
}

void serial_list_ports(void)
{
    printf("[SERIAL] 扫描可用串口:\r\n");
    char port_name[16];

    for (int i = 1; i <= 32; i++) {
        snprintf(port_name, sizeof(port_name), "\\\\.\\COM%d", i);
        HANDLE h = CreateFileA(
            port_name,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL
        );
        if (h != INVALID_HANDLE_VALUE) {
            printf("  -> COM%d 可用\r\n", i);
            CloseHandle(h);
        }
    }
    printf("[SERIAL] 扫描完成\r\n");
}
