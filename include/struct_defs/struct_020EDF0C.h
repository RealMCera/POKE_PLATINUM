#ifndef POKEPLATINUM_STRUCT_020EDF0C_H
#define POKEPLATINUM_STRUCT_020EDF0C_H

#include "functypes/funcptr_020EDF0C.h"
#include "functypes/funcptr_020EDF0C_1.h"
#include "functypes/funcptr_020EDF0C_2.h"
#include "functypes/funcptr_020EDF0C_3.h"

// The callbacks that implement one map-object movement type. A movement type's
// behavior is driven by these callbacks: `init` runs once when the behavior is
// selected, `update` runs on each movement step, `free` runs when the behavior
// is torn down, and `load` restores any state persisted across a map load.
// `movementType` records the movement type the entry is registered under; it is
// not read by the current code.
typedef struct MovementTypeCallbacks {
    int movementType;
    MovementTypeInitFunc init;
    MovementTypeUpdateFunc update;
    MovementTypeFreeFunc free;
    MovementTypeLoadFunc load;
} MovementTypeCallbacks;

#endif // POKEPLATINUM_STRUCT_020EDF0C_H
