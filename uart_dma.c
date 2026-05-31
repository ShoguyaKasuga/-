#include "uart_dma.h"
#include "string.h"

uint8_t g_uart_rx_buf[UART_RX_MAX_LEN]; // DMA 硬件直接写入的缓冲区
volatile uint8_t g_uart_rx_flag = 0;    // 接收完成标志位
volatile uint16_t g_uart_rx_len = 0;   // 实际接收到的字节数

/**
 * @brief  初始化串口2的 DMA 接收配置（以STM32标准库/HAL架构为例）
 */
void UART2_DMA_Init(void) {
    // 1. 配置串口时钟、GPIO引脚（TX推挽输出，RX浮空或上拉输入）
    // 2. 配置 USART2 参数：9600/115200 bps, 8位数据, 1位停止, 无校验
    
    // 3. 核心配置：开启串口的 DMA 接收请求 (USART_CR3_DMAR)
    // SET_BIT(USART2->CR3, USART_CR3_DMAR);
    
    // 4. 配置 DMA 通道：外设地址指向 USART2->DR，内存地址指向 g_uart_rx_buf
    // 开启循环模式（Circular）或单次模式，设置传输方向为外设到内存
    
    // 5. 核心配置：清除串口空闲中断标志，并使能空闲中断 (IDLE Interrupt)
    // __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

/**
 * @brief  串口中断服务函数，放在 stm32f1xx_it.c 的 USART2_IRQHandler 中调用
 */
void UART2_IRQHandler_Callback(void) {
    // 判断是否触发了空闲中断（线路上一个字节传输完成，且停顿超过1个字节的时间）
    if ((USART2->SR & (1 << 4)) != 0) { 
        volatile uint32_t temp;
        temp = USART2->SR; // 先读SR
        temp = USART2->DR; // 再读DR，硬件自动清除 IDLE 标志位，防止死循环
        (void)temp;

        // 关闭 DMA 传输，计算接收到的数据长度
        // 实际长度 = 总分配长度 - DMA 计数器剩余长度 (CNDTR)
        uint16_t remain_bytes = (uint16_t)(DMA1_Channel6->CNDTR);
        g_uart_rx_len = UART_RX_MAX_LEN - remain_bytes;
        
        g_uart_rx_flag = 1; // 通知主循环有新数据包到达

        // 重置 DMA 计数器，准备下一次接收
        DMA1_Channel6->CCR &= ~(1 << 0); // 关闭 DMA Channel
        DMA1_Channel6->CNDTR = UART_RX_MAX_LEN;
        DMA1_Channel6->CCR |= (1 << 0);  // 重新使能 DMA Channel
    }
}