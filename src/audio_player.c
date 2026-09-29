/*
 * audio_player.c - Wavetable tone synthesiser on I2S3 (ES8388 DAC).
 *
 * I2S3 streams a circular DMA buffer of 16-bit stereo samples. When the DMA
 * finishes one half of the buffer, that half is refilled from the ISR while
 * the other half is being played (double buffering).
 *
 * Each note uses a phase accumulator to step through a wavetable, and a
 * short attack/release envelope so notes start and stop without clicks.
 */
#include <math.h>
#include "audio_player.h"
#include "main.h"

#define HALF_FRAMES        128                   /* stereo frames per half buffer (~3.9 ms) */
#define WAVE_BITS          10
#define WAVE_SIZE          (1u << WAVE_BITS)     /* wavetable entries */
#define AMP_FULL           32768                 /* Q15 envelope level = 1.0 */
#define NOTE_GATE          0.90f                 /* fraction of a note that sounds */
#define ATTACK_S           0.003f
#define RELEASE_S          0.012f

static int16_t dma_buf[HALF_FRAMES * 2 /* halves */ * 2 /* L,R */];
static int16_t wavetable[WAVE_SIZE];

static uint32_t sample_rate_hz;
static int32_t  attack_step;
static int32_t  release_step;

/* Written by the Ticker ISR / threads, read by the DMA ISR */
static volatile uint32_t phase_inc;      /* frequency as a 32-bit phase step */
static volatile int32_t  gate_samples;   /* samples left before release */
static volatile int32_t  target_amp;     /* envelope target, 0..AMP_FULL */

/* Owned by the DMA ISR */
static uint32_t phase;
static int32_t  env_amp;

static uint32_t compute_sample_rate(void)
{
    /* I2SCLK = HSE / PLLM * PLLI2SN / PLLI2SR
     * Fs     = I2SCLK / (256 * (2 * I2SDIV + ODD))   (MCLK output enabled) */
    const uint32_t pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
    const uint32_t plli2sn = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SN) >> RCC_PLLI2SCFGR_PLLI2SN_Pos;
    const uint32_t plli2sr = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SR) >> RCC_PLLI2SCFGR_PLLI2SR_Pos;
    const uint32_t i2sdiv = SPI3->I2SPR & SPI_I2SPR_I2SDIV;
    const uint32_t odd = (SPI3->I2SPR & SPI_I2SPR_ODD) ? 1u : 0u;

    const float i2sclk = (float)HSE_VALUE / (float)pllm * (float)plli2sn / (float)plli2sr;
    return (uint32_t)(i2sclk / (256.0f * (float)(2u * i2sdiv + odd)) + 0.5f);
}

static float wave_shape(uint32_t i)
{
    /* Fundamental plus two soft harmonics: brighter than a pure sine, so it
     * carries better on a small speaker, but without the harshness of the
     * PWM square wave. */
    const float t = 6.28318530718f * (float)i / (float)WAVE_SIZE;
    return sinf(t) + 0.35f * sinf(2.0f * t) + 0.15f * sinf(3.0f * t);
}

static void build_wavetable(void)
{
    float peak = 0.0f;
    for (uint32_t i = 0; i < WAVE_SIZE; i++) {
        const float v = fabsf(wave_shape(i));
        if (v > peak) {
            peak = v;
        }
    }
    for (uint32_t i = 0; i < WAVE_SIZE; i++) {
        wavetable[i] = (int16_t)(wave_shape(i) / peak * 29000.0f);   /* ~ -1 dBFS */
    }
}

void audio_init(void)
{
    build_wavetable();
    sample_rate_hz = compute_sample_rate();
    attack_step = (int32_t)(AMP_FULL / (ATTACK_S * (float)sample_rate_hz)) + 1;
    release_step = (int32_t)(AMP_FULL / (RELEASE_S * (float)sample_rate_hz)) + 1;

    for (uint32_t i = 0; i < sizeof dma_buf / sizeof dma_buf[0]; i++) {
        dma_buf[i] = 0;
    }
    /* Size is in 16-bit words for a 16-bit I2S data format */
    if (HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t *)dma_buf,
                             sizeof dma_buf / sizeof dma_buf[0]) != HAL_OK) {
        Error_Handler();
    }
}

uint32_t audio_sample_rate(void)
{
    return sample_rate_hz;
}

void audio_play_note(float freq_hz, float duration_s)
{
    if (freq_hz <= 0.0f || duration_s <= 0.0f) {   /* rest: no note */
        audio_stop();
        return;
    }
    phase_inc = (uint32_t)(freq_hz * (4294967296.0f / (float)sample_rate_hz));
    gate_samples = (int32_t)(duration_s * NOTE_GATE * (float)sample_rate_hz);
    target_amp = AMP_FULL;
}

void audio_stop(void)
{
    gate_samples = 0;
    target_amp = 0;
}

/* Called from the DMA ISR: synthesise `frames` stereo frames into dst. */
static void render(int16_t *dst, uint32_t frames)
{
    const uint32_t inc = phase_inc;
    int32_t gate = gate_samples;
    int32_t target = target_amp;

    for (uint32_t i = 0; i < frames; i++) {
        if (gate > 0) {
            if (--gate == 0) {
                target = 0;                 /* note time is up: release */
            }
        }
        if (env_amp < target) {
            env_amp += attack_step;
            if (env_amp > target) env_amp = target;
        } else if (env_amp > target) {
            env_amp -= release_step;
            if (env_amp < target) env_amp = target;
        }

        const int32_t s = ((int32_t)wavetable[phase >> (32 - WAVE_BITS)] * env_amp) >> 15;
        phase += inc;
        dst[2 * i]     = (int16_t)s;        /* left  */
        dst[2 * i + 1] = (int16_t)s;        /* right */
    }

    gate_samples = gate;
    target_amp = target;
}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3) {
        render(&dma_buf[0], HALF_FRAMES);
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3) {
        render(&dma_buf[HALF_FRAMES * 2], HALF_FRAMES);
    }
}
