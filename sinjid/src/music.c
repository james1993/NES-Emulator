/* ===========================================================================
   music.c -- the nine music tracks, generated the same way the art is.

   The original drives its audio from one clip (sprite 180) whose frame labels
   are the cue names, and a single playSound(name) that jumps that clip to the
   label.  Frames 2..18 of it are the music: Dream, Flute, Orc, Over, Over2,
   Violin, Battle1, Battle2 and Battle3.  Those nine names, and the moments
   each one fires at, are the original's and are reproduced here.

   The music itself is NOT.  Its tracks are embedded audio streams and none of
   them is used, decoded, or approximated by ear.  Every note below is written
   for this remake and rendered by the small synthesiser in this file, exactly
   as art.c draws every shape rather than shipping an image.

   Each track is a beat-addressed list of notes rendered once into a looping
   PCM buffer.  A note that runs past the loop point wraps into the head of
   the buffer, so the loop is seamless with no crossfade.
   =========================================================================== */
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MSR       22050            /* render rate, matching sound.c        */
#define CHUNK     1024             /* frames pushed per update             */
#define TAU       6.28318530718f

/* --- voices ------------------------------------------------------------- */
typedef enum {
    V_BELL,      /* struck metal: bright attack, long decay                 */
    V_PLUCK,     /* a stopped string                                        */
    V_PAD,       /* a held drone                                            */
    V_DRUM,      /* a struck skin, pitch falling away                       */
    V_BASS,      /* the low ostinato                                        */
    V_BOW,       /* sustained and bowed, with vibrato                       */
    V_BREATH     /* an end-blown flute: mostly tone, some air               */
} Voice;

typedef struct {
    float beat;      /* when it starts, in beats from the loop head         */
    float len;       /* how long it sounds, in beats                        */
    short midi;      /* pitch; -1 for the unpitched drum                    */
    unsigned char voice;
    float gain;
} Note;

typedef struct {
    const Note *notes;
    int         count;
    float       bpm;
    float       beats;      /* the loop length                             */
    float       gain;
} Track;

/* --- the writing --------------------------------------------------------
   Two scales carry the whole score.  MINOR_PENT is A minor pentatonic
   (A C D E G); IN_SCALE is the "in" scale on D (D Eb G A Bb), whose two
   half-steps give the village and title cues their colour.  Pitches are
   written as MIDI numbers so the intervals are readable:
        A2 45   D3 50   E3 52   G3 55   A3 57   C4 60   D4 62
        E4 64   G4 67   A4 69   C5 72   D5 74   E5 76   G5 79   A5 81
   ---------------------------------------------------------------------- */

/* Dream -- the title screen, and the first cue of the ending.  Slow, wide
   spacing, a drone underneath and a bell figure that never quite resolves. */
static const Note N_DREAM[] = {
    { 0.0f, 8.0f, 45, V_PAD,   0.30f }, { 0.0f, 8.0f, 57, V_PAD,   0.16f },
    { 0.0f, 2.5f, 76, V_BELL,  0.42f }, { 1.5f, 2.5f, 72, V_BELL,  0.30f },
    { 3.0f, 3.0f, 69, V_BELL,  0.38f }, { 5.0f, 2.0f, 67, V_BELL,  0.26f },
    { 6.5f, 3.5f, 64, V_BELL,  0.34f },
    { 8.0f, 8.0f, 45, V_PAD,   0.30f }, { 8.0f, 8.0f, 55, V_PAD,   0.16f },
    { 8.0f, 2.5f, 74, V_BELL,  0.40f }, { 9.5f, 2.5f, 69, V_BELL,  0.28f },
    {11.0f, 3.0f, 67, V_BELL,  0.36f }, {13.0f, 2.0f, 64, V_BELL,  0.24f },
    {14.5f, 3.5f, 57, V_BELL,  0.34f },
};

/* Flute -- the arena itself, where most of the game is spent.  It has to sit
   still under a lot of listening, so: one drone, a slow pentatonic line, and
   a plucked figure that marks the bar without pushing it. */
