// Android: stdout and stderr into logcat, the app's private data directory,
// and the display's safe insets (from OpenTrailActivity, over JNI).
// Compiled everywhere; empty but for safe_area() off Android.
#include "platform_android.h"

static SafeArea s_area;

SafeArea safe_area(void) { return s_area; }

#if defined(PLATFORM_ANDROID)
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Exported by raylib's Android backend (rcore_android.c); raylib.h does not
// declare it.
extern struct android_app *GetAndroidApp(void);

// Called from OpenTrailActivity.pushSafeInsets(), in the surface's pixels.
JNIEXPORT void JNICALL
Java_com_danheskett_opentrail_OpenTrailActivity_nativeSetSafeInsets(
    JNIEnv *env, jobject thiz, jint top, jint bottom, jint left, jint right, jint cutout_left, jint cutout_right) {
    (void)env; (void)thiz; (void)cutout_left; (void)cutout_right;
    s_area = (SafeArea){ top, bottom, left, right };
}

// stdout and stderr are /dev/null on Android: piped into logcat a line at a time.
static void *log_pump(void *arg) {
    int rd = (int)(long)arg;
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
            __android_log_write(ANDROID_LOG_INFO, "opentrail", line);
            line = nl + 1;
        }
        used = strlen(line);
        memmove(buf, line, used + 1);
        if (used >= sizeof buf - 2) { __android_log_write(ANDROID_LOG_INFO, "opentrail", buf); used = 0; }
    }
    return NULL;
}

void android_boot(void) {
    int fds[2];
    if (pipe(fds) != 0) return;
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    pthread_t t;
    if (pthread_create(&t, NULL, log_pump, (void *)(long)fds[0]) == 0) pthread_detach(t);
}

const char *android_data_dir(void) {
    struct android_app *app = GetAndroidApp();
    return app && app->activity && app->activity->internalDataPath ? app->activity->internalDataPath : ".";
}
#endif
