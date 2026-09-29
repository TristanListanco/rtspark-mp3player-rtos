/*
 * Checks on the course-provided song data (include/song_def.h, unchanged):
 * the player trusts `length`, the note periods and the names, so a mistake
 * there would read past an array or show garbage on the 16x2 display.
 *   pio test -e native -f native/test_songs
 */
#include <unity.h>

#include "player_logic.h"
#include "../../../src/player_logic.cpp"

#include "song.h"
#include "song_def.h"    /* last: #defines Do, Re, ..., b0..b4 */

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

struct SongData {
    const Song *song;
    const char *label;
    int notes;       /* elements in the note[] array */
    int beats;       /* elements in the beat[] array */
};

/* Same order as the songs[] table in main.cpp (B2 B3 B4 = 000 ... 111, then 9, 10) */
static const SongData all[] = {
    { &FUR_ELISE,                 "Fur Elise",        COUNT(Fur_Elise_note),                COUNT(Fur_Elise_beat) },
    { &CANNON_IN_D,               "Canon in D",       COUNT(Canon_In_D_note),               COUNT(Canon_In_D_beat) },
    { &MINUET_IN_G_MAJOR,         "Minuet in G",      COUNT(Minuet_In_G_major_note),        COUNT(Minuet_In_G_major_beat) },
    { &TURKISH_MARCH,             "Turkish March",    COUNT(Turkish_March_note),            COUNT(Turkish_March_beat) },
    { &NOCTRUNE_IN_E_FLAT,        "Nocturne E flat",  COUNT(Nocturne_in_E_flat_note),       COUNT(Nocturne_in_E_flat_beat) },
    { &WALTZ_NO2,                 "Waltz No. 2",      COUNT(Waltz_No2_note),                COUNT(Waltz_No2_beat) },
    { &NOCTRUNE_IN_C_SHARP_MAJOR, "Nocturne C sharp", COUNT(Nocturne_in_C_sharp_minor_note), COUNT(Nocturne_in_C_sharp_minor_beat) },
    { &SYMPHONY_NO40,             "Symphony No. 40",  COUNT(Symphony_No40_note),            COUNT(Symphony_No40_beat) },
    { &SYMPHONY_NO5,              "Symphony No. 5",   COUNT(Symphony_No5_note),             COUNT(Symphony_No5_beat) },
    { &EINE_KLEINE_NACHTAMUSIK,   "Eine Kleine",      COUNT(Eine_Kleine_Nachtamusik_note),  COUNT(Eine_Kleine_Nachtamusik_beat) },
};

void setUp(void) {}
void tearDown(void) {}

static void test_ten_songs_eight_selectable(void)
{
    TEST_ASSERT_EQUAL_INT(10, COUNT(all));
    TEST_ASSERT_TRUE(COUNT(all) >= 8);          /* 3 binary buttons select 8 */
}

static void test_length_matches_note_and_beat_arrays(void)
{
    for (int i = 0; i < COUNT(all); i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(all[i].notes, all[i].song->length, all[i].label);
        TEST_ASSERT_EQUAL_INT_MESSAGE(all[i].beats, all[i].song->length, all[i].label);
    }
}

static void test_notes_are_rests_or_audible_periods(void)
{
    for (int i = 0; i < COUNT(all); i++) {
        const Song &s = *all[i].song;
        for (int k = 0; k < s.length; k++) {
            const float p = s.note[k];
            if (p == 0.0f) {
                continue;                                    /* No: rest */
            }
            /* 0.8 .. 5.2 ms = 190 .. 1250 Hz, far below Nyquist (16.3 kHz) */
            TEST_ASSERT_TRUE_MESSAGE(p >= 0.8f && p <= 5.2f, all[i].label);
            const float hz = note_hz(s, k);
            TEST_ASSERT_TRUE_MESSAGE(hz > 190.0f && hz < 1250.0f, all[i].label);
        }
    }
}

static void test_every_song_has_notes_and_rests_are_handled(void)
{
    int total_rests = 0;
    for (int i = 0; i < COUNT(all); i++) {
        const Song &s = *all[i].song;
        int notes = 0;
        for (int k = 0; k < s.length; k++) {
            if (s.note[k] > 0.0f) {
                notes++;
            } else {
                total_rests++;
                TEST_ASSERT_EQUAL_FLOAT(0.0f, note_hz(s, k));  /* no division by 0 */
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(notes > 0, all[i].label);
    }
    TEST_ASSERT_EQUAL_INT(342, total_rests);        /* "No" entries in song_def.h */
}

static void test_beats_and_tempo_are_positive(void)
{
    for (int i = 0; i < COUNT(all); i++) {
        const Song &s = *all[i].song;
        TEST_ASSERT_TRUE_MESSAGE(s.tempo > 0.0f, all[i].label);
        for (int k = 0; k < s.length; k++) {
            TEST_ASSERT_TRUE_MESSAGE(s.beat[k] > 0.0f && s.beat[k] <= 1.0f, all[i].label);
        }
    }
}

static void test_names_fit_the_16x2_display(void)
{
    for (int i = 0; i < COUNT(all); i++) {
        const Song &s = *all[i].song;
        TEST_ASSERT_TRUE_MESSAGE(s.name1.size() > 0 && s.name1.size() <= 16, all[i].label);
        TEST_ASSERT_TRUE_MESSAGE(s.name2.size() > 0 && s.name2.size() <= 16, all[i].label);
    }
}

static void test_song_lengths_are_playable(void)
{
    /* With beat x 8 x tempo every song lasts between 5 s and 5 min, and no
     * single note is shorter than the 3.9 ms audio buffer. */
    for (int i = 0; i < COUNT(all); i++) {
        const Song &s = *all[i].song;
        float total = 0.0f;
        for (int k = 0; k < s.length; k++) {
            const float d = note_seconds(s, k);
            TEST_ASSERT_TRUE_MESSAGE(d > 0.004f, all[i].label);
            total += d;
        }
        TEST_ASSERT_TRUE_MESSAGE(total > 5.0f && total < 300.0f, all[i].label);
    }
}

static void test_titles_for_the_console(void)
{
    char buf[40];
    song_title(TURKISH_MARCH, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Turkish March - Mozart", buf);
    song_title(FUR_ELISE, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Fur Elise - Beethoven", buf);
    song_title(NOCTRUNE_IN_E_FLAT, buf, sizeof buf);
    TEST_ASSERT_EQUAL_STRING("Nocturne in E flat -Chopin", buf);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_ten_songs_eight_selectable);
    RUN_TEST(test_length_matches_note_and_beat_arrays);
    RUN_TEST(test_notes_are_rests_or_audible_periods);
    RUN_TEST(test_every_song_has_notes_and_rests_are_handled);
    RUN_TEST(test_beats_and_tempo_are_positive);
    RUN_TEST(test_names_fit_the_16x2_display);
    RUN_TEST(test_song_lengths_are_playable);
    RUN_TEST(test_titles_for_the_console);
    return UNITY_END();
}
