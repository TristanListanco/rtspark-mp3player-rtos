/*----------------------------------------------------------------------------
 Header file for Newhaven NHD0216HZ LCD
 *----------------------------------------------------------------------------
 * RT-Spark port of the course-provided driver
 * (Laboratory Activity 2 Resources/NHD_0216HZ.h, ARM University Program 2014).
 *
 * The public interface is unchanged: init_lcd, write_cmd, write_data,
 * printf, set_cursor, clr_lcd, and commands keep their HD44780 meaning
 * (0x01 clear, 0x80 | address = set cursor, ...). What changed:
 *
 *  - The original drives a 16x2 Newhaven LCD through a 74HC595 shift
 *    register (bit-banged SPI: shift_out, write_4bit). The RT-Spark has an
 *    on-board ST7789 graphics LCD instead, so write_cmd / write_data now go
 *    to a model of the HD44780 controller (display RAM + address counter)
 *    that draws the 16x2 characters in a window of the ST7789 (lcd.h).
 *    shift_out / write_4bit and the SPI pins are therefore gone.
 *  - The constructor takes the window position instead of three SPI pins.
 *  - ENABLE / DATA_MODE / COMMAND_MODE (74HC595 bit positions) are removed;
 *    ENABLE also clashes with the STM32 HAL's FunctionalState enumerator.
 *  - printf formats into a 17-byte buffer with vsnprintf. The original's
 *    vsprintf into char[16] overflows on 16-character song names such as
 *    "Turkish March - " (16 characters + terminator = 17 bytes).
 *  - The mbed delays (wait_us, ThisThread::sleep_for) are removed: there is
 *    no bus timing to respect.
 *
 * init_lcd() sets up the character window only; call lcd_init() once first
 * to initialise the ST7789 panel. Not thread-safe (use the LCD mutex).
 *----------------------------------------------------------------------------*/

#ifndef NHD_0216HZ_H
#define NHD_0216HZ_H

#include <stdint.h>
#include "lcd.h"

//Define constants
#define LINE_LENGTH 0x40    /* DDRAM address of line 2 */
#define TOT_LENGTH 0x80     /* "set DDRAM address" command bit */

class NHD_0216HZ{
	public:
		static const int COLUMNS = 16;
		static const int ROWS = 2;

		/* (x, y): top-left pixel of the 16x2 window on the ST7789 */
		NHD_0216HZ(uint16_t x, uint16_t y, lcd_font_t font = LCD_FONT_24) noexcept;

		//Function prototypes
		void init_lcd(void);
		void write_cmd(int data);
		void write_data(char c);
		/* C-style variadic on purpose: same signature as the course driver */
		void printf(const char *format, ...) __attribute__((format(printf, 2, 3)));  // NOLINT(cert-dcl50-cpp)
		void set_cursor(int column, int row);
		void clr_lcd(void);

		/* Added for the colour panel (the NHD LCD is monochrome) */
		void set_colors(uint16_t fg, uint16_t bg);

		/* State of the emulated controller (used by the unit tests) */
		char char_at(int column, int row) const;
		uint8_t address() const { return _ac; }
		bool display_on() const { return _display_on; }

	private:
		static const int DDRAM_SIZE = LINE_LENGTH + 0x28;   /* 0x00-0x27, 0x40-0x67 */

		void draw_cell(uint8_t addr);
		void redraw(void);
		void advance(void);

		uint16_t _x;
		uint16_t _y;
		lcd_font_t _font;
		uint16_t _fg;
		uint16_t _bg;
		char _ddram[DDRAM_SIZE];
		uint8_t _ac;            /* address counter (cursor) */
		bool _increment;        /* entry mode I/D */
		bool _display_on;
		bool _cgram;            /* CGRAM selected: data writes are ignored */
};
#endif

// *******************************ARM University Program Copyright (c) ARM Ltd 2014*************************************
