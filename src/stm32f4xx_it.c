/*
 * stm32f4xx_it.c - Interrupt handlers.
 *
 * SVC_Handler and PendSV_Handler come from the FreeRTOS port (mapped in
 * FreeRTOSConfig.h). TIM2/TIM5 handlers live in timer_if.cpp.
 */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

static void fault_loop(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(BOARD_LED_R_GPIO_Port, BOARD_LED_R_Pin, GPIO_PIN_RESET);
    for (;;) {
    }
}

void NMI_Handler(void)        { for (;;) {} }
void HardFault_Handler(void)  { fault_loop(); }
void MemManage_Handler(void)  { fault_loop(); }
void BusFault_Handler(void)   { fault_loop(); }
void UsageFault_Handler(void) { fault_loop(); }
void DebugMon_Handler(void)   {}

/* 1 kHz SysTick shared by the HAL time base (HAL_Delay, HAL timeouts) and
 * the FreeRTOS tick. FreeRTOS keeps the same 1 kHz rate when it starts. */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* I2S3 TX DMA: half / full transfer -> refill that half of the audio buffer */
void DMA1_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi3_tx);
}
