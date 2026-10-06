#include "map_transition.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"

#include "field/field_system.h"
#include "field/field_system_sub2_t.h"
#include "functypes/funcptr_020EC560.h"
#include "functypes/funcptr_020EC57C.h"
#include "overlay005/fieldmap.h"
#include "overlay005/hblank_system.h"
#include "overlay005/map_name_popup.h"
#include "overlay005/ov5_021D431C.h"
#include "overlay005/struct_ov5_021D432C_decl.h"
#include "overlay005/struct_ov5_021D4E00_decl.h"

#include "camera.h"
#include "field_bgm.h"
#include "field_map_change.h"
#include "field_task.h"
#include "field_transition.h"
#include "heap.h"
#include "inlines.h"
#include "location.h"
#include "map_header.h"
#include "map_object.h"
#include "map_tile_behavior.h"
#include "player_avatar.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "terrain_collision_manager.h"
#include "map_object_animation.h"

// Map transitions. When the player steps onto a warp, door, or map connection,
// field_control.c calls MapTransition_Start (with an explicit transition type)
// or MapTransition_StartAuto (which derives the type from the source and
// destination map kinds). This module runs a field task that fades the screen
// out, swaps the map, fades back in, and then adjusts the player and camera.
// It also exposes MapTransition_StartScreenFade, a small task wrapper around
// StartScreenFade used by the binoculars and by the door/warp animations in
// overlay005.

// Environment for the map-transition field task started by MapTransition_Start
// and MapTransition_StartAuto. The task fades the screen out, swaps the map,
// fades back in, and then runs an optional finish callback.
typedef struct MapTransition {
    int state; // main task state
    int subState; // state of the current fade-out/fade-in sub-task
    Location location; // destination map and warp
    void *animation; // UnkStruct_ov5_021D432C door/warp animation context
    int transitionType; // index into the sFadeOutFuncs/sFadeInFuncs tables
} MapTransition;

// Environment for the screen-fade field task started by
// MapTransition_StartScreenFade. The fields mirror the arguments of
// StartScreenFade.
typedef struct MapTransitionScreenFade {
    int state; // task state
    int mode; // FadeMode
    int typeMain; // main-screen FadeType
    int typeSub; // sub-screen FadeType
    u16 color; // fade color
    int steps; // number of fade steps
    int framesPerStep; // frames between steps
    enum HeapID heapID;
} MapTransitionScreenFade;

static BOOL MapTransition_ScreenFadeTask(FieldTask *taskMan);
static BOOL MapTransition_Task(FieldTask *taskMan);
static BOOL MapTransition_FadeOut(FieldTask *taskMan);
static BOOL MapTransition_FadeOutDoor(FieldTask *taskMan);
static BOOL MapTransition_FadeOutWarp(FieldTask *taskMan);
static BOOL MapTransition_FadeOutWarpStairs(FieldTask *taskMan);
static BOOL MapTransition_FadeOutEnterCave(FieldTask *taskMan);
static BOOL MapTransition_FadeOutExitCave(FieldTask *taskMan);
static BOOL MapTransition_FadeInDoor(FieldTask *taskMan);
static BOOL MapTransition_FadeIn(FieldTask *taskMan);
static BOOL MapTransition_FadeInWarp(FieldTask *taskMan);
static BOOL MapTransition_FadeInWarpStairs(FieldTask *taskMan);
static BOOL MapTransition_FadeInExitCave(FieldTask *taskMan);
static void MapTransition_AdjustPlayerX(FieldSystem *fieldSystem);
static void MapTransition_AdjustPlayerXForWarpStairs(FieldSystem *fieldSystem);

// The transition type selects the fade-out animation, the fade-in animation,
// and an optional finish callback. It is passed explicitly to
// MapTransition_Start or derived from the source/destination map types by
// MapTransition_StartAuto:
//   0 - leaving a building for the outdoors
//   1 - door
//   2 - warp (escalator / warp panel)
//   3 - warp stairs
//   4 - entering a cave from the outdoors
//   5 - leaving a cave for the outdoors
//   6 - generic / same map type
static const UnkFuncPtr_020EC560 sFadeOutFuncs[7] = {
    MapTransition_FadeOut,
    MapTransition_FadeOutDoor,
    MapTransition_FadeOutWarp,
    MapTransition_FadeOutWarpStairs,
    MapTransition_FadeOutEnterCave,
    MapTransition_FadeOutExitCave,
    MapTransition_FadeOut
};

