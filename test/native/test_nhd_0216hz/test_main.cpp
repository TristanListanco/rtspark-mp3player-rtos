/*
 * Unit tests for the NHD_0216HZ port (src/NHD_0216HZ.cpp): the course
 * driver's API and HD44780 behaviour, drawn on a fake ST7789 that records
 * which character ends up in each cell.
 *   pio test -e native -f native/test_nhd_0216hz
 */
#include <string.h>
#include <unity.h>

#include "../../../src/NHD_0216HZ.cpp"      /* unit under test */

/* ---- Fake ST7789 (lcd.h) ------------------------------------------------ */

static const int kX = 24, kY = 64;          /* window position used below */
static const int kW = 12, kH = 24;          /* LCD_FONT_24 cell */

static char screen[2][16];                  /* what is visible in the window */
static uint16_t screen_fg[2][16];
static int draw_calls;
static int draws_outside;                   /* chars drawn outside the window */
static int fills;

extern "C" void lcd_draw_char(uint16_t x, uint16_t y, char c,
                              lcd_font_t font, uint16_t fg, uint16_t bg)
{
    draw_calls++;
    TEST_ASSERT_EQUAL_INT(LCD_FONT_24, font);
    const int col = (x - kX) / kW, row = (y - kY) / kH;
    if (x < kX || y < kY || (x - kX) % kW || (y - kY) % kH ||
        col < 0 || col >= 16 || row < 0 || row >= 2) {
        draws_outside++;
        return;
    }
    screen[row][col] = c;
    screen_fg[row][col] = fg;
}

extern "C" void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    fills++;
    /* the window is only ever blanked as a whole */
    TEST_ASSERT_EQUAL_INT(kX, x);
    TEST_ASSERT_EQUAL_INT(kY, y);
    TEST_ASSERT_EQUAL_INT(16 * kW, w);
    TEST_ASSERT_EQUAL_INT(2 * kH, h);
    memset(screen, '.', sizeof screen);     /* '.' = blank pixels */
}

static const char *row_text(int row)
{
    static char buf[17];
    memcpy(buf, screen[row], 16);
    buf[16] = '\0';
    return buf;
}

void setUp(void)
{
    memset(screen, '#', sizeof screen);     /* '#' = never drawn */
    memset(screen_fg, 0, sizeof screen_fg);
    draw_calls = draws_outside = fills = 0;
}

void tearDown(void)
{
    TEST_ASSERT_EQUAL_INT(0, draws_outside);
}

/* ---- tests ---------------------------------------------------------------- */

static void test_nothing_drawn_before_init(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.write_data('A');                    /* display still off */
    TEST_ASSERT_EQUAL_INT(0, draw_calls);
    TEST_ASSERT_FALSE(lcd.display_on());
}

static void test_init_clears_and_homes(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    TEST_ASSERT_TRUE(lcd.display_on());
    TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());
    TEST_ASSERT_EQUAL_STRING("                ", row_text(0));
    TEST_ASSERT_EQUAL_STRING("                ", row_text(1));
}

static void test_set_cursor_uses_hd44780_addresses(void)
{
    /* original formula: (row * LINE_LENGTH + column) | TOT_LENGTH */
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.set_cursor(0, 0);   TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());
    lcd.set_cursor(5, 0);   TEST_ASSERT_EQUAL_HEX8(0x05, lcd.address());
    lcd.set_cursor(0, 1);   TEST_ASSERT_EQUAL_HEX8(0x40, lcd.address());
    lcd.set_cursor(15, 1);  TEST_ASSERT_EQUAL_HEX8(0x4F, lcd.address());
}

static void test_printf_two_lines_like_the_mbed_player(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.set_cursor(0, 0);
    lcd.printf("%s", "Fur Elise -");
    lcd.set_cursor(0, 1);
    lcd.printf("%s", "Beethoven");
    TEST_ASSERT_EQUAL_STRING("Fur Elise -     ", row_text(0));
    TEST_ASSERT_EQUAL_STRING("Beethoven       ", row_text(1));
    TEST_ASSERT_EQUAL_CHAR('F', lcd.char_at(0, 0));
    TEST_ASSERT_EQUAL_CHAR('B', lcd.char_at(0, 1));
}

static void test_printf_formats_arguments(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.printf("Song %d/%d", 3, 8);
    TEST_ASSERT_EQUAL_STRING("Song 3/8        ", row_text(0));
}

static void test_printf_sixteen_char_name_does_not_overflow(void)
{
    /* The original used char buffer[16] + vsprintf: 16 characters plus the
     * terminator overflowed it. The port must show all 16 (ASan checks the
     * buffer). */
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.printf("%s", "Turkish March - ");
    TEST_ASSERT_EQUAL_STRING("Turkish March - ", row_text(0));
    lcd.set_cursor(0, 1);
    lcd.printf("%s", "Symphony No. 40 ");
    TEST_ASSERT_EQUAL_STRING("Symphony No. 40 ", row_text(1));
}

