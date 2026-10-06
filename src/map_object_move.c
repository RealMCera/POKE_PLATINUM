#include "map_object_move.h"

#include <nitro.h>
#include <string.h>

#include "constants/map_object.h"
#include "generated/movement_types.h"

#include "struct_decls/map_object.h"
#include "struct_decls/map_object_manager.h"

#include "field/field_system.h"
#include "overlay005/object_event_gfx_data.h"
#include "overlay005/ov5_021ECC20.h"
#include "overlay005/ov5_021F134C.h"
#include "overlay005/ov5_021F17B8.h"
#include "overlay005/ov5_021F1CC8.h"
#include "overlay005/ov5_021F2A7C.h"
#include "overlay005/ov5_021F2BD0.h"
#include "overlay005/ov5_021F2D20.h"
#include "overlay005/ov5_021F3284.h"
#include "overlay005/ov5_021F348C.h"
#include "overlay005/ov5_021F37A8.h"
#include "overlay005/ov5_021F3A50.h"

#include "map_object.h"
#include "map_tile_behavior.h"
#include "terrain_collision_manager.h"
#include "unk_020655F4.h"
#include "unk_020673B8.h"

// Movement for map objects and their interaction with the tile they occupy.
// Each frame MapObject_Move runs the object's movement action and then reacts
// to the tile it moved onto: sinking into soft terrain, rustling grass,
// leaving footprints and splashes, and updating its shadow and reflection. It
// also answers the collision queries used to decide whether a step is legal.
//
// The terrain-effect helpers below share a uniform signature (object, current
// tile behavior, previous tile behavior, gfx render details) so the movement
// dispatchers can invoke them as a sequence.

// Vertical sprite offsets, in pixels, applied while standing in soft terrain
// so the sprite appears to sink into snow or mud.
#define sinkInDeepSnowDistance    -12
#define sinkInDeeperSnowDistance  -14
#define sinkInDeepestSnowDistance -16
#define sinkInMudDistance         -12
#define sinkInDeepMudDistance     -14

static int MapObject_ShouldUpdateMovement(const MapObject *mapObj);
static void MapObject_RecalculateHeightIfFlagged(MapObject *mapObj);
static void MapObject_StartMovementIfTileBehaviorValid(MapObject *mapObj);
static void MapObject_StartMove(MapObject *mapObj);
static void MapObject_ProcessMovementFlags(MapObject *mapObj);
static void MapObject_EndMove(MapObject *mapObj);
static void MapObject_ApplyMoveInitEffects(MapObject *mapObj);
static void MapObject_ApplyStepStartEffects(MapObject *mapObj);
static void MapObject_ApplyJumpStartEffects(MapObject *mapObj);
static void MapObject_ApplyStepEndEffects(MapObject *mapObj);
static void MapObject_ApplyJumpEndEffects(MapObject *mapObj);
static void MapObject_SinkIntoTerrain(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_BendTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_RustleTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_LeaveFootprints(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_UpdateShallowWaterEffect(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_ClearShallowWaterEffect(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_UpdateShadowOnMoveStart(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_UpdateMovementShadow(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_RestoreMovementShadow(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_ShowLandingDust(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_BendVeryTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_RustleVeryTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_BendMudWithGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_RustleMudWithGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_CreatePuddleRippleAtPrevPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_CreatePuddleRippleAtPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_CreateMudSplashAtPrevPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_CreateMudSplashAtPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_InitReflection(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_UpdateReflection(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_DetermineElevatedBridgeStatus(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);
static void MapObject_EmptyFunction(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails);

// Direction-indexed predicates over a tile behavior: whether it blocks
// movement heading in `dir`, and whether it blocks movement arriving from
// `dir` (i.e. blocking the opposite direction).
static BOOL (*const sTileBehaviorBlocksMovementInDir[4])(u8);
static BOOL (*const sTileBehaviorBlocksMovementAgainstDir[4])(u8);

void MapObject_InitMove(MapObject *mapObj)
{
    MapObject_CallMovementInit(mapObj);
    TrainerFacing_Init(mapObj);
}

// Advances the object's movement by one frame and applies the terrain effects
// for any movement transition requested during the frame.
void MapObject_Move(MapObject *mapObj)
{
    if (MapObject_CheckManagerStatus(mapObj, 1 << 1)) {
        return;
    }

    MapObject_RecalculateHeightIfFlagged(mapObj);
    MapObject_StartMovementIfTileBehaviorValid(mapObj);
    MapObject_StartMove(mapObj);

    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_4)) {
        MapObject_DoMovementAction(mapObj);
    } else if (MapObject_IsMovementPaused(mapObj) == FALSE && MapObject_ShouldUpdateMovement(mapObj) == TRUE && TrainerFacing_Update(mapObj) == FALSE) {
        MapObject_CallMovementUpdate(mapObj);
    }

    MapObject_ProcessMovementFlags(mapObj);
    MapObject_EndMove(mapObj);
}

// Decides whether the movement action may run this frame. A pending height
// recalculation (STATUS_12) or an unresolved tile behavior (STATUS_11) blocks
// movement until it is fixed, unless the object is already mid-step or is the
// player's partner follower (MOVEMENT_TYPE_FOLLOW_PARTNER_TRAINER, 0x32).
static BOOL MapObject_ShouldUpdateMovement(const MapObject *mapObj)
{
    if (MapObject_IsMoving(mapObj) == TRUE) {
        return TRUE;
    }

    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_12 | MAP_OBJ_STATUS_11) == FALSE) {
        return TRUE;
    } else if (MapObject_GetMovementType(mapObj) == 0x32) {
        return TRUE;
    }

    u32 status = MapObject_GetStatus(mapObj);

    if (status & (1 << 12) && (status & (1 << 23)) == FALSE) {
        return FALSE;
    }

    if (status & (1 << 11) && MapObject_CheckFlags2Bit2(mapObj) == FALSE) {
        return FALSE;
    }

    return TRUE;
}

// If a deferred height recalculation is pending, immediately recompute the
// object's height from the terrain.
static void MapObject_RecalculateHeightIfFlagged(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_12)) {
        MapObject_RecalculateObjectHeight(mapObj);
    }
}

// If the object was left without a valid tile behavior (STATUS_11), re-sample
// it and request a movement start once the tile becomes valid again.
static void MapObject_StartMovementIfTileBehaviorValid(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_11) && MapObject_SetTileBehaviors(mapObj) == TRUE) {
        MapObject_SetStartMovement(mapObj);
    }
}

// Consumes a pending START_MOVEMENT flag and applies the matching terrain
// effects before the movement action runs.
static void MapObject_StartMove(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_START_MOVEMENT)) {
        MapObject_ApplyMoveInitEffects(mapObj);
    }

    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_START_MOVEMENT | MAP_OBJ_STATUS_START_JUMP);
}

