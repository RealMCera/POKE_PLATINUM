#ifndef POKEPLATINUM_MAP_OBJECT_MOVEMENT_PATTERNS_H
#define POKEPLATINUM_MAP_OBJECT_MOVEMENT_PATTERNS_H

#include "struct_decls/map_object.h"

// Movement-type behaviors for overworld map objects. Each behavior is a set of
// init/update/free callbacks registered in the gMovementTypeCallbacks table in
// movement_action_data.c and selected by the object's movement type. The state for each
// behavior lives in the map object's opaque unk_D8 scratch buffer.
//
// This module covers the "standard" behaviors: looking around, wandering,
// rotating in place, walking back and forth, and walking fixed direction
// cycles. The more complex NPC behaviors (following, disguises, walking with
// the player) live in map_object_movement.c.

void MapObjectMovement_LookAround_Init(MapObject *param0);
void MapObjectMovement_LookNorthAndWest_Init(MapObject *param0);
void MapObjectMovement_LookNorthAndEast_Init(MapObject *param0);
void MapObjectMovement_LookSouthAndWest_Init(MapObject *param0);
void MapObjectMovement_LookSouthAndEast_Init(MapObject *param0);
void MapObjectMovement_LookNorthSouthAndWest_Init(MapObject *param0);
void MapObjectMovement_LookNorthSouthAndEast_Init(MapObject *param0);
void MapObjectMovement_LookNorthWestAndEast_Init(MapObject *param0);
void MapObjectMovement_LookSouthWestAndEast_Init(MapObject *param0);
void MapObjectMovement_LookNorthAndSouth_Init(MapObject *param0);
void MapObjectMovement_LookWestAndEast_Init(MapObject *param0);
void MapObjectMovement_LookAround_Update(MapObject *param0);
void MapObjectMovement_LookAround_Free(MapObject *param0);
void MapObjectMovement_WanderAround_Init(MapObject *param0);
void MapObjectMovement_WanderNorthAndSouth_Init(MapObject *param0);
void MapObjectMovement_WanderWestAndEast_Init(MapObject *param0);
void MapObjectMovement_WanderWestAndEastAlt_Init(MapObject *param0);
void MapObjectMovement_Wander_Update(MapObject *param0);
void MapObjectMovement_LookDirection_Update(MapObject *param0);
void MapObjectMovement_LookNorth_Init(MapObject *param0);
void MapObjectMovement_LookSouth_Init(MapObject *param0);
void MapObjectMovement_LookWest_Init(MapObject *param0);
void MapObjectMovement_LookEast_Init(MapObject *param0);
void MapObjectMovement_RotateCounterclockwise_Init(MapObject *param0);
void MapObjectMovement_RotateClockwise_Init(MapObject *param0);
void MapObjectMovement_Rotate_Update(MapObject *param0);
void MapObjectMovement_Spin_Init(MapObject *param0);
void MapObjectMovement_Spin_Update(MapObject *param0);
void MapObjectMovement_WalkBackAndForth_Init(MapObject *param0);
void MapObjectMovement_WalkBackAndForth_Update(MapObject *param0);
void MapObjectMovement_WalkNorthEastWestSouth_Init(MapObject *param0);
void MapObjectMovement_WalkEastWestSouthNorth_Init(MapObject *param0);
void MapObjectMovement_WalkSouthNorthEastWest_Init(MapObject *param0);
void MapObjectMovement_WalkWestSouthNorthEast_Init(MapObject *param0);
void MapObjectMovement_WalkWestEastSouthNorth_Init(MapObject *param0);
void MapObjectMovement_WalkNorthWestEastSouth_Init(MapObject *param0);
void MapObjectMovement_WalkSouthNorthWestEast_Init(MapObject *param0);
void MapObjectMovement_WalkEastSouthNorthWest_Init(MapObject *param0);
void MapObjectMovement_WalkWestNorthSouthEast_Init(MapObject *param0);
void MapObjectMovement_WalkNorthSouthEastWest_Init(MapObject *param0);
void MapObjectMovement_WalkEastWestNorthSouth_Init(MapObject *param0);
void MapObjectMovement_WalkSouthEastWestNorth_Init(MapObject *param0);
void MapObjectMovement_WalkEastNorthSouthWest_Init(MapObject *param0);
void MapObjectMovement_WalkNorthSouthWestEast_Init(MapObject *param0);
void MapObjectMovement_WalkWestEastNorthSouth_Init(MapObject *param0);
void MapObjectMovement_WalkSouthWestEastNorth_Init(MapObject *param0);
void MapObjectMovement_WalkPattern_Update(MapObject *param0);
void MapObjectMovement_WalkNorthWestSouthEast_Init(MapObject *param0);
void MapObjectMovement_WalkSouthEastNorthWest_Init(MapObject *param0);
void MapObjectMovement_WalkWestSouthEastNorth_Init(MapObject *param0);
void MapObjectMovement_WalkEastNorthWestSouth_Init(MapObject *param0);
void MapObjectMovement_WalkNorthEastSouthWest_Init(MapObject *param0);
void MapObjectMovement_WalkSouthWestNorthEast_Init(MapObject *param0);
void MapObjectMovement_WalkWestNorthEastSouth_Init(MapObject *param0);
void MapObjectMovement_WalkEastSouthWestNorth_Init(MapObject *param0);
void MapObjectMovement_WalkPatternPlayer_Update(MapObject *param0);

#endif // POKEPLATINUM_MAP_OBJECT_MOVEMENT_PATTERNS_H
