#ifndef POKEPLATINUM_STRUCT_OV104_0223D3B0_H
#define POKEPLATINUM_STRUCT_OV104_0223D3B0_H

#include "overlay104/struct_ov104_0223D3B0_sub1.h"

// Saved state for the eight managed sprites owned by the Frontier graphics
// layer. spriteIDs holds the resource IDs to reload (0xFFFF = empty) and states
// holds each sprite's saved state.
typedef struct {
    u16 spriteIDs[8];
    FrontierSpriteState states[8];
} FrontierSpriteStateBuffer;

#endif // POKEPLATINUM_STRUCT_OV104_0223D3B0_H
