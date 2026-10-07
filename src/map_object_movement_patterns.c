#include "map_object_movement_patterns.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"

#include "field/field_system.h"

#include "map_object.h"
#include "map_object_move.h"
#include "math_util.h"
#include "player_avatar.h"
#include "player_move.h"
#include "trainer_encounter.h"
#include "map_object_animation.h"

// This module implements the "standard" movement-type behaviors selected
// through the gMovementTypeCallbacks table in movement_action_data.c:
//   - MOVEMENT_TYPE_LOOK_*: turn to face a random allowed direction every few
//     seconds, or face a running player who comes near.
//   - MOVEMENT_TYPE_WANDER_*: take a random step in an allowed direction,
//     pausing between steps.
//   - MOVEMENT_TYPE_LOOK_NORTH/SOUTH/WEST/EAST: turn to one fixed direction.
//   - MOVEMENT_TYPE_ROTATE_*: turn one quarter-turn at a time.
//   - MOVEMENT_TYPE_VS_SEEKER_SPIN: spin in place, reversing direction each
//     full turn.
//   - MOVEMENT_TYPE_WALK_BACK_AND_FORTH: walk until blocked, then reverse.
//   - MOVEMENT_TYPE_WALK_*: walk a fixed cycle of directions.
// Each behavior keeps its state in the map object's opaque unk_D8 scratch
// buffer and drives a small callback state machine from its update function.
//
// Directions are the DIR_* values: 0 = north, 1 = south, 2 = west, 3 = east.

// Entry in sDirectionSets: a direction set ID and the ordered list of
// directions (terminated by -1) that the ID selects.
typedef struct {
    int id;
    const int *directions;
} DirectionSetEntry;

// Axis-aligned rectangle, in map tiles, that a wandering object may move
// within. Derived from the object's movement type.
typedef struct {
    int minX;
    int minZ;
    int maxX;
    int maxZ;
} MovementRange;

// Spin state shared by the trainer types that rotate while walking
// (TRAINER_TYPE_SPIN_COUNTERCLOCKWISE / TRAINER_TYPE_SPIN_CLOCKWISE).
typedef struct {
    s8 savedDir; // facing direction before the spin step
    s8 spinDirection; // 0 = counterclockwise, 1 = clockwise
    u8 lockWasSet; // whether MAP_OBJ_STATUS_LOCK_DIR was already set
    u8 unused;
} SpinningTrainerData;

// State for the MOVEMENT_TYPE_LOOK_* behaviors: periodically turn to a random
// direction from directionSetID, or face a running player who comes near.
typedef struct {
    u16 state; // index into the behavior's state machine
    s16 timer; // frames until the next random turn
    int directionSetID; // direction set to choose from
} LookAroundData;

// How a wandering object reacts when the tile ahead is blocked.
enum WanderCollisionMode {
    WANDER_COLLISION_STOP_ON_ANY = 0, // stop on any collision
    WANDER_COLLISION_STOP_ON_RANGE, // also require the step to stay in range
    WANDER_COLLISION_STOP_ON_OUT_OF_RANGE, // ignore other objects, stop only at map bounds
};

// State for the MOVEMENT_TYPE_WANDER_* behaviors: walk in a random direction
// from directionSetID, pausing between steps.
typedef struct {
    s16 state; // index into the behavior's state machine
    s16 timer; // frames until the next step
    int collisionMode; // how to react to a blocked step (enum WanderCollisionMode)
    int movementAction; // walk action used for each step
    int directionSetID; // direction set to choose from
} WanderData;

// State for the MOVEMENT_TYPE_LOOK_NORTH/SOUTH/WEST/EAST behaviors: turn to a
// fixed direction once.
typedef struct {
    int dir; // direction to face
    int state; // index into the behavior's state machine
} LookDirectionData;

// State for the MOVEMENT_TYPE_ROTATE_* and MOVEMENT_TYPE_VS_SEEKER_SPIN
// behaviors: turn one quarter-turn at a time, pausing between turns.
typedef struct {
    s8 spinDir; // 2 = counterclockwise order, 3 = clockwise order
    s8 unused1;
    s8 state; // index into the behavior's callback table
    s8 unused3;
    int timer; // frames until the next turn
} RotateData;

// State for MOVEMENT_TYPE_WALK_BACK_AND_FORTH: walk until blocked, then
// reverse and walk back.
typedef struct {
    s16 state; // index into the behavior's callback table
    s16 reversed; // whether the object is currently walking back
    SpinningTrainerData spinData; // spin state for spinning trainers
} WalkBackAndForthData;

// State for the MOVEMENT_TYPE_WALK_* cycle behaviors: walk one tile at a time
// through the directions in directionSetID.
typedef struct {
    u8 state; // index into the behavior's callback table
    u8 stepIndex; // current index into the direction set
    u8 initialStep; // step index at which the object is at its start tile
    u8 axis; // 0 = compare x, 1 = compare z
    int directionSetID; // direction set to walk through
    SpinningTrainerData spinData; // spin state for spinning trainers
} WalkPatternData;

// State for the MOVEMENT_TYPE_WALK_* behaviors that also react to the player
// (trainer type TRAINER_TYPE_UNK_010). Like WalkPatternData, but the step
// index can also be walked backwards.
typedef struct {
    u8 state; // index into the behavior's callback table
    s8 stepIndex; // current index into the direction set
    u8 initialStep; // step index at which the object is at its start tile
    u8 axis; // 0 = compare x, 1 = compare z
    u8 directionSetID; // direction set to walk through
    u8 reverse; // 1 = advance stepIndex backwards
    u8 unused6;
    u8 unused7;
    SpinningTrainerData spinData; // spin state for spinning trainers
} WalkPatternPlayerData;

static void MapObjectMovement_InitLookAround(MapObject *mapObj, int param1);
static void MapObjectMovement_InitWander(MapObject *mapObj, int param1, int param2, int param3);
static int DirectionSet_GetLength(const int *param0, int param1);
static int DirectionSet_GetRandom(const int *param0, int param1);
static int DirectionSet_GetRandomById(int param0, int param1);
static const int *DirectionSet_GetById(int param0);
static int MapObjectMovement_GetDirToRunningPlayer(MapObject *mapObj);
static int MapObjectMovement_ChooseDirToFacePlayer(MapObject *mapObj, int param1, int param2);
static void MapObjectMovement_GetMovementRange(MapObject *mapObj, MovementRange *param1);
static int MapObjectMovement_IsInMovementRange(MapObject *mapObj, int param1);
static void MapObjectMovement_InitLookDirection(MapObject *mapObj, int param1);
static void MapObjectMovement_InitRotate(MapObject *mapObj, int param1);
static void MapObjectMovement_InitWalkPattern(MapObject *mapObj, int param1, int param2, int param3);
static void MapObjectMovement_InitWalkPatternPlayer(MapObject *mapObj, int param1, int param2, int param3);
static int MapObjectMovement_IsSpinningTrainer(MapObject *mapObj);
static void MapObjectMovement_InitSpinDirection(MapObject *mapObj, SpinningTrainerData *param1);
static void MapObjectMovement_AdvanceSpinDirection(MapObject *mapObj, SpinningTrainerData *param1);
static void MapObjectMovement_RestoreSpinLock(MapObject *mapObj, SpinningTrainerData *param1);

