/*
 * synth.h - Wavetable tone synthesiser (no hardware dependencies).
 *
 * Produces 16-bit stereo PCM for one note at a time: a phase accumulator
 * steps through a wavetable, and a short attack/release envelope keeps note
 * starts and ends free of clicks. audio_player.c feeds the output to the
 * I2S DMA buffer; the unit tests call it directly.
 *
 * Concurrency: synth_note_on / synth_note_off may be called from an ISR or
 * a thread; synth_render is called from the DMA ISR. They must not preempt
 * each other mid-call (same interrupt priority, or a critical section).
 */
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTH_PEAK        29000     /* sample peak at full envelope (~ -1 dBFS) */
#define SYNTH_NOTE_GATE   0.90f     /* fraction of a note's length that sounds */
#define SYNTH_ATTACK_S    0.003f
#define SYNTH_RELEASE_S   0.012f

/* Builds the wavetable and envelope steps for the given sample rate, and
 * starts silent. */
void synth_init(uint32_t sample_rate_hz);

uint32_t synth_sample_rate(void);

/* Starts a note of freq_hz for duration_s seconds (sounding for
 * SYNTH_NOTE_GATE of it). freq_hz <= 0, freq_hz >= Fs/2 or duration_s <= 0
 * is treated as a rest (silence). */
void synth_note_on(float freq_hz, float duration_s);

/* Fades out to silence. */
void synth_note_off(void);

/* Writes `frames` stereo frames (L, R interleaved; both channels equal). */
void synth_render(int16_t *stereo, uint32_t frames);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_H */