static const UnkFuncPtr_020EC560 sFadeInFuncs[7] = {
    MapTransition_FadeInDoor,
    MapTransition_FadeIn,
    MapTransition_FadeInWarp,
    MapTransition_FadeInWarpStairs,
    MapTransition_FadeIn,
    MapTransition_FadeInExitCave,
    MapTransition_FadeIn
};

static const UnkFuncPtr_020EC57C sTransitionFinishFuncs[7] = {
    NULL,
    NULL,
    MapTransition_AdjustPlayerX,
    MapTransition_AdjustPlayerXForWarpStairs,
    NULL,
    NULL,
    NULL
};

// Starts a field task that fades the screen using the given StartScreenFade
// parameters.
void MapTransition_StartScreenFade(FieldTask *taskMan, int mode, int typeMain, int typeSub, u16 color, int steps, int framesPerStep, enum HeapID heapID)
{
    MapTransitionScreenFade *v0 = Heap_Alloc(heapID, sizeof(MapTransitionScreenFade));

    v0->mode = mode;
    v0->typeMain = typeMain;
    v0->typeSub = typeSub;
    v0->color = color;
    v0->steps = steps;
    v0->framesPerStep = framesPerStep;
    v0->heapID = heapID;
    v0->state = 0;

    FieldTask_InitCall(taskMan, MapTransition_ScreenFadeTask, v0);
}

// Stops HBlank, runs the fade, then restarts HBlank once it completes.
static BOOL MapTransition_ScreenFadeTask(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransitionScreenFade *v1 = FieldTask_GetEnv(taskMan);

    switch (v1->state) {
    case 0:
        // HBlank must be stopped while the fade runs, then restarted.
        HBlankSystem_Stop(fieldSystem->unk_04->hBlankSystem);
        StartScreenFade(v1->mode, v1->typeMain, v1->typeSub, v1->color, v1->steps, v1->framesPerStep, v1->heapID);
        v1->state++;
        break;
    case 1:
        if (IsScreenFadeDone()) {
            HBlankSystem_Start(fieldSystem->unk_04->hBlankSystem);
            Heap_Free(v1);
            return 1;
        }
    }

    return 0;
}

// Starts a map transition to the given location using an explicit transition
// type.
void MapTransition_Start(FieldSystem *fieldSystem, const int mapHeaderID, const int warpId, const int x, const int z, const int faceDirection, const int transitionType)
{
    MapTransition *v0 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(MapTransition));

    v0->state = 0;
    v0->subState = 0;

    Location_Set(&v0->location, mapHeaderID, warpId, x, z, faceDirection);

    v0->transitionType = transitionType;

    FieldSystem_CreateTask(fieldSystem, MapTransition_Task, v0);
}

// Starts a map transition, choosing the transition type from the source map's
// kind (cave/outdoors/building) and the destination's.
void MapTransition_StartAuto(FieldSystem *fieldSystem, const int mapHeaderID, const int warpId, const int x, const int z, const int faceDirection)
{
    MapTransition *v2 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(MapTransition));

    v2->state = 0;
    v2->subState = 0;

    Location_Set(&v2->location, mapHeaderID, warpId, x, z, faceDirection);

    enum MapHeaderID srcMapHeaderID = fieldSystem->location->mapHeaderID;
    int transitionType = 0;

    if (MapHeader_IsCave(srcMapHeaderID)) {
        if (MapHeader_IsCave(mapHeaderID)) {
            transitionType = 6;
        } else if (MapHeader_IsOutdoors(mapHeaderID)) {
            transitionType = 5;
        } else if (MapHeader_IsBuilding(mapHeaderID)) {
            transitionType = 6;
        } else {
            GF_ASSERT(FALSE);
        }
    } else if (MapHeader_IsOutdoors(srcMapHeaderID)) {
        if (MapHeader_IsCave(mapHeaderID)) {
            transitionType = 4;
        } else if (MapHeader_IsBuilding(mapHeaderID)) {
            transitionType = 6;
        } else {
            GF_ASSERT(FALSE);
        }
    } else if (MapHeader_IsBuilding(srcMapHeaderID)) {
        if (MapHeader_IsOutdoors(mapHeaderID)) {
            transitionType = 0;
        } else if (MapHeader_IsBuilding(mapHeaderID)) {
            transitionType = 6;
        } else if (MapHeader_IsCave(mapHeaderID)) {
            transitionType = 0;
        } else {
            GF_ASSERT(FALSE);
        }
    } else {
        GF_ASSERT(FALSE);
    }

    v2->transitionType = transitionType;

    FieldSystem_CreateTask(fieldSystem, MapTransition_Task, v2);
}

