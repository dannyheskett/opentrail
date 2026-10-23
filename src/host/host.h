// The raylib host: window, keyboard, audio, and frame presentation for the
// emulated Apple II. The game runs on the main thread and owns the loop, as the
// original did; the host is reached through Mach.idle once per 1/60 s of
// emulated time, and on each call it presents a frame and paces to real time.
#ifndef OT_HOST_H
#define OT_HOST_H

#include "mach.h"
#include "music.h"

// A presenter shows the running mode: view_legacy.c the emulated Apple II
// screen, view_modern.c the modern scene. Once a frame the host calls
// prepare() (offscreen work: render textures), then draw() into the window,
// and again into the recording when one is running, so draw() only draws
// into the current target at w x h. key() may consume raylib keys the view
// owns; it sees every key but Alt+Enter and F12. While modal() is true (a
// dialog of the view's own is up) the game is paused and every key goes to
// key(). input() runs once a frame after the keys (the mouse).
typedef struct {
    void (*prepare)(void *ctx);
    void (*draw)(void *ctx, int w, int h);
    int  (*key)(void *ctx, int raylib_key);
    int  (*modal)(void *ctx);
    void (*input)(void *ctx);
    void *ctx;
} HostPresenter;

void host_init(Mach *m, const char *title, int w, int h);   // initial window size
void host_close(void);             // also ends a recording (recorder.c)
int  host_quit_requested(void);     // the window was closed, or host_quit
void host_quit(void);               // end the program at the next frame
// Run the program (which owns the loop) until it returns or the window closes.
void host_run(void (*program)(Mach *m, void *arg), void *arg);
void host_set_presenter(HostPresenter p);
// True while the views draw the recording's frame (a fractional scale is
// fine there: it is drawn at twice the size and smoothed down).
int  host_recording_pass(void);

// The harness (tools/c/harness.c): a hidden window, no audio and no pacing;
// the harness's own idle() runs the frames. host_render draws the presenter
// (prepare, then draw at w x h) offscreen and reads it back into px (RGBA,
// top row first), so a frame can be looked at whenever the harness stops.
void host_init_hidden(Mach *m, int w, int h, void (*idle)(void *ctx), void *ctx);
void host_render(uint32_t *px, int w, int h);
int  host_view_key(int raylib_key);  // the presenter's key() (its quit dialog)
int  host_view_modal(void);

// Modern mode's music (music.h), mixed into the sound at the host's rate,
// HOST_AUDIO_RATE (NULL: the speaker alone).
#define HOST_AUDIO_RATE CORE_AUDIO_RATE
void host_set_music(MusicSynth *mu);

// Asset loader, the sound's ring and the persistent files: core.h.
#include "core.h"

#endif
