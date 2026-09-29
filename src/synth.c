/*
 * synth.c - Wavetable tone synthesiser.
 */
#include <math.h>
#include "synth.h"

#define WAVE_BITS   10
#define WAVE_SIZE   (1u << WAVE_BITS)       /* wavetable entries */
#define AMP_FULL    32768                   /* Q15 envelope level = 1.0 */

static int16_t wavetable[WAVE_SIZE];

static uint32_t sample_rate_hz;
static int32_t  attack_step;
static int32_t  release_step;

/* Set by note on/off, read by the renderer */
static volatile uint32_t phase_inc;         /* frequency as a 32-bit phase step */
static volatile int32_t  gate_samples;      /* samples left before release */
static volatile int32_t  target_amp;        /* envelope target, 0..AMP_FULL */

/* Owned by the renderer */
static uint32_t phase;
static int32_t  env_amp;

static float wave_shape(uint32_t i)
{
    /* Fundamental plus two soft harmonics: brighter than a pure sine, so it
     * carries better on a small speaker, but without the harshness of the
     * PWM square wave the mbed version produced. */
    const float t = 6.28318530718f * (float)i / (float)WAVE_SIZE;
    return sinf(t) + 0.35f * sinf(2.0f * t) + 0.15f * sinf(3.0f * t);
}

void synth_init(uint32_t rate_hz)
{
    float peak = 0.0f;
    for (uint32_t i = 0; i < WAVE_SIZE; i++) {
        const float v = fabsf(wave_shape(i));
        if (v > peak) {
            peak = v;
        }
    }
    for (uint32_t i = 0; i < WAVE_SIZE; i++) {
        wavetable[i] = (int16_t)(wave_shape(i) / peak * (float)SYNTH_PEAK);
    }

    sample_rate_hz = rate_hz;
    attack_step = (int32_t)((float)AMP_FULL / (SYNTH_ATTACK_S * (float)rate_hz)) + 1;
    release_step = (int32_t)((float)AMP_FULL / (SYNTH_RELEASE_S * (float)rate_hz)) + 1;

    phase = 0;
    env_amp = 0;
    phase_inc = 0;
    gate_samples = 0;
    target_amp = 0;
}

uint32_t synth_sample_rate(void)
{
    return sample_rate_hz;
}

void synth_note_on(float freq_hz, float duration_s)
{
    const float nyquist = 0.5f * (float)sample_rate_hz;
    if (freq_hz <= 0.0f || freq_hz >= nyquist || duration_s <= 0.0f) {
        synth_note_off();                       /* rest: no note */
        return;
    }
    phase_inc = (uint32_t)(freq_hz * (4294967296.0f / (float)sample_rate_hz));
    gate_samples = (int32_t)(duration_s * SYNTH_NOTE_GATE * (float)sample_rate_hz);
    target_amp = AMP_FULL;
}

void synth_note_off(void)
{
    gate_samples = 0;
    target_amp = 0;
}

void synth_render(int16_t *stereo, uint32_t frames)
{
    const uint32_t inc = phase_inc;
    int32_t gate = gate_samples;
    int32_t target = target_amp;

    for (uint32_t i = 0; i < frames; i++) {
        if (gate > 0) {
            if (--gate == 0) {
                target = 0;                     /* note time is up: release */
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
        stereo[2 * i]     = (int16_t)s;         /* left  */
        stereo[2 * i + 1] = (int16_t)s;         /* right */
    }

    gate_samples = gate;
    target_amp = target;
}
