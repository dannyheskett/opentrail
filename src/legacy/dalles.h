// DALLESUTIL.OBJ, the raft game's sprite extension (FLOAT loads it at $3000;
// spec section 29.7). While it is installed its CST and IMAGE replace the
// main extension's: its shift tables have a table for shift 0 too (seven
// tables, shift s at base + 256 s), and its IMAGE draws raw images only.
//
// Sprites: three slots, each a 16-byte record at $3A85 + 16 n, kept at those
// addresses in m->ram as the original did. PUTP draws a masked sprite (image n
// is the mask, n + 1 the picture), saving what it covers in the slot's buffer;
// before moving a sprite, the sprites it overlaps (by the original's own
// rectangle test) are taken off in order and put back after.
#ifndef OT_DALLES_H
#define OT_DALLES_H

#ifndef OT_MACH_TYPEDEF                 // C99 allows one typedef of a name
#define OT_MACH_TYPEDEF
typedef struct Mach Mach;
#endif

void dalles_install(Mach *m);                       // & RNH,"DALLESUTIL.OBJ": empty sprite slots
void dalles_cst(Mach *m, int addr);                 // & CST addr ($3AD8)
void dalles_image(Mach *m, int n, int x, int y);    // & IMAGE n,x,y ($3AEF)
void amp_dpb(Mach *m, int n, int addr);             // & DPB n,addr ($3B55): slot n's save buffer
void amp_kilp(Mach *m, int n);                      // & KILP n ($3B72)
void amp_putp(Mach *m, int slot, int image, int x, int y);  // & PUTP slot,image AT x,y ($3B7F)
void amp_rmovp(Mach *m, int n);                     // & RMOVP n ($3BBC): take sprite n off

#endif
