// The state UIKit publishes and the game thread reads (plat_ios.h): every
// value a word written in one store, the taps in a one-writer, one-reader
// ring, and the display link's tick as a semaphore the game thread waits on.
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "plat_ios.h"
#include <dispatch/dispatch.h>
#include <os/log.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

static _Atomic int s_w = 1, s_h = 1, s_sx, s_sy, s_sw = 1, s_sh = 1;
static _Atomic bool s_active = true;

void plat_ios_set_screen(int w, int h, int safe_x, int safe_y, int safe_w, int safe_h) {
    atomic_store(&s_w, w > 0 ? w : 1); atomic_store(&s_h, h > 0 ? h : 1);
    atomic_store(&s_sx, safe_x); atomic_store(&s_sy, safe_y);
    atomic_store(&s_sw, safe_w > 0 ? safe_w : 1); atomic_store(&s_sh, safe_h > 0 ? safe_h : 1);
}

void plat_ios_screen(int *w, int *h, int *safe_x, int *safe_y, int *safe_w, int *safe_h) {
    *w = atomic_load(&s_w); *h = atomic_load(&s_h);
    *safe_x = atomic_load(&s_sx); *safe_y = atomic_load(&s_sy);
    *safe_w = atomic_load(&s_sw); *safe_h = atomic_load(&s_sh);
}

#define TAPS 64
static float s_tap[TAPS][2];
static _Atomic unsigned s_tap_wr, s_tap_rd;

void plat_ios_push_tap(float x, float y) {
    unsigned wr = atomic_load(&s_tap_wr);
    if (wr - atomic_load(&s_tap_rd) >= TAPS) return;        // full: a tap dropped
    s_tap[wr % TAPS][0] = x; s_tap[wr % TAPS][1] = y;
    atomic_store(&s_tap_wr, wr + 1);
}

bool plat_ios_pop_tap(float *x, float *y) {
    unsigned rd = atomic_load(&s_tap_rd);
    if (rd == atomic_load(&s_tap_wr)) return false;
    *x = s_tap[rd % TAPS][0]; *y = s_tap[rd % TAPS][1];
    atomic_store(&s_tap_rd, rd + 1);
    return true;
}

void plat_ios_set_active(bool active) { atomic_store(&s_active, active); }
bool plat_ios_active(void) { return atomic_load(&s_active); }

static dispatch_semaphore_t s_tick;
static double s_start;

void plat_ios_tick(void) {
    if (!s_tick) return;
    dispatch_semaphore_signal(s_tick);
}

void plat_ios_wait_tick(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ s_tick = dispatch_semaphore_create(0); s_start = CACurrentMediaTime(); });
    // at most a frame and a bit: a stalled display link (in the background) must not hang the game for ever
    dispatch_semaphore_wait(s_tick, dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC));
}

double plat_ios_time(void) { return s_start ? CACurrentMediaTime() - s_start : 0; }

// stdout is nowhere on a device: piped into the unified log, a line at a time.
// stdout only: NSLog writes to stderr, which would come back round.
void ios_log_stdout(void) {
    int fds[2];
    if (pipe(fds) != 0) return;
    dup2(fds[1], STDOUT_FILENO);
    setvbuf(stdout, NULL, _IOLBF, 0);
    int rd = fds[0];
    [NSThread detachNewThreadWithBlock:^{
        char buf[1024];
        size_t used = 0;
        for (;;) {
            ssize_t n = read(rd, buf + used, sizeof buf - used - 1);
            if (n <= 0) break;
            used += (size_t)n;
            buf[used] = 0;
            char *line = buf, *nl;
            while ((nl = strchr(line, '\n')) != NULL) {
                *nl = 0;
                os_log(OS_LOG_DEFAULT, "opentrail: %{public}s", line);
                line = nl + 1;
            }
            used = strlen(line);
            memmove(buf, line, used + 1);
            if (used >= sizeof buf - 2) { os_log(OS_LOG_DEFAULT, "opentrail: %{public}s", buf); used = 0; }
        }
    }];
}

const char *ios_data_dir(void) {
    static char dir[1024];
    if (dir[0]) return dir;
    @autoreleasepool {
        NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        NSString *path = [(dirs.count ? dirs[0] : NSTemporaryDirectory()) stringByAppendingPathComponent:@"opentrail"];
        [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil];
        snprintf(dir, sizeof dir, "%s", path.UTF8String);
    }
    return dir;
}
