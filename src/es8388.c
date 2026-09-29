/*
 * es8388.c - ES8388 codec control over I2C2 (HAL).
 *
 * The register sequence follows the RT-Thread RT-Spark BSP
 * (board/ports/audio/drv_es8388.c, Apache-2.0), reduced to DAC playback.
 */
#include "es8388.h"
#include "main.h"

#define ES8388_I2C_ADDR     (0x10 << 1)     /* CE pin tied to GND */
#define ES8388_I2C_TIMEOUT  20              /* ms */

/* Volume range covered by the potentiometer, in 0.5 dB DAC steps (120 = -60 dB) */
#define ES8388_VOLUME_RANGE 120
#define ES8388_DAC_MUTE_BIT 0x04
#define ES8388_SOFT_RAMP    0x20            /* ramp volume changes (no zipper noise) */

static HAL_StatusTypeDef es_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c2, ES8388_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, ES8388_I2C_TIMEOUT);
}

HAL_StatusTypeDef es8388_init(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c2, ES8388_I2C_ADDR, 3, ES8388_I2C_TIMEOUT) != HAL_OK) {
        return HAL_ERROR;
    }

    static const uint8_t seq[][2] = {
        { ES8388_DACCONTROL3,  ES8388_DAC_MUTE_BIT }, /* mute while configuring */
        { ES8388_CONTROL2,     0x50 },  /* reference / low-power defaults */
        { ES8388_CHIPPOWER,    0x00 },  /* power up all blocks */
        { ES8388_MASTERMODE,   0x00 },  /* I2S slave */
        { ES8388_DACPOWER,     0xC0 },  /* DAC + outputs off during setup */
        { ES8388_CONTROL1,     0x12 },  /* same Fs for ADC/DAC, VMID 500k */
        { ES8388_DACCONTROL1,  0x18 },  /* I2S (Philips) format, 16-bit words */
        { ES8388_DACCONTROL2,  0x02 },  /* single speed, MCLK/LRCK = 256 */
        { ES8388_DACCONTROL16, 0x00 },  /* mixer inputs LIN1/RIN1 (unused) */
        { ES8388_DACCONTROL17, 0x90 },  /* left DAC -> left mixer, 0 dB */
        { ES8388_DACCONTROL20, 0x90 },  /* right DAC -> right mixer, 0 dB */
        { ES8388_DACCONTROL21, 0x80 },  /* DAC and ADC share LRCK */
        { ES8388_DACCONTROL23, 0x00 },  /* VROI = 1.5k */
        { ES8388_DACCONTROL4,  0x00 },  /* DAC digital volume 0 dB */
        { ES8388_DACCONTROL5,  0x00 },
        { ES8388_ADCPOWER,     0xFF },  /* ADC / microphone path powered down */
        { ES8388_DACCONTROL24, 0x1E },  /* LOUT1 0 dB */
        { ES8388_DACCONTROL25, 0x1E },  /* ROUT1 0 dB */
        { ES8388_DACPOWER,     0x3C },  /* DACs + LOUT1/ROUT1/LOUT2/ROUT2 on */
        { ES8388_DACCONTROL3,  ES8388_SOFT_RAMP }, /* unmute, soft volume ramp */
    };

    for (unsigned i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        if (es_write(seq[i][0], seq[i][1]) != HAL_OK) {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef es8388_mute(int mute)
{
    return es_write(ES8388_DACCONTROL3,
                    ES8388_SOFT_RAMP | (mute ? ES8388_DAC_MUTE_BIT : 0));
}

HAL_StatusTypeDef es8388_set_volume(uint8_t percent)
{
    if (percent == 0) {
        return es8388_mute(1);
    }
    if (percent > 100) {
        percent = 100;
    }

    /* Linear in dB: 100 % -> 0 dB ... 1 % -> -59.4 dB (register = 0.5 dB steps) */
    const uint8_t att = (uint8_t)((100 - percent) * ES8388_VOLUME_RANGE / 100);
    HAL_StatusTypeDef st = es_write(ES8388_DACCONTROL4, att);
    if (st == HAL_OK) st = es_write(ES8388_DACCONTROL5, att);
    if (st == HAL_OK) st = es8388_mute(0);
    return st;
}
