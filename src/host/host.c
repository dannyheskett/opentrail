#include "host.h"
#include "core.h"
#include "platform.h"
#include "recorder.h"
#include "music.h"
#include <raylib.h>
#include <rlgl.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#define RATE HOST_AUDIO_RATE

static Mach *s_m;
static HostPresenter s_pres;
static int s_quit, s_quit_req;
static jmp_buf s_exit;               // host_run: unwind the game when the window closes

int rec_w = 1120, rec_h = 768;       // platform.h: the recording size
static RenderTexture2D s_rec;        // the frame as recorded (REC_W x REC_H, what the encoder reads)
static RenderTexture2D s_rec_super;  // REC_SS x that, drawn into then minified (the family's present.c)
static int s_rec_w, s_rec_h, s_rec_pass;
#define REC_SS 2                     // capture supersampling factor

int host_recording_pass(void) { return s_rec_pass; }

// The sound (core.c): raylib's audio thread drains the ring the game fills.
static AudioStream s_stream;
static int s_audio;

static void audio_cb(void *buffer, unsigned int frames) { core_audio_read(buffer, (int)frames); }

void host_set_music(MusicSynth *mu) { core_set_music(mu); }

static int is_modal(void) { return s_pres.modal && s_pres.modal(s_pres.ctx); }

// raylib keys -> Apple II key codes (bit 7 added by mach_key_push).
static void pump_keys(void) {
    int modal = is_modal();
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int k;
    while ((k = GetKeyPressed()) != 0) {
        int code = -1;
        int alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        if ((k == KEY_ENTER || k == KEY_KP_ENTER) && alt) { ToggleBorderlessWindowed(); continue; }
        if (k == KEY_F12) { recorder_toggle(); continue; }
        if (s_pres.key && s_pres.key(s_pres.ctx, k)) continue;          // the view's own keys
        switch (k) {
        case KEY_ENTER: case KEY_KP_ENTER: code = 0x0D; break;
        case KEY_ESCAPE: code = 0x1B; break;
        case KEY_BACKSPACE: case KEY_LEFT: code = 0x08; break;
        case KEY_RIGHT: code = 0x15; break;
        case KEY_UP: code = 0x0B; break;
        case KEY_DOWN: code = 0x0A; break;
        case KEY_TAB: code = 0x09; break;
        case KEY_DELETE: code = 0x7F; break;
        default:
            if (ctrl && k >= KEY_A && k <= KEY_Z) code = k - KEY_A + 1;   // Ctrl-A .. Ctrl-Z
            break;
        }
        if (code >= 0) mach_key_push(s_m, (uint8_t)code);
    }
    int ch, drop = modal || is_modal();                 // typed into the view's dialog
    while ((ch = GetCharPressed()) != 0)
        if (!drop && !ctrl && ch >= 0x20 && ch < 0x7F) mach_key_push(s_m, (uint8_t)ch);
}

static void draw_screen(void) {
    if (s_pres.prepare) s_pres.prepare(s_pres.ctx);
    BeginDrawing();
    ClearBackground(BLACK);
    if (s_pres.draw) s_pres.draw(s_pres.ctx, GetScreenWidth(), GetScreenHeight());
    if (recorder_active()) {                         // a small REC mark at the top left: the window only
        int w = GetScreenWidth(), h = GetScreenHeight();
        int ref = w > h ? w : h;
        int fs = ref / 64, r = fs / 3;
        DrawCircle(fs / 2 + r, fs / 2 + fs / 2, (float)r, (Color){ 230, 41, 55, 255 });
        DrawText("REC", fs / 2 + r * 3, fs / 2, fs, (Color){ 230, 41, 55, 255 });
    }
    EndDrawing();
}