static const Note N_FLUTE[] = {
    { 0.0f,16.0f, 50, V_PAD,    0.22f },
    { 0.0f, 3.0f, 74, V_BREATH, 0.34f }, { 3.0f, 1.0f, 72, V_BREATH, 0.24f },
    { 4.0f, 2.0f, 69, V_BREATH, 0.30f }, { 6.0f, 2.0f, 67, V_BREATH, 0.26f },
    { 8.0f, 3.0f, 69, V_BREATH, 0.32f }, {11.0f, 1.0f, 74, V_BREATH, 0.22f },
    {12.0f, 4.0f, 62, V_BREATH, 0.30f },
    { 0.0f, 1.0f, 50, V_PLUCK,  0.20f }, { 2.0f, 1.0f, 57, V_PLUCK,  0.14f },
    { 4.0f, 1.0f, 62, V_PLUCK,  0.18f }, { 6.0f, 1.0f, 57, V_PLUCK,  0.13f },
    { 8.0f, 1.0f, 50, V_PLUCK,  0.20f }, {10.0f, 1.0f, 57, V_PLUCK,  0.14f },
    {12.0f, 1.0f, 55, V_PLUCK,  0.18f }, {14.0f, 1.0f, 57, V_PLUCK,  0.13f },
};

/* Battle1 -- the first of the three, and the one heard most.  Drums and a
   bass ostinato only; the fight supplies the rest. */
static const Note N_BATTLE1[] = {
    { 0.0f, 0.5f, -1, V_DRUM, 0.52f }, { 1.0f, 0.5f, -1, V_DRUM, 0.30f },
    { 2.0f, 0.5f, -1, V_DRUM, 0.46f }, { 2.5f, 0.5f, -1, V_DRUM, 0.24f },
    { 3.0f, 0.5f, -1, V_DRUM, 0.30f },
    { 4.0f, 0.5f, -1, V_DRUM, 0.52f }, { 5.0f, 0.5f, -1, V_DRUM, 0.30f },
    { 6.0f, 0.5f, -1, V_DRUM, 0.46f }, { 6.5f, 0.5f, -1, V_DRUM, 0.24f },
    { 7.0f, 0.5f, -1, V_DRUM, 0.34f }, { 7.5f, 0.5f, -1, V_DRUM, 0.24f },
    { 0.0f, 1.0f, 45, V_BASS, 0.40f }, { 1.0f, 1.0f, 45, V_BASS, 0.28f },
    { 2.0f, 1.0f, 52, V_BASS, 0.36f }, { 3.0f, 1.0f, 50, V_BASS, 0.30f },
    { 4.0f, 1.0f, 45, V_BASS, 0.40f }, { 5.0f, 1.0f, 45, V_BASS, 0.28f },
    { 6.0f, 1.0f, 43, V_BASS, 0.36f }, { 7.0f, 1.0f, 45, V_BASS, 0.30f },
};

/* Battle2 -- the same engine, faster, with a line over the top. */
static const Note N_BATTLE2[] = {
    { 0.0f, 0.5f, -1, V_DRUM, 0.52f }, { 0.75f,0.5f, -1, V_DRUM, 0.22f },
    { 1.5f, 0.5f, -1, V_DRUM, 0.34f }, { 2.0f, 0.5f, -1, V_DRUM, 0.48f },
    { 3.0f, 0.5f, -1, V_DRUM, 0.30f }, { 3.5f, 0.5f, -1, V_DRUM, 0.24f },
    { 4.0f, 0.5f, -1, V_DRUM, 0.52f }, { 4.75f,0.5f, -1, V_DRUM, 0.22f },
    { 5.5f, 0.5f, -1, V_DRUM, 0.34f }, { 6.0f, 0.5f, -1, V_DRUM, 0.48f },
    { 7.0f, 0.5f, -1, V_DRUM, 0.34f }, { 7.5f, 0.5f, -1, V_DRUM, 0.30f },
    { 0.0f, 1.0f, 45, V_BASS, 0.40f }, { 1.5f, 0.5f, 45, V_BASS, 0.26f },
    { 2.0f, 1.0f, 48, V_BASS, 0.36f }, { 3.0f, 1.0f, 50, V_BASS, 0.32f },
    { 4.0f, 1.0f, 45, V_BASS, 0.40f }, { 5.5f, 0.5f, 45, V_BASS, 0.26f },
    { 6.0f, 1.0f, 52, V_BASS, 0.36f }, { 7.0f, 1.0f, 50, V_BASS, 0.32f },
    { 0.0f, 0.75f, 69, V_PLUCK, 0.26f }, { 1.0f, 0.5f, 72, V_PLUCK, 0.20f },
    { 2.0f, 1.0f,  74, V_PLUCK, 0.24f }, { 3.5f, 0.5f, 72, V_PLUCK, 0.18f },
    { 4.0f, 0.75f, 69, V_PLUCK, 0.26f }, { 5.0f, 0.5f, 67, V_PLUCK, 0.20f },
    { 6.0f, 1.5f,  64, V_PLUCK, 0.24f },
};