static void test_printf_stops_at_16_characters(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.printf("%s", "This line is far too long for the display");
    TEST_ASSERT_EQUAL_STRING("This line is far", row_text(0));
    TEST_ASSERT_EQUAL_HEX8(0x10, lcd.address());   /* cursor after 16 chars */
    TEST_ASSERT_EQUAL_STRING("                ", row_text(1));
}

static void test_clr_lcd_blanks_and_homes(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.printf("hello");
    lcd.set_cursor(3, 1);
    lcd.printf("world");
    lcd.clr_lcd();
    TEST_ASSERT_EQUAL_STRING("                ", row_text(0));
    TEST_ASSERT_EQUAL_STRING("                ", row_text(1));
    TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());
}

static void test_writes_past_column_16_stay_off_screen(void)
{
    /* HD44780 DDRAM line 1 is 40 characters: 0x10..0x27 are not visible */
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.set_cursor(16, 0);
    const int before = draw_calls;
    lcd.write_data('X');
    TEST_ASSERT_EQUAL_INT(before, draw_calls);     /* nothing drawn */
    TEST_ASSERT_EQUAL_HEX8(0x11, lcd.address());
}

static void test_address_wraps_from_line_1_to_line_2(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.write_cmd(TOT_LENGTH | 0x27);              /* last DDRAM cell of line 1 */
    lcd.write_data('a');
    TEST_ASSERT_EQUAL_HEX8(0x40, lcd.address());   /* HD44780: 0x27 -> 0x40 */
    lcd.write_data('b');
    TEST_ASSERT_EQUAL_CHAR('b', lcd.char_at(0, 1));
    lcd.write_cmd(TOT_LENGTH | 0x67);              /* last cell of line 2 */
    lcd.write_data('c');
    TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());   /* 0x67 -> 0x00 */
}

static void test_invalid_ddram_address_is_normalised(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.write_cmd(TOT_LENGTH | 0x30);              /* gap between the lines */
    TEST_ASSERT_EQUAL_HEX8(0x40, lcd.address());
    lcd.write_cmd(TOT_LENGTH | 0x7F);
    TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());
}

static void test_display_off_blanks_and_on_restores(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.printf("keep me");
    lcd.write_cmd(0x08);                           /* display off */
    TEST_ASSERT_FALSE(lcd.display_on());
    TEST_ASSERT_EQUAL_STRING("................", row_text(0));
    TEST_ASSERT_EQUAL_CHAR('k', lcd.char_at(0, 0));  /* text kept in DDRAM */
    lcd.write_cmd(0x0C);                           /* display on */
    TEST_ASSERT_EQUAL_STRING("keep me         ", row_text(0));
}

static void test_entry_mode_and_cursor_shift(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.set_cursor(5, 0);
    lcd.write_cmd(0x04);                           /* entry mode: decrement */
    lcd.write_data('Z');
    TEST_ASSERT_EQUAL_HEX8(0x04, lcd.address());
    lcd.write_cmd(0x06);                           /* back to increment */
    lcd.write_cmd(0x14);                           /* cursor right */
    TEST_ASSERT_EQUAL_HEX8(0x05, lcd.address());
    lcd.write_cmd(0x10);                           /* cursor left */
    TEST_ASSERT_EQUAL_HEX8(0x04, lcd.address());
    lcd.write_cmd(0x02);                           /* return home */
    TEST_ASSERT_EQUAL_HEX8(0x00, lcd.address());
}

static void test_cgram_writes_are_ignored(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.write_cmd(0x40);                           /* set CGRAM address */
    lcd.write_data('Q');
    TEST_ASSERT_EQUAL_STRING("                ", row_text(0));
    lcd.set_cursor(0, 0);                          /* back to DDRAM */
    lcd.write_data('Q');
    TEST_ASSERT_EQUAL_CHAR('Q', screen[0][0]);
}

static void test_colours_are_used_for_new_text(void)
{
    NHD_0216HZ lcd(kX, kY);
    lcd.init_lcd();
    lcd.set_colors(LCD_YELLOW, LCD_BLACK);
    lcd.printf("x");
    TEST_ASSERT_EQUAL_HEX16(LCD_YELLOW, screen_fg[0][0]);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_drawn_before_init);
    RUN_TEST(test_init_clears_and_homes);
    RUN_TEST(test_set_cursor_uses_hd44780_addresses);
    RUN_TEST(test_printf_two_lines_like_the_mbed_player);
    RUN_TEST(test_printf_formats_arguments);
    RUN_TEST(test_printf_sixteen_char_name_does_not_overflow);
    RUN_TEST(test_printf_stops_at_16_characters);
    RUN_TEST(test_clr_lcd_blanks_and_homes);
    RUN_TEST(test_writes_past_column_16_stay_off_screen);
    RUN_TEST(test_address_wraps_from_line_1_to_line_2);
    RUN_TEST(test_invalid_ddram_address_is_normalised);
    RUN_TEST(test_display_off_blanks_and_on_restores);
    RUN_TEST(test_entry_mode_and_cursor_shift);
    RUN_TEST(test_cgram_writes_are_ignored);
    RUN_TEST(test_colours_are_used_for_new_text);
    return UNITY_END();
}
