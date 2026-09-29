# RT-Spark Personal MP3 Player (BCA182 Laboratory Activity 2)

An 8-song "MP3 player" for the RT-Thread **RT-Spark** board (STM32F407ZGT6), built with
**PlatformIO + STM32Cube HAL + FreeRTOS** (no Arduino framework).

- Songs play through the on-board **ES8388** codec to a speaker on the 3.5 mm headphone jack.
- The song name and a confirmation message appear on the on-board **240×240 ST7789 LCD**.
- Buttons **B2–B4** pick a song in binary, **B1** selects and confirms (5 s window), **USER_BUTTON** stops/plays.
- An **RGB LED** shows the state (blue playing, red paused, green changing song).
- A **potentiometer** sets the volume.
- Usage instructions are printed on the **UART** console.
- Three FreeRTOS threads run cooperatively, the LCD is protected by a **mutex**, and the MCU sleeps when idle.

## How each requirement is met

| Lab requirement | Where / how |
|---|---|
| Define missing inputs and outputs | `include/main.h` (pin map), "Inputs and outputs" section of `src/main.cpp`, `MX_GPIO_Init()` |
| `update_lcd_leds_thread()`, `polling_buttons()`, `adjust_volume()` | `src/main.cpp`, the three thread functions |
| B2–B4 give the song number in binary; nothing pressed = first song | `polling_buttons()`: `index = B2<<2 \| B3<<1 \| B4` (pressed = 1) |
| Hold B2–B4, press B1 to select, release, press B1 again to confirm | `polling_buttons()`, B1 edge handling |
| 5 s to confirm, otherwise continue normally | `Timeout confirm_timeout` → `confirm_window_expired()` |
| Song name on LCD while playing, confirmation message when changing | `update_lcd_leds_thread()` (two-line name via NHD-style `set_cursor`/`print_lcd`) |
| Timeout for the 5 s window, Ticker to play music | `include/timer_if.h`: mbed-style `Ticker`/`Timeout` on hardware timers TIM2/TIM5 |
| USER_BUTTON stops / plays | `polling_buttons()` → `stop_playback()` / `start_playback()` |
| Blue = playing, red = paused, green = changing song | `update_lcd_leds_thread()` |
| 3 threads, infinite loops, cooperative scheduling | `configUSE_PREEMPTION 0`; every thread ends its loop with `vTaskDelay()` |
| Mutex for exclusive LCD access | `lcd_mutex` (LCD thread and volume thread both draw) |
| Adjust the volume | `adjust_volume()`: ADC3 → ES8388 DAC volume over I2C2 |
| main: clear LCD, start threads, UART instructions, sleep mode | `main()`; sleep happens in `vApplicationIdleHook()` (`src/freertos_hooks.c`) |

## Wiring

External parts go on a breadboard. They connect to the **40-pin header** (right edge of the board)
and the **PMOD2 header** (top edge, next to the LCD).

