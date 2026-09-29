/*
 * stm32f4xx_it.h - Interrupt handler prototypes (as STM32CubeMX generates).
 * Handlers: src/stm32f4xx_it.c, TIM2/TIM5 in src/timer_if.cpp.
 * SVC_Handler / PendSV_Handler are the FreeRTOS port's (FreeRTOSConfig.h).
 */
#ifndef STM32F4XX_IT_H
#define STM32F4XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
void SysTick_Handler(void);
void DMA1_Stream7_IRQHandler(void);
void TIM2_IRQHandler(void);
void TIM5_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_IT_H */
