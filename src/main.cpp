/*
 * main.cpp - BCA182 Laboratory Activity 2: Personal MP3 Player
 *
 * Board  : RT-Thread RT-Spark (STM32F407ZGT6, 168 MHz)
 * Stack  : STM32Cube HAL + FreeRTOS (cooperative scheduling)
 *
 * Features
 *  - 8 selectable songs: B2..B4 give the song number in binary, B1 selects,
 *    B1 again (within 5 s) confirms. If nothing happens for 5 s, playback
 *    continues as before.
 *  - USER_BUTTON stops / plays the current song.
 *  - LCD shows the song name while playing and a confirmation message while
 *    a new song is being chosen.
 *  - RGB LED: blue = playing, red = paused, green = changing song.
 *  - Potentiometer sets the volume; usage instructions are sent over UART.
 *
 * Threads (each an infinite loop that waits after every update):
 *  1. update_lcd_leds_thread()  LCD + RGB LEDs
 *  2. polling_buttons()         B1..B4 and USER_BUTTON
 *  3. adjust_volume()           potentiometer -> ES8388 volume
 * Interrupt-driven timers (timer_if.h):
 *  - Ticker  music_ticker       plays the song note by note
 *  - Timeout confirm_timeout    5 s confirmation window
 *
 * Course files used: song.h / song_def.h (unchanged) and NHD_0216HZ
 * (ported to the on-board LCD, see NHD_0216HZ.h). The decisions that don't
 * touch hardware live in player_logic.cpp, which has unit tests.
 */
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "audio_player.h"
#include "console.h"
#include "es8388.h"
#include "lcd.h"
#include "NHD_0216HZ.h"
#include "player_logic.h"
#include "timer_if.h"

/* Course files. song_def.h #defines short names (Do, Re, Mi, No, b0..b4, ...)
 * and pulls in `using namespace std`, so keep these includes last and don't
 * use those names below. Its note values are double literals stored in float
 * arrays (e.g. 3.822): harmless, so that one warning is off for these files. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#include "song.h"
#include "song_def.h"
#pragma GCC diagnostic pop

/* ===========================================================================
 * Songs
 * ========================================================================= */
static Song *const songs[] = {
    &FUR_ELISE,                 /* B2 B3 B4 = 000 */
    &CANNON_IN_D,               /*            001 */
    &MINUET_IN_G_MAJOR,         /*            010 */
    &TURKISH_MARCH,             /*            011 */
    &NOCTRUNE_IN_E_FLAT,        /*            100 */
    &WALTZ_NO2,                 /*            101 */
    &NOCTRUNE_IN_C_SHARP_MAJOR, /*            110 */
    &SYMPHONY_NO40,             /*            111 */
    &SYMPHONY_NO5,              /* 9 and 10 can't be selected with 3 buttons */
    &EINE_KLEINE_NACHTAMUSIK,
};
static const int kNumSongs = sizeof songs / sizeof songs[0];

/* ===========================================================================
 * Configuration
 * ========================================================================= */
static const int        kSelectableSongs = 8;        /* 3 binary buttons -> 2^3 songs */
static const float      kConfirmWindowS  = 5.0f;     /* time to confirm a new song */
static const float      kSongRepeatGapS  = 1.5f;     /* silence before a song repeats */
static const TickType_t kLcdPeriod       = pdMS_TO_TICKS(50);
static const TickType_t kButtonPeriod    = pdMS_TO_TICKS(10);
static const TickType_t kVolumePeriod    = pdMS_TO_TICKS(100);
static const uint8_t    kDebounceSamples = 3;        /* x kButtonPeriod = 30 ms */
static const uint32_t   kPotRawMin       = 80;       /* ADC counts at the ends of the */
static const uint32_t   kPotRawMax       = 4000;     /* pot travel (100R keeps min > 0) */

static_assert(kNumSongs >= kSelectableSongs, "need at least 8 songs");

/* ===========================================================================
 * Inputs and outputs (pin numbers and wiring in main.h)
 * ========================================================================= */
struct Pin {
    GPIO_TypeDef *port;
    uint16_t pin;
};

/* Inputs: buttons pull the pin LOW when pressed */
static const Pin button1     = { BTN1_GPIO_Port, BTN1_Pin };         /* select / confirm */
static const Pin button2     = { BTN2_GPIO_Port, BTN2_Pin };         /* song bit 2 (MSB) */
static const Pin button3     = { BTN3_GPIO_Port, BTN3_Pin };         /* song bit 1 */
static const Pin button4     = { BTN4_GPIO_Port, BTN4_Pin };         /* song bit 0 (LSB) */
static const Pin user_button = { USER_BTN_GPIO_Port, USER_BTN_Pin }; /* play / stop */
/* Potentiometer: ADC3 channel 4 (hadc3, PF6) */