/* Battle3 -- the last of the rotation: everything on, and higher. */
static const Note N_BATTLE3[] = {
    { 0.0f, 0.5f, -1, V_DRUM, 0.54f }, { 0.5f, 0.5f, -1, V_DRUM, 0.22f },
    { 1.0f, 0.5f, -1, V_DRUM, 0.34f }, { 1.5f, 0.5f, -1, V_DRUM, 0.26f },
    { 2.0f, 0.5f, -1, V_DRUM, 0.50f }, { 2.5f, 0.5f, -1, V_DRUM, 0.24f },
    { 3.0f, 0.5f, -1, V_DRUM, 0.36f }, { 3.5f, 0.5f, -1, V_DRUM, 0.28f },
    { 4.0f, 0.5f, -1, V_DRUM, 0.54f }, { 4.5f, 0.5f, -1, V_DRUM, 0.22f },
    { 5.0f, 0.5f, -1, V_DRUM, 0.34f }, { 5.5f, 0.5f, -1, V_DRUM, 0.26f },
    { 6.0f, 0.5f, -1, V_DRUM, 0.50f }, { 6.5f, 0.5f, -1, V_DRUM, 0.30f },
    { 7.0f, 0.5f, -1, V_DRUM, 0.38f }, { 7.5f, 0.5f, -1, V_DRUM, 0.34f },
    { 0.0f, 0.5f, 45, V_BASS, 0.42f }, { 0.5f, 0.5f, 45, V_BASS, 0.22f },
    { 1.0f, 1.0f, 43, V_BASS, 0.34f }, { 2.0f, 1.0f, 48, V_BASS, 0.38f },
    { 3.0f, 1.0f, 50, V_BASS, 0.32f },
    { 4.0f, 0.5f, 45, V_BASS, 0.42f }, { 4.5f, 0.5f, 45, V_BASS, 0.22f },
    { 5.0f, 1.0f, 52, V_BASS, 0.34f }, { 6.0f, 1.0f, 50, V_BASS, 0.38f },
    { 7.0f, 1.0f, 48, V_BASS, 0.32f },
    { 0.0f, 0.5f, 81, V_PLUCK, 0.28f }, { 0.5f, 0.5f, 79, V_PLUCK, 0.20f },
    { 1.0f, 1.0f, 76, V_PLUCK, 0.26f }, { 2.0f, 0.5f, 74, V_PLUCK, 0.24f },
    { 2.5f, 0.5f, 76, V_PLUCK, 0.20f }, { 3.0f, 1.0f, 72, V_PLUCK, 0.26f },
    { 4.0f, 0.5f, 81, V_PLUCK, 0.28f }, { 4.5f, 0.5f, 84, V_PLUCK, 0.20f },
    { 5.0f, 1.0f, 79, V_PLUCK, 0.26f }, { 6.0f, 2.0f, 76, V_PLUCK, 0.26f },
};

/* Orc -- a battle won.  Short, bright, and it resolves, which nothing else
   in the score does. */
static const Note N_ORC[] = {
    { 0.0f, 0.5f, -1, V_DRUM, 0.50f }, { 1.0f, 0.5f, -1, V_DRUM, 0.34f },
    { 2.0f, 0.5f, -1, V_DRUM, 0.44f },
    { 0.0f, 0.5f, 57, V_BELL, 0.40f }, { 0.5f, 0.5f, 64, V_BELL, 0.36f },
    { 1.0f, 0.5f, 69, V_BELL, 0.42f }, { 1.5f, 0.5f, 72, V_BELL, 0.38f },
    { 2.0f, 2.0f, 76, V_BELL, 0.46f }, { 2.5f, 1.5f, 69, V_BELL, 0.24f },
    { 0.0f, 4.0f, 45, V_PAD,  0.24f },
};

