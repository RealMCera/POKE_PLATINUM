#ifndef POKEPLATINUM_STRUCT_0208B878_SUB1_H
#define POKEPLATINUM_STRUCT_0208B878_SUB1_H

#include "narc.h"
#include "palette.h"
#include "sprite_system.h"

// Rendering handles shared by every sprite in a Vs. Recorder ring.
typedef struct {
    NARC *narc;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    PaletteData *paletteData;
} VsRecorderRingResources;

#endif // POKEPLATINUM_STRUCT_0208B878_SUB1_H
