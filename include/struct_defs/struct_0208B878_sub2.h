#ifndef POKEPLATINUM_STRUCT_0208B878_SUB2_H
#define POKEPLATINUM_STRUCT_0208B878_SUB2_H

#include "sprite_system.h"

// One sprite of a Vs. Recorder ring. Each sprite flies toward its target
// position and then settles into a shared orbit around the ring's center.
typedef struct {
    ManagedSprite *sprite;
    // Target position the sprite approaches before entering the orbit.
    s16 targetX;
    s16 targetY;
    // 0 while approaching the target, 1 once orbiting.
    int state;
    // Current orbit angle in degrees (0-719).
    int angle;
} VsRecorderRingSprite;

#endif // POKEPLATINUM_STRUCT_0208B878_SUB2_H
