/*
 * FreeRTOS configuration for the RT-Spark MP3 player (STM32F407ZGT6 @ 168 MHz).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t SystemCoreClock;
#ifdef __cplusplus
}
#endif

/* ---- Scheduler ------------------------------------------------------------
 * The lab asks for cooperative scheduling: a thread keeps the CPU until it
 * blocks (vTaskDelay) after each update, then the next ready thread runs.
 * The application uses a mutex and critical sections, so it also works if
 * this is changed to 1 (preemptive). */
#define configUSE_PREEMPTION                    0
#define configUSE_TIME_SLICING                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      ( 1000 )
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( 128 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* ---- Kernel objects ------------------------------------------------------ */
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TIMERS                        0   /* Ticker/Timeout use hardware timers */
#define configUSE_CO_ROUTINES                   0

/* ---- Memory -------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( 16 * 1024 )

/* ---- Hooks --------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                     1   /* enters MCU Sleep mode */
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* ---- Cortex-M4 interrupt priorities (4 priority bits on STM32F4) ----------
 * ISRs that call FreeRTOS "FromISR" APIs, or that must be masked by
 * taskENTER_CRITICAL(), need a priority of 5..15 (numerically). */
#define configPRIO_BITS                              4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

#define configASSERT( x ) \
    if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ) {} }

/* ---- Optional API -------------------------------------------------------- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    0
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* ---- Exception handlers -------------------------------------------------
 * SVC and PendSV map straight to the FreeRTOS port. SysTick_Handler is
 * written in src/stm32f4xx_it.c because it also drives the HAL time base. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
