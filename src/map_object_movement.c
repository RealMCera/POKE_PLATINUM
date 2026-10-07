#include "map_object_movement.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"
#include "struct_decls/map_object_manager.h"

#include "field/field_system.h"
#include "overlay005/field_effect_manager.h"
#include "overlay005/ov5_021F3D00.h"

#include "map_object.h"
#include "map_object_move.h"
#include "map_tile_behavior.h"
#include "overworld_anim_manager.h"
#include "player_avatar.h"
#include "terrain_collision_manager.h"
#include "trainer_encounter.h"
#include "map_object_animation.h"

// This module implements the movement behaviors (selected through the
// gMovementTypeCallbacks table in movement_action_data.c) for several overworld NPCs:
//   - MOVEMENT_TYPE_FOLLOW_PLAYER: a partner trainer / follower that trails the
//     player, stepping onto the tile the player just left.
//   - MOVEMENT_TYPE_FOLLOW_PARTNER_TRAINER: an object that mirrors the movement
//     of a partner sharing its trainer ID and map header.
//   - MOVEMENT_TYPE_DISGUISE_*: a hidden trainer disguised as scenery, revealed
//     when the player approaches.
//   - MOVEMENT_TYPE_055..062: an object that walks in the player's facing
//     direction at the player's pace.
//   - MOVEMENT_TYPE_063..066: an object that wanders, turning aside when the
//     way ahead is blocked.
// Each behavior keeps its state in the map object's opaque unk_D8 scratch
// buffer and drives a small callback state machine from its update function.

// State for MOVEMENT_TYPE_FOLLOW_PLAYER. The follower remembers where the
// player was and walks toward the player's previous tile whenever it changes.
typedef struct {
    u8 state; // index into sFollowPlayerCallbacks
    u8 trackingPlayer; // whether playerX/playerZ hold a valid player position
    s16 playerX; // last recorded player x
    s16 playerZ; // last recorded player z
    u16 unk_06; // unused; set to 0xFF when tracking starts
} FollowPlayerData;

// State for MOVEMENT_TYPE_FOLLOW_PARTNER_TRAINER. The object follows a partner
// object that shares its trainer ID and map header.
typedef struct {
    u8 state; // index into sFollowPartnerTrainerCallbacks
    u8 trackingPartner; // whether partnerX/partnerZ/partner are valid
    s16 partnerX; // last recorded partner x
    s16 partnerZ; // last recorded partner z
    u16 unk_06; // unused; set to 0xFF when tracking starts
    MapObject *partner; // the partner object being followed
} FollowPartnerTrainerData;

// State for the MOVEMENT_TYPE_DISGUISE_* behaviors. The object is hidden under
// a scenery model until revealed; disguiseType selects which model.
typedef struct {
    u8 state; // index into sDisguiseCallbacks
    u8 disguiseType; // 0 = snow, 1 = sand, 2 = rock, 3 = grass
    u8 revealed; // set once the disguise has been revealed
    u8 unk_03; // unused
    OverworldAnimManager *animManager; // the disguise model's animation
} DisguiseData;

// State for MOVEMENT_TYPE_055..062. The object walks in the player's current
// facing direction, matching the player's movement speed.
typedef struct {
    u8 state; // index into sWalkWithPlayerCallbacks
    s8 playerDir; // cached player facing direction; -1 until first read
    u8 checkTallGrass; // if set, very tall grass counts as an obstacle
    u8 unk_03; // unused
} WalkWithPlayerData;

// State for MOVEMENT_TYPE_063..066. The object wanders one tile at a time,
// trying side/diagonal directions when the way ahead is blocked.
typedef struct {
    u32 state; // index into sWanderAvoidObstaclesCallbacks
    int mode; // 2 enables trying the opposite direction when blocked
    int dirIndex; // 0/1 selects which side to try first
} WanderAvoidObstaclesData;

static int FollowPlayer_EnsurePlayerTracked(MapObject *mapObj, FollowPlayerData *param1);
static void FollowPlayer_RecordPlayerPos(MapObject *mapObj, FollowPlayerData *param1);
static int FollowPlayer_HasPlayerMoved(MapObject *mapObj, FollowPlayerData *param1);
static void FollowPlayer_UpdatePlayerPos(MapObject *mapObj, FollowPlayerData *param1);
static u32 FollowPlayer_GetPlayerMovementAction(MapObject *mapObj);
static int FollowPlayer_MoveTowardPlayer(MapObject *mapObj);
static int FollowPartnerTrainer_EnsurePartnerTracked(MapObject *mapObj, FollowPartnerTrainerData *param1);
static void FollowPartnerTrainer_RecordPartnerPos(MapObject *mapObj, FollowPartnerTrainerData *param1, MapObject *param2);
static int FollowPartnerTrainer_HasPartnerMoved(MapObject *mapObj, FollowPartnerTrainerData *param1);
static int FollowPartnerTrainer_MoveTowardPartner(MapObject *mapObj, FollowPartnerTrainerData *param1);
static int WanderAvoidObstacles_PickDirection(MapObject *mapObj, WanderAvoidObstaclesData *param1, int param2);