// Main transition task: fade out, finish the old map, load the new map, run the
// finish callback, then fade in.
static BOOL MapTransition_Task(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    Location *v2 = &v1->location;

    switch (v1->state) {
    case 0:
        v1->subState = 0;
        FieldBGM_TryFadeIn(fieldSystem, v2->mapHeaderID);
        FieldTask_InitCall(taskMan, sFadeOutFuncs[v1->transitionType], v1);
        (v1->state)++;
        break;
    case 1:
        FieldTransition_FinishMap(taskMan);
        (v1->state)++;
        break;
    case 2:
        FieldTask_ChangeMapByLocation(taskMan, &v1->location);
        (v1->state)++;
        break;
    case 3:
        FieldTransition_StartMap(taskMan);
        (v1->state)++;
        break;
    case 4:
        // Optional per-transition fix-up (e.g. nudging the player off a warp).
        if (sTransitionFinishFuncs[v1->transitionType] != NULL) {
            sTransitionFinishFuncs[v1->transitionType](fieldSystem);
        }

        (v1->state)++;
        break;
    case 5:
        // Wait for the old map's BGM to finish fading before starting the new
        // map's music and location-name popup.
        if (Sound_IsFadeActive()) {
            break;
        }

        FieldBGM_PlayForMapHeader(fieldSystem, v2->mapHeaderID);
        FieldSystem_RequestLocationName(fieldSystem);

        v1->subState = 0;
        FieldTask_InitCall(taskMan, sFadeInFuncs[v1->transitionType], v1);
        (v1->state)++;
        break;
    case 6:
        Heap_Free(v1);
        return 1;
    }

    return 0;
}

// Generic fade-out: plays the stair sound and fades the screen.
static BOOL MapTransition_FadeOut(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);

    switch (v1->subState) {
    case 0:
        Sound_PlayEffect(SEQ_SE_DP_KAIDAN2_sseq);

        FieldTransition_FadeOut(taskMan);
        (v1->subState)++;
        break;
    case 1:
        return 1;
    }

    return 0;
}

// Fade-out for a door: plays the door-open animation, then fades.
static BOOL MapTransition_FadeOutDoor(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    UnkStruct_ov5_021D432C *v2;

    switch (v1->subState) {
    case 0:
        v1->animation = ov5_021D431C();
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;
        ov5_021D4334(PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar), v2);
        (v1->subState)++;
        break;
    case 1:
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;

        if (ov5_021D433C(fieldSystem, v2)) {
            ov5_021D432C(v1->animation);
            (v1->subState)++;
        }
        break;
    case 2:
        FieldTransition_FadeOut(taskMan);
        (v1->subState)++;
        break;
    case 3:
        return 1;
    }

    return 0;
}

// Fade-out for a warp/escalator: plays the warp animation.
static BOOL MapTransition_FadeOutWarp(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    UnkStruct_ov5_021D432C *v2;

    switch (v1->subState) {
    case 0:
        v1->animation = ov5_021D431C();
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;
        ov5_021D4334(PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar), v2);
        (v1->subState)++;
        break;
    case 1:
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;

        if (ov5_021D4A24(fieldSystem, v2, PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar))) {
            ov5_021D432C(v1->animation);
            (v1->subState)++;
        }
        break;
    case 2:
        return 1;
    }

    return 0;
}

// Fade-out for warp stairs: walks the player one tile, then fades.
static BOOL MapTransition_FadeOutWarpStairs(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    MapObject *v2;

    switch (v1->subState) {
    case 0: {
        int v3 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);

        v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        if (v3 == 2) {
            LocalMapObj_SetAnimationCode(v2, MOVEMENT_ACTION_WALK_SLOW_WEST);
        } else if (v3 == 3) {
            LocalMapObj_SetAnimationCode(v2, MOVEMENT_ACTION_WALK_SLOW_EAST);
        } else {
            GF_ASSERT(FALSE);
        }
    }
        (v1->subState)++;
        break;
    case 1:
        v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        if (LocalMapObj_CheckAnimationFinished(v2) == 1) {
            LocalMapObj_ClearAnimation(v2);
            (v1->subState)++;
        }
        break;
    case 2:
        Sound_PlayEffect(SEQ_SE_DP_KAIDAN2_sseq);
        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
        (v1->subState)++;
        break;
    case 3:
        if (IsScreenFadeDone()) {
            return 1;
        }
        break;
    }

    return 0;
}