Header rows below are counted from the end nearest the LCD, as in RT-Thread's
[RT-Spark pin map](https://github.com/RT-Thread/rt-thread/blob/master/bsp/stm32/stm32f407-rt-spark/applications/arduino_pinout/Rt-spark_Rtduino_Pin_Map.drawio.png).
Check each pin against that picture before wiring.

### Push buttons B1–B4 (handout Figure 1.3: pull-up, active LOW)

Each button: **3.3 V → 330 Ω → GPIO pin**, and **GPIO pin → button → GND**.

| Button | Function | MCU pin | 40-pin header (left column) |
|---|---|---|---|
| B1 | select / confirm | **PD10** | row 16 |
| B2 | song bit 2 (MSB) | **PD8** | row 17 |
| B3 | song bit 1 | **PE14** | row 18 |
| B4 | song bit 0 (LSB) | **PE12** | row 19 |
| 3.3 V | resistor supply | 3.3V | row 1 or 9 |
| GND | button return | GND | row 20 |

The internal pull-ups are also enabled, so a missing 330 Ω resistor won't leave an input floating.

**USER_BUTTON** is the on-board **KEY_LEFT** key (PC0). It needs no wiring.

### RGB LED (handout Figure 1.4)

| Colour | MCU pin | 40-pin header (right column) |
|---|---|---|
| Red   | **PG2** | row 11 |
| Green | **PG4** | row 12 |
| Blue  | **PG6** | row 13 |
| Common | GND (common cathode) or 3.3 V (common anode) | row 10 = GND |

Put a series resistor on each colour.

The code assumes a **common-cathode** LED. For a **common-anode** LED, set `RGB_LED_COMMON_ANODE` to `1` in `include/main.h`.

The on-board red (PF12) and blue (PF11) LEDs mirror the red/blue state, so you can test before the breadboard is wired.

### Potentiometer (handout Figure 1.2)

**3.3 V → 10 kΩ pot → 100 Ω → GND**, with the **wiper → PF6** (PMOD2 header, pin labelled **A0**, ADC3_IN4).
3.3 V and GND are on the same PMOD2 row.

### Speaker

Plug the speaker (ABI-001-RC) into the on-board **3.5 mm headphone jack** with a stereo (TRS) plug or breakout.
Connect it between **Tip (left) and Sleeve (GND)**. Both channels carry the same signal. Don't join L and R.

### On-board (no wiring, for reference)

| Peripheral | Connection |
|---|---|
| LCD ST7789v3 | FSMC 8-bit, NE3 = PG10, RS = A18 (PD13), D0–D7 = PD14 PD15 PD0 PD1 PE7–PE10, RD = PD4, WR = PD5, RST = PD3, backlight = PF9 |
| ES8388 codec | I2S3: MCK PC7, WS PA15, CK PB3, SD PB5 (DMA1 Stream 7); I2C2: SCL PF1, SDA PF0, address 0x10 |
| UART console | USART1 TX PA9 / RX PA10, bridged to the ST-LINK USB virtual COM port |

## Build, flash, run

Requires PlatformIO (VS Code extension or CLI). Connect the board's **USB-DBG** (ST-LINK) port.

```sh
pio run                  # build
pio run -t upload        # flash through the on-board ST-LINK
pio device monitor       # UART console, 115200 baud
```

In VS Code: PlatformIO sidebar → *rtspark* → **Build**, **Upload**, **Monitor**.

After reset, the console prints the instructions and the song table:

```
B2 B3 B4   Song
  0  0  0   1. Fur Elise - Beethoven
  0  0  1   2. Canon In D - Pachebelbel
  0  1  0   3. Minuet in G major - Bach
  0  1  1   4. Turkish March - Mozart
  1  0  0   5. Nocturne in E flat -Chopin
  1  0  1   6. Waltz No. 2 - Shostakovich
  1  1  0   7. Nocturne in C sharp - Chopin
  1  1  1   8. Symphony No. 40 - Mozart

LCD id 0x81B3 | ES8388 OK | I2S 32609 Hz
Playback is stopped: press USER to play song 1.
```

## Using the player

1. Press **USER (KEY_LEFT)** to play. The LED turns **blue**. Press again to pause (**red**).
2. To change song, hold **B2 B3 B4** for the song number in binary. Nothing held = song 1 (Für Elise), B2+B4 = `101` = song 6 (Waltz No. 2).
3. Keep them held and press **B1**, then release everything. The LED turns **green** and the LCD asks
   "Change song?" with a countdown.
4. Press **B1** again within **5 s** to play the new song from the start. If you don't, the display returns
   to the current song and playback carries on as before.
5. Turn the **potentiometer** to change the volume. The LCD shows the level at the bottom.

## Software design

```
            +-------------------------+     +---------------------+
Ticker ISR  | play_next_note()        | --> | audio_player.c      | --DMA--> I2S3 --> ES8388 --> jack
(TIM2)      | next note, re-arm       |     | wavetable synth     |
            +-------------------------+     +---------------------+
Timeout ISR | confirm_window_expired()|
(TIM5)      +-------------------------+
                 ^ shared state (volatile, critical sections)
   +---------------------+  +-------------------+  +-----------------+
   | update_lcd_leds     |  | polling_buttons   |  | adjust_volume   |
   | LCD + RGB, 50 ms    |  | debounce, 10 ms   |  | ADC -> I2C, 100 ms
   +---------------------+  +-------------------+  +-----------------+
            \____ lcd_mutex ____________________________/
   idle task -> vApplicationIdleHook() -> Sleep mode (WFI)
```

- **Scheduling.** All three threads have the same priority and `configUSE_PREEMPTION` is `0`. A thread keeps
  the CPU until it calls `vTaskDelay()` at the end of its update, then the next ready thread runs.
  When none is ready, the idle hook enters **Sleep mode**: the CPU clock stops, and DMA, timers and SysTick keep running.
- **Ticker and Timeout** (`timer_if.h/.cpp`) mimic mbed's API. Each owns a 32-bit timer counting at 1 MHz and calls
  the attached function from its interrupt. The music ticker re-attaches itself with each note's duration
  (`beat × 8 × tempo`, see below), which is how the mbed Module 8 player advances notes.
- **Sound.** The mbed version sets a PWM period for each note. The ES8388 takes PCM samples instead, so
  `audio_player.c` synthesises the note with a wavetable and phase accumulator into a double-buffered circular DMA.
  `song_def.h` stores each note as the PWM **period in milliseconds** (`La` = 2.272 ms), so the player converts it
  with frequency = 1000 / period.
  A short attack/release envelope avoids clicks, and each note sounds for 90 % of its length so repeated notes stay distinct.
  Rests (0 Hz notes) are silent.
- **Volume** is the ES8388 DAC digital attenuation: 0 dB down to −60 dB, linear in dB across the pot travel, and mute at 0 %.
- **Interrupt priorities.** The audio DMA and both timers use priority 6. That is below
  `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5), so `taskENTER_CRITICAL()` masks them while the threads read
  multi-field state. Having the same priority, they never preempt each other.

### Files

| File | Purpose |
|---|---|
| `src/main.cpp` | Application: state, Ticker/Timeout callbacks, the 3 threads, `main()` |
| `include/main.h` | Pin map (CubeMX-style labels), handles, IRQ priorities |
| `src/board_init.c` | `SystemClock_Config()`, `MX_*_Init()`, HAL MSP callbacks, `Error_Handler()` |
| `src/stm32f4xx_it.c` | SysTick (HAL + FreeRTOS tick), DMA, fault handlers |
| `src/freertos_hooks.c` | Idle hook (sleep mode), stack-overflow and malloc-failed hooks |
| `include/timer_if.h`, `src/timer_if.cpp` | mbed-style `Ticker` and `Timeout` |
| `include/lcd.h`, `src/lcd.c`, `include/lcd_font.h` | ST7789 driver + NHD_0216HZ-style text API |
| `include/es8388.h`, `src/es8388.c` | Codec setup and volume over I2C2 |
| `include/audio_player.h`, `src/audio_player.c` | I2S DMA tone synthesiser |
| `include/console.h`, `src/console.c` | Mutex-protected UART printing |
| `include/song.h`, `include/song_def.h` | Course-provided `Song` class and the 10 songs (unchanged copies) |
| `include/FreeRTOSConfig.h`, `include/stm32f4xx_hal_conf.h` | RTOS and HAL configuration |
| `lib/FreeRTOS-Kernel/` | FreeRTOS V11.1.0 (ARM_CM4F port, heap_4) |
| `tools/fpu_flags.py` | Adds the Cortex-M4F hard-float flags PlatformIO doesn't set by default |

### Course files

`Laboratory Activity 2 Resources/` holds the files handed out with the lab:

- **`song.h`, `song_def.h`:** used unchanged. They're copied to `include/`.
  - `main.cpp` lists the ten `Song` objects in `songs[]`.
  - Only the first 8 can be chosen with three buttons, so Symphony No. 5 and Eine Kleine Nachtmusik are defined but not selectable.
- **`NHD_0216HZ.h/.cpp`:** mbed-only (SPI shift register, `DigitalOut`), so they're used as the basis for `lcd.h/lcd.c`.
  That driver keeps the same calls (`init_lcd`, `clr_lcd`, `set_cursor`, and `print_lcd` for `printf`) on the ST7789.
- **`main.cpp` (mbed basis):** not in the folder. Two things were therefore inferred from `song_def.h`:
  - **Pitch:** `note[]` holds PWM periods in ms, and `No` (0) is a rest. `note_hz()` converts them.
  - **Duration:** `note_seconds()` computes `beat × 8 × tempo`. `beat` is a fraction of a whole note (b0 = 1 … b3 = 1/8),
    so `tempo` is the length of a b3 note in seconds.
    - This scale gives sensible tempos for the fast and medium pieces.
    - Nocturne in E♭ writes every eighth note as `b0`, so it plays slowly (about 4 minutes).
    - If the course `main.cpp` uses another formula, change `kTempoScale` / `note_seconds()` in `src/main.cpp`.
      That's the only place timing is defined.

## STM32CubeMX equivalent

If a `.ioc` is required, create one for STM32F407ZGTx with the settings below. The generated init code
matches `src/board_init.c`.

| Block | Setting |
|---|---|
| RCC | HSE crystal 8 MHz; PLL M 4, N 168, P 2, Q 7 → 168 MHz; APB1 /4, APB2 /2; PLLI2S N 192, R 2 |
| SYS | Debug: Serial Wire; timebase SysTick (shared with FreeRTOS) |
| GPIO out | PG2, PG4, PG6 (RGB); PF11, PF12 (board LEDs); PD3 (LCD_RST); PF9 (LCD_BL) |
| GPIO in (pull-up) | PD10, PD8, PE14, PE12 (B1–B4); PC0 (USER) |
| ADC3 | IN4 (PF6), 12-bit, software start, 480 cycles |
| I2C2 | PF0 SDA, PF1 SCL, 100 kHz |
| I2S3 | Half-duplex master TX, Philips, 16-bit, MCLK output, 32 kHz; DMA1 Stream 7 circular, half-word |
| FSMC | NOR/SRAM 3, chip select NE3, LCD interface, 8-bit, A18 as RS |
| USART1 | PA9/PA10, 115200 8N1 |
| NVIC | Priority group 4; DMA1 Stream 7, TIM2, TIM5 = priority 6 |
| FREERTOS | Preemption off, 1 kHz tick, 16 KB heap_4, idle hook on |

## Verification status

- **Build:** compiles with no warnings under `-Wall -Wextra`.
  - Flash: 54 KB of 1 MB. RAM: 31 KB of 128 KB (16 KB FreeRTOS heap, about 10 KB of song arrays).
  - The ELF was checked for the hard-float ABI and for the correct SVC, PendSV, SysTick, DMA1_Stream7, TIM2 and TIM5 vector entries.
- **Audio path (off-target):** `audio_player.c` and the course `song_def.h` were compiled on a PC with a stubbed HAL,
  and all 10 songs (1,198 notes) were rendered with the firmware's note timing and pitch conversion.
  - All 856 pitched notes were within ±25 cents of their target.
  - All 342 rests were silent, with no clipping and no discontinuities.
- **On hardware:** not yet run.
  - These peripheral settings come from the RT-Spark schematic and RT-Thread's official BSP:
    LCD FSMC bank and A18 RS line, the ST7789 init sequence, I2S3/I2C2 pins and the ES8388 register sequence.
  - At first power-up, check the UART line `LCD id 0x81B3 | ES8388 OK`.

## Troubleshooting

| Symptom | Check |
|---|---|
| On-board red LED blinks fast | An init step failed (`Error_Handler`). Re-flash; check the board's power switch |
| `ES8388 NOT RESPONDING` | I2C2 (PF0/PF1) is shared with the on-board sensors; power-cycle the board |
| No sound but the LED is blue | Volume at 0 % (pot fully down); speaker on Tip + Sleeve of the jack |
| Buttons do nothing | Buttons must connect the pin to **GND** when pressed; check the 330 Ω pull-ups to 3.3 V |
| LED colours inverted | Set `RGB_LED_COMMON_ANODE` in `include/main.h` |
| Upload fails after the first flash | The MCU sleeps when idle. Hold RESET while starting the upload, or use `upload_flags = -c "reset_config srst_only srst_nogate connect_assert_srst"` |

## Credits

- LCD init sequence, bitmap font and ES8388 register sequence adapted from the
  [RT-Thread RT-Spark BSP](https://github.com/RT-Thread/rt-thread/tree/master/bsp/stm32/stm32f407-rt-spark) (Apache-2.0).
- I2S3 pin mapping from the [RT-Spark SDK](https://github.com/RT-Thread-Studio/sdk-bsp-stm32f407-spark) wavplayer example.
- [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) V11.1.0 (MIT).
