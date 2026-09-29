/*
 * On-board smoke tests: run on the RT-Spark to check the hardware bring-up
 * the firmware depends on (LCD, codec, I2S clocking, Ticker/Timeout timing,
 * inputs, ADC), plus a short tone and LED sequence to check by ear and eye.
 *
 *   pio test -e rtspark_test        (board on USB-DBG; don't press any button)
 *
 * The FreeRTOS scheduler is not started here; the Ticker / Timeout timers
 * are interrupt-driven and don't need it.
 */
#include <stdio.h>
#include <unity.h>

#include "main.h"
#include "audio_player.h"
#include "es8388.h"
#include "lcd.h"
#include "timer_if.h"

static Ticker  ticker;          /* TIM2 */
static Timeout timeout;         /* TIM5 */

static volatile int ticks;
static volatile int timeouts;
static volatile uint32_t ticker_period_ms;

static void count_tick(void) { ticks = ticks + 1; }
static void count_timeout(void) { timeouts = timeouts + 1; }

/* Re-arms itself with a longer period each time, like the music ticker */
static void stretching_tick(void)
{
    ticks = ticks + 1;
    ticker_period_ms = ticker_period_ms + 10;
    ticker.attach(&stretching_tick, ticker_period_ms / 1000.0f);
}

void setUp(void)
{
    ticks = 0;
    timeouts = 0;
}

void tearDown(void)
{
    ticker.detach();
    timeout.detach();
}

/* ---- on-board peripherals -------------------------------------------------- */

static void test_lcd_reports_st7789v3_id(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x81B3, lcd_init());
    lcd_clear(LCD_NAVY);
    lcd_draw_string(36, 108, "HARDWARE TEST", LCD_FONT_24, LCD_WHITE, LCD_NAVY);
}

static void test_codec_answers_on_i2c2(void)
{
    TEST_ASSERT_EQUAL_INT(HAL_OK, es8388_init());
    TEST_ASSERT_EQUAL_INT(HAL_OK, es8388_set_attenuation(20));    /* -10 dB */
}

static void test_i2s_runs_at_expected_sample_rate(void)
{
    audio_init();
    /* 192 MHz / (256 * 23) = 32609 Hz */
    TEST_ASSERT_UINT32_WITHIN(5, 32609, audio_sample_rate());

    /* DMA is streaming: its remaining-count register keeps changing */
    const uint32_t a = hdma_spi3_tx.Instance->NDTR;
    HAL_Delay(2);
    const uint32_t b = hdma_spi3_tx.Instance->NDTR;
    TEST_ASSERT_NOT_EQUAL(a, b);
}

/* ---- Ticker / Timeout (TIM2 / TIM5) --------------------------------------- */

static void test_ticker_fires_at_its_rate(void)
{
    ticker.attach(&count_tick, 0.010f);                         /* 100 Hz */
    HAL_Delay(1000);
    ticker.detach();
    TEST_ASSERT_INT_WITHIN(2, 100, ticks);
}

static void test_ticker_can_reattach_from_its_callback(void)
{
    ticker_period_ms = 10;
    ticker.attach(&stretching_tick, 0.010f);
    HAL_Delay(1000);                  /* 10+20+30+...+130 = 910 ms -> 13 calls */
    ticker.detach();
    TEST_ASSERT_INT_WITHIN(1, 13, ticks);
}

static void test_detach_stops_the_ticker(void)
{
    ticker.attach(&count_tick, 0.005f);
    HAL_Delay(50);
    ticker.detach();
    const int n = ticks;
    HAL_Delay(50);
    TEST_ASSERT_EQUAL_INT(n, ticks);
    TEST_ASSERT_FALSE(ticker.attached());
}

static void test_timeout_fires_once_after_its_delay(void)
{
    timeout.attach(&count_timeout, 0.200f);
    HAL_Delay(180);
    TEST_ASSERT_EQUAL_INT(0, timeouts);                          /* not yet */
    HAL_Delay(40);
    TEST_ASSERT_EQUAL_INT(1, timeouts);                          /* at ~200 ms */
    HAL_Delay(400);
    TEST_ASSERT_EQUAL_INT(1, timeouts);                          /* only once */
    TEST_ASSERT_FALSE(timeout.attached());
}

static void test_timeout_can_be_cancelled(void)
{
    timeout.attach(&count_timeout, 0.100f);
    HAL_Delay(50);
    timeout.detach();
    HAL_Delay(100);
    TEST_ASSERT_EQUAL_INT(0, timeouts);
}

/* ---- inputs ----------------------------------------------------------------- */

static void test_buttons_read_released(void)
{
    /* pull-ups hold every button input high while nothing is pressed */
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, HAL_GPIO_ReadPin(BTN3_GPIO_Port, BTN3_Pin));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, HAL_GPIO_ReadPin(BTN4_GPIO_Port, BTN4_Pin));
    TEST_ASSERT_EQUAL_INT(GPIO_PIN_SET, HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin));
}

static void test_potentiometer_reading(void)
{
    HAL_ADC_Start(&hadc3);
    TEST_ASSERT_EQUAL_INT(HAL_OK, HAL_ADC_PollForConversion(&hadc3, 10));
    const uint32_t raw = HAL_ADC_GetValue(&hadc3);
    HAL_ADC_Stop(&hadc3);
    TEST_ASSERT_TRUE(raw <= 4095);
    char msg[48];
    snprintf(msg, sizeof msg, "pot raw = %lu (0..4095)", (unsigned long)raw);
    TEST_MESSAGE(msg);
}

/* ---- by ear / by eye ----------------------------------------------------- */

static void test_play_a4_tone_and_cycle_leds(void)
{
    TEST_MESSAGE("You should hear a 440 Hz tone and see red, green, blue");
    audio_play_note(440.0f, 0.9f);
    const struct { GPIO_TypeDef *port; uint16_t pin; } leds[] = {
        { LED_RED_GPIO_Port, LED_RED_Pin },
        { LED_GREEN_GPIO_Port, LED_GREEN_Pin },
        { LED_BLUE_GPIO_Port, LED_BLUE_Pin },
    };
    const GPIO_PinState on = RGB_LED_COMMON_ANODE ? GPIO_PIN_RESET : GPIO_PIN_SET;
    const GPIO_PinState off = RGB_LED_COMMON_ANODE ? GPIO_PIN_SET : GPIO_PIN_RESET;
    for (unsigned i = 0; i < sizeof leds / sizeof leds[0]; i++) {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, on);
        HAL_Delay(300);
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, off);
    }
    audio_stop();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_FSMC_Init();
    MX_I2C2_Init();
    MX_I2S3_Init();
    MX_ADC3_Init();

    HAL_Delay(2000);            /* give `pio test` time to open the serial port */

    UNITY_BEGIN();
    RUN_TEST(test_lcd_reports_st7789v3_id);
    RUN_TEST(test_codec_answers_on_i2c2);
    RUN_TEST(test_i2s_runs_at_expected_sample_rate);
    RUN_TEST(test_ticker_fires_at_its_rate);
    RUN_TEST(test_ticker_can_reattach_from_its_callback);
    RUN_TEST(test_detach_stops_the_ticker);
    RUN_TEST(test_timeout_fires_once_after_its_delay);
    RUN_TEST(test_timeout_can_be_cancelled);
    RUN_TEST(test_buttons_read_released);
    RUN_TEST(test_potentiometer_reading);
    RUN_TEST(test_play_a4_tone_and_cycle_leds);
    UNITY_END();

    for (;;) {
    }
}
