#ifndef UART_H
#define UART_H

#include <pthread.h>
#include <termios.h>
#include <sys/select.h>
#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif  /* End of #ifdef __cplusplus */

typedef enum hiUARTHAL_BIT_RATE
{
    HAL_UART_BITRATE_300,
    HAL_UART_BITRATE_1200,
    HAL_UART_BITRATE_2400,
    HAL_UART_BITRATE_4800,
    HAL_UART_BITRATE_9600,
    HAL_UART_BITRATE_19200,
    HAL_UART_BITRATE_38400,
    HAL_UART_BITRATE_115200,
    HAL_UART_BITRATE_921600,
    HAL_UART_BITRATE_1000000,
    HAL_UART_BITRATE_1500000,
    HAL_UART_BITRATE_2000000,
    HAL_UART_BITRATE_3000000,
    HAL_UART_BITRATE_4000000,
    HAL_UART_BITRATE_BUTT,
} UARTHAL_BIT_RATE;

typedef enum hiUARTHAL_DATA_BIT
{
    HAL_UART_DATABIT_7,
    HAL_UART_DATABIT_8,
    HAL_UART_DATABIT_BUTT,
} UARTHAL_DATA_BIT;

typedef enum hiUARTHAL_STOP_BIT
{
    HAL_UART_STOPBIT_1,
    HAL_UART_STOPBIT_2,
    HAL_UART_STOPBIT_BUTT,
} UARTHAL_STOP_BIT;

typedef enum hiUARTHAL_PARITY
{
    HAL_UART_PARITY_N,
    HAL_UART_PARITY_O,
    HAL_UART_PARITY_E,
    HAL_UART_PARITY_S,
    HAL_UART_PARITY_BUTT,
} UARTHAL_PARITY;

typedef struct hiUARTHAL_ATTR
{
    UARTHAL_BIT_RATE bitRate;
    UARTHAL_DATA_BIT dataBits;
    UARTHAL_STOP_BIT stopBits;
    UARTHAL_PARITY parity; //Even (E), odd (O), no (N)
}UARTHAL_ATTR;



uint32_t init_uart(int *fd,char *device);
uint32_t deinit_uart(uint32_t uartFd);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif // UART_H
