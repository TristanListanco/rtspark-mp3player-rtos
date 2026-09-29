/*
 * player_logic.h - Hardware-independent parts of the MP3 player.
 *
 * Everything here is plain C++ (no HAL, no FreeRTOS), so it runs in the
 * unit tests on a PC (test/). main.cpp wires it to the buttons, the timers,
 * the codec and the LCD.
 */
#ifndef PLAYER_LOGIC_H
#define PLAYER_LOGIC_H

#include <stddef.h>
#include <stdint.h>

class Song;     /* course song.h */

/* ---- Course song data ----------------------------------------------------
 * song_def.h stores each note as the PWM period in milliseconds (the mbed
 * version did speaker.period_ms(note)): La = 2.272 ms = 440 Hz, No = 0 = rest.
 * beat is a fraction of a whole note (b0 = 1 ... b3 = 1/8); with the x8 scale
 * `tempo` is the length of a b3 note in seconds. */
const float kTempoScale = 8.0f;

float note_hz(const Song &song, int i);          /* 0 for a rest */
float note_seconds(const Song &song, int i);     /* beat x kTempoScale x tempo */

/* ---- Music ticker --------------------------------------------------------
 * One step of the Ticker callback: which note to start now, and when the
 * ticker should fire again. Past the last note: silence for repeat_gap_s,
 * then the song starts over. */
struct NoteStep {
    float freq_hz;       /* note to start now, 0 = silence */
    float duration_s;    /* its length */
    float next_s;        /* delay until the next ticker callback */
    int   next_index;    /* value of the note index after this step */
};

NoteStep next_note_step(const Song &song, int index, float repeat_gap_s);

/* "name1 name2" with the 16x2-LCD padding spaces removed, e.g.
 * "Turkish March - " + " Mozart" -> "Turkish March - Mozart". Always
 * NUL-terminated (truncated to size - 1); no heap allocation. */
void song_title(const Song &song, char *buf, size_t size);

/* ---- Song selection ------------------------------------------------------
 * B2..B4 held down = song number in binary, B2 = MSB, pressed = 1. */
int song_index_from_buttons(bool msb, bool mid, bool lsb);

/* B1 select / confirm with a confirmation window:
 *   first press      -> SELECTED  (remember the song, window opens)
 *   second press     -> CONFIRMED (play the remembered song)
 *   window times out -> back to normal, nothing changes
 * press() is called from a thread and timeout() from the Timeout ISR, so
 * the caller serialises them (critical section). */
class SongSelector {
public:
    enum Result { SELECTED, CONFIRMED };

    SongSelector() noexcept : confirming_(false), pending_(0) {}

    Result press(int buttons_index);
    void timeout() { confirming_ = false; }

    bool confirming() const { return confirming_; }
    int pending() const { return pending_; }

private:
    volatile bool confirming_;
    volatile int pending_;
};

/* ---- Buttons -------------------------------------------------------------
 * A change of the raw input is accepted after `samples` consecutive equal
 * readings. debounce() returns true once, when a press is accepted. */
struct Debouncer {
    bool pressed;        /* debounced state */
    uint8_t count;
};

bool debounce(Debouncer &d, bool raw_pressed, uint8_t samples);

/* ---- Volume --------------------------------------------------------------- */

/* ADC reading -> 0..100 %, with dead zones below raw_min and above raw_max */
int pot_to_percent(uint32_t raw, uint32_t raw_min, uint32_t raw_max);

/* 2 % hysteresis against ADC noise, but 0 % and 100 % are always reached.
 * shown < 0 means nothing applied yet. */
bool volume_should_update(int shown, int percent);

/* ES8388 DAC attenuation in 0.5 dB steps, linear in dB:
 * 100 % -> 0 (0 dB) ... 1 % -> 118 (-59 dB). 0 % is handled by muting. */
uint8_t volume_to_attenuation(int percent);

#endif /* PLAYER_LOGIC_H */
