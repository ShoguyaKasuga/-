#include "modbus_rtu.h"
#include "uart_dma.h"

// 16位 Modbus CRC 校验表（高低字节拆分，省去循环计算，大幅提升时钟效率）
static const uint8_t crc16_hi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};
static const uint8_t crc16_lo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0xC5, 0x05, 0x04, 0xC4
};

/**
 * @brief  计算 Modbus 专用 CRC16 校验和
 */
uint16_t Modbus_CalcCRC(uint8_t *ptr, uint16_t len) {
    uint8_t hi = 0xFF;
    uint8_t lo = 0xFF;
    uint16_t index;

    while (len--) {
        index = hi ^ *ptr++;
        hi = lo ^ crc16_hi[index];
        lo = crc16_lo[index];
    }
    return (uint16_t)((hi << 8) | lo);
}

/**
 * @brief  Modbus 协议帧解析核心
 * @note   在 main.c 的主循环中被触发，避免在中断里做耗时操作
 */
void Modbus_Process(void) {
    if (g_uart_rx_flag == 0) return; // 没有收到新数据包，直接返回

    // 边界条件防御：Modbus-RTU 最短的合法帧（地址+功能码+寄存器高+寄存器低+数量高+数量低+CRC高+CRC低）是 8 个字节
    if (g_uart_rx_len < 8 || g_uart_rx_len > UART_RX_MAX_LEN) {
        goto _frame_error;
    }

    // 1. 校验设备地址
    if (g_uart_rx_buf[0] != MODBUS_SLAVE_ADDR) {
        goto _frame_error;
    }

    // 2. 校验 CRC16
    uint16_t calc_crc = Modbus_CalcCRC(g_uart_rx_buf, g_uart_rx_len - 2);
    uint16_t recv_crc = (g_uart_rx_buf[g_uart_rx_len - 2] << 8) | g_uart_rx_buf[g_uart_rx_len - 1];
    if (calc_crc != recv_crc) {
        goto _frame_error; // 校验失败，可能是RS485总线受到电磁干扰
    }

    // 3. 执行功能码分支
    uint8_t function_code = g_uart_rx_buf[1];
    switch (function_code) {
        case 0x03: // 读保持寄存器
            // 提取目标寄存器起始地址和读取数量
            // 执行相应的寄存器数据拷贝，打包装入 TX 缓存发送回主机
            break;
            
        case 0x06: // 写单个寄存器
            // 提取寄存器地址和写入值，写入本地变量或保存到 Flash
            break;
            
        default:
            // 不支持的功能码，返回异常响应帧
            break;
    }

_frame_error:
    // 清除状态标志，释放缓存，等待下一次 DMA 接收
    g_uart_rx_flag = 0;
    g_uart_rx_len = 0;
}