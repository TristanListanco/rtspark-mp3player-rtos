/*
 * Unit tests for player_logic.cpp: note conversion, the music ticker
 * sequence, song selection, button debouncing and volume mapping.
 *   pio test -e native -f native/test_player_logic
 */
#include <string.h>
#include <unity.h>

#include "../../../src/player_logic.cpp"   /* unit under test */

/* A 3-note test song: A4 (La = 2.272 ms), rest, C4 (Do = 3.822 ms) */
static float t_note[] = { 2.272f, 0.0f, 3.822f };
static float t_beat[] = { 0.125f, 0.25f, 1.0f };
static Song test_song(string("Test Song -  "), string(" Unit"), t_note, t_beat, 0.2f, 3);

void setUp(void) {}
void tearDown(void) {}

/* ---- note conversion ---------------------------------------------------- */

static void test_note_hz_converts_period_in_ms(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 440.14f, note_hz(test_song, 0));   /* La */
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 261.64f, note_hz(test_song, 2));   /* Do */
}

static void test_note_hz_rest_is_zero(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, note_hz(test_song, 1));
}

static void test_note_seconds_is_beat_times_8_times_tempo(void)
{
    /* b3 (1/8) lasts exactly `tempo` seconds */
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, note_seconds(test_song, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.4f, note_seconds(test_song, 1));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.6f, note_seconds(test_song, 2));
}

/* ---- music ticker sequence ---------------------------------------------- */

static void test_next_note_step_walks_through_the_song(void)
{
    NoteStep s = next_note_step(test_song, 0, 1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 440.14f, s.freq_hz);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, s.duration_s);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, s.duration_s, s.next_s);   /* next note when this ends */
    TEST_ASSERT_EQUAL_INT(1, s.next_index);

    s = next_note_step(test_song, s.next_index, 1.5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s.freq_hz);                   /* rest: silent ... */
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.4f, s.next_s);            /* ... but keeps its time */
    TEST_ASSERT_EQUAL_INT(2, s.next_index);

    s = next_note_step(test_song, s.next_index, 1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 261.64f, s.freq_hz);
    TEST_ASSERT_EQUAL_INT(3, s.next_index);
}

static void test_next_note_step_repeats_after_a_gap(void)
{
    const NoteStep s = next_note_step(test_song, 3, 1.5f);      /* past the last note */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s.freq_hz);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, s.next_s);
    TEST_ASSERT_EQUAL_INT(0, s.next_index);                     /* start over */
}

static void test_next_note_step_rejects_bad_index(void)
{
    const NoteStep s = next_note_step(test_song, -4, 1.5f);
    TEST_ASSERT_EQUAL_INT(0, s.next_index);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s.freq_hz);
}

/* ---- song title ----------------------------------------------------------- */

