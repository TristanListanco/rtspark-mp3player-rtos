/*
 * es8388.h - Everest ES8388 audio codec (RT-Spark on-board, I2C2 address 0x10).
 *
 * The codec runs as an I2S slave: the STM32 I2S3 peripheral supplies MCLK
 * (256 x Fs), BCLK and LRCK. Only the DAC -> LOUT1/ROUT1 (headphone jack)
 * path is used.
 */
#ifndef ES8388_H
#define ES8388_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register map (subset) */
#define ES8388_CONTROL1      0x00
#define ES8388_CONTROL2      0x01
#define ES8388_CHIPPOWER     0x02
#define ES8388_ADCPOWER      0x03
#define ES8388_DACPOWER      0x04
#define ES8388_MASTERMODE    0x08
#define ES8388_DACCONTROL1   0x17
#define ES8388_DACCONTROL2   0x18
#define ES8388_DACCONTROL3   0x19
#define ES8388_DACCONTROL4   0x1A   /* left DAC digital volume, 0.5 dB/step */
#define ES8388_DACCONTROL5   0x1B   /* right DAC digital volume */
#define ES8388_DACCONTROL16  0x26
#define ES8388_DACCONTROL17  0x27
#define ES8388_DACCONTROL20  0x2A
#define ES8388_DACCONTROL21  0x2B
#define ES8388_DACCONTROL23  0x2D
#define ES8388_DACCONTROL24  0x2E   /* LOUT1 volume */
#define ES8388_DACCONTROL25  0x2F   /* ROUT1 volume */

/* Configures the codec for 16-bit I2S playback and powers up the DAC and the
 * headphone outputs. Returns HAL_OK, or HAL_ERROR if the chip doesn't answer. */
HAL_StatusTypeDef es8388_init(void);

/* Output volume 0..100 %. 100 % = 0 dB, 1 % = -59.4 dB, 0 % = muted. */
HAL_StatusTypeDef es8388_set_volume(uint8_t percent);

HAL_StatusTypeDef es8388_mute(int mute);

#ifdef __cplusplus
}
#endif

#endif /* ES8388_H */
