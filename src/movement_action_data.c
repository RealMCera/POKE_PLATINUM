#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"
#include "struct_defs/struct_020EDF0C.h"

#include "berry_patch_graphics.h"
#include "map_object.h"
#include "map_object_movement_patterns.h"
#include "map_object_animation.h"
#include "map_object_movement.h"

// Movement-action and movement-type dispatch data.
//
// gMovementActionFuncs maps each MOVEMENT_ACTION_* to the per-step callback
// that implements it. gMovementActionCodes groups the movement actions that
// have four facing variants (north/south/west/east) so that
// MovementAction_TurnActionTowardsDir and MovementAction_GetDirFromAction can
// translate between a direction and the matching action.
//
// gMovementTypeCallbacks maps each MOVEMENT_TYPE_* to a MovementTypeCallbacks
// set. MapObject_LoadMovementCallbacks installs the init/update/free callbacks
// on the map object, and MapObject_CallMovementLoad invokes the load callback.
// The static tables below are the individual callback sets; most movement
// types share the same init/update/free functions and differ only in the
// direction set they were configured with.

static const MovementTypeCallbacks sMovementTypeCallbacks_None = {
    0x0,
    MapObjectMovement_NoOp1,
    MapObjectMovement_NoOp2,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_Player = {
    0x1,
    MapObjectMovement_NoOp1,
    MapObjectMovement_NoOp2,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookAround = {
    0x2,
    MapObjectMovement_LookAround_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderAround = {
    0x3,
    MapObjectMovement_WanderAround_Init,
    MapObjectMovement_Wander_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderNorthAndSouth = {
    0x3,
    MapObjectMovement_WanderNorthAndSouth_Init,
    MapObjectMovement_Wander_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderWestAndEast = {
    0x3,
    MapObjectMovement_WanderWestAndEast_Init,
    MapObjectMovement_Wander_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderWestAndEastAlt = {
    0x3,
    MapObjectMovement_WanderWestAndEastAlt_Init,
    MapObjectMovement_Wander_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthAndWest = {
    0x3,
    MapObjectMovement_LookNorthAndWest_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthAndEast = {
    0x3,
    MapObjectMovement_LookNorthAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookSouthAndWest = {
    0x3,
    MapObjectMovement_LookSouthAndWest_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookSouthAndEast = {
    0x3,
    MapObjectMovement_LookSouthAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthSouthAndWest = {
    0x3,
    MapObjectMovement_LookNorthSouthAndWest_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthSouthAndEast = {
    0x3,
    MapObjectMovement_LookNorthSouthAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthWestAndEast = {
    0x3,
    MapObjectMovement_LookNorthWestAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookSouthWestAndEast = {
    0x3,
    MapObjectMovement_LookSouthWestAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorthAndSouth = {
    0x3,
    MapObjectMovement_LookNorthAndSouth_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookWestAndEast = {
    0x3,
    MapObjectMovement_LookWestAndEast_Init,
    MapObjectMovement_LookAround_Update,
    MapObjectMovement_LookAround_Free,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookNorth = {
    0x3,
    MapObjectMovement_LookNorth_Init,
    MapObjectMovement_LookDirection_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookSouth = {
    0x3,
    MapObjectMovement_LookSouth_Init,
    MapObjectMovement_LookDirection_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookWest = {
    0x3,
    MapObjectMovement_LookWest_Init,
    MapObjectMovement_LookDirection_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_LookEast = {
    0x3,
    MapObjectMovement_LookEast_Init,
    MapObjectMovement_LookDirection_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_RotateCounterclockwise = {
    0x3,
    MapObjectMovement_RotateCounterclockwise_Init,
    MapObjectMovement_Rotate_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_RotateClockwise = {
    0x3,
    MapObjectMovement_RotateClockwise_Init,
    MapObjectMovement_Rotate_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_Spin = {
    0x3,
    MapObjectMovement_Spin_Init,
    MapObjectMovement_Spin_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkBackAndForth = {
    0x3,
    MapObjectMovement_WalkBackAndForth_Init,
    MapObjectMovement_WalkBackAndForth_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthEastWestSouth = {
    0x3,
    MapObjectMovement_WalkNorthEastWestSouth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastWestSouthNorth = {
    0x3,
    MapObjectMovement_WalkEastWestSouthNorth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthNorthEastWest = {
    0x3,
    MapObjectMovement_WalkSouthNorthEastWest_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestSouthNorthEast = {
    0x3,
    MapObjectMovement_WalkWestSouthNorthEast_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestEastSouthNorth = {
    0x3,
    MapObjectMovement_WalkWestEastSouthNorth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthWestEastSouth = {
    0x3,
    MapObjectMovement_WalkNorthWestEastSouth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthNorthWestEast = {
    0x3,
    MapObjectMovement_WalkSouthNorthWestEast_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastSouthNorthWest = {
    0x3,
    MapObjectMovement_WalkEastSouthNorthWest_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestNorthSouthEast = {
    0x3,
    MapObjectMovement_WalkWestNorthSouthEast_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthSouthEastWest = {
    0x3,
    MapObjectMovement_WalkNorthSouthEastWest_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastWestNorthSouth = {
    0x3,
    MapObjectMovement_WalkEastWestNorthSouth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthEastWestNorth = {
    0x3,
    MapObjectMovement_WalkSouthEastWestNorth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastNorthSouthWest = {
    0x3,
    MapObjectMovement_WalkEastNorthSouthWest_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthSouthWestEast = {
    0x3,
    MapObjectMovement_WalkNorthSouthWestEast_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestEastNorthSouth = {
    0x3,
    MapObjectMovement_WalkWestEastNorthSouth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthWestEastNorth = {
    0x3,
    MapObjectMovement_WalkSouthWestEastNorth_Init,
    MapObjectMovement_WalkPattern_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthWestSouthEast = {
    0x3,
    MapObjectMovement_WalkNorthWestSouthEast_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthEastNorthWest = {
    0x3,
    MapObjectMovement_WalkSouthEastNorthWest_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestSouthEastNorth = {
    0x3,
    MapObjectMovement_WalkWestSouthEastNorth_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastNorthWestSouth = {
    0x3,
    MapObjectMovement_WalkEastNorthWestSouth_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkNorthEastSouthWest = {
    0x3,
    MapObjectMovement_WalkNorthEastSouthWest_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkSouthWestNorthEast = {
    0x3,
    MapObjectMovement_WalkSouthWestNorthEast_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWestNorthEastSouth = {
    0x3,
    MapObjectMovement_WalkWestNorthEastSouth_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkEastSouthWestNorth = {
    0x3,
    MapObjectMovement_WalkEastSouthWestNorth_Init,
    MapObjectMovement_WalkPatternPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_BerrySoil = {
    0x2f,
    BerryPatchGraphics_NewData,
    BerryPatchGraphics_UpdateGrowthStage,
    BerryPatchGraphics_NoOp,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_FollowPlayer = {
    0x3,
    MapObjectMovement_FollowPlayer_Init,
    MapObjectMovement_FollowPlayer_Update,
    MapObjectMovement_FollowPlayer_Load,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_FollowPartnerTrainer = {
    0x3,
    MapObjectMovement_FollowPartnerTrainer_Init,
    MapObjectMovement_FollowPartnerTrainer_Update,
    MapObjectMovement_FollowPartnerTrainer_Free,
    MapObjectMovement_FollowPartnerTrainer_Load
};

static const MovementTypeCallbacks sMovementTypeCallbacks_DisguiseSnow = {
    0x33,
    MapObjectMovement_DisguiseSnow_Init,
    MapObjectMovement_Disguise_Update,
    MapObjectMovement_Disguise_Free,
    MapObjectMovement_Disguise_Load
};

static const MovementTypeCallbacks sMovementTypeCallbacks_DisguiseSand = {
    0x34,
    MapObjectMovement_DisguiseSand_Init,
    MapObjectMovement_Disguise_Update,
    MapObjectMovement_Disguise_Free,
    MapObjectMovement_Disguise_Load
};

static const MovementTypeCallbacks sMovementTypeCallbacks_DisguiseRock = {
    0x35,
    MapObjectMovement_DisguiseRock_Init,
    MapObjectMovement_Disguise_Update,
    MapObjectMovement_Disguise_Free,
    MapObjectMovement_Disguise_Load
};

static const MovementTypeCallbacks sMovementTypeCallbacks_DisguiseGrass = {
    0x36,
    MapObjectMovement_DisguiseGrass_Init,
    MapObjectMovement_Disguise_Update,
    MapObjectMovement_Disguise_Free,
    MapObjectMovement_Disguise_Load
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayer = {
    0x37,
    MapObjectMovement_WalkWithPlayer_Init,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayer056 = {
    0x38,
    MapObjectMovement_WalkWithPlayer_Init056,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayer057 = {
    0x39,
    MapObjectMovement_WalkWithPlayer_Init057,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayer058 = {
    0x3A,
    MapObjectMovement_WalkWithPlayer_Init058,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayerTallGrass = {
    0x3B,
    MapObjectMovement_WalkWithPlayerTallGrass_Init,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayerTallGrass060 = {
    0x3C,
    MapObjectMovement_WalkWithPlayerTallGrass_Init060,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayerTallGrass061 = {
    0x3D,
    MapObjectMovement_WalkWithPlayerTallGrass_Init061,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WalkWithPlayerTallGrass062 = {
    0x3E,
    MapObjectMovement_WalkWithPlayerTallGrass_Init062,
    MapObjectMovement_WalkWithPlayer_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderAvoidObstacles = {
    0x3f,
    MapObjectMovement_WanderAvoidObstacles_Init,
    MapObjectMovement_WanderAvoidObstacles_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderAvoidObstacles064 = {
    0x40,
    MapObjectMovement_WanderAvoidObstacles_Init064,
    MapObjectMovement_WanderAvoidObstacles_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderAvoidObstacles065 = {
    0x41,
    MapObjectMovement_WanderAvoidObstacles_Init065,
    MapObjectMovement_WanderAvoidObstacles_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

static const MovementTypeCallbacks sMovementTypeCallbacks_WanderAvoidObstacles066 = {
    0x42,
    MapObjectMovement_WanderAvoidObstacles_Init066,
    MapObjectMovement_WanderAvoidObstacles_Update,
    MapObjectMovement_NoOp3,
    MapObjectMovement_NoOp4
};

// Movement-type dispatch table, indexed by MOVEMENT_TYPE_*. The order must
// match generated/movement_types.txt.
const MovementTypeCallbacks *const gMovementTypeCallbacks[] = {
    &sMovementTypeCallbacks_None,
    &sMovementTypeCallbacks_Player,
    &sMovementTypeCallbacks_LookAround,
    &sMovementTypeCallbacks_WanderAround,
    &sMovementTypeCallbacks_WanderNorthAndSouth,
    &sMovementTypeCallbacks_WanderWestAndEast,
    &sMovementTypeCallbacks_LookNorthAndWest,
    &sMovementTypeCallbacks_LookNorthAndEast,
    &sMovementTypeCallbacks_LookSouthAndWest,
    &sMovementTypeCallbacks_LookSouthAndEast,
    &sMovementTypeCallbacks_LookNorthSouthAndWest,
    &sMovementTypeCallbacks_LookNorthSouthAndEast,
    &sMovementTypeCallbacks_LookNorthWestAndEast,
    &sMovementTypeCallbacks_LookSouthWestAndEast,
    &sMovementTypeCallbacks_LookNorth,
    &sMovementTypeCallbacks_LookSouth,
    &sMovementTypeCallbacks_LookWest,
    &sMovementTypeCallbacks_LookEast,
    &sMovementTypeCallbacks_RotateCounterclockwise,
    &sMovementTypeCallbacks_RotateClockwise,
    &sMovementTypeCallbacks_WalkBackAndForth,
    &sMovementTypeCallbacks_WalkNorthEastWestSouth,
    &sMovementTypeCallbacks_WalkEastWestSouthNorth,
    &sMovementTypeCallbacks_WalkSouthNorthEastWest,
    &sMovementTypeCallbacks_WalkWestSouthNorthEast,
    &sMovementTypeCallbacks_WalkWestEastSouthNorth,
    &sMovementTypeCallbacks_WalkNorthWestEastSouth,
    &sMovementTypeCallbacks_WalkSouthNorthWestEast,
    &sMovementTypeCallbacks_WalkEastSouthNorthWest,
    &sMovementTypeCallbacks_WalkWestNorthSouthEast,
    &sMovementTypeCallbacks_WalkNorthSouthEastWest,
    &sMovementTypeCallbacks_WalkEastWestNorthSouth,
    &sMovementTypeCallbacks_WalkSouthEastWestNorth,
    &sMovementTypeCallbacks_WalkEastNorthSouthWest,
    &sMovementTypeCallbacks_WalkNorthSouthWestEast,
    &sMovementTypeCallbacks_WalkWestEastNorthSouth,
    &sMovementTypeCallbacks_WalkSouthWestEastNorth,
    &sMovementTypeCallbacks_WalkNorthWestSouthEast,
    &sMovementTypeCallbacks_WalkSouthEastNorthWest,
    &sMovementTypeCallbacks_WalkWestSouthEastNorth,
    &sMovementTypeCallbacks_WalkEastNorthWestSouth,
    &sMovementTypeCallbacks_WalkNorthEastSouthWest,
    &sMovementTypeCallbacks_WalkSouthWestNorthEast,
    &sMovementTypeCallbacks_WalkWestNorthEastSouth,
    &sMovementTypeCallbacks_WalkEastSouthWestNorth,
    &sMovementTypeCallbacks_LookNorthAndSouth,
    &sMovementTypeCallbacks_LookWestAndEast,
    &sMovementTypeCallbacks_BerrySoil,
    &sMovementTypeCallbacks_FollowPlayer,
    &sMovementTypeCallbacks_Spin,
    &sMovementTypeCallbacks_FollowPartnerTrainer,
    &sMovementTypeCallbacks_DisguiseSnow,
    &sMovementTypeCallbacks_DisguiseSand,
    &sMovementTypeCallbacks_DisguiseRock,
    &sMovementTypeCallbacks_DisguiseGrass,
    &sMovementTypeCallbacks_WalkWithPlayer,
    &sMovementTypeCallbacks_WalkWithPlayer056,
    &sMovementTypeCallbacks_WalkWithPlayer057,
    &sMovementTypeCallbacks_WalkWithPlayer058,
    &sMovementTypeCallbacks_WalkWithPlayerTallGrass,
    &sMovementTypeCallbacks_WalkWithPlayerTallGrass060,
    &sMovementTypeCallbacks_WalkWithPlayerTallGrass061,
    &sMovementTypeCallbacks_WalkWithPlayerTallGrass062,
    &sMovementTypeCallbacks_WanderAvoidObstacles,
    &sMovementTypeCallbacks_WanderAvoidObstacles064,
    &sMovementTypeCallbacks_WanderAvoidObstacles065,
    &sMovementTypeCallbacks_WanderAvoidObstacles066,
    &sMovementTypeCallbacks_WanderWestAndEastAlt
};

// Movement-action dispatch table, indexed by enum MovementAction.
BOOL (*const *const gMovementActionFuncs[MAX_MOVEMENT_ACTION])(MapObject *) = {
    [MOVEMENT_ACTION_FACE_NORTH] = gMovementActionFuncs_FaceNorth,
    [MOVEMENT_ACTION_FACE_SOUTH] = gMovementActionFuncs_FaceSouth,
    [MOVEMENT_ACTION_FACE_WEST] = gMovementActionFuncs_FaceWest,
    [MOVEMENT_ACTION_FACE_EAST] = gMovementActionFuncs_FaceEast,
    [MOVEMENT_ACTION_WALK_SLOWER_NORTH] = gMovementActionFuncs_WalkSlowerNorth,
    [MOVEMENT_ACTION_WALK_SLOWER_SOUTH] = gMovementActionFuncs_WalkSlowerSouth,
    [MOVEMENT_ACTION_WALK_SLOWER_WEST] = gMovementActionFuncs_WalkSlowerWest,
    [MOVEMENT_ACTION_WALK_SLOWER_EAST] = gMovementActionFuncs_WalkSlowerEast,
    [MOVEMENT_ACTION_WALK_SLOW_NORTH] = gMovementActionFuncs_WalkSlowNorth,
    [MOVEMENT_ACTION_WALK_SLOW_SOUTH] = gMovementActionFuncs_WalkSlowSouth,
    [MOVEMENT_ACTION_WALK_SLOW_WEST] = gMovementActionFuncs_WalkSlowWest,
    [MOVEMENT_ACTION_WALK_SLOW_EAST] = gMovementActionFuncs_WalkSlowEast,
    [MOVEMENT_ACTION_WALK_NORMAL_NORTH] = gMovementActionFuncs_WalkNormalNorth,
    [MOVEMENT_ACTION_WALK_NORMAL_SOUTH] = gMovementActionFuncs_WalkNormalSouth,
    [MOVEMENT_ACTION_WALK_NORMAL_WEST] = gMovementActionFuncs_WalkNormalWest,
    [MOVEMENT_ACTION_WALK_NORMAL_EAST] = gMovementActionFuncs_WalkNormalEast,
    [MOVEMENT_ACTION_WALK_FAST_NORTH] = gMovementActionFuncs_WalkFastNorth,
    [MOVEMENT_ACTION_WALK_FAST_SOUTH] = gMovementActionFuncs_WalkFastSouth,
    [MOVEMENT_ACTION_WALK_FAST_WEST] = gMovementActionFuncs_WalkFastWest,
    [MOVEMENT_ACTION_WALK_FAST_EAST] = gMovementActionFuncs_WalkFastEast,
    [MOVEMENT_ACTION_WALK_FASTER_NORTH] = gMovementActionFuncs_WalkFasterNorth,
    [MOVEMENT_ACTION_WALK_FASTER_SOUTH] = gMovementActionFuncs_WalkFasterSouth,
    [MOVEMENT_ACTION_WALK_FASTER_WEST] = gMovementActionFuncs_WalkFasterWest,
    [MOVEMENT_ACTION_WALK_FASTER_EAST] = gMovementActionFuncs_WalkFasterEast,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_NORTH] = gMovementActionFuncs_WalkOnSpotSlowerNorth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_SOUTH] = gMovementActionFuncs_WalkOnSpotSlowerSouth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_WEST] = gMovementActionFuncs_WalkOnSpotSlowerWest,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_EAST] = gMovementActionFuncs_WalkOnSpotSlowerEast,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH] = gMovementActionFuncs_WalkOnSpotSlowNorth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_SOUTH] = gMovementActionFuncs_WalkOnSpotSlowSouth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_WEST] = gMovementActionFuncs_WalkOnSpotSlowWest,
    [MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_EAST] = gMovementActionFuncs_WalkOnSpotSlowEast,
    [MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_NORTH] = gMovementActionFuncs_WalkOnSpotNormalNorth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_SOUTH] = gMovementActionFuncs_WalkOnSpotNormalSouth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_WEST] = gMovementActionFuncs_WalkOnSpotNormalWest,
    [MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_EAST] = gMovementActionFuncs_WalkOnSpotNormalEast,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FAST_NORTH] = gMovementActionFuncs_WalkOnSpotFastNorth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FAST_SOUTH] = gMovementActionFuncs_WalkOnSpotFastSouth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FAST_WEST] = gMovementActionFuncs_WalkOnSpotFastWest,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FAST_EAST] = gMovementActionFuncs_WalkOnSpotFastEast,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_NORTH] = gMovementActionFuncs_WalkOnSpotFasterNorth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_SOUTH] = gMovementActionFuncs_WalkOnSpotFasterSouth,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_WEST] = gMovementActionFuncs_WalkOnSpotFasterWest,
    [MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_EAST] = gMovementActionFuncs_WalkOnSpotFasterEast,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_NORTH] = gMovementActionFuncs_JumpOnSpotSlowNorth,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_SOUTH] = gMovementActionFuncs_JumpOnSpotSlowSouth,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_WEST] = gMovementActionFuncs_JumpOnSpotSlowWest,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_EAST] = gMovementActionFuncs_JumpOnSpotSlowEast,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_NORTH] = gMovementActionFuncs_JumpOnSpotFastNorth,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_SOUTH] = gMovementActionFuncs_JumpOnSpotFastSouth,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_WEST] = gMovementActionFuncs_JumpOnSpotFastWest,
    [MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_EAST] = gMovementActionFuncs_JumpOnSpotFastEast,
    [MOVEMENT_ACTION_JUMP_NEAR_FAST_NORTH] = gMovementActionFuncs_JumpNearFastNorth,
    [MOVEMENT_ACTION_JUMP_NEAR_FAST_SOUTH] = gMovementActionFuncs_JumpNearFastSouth,
    [MOVEMENT_ACTION_JUMP_NEAR_FAST_WEST] = gMovementActionFuncs_JumpNearFastWest,
    [MOVEMENT_ACTION_JUMP_NEAR_FAST_EAST] = gMovementActionFuncs_JumpNearFastEast,
    [MOVEMENT_ACTION_JUMP_FAR_NORTH] = gMovementActionFuncs_JumpFarNorth,
    [MOVEMENT_ACTION_JUMP_FAR_SOUTH] = gMovementActionFuncs_JumpFarSouth,
    [MOVEMENT_ACTION_JUMP_FAR_WEST] = gMovementActionFuncs_JumpFarWest,
    [MOVEMENT_ACTION_JUMP_FAR_EAST] = gMovementActionFuncs_JumpFarEast,
    [MOVEMENT_ACTION_DELAY_1] = gMovementActionFuncs_Delay1,
    [MOVEMENT_ACTION_DELAY_2] = gMovementActionFuncs_Delay2,
    [MOVEMENT_ACTION_DELAY_4] = gMovementActionFuncs_Delay4,
    [MOVEMENT_ACTION_DELAY_8] = gMovementActionFuncs_Delay8,
    [MOVEMENT_ACTION_DELAY_15] = gMovementActionFuncs_Delay15,
    [MOVEMENT_ACTION_DELAY_16] = gMovementActionFuncs_Delay16,
    [MOVEMENT_ACTION_DELAY_32] = gMovementActionFuncs_Delay32,
    [MOVEMENT_ACTION_WARP_OUT] = gMovementActionFuncs_WarpOut,
    [MOVEMENT_ACTION_WARP_IN] = gMovementActionFuncs_WarpIn,
    [MOVEMENT_ACTION_SET_INVISIBLE] = gMovementActionFuncs_SetInvisible,
    [MOVEMENT_ACTION_SET_VISIBLE] = gMovementActionFuncs_SetVisible,
    [MOVEMENT_ACTION_LOCK_DIR] = gMovementActionFuncs_LockDir,
    [MOVEMENT_ACTION_UNLOCK_DIR] = gMovementActionFuncs_UnlockDir,
    [MOVEMENT_ACTION_PAUSE_ANIMATION] = gMovementActionFuncs_PauseAnimation,
    [MOVEMENT_ACTION_RESUME_ANIMATION] = gMovementActionFuncs_ResumeAnimation,
    [MOVEMENT_ACTION_EMOTE_EXCLAMATION_MARK] = gMovementActionFuncs_EmoteExclamationMark,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_NORTH] = gMovementActionFuncs_WalkSlightlyFastNorth,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_SOUTH] = gMovementActionFuncs_WalkSlightlyFastSouth,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_WEST] = gMovementActionFuncs_WalkSlightlyFastWest,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_EAST] = gMovementActionFuncs_WalkSlightlyFastEast,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_NORTH] = gMovementActionFuncs_WalkSlightlyFasterNorth,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_SOUTH] = gMovementActionFuncs_WalkSlightlyFasterSouth,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_WEST] = gMovementActionFuncs_WalkSlightlyFasterWest,
    [MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_EAST] = gMovementActionFuncs_WalkSlightlyFasterEast,
    [MOVEMENT_ACTION_WALK_FASTEST_NORTH] = gMovementActionFuncs_WalkFastestNorth,
    [MOVEMENT_ACTION_WALK_FASTEST_SOUTH] = gMovementActionFuncs_WalkFastestSouth,
    [MOVEMENT_ACTION_WALK_FASTEST_WEST] = gMovementActionFuncs_WalkFastestWest,
    [MOVEMENT_ACTION_WALK_FASTEST_EAST] = gMovementActionFuncs_WalkFastestEast,
    [MOVEMENT_ACTION_RUN_NORTH] = gMovementActionFuncs_RunNorth,
    [MOVEMENT_ACTION_RUN_SOUTH] = gMovementActionFuncs_RunSouth,
    [MOVEMENT_ACTION_RUN_WEST] = gMovementActionFuncs_RunWest,
    [MOVEMENT_ACTION_RUN_EAST] = gMovementActionFuncs_RunEast,
    [MOVEMENT_ACTION_JUMP_NEAR_SLOW_WEST] = gMovementActionFuncs_JumpNearSlowWest,
    [MOVEMENT_ACTION_JUMP_NEAR_SLOW_EAST] = gMovementActionFuncs_JumpNearSlowEast,
    [MOVEMENT_ACTION_JUMP_FARTHER_WEST] = gMovementActionFuncs_JumpFartherWest,
    [MOVEMENT_ACTION_JUMP_FARTHER_EAST] = gMovementActionFuncs_JumpFartherEast,
    [MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_NORTH] = gMovementActionFuncs_WalkEverSoSlightlyFastNorth,
    [MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_SOUTH] = gMovementActionFuncs_WalkEverSoSlightlyFastSouth,
    [MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_WEST] = gMovementActionFuncs_WalkEverSoSlightlyFastWest,
    [MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_EAST] = gMovementActionFuncs_WalkEverSoSlightlyFastEast,
    [MOVEMENT_ACTION_POKECENTER_NURSE_BOW] = gMovementActionFuncs_PokecenterNurseBow,
    [MOVEMENT_ACTION_REVEAL_TRAINER] = gMovementActionFuncs_RevealTrainer,
    [MOVEMENT_ACTION_PLAYER_GIVE] = gMovementActionFuncs_PlayerGive,
    [MOVEMENT_ACTION_EMOTE_DOUBLE_EXCLAMATION_MARK] = gMovementActionFuncs_EmoteDoubleExclamationMark,
    [MOVEMENT_ACTION_PLAYER_RECEIVE] = gMovementActionFuncs_PlayerReceive,
    [MOVEMENT_ACTION_105] = gMovementActionFuncs_105,
    [MOVEMENT_ACTION_106] = gMovementActionFuncs_106,
    [MOVEMENT_ACTION_107] = gMovementActionFuncs_107,
    [MOVEMENT_ACTION_108] = gMovementActionFuncs_108,
    [MOVEMENT_ACTION_109] = gMovementActionFuncs_109,
    [MOVEMENT_ACTION_110] = gMovementActionFuncs_110,
    [MOVEMENT_ACTION_111] = gMovementActionFuncs_111,
    [MOVEMENT_ACTION_112] = gMovementActionFuncs_112,
    [MOVEMENT_ACTION_113] = gMovementActionFuncs_113,
    [MOVEMENT_ACTION_114] = gMovementActionFuncs_114,
    [MOVEMENT_ACTION_115] = gMovementActionFuncs_115,
    [MOVEMENT_ACTION_116] = gMovementActionFuncs_116,
    [MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_NORTH] = gMovementActionFuncs_JumpDistortionWorldNorth,
    [MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_SOUTH] = gMovementActionFuncs_JumpDistortionWorldSouth,
    [MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_WEST] = gMovementActionFuncs_JumpDistortionWorldWest,
    [MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_EAST] = gMovementActionFuncs_JumpDistortionWorldEast,
    [MOVEMENT_ACTION_121] = gMovementActionFuncs_121,
    [MOVEMENT_ACTION_122] = gMovementActionFuncs_122,
    [MOVEMENT_ACTION_123] = gMovementActionFuncs_123,
    [MOVEMENT_ACTION_124] = gMovementActionFuncs_124,
    [MOVEMENT_ACTION_125] = gMovementActionFuncs_125,
    [MOVEMENT_ACTION_126] = gMovementActionFuncs_126,
    [MOVEMENT_ACTION_127] = gMovementActionFuncs_127,
    [MOVEMENT_ACTION_128] = gMovementActionFuncs_128,
    [MOVEMENT_ACTION_129] = gMovementActionFuncs_129,
    [MOVEMENT_ACTION_130] = gMovementActionFuncs_130,
    [MOVEMENT_ACTION_131] = gMovementActionFuncs_131,
    [MOVEMENT_ACTION_132] = gMovementActionFuncs_132,
    [MOVEMENT_ACTION_133] = gMovementActionFuncs_133,
    [MOVEMENT_ACTION_134] = gMovementActionFuncs_134,
    [MOVEMENT_ACTION_135] = gMovementActionFuncs_135,
    [MOVEMENT_ACTION_136] = gMovementActionFuncs_136,
    [MOVEMENT_ACTION_137] = gMovementActionFuncs_137,
    [MOVEMENT_ACTION_138] = gMovementActionFuncs_138,
    [MOVEMENT_ACTION_139] = gMovementActionFuncs_139,
    [MOVEMENT_ACTION_140] = gMovementActionFuncs_140,
    [MOVEMENT_ACTION_141] = gMovementActionFuncs_141,
    [MOVEMENT_ACTION_142] = gMovementActionFuncs_142,
    [MOVEMENT_ACTION_143] = gMovementActionFuncs_143,
    [MOVEMENT_ACTION_144] = gMovementActionFuncs_144,
    [MOVEMENT_ACTION_145] = gMovementActionFuncs_145,
    [MOVEMENT_ACTION_146] = gMovementActionFuncs_146,
    [MOVEMENT_ACTION_147] = gMovementActionFuncs_147,
    [MOVEMENT_ACTION_148] = gMovementActionFuncs_148,
    [MOVEMENT_ACTION_149] = gMovementActionFuncs_149,
    [MOVEMENT_ACTION_150] = gMovementActionFuncs_150,
    [MOVEMENT_ACTION_151] = gMovementActionFuncs_151,
    [MOVEMENT_ACTION_152] = gMovementActionFuncs_152,
    [MOVEMENT_ACTION_153] = gMovementActionFuncs_153,
};

// Direction groups for movement actions with four facing variants. Each table
// is indexed by DIR_* and holds the movement action for that direction.
static const int sMovementActionCodes_Face[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_FACE_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_FACE_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_FACE_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_FACE_EAST,
};

static const int sMovementActionCodes_WalkSlower[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_SLOWER_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_SLOWER_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_SLOWER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_SLOWER_EAST,
};

static const int sMovementActionCodes_WalkSlow[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_SLOW_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_SLOW_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_SLOW_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_SLOW_EAST,
};

static const int sMovementActionCodes_WalkNormal[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_NORMAL_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_NORMAL_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_NORMAL_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_NORMAL_EAST,
};

static const int sMovementActionCodes_WalkFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_FAST_EAST,
};

static const int sMovementActionCodes_WalkFaster[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_FASTER_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_FASTER_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_FASTER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_FASTER_EAST,
};

static const int sMovementActionCodes_WalkOnSpotSlower[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_EAST,
};

static const int sMovementActionCodes_WalkOnSpotSlow[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_EAST,
};

static const int sMovementActionCodes_WalkOnSpotNormal[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_ON_SPOT_NORMAL_EAST,
};

static const int sMovementActionCodes_WalkOnSpotFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_ON_SPOT_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_ON_SPOT_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_ON_SPOT_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_ON_SPOT_FAST_EAST,
};

static const int sMovementActionCodes_WalkOnSpotFaster[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_ON_SPOT_FASTER_EAST,
};

static const int sMovementActionCodes_JumpOnSpotSlow[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_ON_SPOT_SLOW_EAST,
};

static const int sMovementActionCodes_JumpOnSpotFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_ON_SPOT_FAST_EAST,
};

static const int sMovementActionCodes_JumpNearFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_NEAR_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_NEAR_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_NEAR_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_NEAR_FAST_EAST,
};

static const int sMovementActionCodes_JumpFar[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_FAR_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_FAR_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_FAR_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_FAR_EAST,
};

static const int sMovementActionCodes_WalkSlightlyFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_EAST,
};

static const int sMovementActionCodes_WalkSlightlyFaster[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_SLIGHTLY_FASTER_EAST,
};

static const int sMovementActionCodes_WalkFastest[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_FASTEST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_FASTEST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_FASTEST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_FASTEST_EAST,
};

static const int sMovementActionCodes_Run[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_RUN_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_RUN_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_RUN_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_RUN_EAST,
};

static const int sMovementActionCodes_JumpNearSlow[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_NEAR_SLOW_WEST,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_NEAR_SLOW_EAST,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_NEAR_SLOW_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_NEAR_SLOW_EAST,
};

static const int sMovementActionCodes_JumpFarther[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_FARTHER_WEST,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_FARTHER_EAST,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_FARTHER_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_FARTHER_EAST,
};

static const int sMovementActionCodes_WalkEverSoSlightlyFast[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_EAST,
};

static const int sMovementActionCodes_JumpDistortionWorld[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_NORTH,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_SOUTH,
    [DIR_WEST] = MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_WEST,
    [DIR_EAST] = MOVEMENT_ACTION_JUMP_DISTORTION_WORLD_EAST,
};

static const int sMovementActionCodes_105[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_105,
    [DIR_SOUTH] = MOVEMENT_ACTION_106,
    [DIR_WEST] = MOVEMENT_ACTION_107,
    [DIR_EAST] = MOVEMENT_ACTION_108,
};

static const int sMovementActionCodes_109[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_109,
    [DIR_SOUTH] = MOVEMENT_ACTION_110,
    [DIR_WEST] = MOVEMENT_ACTION_111,
    [DIR_EAST] = MOVEMENT_ACTION_112,
};

static const int sMovementActionCodes_113[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_113,
    [DIR_SOUTH] = MOVEMENT_ACTION_114,
    [DIR_WEST] = MOVEMENT_ACTION_115,
    [DIR_EAST] = MOVEMENT_ACTION_116,
};

static const int sMovementActionCodes_145[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_145,
    [DIR_SOUTH] = MOVEMENT_ACTION_146,
    [DIR_WEST] = MOVEMENT_ACTION_147,
    [DIR_EAST] = MOVEMENT_ACTION_148,
};

static const int sMovementActionCodes_149[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_149,
    [DIR_SOUTH] = MOVEMENT_ACTION_150,
    [DIR_WEST] = MOVEMENT_ACTION_151,
    [DIR_EAST] = MOVEMENT_ACTION_152,
};

static const int sMovementActionCodes_121[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_121,
    [DIR_SOUTH] = MOVEMENT_ACTION_122,
    [DIR_WEST] = MOVEMENT_ACTION_123,
    [DIR_EAST] = MOVEMENT_ACTION_124,
};

static const int sMovementActionCodes_125[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_125,
    [DIR_SOUTH] = MOVEMENT_ACTION_126,
    [DIR_WEST] = MOVEMENT_ACTION_127,
    [DIR_EAST] = MOVEMENT_ACTION_128,
};

static const int sMovementActionCodes_129[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_129,
    [DIR_SOUTH] = MOVEMENT_ACTION_130,
    [DIR_WEST] = MOVEMENT_ACTION_131,
    [DIR_EAST] = MOVEMENT_ACTION_132,
};

static const int sMovementActionCodes_133[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_133,
    [DIR_SOUTH] = MOVEMENT_ACTION_134,
    [DIR_WEST] = MOVEMENT_ACTION_135,
    [DIR_EAST] = MOVEMENT_ACTION_136,
};

static const int sMovementActionCodes_137[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_137,
    [DIR_SOUTH] = MOVEMENT_ACTION_138,
    [DIR_WEST] = MOVEMENT_ACTION_139,
    [DIR_EAST] = MOVEMENT_ACTION_140,
};

static const int sMovementActionCodes_141[] = {
    [DIR_NORTH] = MOVEMENT_ACTION_141,
    [DIR_SOUTH] = MOVEMENT_ACTION_142,
    [DIR_WEST] = MOVEMENT_ACTION_143,
    [DIR_EAST] = MOVEMENT_ACTION_144,
};

// Table of direction groups, terminated by NULL. MovementAction_* scan this
// list to find the group containing a given movement action.
const int *const gMovementActionCodes[] = {
    sMovementActionCodes_Face,
    sMovementActionCodes_WalkSlower,
    sMovementActionCodes_WalkSlow,
    sMovementActionCodes_WalkNormal,
    sMovementActionCodes_WalkFast,
    sMovementActionCodes_WalkFaster,
    sMovementActionCodes_WalkOnSpotSlower,
    sMovementActionCodes_WalkOnSpotSlow,
    sMovementActionCodes_WalkOnSpotNormal,
    sMovementActionCodes_WalkOnSpotFast,
    sMovementActionCodes_WalkOnSpotFaster,
    sMovementActionCodes_JumpOnSpotSlow,
    sMovementActionCodes_JumpOnSpotFast,
    sMovementActionCodes_JumpNearFast,
    sMovementActionCodes_JumpFar,
    sMovementActionCodes_WalkSlightlyFast,
    sMovementActionCodes_WalkSlightlyFaster,
    sMovementActionCodes_WalkFastest,
    sMovementActionCodes_Run,
    sMovementActionCodes_JumpNearSlow,
    sMovementActionCodes_JumpFarther,
    sMovementActionCodes_WalkEverSoSlightlyFast,
    sMovementActionCodes_105,
    sMovementActionCodes_109,
    sMovementActionCodes_JumpDistortionWorld,
    sMovementActionCodes_113,
    sMovementActionCodes_121,
    sMovementActionCodes_125,
    sMovementActionCodes_129,
    sMovementActionCodes_133,
    sMovementActionCodes_137,
    sMovementActionCodes_141,
    sMovementActionCodes_145,
    sMovementActionCodes_149,
    NULL
};
