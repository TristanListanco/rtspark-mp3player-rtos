/*
 * lcd.c - ST7789v3 240x240 LCD on the RT-Spark, driven through FSMC.
 *
 * Wiring (RT-Spark schematic): 8-bit 8080 bus on FSMC bank 1, chip select NE3
 * (PG10), RS/DCX on address line A18 (PD13). With an 8-bit bus, CPU address
 * bit 18 appears directly on FSMC_A18, so:
 *   0x6800_0000 (A18 = 0) -> command register
 *   0x6804_0000 (A18 = 1) -> data / GRAM
 * The panel init sequence is the one used by the RT-Thread RT-Spark BSP
 * (board/ports/lcd/drv_lcd.c).
 */
#include "lcd.h"
#include "lcd_font.h"
#include "main.h"

#define LCD_CMD_REG   (*(volatile uint8_t *)0x68000000UL)
#define LCD_DATA_REG  (*(volatile uint8_t *)0x68040000UL)

static inline void lcd_write_cmd(uint8_t cmd)  { LCD_CMD_REG = cmd; }
static inline void lcd_write_data(uint8_t d)   { LCD_DATA_REG = d; }
static inline uint8_t lcd_read_data(void)      { return LCD_DATA_REG; }

static inline void lcd_write_pixel(uint16_t color)
{
    LCD_DATA_REG = (uint8_t)(color >> 8);   /* RGB565, high byte first */
    LCD_DATA_REG = (uint8_t)(color & 0xFF);
}

static void lcd_cmd_args(uint8_t cmd, const uint8_t *args, uint8_t n)
{
    lcd_write_cmd(cmd);
    for (uint8_t i = 0; i < n; i++) {
        lcd_write_data(args[i]);
    }
}

/* Sets the drawing window and starts a GRAM write (RAMWR). */
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_write_cmd(0x2A);                    /* CASET */
    lcd_write_data(x0 >> 8);
    lcd_write_data(x0 & 0xFF);
    lcd_write_data(x1 >> 8);
    lcd_write_data(x1 & 0xFF);
    lcd_write_cmd(0x2B);                    /* RASET */
    lcd_write_data(y0 >> 8);
    lcd_write_data(y0 & 0xFF);
    lcd_write_data(y1 >> 8);
    lcd_write_data(y1 & 0xFF);
    lcd_write_cmd(0x2C);                    /* RAMWR */
}

void lcd_backlight(int on)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint16_t lcd_init(void)
{
    /* Hardware reset */
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100);

    /* RDDID (0x04): dummy byte, then ID1..ID3. The ST7789v3 reports 0x81B3. */
    lcd_write_cmd(0x04);
    (void)lcd_read_data();
    (void)lcd_read_data();
    uint16_t id = (uint16_t)lcd_read_data() << 8;
    id |= lcd_read_data();

    static const uint8_t porch[]     = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
    static const uint8_t power1[]    = { 0xA4, 0xA1 };
    static const uint8_t gamma_pos[] = { 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
                                         0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23 };
    static const uint8_t gamma_neg[] = { 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
                                         0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23 };
    uint8_t arg;

    arg = 0x00; lcd_cmd_args(0x36, &arg, 1);    /* MADCTL: normal orientation */
    arg = 0x65; lcd_cmd_args(0x3A, &arg, 1);    /* COLMOD: 16 bit/pixel RGB565 */
    lcd_cmd_args(0xB2, porch, sizeof porch);    /* Porch setting */
    arg = 0x35; lcd_cmd_args(0xB7, &arg, 1);    /* Gate control */
    arg = 0x37; lcd_cmd_args(0xBB, &arg, 1);    /* VCOM */
    arg = 0x2C; lcd_cmd_args(0xC0, &arg, 1);    /* LCM control */
    arg = 0x01; lcd_cmd_args(0xC2, &arg, 1);    /* VDV/VRH enable */
    arg = 0x12; lcd_cmd_args(0xC3, &arg, 1);    /* VRH */
    arg = 0x20; lcd_cmd_args(0xC4, &arg, 1);    /* VDV */
    arg = 0x0F; lcd_cmd_args(0xC6, &arg, 1);    /* 60 Hz frame rate */
    lcd_cmd_args(0xD0, power1, sizeof power1);  /* Power control 1 */
    lcd_cmd_args(0xE0, gamma_pos, sizeof gamma_pos);
    lcd_cmd_args(0xE1, gamma_neg, sizeof gamma_neg);
    lcd_write_cmd(0x21);                        /* Inversion on (IPS panel) */
    arg = 0x00; lcd_cmd_args(0x35, &arg, 1);    /* Tearing effect line on */
    lcd_write_cmd(0x11);                        /* Sleep out */
    HAL_Delay(120);
    lcd_write_cmd(0x29);                        /* Display on */

    lcd_clear(LCD_BLACK);
    lcd_backlight(1);
    return id;
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0 || h == 0) {
        return;
    }
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    lcd_set_window(x, y, x + w - 1, y + h - 1);
    for (uint32_t n = (uint32_t)w * h; n > 0; n--) {
        lcd_write_pixel(color);
    }
}