int (*const sRotateCallbacks[])(MapObject *, RotateData *);
int (*const sSpinCallbacks[])(MapObject *, RotateData *);
int (*const sWalkBackAndForthCallbacks[])(MapObject *, WalkBackAndForthData *);
int (*const sWalkPatternCallbacks[])(MapObject *, WalkPatternData *);
int (*const sWalkPatternPlayerCallbacks[])(MapObject *, WalkPatternPlayerData *);
static const int sSpinDirectionOrders[2][4];
const int sLookAroundDelays[];
const int sDirectionsAll[];
const int sDirectionsNorthWest[];
const int sDirectionsNorthEast[];
const int sDirectionsSouthWest[];
const int sDirectionsSouthEast[];
const int sDirectionsNorthSouthWest[];
const int sDirectionsNorthSouthEast[];
const int sDirectionsNorthWestEast[];
const int sDirectionsSouthWestEast[];
const int sDirectionsNorthSouth[];
const int sDirectionsWestEast[];
const int sDirectionsAll2[];
const int sDirectionsNorthSouth2[];
const int sDirectionsWestEast2[];
const int sWalkOrderNorthEastWestSouth[];
const int sWalkOrderEastWestSouthNorth[];
const int sWalkOrderSouthNorthEastWest[];
const int sWalkOrderWestSouthNorthEast[];
const int sWalkOrderWestEastSouthNorth[];
const int sWalkOrderWestEastSouthNorth2[];
const int sWalkOrderSouthNorthWestEast[];
const int sWalkOrderEastSouthNorthWest[];
const int sWalkOrderWestNorthSouthEast[];
const int sWalkOrderNorthSouthEastWest[];
const int sWalkOrderEastWestNorthSouth[];
const int sWalkOrderSouthEastWestNorth[];
const int sWalkOrderEastNorthSouthWest[];
const int sWalkOrderNorthSouthWestEast[];
const int sWalkOrderWestEastNorthSouth[];
const int sWalkOrderSouthWestEastNorth[];
const int sWalkOrderNorthWestSouthEast[];
const int sWalkOrderSouthEastNorthWest[];
const int sWalkOrderWestSouthEastNorth[];
const int sWalkOrderEastNorthWestSouth[];
const int sWalkOrderNorthEastSouthWest[];
const int sWalkOrderSouthWestNorthEast[];
const int sWalkOrderWestNorthEastSouth[];
const int sWalkOrderEastSouthWestNorth[];
static const DirectionSetEntry sDirectionSets[40];
const int sPlayerReactiveMovementTypes[];

// Shared initializer for the look-around behaviors. param1 is the direction
// set the object may turn to; the first turn happens after a random delay.
static void MapObjectMovement_InitLookAround(MapObject *mapObj, int param1)
{
    LookAroundData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(LookAroundData)));
    v0->timer = DirectionSet_GetRandom(sLookAroundDelays, -1);
    v0->directionSetID = param1;

    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);
}

void MapObjectMovement_LookAround_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 0);
}

void MapObjectMovement_LookNorthAndWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 1);
}

void MapObjectMovement_LookNorthAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 2);
}

void MapObjectMovement_LookSouthAndWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 3);
}

void MapObjectMovement_LookSouthAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 4);
}

void MapObjectMovement_LookNorthSouthAndWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 5);
}

void MapObjectMovement_LookNorthSouthAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 6);
}

void MapObjectMovement_LookNorthWestAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 7);
}

void MapObjectMovement_LookSouthWestAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 8);
}

void MapObjectMovement_LookNorthAndSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 9);
}

void MapObjectMovement_LookWestAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookAround(mapObj, 10);
}

// Faces a running player if one is in range; otherwise counts down and turns
// to a random direction from the object's set.
void MapObjectMovement_LookAround_Update(MapObject *mapObj)
{
    LookAroundData *v0 = MapObject_GetUnkD8(mapObj);
    int v1 = MapObjectMovement_ChooseDirToFacePlayer(mapObj, v0->directionSetID, -1);

    if (v1 != -1) {
        MapObject_TryFace(mapObj, v1);
    } else {
        switch (v0->state) {
        case 0:
            v0->timer--;

            if (v0->timer <= 0) {
                v0->timer = DirectionSet_GetRandom(sLookAroundDelays, -1);
                MapObject_TryFace(mapObj, DirectionSet_GetRandomById(v0->directionSetID, -1));
            }
        }
    }

    MapObject_UpdateCoords(mapObj);
}

void MapObjectMovement_LookAround_Free(MapObject *mapObj)
{
    return;
}

void MapObjectMovement_WanderAround_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWander(mapObj, 0xc, 11, WANDER_COLLISION_STOP_ON_ANY);
}

void MapObjectMovement_WanderNorthAndSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWander(mapObj, 0xc, 12, WANDER_COLLISION_STOP_ON_ANY);
}

void MapObjectMovement_WanderWestAndEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWander(mapObj, 0xc, 13, WANDER_COLLISION_STOP_ON_ANY);
}

// MOVEMENT_TYPE_067: same direction set as MOVEMENT_TYPE_WANDER_WEST_AND_EAST,
// but only stops when a step leaves the map bounds (other objects are ignored).
void MapObjectMovement_WanderWestAndEastAlt_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWander(mapObj, 0xc, 13, WANDER_COLLISION_STOP_ON_OUT_OF_RANGE);
}

// Shared initializer for the wander behaviors. param1 is the walk action used
// for each step, param2 the direction set to choose from, and param3 how to
// react to a blocked step (enum WanderCollisionMode).
static void MapObjectMovement_InitWander(MapObject *mapObj, int param1, int param2, int param3)
{
    WanderData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WanderData)));

    v0->collisionMode = param3;
    v0->movementAction = param1;
    v0->directionSetID = param2;

    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);
}

