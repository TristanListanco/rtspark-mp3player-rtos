/*
 * main.h - RT-Spark MP3 player: pin map, peripheral handles and init functions.
 *
 * Pin labels follow the STM32CubeMX convention (<LABEL>_Pin / <LABEL>_GPIO_Port).
 * On-board connections come from the RT-Spark schematic and the RT-Thread BSP
 * (bsp/stm32/stm32f407-rt-spark). External parts plug into the 40-pin header
 * and the PMOD2 header; see README.md for the wiring diagram.
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ===================== INPUTS ============================================ */

/* External push buttons B1..B4 (Figure 1.3: 330R pull-up to 3V3, switch to
 * GND, so a pressed button reads LOW). 40-pin header, left column, rows 14-17. */
#define BTN1_Pin              GPIO_PIN_10   /* PD10 - header D12: select / confirm */
#define BTN1_GPIO_Port        GPIOD
#define BTN2_Pin              GPIO_PIN_8    /* PD8  - header D13: song bit 2 (MSB) */
#define BTN2_GPIO_Port        GPIOD
#define BTN3_Pin              GPIO_PIN_14   /* PE14 - header D14: song bit 1       */
#define BTN3_GPIO_Port        GPIOE
#define BTN4_Pin              GPIO_PIN_12   /* PE12 - header D15: song bit 0 (LSB) */
#define BTN4_GPIO_Port        GPIOE

/* USER_BUTTON = on-board KEY_LEFT (KEY0), active LOW. Play / pause. */
#define USER_BTN_Pin          GPIO_PIN_0    /* PC0 */
#define USER_BTN_GPIO_Port    GPIOC

/* Volume potentiometer wiper (Figure 1.2): PMOD2 header "A0" = PF6 = ADC3_IN4 */
#define POT_Pin               GPIO_PIN_6    /* PF6 */
#define POT_GPIO_Port         GPIOF
#define POT_ADC_CHANNEL       ADC_CHANNEL_4

/* ===================== OUTPUTS =========================================== */

/* External RGB LED (Figure 1.4). 40-pin header, right column, rows 11-13.
 * Set RGB_LED_COMMON_ANODE to 1 if the common pin is wired to 3V3
 * (then a LOW output turns the LED on). */
#define RGB_LED_COMMON_ANODE  0
#define LED_RED_Pin           GPIO_PIN_2    /* PG2 - header D18 */
#define LED_RED_GPIO_Port     GPIOG
#define LED_GREEN_Pin         GPIO_PIN_4    /* PG4 - header D30 */
#define LED_GREEN_GPIO_Port   GPIOG
#define LED_BLUE_Pin          GPIO_PIN_6    /* PG6 - header D29 */
#define LED_BLUE_GPIO_Port    GPIOG

/* On-board user LEDs (active LOW). They mirror the red/blue player state so
 * the board can be tested before the breadboard is wired. */
#define BOARD_LED_R_Pin       GPIO_PIN_12   /* PF12 */
#define BOARD_LED_R_GPIO_Port GPIOF
#define BOARD_LED_B_Pin       GPIO_PIN_11   /* PF11 */
#define BOARD_LED_B_GPIO_Port GPIOF

/* ===================== ON-BOARD PERIPHERALS ============================== */

/* 1.3" 240x240 ST7789v3 LCD, 8080 8-bit parallel through FSMC bank 1 / NE3:
 *   D0..D7 = PD14 PD15 PD0 PD1 PE7 PE8 PE9 PE10, NOE = PD4, NWE = PD5,
 *   NE3 = PG10 (chip select), A18 = PD13 (RS / D/CX). */
#define LCD_RST_Pin           GPIO_PIN_3    /* PD3 */
#define LCD_RST_GPIO_Port     GPIOD
#define LCD_BL_Pin            GPIO_PIN_9    /* PF9 - backlight enable */
#define LCD_BL_GPIO_Port      GPIOF

/* ES8388 audio codec (I2C address 0x10, CE = GND):
 *   I2S3: MCK = PC7, WS = PA15, CK = PB3, SD = PB5   (DMA1 Stream 7, channel 0)
 *   I2C2: SCL = PF1, SDA = PF0
 * LOUT1/ROUT1 drive the 3.5 mm headphone jack (external speaker). */

/* Console: USART1 TX = PA9, RX = PA10 (ST-LINK virtual COM port), 115200 8N1 */

/* ===================== INTERRUPT PRIORITIES ==============================
 * Numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5), so these
 * ISRs are masked by taskENTER_CRITICAL(). Audio DMA and the Ticker/Timeout
 * timers share one priority so they never preempt each other. */
#define AUDIO_IRQ_PRIORITY    6
#define TIMER_IRQ_PRIORITY    6

/* ===================== HANDLES / INIT ==================================== */
extern ADC_HandleTypeDef   hadc3;
extern I2C_HandleTypeDef   hi2c2;
extern I2S_HandleTypeDef   hi2s3;
extern DMA_HandleTypeDef   hdma_spi3_tx;
extern UART_HandleTypeDef  huart1;
extern SRAM_HandleTypeDef  hsram3;

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_USART1_UART_Init(void);
void MX_FSMC_Init(void);
void MX_I2C2_Init(void);
void MX_I2S3_Init(void);
void MX_ADC3_Init(void);
void Error_Handler(void) __attribute__((noreturn));   /* blinks the red LED forever */

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