void lcd_clear(uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

static const uint8_t *glyph_for(char c, lcd_font_t font, uint16_t *bytes_per_row)
{
    if (c < LCD_FONT_FIRST_CHAR || c > LCD_FONT_LAST_CHAR) {
        c = '?';
    }
    const uint16_t w = font / 2;
    const uint16_t bpr = (w + 7) / 8;
    const uint32_t offset = (uint32_t)(c - LCD_FONT_FIRST_CHAR) * bpr * font;
    *bytes_per_row = bpr;

    switch (font) {
    case LCD_FONT_32: return asc2_3216 + offset;
    case LCD_FONT_24: return asc2_2412 + offset;
    default:          return asc2_1608 + offset;
    }
}

static void lcd_draw_char(uint16_t x, uint16_t y, char c,
                          lcd_font_t font, uint16_t fg, uint16_t bg)
{
    const uint16_t w = font / 2;
    const uint16_t h = font;
    if (x + w > LCD_WIDTH || y + h > LCD_HEIGHT) {
        return;
    }

    uint16_t bpr;
    const uint8_t *g = glyph_for(c, font, &bpr);

    lcd_set_window(x, y, x + w - 1, y + h - 1);
    for (uint16_t row = 0; row < h; row++) {
        const uint8_t *line = g + row * bpr;
        for (uint16_t col = 0; col < w; col++) {
            const uint8_t bit = line[col >> 3] & (0x80 >> (col & 7));
            lcd_write_pixel(bit ? fg : bg);
        }
    }
}

uint16_t lcd_draw_string(uint16_t x, uint16_t y, const char *s,
                         lcd_font_t font, uint16_t fg, uint16_t bg)
{
    const uint16_t w = font / 2;
    while (*s != '\0' && x + w <= LCD_WIDTH) {
        lcd_draw_char(x, y, *s++, font, fg, bg);
        x += w;
    }
    return x;
}

uint16_t lcd_text_width(const char *s, lcd_font_t font)
{
    uint16_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n * (font / 2);
}

/* ---- NHD_0216HZ-style character API ------------------------------------ */

static lcd_font_t text_font = LCD_FONT_24;
static uint16_t text_fg = LCD_WHITE;
static uint16_t text_bg = LCD_BLACK;
static uint16_t origin_x = 0;
static uint16_t origin_y = 0;
static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;

void init_lcd(void)
{
    (void)lcd_init();
}

void clr_lcd(void)
{
    lcd_clear(text_bg);
    set_cursor(0, 0);
}

void set_cursor(int column, int row)
{
    cursor_x = origin_x + (uint16_t)column * (text_font / 2);
    cursor_y = origin_y + (uint16_t)row * text_font;
}

void print_lcd(const char *string)
{
    cursor_x = lcd_draw_string(cursor_x, cursor_y, string, text_font, text_fg, text_bg);
}

void lcd_set_text_font(lcd_font_t font)
{
    text_font = font;
}

void lcd_set_text_color(uint16_t fg, uint16_t bg)
{
    text_fg = fg;
    text_bg = bg;
}

void lcd_set_text_origin(uint16_t x, uint16_t y)
{
    origin_x = x;
    origin_y = y;
}