// Fade-out when entering a cave from the outdoors.
static BOOL MapTransition_FadeOutEnterCave(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    MapObject *v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

    switch (v1->subState) {
    case 0: {
        UnkStruct_ov5_021D4E00 *v3;

        v3 = ov5_021D4E00();
        FieldTask_InitCall(taskMan, ov5_021D4FA0, v3);
        v1->subState++;
    } break;
    case 1:
        return 1;
    }

    return 0;
}

// Fade-out when leaving a cave for the outdoors.
static BOOL MapTransition_FadeOutExitCave(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    MapObject *v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

    switch (v1->subState) {
    case 0: {
        UnkStruct_ov5_021D4E00 *v3;

        v3 = ov5_021D4E00();
        FieldTask_InitCall(taskMan, ov5_021D4F14, v3);
        v1->subState++;
    } break;
    case 1:
        return 1;
    }

    return 0;
}

// Fade-in for a door: hides the player, plays the door-close animation, then
// reveals them.
static BOOL MapTransition_FadeInDoor(FieldTask *taskMan)
{
    MapObject *mapObj;
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v2 = FieldTask_GetEnv(taskMan);
    UnkStruct_ov5_021D432C *v3;

    switch (v2->subState) {
    case 0: {
        u8 v4;
        MapObject *v5 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        v4 = TerrainCollisionManager_GetTileBehavior(fieldSystem, PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar));

        if (TileBehavior_IsDoor(v4)) {
            MapObject_SetHidden(v5, 1);
            (v2->subState) = 1;
        } else {
            UnkStruct_ov5_021D4E00 *v6;

            v6 = ov5_021D4E00();
            FieldTask_InitCall(taskMan, ov5_021D5020, v6);
            (v2->subState) = 3;
        }
    } break;
    case 1:
        v2->animation = (UnkStruct_ov5_021D432C *)ov5_021D431C();
        v3 = (UnkStruct_ov5_021D432C *)v2->animation;
        ov5_021D4334(PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar), v3);
        (v2->subState)++;
        break;
    case 2:
        v3 = (UnkStruct_ov5_021D432C *)v2->animation;

        if (ov5_021D453C(fieldSystem, v3)) {
            ov5_021D432C(v3);
            {
                MapObject *v7 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

                MapObject_SetHidden(v7, 0);
            }
            return 1;
        }
        break;
    case 3:
        return 1;
    }

    return 0;
}

// Generic fade-in; if the player is standing on a door tile, delegates to the
// door fade-in.
static BOOL MapTransition_FadeIn(FieldTask *taskMan)
{
    MapObject *mapObj;
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v2 = FieldTask_GetEnv(taskMan);
    UnkStruct_ov5_021D432C *v3;

    switch (v2->subState) {
    case 0: {
        u8 v4;
        MapObject *v5 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        v4 = TerrainCollisionManager_GetTileBehavior(fieldSystem, PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar));

        if (TileBehavior_IsDoor(v4)) {
            // The door fade-in owns the rest of the transition.
            MapObject_SetHidden(v5, 1);
            v2->subState = 1;
            FieldTask_InitJump(taskMan, MapTransition_FadeInDoor, v2);
        } else {
            UnkStruct_ov5_021D4E00 *v6;

            v6 = ov5_021D4E00();
            FieldTask_InitCall(taskMan, ov5_021D5150, v6);
            (v2->subState)++;
        }
    } break;
    case 1:
        return 1;
    }

    return 0;
}

// Fade-in for a warp/escalator.
static BOOL MapTransition_FadeInWarp(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    UnkStruct_ov5_021D432C *v2;

    switch (v1->subState) {
    case 0:
        v1->animation = ov5_021D431C();
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;
        ov5_021D4334(PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar), v2);
        (v1->subState)++;
        break;
    case 1:
        v2 = (UnkStruct_ov5_021D432C *)v1->animation;

        if (ov5_021D4858(fieldSystem, v2, PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar))) {
            ov5_021D432C(v1->animation);
            (v1->subState)++;
        }
        break;
    case 2:
        return 1;
    }

    return 0;
}

