// Modern mode's presenter (view_modern.c).
#ifndef OT_VIEW_MODERN_H
#define OT_VIEW_MODERN_H

#include "host.h"
#include "modern.h"

HostPresenter view_modern(MShell *ms);

// Modern mode's pictures and text font, for the mode chooser (chooser.c).
struct Texture;
const struct Texture *view_modern_tex(const char *id);   // NULL when missing
void view_modern_text(const char *s, float x, float y, int size, unsigned rgba);

#endif