// The frame just drawn, again at the recording size, into the video.
// As the family's present.c does it: the frame at REC_SS x the capture size,
// then minified with bilinear filtering (the anti-aliasing; the views scale by
// fractions in this pass, so modern's canvas fills the video), into the
// canvas the encoder reads.
static void capture(void) {
    if (!recorder_active() || !s_pres.draw) return;
    if (s_rec_w != rec_w || s_rec_h != rec_h) {
        if (s_rec_w) { UnloadRenderTexture(s_rec); UnloadRenderTexture(s_rec_super); }
        s_rec = LoadRenderTexture(rec_w, rec_h);
        s_rec_super = LoadRenderTexture(REC_SS * rec_w, REC_SS * rec_h);
        SetTextureFilter(s_rec_super.texture, TEXTURE_FILTER_BILINEAR);
        s_rec_w = rec_w; s_rec_h = rec_h;
    }
    // 1) The frame at REC_SS x. Blend colour normally but keep the target
    //    opaque: with the default blend every translucent draw lowers the
    //    texture's alpha, and the minify below would show those pixels faded.
    BeginTextureMode(s_rec_super);
    ClearBackground(BLACK);
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    s_rec_pass = 1;
    s_pres.draw(s_pres.ctx, REC_SS * rec_w, REC_SS * rec_h);
    s_rec_pass = 0;
    EndBlendMode();
    EndTextureMode();
    // 2) Minify into the encoder's canvas; the negative source height flips the
    //    bottom-up render texture upright.
    BeginTextureMode(s_rec);
    Rectangle src = { 0, 0, (float)(REC_SS * rec_w), -(float)(REC_SS * rec_h) };
    Rectangle dst = { 0, 0, (float)rec_w, (float)rec_h };
    DrawTexturePro(s_rec_super.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndTextureMode();
    recorder_capture(&s_rec);
}

// WindowShouldClose comes between the frame and the keys: EndDrawing empties
// raylib's key queue, and on the web the keys arrive only while
// WindowShouldClose yields to the browser, so read after it they are kept.
static void idle(void *ctx) {
    (void)ctx;
    core_pump_audio(s_m);
    draw_screen();                                   // EndDrawing paces to 60 Hz
    capture();                                       // one video frame per frame of game time
    int closed = WindowShouldClose();
    pump_keys();
    if (s_pres.input) s_pres.input(s_pres.ctx);
    while (is_modal() && !s_quit_req && !closed) {   // the game waits
        draw_screen();
        closed = WindowShouldClose();
        pump_keys();
        if (s_pres.input) s_pres.input(s_pres.ctx);  // the dialog's or the touch menu's taps
    }
    if (s_quit_req || closed) { s_quit = 1; longjmp(s_exit, 1); }
}

void host_init(Mach *m, const char *title, int w, int h) {
    s_m = m;
#if defined(PLATFORM_ANDROID)
    // The family's: immersive fullscreen at the display's own size (0 x 0); a
    // fixed size is scaled up, and the desktop's window calls do not apply.
    (void)w; (void)h;
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT);
    InitWindow(0, 0, title);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(w, h, title);
    SetWindowMinSize(560, 384);
#endif
    SetExitKey(KEY_NULL);                            // Escape belongs to the game
    SetTargetFPS(60);

    InitAudioDevice();
    s_audio = IsAudioDeviceReady();
    if (s_audio) {
        SetAudioStreamBufferSizeDefault(1024);
        s_stream = LoadAudioStream(RATE, 16, 2);
        SetAudioStreamCallback(s_stream, audio_cb);
        PlayAudioStream(s_stream);
    }
    core_audio_init(m, RATE);
    m->idle = idle;
    m->idle_ctx = NULL;
    m->load = host_load;
}

void host_init_hidden(Mach *m, int w, int h, void (*idle_fn)(void *ctx), void *ctx) {
    s_m = m;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(w, h, "OpenTrail harness");
    SetExitKey(KEY_NULL);
    m->idle = idle_fn;
    m->idle_ctx = ctx;
    m->load = host_load;
}

void host_render(uint32_t *px, int w, int h) {
    static RenderTexture2D rt;
    static int rw, rh;
    if (rw != w || rh != h) {
        if (rw) UnloadRenderTexture(rt);
        rt = LoadRenderTexture(w, h);
        rw = w; rh = h;
    }
    if (s_pres.prepare) s_pres.prepare(s_pres.ctx);
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    if (s_pres.draw) s_pres.draw(s_pres.ctx, w, h);
    EndTextureMode();
    Image img = LoadImageFromTexture(rt.texture);   // bottom row first
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const uint32_t *src = img.data;
    for (int y = 0; y < h; y++) memcpy(px + (size_t)y * w, src + (size_t)(h - 1 - y) * w, (size_t)w * 4);
    UnloadImage(img);
}

int host_view_key(int k) { return s_pres.key ? s_pres.key(s_pres.ctx, k) : 0; }
int host_view_modal(void) { return is_modal(); }

void host_close(void) {
    recorder_stop();
    if (s_rec_w) { UnloadRenderTexture(s_rec); UnloadRenderTexture(s_rec_super); }
    if (s_audio) { UnloadAudioStream(s_stream); CloseAudioDevice(); }
    CloseWindow();
}

int host_quit_requested(void) { return s_quit; }
void host_quit(void) { s_quit_req = 1; }

void host_run(void (*program)(Mach *m, void *arg), void *arg) {
    if (setjmp(s_exit)) return;
    program(s_m, arg);
}
void host_set_presenter(HostPresenter p) { s_pres = p; }