/* Over -- a battle lost.  Low, slow, and it falls the whole way. */
static const Note N_OVER[] = {
    { 0.0f, 8.0f, 38, V_PAD,  0.30f }, { 0.0f, 8.0f, 45, V_PAD,  0.14f },
    { 0.0f, 2.5f, 57, V_BELL, 0.36f }, { 2.0f, 2.5f, 55, V_BELL, 0.30f },
    { 4.0f, 2.5f, 52, V_BELL, 0.32f }, { 6.0f, 3.0f, 50, V_BELL, 0.34f },
};

/* Over2 -- the ending's middle cue.  The same fall as Over, but it lands
   somewhere warmer. */
static const Note N_OVER2[] = {
    { 0.0f,12.0f, 43, V_PAD,  0.26f }, { 0.0f,12.0f, 50, V_PAD,  0.14f },
    { 0.0f, 3.0f, 62, V_BELL, 0.36f }, { 2.5f, 2.5f, 67, V_BELL, 0.28f },
    { 5.0f, 3.0f, 64, V_BELL, 0.34f }, { 7.5f, 2.5f, 60, V_BELL, 0.26f },
    {10.0f, 3.0f, 57, V_BELL, 0.32f },
};

/* Violin -- the last cue in the game.  One bowed line, held. */
static const Note N_VIOLIN[] = {
    { 0.0f,16.0f, 45, V_PAD, 0.22f },
    { 0.0f, 3.0f, 69, V_BOW, 0.36f }, { 3.0f, 2.0f, 72, V_BOW, 0.30f },
    { 5.0f, 3.0f, 76, V_BOW, 0.38f }, { 8.0f, 2.0f, 74, V_BOW, 0.30f },
    {10.0f, 2.0f, 72, V_BOW, 0.32f }, {12.0f, 4.0f, 69, V_BOW, 0.36f },
};

#define TRK(a, bpm, beats, g) { a, (int)(sizeof a / sizeof a[0]), bpm, beats, g }
static const Track TRACKS[MUS_COUNT] = {
    [MUS_DREAM]   = TRK(N_DREAM,   100.0f, 16.0f, 0.85f),
    [MUS_FLUTE]   = TRK(N_FLUTE,    84.0f, 16.0f, 0.85f),
    [MUS_ORC]     = TRK(N_ORC,     120.0f,  4.0f, 0.90f),
    [MUS_OVER]    = TRK(N_OVER,     60.0f,  8.0f, 0.85f),
    [MUS_OVER2]   = TRK(N_OVER2,    72.0f, 12.0f, 0.85f),
    [MUS_VIOLIN]  = TRK(N_VIOLIN,   66.0f, 16.0f, 0.85f),
    [MUS_BATTLE1] = TRK(N_BATTLE1, 132.0f,  8.0f, 0.95f),
    [MUS_BATTLE2] = TRK(N_BATTLE2, 144.0f,  8.0f, 0.95f),
    [MUS_BATTLE3] = TRK(N_BATTLE3, 156.0f,  8.0f, 0.95f),
};

/* The cue names are the original's own frame labels, in its frame order. */
const char *MUSIC_NAMES[MUS_COUNT] = {
    "Dream", "Flute", "Orc", "Over", "Over2", "Violin",
    "Battle1", "Battle2", "Battle3"
};

/* --- the synthesiser ---------------------------------------------------- */