/* Outputs */
static const Pin led_red     = { LED_RED_GPIO_Port, LED_RED_Pin };
static const Pin led_green   = { LED_GREEN_GPIO_Port, LED_GREEN_Pin };
static const Pin led_blue    = { LED_BLUE_GPIO_Port, LED_BLUE_Pin };
static const Pin board_led_r = { BOARD_LED_R_GPIO_Port, BOARD_LED_R_Pin };
static const Pin board_led_b = { BOARD_LED_B_GPIO_Port, BOARD_LED_B_Pin };
/* Speaker: ES8388 headphone output (audio_player.c). LCD: see below. */

static inline bool is_pressed(const Pin &p)
{
    return HAL_GPIO_ReadPin(p.port, p.pin) == GPIO_PIN_RESET;
}

static inline void rgb_led(const Pin &p, bool on)
{
    const bool high = RGB_LED_COMMON_ANODE ? !on : on;
    HAL_GPIO_WritePin(p.port, p.pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void board_led(const Pin &p, bool on)
{
    HAL_GPIO_WritePin(p.port, p.pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);   /* active low */
}

/* ===========================================================================
 * LCD layout (240 x 240). The song name uses the course's NHD_0216HZ 16x2
 * character API, drawn in a window of the on-board ST7789.
 * ========================================================================= */
static const uint16_t kHeaderH    = 30;
static const uint16_t kInfoY      = 40;    /* "Song 3 / 8"            font 16 */
static const uint16_t kNameX      = (LCD_WIDTH - NHD_0216HZ::COLUMNS * (LCD_FONT_24 / 2)) / 2;
static const uint16_t kNameY      = 64;    /* 16x2 character window   font 24 */
static const uint16_t kStatusY    = 128;   /* PLAYING / PAUSED        font 24 */
static const uint16_t kCountdownY = 154;   /* confirmation countdown  font 16 */
static const uint16_t kProgressY  = 172;
static const uint16_t kVolumeY    = 196;   /* volume label (thread 3) font 16 */
static const uint16_t kVolumeBarY = 218;
static const uint16_t kBarX       = 20;
static const uint16_t kBarW       = 200;
static const uint16_t kBarH       = 10;

static NHD_0216HZ lcd(kNameX, kNameY);

/* The LCD is written by two threads (song/status and volume): one at a time */
static SemaphoreHandle_t lcd_mutex;

/* Holds lcd_mutex for the lifetime of the object (RAII), so every take has
 * its give. */
class LcdLock {
public:
    LcdLock() { xSemaphoreTake(lcd_mutex, portMAX_DELAY); }
    // cppcheck-suppress cstyleCast ; the cast is inside FreeRTOS's xSemaphoreGive macro
    ~LcdLock() { xSemaphoreGive(lcd_mutex); }
private:
    LcdLock(const LcdLock &);
    LcdLock &operator=(const LcdLock &);
};

/* ===========================================================================
 * Player state (shared by the threads and the Ticker/Timeout interrupts)
 * ========================================================================= */
enum PlayState { STOPPED, PLAYING };

static volatile PlayState  play_state      = STOPPED;
static volatile bool       confirm_expired = false;  /* set by the Timeout ISR */
static volatile int        current_song    = 0;      /* song being played */
static volatile int        note_index      = 0;      /* next note of current_song */
static volatile TickType_t confirm_deadline;
static SongSelector        selector;                 /* B1 select / confirm */

static Ticker  music_ticker;
static Timeout confirm_timeout;

static bool codec_ok;    /* ES8388 answered on I2C2 at boot */

/* Ticker callback (interrupt): start the next note (0 Hz = rest, silent),
 * then re-arm the ticker for that note's duration. After the last note it
 * waits kSongRepeatGapS and the song starts over. */
static void play_next_note(void)
{
    const NoteStep step = next_note_step(*songs[current_song], note_index, kSongRepeatGapS);
    audio_play_note(step.freq_hz, step.duration_s);
    note_index = step.next_index;
    music_ticker.attach(&play_next_note, step.next_s);
}

/* Timeout callback (interrupt): no confirmation within 5 s, carry on. */
static void confirm_window_expired(void)
{
    selector.timeout();
    confirm_expired = true;
}

/* Play (or resume) current_song from note_index */
static void start_playback(void)
{
    play_state = PLAYING;
    music_ticker.attach(&play_next_note, 0.001f);
}

static void stop_playback(void)
{
    music_ticker.detach();
    audio_stop();
    play_state = STOPPED;
}

static void play_song(int song)
{
    music_ticker.detach();
    audio_stop();
    current_song = song;
    note_index = 0;
    start_playback();
}

/* "Turkish March - Mozart" for the console (polling thread and main only) */
static const char *full_name(int song)
{
    static char buf[40];
    song_title(*songs[song], buf, sizeof buf);
    return buf;
}

/* ===========================================================================
 * Thread 1: update the LCD and the RGB LEDs
 * ========================================================================= */

/* Caller holds lcd_mutex for all draw_* functions */
static void draw_centered(uint16_t y, const char *text, lcd_font_t font,
                          uint16_t fg, uint16_t bg)
{
    lcd_fill_rect(0, y, LCD_WIDTH, font, bg);
    const uint16_t w = lcd_text_width(text, font);
    lcd_draw_string(w < LCD_WIDTH ? (LCD_WIDTH - w) / 2 : 0, y, text, font, fg, bg);
}

static void draw_header(const char *title, uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, kHeaderH, color);
    const uint16_t w = lcd_text_width(title, LCD_FONT_16);
    lcd_draw_string((LCD_WIDTH - w) / 2, (kHeaderH - LCD_FONT_16) / 2, title,
                    LCD_FONT_16, LCD_WHITE, color);
}

/* Song number + the two name lines, as the mbed version showed them on the
 * 16x2 NHD LCD: name1 on line 1, name2 on line 2. */
static void draw_song_block(int song, uint16_t name_color)
{
    char info[24];
    (void)snprintf(info, sizeof info, "Song %d / %d", song + 1, kSelectableSongs);
    draw_centered(kInfoY, info, LCD_FONT_16, LCD_GRAY, LCD_BLACK);

    lcd.set_colors(name_color, LCD_BLACK);
    lcd.clr_lcd();
    lcd.set_cursor(0, 0);
    lcd.printf("%s", songs[song]->name1.c_str());
    lcd.set_cursor(0, 1);
    lcd.printf("%s", songs[song]->name2.c_str());
}

static void draw_progress(uint16_t px, uint16_t color)
{
    lcd_fill_rect(kBarX, kProgressY, px, kBarH, color);
    lcd_fill_rect(kBarX + px, kProgressY, kBarW - px, kBarH, LCD_DARKGRAY);
}

/* What is on the screen now, so only changed parts are redrawn */
struct ScreenState {
    int mode;        /* -1 nothing yet, 0 now playing, 1 confirmation */
    int song;
    int playing;
    int countdown;
    int progress;
};

/* Snapshot of the player state taken for one screen update */
struct PlayerView {
    bool selecting;
    bool playing;
    int song;        /* pending song while selecting, else current song */
    int index;       /* note index, for the progress bar */
    TickType_t deadline;
};

/* Caller holds the LCD lock */
static void draw_screen(ScreenState &shown, const PlayerView &v)
{
    if (v.selecting) {
        /* Confirmation message for the chosen song */
        if (shown.mode != 1 || shown.song != v.song) {
            draw_header("Change song?", LCD_DARKGREEN);
            lcd_fill_rect(0, kHeaderH, LCD_WIDTH, kVolumeY - kHeaderH, LCD_BLACK);
            draw_song_block(v.song, LCD_YELLOW);
            draw_centered(kStatusY, "Press B1 to confirm", LCD_FONT_16, LCD_GREEN, LCD_BLACK);
            shown.mode = 1;
            shown.song = v.song;
            shown.countdown = -1;
        }
        const int32_t ms_left = (int32_t)(v.deadline - xTaskGetTickCount());
        const int seconds_left = ms_left > 0 ? (int)((ms_left + 999) / 1000) : 0;
        if (seconds_left != shown.countdown) {
            char text[24];
            (void)snprintf(text, sizeof text, "Cancel in %d s", seconds_left);
            draw_centered(kCountdownY, text, LCD_FONT_16, LCD_WHITE, LCD_BLACK);
            shown.countdown = seconds_left;
        }
    } else {
        /* Name of the song being played */
        if (shown.mode != 0 || shown.song != v.song) {
            lcd_fill_rect(0, kHeaderH, LCD_WIDTH, kVolumeY - kHeaderH, LCD_BLACK);
            draw_song_block(v.song, LCD_WHITE);
            shown.mode = 0;
            shown.song = v.song;
            shown.playing = -1;
            shown.progress = -1;
        }
        if (shown.playing != (int)v.playing) {
            draw_header("RT-Spark MP3 Player", v.playing ? LCD_BLUE : LCD_MAROON);
            draw_centered(kStatusY, v.playing ? "PLAYING" : "PAUSED", LCD_FONT_24,
                          v.playing ? LCD_CYAN : LCD_RED, LCD_BLACK);
            shown.playing = v.playing;
            shown.progress = -1;
        }
        const int length = songs[v.song]->length;
        const int px = length > 0 ? (v.index > length ? length : v.index) * kBarW / length : 0;
        if (px != shown.progress) {
            draw_progress((uint16_t)px, v.playing ? LCD_CYAN : LCD_GRAY);
            shown.progress = px;
        }
    }
}

static void update_lcd_leds_thread(void *arg)
{
    ScreenState shown = { -1, -1, -1, -1, -1 };

    for (;;) {
        /* Consistent snapshot of the state the interrupts also change */
        PlayerView v;
        taskENTER_CRITICAL();
        v.selecting = selector.confirming();
        v.playing = (play_state == PLAYING);
        v.song = v.selecting ? selector.pending() : current_song;
        v.index = note_index;
        v.deadline = confirm_deadline;
        taskEXIT_CRITICAL();

        /* RGB LEDs: green = changing song, blue = playing, red = paused */
        rgb_led(led_green, v.selecting);
        rgb_led(led_blue, !v.selecting && v.playing);
        rgb_led(led_red, !v.selecting && !v.playing);
        board_led(board_led_b, !v.selecting && v.playing);
        board_led(board_led_r, !v.selecting && !v.playing);

        {
            const LcdLock lock;          /* exclusive LCD access */
            draw_screen(shown, v);
        }

        vTaskDelay(kLcdPeriod);          /* wait: let the next thread run */
    }
}

/* ===========================================================================
 * Thread 2: poll the buttons
 * ========================================================================= */
static void polling_buttons(void *arg)
{
    /* (not b1..b4: song_def.h #defines those as beat lengths) */
    Debouncer btn1 = { false, 0 };
    Debouncer btn2 = { false, 0 };
    Debouncer btn3 = { false, 0 };
    Debouncer btn4 = { false, 0 };
    Debouncer user = { false, 0 };

    for (;;) {
        const bool b1_pressed = debounce(btn1, is_pressed(button1), kDebounceSamples);
        debounce(btn2, is_pressed(button2), kDebounceSamples);
        debounce(btn3, is_pressed(button3), kDebounceSamples);
        debounce(btn4, is_pressed(button4), kDebounceSamples);
        const bool user_pressed = debounce(user, is_pressed(user_button), kDebounceSamples);

        /* USER_BUTTON: stop / play */
        if (user_pressed) {
            if (play_state == PLAYING) {
                stop_playback();
                console_printf("[USER] Paused  : %s\r\n", full_name(current_song));
            } else {
                start_playback();
                console_printf("[USER] Playing : %s\r\n", full_name(current_song));
            }
        }

        /* B1: select (B2..B4 held = song number) / confirm */
        if (b1_pressed) {
            const int selected = song_index_from_buttons(btn2.pressed, btn3.pressed, btn4.pressed);

            taskENTER_CRITICAL();                /* vs. the Timeout interrupt */
            const SongSelector::Result r = selector.press(selected);
            if (r == SongSelector::CONFIRMED) {
                confirm_timeout.detach();
            }
            taskEXIT_CRITICAL();

            if (r == SongSelector::SELECTED) {
                confirm_deadline = xTaskGetTickCount() +
                                   pdMS_TO_TICKS((uint32_t)(kConfirmWindowS * 1000.0f));
                confirm_expired = false;
                confirm_timeout.attach(&confirm_window_expired, kConfirmWindowS);
                console_printf("[B1] Selected song %d (%d%d%d): %s - press B1 again within 5 s\r\n",
                               selected + 1, btn2.pressed, btn3.pressed, btn4.pressed, full_name(selected));
            } else {
                const int chosen = selector.pending();
                play_song(chosen);
                console_printf("[B1] Confirmed, playing song %d: %s\r\n",
                               chosen + 1, full_name(chosen));
            }
        }

        if (confirm_expired) {
            confirm_expired = false;
            console_printf("[B1] Not confirmed within 5 s, keeping the current song\r\n");
        }

        vTaskDelay(kButtonPeriod);       /* wait: let the next thread run */
    }
}

/* ===========================================================================
 * Thread 3: adjust the volume from the potentiometer
 * ========================================================================= */
static uint32_t read_potentiometer(void)
{
    uint32_t sum = 0;
    uint32_t n = 0;
    for (int i = 0; i < 8; i++) {
        HAL_ADC_Start(&hadc3);
        if (HAL_ADC_PollForConversion(&hadc3, 2) == HAL_OK) {
            sum += HAL_ADC_GetValue(&hadc3);
            n++;
        }
    }
    HAL_ADC_Stop(&hadc3);
    return n > 0 ? sum / n : 0;
}

static void draw_volume(int percent)
{
    char text[24];
    (void)snprintf(text, sizeof text, "Volume %d%%", percent);
    draw_centered(kVolumeY, text, LCD_FONT_16, LCD_WHITE, LCD_BLACK);

    const uint16_t w = (uint16_t)(percent * kBarW / 100);
    lcd_fill_rect(kBarX, kVolumeBarY, w, kBarH, LCD_ORANGE);
    lcd_fill_rect(kBarX + w, kVolumeBarY, kBarW - w, kBarH, LCD_DARKGRAY);
}

static void apply_volume(int percent)
{
    if (!codec_ok) {                     /* avoid I2C timeouts if the codec is missing */
        return;
    }
    if (percent == 0) {
        es8388_mute(1);
    } else {
        es8388_set_attenuation(volume_to_attenuation(percent));
        es8388_mute(0);
    }
}

static void adjust_volume(void *arg)
{
    int volume = -1;

    for (;;) {
        const int percent = pot_to_percent(read_potentiometer(), kPotRawMin, kPotRawMax);

        if (volume_should_update(volume, percent)) {
            volume = percent;
            apply_volume(percent);

            {
                const LcdLock lock;      /* exclusive LCD access */
                draw_volume(percent);
            }

            console_printf("Volume: %d%%\r\n", percent);
        }

        vTaskDelay(kVolumePeriod);       /* wait: let the next thread run */
    }
}

/* ===========================================================================
 * Main program
 * ========================================================================= */
static void print_instructions(uint16_t lcd_id)
{
    console_write(
        "\r\n"
        "=====================================================\r\n"
        "   RT-Spark Personal MP3 Player (FreeRTOS, 3 threads)\r\n"
        "=====================================================\r\n"
        "How to use:\r\n"
        "  USER button (KEY_LEFT) : stop / play the current song\r\n"
        "  Change the song:\r\n"
        "    1. Hold B2 B3 B4 to set the song number in binary\r\n"
        "       (B2 = MSB, pressed = 1; nothing pressed = song 1)\r\n"
        "    2. While holding them, press B1, then release all buttons\r\n"
        "    3. Press B1 again within 5 s to confirm\r\n"
        "       (no confirmation in 5 s = keep the current song)\r\n"
        "  Potentiometer          : volume\r\n"
        "LEDs: BLUE = playing, RED = paused, GREEN = changing song\r\n"
        "\r\n"
        "B2 B3 B4   Song\r\n");
    for (int i = 0; i < kSelectableSongs; i++) {
        console_printf("  %d  %d  %d   %d. %s\r\n",
                       (i >> 2) & 1, (i >> 1) & 1, i & 1, i + 1, full_name(i));
    }
    console_printf("\r\nLCD id 0x%04X | ES8388 %s | I2S %lu Hz\r\n",
                   lcd_id, codec_ok ? "OK" : "NOT RESPONDING (check I2C2 PF0/PF1)",
                   (unsigned long)audio_sample_rate());
    console_write("Playback is stopped: press USER to play song 1.\r\n\r\n");
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

    /* Clear the LCD display: the ST7789 panel, then the 16x2 character window */
    const uint16_t lcd_id = lcd_init();
    lcd_clear(LCD_BLACK);
    lcd.init_lcd();

    /* Audio: I2S clocks + DMA first (the codec runs from MCLK), then the codec */
    audio_init();
    codec_ok = (es8388_init() == HAL_OK);

    /* Note: once a FreeRTOS object exists, FreeRTOS keeps interrupts at
     * priority >= 5 masked until the scheduler starts, so HAL_Delay() must
     * not be used past this point. The console below busy-waits instead. */
    console_init();
    lcd_mutex = xSemaphoreCreateMutex();
    configASSERT(lcd_mutex != NULL);

    /* Start all threads (same priority, cooperative: each one waits after
     * its update so the next one can be scheduled) */
    if (xTaskCreate(update_lcd_leds_thread, "lcd_leds", 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS ||
        xTaskCreate(polling_buttons,        "buttons",  384, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS ||
        xTaskCreate(adjust_volume,          "volume",   384, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        Error_Handler();
    }

    /* Send the instructions for using the audio player via UART */
    print_instructions(lcd_id);

    /* Hand the CPU to the threads. When all of them are waiting, the idle
     * hook (freertos_hooks.c) puts the MCU into sleep mode. */
    vTaskStartScheduler();

    for (;;) {
        /* not reached */
    }
}
