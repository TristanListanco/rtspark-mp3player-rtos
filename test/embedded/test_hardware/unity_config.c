/*
 * Unity output over USART1 (see unity_config.h).
 */
#include "unity_config.h"
#include "main.h"

void unityOutputStart(unsigned long baudrate)
{
    (void)baudrate;                         /* MX_USART1_UART_Init: 115200 */
    if (huart1.Instance == NULL) {
        MX_USART1_UART_Init();
    }
}

void unityOutputChar(unsigned int c)
{
    const uint8_t ch = (uint8_t)c;
    HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);
}

void unityOutputFlush(void)
{
}

void unityOutputComplete(void)
{
}
