// The host's raylib-free half, shared by every host (host.c's raylib one,
// ios/host_ios.c): the original's files, the sound mixed for the device, and
// the persistent files (the graves and the Top Ten).
#ifndef OT_CORE_H
#define OT_CORE_H

#include "mach.h"
#include "music.h"
#include "game.h"
#include <stdint.h>

// Asset loader for Mach.load: raw DOS file bodies, compiled in (embed.h).
int host_load(void *ctx, const char *name, uint8_t *buf, int max);

#define CORE_AUDIO_RATE 44100                // the sound, every platform

// The sound: once a frame the game thread renders the speaker (and modern
// mode's music, when set) up to m->cycles into a ring; the device's audio
// thread reads it (interleaved stereo int16, zeros when it runs dry). One
// producer, one consumer.
void core_audio_init(Mach *m, int rate);
void core_set_music(MusicSynth *mu);
void core_pump_audio(Mach *m);
int  core_audio_read(int16_t *out, int frames);

// The persistent files, one per disk file, in the user's data directory
// (core_set_data_dir() first where the platform names it: Android, iOS).
void core_set_data_dir(const char *dir);
void persist_load(Persist *p);
// persist_save(p, which) is the engine's (game.h): 0 both tombstone files, 1 the Top Ten.

#endif