// Per-behavior callback tables. Each update function indexes its table with the
// behavior's `state` and loops while the callback returns TRUE.
int (*const sFollowPlayerCallbacks[])(MapObject *, FollowPlayerData *);
int (*const sFollowPartnerTrainerCallbacks[])(MapObject *, FollowPartnerTrainerData *);
int (*const sDisguiseCallbacks[])(MapObject *, DisguiseData *);

void MapObjectMovement_FollowPlayer_Init(MapObject *mapObj)
{
    FollowPlayerData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(FollowPlayerData)));

    FollowPlayer_EnsurePlayerTracked(mapObj, v0);
    MapObject_SetUnkA0(mapObj, 0);
    MapObject_ClearStatus1(mapObj);
    MapObject_SetStatus18(mapObj, 0);
}

void MapObjectMovement_FollowPlayer_Update(MapObject *mapObj)
{
    FollowPlayerData *v0 = MapObject_GetUnkD8(mapObj);

    if (FollowPlayer_EnsurePlayerTracked(mapObj, v0) == 0) {
        return;
    }

    MapObject_SetStatus18(mapObj, 0);

    while (sFollowPlayerCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

void MapObjectMovement_FollowPlayer_Load(MapObject *mapObj)
{
    return;
}

static int FollowPlayer_Step(MapObject *mapObj, FollowPlayerData *param1)
{
    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    if (FollowPlayer_HasPlayerMoved(mapObj, param1) == 1) {
        FollowPlayer_UpdatePlayerPos(mapObj, param1);

        if (FollowPlayer_MoveTowardPlayer(mapObj) == 1) {
            MapObject_SetStatus1(mapObj);
            param1->state++;
            return TRUE;
        }
    }

    return FALSE;
}

static int FollowPlayer_ResetIfIdle(MapObject *mapObj, FollowPlayerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        MapObject_ClearStatus1(mapObj);
        param1->state = 0;
    }

    return 0;
}

// FollowPlayer_Step tries to take a step toward the player; once it starts
// moving, FollowPlayer_ResetIfIdle waits for the step to finish and resets.
static int (*const sFollowPlayerCallbacks[])(MapObject *, FollowPlayerData *) = {
    FollowPlayer_Step,
    FollowPlayer_ResetIfIdle
};

static int FollowPlayer_EnsurePlayerTracked(MapObject *mapObj, FollowPlayerData *param1)
{
    const MapObjectManager *v0 = MapObject_MapObjectManager(mapObj);
    MapObject *v1 = MapObjectMan_GetPlayerMapObject(v0);

    if (v1 == NULL) {
        param1->trackingPlayer = 0;
        return 0;
    }

    if (param1->trackingPlayer == 0) {
        FollowPlayer_RecordPlayerPos(mapObj, param1);
    }

    return 1;
}

static void FollowPlayer_RecordPlayerPos(MapObject *mapObj, FollowPlayerData *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    param1->trackingPlayer = 1;
    param1->playerX = PlayerAvatar_GetXPos(playerAvatar);
    param1->playerZ = PlayerAvatar_GetZPos(playerAvatar);
    param1->unk_06 = 0xff;
}

static int FollowPlayer_HasPlayerMoved(MapObject *mapObj, FollowPlayerData *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    if (playerAvatar != NULL) {
        int v2 = PlayerAvatar_GetXPos(playerAvatar);
        int v3 = PlayerAvatar_GetZPos(playerAvatar);

        if ((v2 != param1->playerX) || (v3 != param1->playerZ)) {
            return 1;
        }
    }

    return 0;
}

static void FollowPlayer_UpdatePlayerPos(MapObject *mapObj, FollowPlayerData *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    param1->playerX = PlayerAvatar_GetXPos(playerAvatar);
    param1->playerZ = PlayerAvatar_GetZPos(playerAvatar);
}

static u32 FollowPlayer_GetPlayerMovementAction(MapObject *mapObj)
{
    u32 v0;
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    v0 = PlayerAvatar_GetMovementAction(playerAvatar);

    // The follower cannot run, so translate the player's run actions into the
    // equivalent fast-walk actions.
    switch (v0) {
    case MOVEMENT_ACTION_RUN_NORTH:
        v0 = MOVEMENT_ACTION_WALK_FAST_NORTH;
        break;
    case MOVEMENT_ACTION_RUN_SOUTH:
        v0 = MOVEMENT_ACTION_WALK_FAST_SOUTH;
        break;
    case MOVEMENT_ACTION_RUN_WEST:
        v0 = MOVEMENT_ACTION_WALK_FAST_WEST;
        break;
    case MOVEMENT_ACTION_RUN_EAST:
        v0 = MOVEMENT_ACTION_WALK_FAST_EAST;
        break;
    }

    return v0;
}

static int FollowPlayer_MoveTowardPlayer(MapObject *mapObj)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);
    int xCur = MapObject_GetX(mapObj);
    int zCur = MapObject_GetZ(mapObj);
    int xPrev = PlayerAvatar_XPosPrev(playerAvatar);
    int zPrev = PlayerAvatar_ZPosPrev(playerAvatar);

    if (xCur != xPrev || zCur != zPrev) {
        u32 v6 = FollowPlayer_GetPlayerMovementAction(mapObj);
        int dir = GetDirectionBetweenPoints(xCur, zCur, xPrev, zPrev);

        v6 = MovementAction_TurnActionTowardsDir(dir, v6);
        LocalMapObj_SetMovementAction(mapObj, v6);

        return 1;
    }

    return 0;
}

