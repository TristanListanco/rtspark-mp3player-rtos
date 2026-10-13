/*
 * es8388.h - Everest ES8388 audio codec (RT-Spark on-board, I2C address 0x10).
 *
 * The codec runs as an I2S slave: STM32 I2S3 supplies MCLK, BCLK and LRCK.
 * This project uses the DAC -> LOUT1/ROUT1 playback path.
 */
#ifndef ES8388_H
#define ES8388_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8388_CONTROL1      0x00
#define ES8388_CONTROL2      0x01
#define ES8388_CHIPPOWER     0x02
#define ES8388_ADCPOWER      0x03
#define ES8388_DACPOWER      0x04
#define ES8388_MASTERMODE    0x08
#define ES8388_DACCONTROL1   0x17
#define ES8388_DACCONTROL2   0x18
#define ES8388_DACCONTROL3   0x19
#define ES8388_DACCONTROL4   0x1A
#define ES8388_DACCONTROL5   0x1B
#define ES8388_DACCONTROL16  0x26
#define ES8388_DACCONTROL17  0x27
#define ES8388_DACCONTROL20  0x2A
#define ES8388_DACCONTROL21  0x2B
#define ES8388_DACCONTROL23  0x2D
#define ES8388_DACCONTROL24  0x2E
#define ES8388_DACCONTROL25  0x2F

/* Configure the codec while the playback path is muted/powered safely. */
HAL_StatusTypeDef es8388_init(void);

/* Start the RT-Spark DAC playback state machine, power the output path and
 * unmute it. Call this after I2S3 has been configured and before DMA starts. */
HAL_StatusTypeDef es8388_start_dac(void);

/* Read back critical playback registers. Useful as a boot-time sanity check. */
HAL_StatusTypeDef es8388_verify_playback_config(void);

/* DAC digital attenuation in 0.5 dB steps (0 = 0 dB ... 192 = -96 dB). */
HAL_StatusTypeDef es8388_set_attenuation(uint8_t half_db_steps);

/* Mute/unmute without destroying the other DACCONTROL3 bits. */
HAL_StatusTypeDef es8388_mute(int mute);

#ifdef __cplusplus
}
#endif

#endif /* ES8388_H */