static float mnoise(void) { return (rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

static float midi_hz(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }

/* One note, added into buf at `at` with wraparound so a note may cross the
   loop point and come back in at the head. */
static void render_note(float *buf, int n, int at, const Note *no, float spb)
{
    float dur = no->len * spb;
    float hz  = (no->midi >= 0) ? midi_hz(no->midi) : 0.0f;
    int   len = (int)(dur * MSR);
    /* the tail is allowed to ring past the written length */
    int   ring = (int)(MSR * (no->voice == V_BELL ? 1.6f :
                              no->voice == V_PLUCK ? 0.9f :
                              no->voice == V_DRUM ? 0.35f : 0.18f));
    int   total = len + ring;

    /* Karplus-Strong needs a delay line; everything else is stateless. */
    float *ks = NULL; int ksn = 0, ksi = 0;
    if (no->voice == V_PLUCK) {
        ksn = (int)(MSR / (hz > 1 ? hz : 1));
        if (ksn < 2) ksn = 2;
        ks = (float *)malloc(sizeof(float) * ksn);
        for (int i = 0; i < ksn; i++) ks[i] = mnoise();
    }

    for (int i = 0; i < total; i++) {
        float t = (float)i / MSR;
        float e, v = 0.0f;
        switch (no->voice) {
        case V_BELL: {
            e = expf(-t * 2.2f);
            float p = TAU * hz * t;
            v = (sinf(p) + 0.42f * sinf(p * 2.76f) + 0.22f * sinf(p * 5.4f)) * e;
        } break;
        case V_PLUCK: {
            /* pluck: a filtered delay line, damped a little harder up high */
            float damp = 0.494f;
            float out = ks[ksi];
            ks[ksi] = damp * (ks[ksi] + ks[(ksi + 1) % ksn]);
            ksi = (ksi + 1) % ksn;
            v = out * expf(-t * 1.4f);
        } break;
        case V_PAD: {
            float a = t < 0.35f ? t / 0.35f : 1.0f;
            float r = (t > dur) ? expf(-(t - dur) * 6.0f) : 1.0f;
            e = a * r;
            /* two saws a few cents apart, so it beats slowly */
            float s1 = fmodf(hz * t, 1.0f) * 2.0f - 1.0f;
            float s2 = fmodf(hz * 1.004f * t, 1.0f) * 2.0f - 1.0f;
            v = (s1 + s2) * 0.28f * e;
        } break;
        case V_DRUM: {
            e = expf(-t * 16.0f);
            float body = sinf(TAU * (92.0f * expf(-t * 26.0f) + 46.0f) * t);
            v = (body * 0.85f + mnoise() * 0.5f * expf(-t * 42.0f)) * e;
        } break;
        case V_BASS: {
            float r = (t > dur) ? expf(-(t - dur) * 22.0f) : 1.0f;
            e = expf(-t * 1.9f) * r;
            float ph = fmodf(hz * t, 1.0f);
            float tri = 4.0f * fabsf(ph - 0.5f) - 1.0f;   /* triangle */
            v = tri * e;
        } break;
        case V_BOW: {
            float a = t < 0.22f ? t / 0.22f : 1.0f;
            float r = (t > dur) ? expf(-(t - dur) * 7.0f) : 1.0f;
            float vib = 1.0f + 0.006f * sinf(TAU * 5.2f * t);
            e = a * r;
            float ph = fmodf(hz * vib * t, 1.0f);
            v = (2.0f * ph - 1.0f) * 0.45f * e;
        } break;
        default: {  /* V_BREATH */
            float a = t < 0.16f ? t / 0.16f : 1.0f;
            float r = (t > dur) ? expf(-(t - dur) * 8.0f) : 1.0f;
            float vib = 1.0f + 0.004f * sinf(TAU * 4.6f * t);
            e = a * r;
            v = (sinf(TAU * hz * vib * t) * 0.9f
                 + sinf(TAU * hz * 2.0f * t) * 0.10f
                 + mnoise() * 0.05f) * e;
        } break;
        }
        buf[(at + i) % n] += v * no->gain;
    }
    free(ks);
}

/* --- playback ----------------------------------------------------------- */

static AudioStream stream;
static bool   ready = false;
static bool   enabled = true;
static int    current = MUS_NONE;
static short *loop[MUS_COUNT];
static int    loopN[MUS_COUNT];
static int    cursor = 0;
static float  fade = 1.0f;      /* 1 playing, falling to 0 to switch       */
static int    pending = MUS_NONE;
static short  mix[CHUNK];
static int    prevsound = 0;    /* the original's own battle counter        */

static void build(int id)
{
    if (loop[id]) return;
    const Track *tr = &TRACKS[id];
    float spb = 60.0f / tr->bpm;
    int   n   = (int)(tr->beats * spb * MSR);
    float *acc = (float *)calloc(n, sizeof(float));
    if (!acc) return;
    for (int i = 0; i < tr->count; i++)
        render_note(acc, n, (int)(tr->notes[i].beat * spb * MSR), &tr->notes[i], spb);

    /* Normalise on loudness, not on peak.  A sustained track (the bowed and
       breathed ones) carries far more energy per unit of peak than a bell
       track does, so peak-matching alone left Flute sounding about twice as
       loud as Over2.  Scale to a common RMS, then pull back if that would
       push the peak past the ceiling. */
    float peak = 1e-6f, sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(acc[i]);
        if (a > peak) peak = a;
        sum += acc[i] * acc[i];
    }
    float rms = sqrtf(sum / (float)n) + 1e-9f;
    const float TARGET_RMS = 0.13f, CEILING = 0.80f;
    float k = (TARGET_RMS * tr->gain) / rms;
    if (peak * k > CEILING) k = CEILING / peak;
    short *out = (short *)malloc(sizeof(short) * n);
    if (out) {
        for (int i = 0; i < n; i++) {
            float v = acc[i] * k;
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            out[i] = (short)(v * 30000.0f);
        }
    }
    free(acc);
    loop[id] = out;
    loopN[id] = n;
}

void music_init(void)
{
    if (!IsAudioDeviceReady()) return;
    stream = LoadAudioStream(MSR, 16, 1);
    if (!IsAudioStreamValid(stream)) return;
    SetAudioStreamVolume(stream, 0.55f);
    PlayAudioStream(stream);
    ready = true;
    /* Render every loop up front.  Together they come to about 3 MB, and
       doing it here keeps the first bar of a track from hitching the frame
       it starts on. */
    for (int i = 0; i < MUS_COUNT; i++) build(i);
    /* SJ_MUSIC_DUMP=<dir> writes each rendered loop out as a .wav, which is
       how the score is checked without a sound card in the room. */
    {
        const char *dir = getenv("SJ_MUSIC_DUMP");
        if (dir) for (int i = 0; i < MUS_COUNT; i++) {
            if (!loop[i]) continue;
            char path[512];
            snprintf(path, sizeof path, "%s/%s.wav", dir, MUSIC_NAMES[i]);
            Wave w = { 0 };
            w.frameCount = (unsigned)loopN[i];
            w.sampleRate = MSR; w.sampleSize = 16; w.channels = 1;
            w.data = loop[i];
            ExportWave(w, path);
        }
    }
}

/* Switch tracks.  Asking for the track already playing is a no-op, so this
   can be called every frame from wherever the scene is decided. */
void music_play(int id)
{
    if (!ready || id < MUS_NONE || id >= MUS_COUNT) return;
    if (id == current && pending == MUS_NONE) return;
    if (id == current) { pending = MUS_NONE; return; }
    if (current == MUS_NONE) {           /* nothing playing: start clean */
        build(id);
        current = id; cursor = 0; fade = 1.0f; pending = MUS_NONE;
        return;
    }
    if (id != MUS_NONE) build(id);
    pending = id;                        /* fade the old one out first   */
}

void music_stop(void) { music_play(MUS_NONE); }

void music_set_enabled(bool on)
{
    enabled = on;
    if (ready) SetAudioStreamVolume(stream, on ? 0.55f : 0.0f);
}

bool music_enabled(void) { return enabled; }

/* The original picks the battle track from a counter it keeps across the
   whole session: prevsound starts at 0, and each fight does
       if (prevsound < 3) prevsound++; else prevsound = 1;
   then plays 'Battle' + prevsound -- so the three cycle in order. */
int music_battle_next(void)
{
    if (prevsound < 3) prevsound++;
    else               prevsound = 1;
    return MUS_BATTLE1 + (prevsound - 1);
}

void music_update(void)
{
    if (!ready) return;
    while (IsAudioStreamProcessed(stream)) {
        for (int i = 0; i < CHUNK; i++) {
            float v = 0.0f;
            if (current != MUS_NONE && loop[current] && loopN[current] > 0) {
                v = loop[current][cursor] / 32768.0f;
                if (++cursor >= loopN[current]) cursor = 0;
            }
            if (pending != MUS_NONE || fade < 1.0f) {
                /* ~60 ms of fade either side of a switch */
                float step = 1.0f / (0.06f * MSR);
                if (pending != MUS_NONE) {
                    fade -= step;
                    if (fade <= 0.0f) {
                        fade = 0.0f;
                        current = pending; pending = MUS_NONE; cursor = 0;
                    }
                } else if (fade < 1.0f) {
                    fade += step;
                    if (fade > 1.0f) fade = 1.0f;
                }
            }
            v *= fade;
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            mix[i] = (short)(v * 32000.0f);
        }
        UpdateAudioStream(stream, mix, CHUNK);
    }
}

void music_close(void)
{
    if (ready) { StopAudioStream(stream); UnloadAudioStream(stream); ready = false; }
    for (int i = 0; i < MUS_COUNT; i++) { free(loop[i]); loop[i] = NULL; loopN[i] = 0; }
}
