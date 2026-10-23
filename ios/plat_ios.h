// What UIKit tells the game thread (ios_main.m writes; host_ios.c reads), and
// the Objective-C services the plain-C host calls. C linkage throughout.
#ifndef OT_PLAT_IOS_H
#define OT_PLAT_IOS_H

#include <stdbool.h>
#include <stdint.h>

// The drawable in device pixels, and the safe rectangle inside it.
void plat_ios_set_screen(int w, int h, int safe_x, int safe_y, int safe_w, int safe_h);
void plat_ios_screen(int *w, int *h, int *safe_x, int *safe_y, int *safe_w, int *safe_h);

// Taps, in device pixels: a queue from the main thread to the game thread.
void plat_ios_push_tap(float x, float y);
bool plat_ios_pop_tap(float *x, float *y);

// In the foreground (drawing allowed, the clock runs).
void plat_ios_set_active(bool active);
bool plat_ios_active(void);

// The display link's tick; the game thread waits for the next one.
void plat_ios_tick(void);
void plat_ios_wait_tick(void);
double plat_ios_time(void);                 // seconds since start

// Services (present_metal.m, audio_ios.m, plat_ios.m).
void ios_present(const uint8_t *rgba, int cw, int ch, float x, float y, float w, float h);
void ios_audio_start(void);
void ios_log_stdout(void);                  // stdout into the unified log
const char *ios_data_dir(void);             // Application Support/opentrail, created

// The game thread's body (host_ios.c); never returns.
void ios_game_main(void);

#endif