// Applies jump/movement start effects for transitions requested by the
// movement action during this frame, and clears the corresponding flags.
static void MapObject_ProcessMovementFlags(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_START_JUMP)) {
        MapObject_ApplyJumpStartEffects(mapObj);
    } else if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_START_MOVEMENT)) {
        MapObject_ApplyStepStartEffects(mapObj);
    }

    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_START_MOVEMENT | MAP_OBJ_STATUS_START_JUMP);
}

// Applies jump/movement end effects and clears the corresponding flags.
static void MapObject_EndMove(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_END_JUMP)) {
        MapObject_ApplyJumpEndEffects(mapObj);
    } else if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_END_MOVEMENT)) {
        MapObject_ApplyStepEndEffects(mapObj);
    }

    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_END_MOVEMENT | MAP_OBJ_STATUS_END_JUMP);
}

// Terrain effects for a movement start that does not itself change the tile
// (SetStartMovement), e.g. turning in place: grass is bent without the full
// rustle and no footprints are left.
static void MapObject_ApplyMoveInitEffects(MapObject *mapObj)
{
    MapObject_SetTileBehaviors(mapObj);

    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        u8 currTileBehavior = MapObject_GetCurrTileBehavior(mapObj);
        u8 prevTileBehavior = MapObject_GetPrevTileBehavior(mapObj);
        const ObjectEventGfxRenderDetailsEntry *v2 = ov5_021ECD04(mapObj);

        MapObject_DetermineElevatedBridgeStatus(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_BendTallGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateShallowWaterEffect(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateShadowOnMoveStart(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_SinkIntoTerrain(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_BendVeryTallGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_BendMudWithGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_InitReflection(mapObj, currTileBehavior, prevTileBehavior, v2);
    }
}

// Terrain effects at the start of a tile-to-tile step. The movement action has
// already advanced the object, so the departed tile is still in the "previous"
// coordinates: that is where footprints and splashes belong.
static void MapObject_ApplyStepStartEffects(MapObject *mapObj)
{
    MapObject_SetTileBehaviors(mapObj);

    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        u8 currTileBehavior = MapObject_GetCurrTileBehavior(mapObj);
        u8 prevTileBehavior = MapObject_GetPrevTileBehavior(mapObj);
        const ObjectEventGfxRenderDetailsEntry *v2 = ov5_021ECD04(mapObj);

        MapObject_DetermineElevatedBridgeStatus(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RustleTallGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_LeaveFootprints(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateShallowWaterEffect(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateMovementShadow(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RustleVeryTallGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RustleMudWithGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreatePuddleRippleAtPrevPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreateMudSplashAtPrevPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_InitReflection(mapObj, currTileBehavior, prevTileBehavior, v2);

        MapObject_EmptyFunction(mapObj, currTileBehavior, prevTileBehavior, v2);
    }
}

// Terrain effects when a jump leaves the ground.
static void MapObject_ApplyJumpStartEffects(MapObject *mapObj)
{
    MapObject_SetTileBehaviors(mapObj);

    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        u8 currTileBehavior = MapObject_GetCurrTileBehavior(mapObj);
        u8 prevTileBehavior = MapObject_GetPrevTileBehavior(mapObj);
        const ObjectEventGfxRenderDetailsEntry *v2 = ov5_021ECD04(mapObj);

        MapObject_DetermineElevatedBridgeStatus(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateMovementShadow(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_InitReflection(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_ClearShallowWaterEffect(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_EmptyFunction(mapObj, currTileBehavior, prevTileBehavior, v2);
    }
}

// Terrain effects when a tile-to-tile step finishes; the object has arrived at
// the new tile, so the effects use the current coordinates.
static void MapObject_ApplyStepEndEffects(MapObject *mapObj)
{
    MapObject_SetTileBehaviors(mapObj);

    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        u8 currTileBehavior = MapObject_GetCurrTileBehavior(mapObj);
        u8 prevTileBehavior = MapObject_GetPrevTileBehavior(mapObj);
        const ObjectEventGfxRenderDetailsEntry *v2 = ov5_021ECD04(mapObj);

        MapObject_SinkIntoTerrain(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreatePuddleRippleAtPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreateMudSplashAtPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateShallowWaterEffect(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateReflection(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RestoreMovementShadow(mapObj, currTileBehavior, prevTileBehavior, v2);
    }
}

// Terrain effects when a jump lands.
static void MapObject_ApplyJumpEndEffects(MapObject *mapObj)
{
    MapObject_SetTileBehaviors(mapObj);

    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        u8 currTileBehavior = MapObject_GetCurrTileBehavior(mapObj);
        u8 prevTileBehavior = MapObject_GetPrevTileBehavior(mapObj);
        const ObjectEventGfxRenderDetailsEntry *v2 = ov5_021ECD04(mapObj);

        MapObject_SinkIntoTerrain(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreatePuddleRippleAtPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_CreateMudSplashAtPos(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateShallowWaterEffect(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_UpdateReflection(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RestoreMovementShadow(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_RustleTallGrass(mapObj, currTileBehavior, prevTileBehavior, v2);
        MapObject_ShowLandingDust(mapObj, currTileBehavior, prevTileBehavior, v2);
    }
}

// Offsets the sprite downward when standing on soft terrain; the deeper the
// snow or mud layer, the further it sinks. Any other tile resets the offset.
static void MapObject_SinkIntoTerrain(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (MapObject_CheckFlagDoNotSinkIntoTerrain(mapObj) == FALSE) {
        if (TileBehavior_IsDeepMud(currTileBehavior) == TRUE || TileBehavior_IsDeepMudWithGrass(currTileBehavior) == TRUE) {
            VecFx32 spriteOffset = { 0, FX32_ONE * sinkInDeepMudDistance, 0 };

            MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
            return;
        }

        if (TileBehavior_IsMud(currTileBehavior) == TRUE || TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE) {
            VecFx32 spriteOffset = { 0, FX32_ONE * sinkInMudDistance, 0 };

            MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
            return;
        }

        if (TileBehavior_IsDeepestSnow(currTileBehavior) == TRUE) {
            VecFx32 spriteOffset = { 0, FX32_ONE * sinkInDeepestSnowDistance, 0 };

            MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
            return;
        }

        if (TileBehavior_IsDeeperSnow(currTileBehavior) == TRUE) {
            VecFx32 spriteOffset = { 0, FX32_ONE * sinkInDeeperSnowDistance, 0 };

            MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
            return;
        }

        if (TileBehavior_IsDeepSnow(currTileBehavior) == TRUE) {
            VecFx32 spriteOffset = { 0, FX32_ONE * sinkInDeepSnowDistance, 0 };

            MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
            return;
        }
    }

    VecFx32 spriteOffset = { 0, 0, 0 };
    MapObject_SetSpriteTerrainOffset(mapObj, &spriteOffset);
}

// Bends the tall grass without replaying its rustle (the object is already
// standing on the tile).
static void MapObject_BendTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsTallGrass(currTileBehavior) == TRUE) {
        ov5_021F2EA4(mapObj, 0);
    }
}

// Plays the full tall grass rustle as the object steps through the tile.
static void MapObject_RustleTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsTallGrass(currTileBehavior) == TRUE) {
        ov5_021F2EA4(mapObj, 1);
    }
}

// Leaves the object's tracks on the tile it is leaving, choosing the effect
// from that tile's behavior and the object's track type (footsteps vs. bike
// line). Objects with no track type leave nothing.
static void MapObject_LeaveFootprints(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (renderDetails->trackType == 0) {
        return;
    }

    if (TileBehavior_IsSnowWithShadows(prevTileBehavior) == TRUE) {
        if (renderDetails->trackType == 1) {
            ov5_021F1EBC(mapObj);
        } else if (renderDetails->trackType == 2) {
            ov5_021F1EC8(mapObj);
        }
    }

    if (MapObject_IsOnSand(mapObj, prevTileBehavior) == TRUE) {
        if (renderDetails->trackType == 1) {
            ov5_021F1E8C(mapObj);
        } else if (renderDetails->trackType == 2) {
            ov5_021F1E98(mapObj);
        }
        return;
    }

    if (TileBehavior_IsDeeperSnow(prevTileBehavior) == TRUE
        || TileBehavior_IsDeepestSnow(prevTileBehavior) == TRUE
        || TileBehavior_IsDeepSnow(prevTileBehavior)) {
        ov5_021F1EB0(mapObj);
        return;
    }

    if (MapObject_IsOnShallowSnow(mapObj, prevTileBehavior) == TRUE) {
        ov5_021F1EA4(mapObj);
        return;
    }
}

// Creates the shallow-water ripple once when the object enters shallow water,
// and clears the associated status when it leaves.
static void MapObject_UpdateShallowWaterEffect(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsShallowWater(currTileBehavior) == TRUE) {
        if (MapObject_CheckStatus26(mapObj) == FALSE) {
            ov5_021F331C(mapObj, 1);
            MapObject_SetStatus26(mapObj, 1);
        }
    } else {
        MapObject_SetStatus26(mapObj, 0);
    }
}

static void MapObject_ClearShallowWaterEffect(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    MapObject_SetStatus26(mapObj, 0);
}

// Wrapper for MapObject_UpdateMovementShadow, kept separate because the move
// start sequence references it directly.
static void MapObject_UpdateShadowOnMoveStart(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    MapObject_UpdateMovementShadow(mapObj, currTileBehavior, prevTileBehavior, renderDetails);
}

// Hides the object's shadow on surfaces that should not receive one (grass,
// water, snow, mud, reflective tiles); otherwise creates the shadow if it is
// not already present.
static void MapObject_UpdateMovementShadow(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsMovementRunning(mapObjMan) == FALSE) {
        return;
    }

    if (renderDetails->hasShadow == 0) {
        return;
    }

    if (TileBehavior_IsTallGrass(currTileBehavior) == TRUE
        || TileBehavior_IsVeryTallGrass(currTileBehavior) == TRUE
        || MapObject_IsOnWater(mapObj, currTileBehavior) == TRUE
        || TileBehavior_IsPuddle(currTileBehavior) == TRUE
        || TileBehavior_IsShallowWater(currTileBehavior) == TRUE
        || MapObject_IsOnSnow(mapObj, currTileBehavior) == TRUE
        || TileBehavior_IsMud(currTileBehavior) == TRUE
        || TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE
        || TileBehavior_IsReflective(currTileBehavior)) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    } else {
        if (!MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_SHOW_SHADOW)) {
            ov5_021F1570(mapObj);
            MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_SHOW_SHADOW);
        }
    }
}

// Same surface test as MapObject_UpdateMovementShadow, but only toggles the
// hide-shadow status; it never creates a new shadow.
static void MapObject_RestoreMovementShadow(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsMovementRunning(mapObjMan) == FALSE) {
        return;
    }

    if (renderDetails->hasShadow == 0) {
        return;
    }

    if (TileBehavior_IsTallGrass(currTileBehavior) == TRUE
        || TileBehavior_IsVeryTallGrass(currTileBehavior) == TRUE
        || MapObject_IsOnWater(mapObj, currTileBehavior) == TRUE
        || TileBehavior_IsPuddle(currTileBehavior) == TRUE
        || TileBehavior_IsShallowWater(currTileBehavior) == TRUE
        || MapObject_IsOnSnow(mapObj, currTileBehavior) == TRUE
        || TileBehavior_IsMud(currTileBehavior) == TRUE
        || TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE
        || TileBehavior_IsReflective(currTileBehavior)) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    }
}

// Spawns a landing puff when a jump ends on solid ground, but not on water,
// ice, mud, or snow (those surfaces have their own effects).
static void MapObject_ShowLandingDust(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (MapObject_IsOnWater(mapObj, currTileBehavior) == TRUE
        || TileBehavior_IsShallowWater(currTileBehavior) == TRUE
        || TileBehavior_IsIce(currTileBehavior) == TRUE
        || TileBehavior_IsMud(currTileBehavior) == TRUE
        || TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE
        || MapObject_IsOnSnow(mapObj, currTileBehavior) == TRUE) {
        return;
    }

    ov5_021F3638(mapObj);
}

// Immediate (Bend) and fully animated (Rustle) variants of the very tall
// grass effect, mirroring the tall grass pair above.
static void MapObject_BendVeryTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsVeryTallGrass(currTileBehavior) == TRUE) {
        ov5_021F3844(mapObj, 0);
    }
}

static void MapObject_RustleVeryTallGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsVeryTallGrass(currTileBehavior) == TRUE) {
        ov5_021F3844(mapObj, 1);
    }
}