void MapObjectMovement_FollowPartnerTrainer_Init(MapObject *mapObj)
{
    FollowPartnerTrainerData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(FollowPartnerTrainerData)));

    FollowPartnerTrainer_EnsurePartnerTracked(mapObj, v0);
    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);

    v0->trackingPartner = 0;
}

void MapObjectMovement_FollowPartnerTrainer_Update(MapObject *mapObj)
{
    FollowPartnerTrainerData *v0 = MapObject_GetUnkD8(mapObj);

    if (FollowPartnerTrainer_EnsurePartnerTracked(mapObj, v0) == 0) {
        return;
    }

    while (sFollowPartnerTrainerCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

void MapObjectMovement_FollowPartnerTrainer_Free(MapObject *mapObj)
{
    return;
}

void MapObjectMovement_FollowPartnerTrainer_Load(MapObject *mapObj)
{
    FollowPartnerTrainerData *v0 = MapObject_GetUnkD8(mapObj);
    v0->trackingPartner = 0;
}

static int FollowPartnerTrainer_Step(MapObject *mapObj, FollowPartnerTrainerData *param1)
{
    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    if (FollowPartnerTrainer_HasPartnerMoved(mapObj, param1) == 1) {
        if (FollowPartnerTrainer_MoveTowardPartner(mapObj, param1) == 1) {
            MapObject_SetStatus1(mapObj);
            param1->state++;
            return 1;
        }
    }

    return 0;
}

static int FollowPartnerTrainer_ResetIfIdle(MapObject *mapObj, FollowPartnerTrainerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 0) {
        return 0;
    }

    MapObject_ClearStatus1(mapObj);
    param1->state = 0;
    return 0;
}

// FollowPartnerTrainer_Step steps toward the partner; FollowPartnerTrainer_ResetIfIdle
// waits for the step to finish and resets.
static int (*const sFollowPartnerTrainerCallbacks[])(MapObject *, FollowPartnerTrainerData *) = {
    FollowPartnerTrainer_Step,
    FollowPartnerTrainer_ResetIfIdle
};

// Returns the map object that shares this object's trainer ID and map header,
// i.e. the partner this object should follow. Only trainer types 1-8 (the
// trainer classes that can appear as overworld partners) are considered.
MapObject *MapObjectMovement_FindPartnerTrainer(MapObject *mapObj)
{
    int v0 = 0;
    int v1 = MapObject_GetTrainerType(mapObj);
    enum MapHeaderID mapHeaderID = MapObject_GetMapHeaderID(mapObj);
    u32 v3 = MapObject_GetTrainerID(mapObj);
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);
    MapObject *v5;

    switch (v1) {
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
        while (MapObjectMan_FindObjectWithStatus(mapObjMan, &v5, &v0, (1 << 0)) == 1) {
            if ((mapObj != v5) && (MapObject_GetMapHeaderID(v5) == mapHeaderID)) {
                if (MapObject_GetTrainerID(v5) == v3) {
                    return v5;
                }
            }
        }
    }

    return NULL;
}

