/*
 * Unity output for the on-board tests: results go to USART1 (ST-LINK
 * virtual COM port, 115200 baud), which `pio test` reads.
 * PlatformIO has no built-in Unity configuration for STM32Cube.
 */
#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

void unityOutputStart(unsigned long baudrate);
void unityOutputChar(unsigned int c);
void unityOutputFlush(void);
void unityOutputComplete(void);

#ifdef __cplusplus
}
#endif

#define UNITY_OUTPUT_START()    unityOutputStart(115200)
#define UNITY_OUTPUT_CHAR(c)    unityOutputChar(c)
#define UNITY_OUTPUT_FLUSH()    unityOutputFlush()
#define UNITY_OUTPUT_COMPLETE() unityOutputComplete()

#endif /* UNITY_CONFIG_H */
