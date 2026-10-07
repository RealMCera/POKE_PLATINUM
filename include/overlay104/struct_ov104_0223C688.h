#ifndef POKEPLATINUM_STRUCT_OV104_0223C688_H
#define POKEPLATINUM_STRUCT_OV104_0223C688_H

// A loaded object-event graphics resource, cached by the Battle Frontier app so
// that it can be reloaded after a sub-app returns. gfxID is an
// enum ObjectEventGfx value, or 0xFFFF when the slot is empty.
typedef struct {
    u16 gfxID;
    u8 unk_02;
} FrontierObjectGfx;

#endif // POKEPLATINUM_STRUCT_OV104_0223C688_H