// Immediate (Bend) and fully animated (Rustle) variants of the mud-with-grass
// effect.
static void MapObject_BendMudWithGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE) {
        ov5_021F3AEC(mapObj, 0);
    }
}

static void MapObject_RustleMudWithGrass(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsMudWithGrass(currTileBehavior) == TRUE) {
        ov5_021F3AEC(mapObj, 1);
    }
}

// Puddle and mud splash effects, spawned either at the tile just left
// (PrevPos) or the tile just reached (Pos) depending on the movement phase.
static void MapObject_CreatePuddleRippleAtPrevPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsPuddle(prevTileBehavior) == TRUE) {
        ov5_021F2AE4(mapObj, MapObject_GetXPrev(mapObj), MapObject_GetYPrev(mapObj), MapObject_GetZPrev(mapObj));
    }
}

static void MapObject_CreatePuddleRippleAtPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsPuddle(currTileBehavior) == TRUE) {
        ov5_021F2AE4(mapObj, MapObject_GetX(mapObj), MapObject_GetY(mapObj), MapObject_GetZ(mapObj));
    }
}

static void MapObject_CreateMudSplashAtPrevPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsMud(prevTileBehavior) == TRUE) {
        ov5_021F2C38(mapObj, MapObject_GetXPrev(mapObj), MapObject_GetYPrev(mapObj), MapObject_GetZPrev(mapObj));
    }
}

