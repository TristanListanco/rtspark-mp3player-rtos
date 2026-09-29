/*
 * Unit tests for the tone synthesiser (src/synth.c), at the firmware's real
 * sample rate (I2SCLK 192 MHz / (256 x 23) = 32609 Hz).
 *   pio test -e native -f native/test_synth
 */
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <unity.h>

#include "../../../src/synth.c"             /* unit under test */

#include "song.h"
#include "song_def.h"                       /* note periods Do, Re, ... */

static const uint32_t kFs = 32609;
static const uint32_t kBlock = 128;         /* frames per DMA half buffer */

/* Renders `seconds` of audio in DMA-sized blocks; returns the left channel
 * and checks that L == R on the way. */
static std::vector<int16_t> render(float seconds)
{
    const uint32_t frames = (uint32_t)(seconds * kFs);
    std::vector<int16_t> mono;
    int16_t block[kBlock * 2];
    for (uint32_t done = 0; done < frames; done += kBlock) {
        synth_render(block, kBlock);
        for (uint32_t i = 0; i < kBlock; i++) {
            TEST_ASSERT_EQUAL_INT16(block[2 * i], block[2 * i + 1]);
            mono.push_back(block[2 * i]);
        }
    }
    return mono;
}

static int peak(const std::vector<int16_t> &x, size_t from, size_t to)
{
    int m = 0;
    for (size_t i = from; i < to && i < x.size(); i++) {
        if (abs(x[i]) > m) m = abs(x[i]);
    }
    return m;
}

/* Frequency with the most energy near `expected` (Hann-windowed DFT scan,
 * 0.05 % steps), measured over n samples starting at `from`. */
static double dominant_hz(const std::vector<int16_t> &x, size_t from, size_t n, double expected)
{
    double best_f = 0.0, best_m = -1.0;
    for (int k = -100; k <= 100; k++) {
        const double f = expected * (1.0 + k * 0.0005);
        const double w = 2.0 * M_PI * f / kFs;
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < n; i++) {
            const double win = 0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1));
            re += x[from + i] * win * cos(w * i);
            im -= x[from + i] * win * sin(w * i);
        }
        const double m = re * re + im * im;
        if (m > best_m) { best_m = m; best_f = f; }
    }
    return best_f;
}

/* Largest second difference: a discontinuity (click) shows up as a spike far
 * above what the smooth waveform itself produces. */
static int max_curvature(const std::vector<int16_t> &x, size_t from, size_t to)
{
    int m = 0;
    for (size_t i = from + 2; i < to && i < x.size(); i++) {
        const int d2 = abs(x[i] - 2 * x[i - 1] + x[i - 2]);
        if (d2 > m) m = d2;
    }
    return m;
}

void setUp(void)
{
    synth_init(kFs);
}

void tearDown(void) {}

static void test_silent_until_a_note_is_played(void)
{
    const std::vector<int16_t> x = render(0.05f);
    TEST_ASSERT_EQUAL_INT(0, peak(x, 0, x.size()));
    TEST_ASSERT_EQUAL_UINT32(kFs, synth_sample_rate());
}

static void test_pitch_of_every_note_in_song_def(void)
{
    /* every distinct period used by the course songs */
    const float periods[] = { So__1, Si__1, Do, DoD, Re, ReD, Mi, Fa, FaD, So, SoD, La, LaD, Si,
                              Do_2, DoD_2, Re_2, ReD_2, Mi_2, Fa_2, FaD_2, So_2, SoD_2, La_2,
                              LaD_2, Si_2, Do_3, DoD_3, Re_3 };
    for (unsigned i = 0; i < sizeof periods / sizeof periods[0]; i++) {
        synth_init(kFs);
        const double hz = 1000.0 / periods[i];
        synth_note_on((float)hz, 1.0f);
        const std::vector<int16_t> x = render(0.3f);
        const double got = dominant_hz(x, (size_t)(0.02 * kFs), (size_t)(0.2 * kFs), hz);
        const double cents = 1200.0 * log2(got / hz);
        char msg[64];
        snprintf(msg, sizeof msg, "period %.3f ms: %.1f Hz, got %.1f Hz", periods[i], hz, got);
        TEST_ASSERT_TRUE_MESSAGE(fabs(cents) < 5.0, msg);
    }
}

