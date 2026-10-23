// Modern mode's music: the original's tunes (MS0-MS17.BIN, TS.BIN, the
// extension's tune format) played by a small synthesised orchestra instead of
// the one-bit speaker. The melody is the original's, note for note and at its
// tempo; the harmony under it is worked out from the melody (its key, its
// metre, a chord per bar). The sound is rendered here, raylib-free, on the
// emulated clock (as the speaker is), so it is deterministic and the host only
// mixes it in.
#ifndef OT_MUSIC_H
#define OT_MUSIC_H

#include "mach.h"
#include <stdint.h>

enum { MUSIC_TUNES = 20, MUSIC_TAPS = 18, MUSIC_ARRIVAL = 17, MUSIC_STAB = 19, MUSIC_NOTES = 160,
       MUSIC_EVENTS = 1400, MUSIC_VOICES = 40, MUSIC_BARS = 96 };

typedef struct { uint8_t pitch; unsigned long long start, len; } MusicNote;   // cycles from the tune's start

// What the orchestra plays, worked out at load: each event starts a voice of
// one part (a part's new note ends its last one, except the harp's).
typedef struct {
    unsigned long long at, len;     // cycles
    uint8_t part, midi;
    float amp;
} MusicEvent;

typedef struct {
    int key, minor;                 // tonic pitch class, mode
    int beats, unit;                // metre: beats a bar; the beat's length (in the tune's length units)
    int pickup;                     // units before the first full bar
    int nbar;                       // bars (the pickup is bar 0 when there is one)
    uint8_t chord[MUSIC_BARS];      // root pitch class
    uint8_t chord_minor[MUSIC_BARS];
} MusicHarmony;

typedef struct {
    int on, part;
    unsigned long long end;         // the cycle it lets go (then releases)
    float freq, amp, env, age, rel, rel_time, pan;
    float ph[4], lp, lp2;           // oscillators (detuned copies), filter state
    float *ks; int ks_len, ks_pos;  // the harp's string (Karplus-Strong)
    uint32_t noise;
} MusicVoice;

typedef struct {
    int rate;
    MusicNote note[MUSIC_TUNES][MUSIC_NOTES];
    int nnote[MUSIC_TUNES];
    unsigned long long length[MUSIC_TUNES];          // cycles, as the original plays it once
    MusicHarmony harm[MUSIC_TUNES];
    MusicEvent ev[MUSIC_TUNES][MUSIC_EVENTS];
    int nev[MUSIC_TUNES];
    int loaded;

    // playback
    int tune, loop, playing;
    unsigned long long t0;                           // the cycle the tune (or its current pass) began
    int next;                                        // the next event to start
    float gain, fade;                                // fade per sample while stopping (0: none)
    unsigned long long at;                           // the cycle of the next sample
    double frac;
    MusicVoice v[MUSIC_VOICES];
    float ks_buf[MUSIC_VOICES][2048];
    float rv[16][4096]; int rv_len[16], rv_pos[16]; float rv_lp[16];   // the hall: combs and all-passes
} MusicSynth;

// The tunes, through the machine's loader; works out their harmony and
// orchestration. Returns how many loaded.
int  music_load(MusicSynth *mu, Mach *m);
void music_init(MusicSynth *mu, int rate, unsigned long long now);

// Start tune t (0-17 MS<t>, MUSIC_TAPS the tombstone's, MUSIC_STAB the raft's
// collision: four orchestral stabs), looping or once.
void music_play(MusicSynth *mu, int t, int loop, unsigned long long now);
void music_stop(MusicSynth *mu);                          // fade out (about half a second)
int  music_playing(const MusicSynth *mu);                 // the tune, -1 none

// Render every whole stereo frame up to emulated cycle `until` into lr
// (interleaved), at most max frames; returns the frames written.
int  music_render(MusicSynth *mu, unsigned long long until, float *lr, int max);

#endif
