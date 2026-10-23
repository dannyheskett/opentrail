// Android's part of the host (platform_android.c).
#ifndef OT_PLATFORM_ANDROID_H
#define OT_PLATFORM_ANDROID_H

// The display's safe insets in pixels (the camera cutout, the gesture bar);
// zeros where nothing sets them.
typedef struct { int top, bottom, left, right; } SafeArea;
SafeArea safe_area(void);

#if defined(PLATFORM_ANDROID)
void android_boot(void);                    // stdout and stderr into logcat
const char *android_data_dir(void);         // the app's private files directory
#endif

#endif