static int FollowPartnerTrainer_EnsurePartnerTracked(MapObject *mapObj, FollowPartnerTrainerData *param1)
{
    int v0;
    enum MapHeaderID v1;
    u32 v2;
    MapObject *v3;
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    v0 = 0;
    v1 = MapObject_GetMapHeaderID(mapObj);
    v2 = MapObject_GetTrainerID(mapObj);

    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &v3, &v0, (1 << 0)) == 1) {
        if ((mapObj != v3) && (MapObject_GetMapHeaderID(v3) == v1) && (MapObject_GetTrainerID(v3) == v2)) {
            if (param1->trackingPartner == 0) {
                FollowPartnerTrainer_RecordPartnerPos(mapObj, param1, v3);
            }

            return 1;
        }
    }

    param1->trackingPartner = 0;
    return 0;
}

static void FollowPartnerTrainer_RecordPartnerPos(MapObject *mapObj, FollowPartnerTrainerData *param1, MapObject *param2)
{
    param1->trackingPartner = 1;
    param1->partnerX = MapObject_GetX(param2);
    param1->partnerZ = MapObject_GetZ(param2);
    param1->unk_06 = 0xff;
    param1->partner = param2;
}

// The partner has moved if its previous tile differs from this object's tile
// and the partner is either mid-step or free to move (not paused/hidden).
static int FollowPartnerTrainer_HasPartnerMoved(MapObject *mapObj, FollowPartnerTrainerData *param1)
{
    MapObject *v0 = param1->partner;
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    int v3 = MapObject_GetXPrev(v0);
    int v4 = MapObject_GetZPrev(v0);

    if (((v1 != v3) || (v2 != v4)) && ((MapObject_IsMoving(v0) == 1) || (!MapObject_CheckStatus(v0, (MAP_OBJ_STATUS_11 | MAP_OBJ_STATUS_12 | MAP_OBJ_STATUS_PAUSE_MOVEMENT))))) {
        return 1;
    }

    return 0;
}

static int FollowPartnerTrainer_MoveTowardPartner(MapObject *mapObj, FollowPartnerTrainerData *param1)
{
    int v0 = MapObject_GetX(mapObj);
    int v1 = MapObject_GetZ(mapObj);
    int v2 = MapObject_GetX(param1->partner);
    int v3 = MapObject_GetZ(param1->partner);
    int v4 = MapObject_GetXPrev(param1->partner);
    int v5 = MapObject_GetZPrev(param1->partner);
    int v6;

    if ((v0 == v2) && (v1 == v3)) {
        return 0;
    }

    v6 = GetDirectionBetweenPoints(v0, v1, v4, v5);
    v0 += MapObject_GetDxFromDir(v6);
    v1 += MapObject_GetDzFromDir(v6);

    if ((v0 != v2) || (v1 != v3)) {
        u32 v7 = 0xc;

        v7 = MovementAction_TurnActionTowardsDir(v6, v7);
        LocalMapObj_SetMovementAction(mapObj, v7);
        return 1;
    }

    return 0;
}

// Hides the object under a scenery model (snow/sand/rock/grass) and drops it
// half a tile so the model sits on the ground.
static void Disguise_Init(MapObject *mapObj, int param1)
{
    DisguiseData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(DisguiseData)));

    v0->disguiseType = param1;

    MapObject_SetUnkA0(mapObj, 0x0);
    MapObject_ClearStatus1(mapObj);
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);

    VecFx32 v1 = { 0, FX32_ONE * -32, 0 };

    MapObject_SetSpriteJumpOffset(mapObj, &v1);
}

void MapObjectMovement_DisguiseSnow_Init(MapObject *mapObj)
{
    Disguise_Init(mapObj, 0);
}

void MapObjectMovement_DisguiseSand_Init(MapObject *mapObj)
{
    Disguise_Init(mapObj, 1);
}

void MapObjectMovement_DisguiseRock_Init(MapObject *mapObj)
{
    Disguise_Init(mapObj, 2);
}

void MapObjectMovement_DisguiseGrass_Init(MapObject *mapObj)
{
    Disguise_Init(mapObj, 3);
}

