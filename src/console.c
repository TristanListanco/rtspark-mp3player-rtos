/*
 * console.c - Blocking UART console on USART1.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "console.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define CONSOLE_LINE_MAX  160

static SemaphoreHandle_t console_mutex;
static char line_buf[CONSOLE_LINE_MAX];

static int scheduler_running(void)
{
    return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

static void lock(void)
{
    if (console_mutex != NULL && scheduler_running()) {
        xSemaphoreTake(console_mutex, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (console_mutex != NULL && scheduler_running()) {
        xSemaphoreGive(console_mutex);
    }
}

static void uart_send(const char *s, size_t len)
{
    HAL_UART_Transmit(&huart1, (const uint8_t *)s, (uint16_t)len, HAL_MAX_DELAY);
}

void console_init(void)
{
    console_mutex = xSemaphoreCreateMutex();
    configASSERT(console_mutex != NULL);
}

void console_write(const char *s)
{
    lock();
    uart_send(s, strlen(s));
    unlock();
}

void console_printf(const char *fmt, ...)
{
    lock();
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line_buf, sizeof line_buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n >= (int)sizeof line_buf) {
            n = sizeof line_buf - 1;            /* truncated */
        }
        uart_send(line_buf, (size_t)n);
    }
    unlock();
}
