/*
 * es8388.c - ES8388 codec control over I2C2 (HAL).
 *
 * Playback sequencing follows the RT-Thread RT-Spark BSP ES8388 driver:
 * configure the codec while muted, route the DAC into the output mixers,
 * start the DAC state machine, power the DAC/output stages, then unmute.
 */
#include "es8388.h"
#include "main.h"

#define ES8388_I2C_ADDR       (0x10u << 1)  /* HAL expects the 7-bit address shifted left */
#define ES8388_I2C_TIMEOUT_MS 20u

#define ES8388_MAX_ATTENUATION 192u         /* -96 dB */
#define ES8388_DAC_MUTE_BIT    0x04u

static HAL_StatusTypeDef es_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c2, ES8388_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1u, ES8388_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef es_read(uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return HAL_ERROR;
    }
    return HAL_I2C_Mem_Read(&hi2c2, ES8388_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            val, 1u, ES8388_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef es_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0u;
    HAL_StatusTypeDef st = es_read(reg, &current);
    if (st != HAL_OK) {
        return st;
    }
    current = (uint8_t)((current & (uint8_t)~mask) | (value & mask));
    return es_write(reg, current);
}

HAL_StatusTypeDef es8388_init(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c2, ES8388_I2C_ADDR, 3u,
                              ES8388_I2C_TIMEOUT_MS) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Keep this sequence deliberately close to the RT-Spark BSP.  In
     * particular, DACCONTROL17/20 must be 0x9C for normal DAC playback. */
    static const struct { uint8_t reg; uint8_t val; } seq[] = {
        { ES8388_DACCONTROL3,  ES8388_DAC_MUTE_BIT }, /* mute while configuring */
        { ES8388_CONTROL2,     0x50u },
        { ES8388_CHIPPOWER,    0x00u },
        { ES8388_MASTERMODE,   0x00u },               /* codec is I2S slave */
        { ES8388_DACPOWER,     0xC0u },               /* outputs off during setup */
        { ES8388_CONTROL1,     0x12u },
        { ES8388_DACCONTROL1,  0x18u },               /* Philips I2S, 16-bit */
        { ES8388_DACCONTROL2,  0x02u },               /* single speed, 256 x Fs */
        { ES8388_DACCONTROL16, 0x00u },
        { ES8388_DACCONTROL17, 0x9Cu },               /* left DAC -> left mixer */
        { ES8388_DACCONTROL20, 0x9Cu },               /* right DAC -> right mixer */
        { ES8388_DACCONTROL21, 0x80u },
        { ES8388_DACCONTROL23, 0x00u },
        { ES8388_DACCONTROL4,  0x00u },               /* DAC digital volume 0 dB */
        { ES8388_DACCONTROL5,  0x00u },
        { ES8388_ADCPOWER,     0xFFu },                /* ADC path unused */
        { ES8388_DACCONTROL24, 0x1Eu },               /* LOUT1 volume */
        { ES8388_DACCONTROL25, 0x1Eu },               /* ROUT1 volume */
    };

    for (uint32_t i = 0u; i < (uint32_t)(sizeof seq / sizeof seq[0]); ++i) {
        if (es_write(seq[i].reg, seq[i].val) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef es8388_start_dac(void)
{
    uint8_t old_state = 0u;
    uint8_t new_state = 0u;

    /* RT-Spark's driver selects DAC playback through DACCONTROL21 and, when
     * that state changes, restarts the ES8388 state machine. */
    if (es_read(ES8388_DACCONTROL21, &old_state) != HAL_OK) {
        return HAL_ERROR;
    }
    if (es_write(ES8388_DACCONTROL21, 0x80u) != HAL_OK) {
        return HAL_ERROR;
    }
    if (es_read(ES8388_DACCONTROL21, &new_state) != HAL_OK) {
        return HAL_ERROR;
    }

    if (old_state != new_state) {
        if (es_write(ES8388_CHIPPOWER, 0xF0u) != HAL_OK ||
            es_write(ES8388_CHIPPOWER, 0x00u) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    /* Power DAC + output stages only after routing/format configuration. */
    if (es_write(ES8388_DACPOWER, 0x3Cu) != HAL_OK) {
        return HAL_ERROR;
    }

    return es8388_mute(0);
}

HAL_StatusTypeDef es8388_mute(int mute)
{
    return es_update_bits(ES8388_DACCONTROL3, ES8388_DAC_MUTE_BIT,
                          mute ? ES8388_DAC_MUTE_BIT : 0u);
}

HAL_StatusTypeDef es8388_set_attenuation(uint8_t half_db_steps)
{
    if (half_db_steps > ES8388_MAX_ATTENUATION) {
        half_db_steps = ES8388_MAX_ATTENUATION;
    }

    HAL_StatusTypeDef st = es_write(ES8388_DACCONTROL4, half_db_steps);
    if (st == HAL_OK) {
        st = es_write(ES8388_DACCONTROL5, half_db_steps);
    }
    return st;
}

HAL_StatusTypeDef es8388_verify_playback_config(void)
{
    static const struct { uint8_t reg; uint8_t expected; } expected[] = {
        { ES8388_MASTERMODE,   0x00u },
        { ES8388_DACCONTROL1,  0x18u },
        { ES8388_DACCONTROL2,  0x02u },
        { ES8388_DACCONTROL17, 0x9Cu },
        { ES8388_DACCONTROL20, 0x9Cu },
        { ES8388_DACCONTROL21, 0x80u },
        { ES8388_DACPOWER,     0x3Cu },
    };

    for (uint32_t i = 0u; i < (uint32_t)(sizeof expected / sizeof expected[0]); ++i) {
        uint8_t actual = 0u;
        if (es_read(expected[i].reg, &actual) != HAL_OK || actual != expected[i].expected) {
            return HAL_ERROR;
        }
    }

    uint8_t mute = 0u;
    if (es_read(ES8388_DACCONTROL3, &mute) != HAL_OK ||
        (mute & ES8388_DAC_MUTE_BIT) != 0u) {
        return HAL_ERROR;
    }
    return HAL_OK;
}