// Wander state machine: face north, wait, pick a random allowed direction,
// then walk one tile (or give up and restart if the step is blocked).
void MapObjectMovement_Wander_Update(MapObject *mapObj)
{
    int v0;
    WanderData *v1 = MapObject_GetUnkD8(mapObj);

    switch (v1->state) {
    case 0:
        MapObject_ClearStatus1(mapObj);
        MapObject_SetEndMovementOff(mapObj);

        v0 = MapObject_GetFacingDir(mapObj);
        v0 = MovementAction_TurnActionTowardsDir(v0, MOVEMENT_ACTION_FACE_NORTH);

        LocalMapObj_SetMovementAction(mapObj, v0);

        v1->state++;
        break;
    case 1:
        if (LocalMapObj_RunMovementAction(mapObj) == 0) {
            break;
        }

        v1->timer = DirectionSet_GetRandom(sLookAroundDelays, -1);
        v1->state++;
    case 2:
        v1->timer--;

        if (v1->timer) {
            break;
        }

        v1->state++;
    case 3:
        v0 = DirectionSet_GetRandomById(v1->directionSetID, -1);
        MapObject_TryFaceAndTurn(mapObj, v0);

        if (v1->collisionMode == WANDER_COLLISION_STOP_ON_RANGE) {
            if (MapObjectMovement_IsInMovementRange(mapObj, v0) == 0) {
                v1->state = 0;
                break;
            }
        }

        {
            u32 v2 = MapObject_CheckCollisionInDir(mapObj, v0);

            if (v2 != 0) {
                if (v1->collisionMode == WANDER_COLLISION_STOP_ON_OUT_OF_RANGE) {
                    if (v2 & (1 << 0)) {
                        v1->state = 0;
                        break;
                    }
                } else {
                    v1->state = 0;
                    break;
                }
            }
        }

        v0 = MovementAction_TurnActionTowardsDir(v0, v1->movementAction);

        LocalMapObj_SetMovementAction(mapObj, v0);
        MapObject_SetStatus1(mapObj);

        v1->state++;
    case 4:
        if (LocalMapObj_RunMovementAction(mapObj) == 0) {
            break;
        }

        MapObject_ClearStatus1(mapObj);
        v1->state = 0;
    }
}

// Builds the rectangle an object with a directional movement type may move
// within, from its initial position and its movement range on each axis. The
// movement type selects which quadrant(s) of that rectangle are allowed.
static void MapObjectMovement_GetMovementRange(MapObject *mapObj, MovementRange *param1)
{
    int v0, v1, v2, v3, v4;

    v1 = MapObject_GetXInitial(mapObj);
    v2 = MapObject_GetZInitial(mapObj);
    v3 = MapObject_GetMovementRangeX(mapObj);
    v4 = MapObject_GetMovementRangeZ(mapObj);
    v0 = MapObject_GetMovementType(mapObj);

    switch (v0) {
    case 0x6:
        param1->minX = v1 - v3;
        param1->maxX = v1;
        param1->minZ = v2 - v4;
        param1->maxZ = v2;
        break;
    case 0x7:
        param1->minX = v1;
        param1->maxX = v1 + v3;
        param1->minZ = v2 - v4;
        param1->maxZ = v2;
        break;
    case 0x8:
        param1->minX = v1 - v3;
        param1->maxX = v1;
        param1->minZ = v2;
        param1->maxZ = v2 + v4;
        break;
    case 0x9:
        param1->minX = v1;
        param1->maxX = v1 + v3;
        param1->minZ = v2;
        param1->maxZ = v2 + v4;
        break;
    case 0xa:
        param1->minX = v1 - v3;
        param1->maxX = v1;
        param1->minZ = v2 - v4;
        param1->maxZ = v2 + v4;
        break;
    case 0xb:
        param1->minX = v1;
        param1->maxX = v1 + v3;
        param1->minZ = v2 - v4;
        param1->maxZ = v2 + v4;
        break;
    case 0xc:
        param1->minX = v1 - v3;
        param1->maxX = v1 + v3;
        param1->minZ = v2 - v4;
        param1->maxZ = v2;
        break;
    case 0xd:
        param1->minX = v1 - v3;
        param1->maxX = v1 + v3;
        param1->minZ = v2;
        param1->maxZ = v2 + v4;
        break;
    default:
        GF_ASSERT(FALSE);
    }
}

static int MapObjectMovement_IsInMovementRange(MapObject *mapObj, int param1)
{
    int v0, v1;
    MovementRange v2;

    MapObjectMovement_GetMovementRange(mapObj, &v2);

    v0 = MapObject_GetX(mapObj) + MapObject_GetDxFromDir(param1);
    v1 = MapObject_GetZ(mapObj) + MapObject_GetDzFromDir(param1);

    if ((v2.minX > v0) || (v2.maxX < v0)) {
        return 0;
    }

    if ((v2.minZ > v1) || (v2.maxZ < v1)) {
        return 0;
    }

    return 1;
}

// Shared initializer for the fixed-direction look behaviors. param1 is the
// direction to face.
static void MapObjectMovement_InitLookDirection(MapObject *mapObj, int param1)
{
    LookDirectionData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(LookDirectionData)));
    v0->dir = param1;

    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);
    MapObject_UpdateCoords(mapObj);
}

void MapObjectMovement_LookDirection_Update(MapObject *mapObj)
{
    LookDirectionData *v0 = MapObject_GetUnkD8(mapObj);

    switch (v0->state) {
    case 0:
        MapObject_TryFace(mapObj, v0->dir);
        v0->state++;
        break;
    case 1:
        break;
    }
}

void MapObjectMovement_LookNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookDirection(mapObj, 0);
}

void MapObjectMovement_LookSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookDirection(mapObj, 1);
}

void MapObjectMovement_LookWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookDirection(mapObj, 2);
}

void MapObjectMovement_LookEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitLookDirection(mapObj, 3);
}

// Shared initializer for the rotate and spin behaviors. param1 is the rotation
// order (2 = counterclockwise, 3 = clockwise).
static void MapObjectMovement_InitRotate(MapObject *mapObj, int param1)
{
    RotateData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(RotateData)));
    v0->spinDir = param1;

    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);
    MapObject_UpdateCoords(mapObj);
}

void MapObjectMovement_RotateCounterclockwise_Init(MapObject *mapObj)
{
    MapObjectMovement_InitRotate(mapObj, 2);
}

void MapObjectMovement_RotateClockwise_Init(MapObject *mapObj)
{
    MapObjectMovement_InitRotate(mapObj, 3);
}

