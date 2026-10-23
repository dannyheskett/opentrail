package com.danheskett.opentrail;

import android.app.NativeActivity;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

/**
 * NativeActivity subclass whose job is to make the game immersive:
 * hide the status and navigation bars and draw edge-to-edge, so there is no
 * reserved black band above the game. The C game code is unchanged and
 * still loaded via the "android.app.lib_name" manifest meta-data.
 *
 * The theme (windowLayoutInDisplayCutoutMode=shortEdges) plus raylib's
 * FLAG_FULLSCREEN_MODE were not enough on Android 11-15 — the system still
 * reserved the status-bar inset. Hiding the system bars from the Activity is
 * the reliable fix. Re-applied on focus gain because the bars can transiently
 * reappear (e.g. after a swipe or returning from background).
 */
public class OpenTrailActivity extends NativeActivity {

    // NativeActivity loads libopentrail.so by dlopen()ing it in framework
    // native code, which never registers it with this class's ClassLoader. ART's
    // implicit JNI resolution only searches libraries the ClassLoader knows
    // about, so a `native` method declared here would fail to resolve at the
    // first call ("No implementation found for nativeSetSafeInsets") and crash
    // the app. Loading the same .so again from Java registers it for JNI
    // resolution; dlopen is idempotent, so NativeActivity's own load and the
    // single android_main entry are unaffected.
    static {
        System.loadLibrary("opentrail");
    }

    // Implemented in native (src/host/safe_area.c). Hands the window insets to the
    // layout so the game stays clear of the camera cutout and the gesture bar.
    // All four edges: sideways, the cutout and the bar sit on a side edge.
    private native void nativeSetSafeInsets(int top, int bottom, int left, int right,
                                            int cutoutLeft, int cutoutRight);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        goImmersive();
        // Re-read the cutout whenever the window insets settle (first layout,
        // rotation, foldables) and forward it to native.
        getWindow().getDecorView().setOnApplyWindowInsetsListener((v, insets) -> {
            pushSafeInsets();
            return insets;
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            goImmersive();
            pushSafeInsets();
        }
    }

    private void pushSafeInsets() {
        int top = 0, bottom = 0, left = 0, right = 0, cutLeft = 0, cutRight = 0;
        if (Build.VERSION.SDK_INT >= 28) {
            WindowInsets wi = getWindow().getDecorView().getRootWindowInsets();
            DisplayCutout dc = (wi != null) ? wi.getDisplayCutout() : null;
            if (dc != null) {
                top = dc.getSafeInsetTop();
                bottom = dc.getSafeInsetBottom();
                left = dc.getSafeInsetLeft();
                right = dc.getSafeInsetRight();
                // The top-edge cutout is the bounding rect flush with the top of
                // the screen; take its horizontal extent so native knows where
                // the camera sits.
                for (Rect r : dc.getBoundingRects()) {
                    if (r.top <= 0) {
                        cutLeft = r.left;
                        cutRight = r.right;
                        break;
                    }
                }
            }
        }
        // The cutout layout is cosmetic; never let a JNI/link hiccup crash the
        // game.
        try {
            nativeSetSafeInsets(top, bottom, left, right, cutLeft, cutRight);
        } catch (Throwable t) {
            // Native keeps zeros: the canvas fills the whole screen.
        }
    }

    private void goImmersive() {
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }
}
