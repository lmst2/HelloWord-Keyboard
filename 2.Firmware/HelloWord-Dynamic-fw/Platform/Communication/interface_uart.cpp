#include "common_inc.h"
#include "interface_uart.hpp"
#include "usart.h"

#define UART_TX_BUFFER_SIZE 64

// Legacy uart2_stream_output for printf compatibility
class uart2Sender : public StreamSink
{
public:
    int process_bytes(uint8_t *buffer, size_t length, size_t *processed_bytes) override
    {
        while (length) {
            size_t chunk = length < UART_TX_BUFFER_SIZE ? length : UART_TX_BUFFER_SIZE;
            if (osSemaphoreAcquire(sem_uart_dma, PROTOCOL_SERVER_TIMEOUT_MS) != osOK)
                return -1;
            memcpy(tx_buf_, buffer, chunk);
            if (HAL_UART_Transmit_DMA(&huart2, tx_buf_, chunk) != HAL_OK)
                return -1;
            buffer += chunk;
            length -= chunk;
            if (processed_bytes)
                *processed_bytes += chunk;
        }
        return 0;
    }
    size_t get_free_space() override { return SIZE_MAX; }
private:
    uint8_t tx_buf_[UART_TX_BUFFER_SIZE];
} uart2_stream_output;

StreamSink *uart2_stream_output_ptr = &uart2_stream_output;

// UART server task is now handled by ThreadUartComm in main.cpp
// Keep StartUartServer as no-op for backward compatibility with InitCommunication
void StartUartServer()
{
    // SLIP-based UART comm is started in ThreadUartComm (main.cpp)
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    osSemaphoreRelease(sem_uart_dma);
}
