# Static analysis report

Covers the project's own code (`src/`, `include/`) and the course-provided files in
`Laboratory Activity 2 Resources/`. Third-party code isn't analysed: the FreeRTOS kernel,
the ST HAL/CMSIS, the bitmap font and ST's HAL config template.

## How to run

```sh
pio check            # cppcheck + clang-tidy, configured in platformio.ini
pio run              # the firmware build itself uses the strict warning set below
pio test -e native   # unit tests under AddressSanitizer + UndefinedBehaviorSanitizer
```

| Tool | Version | Configuration |
|---|---|---|
| cppcheck | 2.11 | `--enable=all`, C++14. The HAL headers are read for types; framework findings aren't reported |
| clang-tidy | 15 | `bugprone-*`, `cert-*`, `clang-analyzer-*` (Clang static analyzer), `performance-*`, `portability-*`, `misc-*`; exclusions listed below |
| GCC | 7.2 (arm-none-eabi) | `-Wall -Wextra` plus `-Wshadow -Wdouble-promotion -Wfloat-conversion -Wformat=2 -Wundef -Wcast-align -Wlogical-op -Wduplicated-cond -Wnull-dereference -Wmissing-declarations` (project sources only, via `build_src_flags`) |
| ASan + UBSan | Apple clang 21 | all native unit tests (`test/native`) |

## Result for the project code

| Tool | First run | Now |
|---|---|---|
| cppcheck | 1 medium, 83 low | **0** |
| clang-tidy | 39 medium (29 in our code) | **0** |
| GCC strict warnings | 10 | **0** |
| Native tests under ASan/UBSan | — | **51/51 pass**, no sanitizer reports |

**Checking the tools themselves.** A scratch file with a `strcpy` overflow and a null dereference was run
through `pio check`. cppcheck reported both as *high* (`bufferAccessOutOfBounds`, `nullPointer`).
clang-tidy's analyzer reported both as well (`insecureAPI.strcpy`, `core.NullDereference`). The file was then deleted.

### Fixed

| Tool / check | Where | Problem | Fix |
|---|---|---|---|
| cppcheck `negativeIndex` (medium) | `timer_if.cpp` | After `if (slot_ < 0) Error_Handler();` the analyzer saw a path reading `kTimers[-1]`, because `Error_Handler` wasn't known to never return | `Error_Handler` declared `__attribute__((noreturn))` in `main.h` |
| cppcheck `shadowFunction` | `main.cpp` | Local `index` shadowed POSIX `index()` | renamed (`selected`, `song`) |
| cppcheck `cstyleCast` | `main.cpp` | C cast inside FreeRTOS's `xSemaphoreGive` macro, at every give | LCD mutex wrapped in an RAII `LcdLock`: one take/give pair, one commented suppression |
| clang-tidy `cert-err33-c` ×7 | `main.cpp`, `player_logic.cpp`, `NHD_0216HZ.cpp` | `snprintf`/`vsnprintf` result ignored | explicit `(void)`: truncation to the buffer is intended |
| clang-tidy `cert-err58-cpp` ×4 | `main.cpp` globals | constructors of static objects "may throw" | `NHD_0216HZ`, `SongSelector`, `Ticker`, `Timeout` constructors marked `noexcept` |
| clang-tidy `bugprone-sizeof-expression` | `es8388.c` | `sizeof seq / sizeof seq[0]` on a 2-D array looked suspicious | init sequence rewritten as an array of `{reg, val}` structs |
| clang-tidy `bugprone-incorrect-roundings` | `timer_if.cpp` | `(uint32_t)(x + 0.5f)` rounding | `lroundf()` |
| clang-tidy `misc-const-correctness` ×2 | `timer_if.cpp` | lock guards not `const` | `const IrqLock lock;` |
| GCC `-Wmissing-declarations` ×10 | `stm32f4xx_it.c`, `timer_if.cpp` | interrupt handlers without prototypes | `include/stm32f4xx_it.h`, as STM32CubeMX generates |

### Accepted, with reasons

| Check | Where | Why it stays |
|---|---|---|
| cppcheck `unusedFunction` | IRQ handlers, FreeRTOS hooks, cross-file calls | Handlers and hooks are called by the hardware and the kernel. The rest is used across files, which this check doesn't follow as configured. Suppressed globally |
| cppcheck `constParameterPointer` | `HAL_*_MspInit`, `HAL_I2S_Tx*Callback` | The signature must match the HAL's weak prototype. Suppressed inline |
| clang-tidy `cert-dcl50-cpp` | `NHD_0216HZ::printf` | C-style variadic function, kept because it's the course driver's API. `NOLINT` with reason; `format(printf)` attribute added so GCC checks the format strings |
| clang-tidy `performance-no-int-to-ptr` | `GPIOx`, `TIMx_BASE` | Memory-mapped registers are integer addresses cast to pointers by design. Check disabled |
| GCC `-Wfloat-conversion` | around `#include "song_def.h"` in `main.cpp` | The course file's `double` literals go into `float` arrays (1,123 warnings). Harmless; silenced for that include only |

## Findings in the course-provided files