void MapObjectMovement_Rotate_Update(MapObject *mapObj)
{
    RotateData *v0 = MapObject_GetUnkD8(mapObj);

    while (sRotateCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static int MapObjectMovement_Rotate_FaceNorth(MapObject *mapObj, RotateData *param1)
{
    int v0 = MapObjectMovement_ChooseDirToFacePlayer(mapObj, 38, -1);

    if (v0 == -1) {
        v0 = MapObject_GetFacingDir(mapObj);
    }

    v0 = MovementAction_TurnActionTowardsDir(v0, MOVEMENT_ACTION_FACE_NORTH);
    LocalMapObj_SetMovementAction(mapObj, v0);
    param1->state = 1;

    return 1;
}

static int MapObjectMovement_Rotate_WaitForTurn(MapObject *mapObj, RotateData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 0) {
        return 0;
    }

    param1->timer = 0;
    param1->state = 2;

    return 1;
}

static int MapObjectMovement_Rotate_Wait(MapObject *mapObj, RotateData *param1)
{
    if (param1->timer) {
        if (MapObjectMovement_ChooseDirToFacePlayer(mapObj, 38, -1) != -1) {
            param1->state = 0;
            return 1;
        }
    }

    param1->timer++;

    if (param1->timer < 24) {
        return 0;
    }

    param1->state = 3;
    return 1;
}

// Turns to the next direction in the rotation order: counterclockwise
// (N, W, S, E) when spinDir is 2, clockwise (N, E, S, W) when it is 3.
static int MapObjectMovement_Rotate_NextDirection(MapObject *mapObj, RotateData *param1)
{
    int v0, v1, *v2;
    int v3[5] = { 0, 2, 1, 3, -1 };
    int v4[5] = { 0, 3, 1, 2, -1 };

    if (param1->spinDir == 2) {
        v2 = v3;
    } else {
        v2 = v4;
    }

    v1 = MapObject_GetFacingDir(mapObj);

    for (v0 = 0; v2[v0] != -1; v0++) {
        if (v1 == v2[v0]) {
            break;
        }
    }

    GF_ASSERT(v2[v0] != -1);

    v0++;

    if (v2[v0] == -1) {
        v0 = 0;
    }

    v1 = v2[v0];

    MapObject_TryFace(mapObj, v1);

    param1->state = 0;
    return 1;
}

// Rotate state machine: face north, wait for the turn to finish, pause, then
// turn one quarter-turn and repeat.
static int (*const sRotateCallbacks[])(MapObject *, RotateData *) = {
    MapObjectMovement_Rotate_FaceNorth,
    MapObjectMovement_Rotate_WaitForTurn,
    MapObjectMovement_Rotate_Wait,
    MapObjectMovement_Rotate_NextDirection
};

void MapObjectMovement_Spin_Init(MapObject *mapObj)
{
    MapObjectMovement_InitRotate(mapObj, 3);
}

void MapObjectMovement_Spin_Update(MapObject *mapObj)
{
    RotateData *v0 = MapObject_GetUnkD8(mapObj);

    while (sSpinCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static int MapObjectMovement_Spin_FaceNorth(MapObject *mapObj, RotateData *param1)
{
    int v0 = MapObject_GetFacingDir(mapObj);

    v0 = MovementAction_TurnActionTowardsDir(v0, MOVEMENT_ACTION_FACE_NORTH);
    LocalMapObj_SetMovementAction(mapObj, v0);
    param1->state = 1;

    return 1;
}

static int MapObjectMovement_Spin_WaitForTurn(MapObject *mapObj, RotateData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 0) {
        return 0;
    }

    param1->timer = 0;
    param1->state = 2;

    return 1;
}

static int MapObjectMovement_Spin_Wait(MapObject *mapObj, RotateData *param1)
{
    param1->timer++;

    if (param1->timer < 24) {
        return 0;
    }

    param1->state = 3;
    return 1;
}

// Like MapObjectMovement_Rotate_NextDirection, but reverses the spin direction
// once the object is facing its initial direction again (a full turn).
static int MapObjectMovement_Spin_NextDirection(MapObject *mapObj, RotateData *param1)
{
    int v0, v1, *v2;
    int v3[5] = { 0, 2, 1, 3, -1 };
    int v4[5] = { 0, 3, 1, 2, -1 };

    if (param1->spinDir == 2) {
        v2 = v3;
    } else {
        v2 = v4;
    }

    v1 = MapObject_GetFacingDir(mapObj);

    for (v0 = 0; v2[v0] != -1; v0++) {
        if (v1 == v2[v0]) {
            break;
        }
    }

    GF_ASSERT(v2[v0] != -1);

    v0++;

    if (v2[v0] == -1) {
        v0 = 0;
    }

    v1 = v2[v0];

    MapObject_TryFace(mapObj, v1);

    {
        int v5 = MapObject_GetFacingDir(mapObj);
        int v6 = MapObject_GetInitialDir(mapObj);

        if (v5 == v6) {
            param1->spinDir = Direction_GetOpposite(param1->spinDir);
        }
    }

    param1->state = 0;
    return 1;
}

// VS-seeker spin state machine: like sRotateCallbacks, but the spin direction
// reverses every time the object completes a full turn.
static int (*const sSpinCallbacks[])(MapObject *, RotateData *) = {
    MapObjectMovement_Spin_FaceNorth,
    MapObjectMovement_Spin_WaitForTurn,
    MapObjectMovement_Spin_Wait,
    MapObjectMovement_Spin_NextDirection
};

void MapObjectMovement_WalkBackAndForth_Init(MapObject *mapObj)
{
    WalkBackAndForthData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WalkBackAndForthData)));

    if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
        MapObjectMovement_InitSpinDirection(mapObj, &v0->spinData);
    }
}

void MapObjectMovement_WalkBackAndForth_Update(MapObject *mapObj)
{
    WalkBackAndForthData *v0 = MapObject_GetUnkD8(mapObj);

    while (sWalkBackAndForthCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static int MapObjectMovement_WalkBackAndForth_Turn(MapObject *mapObj, WalkBackAndForthData *param1)
{
    int v0 = MapObject_GetInitialDir(mapObj);

    if (param1->reversed == 1) {
        v0 = Direction_GetOpposite(v0);
    }

    MapObject_Turn(mapObj, v0);

    if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
        MapObject_TryFace(mapObj, v0);
    }

    param1->state = 1;
    return 1;
}

// Walks one tile in the current direction. If the way is blocked the object
// reverses (and turns around once it is back at its start tile); if the
// reversed direction is also blocked it walks on the spot instead.
static int MapObjectMovement_WalkBackAndForth_Step(MapObject *mapObj, WalkBackAndForthData *param1)
{
    if (param1->reversed) {
        int v0, v1, v2, v3;

        v0 = MapObject_GetXInitial(mapObj);
        v1 = MapObject_GetZInitial(mapObj);
        v2 = MapObject_GetX(mapObj);
        v3 = MapObject_GetZ(mapObj);

        if ((v0 == v2) && (v1 == v3)) {
            int v4 = Direction_GetOpposite(MapObject_GetMovingDir(mapObj));

            MapObject_Turn(mapObj, v4);

            if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
                MapObject_TryFace(mapObj, v4);
            }

            param1->reversed = 0;
        }
    }

    {
        int v5, v6;
        u32 v7;

        v5 = MapObject_GetMovingDir(mapObj);
        v7 = MapObject_CheckCollisionInDir(mapObj, v5);

        if (v7 & (1 << 0)) {
            param1->reversed = 1;
            v5 = Direction_GetOpposite(v5);
            v7 = MapObject_CheckCollisionInDir(mapObj, v5);
        }

        v6 = 0xc;

        if (v7 != 0) {
            v6 = 0x20;
        }

        v6 = MovementAction_TurnActionTowardsDir(v5, v6);
        LocalMapObj_SetMovementAction(mapObj, v6);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_AdvanceSpinDirection(mapObj, &param1->spinData);
        }
    }

    MapObject_SetStatus1(mapObj);
    param1->state = 2;

    return 1;
}

