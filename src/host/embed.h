// The original's files (assets/*.bin) and modern mode's pictures
// (assets/modern/*.png), compiled into the game by tools/art.py embed, so the
// binary needs nothing beside it.
#ifndef OT_EMBED_H
#define OT_EMBED_H

typedef struct {
    const char *name;               // the DOS file name, e.g. "L0.PCK"
    const unsigned char *data;
    int size;
} EmbeddedFile;

extern const EmbeddedFile EMBEDDED[];
extern const int EMBEDDED_COUNT;
extern const EmbeddedFile EMBEDDED_ART[];   // by id, e.g. "scene_title" (build/embed_art.c)
extern const int EMBEDDED_ART_COUNT;

#endif