The files in `Laboratory Activity 2 Resources/` were analysed as handed out. `song.h` and `song_def.h` are used
unchanged. `NHD_0216HZ.h/.cpp` are mbed-only and were ported to the on-board LCD, and the port fixes the driver bugs below.

| # | File | Found by | Issue | Effect / how it's handled here |
|---|---|---|---|---|
| 1 | `NHD_0216HZ.cpp` `printf` | unit test + AddressSanitizer | **Stack buffer overflow.** `char buffer[16]` + `vsprintf`: a 16-character string needs 17 bytes. Three course names are exactly 16 characters ("Turkish March - ", "Symphony No. 40 ", "Symphony No. 5 -"). ASan with the original code: `stack-buffer-overflow … WRITE of size 17` in `NHD_0216HZ::printf`. cppcheck and clang-tidy can't see it, because the length comes from the call site | Port uses `char buffer[17]` + `vsnprintf`. `test_printf_sixteen_char_name_does_not_overflow` is the regression test |
| 2 | `NHD_0216HZ.cpp:49` | cppcheck `selfAssignment` | `hi_n = hi_n = (data & 0xF0);` | Harmless typo; the code isn't needed in the port |
| 3 | `NHD_0216HZ.h` | manual review | `#define ENABLE 0x08` collides with the STM32 HAL's `ENABLE` (`FunctionalState`) | Doesn't compile in an STM32Cube project if included before the HAL, and silently redefines `ENABLE` if included after. Removed in the port |
| 4 | `NHD_0216HZ.cpp:7` | manual review | `#include "headers/NHD_0216HZ.h"`, while the handout says to put all files in one directory | Doesn't compile as distributed. Port includes `"NHD_0216HZ.h"` |
| 5 | `NHD_0216HZ.cpp` `init_lcd` | manual review (HD44780 datasheet) | At a cold power-up the controller is in 8-bit mode. `shift_out(0x30)` has no Enable pulse, so it's never latched. The first nibble of `write_cmd(0x20)` then switches to 4-bit mode, and every later command is off by one nibble. The datasheet's reset sequence sends `0x3` three times, then `0x2` | Likely works only when the LCD was already in 4-bit mode (warm reset). Not applicable to the port, which has no nibble interface |
| 6 | `NHD_0216HZ.cpp` `write_4bit` | manual review | The byte that drops Enable (`hi_n & ~ENABLE`) also clears RS, so RS and E change in the same 74HC595 latch. That leaves no RS hold time after E falls (HD44780 needs ≥ 10 ns) | Relies on propagation delays. Not applicable to the port |
| 7 | `song.h` | cppcheck `passedByValue` ×2, `useInitializationList` ×2 | `std::string` parameters copied, members assigned in the constructor body | Only costs a few copies at start-up. File kept unchanged, notes suppressed |
| 8 | `song.h`, `song_def.h` | clang-tidy `google-build-using-namespace` | `using namespace std;` in headers affects every file that includes them | The song headers are included last in `main.cpp` |
| 9 | `song_def.h` | manual review / compiler | Macros with short names (`Do`, `Re`, `Mi`, `No`, `b0`–`b4`, …) | The first build broke on a local variable called `b1`. Include last and avoid those names (noted in `main.cpp`) |
| 10 | `song_def.h:11-12` | clang-tidy `cert-dcl51-cpp` | `So__1`, `Si__1` contain `__`, reserved for the implementation in C++ | Undefined behaviour in principle; works with GCC/Clang. Kept |
| 11 | `song_def.h` | GCC `-Wfloat-conversion` (1,123), clang-tidy `narrowing-conversions` (10) | `double` literals (3.822, 0.18, …) stored in `float` | Values fit in a float, so harmless. Warning silenced for that include only |
| 12 | `song_def.h` | manual review, clang `-Wglobal-constructors` | Non-`const`, non-`static` arrays and 10 `Song` objects (with `std::string`) defined in a header | Including it from two `.cpp` files would be a multiple-definition link error. The arrays occupy about 10 KB of RAM instead of flash, and 10 constructors allocate on the heap before `main()`. It's included from `main.cpp` only |
| 13 | `song_def.h` | unit test | Each `length` (72, 88, …) is typed by hand, with no compile-time check against the array size | `test_songs` checks all 10: they match |
| 14 | `song_def.h` | manual review | Typos: `CANNON_IN_D`, `NOCTRUNE_*`, "Pachebelbel", "Nachtamusik". `NOCTRUNE_IN_C_SHARP_MAJOR` holds the C♯ **minor** nocturne | Cosmetic; kept so the names match the handout |

## Limits

- Static analysis and the host tests can't check what depends on the real hardware: register setup for
  FSMC, I2C, I2S and DMA, interrupt timing, and the codec's analog output.
  The on-board suite `test/embedded/test_hardware` covers the basics, but it hasn't been run yet (it needs the board).
- PlatformIO's clang-tidy 15 can't parse the newer macOS SDK C++ headers, so the course headers were checked by
  clang-tidy through the firmware build (GCC's newlib/libstdc++), not standalone.