static int MapObjectMovement_WalkBackAndForth_Wait(MapObject *mapObj, WalkBackAndForthData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        MapObject_ClearStatus1(mapObj);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_RestoreSpinLock(mapObj, &param1->spinData);
        }

        param1->state = 0;
    }

    return 0;
}

// Walk-back-and-forth state machine: turn to the initial direction, walk one
// tile (reversing when blocked), then wait for the step to finish.
static int (*const sWalkBackAndForthCallbacks[])(MapObject *, WalkBackAndForthData *) = {
    MapObjectMovement_WalkBackAndForth_Turn,
    MapObjectMovement_WalkBackAndForth_Step,
    MapObjectMovement_WalkBackAndForth_Wait
};

// Shared initializer for the walk-cycle behaviors. param1 is the step index at
// which the object is at its start tile, param2 the axis to compare against
// the start tile (0 = x, 1 = z), and param3 the direction set to walk through.
static void MapObjectMovement_InitWalkPattern(MapObject *mapObj, int param1, int param2, int param3)
{
    WalkPatternData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WalkPatternData)));
    v0->initialStep = param1;
    v0->axis = param2;
    v0->directionSetID = param3;

    if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
        MapObjectMovement_InitSpinDirection(mapObj, &v0->spinData);
    }
}

void MapObjectMovement_WalkNorthEastWestSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 14);
}

void MapObjectMovement_WalkEastWestSouthNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 15);
}

void MapObjectMovement_WalkSouthNorthEastWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 16);
}

void MapObjectMovement_WalkWestSouthNorthEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 17);
}

void MapObjectMovement_WalkWestEastSouthNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 18);
}

void MapObjectMovement_WalkNorthWestEastSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 19);
}

void MapObjectMovement_WalkSouthNorthWestEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 20);
}

void MapObjectMovement_WalkEastSouthNorthWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 21);
}

void MapObjectMovement_WalkWestNorthSouthEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 22);
}

void MapObjectMovement_WalkNorthSouthEastWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 23);
}

void MapObjectMovement_WalkEastWestNorthSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 24);
}

void MapObjectMovement_WalkSouthEastWestNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 25);
}

void MapObjectMovement_WalkEastNorthSouthWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 26);
}

void MapObjectMovement_WalkNorthSouthWestEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 1, 27);
}

void MapObjectMovement_WalkWestEastNorthSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 28);
}

void MapObjectMovement_WalkSouthWestEastNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPattern(mapObj, 2, 0, 29);
}

void MapObjectMovement_WalkPattern_Update(MapObject *mapObj)
{
    WalkPatternData *v0 = MapObject_GetUnkD8(mapObj);

    while (sWalkPatternCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

// Walks one tile in the direction at stepIndex. The step index advances when
// the object is back at its start tile, and again if the chosen direction is
// blocked; if the next direction is also blocked the object walks on the spot.
static int MapObjectMovement_WalkPattern_Step(MapObject *mapObj, WalkPatternData *param1)
{
    if (param1->stepIndex == param1->initialStep) {
        if (param1->axis == 0) {
            int v0 = MapObject_GetXInitial(mapObj);
            int v1 = MapObject_GetX(mapObj);

            if (v0 == v1) {
                param1->stepIndex++;
            }
        } else {
            int v2 = MapObject_GetZInitial(mapObj);
            int v3 = MapObject_GetZ(mapObj);

            if (v2 == v3) {
                param1->stepIndex++;
            }
        }
    }

    if (param1->stepIndex == 3) {
        int v4 = MapObject_GetXInitial(mapObj);
        int v5 = MapObject_GetZInitial(mapObj);
        int v6 = MapObject_GetX(mapObj);
        int v7 = MapObject_GetZ(mapObj);

        if ((v4 == v6) && (v5 == v7)) {
            param1->stepIndex = 0;
        }
    }

    {
        const int *v8;
        int v9, v10;
        u32 v11;

        v8 = DirectionSet_GetById(param1->directionSetID);
        v9 = v8[param1->stepIndex];

        MapObject_Turn(mapObj, v9);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
            MapObject_TryFace(mapObj, v9);
        }

        v11 = MapObject_CheckCollisionInDir(mapObj, v9);

        if (v11 & (1 << 0)) {
            param1->stepIndex++;
            v9 = v8[param1->stepIndex];

            MapObject_Turn(mapObj, v9);

            if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
                MapObject_TryFace(mapObj, v9);
            }

            v11 = MapObject_CheckCollisionInDir(mapObj, v9);
        }

        v10 = 0xc;

        if (v11 != 0) {
            v10 = 0x20;
        }

        v10 = MovementAction_TurnActionTowardsDir(v9, v10);
        LocalMapObj_SetMovementAction(mapObj, v10);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_AdvanceSpinDirection(mapObj, &param1->spinData);
        }
    }

    MapObject_SetStatus1(mapObj);
    param1->state = 1;

    return 1;
}

static int MapObjectMovement_WalkPattern_Wait(MapObject *mapObj, WalkPatternData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        MapObject_ClearStatus1(mapObj);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_RestoreSpinLock(mapObj, &param1->spinData);
        }

        param1->state = 0;
    }

    return 0;
}

// Walk-cycle state machine: take one step through the direction set, then wait
// for the step to finish.
static int (*const sWalkPatternCallbacks[])(MapObject *, WalkPatternData *) = {
    MapObjectMovement_WalkPattern_Step,
    MapObjectMovement_WalkPattern_Wait
};

// Shared initializer for the player-reactive walk-cycle behaviors. Parameters
// match MapObjectMovement_InitWalkPattern.
static void MapObjectMovement_InitWalkPatternPlayer(MapObject *mapObj, int param1, int param2, int param3)
{
    WalkPatternPlayerData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WalkPatternPlayerData)));

    v0->initialStep = param1;
    v0->axis = param2;
    v0->directionSetID = param3;

    if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
        MapObjectMovement_InitSpinDirection(mapObj, &v0->spinData);
    }
}

void MapObjectMovement_WalkNorthWestSouthEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 1, 30);
}

void MapObjectMovement_WalkSouthEastNorthWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 1, 31);
}

void MapObjectMovement_WalkWestSouthEastNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 0, 32);
}

void MapObjectMovement_WalkEastNorthWestSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 0, 33);
}

void MapObjectMovement_WalkNorthEastSouthWest_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 1, 34);
}

void MapObjectMovement_WalkSouthWestNorthEast_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 1, 35);
}

void MapObjectMovement_WalkWestNorthEastSouth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 0, 36);
}

