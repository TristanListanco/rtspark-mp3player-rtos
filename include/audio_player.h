/*
 * audio_player.h - Tone generator feeding the ES8388 over I2S3 + circular DMA.
 */
#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Prepare the synthesiser and zero the DMA buffer. Does not start I2S DMA. */
void audio_init(void);

/* Start I2S3 circular DMA after the ES8388 DAC path is ready. */
HAL_StatusTypeDef audio_start(void);

/* Stop the I2S DMA stream (used by hardware tests/recovery). */
HAL_StatusTypeDef audio_shutdown(void);

void audio_play_note(float freq_hz, float duration_s);
void audio_stop(void);
uint32_t audio_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PLAYER_H */
