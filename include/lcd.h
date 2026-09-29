/*
 * lcd.h - RT-Spark on-board 1.3" 240x240 LCD (ST7789v3, 8080 8-bit via FSMC).
 *
 * Graphics driver: rectangles and text at pixel positions in RGB565 colours.
 * The 16x2 character API of the course's NHD_0216HZ driver is ported on top
 * of it in NHD_0216HZ.h.
 *
 * The driver is not thread-safe: the application serialises access with a
 * FreeRTOS mutex (see lcd_mutex in main.cpp).
 */
#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH   240
#define LCD_HEIGHT  240

/* RGB565 colours */
#define LCD_BLACK      0x0000
#define LCD_WHITE      0xFFFF
#define LCD_RED        0xF800
#define LCD_GREEN      0x07E0
#define LCD_BLUE       0x001F
#define LCD_YELLOW     0xFFE0
#define LCD_CYAN       0x07FF
#define LCD_GRAY       0x8410
#define LCD_DARKGRAY   0x31A6
#define LCD_NAVY       0x000F
#define LCD_DARKGREEN  0x03E0
#define LCD_MAROON     0x7800
#define LCD_ORANGE     0xFD20

/* Available font heights in pixels (glyph width = height / 2) */
typedef enum {
    LCD_FONT_16 = 16,
    LCD_FONT_24 = 24,
    LCD_FONT_32 = 32
} lcd_font_t;

/* ---- Graphics layer ---------------------------------------------------- */

/* Resets and initialises the panel, turns the backlight on.
 * Returns the ID read from the controller (0x81B3 for the ST7789v3). */
uint16_t lcd_init(void);

void lcd_clear(uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* Draws one character cell (font / 2 wide, font high). Characters outside
 * ' '..'}' are shown as '?'. */
void lcd_draw_char(uint16_t x, uint16_t y, char c,
                   lcd_font_t font, uint16_t fg, uint16_t bg);

/* Draws a string (no wrapping) and returns the x position after it. */
uint16_t lcd_draw_string(uint16_t x, uint16_t y, const char *s,
                         lcd_font_t font, uint16_t fg, uint16_t bg);

/* Pixel width of a string in the given font. */
uint16_t lcd_text_width(const char *s, lcd_font_t font);

void lcd_backlight(int on);

#ifdef __cplusplus
}
#endif

#endif /* LCD_H */
