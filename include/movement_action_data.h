#ifndef POKEPLATINUM_MOVEMENT_ACTION_DATA_H
#define POKEPLATINUM_MOVEMENT_ACTION_DATA_H

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"
#include "struct_defs/struct_020EDF0C.h"

// Per-step callbacks for each movement action, indexed by enum MovementAction.
extern int (*const *const gMovementActionFuncs[MAX_MOVEMENT_ACTION])(MapObject *);
// Callback sets for each map-object movement type, indexed by MOVEMENT_TYPE_*.
extern const MovementTypeCallbacks *const gMovementTypeCallbacks[];
// Direction groups for movement actions that come in four facing variants. Each
// entry is a DIR_*-indexed array of movement actions, terminated by NULL.
extern const int *const gMovementActionCodes[];

#endif // POKEPLATINUM_MOVEMENT_ACTION_DATA_H
