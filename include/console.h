/*
 * console.h - Text output on USART1 (ST-LINK virtual COM port, 115200 8N1).
 *
 * Output is blocking. Once the scheduler runs, a mutex keeps lines from
 * different threads from interleaving. Don't call from interrupts.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the console mutex. Call once before starting the scheduler. */
void console_init(void);

void console_write(const char *s);
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */
