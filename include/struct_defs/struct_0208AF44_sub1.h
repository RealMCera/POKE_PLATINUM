#ifndef POKEPLATINUM_STRUCT_0208AF44_SUB1_H
#define POKEPLATINUM_STRUCT_0208AF44_SUB1_H

// Per-sprite animation state for the number-entry screen. Digits and dividers
// use offsetX/offsetY as a per-frame position delta and timer as a countdown;
// the keypad cursor (controls[1]) instead reuses offsetX/offsetY as its grid
// column/row.
typedef struct {
    s16 offsetX;
    s16 offsetY;
    u8 timer;
    u8 step;
} NumberEntrySpriteAnim;

#endif // POKEPLATINUM_STRUCT_0208AF44_SUB1_H