void MapObjectMovement_WalkEastSouthWestNorth_Init(MapObject *mapObj)
{
    MapObjectMovement_InitWalkPatternPlayer(mapObj, 2, 0, 37);
}

void MapObjectMovement_WalkPatternPlayer_Update(MapObject *mapObj)
{
    WalkPatternPlayerData *v0 = MapObject_GetUnkD8(mapObj);

    while (sWalkPatternPlayerCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static void MapObjectMovement_WalkPatternPlayer_AdvanceStep(WalkPatternPlayerData *param0)
{
    if (param0->reverse == 1) {
        param0->stepIndex--;

        if (param0->stepIndex < 0) {
            param0->stepIndex = 3;
        }
    } else {
        param0->stepIndex++;
    }
}

// Trainer type TRAINER_TYPE_UNK_010 jumps in place when the player is within
// range; every other type goes straight to the walk step. Note that the jump
// action is set from the distance returned by MapObject_GetDistanceToPlayer
// (v4), not from the computed jump action (v5).
static int MapObjectMovement_WalkPatternPlayer_CheckPlayer(MapObject *mapObj, WalkPatternPlayerData *param1)
{
    if (MapObject_GetTrainerType(mapObj) == 0xa) {
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
        PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);
        int v2 = MapObject_GetFacingDir(mapObj);
        int v3 = MapObject_GetDataAt(mapObj, 0);
        int v4 = MapObject_GetDistanceToPlayer(mapObj, playerAvatar, v2, v3);

        if (v4 != -1) {
            int v5 = MovementAction_TurnActionTowardsDir(v2, MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_NORTH);

            LocalMapObj_SetMovementAction(mapObj, v4);
            MapObject_SetStatus1(mapObj);
            param1->state = 1;
            return 1;
        }
    }

    param1->state = 2;

    return 1;
}

static int MapObjectMovement_WalkPatternPlayer_WaitForJump(MapObject *mapObj, WalkPatternPlayerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        MapObject_ClearStatus1(mapObj);
        param1->state = 2;
    }

    return 0;
}

// Same as MapObjectMovement_WalkPattern_Step, but the step index can also be
// advanced backwards (see MapObjectMovement_WalkPatternPlayer_AdvanceStep).
static int MapObjectMovement_WalkPatternPlayer_Step(MapObject *mapObj, WalkPatternPlayerData *param1)
{
    if (param1->stepIndex == param1->initialStep) {
        if (param1->axis == 0) {
            int v0 = MapObject_GetXInitial(mapObj);
            int v1 = MapObject_GetX(mapObj);

            if (v0 == v1) {
                MapObjectMovement_WalkPatternPlayer_AdvanceStep(param1);
            }
        } else {
            int v2 = MapObject_GetZInitial(mapObj);
            int v3 = MapObject_GetZ(mapObj);

            if (v2 == v3) {
                MapObjectMovement_WalkPatternPlayer_AdvanceStep(param1);
            }
        }
    }

    if (param1->stepIndex == 3) {
        int v4 = MapObject_GetXInitial(mapObj);
        int v5 = MapObject_GetZInitial(mapObj);
        int v6 = MapObject_GetX(mapObj);
        int v7 = MapObject_GetZ(mapObj);

        if ((v4 == v6) && (v5 == v7)) {
            param1->stepIndex = 0;
        }
    }

    {
        const int *v8;
        int v9, v10;
        u32 v11;

        v8 = DirectionSet_GetById(param1->directionSetID);
        v9 = v8[param1->stepIndex];

        MapObject_Turn(mapObj, v9);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
            MapObject_TryFace(mapObj, v9);
        }

        v11 = MapObject_CheckCollisionInDir(mapObj, v9);

        if (v11 & (1 << 0)) {
            MapObjectMovement_WalkPatternPlayer_AdvanceStep(param1);
            v9 = v8[param1->stepIndex];

            MapObject_Turn(mapObj, v9);

            if (MapObjectMovement_IsSpinningTrainer(mapObj) == 0) {
                MapObject_TryFace(mapObj, v9);
            }

            v11 = MapObject_CheckCollisionInDir(mapObj, v9);
        }

        v10 = 0xc;

        if (v11 != 0) {
            v10 = 0x20;
        }

        v10 = MovementAction_TurnActionTowardsDir(v9, v10);
        LocalMapObj_SetMovementAction(mapObj, v10);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_AdvanceSpinDirection(mapObj, &param1->spinData);
        }
    }

    MapObject_SetStatus1(mapObj);
    param1->state = 3;

    return 1;
}

static int MapObjectMovement_WalkPatternPlayer_Wait(MapObject *mapObj, WalkPatternPlayerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        MapObject_ClearStatus1(mapObj);

        if (MapObjectMovement_IsSpinningTrainer(mapObj) == 1) {
            MapObjectMovement_RestoreSpinLock(mapObj, &param1->spinData);
        }

        param1->state = 0;
    }

    return 0;
}

// Walk-cycle state machine for objects that also react to the player: check
// for a nearby player (jumping if one is found), take one step, then wait.
static int (*const sWalkPatternPlayerCallbacks[])(MapObject *, WalkPatternPlayerData *) = {
    MapObjectMovement_WalkPatternPlayer_CheckPlayer,
    MapObjectMovement_WalkPatternPlayer_WaitForJump,
    MapObjectMovement_WalkPatternPlayer_Step,
    MapObjectMovement_WalkPatternPlayer_Wait
};

// Returns the number of directions in a set, i.e. the index of the sentinel
// value param1 (normally -1).
static int DirectionSet_GetLength(const int *param0, int param1)
{
    int i = 0;

    while (param0[i] != param1) {
        i++;
    }

    GF_ASSERT(i);
    return i;
}

static int DirectionSet_GetRandom(const int *param0, int param1)
{
    return param0[LCRNG_Next() % DirectionSet_GetLength(param0, param1)];
}

static int DirectionSet_GetRandomById(int param0, int param1)
{
    const int *v0 = DirectionSet_GetById(param0);
    return v0[LCRNG_Next() % DirectionSet_GetLength(v0, param1)];
}

static const int *DirectionSet_GetById(int param0)
{
    const DirectionSetEntry *v0 = sDirectionSets;

    while (v0->id != 39) {
        if (v0->id == param0) {
            return v0->directions;
        }

        v0++;
    }

    GF_ASSERT(FALSE);
    return NULL;
}

