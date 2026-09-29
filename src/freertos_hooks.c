/*
 * freertos_hooks.c - FreeRTOS application hooks.
 */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * Idle hook = "use sleep mode to reduce power consumption".
 * The idle task runs only when all three application threads are waiting.
 * Sleep mode stops the CPU clock (peripherals, DMA and timers keep running)
 * until the next interrupt: the 1 ms tick, the audio DMA, or a
 * Ticker/Timeout event. Execution resumes right after the WFI.
 */
void vApplicationIdleHook(void)
{
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}
