#ifndef POKEPLATINUM_STRUCT_OV104_0223D3B0_SUB1_H
#define POKEPLATINUM_STRUCT_OV104_0223D3B0_SUB1_H

// A snapshot of one managed sprite's state, saved before a sub-app is launched
// and restored afterwards. valid marks the slot as populated.
typedef struct {
    s16 x;
    s16 y;
    u8 resourceID;
    u8 activeAnim;
    u16 animationFrame : 13;
    u16 visible : 1;
    u16 drawFlag : 1;
    u16 valid : 1;
} FrontierSpriteState;

#endif // POKEPLATINUM_STRUCT_OV104_0223D3B0_SUB1_H