// Returns the direction from the object to the player if the player is running
// and within the object's reaction range, or -1 otherwise. Only trainer types
// TRAINER_TYPE_NORMAL and TRAINER_TYPE_VIEW_ALL_DIRECTIONS react, and only for
// the movement types listed in sPlayerReactiveMovementTypes.
static int MapObjectMovement_GetDirToRunningPlayer(MapObject *mapObj)
{
    int v0 = MapObject_GetTrainerType(mapObj);

    if ((v0 != 0x1) && (v0 != 0x2)) {
        return -1;
    }

    {
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
        PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

        if (PlayerAvatar_IsRunning(playerAvatar) == 0) {
            return -1;
        }

        {
            int v3, v4 = 0;

            v0 = MapObject_GetMovementType(mapObj);

            do {
                v3 = sPlayerReactiveMovementTypes[v4++];

                if (v3 == v0) {
                    break;
                }
            } while (v3 != 0xff);

            if (v0 != v3) {
                return -1;
            }
        }

        {
            const MapObject *v5 = PlayerAvatar_GetMapObject(playerAvatar);
            int v6 = MapObject_GetYFromPos(v5);
            int v7 = MapObject_GetYFromPos(mapObj);

            if (v6 != v7) {
                return -1;
            }
        }

        {
            int v8 = PlayerAvatar_GetXPos(playerAvatar);
            int v9 = PlayerAvatar_GetZPos(playerAvatar);
            int v10 = MapObject_GetDataAt(mapObj, 0);
            int v11 = MapObject_GetX(mapObj);
            int v12 = MapObject_GetZ(mapObj);
            int v13 = v11 - v10;
            int v14 = v11 + v10;
            int v15 = v12 - v10;
            int v16 = v12 + v10;

            if ((v15 <= v9) && (v16 >= v9)) {
                if ((v13 <= v8) && (v14 >= v8)) {
                    return GetDirectionBetweenPoints(v11, v12, v8, v9);
                }
            }
        }
    }

    return -1;
}

// Chooses a direction to face a running player, constrained to the direction
// set param1. If the exact direction to the player is not in the set, falls
// back to whichever of the object's x/z axis directions toward the player is
// allowed. Returns -1 if the player is not a valid target.
static int MapObjectMovement_ChooseDirToFacePlayer(MapObject *mapObj, int param1, int param2)
{
    const int *v0 = DirectionSet_GetById(param1);
    int v1 = DirectionSet_GetLength(v0, param2);

    if (v1 == 1) {
        return -1;
    }

    {
        int v2;

        v2 = MapObjectMovement_GetDirToRunningPlayer(mapObj);

        if (v2 == -1) {
            return v2;
        }

        {
            int v3 = 0;

            do {
                if (v0[v3] == v2) {
                    return v2;
                }

                v3++;
            } while (v3 < v1);

            {
                int v4 = -1, v5 = -1;
                int v6 = MapObject_GetX(mapObj);
                int v7 = MapObject_GetZ(mapObj);
                FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
                PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);
                int v10 = PlayerAvatar_GetXPos(playerAvatar);
                int v11 = PlayerAvatar_GetZPos(playerAvatar);

                if (v6 > v10) {
                    v4 = 2;
                } else if (v6 < v10) {
                    v4 = 3;
                }

                if (v7 > v11) {
                    v5 = 0;
                } else if (v7 < v11) {
                    v5 = 1;
                }

                v3 = 0;

                if (v4 == -1) {
                    do {
                        if (v0[v3] == v5) {
                            return v5;
                        }

                        v3++;
                    } while (v3 < v1);
                } else if (v5 == -1) {
                    do {
                        if (v0[v3] == v4) {
                            return v4;
                        }

                        v3++;
                    } while (v3 < v1);
                } else {
                    do {
                        if (v0[v3] == v4) {
                            return v4;
                        }

                        if (v0[v3] == v5) {
                            return v5;
                        }

                        v3++;
                    } while (v3 < v1);
                }
            }
        }
    }

    return -1;
}

static const int sSpinDirectionOrders[2][4] = {
    { 0x0, 0x2, 0x1, 0x3 },
    { 0x0, 0x3, 0x1, 0x2 }
};

// TRUE for the trainer types that spin while walking
// (TRAINER_TYPE_SPIN_COUNTERCLOCKWISE / TRAINER_TYPE_SPIN_CLOCKWISE).
static int MapObjectMovement_IsSpinningTrainer(MapObject *mapObj)
{
    int v0 = MapObject_GetTrainerType(mapObj);

    if ((v0 == 0x7) || (v0 == 0x8)) {
        return 1;
    }

    return 0;
}

// Selects the spin order from the trainer type: counterclockwise for
// TRAINER_TYPE_SPIN_COUNTERCLOCKWISE, clockwise otherwise.
static void MapObjectMovement_InitSpinDirection(MapObject *mapObj, SpinningTrainerData *param1)
{
    if (MapObject_GetTrainerType(mapObj) == 0x7) {
        param1->spinDirection = 0;
    } else {
        param1->spinDirection = 1;
    }
}

// Turns the object one step around the spin order selected by spinDirection
// (0 = counterclockwise, 1 = clockwise), then locks its facing direction so
// the spin animation cannot be interrupted. The previous lock state is saved
// so MapObjectMovement_RestoreSpinLock can undo it.
static void MapObjectMovement_AdvanceSpinDirection(MapObject *mapObj, SpinningTrainerData *param1)
{
    int v0, v1 = MapObject_GetFacingDir(mapObj);

    for (v0 = 0; (v0 < 4 && v1 != sSpinDirectionOrders[param1->spinDirection][v0]); v0++) {
        (void)0;
    }

    GF_ASSERT(v0 < 4);

    param1->savedDir = v1;

    v0 = (v0 + 1) % 4;
    v1 = sSpinDirectionOrders[param1->spinDirection][v0];

    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_LOCK_DIR)) {
        param1->lockWasSet = 1;
    } else {
        param1->lockWasSet = 0;
    }

    MapObject_TryFace(mapObj, v1);
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_LOCK_DIR);
}

// Restores the lock-dir flag to the state it had before the spin step.
static void MapObjectMovement_RestoreSpinLock(MapObject *mapObj, SpinningTrainerData *param1)
{
    if (param1->lockWasSet == 0) {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_LOCK_DIR);
    }
}

// Direction sets referenced by sDirectionSets. Each is a list of DIR_* values
// terminated by -1. IDs 0x0-0xA are the look-around sets, 0xB-0xD are extra
// sets used by the rotate/spin behaviors, and 0xE-0x26 are the walk-cycle
// orders.
static const int sLookAroundDelays[] = {
    0x10,
    0x20,
    0x30,
    0x40,
    -1
};

static const int sDirectionsAll[] = {
    0x0,
    0x1,
    0x2,
    0x3,
    -1
};

static const int sDirectionsNorthWest[] = {
    0x0,
    0x2,
    -1
};

static const int sDirectionsNorthEast[] = {
    0x0,
    0x3,
    -1
};

static const int sDirectionsSouthWest[] = {
    0x1,
    0x2,
    -1
};

static const int sDirectionsSouthEast[] = {
    0x1,
    0x3,
    -1
};

static const int sDirectionsNorthSouthWest[] = {
    0x0,
    0x1,
    0x2,
    -1
};

static const int sDirectionsNorthSouthEast[] = {
    0x0,
    0x1,
    0x3,
    -1
};

