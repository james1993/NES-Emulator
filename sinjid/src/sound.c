/* ===========================================================================
   sound.c -- audio, generated the same way the art is: entirely in code.

   The original calls playSound() for a fixed vocabulary of events -- sword
   hits, a blocked blow, coins, a picked-up item, a level, a spent skill point,
   a refusal, thunder, a howl, footsteps.  The same vocabulary is reproduced
   here as synthesised waveforms; no audio from the original is used.
   =========================================================================== */
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SR 22050

static Sound SFX[SFX_COUNT];
static bool  audioReady = false;

/* --- tiny synth helpers ------------------------------------------------- */

static float env_ad(float t, float dur, float attack)
{
    if (t < attack) return t / attack;
    float d = (t - attack) / (dur - attack);
    if (d > 1) d = 1;
    return 1.0f - d;
}

static float noise(void) { return (rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

/* Builds one sound from a per-sample callback. */
static Sound synth(float dur, float (*fn)(float t, float dur))
{
    int n = (int)(SR * dur);
    short *buf = (short *)calloc(n, sizeof(short));
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float v = fn(t, dur);
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        buf[i] = (short)(v * 22000);
    }
    Wave w = { 0 };
    w.frameCount = n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = buf;
    Sound s = LoadSoundFromWave(w);
    free(buf);
    return s;
}

/* --- the voices --------------------------------------------------------- */

static float v_sword1(float t, float d)      /* a cut landing              */
{
    float e = env_ad(t, d, 0.002f);
    return (noise() * 0.7f + sinf(t * 2200) * 0.3f) * e * e;
}
static float v_sword2(float t, float d)      /* a heavier cut              */
{
    float e = env_ad(t, d, 0.003f);
    return (noise() * 0.8f + sinf(t * 1400) * 0.4f) * e * e;
}
static float v_block(float t, float d)       /* steel turning steel        */
{
    float e = env_ad(t, d, 0.001f);
    return (sinf(t * 5200) * 0.5f + sinf(t * 7900) * 0.3f + noise() * 0.2f) * e * e * e;
}
static float v_break(float t, float d)       /* a guard shattering         */
{
    float e = env_ad(t, d, 0.001f);
    return (noise() * 0.9f + sinf(t * 900) * 0.4f) * e * e;
}
static float v_coins(float t, float d)
{
    float e = env_ad(t, d, 0.004f);
    float ping = sinf(t * 5400) + sinf(t * 7300) * 0.6f + sinf(t * 9100) * 0.4f;
    return ping * 0.3f * e * e;
}
static float v_item(float t, float d)        /* something picked up        */
{
    float e = env_ad(t, d, 0.01f);
    float f = 700 + 900 * t / d;
    return sinf(t * f * 6.2832f) * 0.5f * e;
}
static float v_level(float t, float d)       /* a level gained             */
{
    float e = env_ad(t, d, 0.02f);
    int step = (int)(t / (d / 3));
    float f = 440.0f * (step == 0 ? 1.0f : step == 1 ? 1.26f : 1.5f);
    return (sinf(t * f * 6.2832f) * 0.5f + sinf(t * f * 12.5664f) * 0.2f) * e;
}
static float v_skill(float t, float d)       /* a point spent              */
{
    float e = env_ad(t, d, 0.005f);
    float f = 900 + 500 * sinf(t * 40);
    return sinf(t * f * 6.2832f) * 0.4f * e;
}
static float v_no(float t, float d)          /* refused                    */
{
    float e = env_ad(t, d, 0.005f);
    return sinf(t * 180 * 6.2832f) * 0.5f * e;
}
static float v_thunder(float t, float d)
{
    float e = env_ad(t, d, 0.05f);
    return (noise() * 0.8f + sinf(t * 60 * 6.2832f) * 0.5f) * e * e;
}
static float v_howl(float t, float d)
{
    float e = env_ad(t, d, 0.15f);
    float f = 320 + 120 * sinf(t * 3.0f);
    return sinf(t * f * 6.2832f) * 0.4f * e;
}
static float v_step(float t, float d)
{
    float e = env_ad(t, d, 0.002f);
    return (noise() * 0.5f + sinf(t * 140 * 6.2832f) * 0.4f) * e * e;
}
static float v_heal(float t, float d)
{
    float e = env_ad(t, d, 0.03f);
    float f = 600 + 400 * t / d;
    return (sinf(t * f * 6.2832f) * 0.4f + sinf(t * f * 3 * 6.2832f) * 0.15f) * e;
}
static float v_spell(float t, float d)
{
    float e = env_ad(t, d, 0.01f);
    float f = 1200 - 700 * t / d;
    return (sinf(t * f * 6.2832f) * 0.4f + noise() * 0.2f) * e;
}
static float v_die(float t, float d)
{
    float e = env_ad(t, d, 0.01f);
    float f = 300 - 200 * t / d;
    return (sinf(t * f * 6.2832f) * 0.5f + noise() * 0.3f) * e * e;
}

void sound_init(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    audioReady = true;
    SFX[SFX_SWORD1]  = synth(0.16f, v_sword1);
    SFX[SFX_SWORD2]  = synth(0.22f, v_sword2);
    SFX[SFX_BLOCK]   = synth(0.20f, v_block);
    SFX[SFX_BREAK]   = synth(0.35f, v_break);
    SFX[SFX_COINS]   = synth(0.30f, v_coins);
    SFX[SFX_ITEM]    = synth(0.25f, v_item);
    SFX[SFX_LEVEL]   = synth(0.60f, v_level);
    SFX[SFX_SKILL]   = synth(0.25f, v_skill);
    SFX[SFX_NO]      = synth(0.18f, v_no);
    SFX[SFX_THUNDER] = synth(0.90f, v_thunder);
    SFX[SFX_HOWL]    = synth(0.80f, v_howl);
    SFX[SFX_STEP]    = synth(0.10f, v_step);
    SFX[SFX_HEAL]    = synth(0.40f, v_heal);
    SFX[SFX_SPELL]   = synth(0.35f, v_spell);
    SFX[SFX_DIE]     = synth(0.50f, v_die);
}

void sound_play(int id)
{
    if (!audioReady || id < 0 || id >= SFX_COUNT) return;
    SetSoundPitch(SFX[id], 0.94f + (rand() % 13) * 0.01f);
    PlaySound(SFX[id]);
}

void sound_close(void)
{
    if (!audioReady) return;
    for (int i = 0; i < SFX_COUNT; i++) UnloadSound(SFX[i]);
    CloseAudioDevice();
    audioReady = false;
}
