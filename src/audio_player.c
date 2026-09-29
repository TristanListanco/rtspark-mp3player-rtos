/*
 * audio_player.c - Streams the synthesiser (synth.c) to the ES8388 over I2S3.
 *
 * I2S3 plays a circular DMA buffer of 16-bit stereo samples. When the DMA
 * finishes one half of the buffer, that half is refilled from the ISR while
 * the other half is being played (double buffering).
 */
#include "audio_player.h"
#include "main.h"
#include "synth.h"

#define HALF_FRAMES  128        /* stereo frames per half buffer (~3.9 ms) */

static int16_t dma_buf[HALF_FRAMES * 2 /* halves */ * 2 /* L,R */];

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

void audio_init(void)
{
    synth_init(compute_sample_rate());

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
    return synth_sample_rate();
}

void audio_play_note(float freq_hz, float duration_s)
{
    synth_note_on(freq_hz, duration_s);
}

void audio_stop(void)
{
    synth_note_off();
}

/* cppcheck-suppress constParameterPointer ; signature fixed by the HAL's weak callback */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3) {
        synth_render(&dma_buf[0], HALF_FRAMES);
    }
}

/* cppcheck-suppress constParameterPointer ; signature fixed by the HAL's weak callback */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3) {
        synth_render(&dma_buf[HALF_FRAMES * 2], HALF_FRAMES);
    }
}