static const int sDirectionsNorthWestEast[] = {
    0x0,
    0x2,
    0x3,
    -1
};

static const int sDirectionsSouthWestEast[] = {
    0x1,
    0x2,
    0x3,
    -1
};

static const int sDirectionsNorthSouth[] = {
    0x0,
    0x1,
    -1
};

static const int sDirectionsWestEast[] = {
    0x2,
    0x3,
    -1
};

static const int sDirectionsAll2[] = {
    0x0,
    0x1,
    0x2,
    0x3,
    -1
};

static const int sDirectionsNorthSouth2[] = {
    0x0,
    0x1,
    -1
};

static const int sDirectionsWestEast2[] = {
    0x2,
    0x3,
    -1
};

static const int sWalkOrderNorthEastWestSouth[] = {
    0x0,
    0x3,
    0x2,
    0x1
};

static const int sWalkOrderEastWestSouthNorth[] = {
    0x3,
    0x2,
    0x1,
    0x0
};

static const int sWalkOrderSouthNorthEastWest[] = {
    0x1,
    0x0,
    0x3,
    0x2
};

static const int sWalkOrderWestSouthNorthEast[] = {
    0x2,
    0x1,
    0x0,
    0x3
};

static const int sWalkOrderWestEastSouthNorth[] = {
    0x2,
    0x3,
    0x1,
    0x0
};

static const int sWalkOrderWestEastSouthNorth2[] = {
    0x2,
    0x3,
    0x1,
    0x0
};

static const int sWalkOrderSouthNorthWestEast[] = {
    0x1,
    0x0,
    0x2,
    0x3
};

static const int sWalkOrderEastSouthNorthWest[] = {
    0x3,
    0x1,
    0x0,
    0x2
};

static const int sWalkOrderWestNorthSouthEast[] = {
    0x2,
    0x0,
    0x1,
    0x3
};

static const int sWalkOrderNorthSouthEastWest[] = {
    0x0,
    0x1,
    0x3,
    0x2
};

static const int sWalkOrderEastWestNorthSouth[] = {
    0x3,
    0x2,
    0x0,
    0x1
};

static const int sWalkOrderSouthEastWestNorth[] = {
    0x1,
    0x3,
    0x2,
    0x0
};

static const int sWalkOrderEastNorthSouthWest[] = {
    0x3,
    0x0,
    0x1,
    0x2
};

static const int sWalkOrderNorthSouthWestEast[] = {
    0x0,
    0x1,
    0x2,
    0x3
};

static const int sWalkOrderWestEastNorthSouth[] = {
    0x2,
    0x3,
    0x0,
    0x1
};

static const int sWalkOrderSouthWestEastNorth[] = {
    0x1,
    0x2,
    0x3,
    0x0
};

static const int sWalkOrderNorthWestSouthEast[] = {
    0x0,
    0x2,
    0x1,
    0x3
};

static const int sWalkOrderSouthEastNorthWest[] = {
    0x1,
    0x3,
    0x0,
    0x2
};

static const int sWalkOrderWestSouthEastNorth[] = {
    0x2,
    0x1,
    0x3,
    0x0
};

static const int sWalkOrderEastNorthWestSouth[] = {
    0x3,
    0x0,
    0x2,
    0x1
};

static const int sWalkOrderNorthEastSouthWest[] = {
    0x0,
    0x3,
    0x1,
    0x2
};

static const int sWalkOrderSouthWestNorthEast[] = {
    0x1,
    0x2,
    0x0,
    0x3
};

static const int sWalkOrderWestNorthEastSouth[] = {
    0x2,
    0x0,
    0x3,
    0x1
};

static const int sWalkOrderEastSouthWestNorth[] = {
    0x3,
    0x1,
    0x2,
    0x0
};

static const int sWalkOrderNorthSouthWestEast2[] = {
    0x0,
    0x1,
    0x2,
    0x3,
    -1
};

// Maps a direction set ID to its direction list. The final entry (ID 0x27) is
// a sentinel that terminates the lookup in DirectionSet_GetById.
static const DirectionSetEntry sDirectionSets[40] = {
    { 0x0, sDirectionsAll },
    { 0x1, sDirectionsNorthWest },
    { 0x2, sDirectionsNorthEast },
    { 0x3, sDirectionsSouthWest },
    { 0x4, sDirectionsSouthEast },
    { 0x5, sDirectionsNorthSouthWest },
    { 0x6, sDirectionsNorthSouthEast },
    { 0x7, sDirectionsNorthWestEast },
    { 0x8, sDirectionsSouthWestEast },
    { 0x9, sDirectionsNorthSouth },
    { 0xA, sDirectionsWestEast },
    { 0xB, sDirectionsAll2 },
    { 0xC, sDirectionsNorthSouth2 },
    { 0xD, sDirectionsWestEast2 },
    { 0xE, sWalkOrderNorthEastWestSouth },
    { 0xF, sWalkOrderEastWestSouthNorth },
    { 0x10, sWalkOrderSouthNorthEastWest },
    { 0x11, sWalkOrderWestSouthNorthEast },
    { 0x12, sWalkOrderWestEastSouthNorth },
    { 0x13, sWalkOrderWestEastSouthNorth2 },
    { 0x14, sWalkOrderSouthNorthWestEast },
    { 0x15, sWalkOrderEastSouthNorthWest },
    { 0x16, sWalkOrderWestNorthSouthEast },
    { 0x17, sWalkOrderNorthSouthEastWest },
    { 0x18, sWalkOrderEastWestNorthSouth },
    { 0x19, sWalkOrderSouthEastWestNorth },
    { 0x1A, sWalkOrderEastNorthSouthWest },
    { 0x1B, sWalkOrderNorthSouthWestEast },
    { 0x1C, sWalkOrderWestEastNorthSouth },
    { 0x1D, sWalkOrderSouthWestEastNorth },
    { 0x1E, sWalkOrderNorthWestSouthEast },
    { 0x1F, sWalkOrderSouthEastNorthWest },
    { 0x20, sWalkOrderWestSouthEastNorth },
    { 0x21, sWalkOrderEastNorthWestSouth },
    { 0x22, sWalkOrderNorthEastSouthWest },
    { 0x23, sWalkOrderSouthWestNorthEast },
    { 0x24, sWalkOrderWestNorthEastSouth },
    { 0x25, sWalkOrderEastSouthWestNorth },
    { 0x26, sWalkOrderNorthSouthWestEast2 },
    { 0x27, NULL }
};

// Movement types whose objects turn to face a running player who comes near.
// Terminated by 0xFF.
static const int sPlayerReactiveMovementTypes[] = {
    0x2,
    0x6,
    0x7,
    0x8,
    0x9,
    0xA,
    0xB,
    0xC,
    0xD,
    0x2D,
    0x2E,
    0x12,
    0x13,
    0xff
};