static void MapObject_CreateMudSplashAtPos(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsMud(currTileBehavior) == TRUE) {
        ov5_021F2C38(mapObj, MapObject_GetX(mapObj), MapObject_GetY(mapObj), MapObject_GetZ(mapObj));
    }
}

// Sets up the object's water reflection. A reflective surface is looked for on
// the current tile, then immediately south of it (where the object is drawn).
// `v2` selects the reflection style: 2 = fully reflective, 1 = shallow/other,
// 0 = puddle.
static void MapObject_InitReflection(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (renderDetails->hasReflection == 0) {
        return;
    }

    if (MapObject_CheckStatus24(mapObj) == FALSE) {
        u8 tileBehavior = GetNullTileBehaviorID();

        if (TileBehavior_HasReflectiveSurface(currTileBehavior) == TRUE) {
            tileBehavior = currTileBehavior;
        } else {
            u8 southTileBehavior = MapObject_GetTileBehaviorFromDir(mapObj, DIR_SOUTH);

            if (TileBehavior_HasReflectiveSurface(southTileBehavior) == TRUE) {
                tileBehavior = southTileBehavior;
            }
        }

        if (tileBehavior != GetNullTileBehaviorID()) {
            int v2;

            MapObject_SetStatus24(mapObj, 1);

            if (TileBehavior_IsReflective(tileBehavior) == TRUE) {
                v2 = 2;
            } else if (TileBehavior_IsPuddle(tileBehavior) == TRUE) {
                v2 = 0;
            } else {
                v2 = 1;
            }

            ov5_021F1800(mapObj, v2);
        }
    }
}

