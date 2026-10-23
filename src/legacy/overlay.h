// Dialogs of the port's own, drawn over the Apple II screen the way the
// original draws its message boxes (overlay.c).
#ifndef OT_OVERLAY_H
#define OT_OVERLAY_H

#include "mach.h"

// The shown hi-res page with the quit dialog over it ("Quit the game? (Y/N)"),
// drawn on a copy of the machine: m itself is not changed. 8192 bytes.
const uint8_t *overlay_quit(const Mach *m);

#endif
