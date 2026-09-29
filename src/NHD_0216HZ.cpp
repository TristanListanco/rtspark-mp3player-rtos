/*----------------------------------------------------------------------------
 Newhaven NHD0216HZ LCD library
 *----------------------------------------------------------------------------
 * RT-Spark port of the course-provided driver
 * (Laboratory Activity 2 Resources/NHD_0216HZ.cpp). init_lcd, printf,
 * set_cursor and clr_lcd are the original code; write_cmd / write_data now
 * drive an HD44780 model drawn on the ST7789 instead of the 74HC595 shift
 * register. See NHD_0216HZ.h for the list of changes.
 *----------------------------------------------------------------------------*/

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "NHD_0216HZ.h"

NHD_0216HZ::NHD_0216HZ(uint16_t x, uint16_t y, lcd_font_t font) noexcept
    : _x(x), _y(y), _font(font), _fg(LCD_WHITE), _bg(LCD_BLACK),
      _ac(0), _increment(true), _display_on(false), _cgram(false)
{
    memset(_ddram, ' ', sizeof _ddram);
}

void NHD_0216HZ::init_lcd(void) {
    /* Same command sequence as the original. The function-set commands
     * (4-bit interface, 2 lines) have no effect on the emulated controller. */
    write_cmd(0x20);    //function set
    write_cmd(0x20);    //function set
    write_cmd(0x0C);    //display ON/OFF
    write_cmd(0x01);    //display clear
    write_cmd(0x06);    //entry-mode set
    write_cmd(0x28);    //function set
    set_cursor(0,0);
}

/* HD44780 instruction set (cursor/blink display and display shift aren't
 * modelled; they aren't used by the player). */
void NHD_0216HZ::write_cmd(int cmd) {
    cmd &= 0xFF;
    if (cmd & TOT_LENGTH) {                 /* 1aaa aaaa: set DDRAM address */
        uint8_t addr = (uint8_t)(cmd & 0x7F);
        if (addr >= 0x28 && addr < LINE_LENGTH) {
            addr = LINE_LENGTH;             /* gap between the lines */
        } else if (addr >= LINE_LENGTH + 0x28) {
            addr = 0;
        }
        _ac = addr;
        _cgram = false;
    } else if (cmd & 0x40) {                /* 01aa aaaa: set CGRAM address */
        _cgram = true;                      /* custom characters: not supported */
    } else if (cmd & 0x20) {                /* 001x xxxx: function set */
        /* interface width / lines / font: nothing to do */
    } else if (cmd & 0x10) {                /* 0001 SRxx: cursor or display shift */
        if ((cmd & 0x08) == 0) {            /* cursor move */
            const bool right = (cmd & 0x04) != 0;
            const bool saved = _increment;
            _increment = right;
            advance();
            _increment = saved;
        }
    } else if (cmd & 0x08) {                /* 0000 1DCB: display on/off */
        const bool on = (cmd & 0x04) != 0;
        if (on != _display_on) {
            _display_on = on;
            redraw();
        }
    } else if (cmd & 0x04) {                /* 0000 01IS: entry mode */
        _increment = (cmd & 0x02) != 0;
    } else if (cmd & 0x02) {                /* 0000 001x: return home */
        _ac = 0;
        _cgram = false;
    } else if (cmd & 0x01) {                /* 0000 0001: clear display */
        memset(_ddram, ' ', sizeof _ddram);
        _ac = 0;
        _increment = true;
        _cgram = false;
        redraw();
    }
}

void NHD_0216HZ::write_data(char c) {
    if (_cgram) {
        return;
    }
    _ddram[_ac] = c;
    draw_cell(_ac);
    advance();
}

void NHD_0216HZ::printf(const char *format, ...) {  // NOLINT(cert-dcl50-cpp): course API
    va_list v;
    char buffer[COLUMNS + 1];               /* original: char buffer[16] + vsprintf */
    va_start(v, format);
    (void)vsnprintf(buffer, sizeof buffer, format, v);   /* truncates at 16, like the LCD */
    va_end(v);

    const char *b = buffer;
    for(int i=0; i<COLUMNS && *b; i++) {
        write_data(*b++);
    }
}

void NHD_0216HZ::set_cursor(int column, int row) {
    int addr;

    addr = (row * LINE_LENGTH) + column;
    addr |= TOT_LENGTH;
    write_cmd(addr);
}

void NHD_0216HZ::clr_lcd(void) {
    write_cmd(0x01);    //display clear
}

void NHD_0216HZ::set_colors(uint16_t fg, uint16_t bg) {
    _fg = fg;
    _bg = bg;
}

char NHD_0216HZ::char_at(int column, int row) const {
    if (column < 0 || column >= COLUMNS || row < 0 || row >= ROWS) {
        return '\0';
    }
    return _ddram[row * LINE_LENGTH + column];
}

/* Address counter step with the HD44780 two-line wrap:
 * 0x27 -> 0x40 and 0x67 -> 0x00 (and the reverse when decrementing). */
void NHD_0216HZ::advance(void) {
    if (_increment) {
        _ac = (_ac == 0x27) ? LINE_LENGTH : (_ac == LINE_LENGTH + 0x27) ? 0 : (uint8_t)(_ac + 1);
    } else {
        _ac = (_ac == 0) ? LINE_LENGTH + 0x27 : (_ac == LINE_LENGTH) ? 0x27 : (uint8_t)(_ac - 1);
    }
}

/* Draws one DDRAM position if it is in the visible 16 columns. */
void NHD_0216HZ::draw_cell(uint8_t addr) {
    if (!_display_on) {
        return;
    }
    const int row = (addr >= LINE_LENGTH) ? 1 : 0;
    const int column = addr - row * LINE_LENGTH;
    if (column >= COLUMNS) {
        return;
    }
    const uint16_t w = _font / 2;
    lcd_draw_char((uint16_t)(_x + column * w), (uint16_t)(_y + row * _font),
                  _ddram[addr], _font, _fg, _bg);
}

void NHD_0216HZ::redraw(void) {
    const uint16_t w = (uint16_t)(COLUMNS * (_font / 2));
    const uint16_t h = (uint16_t)(ROWS * _font);
    if (!_display_on) {
        lcd_fill_rect(_x, _y, w, h, _bg);
        return;
    }
    for (int row = 0; row < ROWS; row++) {
        for (int column = 0; column < COLUMNS; column++) {
            draw_cell((uint8_t)(row * LINE_LENGTH + column));
        }
    }
}

// *******************************ARM University Program Copyright (c) ARM Ltd 2014*************************************