// Drops the reflection once the tile south of the object is no longer
// reflective.
static void MapObject_UpdateReflection(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (renderDetails->hasReflection == 0 || MapObject_CheckStatus24(mapObj) == FALSE) {
        return;
    }

    u8 southTileBehavior = MapObject_GetTileBehaviorFromDir(mapObj, DIR_SOUTH);

    if (TileBehavior_HasReflectiveSurface(southTileBehavior) == FALSE) {
        MapObject_SetStatus24(mapObj, 0);
    }
}

// Tracks whether the object is on an elevated bridge so that bridge-over-
// water/sand/snow tiles count as solid ground instead of the surface below.
static void MapObject_DetermineElevatedBridgeStatus(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    if (TileBehavior_IsBridgeStart(currTileBehavior) == TRUE) {
        MapObject_SetElevatedBridgeStatus(mapObj, TRUE);
    } else if (MapObject_IsStatusOnElevatedBridge(mapObj) == TRUE && TileBehavior_IsBridge(currTileBehavior) == FALSE) {
        MapObject_SetElevatedBridgeStatus(mapObj, FALSE);
    }
}

// Placeholder kept in the effect sequences; does nothing.
static void MapObject_EmptyFunction(MapObject *mapObj, u8 currTileBehavior, u8 prevTileBehavior, const ObjectEventGfxRenderDetailsEntry *renderDetails)
{
    return;
}

// Builds the collision flags for moving to (x, y, z) in direction `dir`: out
// of the object's movement range, blocked by terrain, or blocked by another
// map object. `pos` is the object's current world position, needed by the
// terrain height-change lookup.
u32 MapObject_CheckCollisionAtPos(const MapObject *mapObj, const VecFx32 *pos, int x, int y, int z, int dir)
{
    u32 collisionFlag = MAP_OBJ_COLLISION_NONE;

    if (MapObject_IsOutOfRange(mapObj, x, y, z) == TRUE) {
        collisionFlag |= MAP_OBJ_COLLISION_OUT_OF_RANGE;
    }

    s8 v1;
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);

    if (TerrainCollisionManager_WillMapObjectCollide(fieldSystem, pos, x, z, &v1) == TRUE) {
        collisionFlag |= MAP_OBJ_COLLISION_WILL_COLLIDE;

        if (v1 != 0) {
            collisionFlag |= MAP_OBJ_COLLISION_HEIGHT_CHANGE;
        }
    }

    if (MapObject_IsMovementBlockedInDir(mapObj, x, z, dir) == TRUE) {
        collisionFlag |= MAP_OBJ_COLLISION_WILL_COLLIDE;
    }

    if (MapObject_IsTileOccupiedByOtherObject(mapObj, x, y, z) == TRUE) {
        collisionFlag |= MAP_OBJ_COLLISION_2;
    }

    return collisionFlag;
}

