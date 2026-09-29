/*
 * player_logic.cpp - Hardware-independent player logic (see player_logic.h).
 */
#include <stdio.h>
#include "player_logic.h"
#include "song.h"

/* Text of s without leading / trailing spaces, as pointer + length */
static const char *trim(const string &s, int *len)
{
    const char *p = s.c_str();
    size_t n = s.size();
    while (n > 0 && *p == ' ') { p++; n--; }
    while (n > 0 && p[n - 1] == ' ') { n--; }
    *len = (int)n;
    return p;
}

float note_hz(const Song &song, int i)
{
    const float period_ms = song.note[i];
    return period_ms > 0.0f ? 1000.0f / period_ms : 0.0f;
}

float note_seconds(const Song &song, int i)
{
    return song.beat[i] * kTempoScale * song.tempo;
}

NoteStep next_note_step(const Song &song, int index, float repeat_gap_s)
{
    NoteStep step;
    if (index < 0 || index >= song.length) {
        step.freq_hz = 0.0f;
        step.duration_s = 0.0f;
        step.next_s = repeat_gap_s;
        step.next_index = 0;
        return step;
    }
    step.freq_hz = note_hz(song, index);
    step.duration_s = note_seconds(song, index);
    step.next_s = step.duration_s;
    step.next_index = index + 1;
    return step;
}

void song_title(const Song &song, char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }
    int n1, n2;
    const char *t1 = trim(song.name1, &n1);
    const char *t2 = trim(song.name2, &n2);
    if (n1 > 0 && n2 > 0) {
        (void)snprintf(buf, size, "%.*s %.*s", n1, t1, n2, t2);   /* truncation is fine */
    } else {
        (void)snprintf(buf, size, "%.*s%.*s", n1, t1, n2, t2);
    }
}

int song_index_from_buttons(bool msb, bool mid, bool lsb)
{
    return (msb ? 4 : 0) | (mid ? 2 : 0) | (lsb ? 1 : 0);
}

SongSelector::Result SongSelector::press(int buttons_index)
{
    if (!confirming_) {
        pending_ = buttons_index;
        confirming_ = true;
        return SELECTED;
    }
    confirming_ = false;
    return CONFIRMED;
}

bool debounce(Debouncer &d, bool raw_pressed, uint8_t samples)
{
    if (raw_pressed == d.pressed) {
        d.count = 0;
        return false;
    }
    if (++d.count < samples) {
        return false;
    }
    d.count = 0;
    d.pressed = raw_pressed;
    return raw_pressed;
}

int pot_to_percent(uint32_t raw, uint32_t raw_min, uint32_t raw_max)
{
    if (raw <= raw_min) return 0;
    if (raw >= raw_max) return 100;
    return (int)((raw - raw_min) * 100u / (raw_max - raw_min));
}

bool volume_should_update(int shown, int percent)
{
    if (shown < 0) {
        return true;
    }
    const int diff = percent > shown ? percent - shown : shown - percent;
    return diff >= 2 || ((percent == 0 || percent == 100) && percent != shown);
}

uint8_t volume_to_attenuation(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (uint8_t)((100 - percent) * 120 / 100);
}
