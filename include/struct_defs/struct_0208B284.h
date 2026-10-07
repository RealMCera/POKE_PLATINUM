#ifndef POKEPLATINUM_STRUCT_0208B284_H
#define POKEPLATINUM_STRUCT_0208B284_H

#include "constants/heap.h"

// Construction parameters for a Vs. Recorder sprite ring.
typedef struct {
    // Number of sprites in the ring (12 in the Vs. Recorder viewer).
    int count;
    enum HeapID heapID;
    // GX VRAM type the sprites are drawn to (NNS_G2D_VRAM_TYPE_2DMAIN or
    // NNS_G2D_VRAM_TYPE_2DSUB).
    int vramType;
    // Vs. Recorder mode; 0 selects the player's chosen color palette, any
    // other value falls back to a fixed palette.
    int mode;
    // Subscreen offset applied when reading/writing sprite positions.
    fx32 subscreenOffset;
} VsRecorderRingConfig;

#endif // POKEPLATINUM_STRUCT_0208B284_H