u32 MapObject_CheckCollisionAtCoords(const MapObject *mapObj, int x, int y, int z, int dir)
{
    VecFx32 pos;

    MapObject_GetPosPtr(mapObj, &pos);
    return MapObject_CheckCollisionAtPos(mapObj, &pos, x, y, z, dir);
}

u32 MapObject_CheckCollisionInDir(const MapObject *mapObj, int dir)
{
    int x = MapObject_GetX(mapObj) + MapObject_GetDxFromDir(dir);
    int y = MapObject_GetY(mapObj);
    int z = MapObject_GetZ(mapObj) + MapObject_GetDzFromDir(dir);

    return MapObject_CheckCollisionAtCoords(mapObj, x, y, z, dir);
}

// TRUE if another active, non-hidden object is standing on (x, z) at a height
// within one tile of y. Both the other object's current and previous positions
// are checked so that two objects stepping through each other still collide.
int MapObject_IsTileOccupiedByOtherObject(const MapObject *mapObj, int x, int y, int z)
{
    int maxObjects, objX, objZ;
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);
    const MapObject *v4 = MapObjectMan_GetMapObjectConst(mapObjMan);

    maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);

    do {
        if (v4 != mapObj
            && MapObject_CheckStatus(v4, MAP_OBJ_STATUS_0)
            && !MapObject_CheckStatus(v4, MAP_OBJ_STATUS_18)) {
            objX = MapObject_GetX(v4);
            objZ = MapObject_GetZ(v4);

            if (objX == x && objZ == z) {
                int objY = MapObject_GetY(v4);
                int v6 = objY - y;

                if (v6 < 0) {
                    v6 = -v6;
                }

                if (v6 < (1 * 2)) {
                    return TRUE;
                }
            }

            objX = MapObject_GetXPrev(v4);
            objZ = MapObject_GetZPrev(v4);

            if (objX == x && objZ == z) {
                int objY = MapObject_GetY(v4);
                int v8 = objY - y;

                if (v8 < 0) {
                    v8 = -v8;
                }

                if (v8 < (1 * 2)) {
                    return TRUE;
                }
            }
        }

        MapObject_AdvancePointer(&v4);
        maxObjects--;
    } while (maxObjects);

    return FALSE;
}

int MapObject_IsOutOfRange(const MapObject *mapObj, int x, int y, int z)
{
    int initialPos, movementRange, minPos, maxPos;

    initialPos = MapObject_GetXInitial(mapObj);
    movementRange = MapObject_GetMovementRangeX(mapObj);

    if (movementRange != -1) {
        minPos = initialPos - movementRange;
        maxPos = initialPos + movementRange;

        if (minPos > x || maxPos < x) {
            return TRUE;
        }
    }

    initialPos = MapObject_GetZInitial(mapObj);
    movementRange = MapObject_GetMovementRangeZ(mapObj);

    if (movementRange != -1) {
        minPos = initialPos - movementRange;
        maxPos = initialPos + movementRange;

        if (minPos > z || maxPos < z) {
            return TRUE;
        }
    }

    return FALSE;
}