void MapObjectMovement_Disguise_Update(MapObject *mapObj)
{
    DisguiseData *v0 = MapObject_GetUnkD8(mapObj);

    while (sDisguiseCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

void MapObjectMovement_Disguise_Free(MapObject *mapObj)
{
    OverworldAnimManager *v0 = Disguise_GetAnimManager(mapObj);

    if (v0 != NULL) {
        FieldEffectManager_FinishAnimManager(v0);
    }
}

void MapObjectMovement_Disguise_Load(MapObject *mapObj)
{
    DisguiseData *v0 = MapObject_GetUnkD8(mapObj);

    v0->state = 0;
    Disguise_SetAnimManager(mapObj, NULL);

    if (v0->revealed == 0) {
        VecFx32 v1 = { 0, (FX32_ONE * -32), 0 };

        MapObject_SetSpriteJumpOffset(mapObj, &v1);
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    }
}

static int Disguise_StartAnim(MapObject *mapObj, DisguiseData *param1)
{
    if (param1->revealed == 0) {
        OverworldAnimManager *v0 = ov5_021F3D90(mapObj, param1->disguiseType);

        Disguise_SetAnimManager(mapObj, v0);
    }

    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);
    param1->state++;

    return 0;
}

static int Disguise_WaitForDraw(MapObject *mapObj, DisguiseData *param1)
{
    if (param1->revealed == 0) {
        OverworldAnimManager *v0 = Disguise_GetAnimManager(mapObj);

        if (v0 == NULL) {
            if (MapObject_IsDrawReady(mapObj) == 1) {
                v0 = ov5_021F3D90(mapObj, param1->disguiseType);
                Disguise_SetAnimManager(mapObj, v0);
            }
        }

        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    }

    return 0;
}

// Disguise_StartAnim creates the disguise model; Disguise_WaitForDraw retries
// until the object is ready to draw, then keeps the shadow hidden.
static int (*const sDisguiseCallbacks[])(MapObject *, DisguiseData *) = {
    Disguise_StartAnim,
    Disguise_WaitForDraw
};

void Disguise_SetAnimManager(MapObject *mapObj, OverworldAnimManager *param1)
{
    DisguiseData *v0 = MapObject_GetUnkD8(mapObj);
    v0->animManager = param1;
}

OverworldAnimManager *Disguise_GetAnimManager(MapObject *mapObj)
{
    DisguiseData *v0 = MapObject_GetUnkD8(mapObj);
    return v0->animManager;
}

void Disguise_MarkRevealed(MapObject *mapObj)
{
    DisguiseData *v0 = MapObject_GetUnkD8(mapObj);
    v0->revealed = 1;
}

// State machine: face north, wait for the turn, read the player's direction,
// walk one step in that direction, then wait for the step to finish.
static int (*const sWalkWithPlayerCallbacks[5])(MapObject *, WalkWithPlayerData *);

// param1 is unused (the eight MOVEMENT_TYPE_055..062 wrappers differ only in
// param2); param2 enables the very-tall-grass check.
static void WalkWithPlayer_Init(MapObject *mapObj, int param1, u32 param2)
{
    WalkWithPlayerData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WalkWithPlayerData)));

    v0->playerDir = -1;
    v0->checkTallGrass = param2;

    MapObject_TryFace(mapObj, 0);
}

void MapObjectMovement_WalkWithPlayer_Init(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 0, 0);
}

void MapObjectMovement_WalkWithPlayer_Init056(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 1, 0);
}

void MapObjectMovement_WalkWithPlayer_Init057(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 2, 0);
}

void MapObjectMovement_WalkWithPlayer_Init058(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 3, 0);
}

void MapObjectMovement_WalkWithPlayerTallGrass_Init(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 0, 1);
}

void MapObjectMovement_WalkWithPlayerTallGrass_Init060(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 1, 1);
}

void MapObjectMovement_WalkWithPlayerTallGrass_Init061(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 2, 1);
}

void MapObjectMovement_WalkWithPlayerTallGrass_Init062(MapObject *mapObj)
{
    WalkWithPlayer_Init(mapObj, 3, 1);
}

void MapObjectMovement_WalkWithPlayer_Update(MapObject *mapObj)
{
    WalkWithPlayerData *v0 = MapObject_GetUnkD8(mapObj);

    while (sWalkWithPlayerCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static int WalkWithPlayer_FaceNorth(MapObject *mapObj, WalkWithPlayerData *param1)
{
    int v0 = MapObject_GetFacingDir(mapObj);

    v0 = MovementAction_TurnActionTowardsDir(v0, MOVEMENT_ACTION_FACE_NORTH);

    LocalMapObj_SetMovementAction(mapObj, v0);
    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->state = 1;
    return 1;
}

static int WalkWithPlayer_WaitForTurn(MapObject *mapObj, WalkWithPlayerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        param1->state = 2;
        return 1;
    }

    return 0;
}

static int WalkWithPlayer_ReadPlayerDir(MapObject *mapObj, WalkWithPlayerData *param1)
{
    if (param1->playerDir == -1) {
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);

        param1->playerDir = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    }

    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->state = 3;
    return 1;
}

