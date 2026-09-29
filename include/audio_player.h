/*
 * audio_player.h - Tone generator feeding the ES8388 over I2S3 + circular DMA.
 *
 * The mbed version of this lab plays a note by setting the PWM period of a
 * speaker pin. The ES8388 needs PCM samples instead, so a small synthesiser
 * fills the DMA buffer with a waveform at the current note frequency. The
 * music Ticker sets the note; this module turns it into sound.
 */
#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts I2S3 circular DMA (outputs silence until a note is played).
 * Requires MX_DMA_Init() and MX_I2S3_Init() to have run. */
void audio_init(void);

/* Plays a note of freq_hz for about duration_s seconds (with a short gap at
 * the end so repeated notes are distinguishable). freq_hz <= 0 is a rest.
 * Safe to call from the Ticker ISR. */
void audio_play_note(float freq_hz, float duration_s);

/* Fades the output to silence. Safe to call from threads and ISRs. */
void audio_stop(void);

/* Actual I2S sample rate in Hz (derived from the PLLI2S/I2SPR settings). */
uint32_t audio_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PLAYER_H */