// Fade-in for warp stairs: fades in while walking the player one tile.
static BOOL MapTransition_FadeInWarpStairs(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    MapObject *v2;

    switch (v1->subState) {
    case 0:

        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
        v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        if (1) {
            int v3;

            v3 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);

            if (v3 == 2) {
                LocalMapObj_SetAnimationCode(v2, MOVEMENT_ACTION_WALK_SLOW_WEST);
            } else if (v3 == 3) {
                LocalMapObj_SetAnimationCode(v2, MOVEMENT_ACTION_WALK_SLOW_EAST);
            } else {
                GF_ASSERT(FALSE);
            }
        } else {
            GF_ASSERT(FALSE);
        }

        (v1->subState)++;
        break;
    case 1:
        v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

        if (LocalMapObj_CheckAnimationFinished(v2) == 1) {
            LocalMapObj_ClearAnimation(v2);
            (v1->subState)++;
        }
        break;
    case 2:
        if (IsScreenFadeDone()) {
            (v1->subState)++;
        }
        break;
    case 3:
        return 1;
    }

    return 0;
}

// Fade-in when leaving a cave for the outdoors.
static BOOL MapTransition_FadeInExitCave(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    MapTransition *v1 = FieldTask_GetEnv(taskMan);
    MapObject *v2 = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);

    switch (v1->subState) {
    case 0: {
        UnkStruct_ov5_021D4E00 *v3;

        v3 = ov5_021D4E00();
        FieldTask_InitCall(taskMan, ov5_021D4E10, v3);
        v1->subState++;
    } break;
    case 1:
        return 1;
    }

    return 0;
}

// Finish callback for a warp: nudges the player one tile east/west and
// re-centres the camera.
static void MapTransition_AdjustPlayerX(FieldSystem *fieldSystem)
{
    int v0;
    VecFx32 v1;

    v0 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    PlayerAvatar_GetPosPtr(fieldSystem->playerAvatar, &v1);

    // Step one tile sideways: west when facing west, otherwise east.
    if (v0 == 3) {
        v1.x -= (FX32_ONE * 16);
    } else {
        v1.x += (FX32_ONE * 16);
    }

    v1.y = TerrainCollisionManager_GetHeight(fieldSystem, v1.y, v1.x, v1.z, NULL);

    PlayerAvatar_SetPosDirFromVec(fieldSystem->playerAvatar, &v1, v0);
    Camera_SetTargetAndUpdatePosition(PlayerAvatar_GetPos(fieldSystem->playerAvatar), fieldSystem->camera);
    Camera_TrackTarget(PlayerAvatar_GetPos(fieldSystem->playerAvatar), fieldSystem->camera);
}

// Finish callback for warp stairs: nudges the player one tile in the stair
// direction and re-centres the camera.
static void MapTransition_AdjustPlayerXForWarpStairs(FieldSystem *fieldSystem)
{
    int v0, v1, v2;
    VecFx32 v3;
    u8 v4;

    v2 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    PlayerAvatar_GetPosPtr(fieldSystem->playerAvatar, &v3);

    v0 = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    v1 = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
    v4 = TerrainCollisionManager_GetTileBehavior(fieldSystem, v0, v1);

    // Step off the stair in the direction it faces (east = 2, west = 3).
    if (TileBehavior_IsWarpStairsEast(v4)) {
        v3.x += (FX32_ONE * 16);
        v2 = 2;
    } else if (TileBehavior_IsWarpStairsWest(v4)) {
        v3.x -= (FX32_ONE * 16);
        v2 = 3;
    } else {
        (void)0;
    }

    v3.y = TerrainCollisionManager_GetHeight(fieldSystem, v3.y, v3.x, v3.z, NULL);

    PlayerAvatar_SetPosDirFromVec(fieldSystem->playerAvatar, &v3, v2);
    Camera_SetTargetAndUpdatePosition(PlayerAvatar_GetPos(fieldSystem->playerAvatar), fieldSystem->camera);
    Camera_TrackTarget(PlayerAvatar_GetPos(fieldSystem->playerAvatar), fieldSystem->camera);
}
