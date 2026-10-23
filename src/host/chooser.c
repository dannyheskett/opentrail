// The mode chooser: with neither --classic nor --modern on the command line,
// the game asks which to play before anything else. Drawn on modern mode's
// 384 x 252 canvas (its title picture and its font), scaled up by whole
// numbers; 1 or 2, the arrows and Return, or a click choose, and Esc or
// closing the window quits.
#include "chooser.h"
#include "view_modern.h"
#include <raylib.h>

#define CW 384
#define CH 252
#define ART_H 216

static const char *const NAME[2] = { "1. Classic", "2. Modern" };
static const char *const ABOUT[2] = { "The Apple II original, 1985", "New pictures and music" };

int choose_mode(void) {
    int sel = 1;
    while (!WindowShouldClose()) {
        int w = GetScreenWidth(), h = GetScreenHeight();
        int k = w / CW < h / CH ? w / CW : h / CH;
        if (k < 1) k = 1;
        float ox = (w - CW * k) / 2.0f, oy = (h - CH * k) / 2.0f;
        Rectangle box[2];
        for (int i = 0; i < 2; i++)
            box[i] = (Rectangle){ ox + 72 * k, oy + (112 + i * 48) * k, 240.0f * k, 36.0f * k };
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < 2; i++)
            if (CheckCollisionPointRec(mouse, box[i])) {
                sel = i;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return i;
            }
        int key;
        while ((key = GetKeyPressed()) != 0) {
            if (key == KEY_ONE || key == KEY_KP_1) return 0;
            if (key == KEY_TWO || key == KEY_KP_2) return 1;
            if (key == KEY_UP || key == KEY_DOWN || key == KEY_TAB) sel = !sel;
            if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE) return sel;
            if (key == KEY_ESCAPE) return -1;
        }
        BeginDrawing();
        ClearBackground(BLACK);
        const Texture *bg = view_modern_tex("scene_title");
        if (bg) DrawTexturePro(*bg, (Rectangle){ 0, 0, (float)bg->width, (float)bg->height },
                               (Rectangle){ ox, oy, (float)CW * k, (float)ART_H * k }, (Vector2){ 0, 0 }, 0, WHITE);
        DrawRectangle((int)ox, (int)oy, CW * k, CH * k, (Color){ 0, 0, 0, 150 });
        view_modern_text("OPEN TRAIL", ox + (CW - 10 * 16) / 2.0f * k, oy + 44 * k, 16 * k, 0xF0D890FF);
        view_modern_text("How would you like to play?", ox + (CW - 27 * 8) / 2.0f * k, oy + 82 * k, 8 * k, 0xE6E1C8FF);
        for (int i = 0; i < 2; i++) {
            Rectangle r = box[i];
            DrawRectangleRec(r, i == sel ? (Color){ 70, 52, 30, 235 } : (Color){ 20, 16, 12, 220 });
            DrawRectangleLinesEx(r, (float)k, i == sel ? (Color){ 240, 216, 144, 255 } : (Color){ 120, 100, 70, 255 });
            view_modern_text(NAME[i], r.x + 10 * k, r.y + 7 * k, 8 * k, 0xFFFFFFFF);
            view_modern_text(ABOUT[i], r.x + 10 * k, r.y + 21 * k, 8 * k, 0xA0C8A0FF);
        }
        view_modern_text("Esc: quit", ox + (CW - 9 * 8) / 2.0f * k, oy + 232 * k, 8 * k, 0x9A9080FF);
        EndDrawing();
    }
    return -1;
}