// Returns the tile behavior in the given direction, but only treats very tall
// grass as passable; every other tile sets bit 1 so the caller refuses to walk.
static u32 WalkWithPlayer_GetTileBehavior(MapObject *mapObj, int param1)
{
    u32 v0 = MapObject_GetTileBehaviorFromDir(mapObj, param1);

    if (TileBehavior_IsVeryTallGrass(v0) == 0) {
        v0 = (1 << 1);
    }

    v0 |= MapObject_CheckCollisionInDir(mapObj, param1);
    return v0;
}

// Walks one step in `dir` using `walkAction`, unless the tile is blocked, in
// which case the object just turns to face north.
static void WalkWithPlayer_WalkInDir(MapObject *mapObj, int param1, int param2, u32 param3)
{
    u32 v0;

    if (param3 == 0) {
        v0 = MapObject_CheckCollisionInDir(mapObj, param1);
    } else {
        v0 = WalkWithPlayer_GetTileBehavior(mapObj, param1);
    }

    if (v0 != 0) {
        param2 = MovementAction_TurnActionTowardsDir(param1, MOVEMENT_ACTION_FACE_NORTH);
    } else {
        param2 = MovementAction_TurnActionTowardsDir(param1, param2);
        MapObject_SetStatus1(mapObj);
    }

    LocalMapObj_SetMovementAction(mapObj, param2);
}

// Mirrors the player: walks in the player's facing direction using the walk
// action that matches the player's current movement speed.
static int WalkWithPlayer_Walk(MapObject *mapObj, WalkWithPlayerData *param1)
{
    int v0;
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v2 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    u32 v3 = PlayerAvatar_GetMovementActionSpeed(fieldSystem->playerAvatar);

    switch (v3) {
    case PLAYER_ACTION_SPEED_NONE:
    case PLAYER_ACTION_SPEED_NOT_MOVING:
        v0 = MovementAction_TurnActionTowardsDir(v2, MOVEMENT_ACTION_FACE_NORTH);
        LocalMapObj_SetMovementAction(mapObj, v0);
        break;
    case PLAYER_ACTION_SPEED_SLOWER:
        WalkWithPlayer_WalkInDir(mapObj, v2, MOVEMENT_ACTION_WALK_SLOWER_NORTH, param1->checkTallGrass);
        break;
    case PLAYER_ACTION_SPEED_SLOW:
        WalkWithPlayer_WalkInDir(mapObj, v2, MOVEMENT_ACTION_WALK_SLOW_NORTH, param1->checkTallGrass);
        break;
    case PLAYER_ACTION_SPEED_NORMAL:
        WalkWithPlayer_WalkInDir(mapObj, v2, MOVEMENT_ACTION_WALK_NORMAL_NORTH, param1->checkTallGrass);
        break;
    case PLAYER_ACTION_SPEED_FAST:
        WalkWithPlayer_WalkInDir(mapObj, v2, MOVEMENT_ACTION_WALK_FAST_NORTH, param1->checkTallGrass);
        break;
    case PLAYER_ACTION_SPEED_FASTER:
        WalkWithPlayer_WalkInDir(mapObj, v2, MOVEMENT_ACTION_WALK_FASTER_NORTH, param1->checkTallGrass);
        break;
    }

    param1->state = 4;
    return 1;
}

static int WalkWithPlayer_WaitForWalk(MapObject *mapObj, WalkWithPlayerData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 0) {
        return 0;
    }

    MapObject_ClearStatus1(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->state = 2;
    return 0;
}

static int (*const sWalkWithPlayerCallbacks[5])(MapObject *, WalkWithPlayerData *) = {
    WalkWithPlayer_FaceNorth,
    WalkWithPlayer_WaitForTurn,
    WalkWithPlayer_ReadPlayerDir,
    WalkWithPlayer_Walk,
    WalkWithPlayer_WaitForWalk
};

// State machine: start, pick a direction and walk one step, wait for the step
// to finish, then repeat.
static int (*const sWanderAvoidObstaclesCallbacks[3])(MapObject *, WanderAvoidObstaclesData *);