static void test_level_is_full_scale_minus_headroom(void)
{
    synth_note_on(440.0f, 1.0f);
    const std::vector<int16_t> x = render(0.2f);
    const int p = peak(x, 0, x.size());
    TEST_ASSERT_INT_WITHIN(200, SYNTH_PEAK, p);           /* reaches ~ -1 dBFS */
    TEST_ASSERT_TRUE(p <= SYNTH_PEAK);                    /* never clips */
}

static void test_rest_and_invalid_notes_are_silent(void)
{
    synth_note_on(0.0f, 0.5f);                            /* rest (No) */
    std::vector<int16_t> x = render(0.1f);
    TEST_ASSERT_EQUAL_INT(0, peak(x, 0, x.size()));

    synth_note_on(20000.0f, 0.5f);                        /* above Nyquist */
    x = render(0.1f);
    TEST_ASSERT_EQUAL_INT(0, peak(x, 0, x.size()));

    synth_note_on(440.0f, -1.0f);                         /* no duration */
    x = render(0.1f);
    TEST_ASSERT_EQUAL_INT(0, peak(x, 0, x.size()));
}

static void test_note_sounds_for_90_percent_then_releases(void)
{
    const float dur = 0.2f;
    synth_note_on(523.25f, dur);
    const std::vector<int16_t> x = render(0.3f);
    const size_t gate = (size_t)(dur * SYNTH_NOTE_GATE * kFs);         /* 180 ms */
    const size_t release = (size_t)(SYNTH_RELEASE_S * kFs) + 2;

    TEST_ASSERT_TRUE(peak(x, gate - 400, gate) > SYNTH_PEAK * 9 / 10);  /* still loud */
    TEST_ASSERT_EQUAL_INT(0, peak(x, gate + release, x.size()));       /* then silent */
}

static void test_note_off_fades_within_release_time(void)
{
    synth_note_on(440.0f, 2.0f);
    render(0.1f);
    synth_note_off();
    const std::vector<int16_t> x = render(0.05f);
    const size_t release = (size_t)(SYNTH_RELEASE_S * kFs) + 2;
    TEST_ASSERT_EQUAL_INT(0, peak(x, release, x.size()));
}

static void test_attack_and_release_have_no_clicks(void)
{
    /* Highest note in the songs (Re_3 = 1172 Hz) has the steepest waveform.
     * Its steady-state curvature is the reference; start and end of the note
     * may add only the small envelope slope change. */
    const float hz = 1000.0f / Re_3;
    synth_note_on(hz, 0.2f);
    const std::vector<int16_t> x = render(0.3f);
    const size_t gate = (size_t)(0.2f * SYNTH_NOTE_GATE * kFs);
    const int steady = max_curvature(x, (size_t)(0.05 * kFs), (size_t)(0.15 * kFs));
    const int onset = max_curvature(x, 0, (size_t)(0.01 * kFs));
    const int offset = max_curvature(x, gate - 50, gate + (size_t)(0.02 * kFs));
    TEST_ASSERT_TRUE(onset <= steady * 105 / 100);
    TEST_ASSERT_TRUE(offset <= steady * 105 / 100);
}

static void test_back_to_back_notes_have_no_clicks(void)
{
    /* The ticker starts the next note while the previous one may still be
     * releasing; the envelope continues from its current level. */
    synth_note_on(440.0f, 0.1f);
    std::vector<int16_t> x = render(0.095f);            /* change mid-release */
    synth_note_on(659.25f, 0.1f);
    const std::vector<int16_t> y = render(0.1f);
    x.insert(x.end(), y.begin(), y.end());

    synth_init(kFs);
    synth_note_on(659.25f, 1.0f);
    const std::vector<int16_t> ref = render(0.2f);
    const int steady = max_curvature(ref, (size_t)(0.05 * kFs), ref.size());

    TEST_ASSERT_TRUE(max_curvature(x, 0, x.size()) <= steady * 105 / 100);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_silent_until_a_note_is_played);
    RUN_TEST(test_pitch_of_every_note_in_song_def);
    RUN_TEST(test_level_is_full_scale_minus_headroom);
    RUN_TEST(test_rest_and_invalid_notes_are_silent);
    RUN_TEST(test_note_sounds_for_90_percent_then_releases);
    RUN_TEST(test_note_off_fades_within_release_time);
    RUN_TEST(test_attack_and_release_have_no_clicks);
    RUN_TEST(test_back_to_back_notes_have_no_clicks);
    return UNITY_END();
}
