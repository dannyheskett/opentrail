// Per-game settings for the family's shared host files (recorder.c).
#ifndef OT_PLATFORM_H
#define OT_PLATFORM_H

// The game's name: the recording file prefix.
#define GAME_NAME "opentrail"

// Recording size, both multiples of 16 for the H.264 encoder: legacy mode's
// picture at exactly 2x (1120 x 768), modern mode's at 1280 x 720. Set by
// main.c for the mode before recording starts.
extern int rec_w, rec_h;
#define REC_W rec_w
#define REC_H rec_h

#endif