static void WanderAvoidObstacles_Init(MapObject *mapObj, int param1, int param2)
{
    WanderAvoidObstaclesData *v0 = MapObject_InitUnkD8(mapObj, (sizeof(WanderAvoidObstaclesData)));

    v0->mode = param1;
    v0->dirIndex = param2;
}

void MapObjectMovement_WanderAvoidObstacles_Init(MapObject *mapObj)
{
    WanderAvoidObstacles_Init(mapObj, 0, 0);
}

void MapObjectMovement_WanderAvoidObstacles_Init064(MapObject *mapObj)
{
    WanderAvoidObstacles_Init(mapObj, 1, 1);
}

void MapObjectMovement_WanderAvoidObstacles_Init065(MapObject *mapObj)
{
    WanderAvoidObstacles_Init(mapObj, 2, 0);
}

void MapObjectMovement_WanderAvoidObstacles_Init066(MapObject *mapObj)
{
    WanderAvoidObstacles_Init(mapObj, 2, 1);
}

void MapObjectMovement_WanderAvoidObstacles_Update(MapObject *mapObj)
{
    WanderAvoidObstaclesData *v0 = MapObject_GetUnkD8(mapObj);

    while (sWanderAvoidObstaclesCallbacks[v0->state](mapObj, v0) == 1) {
        (void)0;
    }
}

static int WanderAvoidObstacles_Start(MapObject *mapObj, WanderAvoidObstaclesData *param1)
{
    MapObject_ClearStatus1(mapObj);
    param1->state++;
    return 1;
}

static int WanderAvoidObstacles_Walk(MapObject *mapObj, WanderAvoidObstaclesData *param1)
{
    WanderAvoidObstacles_PickDirection(mapObj, param1, MOVEMENT_ACTION_WALK_NORMAL_NORTH);
    param1->state++;
    return 1;
}

static int WanderAvoidObstacles_WaitForWalk(MapObject *mapObj, WanderAvoidObstaclesData *param1)
{
    if (LocalMapObj_RunMovementAction(mapObj) == 1) {
        param1->state = 0;
        return 1;
    }

    return 0;
}

static int (*const sWanderAvoidObstaclesCallbacks[3])(MapObject *, WanderAvoidObstaclesData *) = {
    WanderAvoidObstacles_Start,
    WanderAvoidObstacles_Walk,
    WanderAvoidObstacles_WaitForWalk
};

// Direction offset tables, indexed by [direction][side] where side is 0 or 1.
// sSideCollision* give the tile to the left/right of the direction and
// sDiagonalCollision* give the diagonal tile; the collision helpers add these
// to the object's position before testing terrain collision.
static const int sSideCollisionDx[4][2] = {
    { -1, 0x1 },
    { 0x1, -1 },
    { 0x0, 0x0 },
    { 0x0, 0x0 }
};

static const int sSideCollisionDz[4][2] = {
    { 0x0, 0x0 },
    { 0x0, 0x0 },
    { 0x1, -1 },
    { -1, 0x1 }
};

static const int sDiagonalCollisionDx[4][2] = {
    { -1, 0x1 },
    { 0x1, -1 },
    { 0x1, 0x1 },
    { -1, -1 }
};

static const int sDiagonalCollisionDz[4][2] = {
    { 0x1, 0x1 },
    { -1, -1 },
    { 0x1, -1 },
    { -1, 0x1 }
};

// Maps [direction][side] to the perpendicular direction reached by turning
// left (sTurnLeftRight) or right (sTurnRightLeft).
static const int sTurnLeftRight[4][2] = {
    { 0x2, 0x3 },
    { 0x3, 0x2 },
    { 0x1, 0x0 },
    { 0x0, 0x1 }
};

static const int sTurnRightLeft[4][2] = {
    { 0x3, 0x2 },
    { 0x2, 0x3 },
    { 0x0, 0x1 },
    { 0x1, 0x0 }
};

// Flips the side index (0 <-> 1).
static const int sFlipSide[2] = {
    0x1,
    0x0
};

static BOOL WanderAvoidObstacles_CheckSideCollision(FieldSystem *fieldSystem, int param1, int param2, int param3, int param4)
{
    BOOL v0;

    param1 += sSideCollisionDx[param3][param4];
    param2 += sSideCollisionDz[param3][param4];

    v0 = TerrainCollisionManager_CheckCollision(fieldSystem, param1, param2);
    return v0;
}