// Checks whether the tile behavior blocks stepping from (x, z) in `dir`: the
// current tile must not block movement in that direction, and the destination
// tile must not block movement arriving from it.
int MapObject_IsMovementBlockedInDir(const MapObject *mapObj, int x, int z, int dir)
{
    if (MapObject_CheckFlags2Bit2(mapObj) == FALSE) {
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
        u8 v1 = MapObject_GetCurrTileBehavior(mapObj);
        u8 v2 = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

        if (v2 == GetNullTileBehaviorID()) {
            return TRUE;
        }

        if (sTileBehaviorBlocksMovementInDir[dir](v1) == TRUE || sTileBehaviorBlocksMovementAgainstDir[dir](v2) == TRUE) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL (*const sTileBehaviorBlocksMovementInDir[4])(u8) = {
    TileBehavior_BlocksMovementNorthward,
    TileBehavior_BlocksMovementSouthward,
    TileBehavior_BlocksMovementWestward,
    TileBehavior_BlocksMovementEastward
};

static BOOL (*const sTileBehaviorBlocksMovementAgainstDir[4])(u8) = {
    TileBehavior_BlocksMovementSouthward,
    TileBehavior_BlocksMovementNorthward,
    TileBehavior_BlocksMovementEastward,
    TileBehavior_BlocksMovementWestward
};

int MapObject_IsOnWater(MapObject *mapObj, u32 tileBehavior)
{
    if (TileBehavior_IsBridgeOverWater(tileBehavior)) {
        if (!MapObject_IsStatusOnElevatedBridge(mapObj)) {
            return TRUE;
        }
    } else if (TileBehavior_IsSurfable(tileBehavior)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnSand(MapObject *mapObj, u32 tileBehavior)
{
    if (TileBehavior_IsBridgeOverSand(tileBehavior)) {
        if (!MapObject_IsStatusOnElevatedBridge(mapObj)) {
            return TRUE;
        }
    } else if (TileBehavior_IsSand(tileBehavior)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnSnow(MapObject *mapObj, u32 tileBehavior)
{
    if (TileBehavior_IsBridgeOverSnow(tileBehavior)) {
        if (!MapObject_IsStatusOnElevatedBridge(mapObj)) {
            return TRUE;
        }
    } else if (TileBehavior_IsSnow(tileBehavior)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnShallowSnow(MapObject *mapObj, u32 tileBehavior)
{
    if (TileBehavior_IsBridgeOverSnow(tileBehavior)) {
        if (!MapObject_IsStatusOnElevatedBridge(mapObj)) {
            return TRUE;
        }
    } else if (TileBehavior_IsShallowSnow(tileBehavior)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnElevatedBridge(MapObject *mapObj, u32 tileBehavior)
{
    if (MapObject_IsStatusOnElevatedBridge(mapObj) == TRUE && TileBehavior_IsBridge(tileBehavior) == TRUE) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnBikeBridgeNorthSouth(MapObject *mapObj, u32 tileBehavior)
{
    if (MapObject_IsStatusOnElevatedBridge(mapObj) == TRUE && TileBehavior_IsBikeBridgeNorthSouth(tileBehavior) == TRUE) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsOnBikeBridgeEastWest(MapObject *mapObj, u32 tileBehavior)
{
    if (MapObject_IsStatusOnElevatedBridge(mapObj) == TRUE && TileBehavior_IsBikeBridgeEastWest(tileBehavior) == TRUE) {
        return TRUE;
    }

    return FALSE;
}

static const int sMapObjectDxDir[] = {
    [DIR_NORTH] = 0,
    [DIR_SOUTH] = 0,
    [DIR_WEST] = -1,
    [DIR_EAST] = 1
};

static const int UNUSED_GPosY_Dir4AddTbl[] = {
    0,
    0,
    0,
    0
};

static const int sMapObjectDzDir[] = {
    [DIR_NORTH] = -1,
    [DIR_SOUTH] = 1,
    [DIR_WEST] = 0,
    [DIR_EAST] = 0
};

int MapObject_GetDxFromDir(int dir)
{
    return sMapObjectDxDir[dir];
}

int MapObject_GetDzFromDir(int dir)
{
    return sMapObjectDzDir[dir];
}

void MapObject_StepDir(MapObject *mapObj, int dir)
{
    MapObject_SetXPrev(mapObj, MapObject_GetX(mapObj));
    MapObject_SetYPrev(mapObj, MapObject_GetY(mapObj));
    MapObject_SetZPrev(mapObj, MapObject_GetZ(mapObj));

    MapObject_AddX(mapObj, MapObject_GetDxFromDir(dir));
    MapObject_AddY(mapObj, 0);
    MapObject_AddZ(mapObj, MapObject_GetDzFromDir(dir));
}

void MapObject_UpdateCoords(MapObject *mapObj)
{
    MapObject_SetXPrev(mapObj, MapObject_GetX(mapObj));
    MapObject_SetYPrev(mapObj, MapObject_GetY(mapObj));
    MapObject_SetZPrev(mapObj, MapObject_GetZ(mapObj));
}

u32 MapObject_GetTileBehaviorFromDir(MapObject *mapObj, int dir)
{
    int x = MapObject_GetX(mapObj) + MapObject_GetDxFromDir(dir);
    int z = MapObject_GetZ(mapObj) + MapObject_GetDzFromDir(dir);
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    u8 tileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

    return tileBehavior;
}

void MapObject_AddVecToPos(MapObject *mapObj, const VecFx32 *vec)
{
    VecFx32 pos;

    MapObject_GetPosPtr(mapObj, &pos);

    pos.x += vec->x;
    pos.y += vec->y;
    pos.z += vec->z;

    MapObject_SetPos(mapObj, &pos);
}

void MapObject_MovePosInDir(MapObject *mapObj, int dir, fx32 distance)
{
    VecFx32 pos;

    MapObject_GetPosPtr(mapObj, &pos);

    switch (dir) {
    case DIR_NORTH:
        pos.z -= distance;
        break;
    case DIR_SOUTH:
        pos.z += distance;
        break;
    case DIR_WEST:
        pos.x -= distance;
        break;
    case DIR_EAST:
        pos.x += distance;
        break;
    }

    MapObject_SetPos(mapObj, &pos);
}

// Recomputes the object's world height from the terrain and, on success,
// refreshes its cached integer y coordinate. If the terrain provides no height
// (or dynamic heights are disabled), STATUS_12 stays set so the recalculation
// is retried on a later frame.
int MapObject_RecalculateObjectHeight(MapObject *mapObj)
{
    VecFx32 pos, updatedPos;

    MapObject_GetPosPtr(mapObj, &pos);
    updatedPos = pos;

    if (MapObject_IsHeightCalculationDisabled(mapObj) == TRUE) {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_12);
        return FALSE;
    }

    int dynamicHeightCalculationEnabled = MapObject_IsDynamicHeightCalculationEnabled(mapObj);
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int heightUpdated = MapObject_RecalculatePositionHeightEx(fieldSystem, &updatedPos, dynamicHeightCalculationEnabled);

    if (heightUpdated == TRUE) {
        pos.y = updatedPos.y;
        MapObject_SetPos(mapObj, &pos);
        MapObject_SetYPrev(mapObj, MapObject_GetY(mapObj));
        MapObject_SetY(mapObj, (pos.y >> 3) / FX32_ONE);
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_12);
    } else {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_12);
    }

    return heightUpdated;
}

// Re-samples the tile behaviors under the object's previous and current map
// coordinates. Objects that ignore terrain (flags2 bit 2, e.g. on distortion
// world platforms) are left with null behaviors. Returns FALSE and flags
// STATUS_11 when the current tile has no behavior, i.e. the object is not yet
// standing on a valid tile.
int MapObject_SetTileBehaviors(MapObject *mapObj)
{
    u8 prevTileBehavior = GetNullTileBehaviorID();
    u8 currTileBehavior = prevTileBehavior;

    if (MapObject_CheckFlags2Bit2(mapObj) == FALSE) {
        int x = MapObject_GetXPrev(mapObj);
        int z = MapObject_GetZPrev(mapObj);
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);

        prevTileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);
        x = MapObject_GetX(mapObj);
        z = MapObject_GetZ(mapObj);
        currTileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);
    }

    MapObject_SetPrevTileBehavior(mapObj, prevTileBehavior);
    MapObject_SetCurrTileBehavior(mapObj, currTileBehavior);

    if (TileBehavior_IsNull(currTileBehavior) == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_11);
        return FALSE;
    }

    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_11);
    return TRUE;
}

void VecFx32_StepDirection(int dir, VecFx32 *vec, fx32 val)
{
    switch (dir) {
    case DIR_NORTH:
        vec->z -= val;
        break;
    case DIR_SOUTH:
        vec->z += val;
        break;
    case DIR_WEST:
        vec->x -= val;
        break;
    case DIR_EAST:
        vec->x += val;
        break;
    }
}

void VecFx32_SetPosFromMapCoords(int x, int z, VecFx32 *outVec)
{
    outVec->x = MAP_OBJECT_COORD_CENTER_TO_FX32(x);
    outVec->z = MAP_OBJECT_COORD_CENTER_TO_FX32(z);
}

// Disguised objects (snowman, sand pile, rock, grass tuft) immediately run a
// movement update so the disguise is applied as soon as it is spawned.
void MapObject_UpdateDisguiseMovement(MapObject *mapObj)
{
    int movementType = MapObject_GetMovementType(mapObj);

    if (movementType == MOVEMENT_TYPE_DISGUISE_SNOW
        || movementType == MOVEMENT_TYPE_DISGUISE_SAND
        || movementType == MOVEMENT_TYPE_DISGUISE_ROCK
        || movementType == MOVEMENT_TYPE_DISGUISE_GRASS) {
        MapObject_CallMovementUpdate(mapObj);
    }
}

static const int sOppositeDirections[] = {
    [DIR_NORTH] = DIR_SOUTH,
    [DIR_SOUTH] = DIR_NORTH,
    [DIR_WEST] = DIR_EAST,
    [DIR_EAST] = DIR_WEST
};

int Direction_GetOpposite(int dir)
{
    return sOppositeDirections[dir];
}

int GetDirectionBetweenPoints(int xSrc, int zSrc, int xDst, int zDst)
{
    if (xSrc > xDst) {
        return DIR_WEST;
    }

    if (xSrc < xDst) {
        return DIR_EAST;
    }

    if (zSrc > zDst) {
        return DIR_NORTH;
    }

    return DIR_SOUTH;
}

int MapObject_RecalculatePositionHeight(FieldSystem *fieldSystem, VecFx32 *pos)
{
    u8 newObjectHeightSource;
    fx32 objectHeight = TerrainCollisionManager_GetHeight(fieldSystem, pos->y, pos->x, pos->z, &newObjectHeightSource);

    if (newObjectHeightSource == CALCULATED_HEIGHT_SOURCE_NONE) {
        return FALSE;
    }

    pos->y = objectHeight;
    return TRUE;
}

int MapObject_RecalculatePositionHeightEx(FieldSystem *fieldSystem, VecFx32 *pos, int dynamicHeightCalculationEnabled)
{
    u8 newObjectHeightSource;
    fx32 objectHeight = TerrainCollisionManager_GetHeight(fieldSystem, pos->y, pos->x, pos->z, &newObjectHeightSource);

    if (newObjectHeightSource == CALCULATED_HEIGHT_SOURCE_NONE) {
        return FALSE;
    }

    if (newObjectHeightSource == CALCULATED_HEIGHT_SOURCE_DYNAMIC && dynamicHeightCalculationEnabled == FALSE) {
        return FALSE;
    }

    pos->y = objectHeight;
    return TRUE;
}