static void test_song_title_removes_lcd_padding(void)
{
    char buf[40];
    song_title(test_song, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Test Song - Unit", buf);
}

static void test_song_title_truncates_safely(void)
{
    char buf[8];
    memset(buf, 'X', sizeof buf);
    song_title(test_song, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Test So", buf);                   /* 7 chars + NUL */
}

static void test_song_title_handles_empty_parts_and_null(void)
{
    float n[] = { 0.0f };
    float b[] = { 1.0f };
    Song only_first(string("Solo"), string("   "), n, b, 0.1f, 1);
    char buf[16];
    song_title(only_first, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Solo", buf);

    song_title(only_first, NULL, 16);                           /* must not crash */
    char one = 'X';
    song_title(only_first, &one, 0);                            /* size 0: untouched */
    TEST_ASSERT_EQUAL_CHAR('X', one);
}

/* ---- song selection ------------------------------------------------------- */

static void test_buttons_give_song_number_in_binary(void)
{
    /* nothing pressed = first song; B2 is the most significant bit */
    for (int i = 0; i < 8; i++) {
        const bool b2 = (i & 4) != 0, b3 = (i & 2) != 0, b4 = (i & 1) != 0;
        TEST_ASSERT_EQUAL_INT(i, song_index_from_buttons(b2, b3, b4));
    }
    TEST_ASSERT_EQUAL_INT(0, song_index_from_buttons(false, false, false));
    TEST_ASSERT_EQUAL_INT(5, song_index_from_buttons(true, false, true));
}

static void test_selector_select_then_confirm(void)
{
    SongSelector sel;
    TEST_ASSERT_FALSE(sel.confirming());

    TEST_ASSERT_EQUAL_INT(SongSelector::SELECTED, sel.press(5));
    TEST_ASSERT_TRUE(sel.confirming());
    TEST_ASSERT_EQUAL_INT(5, sel.pending());

    /* confirm: the buttons held at the second press don't matter */
    TEST_ASSERT_EQUAL_INT(SongSelector::CONFIRMED, sel.press(0));
    TEST_ASSERT_FALSE(sel.confirming());
    TEST_ASSERT_EQUAL_INT(5, sel.pending());
}

static void test_selector_timeout_cancels(void)
{
    SongSelector sel;
    sel.press(3);
    sel.timeout();                                              /* 5 s passed */
    TEST_ASSERT_FALSE(sel.confirming());

    /* the next B1 press starts a new selection instead of confirming */
    TEST_ASSERT_EQUAL_INT(SongSelector::SELECTED, sel.press(6));
    TEST_ASSERT_EQUAL_INT(6, sel.pending());
}

static void test_selector_timeout_when_idle_is_harmless(void)
{
    SongSelector sel;
    sel.timeout();
    TEST_ASSERT_FALSE(sel.confirming());
    TEST_ASSERT_EQUAL_INT(SongSelector::SELECTED, sel.press(1));
}

/* ---- debouncing ----------------------------------------------------------- */

static void test_debounce_accepts_press_after_n_samples_once(void)
{
    Debouncer d = { false, 0 };
    TEST_ASSERT_FALSE(debounce(d, true, 3));
    TEST_ASSERT_FALSE(debounce(d, true, 3));
    TEST_ASSERT_TRUE(debounce(d, true, 3));                     /* 3rd equal sample */
    TEST_ASSERT_TRUE(d.pressed);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE(debounce(d, true, 3));                /* holding: no repeat */
    }
}

static void test_debounce_ignores_bounces(void)
{
    Debouncer d = { false, 0 };
    const bool bouncy[] = { true, false, true, true, false, true, false };
    for (unsigned i = 0; i < sizeof bouncy / sizeof bouncy[0]; i++) {
        TEST_ASSERT_FALSE(debounce(d, bouncy[i], 3));
    }
    TEST_ASSERT_FALSE(d.pressed);
}

static void test_debounce_release_then_press_again(void)
{
    Debouncer d = { false, 0 };
    for (int i = 0; i < 3; i++) debounce(d, true, 3);
    TEST_ASSERT_TRUE(d.pressed);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_FALSE(debounce(d, false, 3));               /* release: no event */
    }
    TEST_ASSERT_FALSE(d.pressed);
    debounce(d, true, 3);
    debounce(d, true, 3);
    TEST_ASSERT_TRUE(debounce(d, true, 3));                     /* second press */
}

/* ---- volume --------------------------------------------------------------- */

static void test_pot_to_percent_range_and_dead_zones(void)
{
    TEST_ASSERT_EQUAL_INT(0, pot_to_percent(0, 80, 4000));
    TEST_ASSERT_EQUAL_INT(0, pot_to_percent(80, 80, 4000));
    TEST_ASSERT_EQUAL_INT(50, pot_to_percent(2040, 80, 4000));
    TEST_ASSERT_EQUAL_INT(100, pot_to_percent(4000, 80, 4000));
    TEST_ASSERT_EQUAL_INT(100, pot_to_percent(4095, 80, 4000));
}

static void test_pot_to_percent_is_monotonic(void)
{
    int last = 0;
    for (uint32_t raw = 0; raw <= 4095; raw++) {
        const int p = pot_to_percent(raw, 80, 4000);
        TEST_ASSERT_TRUE(p >= last);
        TEST_ASSERT_TRUE(p >= 0 && p <= 100);
        last = p;
    }
}

static void test_volume_hysteresis(void)
{
    TEST_ASSERT_TRUE(volume_should_update(-1, 37));             /* first reading */
    TEST_ASSERT_FALSE(volume_should_update(50, 50));
    TEST_ASSERT_FALSE(volume_should_update(50, 51));            /* ADC noise */
    TEST_ASSERT_FALSE(volume_should_update(50, 49));
    TEST_ASSERT_TRUE(volume_should_update(50, 52));
    TEST_ASSERT_TRUE(volume_should_update(50, 48));
    TEST_ASSERT_TRUE(volume_should_update(99, 100));            /* ends always reached */
    TEST_ASSERT_TRUE(volume_should_update(1, 0));
    TEST_ASSERT_FALSE(volume_should_update(100, 100));
}

static void test_volume_to_attenuation(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, volume_to_attenuation(100));     /* 0 dB */
    TEST_ASSERT_EQUAL_UINT8(60, volume_to_attenuation(50));     /* -30 dB */
    TEST_ASSERT_EQUAL_UINT8(118, volume_to_attenuation(1));     /* -59 dB */
    TEST_ASSERT_EQUAL_UINT8(120, volume_to_attenuation(-5));    /* clamped */
    TEST_ASSERT_EQUAL_UINT8(0, volume_to_attenuation(150));
    for (int p = 1; p < 100; p++) {                             /* louder = less attenuation */
        TEST_ASSERT_TRUE(volume_to_attenuation(p + 1) <= volume_to_attenuation(p));
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_note_hz_converts_period_in_ms);
    RUN_TEST(test_note_hz_rest_is_zero);
    RUN_TEST(test_note_seconds_is_beat_times_8_times_tempo);
    RUN_TEST(test_next_note_step_walks_through_the_song);
    RUN_TEST(test_next_note_step_repeats_after_a_gap);
    RUN_TEST(test_next_note_step_rejects_bad_index);
    RUN_TEST(test_song_title_removes_lcd_padding);
    RUN_TEST(test_song_title_truncates_safely);
    RUN_TEST(test_song_title_handles_empty_parts_and_null);
    RUN_TEST(test_buttons_give_song_number_in_binary);
    RUN_TEST(test_selector_select_then_confirm);
    RUN_TEST(test_selector_timeout_cancels);
    RUN_TEST(test_selector_timeout_when_idle_is_harmless);
    RUN_TEST(test_debounce_accepts_press_after_n_samples_once);
    RUN_TEST(test_debounce_ignores_bounces);
    RUN_TEST(test_debounce_release_then_press_again);
    RUN_TEST(test_pot_to_percent_range_and_dead_zones);
    RUN_TEST(test_pot_to_percent_is_monotonic);
    RUN_TEST(test_volume_hysteresis);
    RUN_TEST(test_volume_to_attenuation);
    return UNITY_END();
}