static BOOL WanderAvoidObstacles_CheckDiagonalCollision(FieldSystem *fieldSystem, int param1, int param2, int param3, int param4)
{
    BOOL v0;

    param1 += sDiagonalCollisionDx[param3][param4];
    param2 += sDiagonalCollisionDz[param3][param4];

    v0 = TerrainCollisionManager_CheckCollision(fieldSystem, param1, param2);
    return v0;
}

static int WanderAvoidObstacles_CheckSideCollisionAtObject(MapObject *mapObj, int param1, int param2)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    BOOL v3 = WanderAvoidObstacles_CheckSideCollision(fieldSystem, v1, v2, param1, param2);

    return v3;
}

static int WanderAvoidObstacles_CheckDiagonalCollisionAtObject(MapObject *mapObj, int param1, int param2)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    BOOL v3 = WanderAvoidObstacles_CheckDiagonalCollision(fieldSystem, v1, v2, param1, param2);

    return v3;
}

// Returns `dir` unchanged if the side tile is clear, turns 90 degrees if only
// the diagonal tile is clear, or -1 if both are blocked.
static int WanderAvoidObstacles_AdjustDirection(MapObject *mapObj, int param1, int param2)
{
    if (WanderAvoidObstacles_CheckSideCollisionAtObject(mapObj, param1, param2) == 0) {
        if (WanderAvoidObstacles_CheckDiagonalCollisionAtObject(mapObj, param1, param2) == 0) {
            return -1;
        }

        param1 = sTurnLeftRight[param1][param2];
    }

    return param1;
}

// Adjusts *param1 to a walkable direction and returns the tile behavior there,
// or 0 if no direction is walkable.
static u32 WanderAvoidObstacles_GetWalkableDirBehavior(MapObject *mapObj, int *param1, int param2)
{
    u32 v0;

    *param1 = WanderAvoidObstacles_AdjustDirection(mapObj, *param1, param2);

    if (*param1 != -1) {
        v0 = MapObject_CheckCollisionInDir(mapObj, *param1);
        return v0;
    }

    return 0;
}

// Picks a direction to walk in. It first tries the current facing direction
// (adjusted around obstacles); if that is blocked it may try the opposite
// direction (mode 2 only) and then a perpendicular direction. If nothing is
// walkable it walks on the spot.
static int WanderAvoidObstacles_PickDirection(MapObject *mapObj, WanderAvoidObstaclesData *param1, int param2)
{
    u32 v0;
    int v1 = param1->dirIndex;
    int dir = MapObject_GetFacingDir(mapObj);

    v0 = WanderAvoidObstacles_GetWalkableDirBehavior(mapObj, &dir, v1);

    if (dir == -1) {
        dir = MapObject_GetFacingDir(mapObj);
        param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
        LocalMapObj_SetMovementAction(mapObj, param2);
        return 0;
    }

    if (v0 == 0) {
        param2 = MovementAction_TurnActionTowardsDir(dir, param2);
        MapObject_SetStatus1(mapObj);
        LocalMapObj_SetMovementAction(mapObj, param2);
        return 1;
    }

    if ((v0 & (1 << 0)) && (param1->mode == 2)) {
        dir = Direction_GetOpposite(MapObject_GetFacingDir(mapObj));
        v1 = sFlipSide[v1];
        param1->dirIndex = v1;

        v0 = WanderAvoidObstacles_GetWalkableDirBehavior(mapObj, &dir, v1);

        if (dir == -1) {
            dir = MapObject_GetFacingDir(mapObj);
            param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
            LocalMapObj_SetMovementAction(mapObj, param2);
            return 0;
        }

        if (v0 == 0) {
            param2 = MovementAction_TurnActionTowardsDir(dir, param2);
            MapObject_SetStatus1(mapObj);
            LocalMapObj_SetMovementAction(mapObj, param2);
            return 1;
        }
    }

    if (v0 & (1 << 1)) {
        dir = sTurnRightLeft[dir][v1];
        v0 = WanderAvoidObstacles_GetWalkableDirBehavior(mapObj, &dir, v1);

        if (dir == -1) {
            dir = MapObject_GetFacingDir(mapObj);
            param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
            LocalMapObj_SetMovementAction(mapObj, param2);
            return 0;
        }

        if (v0 == 0) {
            param2 = MovementAction_TurnActionTowardsDir(dir, param2);
            MapObject_SetStatus1(mapObj);
            LocalMapObj_SetMovementAction(mapObj, param2);
            return 1;
        }
    }

    dir = MapObject_GetFacingDir(mapObj);
    param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);

    LocalMapObj_SetMovementAction(mapObj, param2);
    return 0;
}
