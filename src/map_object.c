#include "map_object.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_types.h"
#include "generated/object_events_gfx.h"

#include "struct_decls/struct_02061830_sub1_decl.h"
#include "struct_defs/struct_020EDF0C.h"

#include "field/field_system.h"
#include "functypes/funcptr_020EDF0C.h"
#include "functypes/funcptr_020EDF0C_1.h"
#include "functypes/funcptr_020EDF0C_2.h"
#include "overlay005/object_event_gfx_data.h"
#include "overlay005/ov5_021ECC20.h"
#include "overlay005/ov5_021ECE40.h"
#include "overlay005/struct_ov5_021ED0A4.h"

#include "berry_patch_graphics.h"
#include "heap.h"
#include "map_header_data.h"
#include "map_object_move.h"
#include "narc.h"
#include "script_manager.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "unk_020655F4.h"
#include "unk_020EDBAC.h"

// Map objects are the dynamic entities placed on the overworld: the player,
// NPCs, and scripted props. Each object owns a SysTask that advances its
// movement and draw state every frame. A MapObjectManager owns the fixed-size
// pool of objects plus the shared draw resources.

// Owns the fixed-size pool of map objects and the resources shared by all of
// them. The pool is a single contiguous array; objects are addressed by index
// and "deleted" by clearing their status flags.
typedef struct MapObjectManager {
    u32 status;
    int maxObjects;
    int objectCount;
    int taskBasePriority;
    int unk_10;
    NARC *narc;
    UnkStruct_ov5_021ED0A4 drawManager;
    UnkStruct_02061830_sub1 *unk_120;
    MapObject *mapObj;
    FieldSystem *fieldSystem;
} MapObjectManager;

// A single overworld entity. Coordinates are stored both as integer map tiles
// (x/y/z) and as a fixed-point world position (pos); the two are kept in sync
// by MapObject_UpdateCoords.
typedef struct MapObject {
    u32 status;
    u32 flags2;
    u32 localID;
    enum MapHeaderID mapHeaderID;
    u32 graphicsID;
    u32 movementType;
    u32 trainerType;
    u32 flag;
    u32 script;
    int initialDir;
    int facingDir;
    int movingDir;
    int prevFacingDir;
    int prevMovingDir;
    int data[3];
    int movementRangeX;
    int movementRangeZ;
    int xInitial;
    int yInitial;
    int zInitial;
    int xPrev;
    int yPrev;
    int zPrev;
    int x;
    int y;
    int z;
    VecFx32 pos;
    VecFx32 spriteJumpOffset;
    VecFx32 spritePosOffset;
    VecFx32 spriteTerrainOffset;
    u32 unk_A0; // renderer variant index; see MAP_OBJ_UNK_A0_*
    enum MovementAction movementAction;
    int movementStep;
    u16 currTileBehavior;
    u16 prevTileBehavior;
    SysTask *task;
    const MapObjectManager *mapObjMan;
    UnkFuncPtr_020EDF0C movementInit; // movement callbacks selected from the
    UnkFuncPtr_020EDF0C_1 movementUpdate; // movement-type table (Unk_020EE3A8)
    UnkFuncPtr_020EDF0C_2 movementFree;
    MapObjectRendererInitFunc rendererInit; // renderer callbacks selected from
    MapObjectRendererDrawFunc rendererDraw; // the graphics-ID table
    MapObjectRendererFreeFunc rendererFree;
    MapObjectRendererUnloadFunc rendererUnload;
    MapObjectRendererLoadFunc rendererLoad;
    u8 unk_D8[16]; // opaque per-object scratch buffers owned by subsystems
    u8 unk_E8[16]; // (berry patches, field moves, billboard rendering, ...)
    u8 movementData[16];
    u8 unk_108[32];
} MapObject;

// Work item used while spawning map objects from a map header's object events.
typedef struct MapObjectMan_AddObjectsWork {
    int mapHeaderID;
    int numObjectEvents;
    int index;
    const MapObjectManager *mapObjMan;
    ObjectEvent *objectEvent;
} MapObjectMan_AddObjectsWork;

static MapObjectManager *MapObjectMan_Alloc(int param0);
static void MapObject_Save(FieldSystem *fieldSystem, MapObject *mapObj, MapObjectSave *mapObjSave);
static void MapObject_LoadSave(MapObject *mapObj, MapObjectSave *mapObjSave);
static void MapObject_InitAfterLoad(const MapObjectManager *mapObjMan, MapObject *mapObj);
static void MapObject_ResetStatusForLoad(MapObject *mapObj);
static void MapObject_SyncPosFromCoords(MapObject *mapObj);
static void MapObjectMan_AddMapObjectsFromEventsWorker(MapObjectMan_AddObjectsWork *param0);
static MapObject *MapObjectMan_GetFreeObject(const MapObjectManager *mapObjMan);
static MapObject *MapObjectMan_FindObjectByLocalIDAndMap(const MapObjectManager *mapObjMan, int param1, int param2);
static void MapObjectMan_AddMoveTask(const MapObjectManager *mapObjMan, MapObject *mapObj);
static void MapObject_InitFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent, FieldSystem *fieldSystem);
static void MapObject_SetInitialPosFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent);
static void MapObject_InitStatusAndManager(MapObject *mapObj, const MapObjectManager *mapObjMan);
static void MapObject_LoadMovementCallbacks(MapObject *mapObj);
static void MapObject_LoadRendererCallbacks(MapObject *mapObj);
static void MapObject_Clear(MapObject *mapObj);
static int MapObject_CheckObjectEventMatch(const MapObject *mapObj, int param1, int objEventCount, const ObjectEvent *objectEvent);
static MapObject *MapObjectMan_FindObjectByLocalIDAndFlag(const MapObjectManager *mapObjMan, int localID, int flag);
static void MapObject_StartMovementAndResetShadow(MapObject *mapObj);
static void MapObject_ResetShadowFlags(MapObject *mapObj);
static void MapObject_NoOp(MapObject *mapObj);
static int MapObject_GetFieldSystemGraphicsID(FieldSystem *fieldSystem, int param1);
static void MapObject_RecalculateHeightIfNeeded(MapObject *mapObj);
static void MapObject_InitMovement(MapObject *mapObj);
static void MapObject_RefreshDraw(MapObject *mapObj);
static void MapObject_ReinitFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent, enum MapHeaderID mapHeaderID);
static void MapObject_ReinitFromObjectEventNoScript(MapObject *mapObj, enum MapHeaderID mapHeaderID, const ObjectEvent *objectEvent);
static void MapObjectTask_Move(SysTask *task, void *param1);
static void MapObjectTask_Draw(MapObject *mapObj);
static MapObjectManager *MapObjectMan_Deconst(const MapObjectManager *mapObjMan);
static void MapObjectMan_IncObjectCount(MapObjectManager *mapObjMan);
static void MapObjectMan_DecObjectCount(MapObjectManager *mapObjMan);
static MapObject *MapObjectMan_GetMapObjectStatic(const MapObjectManager *mapObjMan);
static MapObjectManager *MapObject_GetManagerMutable(const MapObject *mapObj);
static const ObjectEvent *ObjectEvent_FindByLocalID(int param0, int param1, const ObjectEvent *objectEvent);
static int ObjectEvent_HasNoScript(const ObjectEvent *objectEvent);
static int ObjectEvent_GetHiddenFlagNoScript(const ObjectEvent *objectEvent);

static const UnkStruct_020EDF0C *MovementType_GetCallbackStruct(u32 param0);
static UnkFuncPtr_020EDF0C MovementType_GetInitCallback(const UnkStruct_020EDF0C *param0);
static UnkFuncPtr_020EDF0C_1 MovementType_GetUpdateCallback(const UnkStruct_020EDF0C *param0);
static UnkFuncPtr_020EDF0C_2 MovementType_GetFreeCallback(const UnkStruct_020EDF0C *param0);
static MapObjectRendererUnloadFunc Renderer_GetUnloadCallback(const MapObjectRendererCallbacks *param0);
static MapObjectRendererLoadFunc Renderer_GetLoadCallback(const MapObjectRendererCallbacks *param0);

static MapObjectRendererInitFunc Renderer_GetInitCallback(const MapObjectRendererCallbacks *param0);
static MapObjectRendererDrawFunc Renderer_GetDrawCallback(const MapObjectRendererCallbacks *param0);
static MapObjectRendererFreeFunc Renderer_GetFreeCallback(const MapObjectRendererCallbacks *param0);
static const MapObjectRendererCallbacks *Renderer_GetByGraphicsID(u32 param0);

MapObjectManager *MapObjectMan_New(FieldSystem *fieldSystem, int maxObjs, int taskBasePriority)
{
    MapObjectManager *mapObjMan = MapObjectMan_Alloc(maxObjs);
    MapObjectMan_SetFieldSystem(mapObjMan, fieldSystem);
    MapObjectMan_SetMaxObjects(mapObjMan, maxObjs);
    MapObjectMan_SetTaskBasePriority(mapObjMan, taskBasePriority);

    return mapObjMan;
}

void MapObjectMan_Delete(MapObjectManager *mapObjMan)
{
    Heap_FreeExplicit(HEAP_ID_FIELD2, MapObjectMan_GetMapObject(mapObjMan));
    Heap_FreeExplicit(HEAP_ID_FIELD2, mapObjMan);
}

// Reconciles the live objects with the object events of the map being entered.
// Objects that no longer correspond to an event (and are not persistent) are
// deleted; objects that do are kept. `oldMapHeaderID` is unused.
void MapObjectMan_UpdateObjectsForMapChange(MapObjectManager *mapObjMan, enum MapHeaderID oldMapHeaderID, enum MapHeaderID newMapHeaderID, int objEventCount, const ObjectEvent *objectEvent)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    while (maxObjects) {
        if (MapObject_IsActive(mapObj) == TRUE) {
            int v0 = MapObject_CheckObjectEventMatch(mapObj, newMapHeaderID, objEventCount, objectEvent);

            switch (v0) {
            case 0:
                if (MapObject_GetMapHeaderID(mapObj) != newMapHeaderID && !MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_PERSISTENT)) {
                    MapObject_Delete(mapObj);
                }
                break;
            case 2:
                break;
            case 1:
                break;
            }
        }

        mapObj++;
        maxObjects--;
    }

    ov5_021EDA38(mapObjMan, MapObjectMan_GetDrawManager(mapObjMan));
}

static MapObjectManager *MapObjectMan_Alloc(int maxObjs)
{
    int size;
    MapObject *mapObj;
    MapObjectManager *mapObjMan = Heap_Alloc(HEAP_ID_FIELD2, sizeof(MapObjectManager));

    GF_ASSERT(mapObjMan != NULL);
    memset(mapObjMan, 0, sizeof(MapObjectManager));

    size = sizeof(MapObject) * maxObjs;
    mapObj = Heap_Alloc(HEAP_ID_FIELD2, size);

    GF_ASSERT(mapObj != NULL);
    memset(mapObj, 0, size);

    MapObjectMan_SetMapObject(mapObjMan, mapObj);

    return mapObjMan;
}

MapObject *MapObjectMan_AddMapObjectFromHeader(const MapObjectManager *mapObjMan, const ObjectEvent *objectEvent, enum MapHeaderID mapHeaderID)
{
    MapObject *mapObj;
    ObjectEvent v1 = *objectEvent;
    ObjectEvent *v2 = &v1;

    int localID = ObjectEvent_GetLocalID(v2);

    if (ObjectEvent_HasNoScript(v2) == FALSE) {
        mapObj = MapObjectMan_FindObjectByLocalIDAndMap(mapObjMan, localID, mapHeaderID);

        if (mapObj != NULL) {
            MapObject_ReinitFromObjectEvent(mapObj, v2, mapHeaderID);

            return mapObj;
        }
    } else {
        mapObj = MapObjectMan_FindObjectByLocalIDAndFlag(mapObjMan, localID, ObjectEvent_GetHiddenFlagNoScript(v2));

        if (mapObj != NULL) {
            MapObject_ReinitFromObjectEventNoScript(mapObj, mapHeaderID, v2);
            return mapObj;
        }
    }

    mapObj = MapObjectMan_GetFreeObject(mapObjMan);

    if (mapObj == NULL) {
        return mapObj;
    }

    MapObject_InitFromObjectEvent(mapObj, v2, MapObjectMan_FieldSystem(mapObjMan));
    MapObject_InitStatusAndManager(mapObj, mapObjMan);
    MapObject_SetMapHeaderID(mapObj, mapHeaderID);
    MapObject_InitMovement(mapObj);
    MapObject_RefreshDraw(mapObj);
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_START_MOVEMENT);
    MapObjectMan_AddMoveTask(mapObjMan, mapObj);
    MapObjectMan_IncObjectCount(MapObjectMan_Deconst(mapObjMan));

    return mapObj;
}

MapObject *MapObjectMan_AddMapObject(const MapObjectManager *mapObjMan, int x, int z, int initialDir, int graphicsID, int movementType, enum MapHeaderID mapHeaderID)
{
    ObjectEvent objectEvent;
    MapObject *mapObj;

    ObjectEvent_SetLocalID(&objectEvent, 0);
    ObjectEvent_SetGraphicsID(&objectEvent, graphicsID);
    ObjectEvent_SetMovementType(&objectEvent, movementType);
    ObjectEvent_SetTrainerType(&objectEvent, 0);
    ObjectEvent_SetHiddenFlag(&objectEvent, 0);
    ObjectEvent_SetScript(&objectEvent, 0);
    ObjectEvent_SetInitialDir(&objectEvent, initialDir);
    ObjectEvent_SetDataAt(&objectEvent, 0, 0);
    ObjectEvent_SetDataAt(&objectEvent, 0, 1);
    ObjectEvent_SetDataAt(&objectEvent, 0, 2);
    ObjectEvent_SetMovementRangeX(&objectEvent, 0);
    ObjectEvent_SetMovementRangeZ(&objectEvent, 0);
    ObjectEvent_SetX(&objectEvent, x);
    ObjectEvent_SetZ(&objectEvent, z);
    ObjectEvent_SetY(&objectEvent, 0);

    mapObj = MapObjectMan_AddMapObjectFromHeader(mapObjMan, &objectEvent, mapHeaderID);

    return mapObj;
}

MapObject *MapObjectMan_AddMapObjectFromLocalID(const MapObjectManager *mapObjMan, int localID, int objEventCount, enum MapHeaderID mapHeaderID, const ObjectEvent *objectEvent)
{
    MapObject *mapObj = NULL;
    const ObjectEvent *v1 = ObjectEvent_FindByLocalID(localID, objEventCount, objectEvent);

    if (v1 != NULL) {
        int hiddenFlag = ObjectEvent_GetHiddenFlag(v1);
        FieldSystem *fieldSystem = MapObjectMan_FieldSystem(mapObjMan);

        if (!FieldSystem_CheckFlag(fieldSystem, hiddenFlag)) {
            mapObj = MapObjectMan_AddMapObjectFromHeader(mapObjMan, v1, mapHeaderID);
        }
    }

    return mapObj;
}

void MapObject_SetGraphicsIDAndRefresh(MapObject *mapObj, int graphicsID)
{
    MapObject_SetGraphicsID(mapObj, graphicsID);
    MapObject_StartMovementAndResetShadow(mapObj);
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_14);
    MapObject_RefreshDraw(mapObj);
}

// Switches the object to a new graphics ID, tearing down the current renderer
// first if one is active.
void MapObject_ChangeGraphics(MapObject *mapObj, int param1)
{
    if (MapObject_IsDrawReady(mapObj) == TRUE) {
        MapObject_ClearGraphics(mapObj);
    }

    MapObject_SetGraphicsIDAndRefresh(mapObj, param1);
}

void MapObject_Delete(MapObject *mapObj)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsDrawInitialized(mapObjMan) == TRUE) {
        MapObject_CallRendererFree(mapObj);
    }

    MapObject_CallMovementFree(mapObj);
    MapObject_DeleteTask(mapObj);
    MapObjectMan_DecObjectCount(MapObject_GetManagerMutable(mapObj));
    MapObject_Clear(mapObj);
}

void MapObject_SetFlagAndDeleteObject(MapObject *mapObj)
{
    int flag = MapObject_GetFlag(mapObj);
    FieldSystem_SetFlag(MapObject_FieldSystem(mapObj), flag);
    MapObject_Delete(mapObj);
}

// Tears down the object's renderer and installs no-op callbacks so the object
// draws nothing.
void MapObject_ClearGraphics(MapObject *mapObj)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsDrawInitialized(mapObjMan) == TRUE) {
        if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_14)) {
            MapObject_CallRendererFree(mapObj);
        }

        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_14);
    }

    MapObject_SetGraphicsID(mapObj, 0xffff);
    MapObject_SetRendererInitCallback(mapObj, MapObjectRenderer_NoOp1);
    MapObject_SetRendererDrawCallback(mapObj, MapObjectRenderer_NoOp2);
    MapObject_SetRendererFreeCallback(mapObj, MapObjectRenderer_NoOp2);
    MapObject_SetRendererUnloadCallback(mapObj, MapObjectRenderer_NoOp3);
    MapObject_SetRendererLoadCallback(mapObj, MapObjectRenderer_NoOp4);
}

void MapObjectMan_DeleteAll(MapObjectManager *mapObjMan)
{
    int i = 0;
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_0)) {
            MapObject_Delete(mapObj);
        }

        mapObj++;
        i++;
    } while (i < maxObjects);
}

void MapObjectMan_UnloadAllRenderers(MapObjectManager *mapObjMan)
{
    GF_ASSERT(MapObjectMan_IsDrawInitialized(mapObjMan) == TRUE);

    int i = 0;
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_0) && MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_14)) {
            MapObject_CallRendererUnload(mapObj);
            MapObject_NoOp(mapObj);
        }

        mapObj++;
        i++;
    } while (i < maxObjects);
}

void MapObjectMan_LoadAllRenderers(MapObjectManager *mapObjMan)
{
    GF_ASSERT(MapObjectMan_IsDrawInitialized(mapObjMan) == TRUE);

    int i = 0;
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_IsActive(mapObj) == TRUE) {
            if (MapObject_CheckStatus14(mapObj) == TRUE) {
                MapObject_CallRendererLoad(mapObj);
            } else {
                MapObject_RefreshDraw(mapObj);
            }

            MapObject_StartMovementAndResetShadow(mapObj);
            MapObject_UpdateDisguiseMovement(mapObj);
        }

        mapObj++;
        i++;
    } while (i < maxObjects);
}

void MapObjectMan_SaveAll(FieldSystem *fieldSystem, const MapObjectManager *mapObjMan, MapObjectSave *mapObjSave, int param3)
{
    int v0 = 0;
    MapObject *mapObj;

    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &mapObj, &v0, MAP_OBJ_STATUS_0)) {
        MapObject_Save(fieldSystem, mapObj, mapObjSave);
        mapObjSave++;
        param3--;
        GF_ASSERT(param3 > 0);
    }

    if (param3) {
        memset(mapObjSave, 0, param3 * sizeof(MapObjectSave));
    }
}

void MapObjectMan_LoadAllObjects(const MapObjectManager *mapObjMan, MapObjectSave *mapObjSave, int size)
{
    int v0 = 0;
    MapObject *mapObj;

    while (size) {
        if (mapObjSave->status & MAP_OBJ_STATUS_0) {
            mapObj = MapObjectMan_GetFreeObject(mapObjMan);
            GF_ASSERT(mapObj != NULL);

            MapObject_LoadSave(mapObj, mapObjSave);
            MapObject_InitAfterLoad(mapObjMan, mapObj);
        }

        mapObjSave++;
        size--;
    }
}

static void MapObject_Save(FieldSystem *fieldSystem, MapObject *mapObj, MapObjectSave *mapObjSave)
{
    mapObjSave->status = MapObject_GetStatus(mapObj);
    mapObjSave->flags2 = MapObject_GetFlags2(mapObj);
    mapObjSave->localID = MapObject_GetLocalID(mapObj);
    mapObjSave->mapID = MapObject_GetMapHeaderID(mapObj);
    mapObjSave->graphicsID = MapObject_GetGraphicsID(mapObj);
    mapObjSave->movementType = MapObject_GetMovementType(mapObj);
    mapObjSave->trainerType = MapObject_GetTrainerType(mapObj);
    mapObjSave->flag = MapObject_GetFlag(mapObj);
    mapObjSave->script = MapObject_GetScript(mapObj);
    mapObjSave->initialDir = MapObject_GetInitialDir(mapObj);
    mapObjSave->facingDir = MapObject_GetFacingDir(mapObj);
    mapObjSave->movingDir = MapObject_GetMovingDir(mapObj);
    mapObjSave->param[0] = MapObject_GetDataAt(mapObj, 0);
    mapObjSave->param[1] = MapObject_GetDataAt(mapObj, 1);
    mapObjSave->param[2] = MapObject_GetDataAt(mapObj, 2);
    mapObjSave->movementRangeX = MapObject_GetMovementRangeX(mapObj);
    mapObjSave->movementRangeZ = MapObject_GetMovementRangeZ(mapObj);
    mapObjSave->xInitial = MapObject_GetXInitial(mapObj);
    mapObjSave->yInitial = MapObject_GetYInitial(mapObj);
    mapObjSave->zInitial = MapObject_GetZInitial(mapObj);
    mapObjSave->x = MapObject_GetX(mapObj);
    mapObjSave->y = MapObject_GetY(mapObj);
    mapObjSave->z = MapObject_GetZ(mapObj);

    VecFx32 v0;
    int v1, v2;

    VecFx32_SetPosFromMapCoords(mapObjSave->x, mapObjSave->z, &v0);
    v0.y = MapObject_GetPosY(mapObj);

    v2 = MapObject_IsDynamicHeightCalculationEnabled(mapObj);
    v1 = MapObject_RecalculatePositionHeightEx(fieldSystem, &v0, v2);

    if (v1 == 0) {
        mapObjSave->vecY = MapObject_GetPosY(mapObj);
    } else {
        if (MapObject_IsHeightCalculationDisabled(mapObj) == TRUE) {
            v0.y = MapObject_GetPosY(mapObj);
        }

        mapObjSave->vecY = v0.y;
    }

    memcpy(mapObjSave->unk_30, MapObject_GetUnkD8(mapObj), 16);
    memcpy(mapObjSave->unk_40, MapObject_GetUnkE8(mapObj), 16);
}

static void MapObject_LoadSave(MapObject *mapObj, MapObjectSave *mapObjSave)
{
    MapObject_SetStatus(mapObj, mapObjSave->status);
    MapObject_SetFlags2(mapObj, mapObjSave->flags2);
    MapObject_SetLocalID(mapObj, mapObjSave->localID);
    MapObject_SetMapHeaderID(mapObj, mapObjSave->mapID);
    MapObject_SetGraphicsID(mapObj, mapObjSave->graphicsID);
    MapObject_SetMovementType(mapObj, mapObjSave->movementType);
    MapObject_SetTrainerType(mapObj, mapObjSave->trainerType);
    MapObject_SetFlag(mapObj, mapObjSave->flag);
    MapObject_SetScript(mapObj, mapObjSave->script);
    MapObject_SetInitialDir(mapObj, mapObjSave->initialDir);
    MapObject_Face(mapObj, mapObjSave->facingDir);
    MapObject_Turn(mapObj, mapObjSave->movingDir);
    MapObject_SetDataAt(mapObj, mapObjSave->param[0], 0);
    MapObject_SetDataAt(mapObj, mapObjSave->param[1], 1);
    MapObject_SetDataAt(mapObj, mapObjSave->param[2], 2);
    MapObject_SetMovementRangeX(mapObj, mapObjSave->movementRangeX);
    MapObject_SetMovementRangeZ(mapObj, mapObjSave->movementRangeZ);
    MapObject_SetXInitial(mapObj, mapObjSave->xInitial);
    MapObject_SetYInitial(mapObj, mapObjSave->yInitial);
    MapObject_SetZInitial(mapObj, mapObjSave->zInitial);
    MapObject_SetX(mapObj, mapObjSave->x);
    MapObject_SetY(mapObj, mapObjSave->y);
    MapObject_SetZ(mapObj, mapObjSave->z);

    VecFx32 v0 = { 0, 0, 0 };

    v0.y = mapObjSave->vecY;
    MapObject_SetPos(mapObj, &v0);

    memcpy(MapObject_GetUnkD8(mapObj), mapObjSave->unk_30, 16);
    memcpy(MapObject_GetUnkE8(mapObj), mapObjSave->unk_40, 16);
}

static void MapObject_InitAfterLoad(const MapObjectManager *mapObjMan, MapObject *mapObj)
{
    MapObject_ResetStatusForLoad(mapObj);
    MapObject_SyncPosFromCoords(mapObj);
    MapObject_SetMapObjectManager(mapObj, mapObjMan);
    MapObject_LoadMovementCallbacks(mapObj);
    sub_020656DC(mapObj);
    MapObject_RefreshDraw(mapObj);
    MapObjectMan_AddMoveTask(mapObjMan, mapObj);
    MapObject_CallMovementLoad(mapObj);
    MapObjectMan_IncObjectCount(MapObjectMan_Deconst(mapObjMan));
}

static void MapObject_ResetStatusForLoad(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_0 | MAP_OBJ_STATUS_START_MOVEMENT);
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_PAUSE_MOVEMENT | MAP_OBJ_STATUS_HIDE | MAP_OBJ_STATUS_14 | MAP_OBJ_STATUS_START_JUMP | MAP_OBJ_STATUS_END_JUMP | MAP_OBJ_STATUS_END_MOVEMENT | MAP_OBJ_STATUS_18 | MAP_OBJ_STATUS_19 | MAP_OBJ_STATUS_21 | MAP_OBJ_STATUS_22 | MAP_OBJ_HEIGHT_CALCULATION_DISABLED);
    MapObject_ResetShadowFlags(mapObj);
}

static void MapObject_SyncPosFromCoords(MapObject *mapObj)
{
    int v0;
    VecFx32 v1;

    MapObject_GetPosPtr(mapObj, &v1);

    v0 = MapObject_GetX(mapObj);
    v1.x = (((v0) << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);

    MapObject_SetXPrev(mapObj, v0);
    v0 = MapObject_GetY(mapObj);
    MapObject_SetYPrev(mapObj, v0);

    v0 = MapObject_GetZ(mapObj);
    v1.z = (((v0) << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);

    MapObject_SetZPrev(mapObj, v0);
    MapObject_SetPos(mapObj, &v1);
}

void MapObjectMan_AddMapObjectsFromEvents(const MapObjectManager *mapObjMan, enum MapHeaderID mapHeaderID, u32 numObjectEvents, const ObjectEvent *objectEvent)
{
    GF_ASSERT(numObjectEvents);

    int v0 = sizeof(ObjectEvent) * numObjectEvents;
    ObjectEvent *v1 = Heap_AllocAtEnd(HEAP_ID_FIELD2, v0);

    GF_ASSERT(v1 != NULL);
    memcpy(v1, objectEvent, v0);

    MapObjectMan_AddObjectsWork *v2 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(MapObjectMan_AddObjectsWork));
    GF_ASSERT(v2 != NULL);

    v2->mapHeaderID = mapHeaderID;
    v2->numObjectEvents = numObjectEvents;
    v2->index = 0;
    v2->mapObjMan = mapObjMan;
    v2->objectEvent = v1;

    MapObjectMan_AddMapObjectsFromEventsWorker(v2);
}

static void MapObjectMan_AddMapObjectsFromEventsWorker(MapObjectMan_AddObjectsWork *param0)
{
    MapObject *mapObj;
    FieldSystem *fieldSystem;
    const ObjectEvent *objectEvent;

    fieldSystem = MapObjectMan_FieldSystem(param0->mapObjMan);
    objectEvent = param0->objectEvent;

    do {
        if (ObjectEvent_HasNoScript(objectEvent) == TRUE || FieldSystem_CheckFlag(fieldSystem, objectEvent->hiddenFlag) == FALSE) {
            mapObj = MapObjectMan_AddMapObjectFromHeader(param0->mapObjMan, objectEvent, param0->mapHeaderID);
            GF_ASSERT(mapObj != NULL);
        }

        objectEvent++;
        param0->index++;
    } while (param0->index < param0->numObjectEvents);

    Heap_FreeExplicit(HEAP_ID_FIELD2, param0->objectEvent);
    Heap_FreeExplicit(HEAP_ID_FIELD2, param0);
}

static MapObject *MapObjectMan_GetFreeObject(const MapObjectManager *mapObjMan)
{
    int i = 0;
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (!MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_0)) {
            return mapObj;
        }

        mapObj++;
        i++;
    } while (i < maxObjects);

    return NULL;
}

static MapObject *MapObjectMan_FindObjectByLocalIDAndMap(const MapObjectManager *mapObjMan, int param1, int param2)
{
    int v0 = 0;
    MapObject *mapObj;

    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &mapObj, &v0, MAP_OBJ_STATUS_0) == TRUE) {
        if (MapObject_CheckStatus25(mapObj) == TRUE
            && MapObject_GetLocalID(mapObj) == param1
            && MapObject_GetNoScriptFlag(mapObj) == param2) {
            return mapObj;
        }
    }

    return NULL;
}

static void MapObjectMan_AddMoveTask(const MapObjectManager *mapObjMan, MapObject *mapObj)
{
    int v0 = MapObjectMan_GetTaskBasePriority(mapObjMan);
    int movementType = MapObject_GetMovementType(mapObj);
    SysTask *task;

    if (movementType == MOVEMENT_TYPE_FOLLOW_PLAYER
        || movementType == MOVEMENT_TYPE_FOLLOW_PARTNER_TRAINER) {
        v0 += 2;
    }

    task = SysTask_Start(MapObjectTask_Move, mapObj, v0);
    GF_ASSERT(task != NULL);

    MapObject_SetTask(mapObj, task);
}

static void MapObject_InitFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent, FieldSystem *fieldSystem)
{
    MapObject_SetLocalID(mapObj, ObjectEvent_GetLocalID(objectEvent));
    MapObject_SetGraphicsID(mapObj, MapObject_GetFieldSystemGraphicsID(fieldSystem, ObjectEvent_GetGraphicsID(objectEvent)));
    MapObject_SetMovementType(mapObj, ObjectEvent_GetMovementType(objectEvent));
    MapObject_SetTrainerType(mapObj, ObjectEvent_GetTrainerType(objectEvent));
    MapObject_SetFlag(mapObj, ObjectEvent_GetHiddenFlag(objectEvent));
    MapObject_SetScript(mapObj, ObjectEvent_GetScript(objectEvent));
    MapObject_SetInitialDir(mapObj, ObjectEvent_GetInitialDir(objectEvent));
    MapObject_SetDataAt(mapObj, ObjectEvent_GetDataAt(objectEvent, 0), 0);
    MapObject_SetDataAt(mapObj, ObjectEvent_GetDataAt(objectEvent, 1), 1);
    MapObject_SetDataAt(mapObj, ObjectEvent_GetDataAt(objectEvent, 2), 2);
    MapObject_SetMovementRangeX(mapObj, ObjectEvent_GetMovementRangeX(objectEvent));
    MapObject_SetMovementRangeZ(mapObj, ObjectEvent_GetMovementRangeZ(objectEvent));
    MapObject_SetInitialPosFromObjectEvent(mapObj, objectEvent);
}

static void MapObject_SetInitialPosFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent)
{
    int v0 = ObjectEvent_GetX(objectEvent);
    VecFx32 v1;

    v1.x = (((v0) << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);

    MapObject_SetXInitial(mapObj, v0);
    MapObject_SetXPrev(mapObj, v0);
    MapObject_SetX(mapObj, v0);

    v0 = ObjectEvent_GetY(objectEvent);
    v1.y = (fx32)v0;
    v0 = ((v0) >> 3) / FX32_ONE;

    MapObject_SetYInitial(mapObj, v0);
    MapObject_SetYPrev(mapObj, v0);
    MapObject_SetY(mapObj, v0);

    v0 = ObjectEvent_GetZ(objectEvent);
    v1.z = (((v0) << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);

    MapObject_SetZInitial(mapObj, v0);
    MapObject_SetZPrev(mapObj, v0);
    MapObject_SetZ(mapObj, v0);
    MapObject_SetPos(mapObj, &v1);
}

static void MapObject_InitStatusAndManager(MapObject *mapObj, const MapObjectManager *mapObjMan)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_0 | MAP_OBJ_STATUS_12 | MAP_OBJ_STATUS_11);

    if (MapObject_HasNoScript(mapObj) == TRUE) {
        MapObject_SetStatus25(mapObj, 1);
    }

    MapObject_SetMapObjectManager(mapObj, mapObjMan);
    MapObject_Face(mapObj, MapObject_GetInitialDir(mapObj));
    MapObject_Turn(mapObj, MapObject_GetInitialDir(mapObj));
    sub_020656DC(mapObj);
}

static void MapObject_LoadMovementCallbacks(MapObject *mapObj)
{
    const UnkStruct_020EDF0C *v0 = MovementType_GetCallbackStruct(MapObject_GetMovementType(mapObj));

    MapObject_SetMovementInitCallback(mapObj, MovementType_GetInitCallback(v0));
    MapObject_SetMovementUpdateCallback(mapObj, MovementType_GetUpdateCallback(v0));
    MapObject_SetMovementFreeCallback(mapObj, MovementType_GetFreeCallback(v0));
}

static void MapObject_LoadRendererCallbacks(MapObject *mapObj)
{
    const MapObjectRendererCallbacks *v0;
    u32 v1 = MapObject_GetGraphicsID(mapObj);

    if (v1 == 0x2000) {
        v0 = &gInvisibleObjectEventGfxRenderer;
    } else {
        v0 = Renderer_GetByGraphicsID(v1);
    }

    MapObject_SetRendererInitCallback(mapObj, Renderer_GetInitCallback(v0));
    MapObject_SetRendererDrawCallback(mapObj, Renderer_GetDrawCallback(v0));
    MapObject_SetRendererFreeCallback(mapObj, Renderer_GetFreeCallback(v0));
    MapObject_SetRendererUnloadCallback(mapObj, Renderer_GetUnloadCallback(v0));
    MapObject_SetRendererLoadCallback(mapObj, Renderer_GetLoadCallback(v0));
}

static void MapObject_Clear(MapObject *mapObj)
{
    memset(mapObj, 0, sizeof(MapObject));
}

// Searches the map's object events for one that corresponds to `mapObj`.
// Returns 0 if none match, 1 if a no-script event matches the object's
// no-script flag, or 2 if an event for the same map matches.
static int MapObject_CheckObjectEventMatch(const MapObject *mapObj, int param1, int objEventCount, const ObjectEvent *objectEvent)
{
    int localID;
    int flag;

    while (objEventCount) {
        localID = ObjectEvent_GetLocalID(objectEvent);

        if (MapObject_GetLocalID(mapObj) == localID) {
            if (ObjectEvent_HasNoScript(objectEvent) == TRUE) {
                flag = ObjectEvent_GetHiddenFlagNoScript(objectEvent);

                if (MapObject_CheckStatus25(mapObj) == TRUE) {
                    if (MapObject_GetNoScriptFlag(mapObj) == flag) {
                        return 1;
                    }
                } else if (MapObject_GetMapHeaderID(mapObj) == flag) {
                    return 2;
                }
            } else if (MapObject_CheckStatus25(mapObj) == TRUE && MapObject_GetNoScriptFlag(mapObj) == param1) {
                return 2;
            }
        }

        objEventCount--;
        objectEvent++;
    }

    return 0;
}

static MapObject *MapObjectMan_FindObjectByLocalIDAndFlag(const MapObjectManager *mapObjMan, int localID, int flag)
{
    int v0 = 0;
    MapObject *mapObj;

    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &mapObj, &v0, MAP_OBJ_STATUS_0) == TRUE) {
        if (MapObject_GetLocalID(mapObj) == localID && MapObject_GetMapHeaderID(mapObj) == flag) {
            return mapObj;
        }
    }

    return NULL;
}

MapObject *MapObjMan_LocalMapObjByIndex(const MapObjectManager *mapObjMan, int index)
{
    int maxObjects;
    MapObject *mapObj;

    GF_ASSERT(mapObjMan != NULL);

    maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    mapObj = MapObjectMan_GetMapObjectStatic(mapObjMan);

    do {
        if (MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_0) == TRUE && MapObject_CheckStatus25(mapObj) == FALSE
            && MapObject_GetLocalID(mapObj) == index) {
            return mapObj;
        }

        mapObj++;
        maxObjects--;
    } while (maxObjects > 0);

    return NULL;
}

MapObject *MapObjMan_GetLocalMapObjByMovementType(const MapObjectManager *mapObjMan, int movementType)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObjectStatic(mapObjMan);

    do {
        if (MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_0) == TRUE && MapObject_GetMovementType(mapObj) == movementType) {
            return mapObj;
        }

        mapObj++;
        maxObjects--;
    } while (maxObjects > 0);

    return NULL;
}

BOOL MapObjectMan_FindObjectWithStatus(const MapObjectManager *mapObjMan, MapObject **mapObj, int *startIdx, u32 status)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *currMapObj;

    if (*startIdx >= maxObjects) {
        return FALSE;
    }

    currMapObj = MapObjectMan_GetMapObjectStatic(mapObjMan);
    currMapObj = &currMapObj[*startIdx];

    do {
        (*startIdx)++;

        if (MapObject_CheckStatus(currMapObj, status) == status) {
            *mapObj = currMapObj;
            return TRUE;
        }

        currMapObj++;
    } while (*startIdx < maxObjects);

    return FALSE;
}

static void MapObject_StartMovementAndResetShadow(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_START_MOVEMENT);
    MapObject_ResetShadowFlags(mapObj);
}

static void MapObject_ResetShadowFlags(MapObject *mapObj)
{
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_SHOW_SHADOW | MAP_OBJ_STATUS_HIDE_SHADOW | MAP_OBJ_STATUS_26 | MAP_OBJ_STATUS_24);
}

static void MapObject_NoOp(MapObject *mapObj)
{
    (void)0;
}

static int MapObject_GetFieldSystemGraphicsID(FieldSystem *fieldSystem, int graphicsID)
{
    if (graphicsID >= OBJ_EVENT_GFX_VAR_0 && graphicsID <= OBJ_EVENT_GFX_VAR_F) {
        graphicsID -= OBJ_EVENT_GFX_VAR_0;
        graphicsID = FieldSystem_GetGraphicsID(fieldSystem, graphicsID);
    }

    return graphicsID;
}

static void MapObject_RecalculateHeightIfNeeded(MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_12)) {
        MapObject_RecalculateObjectHeight(mapObj);
    }
}

static void MapObject_InitMovement(MapObject *mapObj)
{
    MapObject_LoadMovementCallbacks(mapObj);
    MapObject_InitMove(mapObj);
}

static void MapObject_RefreshDraw(MapObject *mapObj)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsDrawInitialized(mapObjMan) == FALSE) {
        return;
    }

    MapObject_RecalculateHeightIfNeeded(mapObj);
    MapObject_SetUnkA0(mapObj, 0);
    ov5_021EDD78(mapObj, 0);

    if (MapObject_CheckStatus14(mapObj) == FALSE) {
        MapObject_LoadRendererCallbacks(mapObj);
        MapObject_CallRendererInit(mapObj);
        MapObject_SetStatus14(mapObj);
    }
}

int MapObject_HasNoScript(const MapObject *mapObj)
{
    u16 script = (u16)MapObject_GetScript(mapObj);

    if (script == 0xffff) {
        return TRUE;
    }

    return FALSE;
}

static void MapObject_ReinitFromObjectEvent(MapObject *mapObj, const ObjectEvent *objectEvent, enum MapHeaderID mapHeaderID)
{
    GF_ASSERT(MapObject_CheckStatus25(mapObj) == TRUE);

    MapObject_SetStatus25(mapObj, 0);
    MapObject_SetMapHeaderID(mapObj, mapHeaderID);
    MapObject_SetScript(mapObj, ObjectEvent_GetScript(objectEvent));
    MapObject_SetFlag(mapObj, ObjectEvent_GetHiddenFlag(objectEvent));
}

static void MapObject_ReinitFromObjectEventNoScript(MapObject *mapObj, enum MapHeaderID mapHeaderID, const ObjectEvent *objectEvent)
{
    GF_ASSERT(ObjectEvent_HasNoScript(objectEvent) == TRUE);

    MapObject_SetStatus25(mapObj, 1);
    MapObject_SetScript(mapObj, ObjectEvent_GetScript(objectEvent));
    MapObject_SetFlag(mapObj, ObjectEvent_GetHiddenFlagNoScript(objectEvent));
    MapObject_SetMapHeaderID(mapObj, mapHeaderID);
}

int MapObject_CalculateTaskPriority(const MapObject *mapObj, int priority)
{
    int result = MapObject_GetTaskBasePriority(mapObj);
    result += priority;

    return result;
}

// Returns TRUE if the object is active, has the given local ID, and belongs to
// the given map. No-script objects store their map in the flag field instead.
int MapObject_MatchesLocalIDAndMap(const MapObject *mapObj, int param1, enum MapHeaderID param2)
{
    if (!MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_0)) {
        return FALSE;
    }

    if (MapObject_GetLocalID(mapObj) != param1) {
        return FALSE;
    }

    if (MapObject_GetMapHeaderID(mapObj) != param2) {
        if (MapObject_CheckStatus25(mapObj) == FALSE) {
            return FALSE;
        }

        if (MapObject_GetNoScriptFlag(mapObj) != param2) {
            return FALSE;
        }
    }

    return TRUE;
}

// Returns TRUE if the object is active, has the given effective graphics ID,
// and matches the given local ID and map.
int MapObject_MatchesGraphicsAndLocalID(const MapObject *mapObj, int param1, int param2, enum MapHeaderID param3)
{
    if (!MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_0)) {
        return 0;
    }

    int v0 = MapObject_GetEffectiveGraphicsID(mapObj);

    if (v0 != param1) {
        return 0;
    }

    return MapObject_MatchesLocalIDAndMap(mapObj, param2, param3);
}

static void MapObjectTask_Move(SysTask *task, void *_mapObject)
{
    MapObject *mapObj = (MapObject *)_mapObject;

    MapObject_Move(mapObj);

    if (MapObject_IsActive(mapObj) == FALSE) {
        return;
    }

    MapObjectTask_Draw(mapObj);
}

static void MapObjectTask_Draw(MapObject *mapObj)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsDrawInitialized(mapObjMan) == TRUE) {
        MapObject_Draw(mapObj);
    }
}

static MapObjectManager *MapObjectMan_Deconst(const MapObjectManager *mapObjMan)
{
    return (MapObjectManager *)mapObjMan;
}

void MapObjectMan_SetMaxObjects(MapObjectManager *mapObjMan, int maxObjs)
{
    mapObjMan->maxObjects = maxObjs;
}

int MapObjectMan_GetMaxObjects(const MapObjectManager *mapObjMan)
{
    return mapObjMan->maxObjects;
}

static void MapObjectMan_IncObjectCount(MapObjectManager *mapObjMan)
{
    mapObjMan->objectCount++;
}

static void MapObjectMan_DecObjectCount(MapObjectManager *mapObjMan)
{
    mapObjMan->objectCount--;
}

void MapObjectMan_SetStatusFlagOn(MapObjectManager *mapObjMan, u32 flag)
{
    mapObjMan->status |= flag;
}

void MapObjectMan_SetStatusFlagOff(MapObjectManager *mapObjMan, u32 flag)
{
    mapObjMan->status &= ~flag;
}

u32 MapObjectMan_CheckStatus(const MapObjectManager *mapObjMan, u32 flag)
{
    return mapObjMan->status & flag;
}

void MapObjectMan_SetTaskBasePriority(MapObjectManager *mapObjMan, int basePriority)
{
    mapObjMan->taskBasePriority = basePriority;
}

int MapObjectMan_GetTaskBasePriority(const MapObjectManager *mapObjMan)
{
    return mapObjMan->taskBasePriority;
}

UnkStruct_ov5_021ED0A4 *MapObjectMan_GetDrawManager(const MapObjectManager *mapObjMan)
{
    return &(((MapObjectManager *)mapObjMan)->drawManager);
}

void MapObjectMan_SetMapObject(MapObjectManager *mapObjMan, MapObject *mapObj)
{
    mapObjMan->mapObj = mapObj;
}

const MapObject *MapObjectMan_GetMapObjectConst(const MapObjectManager *mapObjMan)
{
    return mapObjMan->mapObj;
}

static MapObject *MapObjectMan_GetMapObjectStatic(const MapObjectManager *mapObjMan)
{
    return mapObjMan->mapObj;
}

MapObject *MapObjectMan_GetMapObject(const MapObjectManager *mapObjMan)
{
    return mapObjMan->mapObj;
}

void MapObject_AdvancePointer(const MapObject **mapObj)
{
    (*mapObj)++;
}

void MapObjectMan_SetFieldSystem(MapObjectManager *mapObjMan, FieldSystem *fieldSystem)
{
    mapObjMan->fieldSystem = fieldSystem;
}

FieldSystem *MapObjectMan_FieldSystem(const MapObjectManager *mapObjMan)
{
    return mapObjMan->fieldSystem;
}

void MapObjectMan_SetNARC(MapObjectManager *mapObjMan, NARC *narc)
{
    mapObjMan->narc = narc;
}

NARC *MapObjectMan_GetNARC(const MapObjectManager *mapObjMan)
{
    GF_ASSERT(mapObjMan->narc != NULL);
    return ((MapObjectManager *)mapObjMan)->narc;
}

void MapObject_SetStatus(MapObject *mapObj, u32 status)
{
    mapObj->status = status;
}

u32 MapObject_GetStatus(const MapObject *mapObj)
{
    return mapObj->status;
}

void MapObject_SetStatusFlagOn(MapObject *mapObj, u32 flag)
{
    mapObj->status |= flag;
}

void MapObject_SetStatusFlagOff(MapObject *mapObj, u32 flag)
{
    mapObj->status &= ~flag;
}

u32 MapObject_CheckStatus(const MapObject *mapObj, u32 flag)
{
    return mapObj->status & flag;
}

BOOL MapObject_CheckStatusFlag(const MapObject *mapObj, u32 flag)
{
    return mapObj->status & flag
        ? TRUE
        : FALSE;
}

void MapObject_SetFlags2(MapObject *mapObj, u32 param1)
{
    mapObj->flags2 = param1;
}

u32 MapObject_GetFlags2(const MapObject *mapObj)
{
    return mapObj->flags2;
}

void MapObject_SetFlags2FlagOn(MapObject *mapObj, u32 param1)
{
    mapObj->flags2 |= param1;
}

void MapObject_SetFlags2FlagOff(MapObject *mapObj, u32 param1)
{
    mapObj->flags2 &= ~param1;
}

u32 MapObject_CheckFlags2(const MapObject *mapObj, u32 param1)
{
    return mapObj->flags2 & param1;
}

void MapObject_SetLocalID(MapObject *mapObj, u32 localID)
{
    mapObj->localID = localID;
}

u32 MapObject_GetLocalID(const MapObject *mapObj)
{
    return mapObj->localID;
}

void MapObject_SetMapHeaderID(MapObject *mapObj, enum MapHeaderID mapHeaderID)
{
    mapObj->mapHeaderID = mapHeaderID;
}

enum MapHeaderID MapObject_GetMapHeaderID(const MapObject *mapObj)
{
    return mapObj->mapHeaderID;
}

void MapObject_SetGraphicsID(MapObject *mapObj, u32 graphicsID)
{
    mapObj->graphicsID = graphicsID;
}

u32 MapObject_GetGraphicsID(const MapObject *mapObj)
{
    return mapObj->graphicsID;
}

u32 MapObject_GetEffectiveGraphicsID(const MapObject *mapObj)
{
    u32 graphicsID = MapObject_GetGraphicsID(mapObj);

    if (BerryPatchGraphics_IsBerryPatch(graphicsID) == TRUE) {
        graphicsID = BerryPatchGraphics_GetCurrentGraphicsResourceID(mapObj);
    }

    return graphicsID;
}

void MapObject_SetMovementType(MapObject *mapObj, u32 movementType)
{
    mapObj->movementType = movementType;
}

u32 MapObject_GetMovementType(const MapObject *mapObj)
{
    return mapObj->movementType;
}

void MapObject_SetTrainerType(MapObject *mapObj, u32 trainerType)
{
    mapObj->trainerType = trainerType;
}

u32 MapObject_GetTrainerType(const MapObject *mapObj)
{
    return mapObj->trainerType;
}

void MapObject_SetFlag(MapObject *mapObj, u32 flag)
{
    mapObj->flag = flag;
}

u32 MapObject_GetFlag(const MapObject *mapObj)
{
    return mapObj->flag;
}

void MapObject_SetScript(MapObject *mapObj, u32 script)
{
    mapObj->script = script;
}

u32 MapObject_GetScript(const MapObject *mapObj)
{
    return mapObj->script;
}

void MapObject_SetInitialDir(MapObject *mapObj, int initialDir)
{
    mapObj->initialDir = initialDir;
}

u32 MapObject_GetInitialDir(const MapObject *mapObj)
{
    return mapObj->initialDir;
}

void MapObject_Face(MapObject *mapObj, int dir)
{
    mapObj->prevFacingDir = mapObj->facingDir;
    mapObj->facingDir = dir;
}

void MapObject_TryFace(MapObject *mapObj, int dir)
{
    if (!MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_LOCK_DIR)) {
        mapObj->prevFacingDir = mapObj->facingDir;
        mapObj->facingDir = dir;
    }
}

int MapObject_GetFacingDir(const MapObject *mapObj)
{
    return mapObj->facingDir;
}

int MapObject_GetPrevFacingDir(const MapObject *mapObj)
{
    return mapObj->prevFacingDir;
}

void MapObject_Turn(MapObject *mapObj, int dir)
{
    mapObj->prevMovingDir = mapObj->movingDir;
    mapObj->movingDir = dir;
}

int MapObject_GetMovingDir(const MapObject *mapObj)
{
    return mapObj->movingDir;
}

void MapObject_TryFaceAndTurn(MapObject *mapObj, int dir)
{
    MapObject_TryFace(mapObj, dir);
    MapObject_Turn(mapObj, dir);
}

void MapObject_SetDataAt(MapObject *mapObj, int value, int index)
{
    switch (index) {
    case 0:
        mapObj->data[0] = value;
        break;
    case 1:
        mapObj->data[1] = value;
        break;
    case 2:
        mapObj->data[2] = value;
        break;
    default:
        GF_ASSERT(FALSE);
    }
}

int MapObject_GetDataAt(const MapObject *mapObj, int index)
{
    switch (index) {
    case 0:
        return mapObj->data[0];
    case 1:
        return mapObj->data[1];
    case 2:
        return mapObj->data[2];
    }

    GF_ASSERT(FALSE);
    return FALSE;
}

void MapObject_SetMovementRangeX(MapObject *mapObj, int movementRangeX)
{
    mapObj->movementRangeX = movementRangeX;
}

int MapObject_GetMovementRangeX(const MapObject *mapObj)
{
    return mapObj->movementRangeX;
}

void MapObject_SetMovementRangeZ(MapObject *mapObj, int movementRangeZ)
{
    mapObj->movementRangeZ = movementRangeZ;
}

int MapObject_GetMovementRangeZ(const MapObject *mapObj)
{
    return mapObj->movementRangeZ;
}

void MapObject_SetUnkA0(MapObject *mapObj, u32 param1)
{
    mapObj->unk_A0 = param1;
}

u32 MapObject_GetUnkA0(const MapObject *mapObj)
{
    return mapObj->unk_A0;
}

void MapObject_SetTask(MapObject *mapObj, SysTask *task)
{
    mapObj->task = task;
}

SysTask *MapObject_GetTask(const MapObject *mapObj)
{
    return mapObj->task;
}

void MapObject_DeleteTask(const MapObject *mapObj)
{
    SysTask_Done(MapObject_GetTask(mapObj));
}

void MapObject_SetMapObjectManager(MapObject *mapObj, const MapObjectManager *mapObjMan)
{
    mapObj->mapObjMan = mapObjMan;
}

const MapObjectManager *MapObject_MapObjectManager(const MapObject *mapObj)
{
    return mapObj->mapObjMan;
}

static MapObjectManager *MapObject_GetManagerMutable(const MapObject *mapObj)
{
    return MapObjectMan_Deconst(mapObj->mapObjMan);
}

void *MapObject_InitUnkD8(MapObject *mapObj, int size)
{
    void *v0;

    GF_ASSERT(size <= 16);

    v0 = MapObject_GetUnkD8(mapObj);
    memset(v0, 0, size);

    return v0;
}

void *MapObject_GetUnkD8(MapObject *mapObj)
{
    return mapObj->unk_D8;
}

void *MapObject_InitUnkE8(MapObject *mapObj, int size)
{
    u8 *v0;

    GF_ASSERT(size <= 16);

    v0 = MapObject_GetUnkE8(mapObj);
    memset(v0, 0, size);

    return v0;
}

void *MapObject_GetUnkE8(MapObject *mapObj)
{
    return mapObj->unk_E8;
}

void *MapObject_InitMovementData(MapObject *mapObj, int size)
{
    GF_ASSERT(size <= 16);

    void *movementData = MapObject_GetMovementData(mapObj);
    memset(movementData, 0, size);

    return movementData;
}

void *MapObject_GetMovementData(MapObject *mapObj)
{
    return mapObj->movementData;
}

void *MapObject_InitUnk108(MapObject *mapObj, int size)
{
    u8 *v0;

    GF_ASSERT(size <= 32);

    v0 = MapObject_GetUnk108(mapObj);
    memset(v0, 0, size);

    return v0;
}

void *MapObject_GetUnk108(MapObject *mapObj)
{
    return mapObj->unk_108;
}

void MapObject_SetMovementInitCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C param1)
{
    mapObj->movementInit = param1;
}

void MapObject_CallMovementInit(MapObject *mapObj)
{
    mapObj->movementInit(mapObj);
}

void MapObject_SetMovementUpdateCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C_1 param1)
{
    mapObj->movementUpdate = param1;
}

void MapObject_CallMovementUpdate(MapObject *mapObj)
{
    mapObj->movementUpdate(mapObj);
}

void MapObject_SetMovementFreeCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C_2 param1)
{
    mapObj->movementFree = param1;
}

void MapObject_CallMovementFree(MapObject *mapObj)
{
    mapObj->movementFree(mapObj);
}

void MapObject_CallMovementLoad(MapObject *mapObj)
{
    const UnkStruct_020EDF0C *v0 = MovementType_GetCallbackStruct(MapObject_GetMovementType(mapObj));
    v0->unk_10(mapObj);
}

void MapObject_SetRendererInitCallback(MapObject *mapObj, MapObjectRendererInitFunc param1)
{
    mapObj->rendererInit = param1;
}

void MapObject_CallRendererInit(MapObject *mapObj)
{
    mapObj->rendererInit(mapObj);
}

void MapObject_SetRendererDrawCallback(MapObject *mapObj, MapObjectRendererDrawFunc param1)
{
    mapObj->rendererDraw = param1;
}

void MapObject_CallRendererDraw(MapObject *mapObj)
{
    mapObj->rendererDraw(mapObj);
}

void MapObject_SetRendererFreeCallback(MapObject *mapObj, MapObjectRendererFreeFunc param1)
{
    mapObj->rendererFree = param1;
}

void MapObject_CallRendererFree(MapObject *mapObj)
{
    mapObj->rendererFree(mapObj);
}

void MapObject_SetRendererUnloadCallback(MapObject *mapObj, MapObjectRendererUnloadFunc param1)
{
    mapObj->rendererUnload = param1;
}

void MapObject_CallRendererUnload(MapObject *mapObj)
{
    mapObj->rendererUnload(mapObj);
}

void MapObject_SetRendererLoadCallback(MapObject *mapObj, MapObjectRendererLoadFunc param1)
{
    mapObj->rendererLoad = param1;
}

void MapObject_CallRendererLoad(MapObject *mapObj)
{
    mapObj->rendererLoad(mapObj);
}

void MapObject_SetMovementAction(MapObject *mapObj, enum MovementAction movementAction)
{
    mapObj->movementAction = movementAction;
}

enum MovementAction MapObject_GetMovementAction(const MapObject *mapObj)
{
    return mapObj->movementAction;
}

void MapObject_SetMovementStep(MapObject *mapObj, int movementStep)
{
    mapObj->movementStep = movementStep;
}

void MapObject_AdvanceMovementStep(MapObject *mapObj)
{
    mapObj->movementStep++;
}

int MapObject_GetMovementStep(const MapObject *mapObj)
{
    return mapObj->movementStep;
}

void MapObject_SetCurrTileBehavior(MapObject *mapObj, u32 tileBehavior)
{
    mapObj->currTileBehavior = tileBehavior;
}

u32 MapObject_GetCurrTileBehavior(const MapObject *mapObj)
{
    return mapObj->currTileBehavior;
}

void MapObject_SetPrevTileBehavior(MapObject *mapObj, u32 tileBehavior)
{
    mapObj->prevTileBehavior = tileBehavior;
}

u32 MapObject_GetPrevTileBehavior(const MapObject *mapObj)
{
    return mapObj->prevTileBehavior;
}

FieldSystem *MapObject_FieldSystem(const MapObject *mapObj)
{
    MapObjectManager *mapObjMan = MapObject_GetManagerMutable(mapObj);
    return MapObjectMan_FieldSystem(mapObjMan);
}

int MapObject_GetTaskBasePriority(const MapObject *mapObj)
{
    return MapObjectMan_GetTaskBasePriority(MapObject_MapObjectManager(mapObj));
}

int MapObject_GetNoScriptFlag(const MapObject *mapObj)
{
    GF_ASSERT(MapObject_CheckStatus25(mapObj) == TRUE);
    return MapObject_GetFlag(mapObj);
}

void MapObjectMan_StopAllMovement(MapObjectManager *mapObjMan)
{
    MapObjectMan_SetStatusFlagOn(mapObjMan, MAP_OBJ_STATUS_1 | MAP_OBJ_STATUS_START_MOVEMENT);
}

void MapObjectMan_ResumeAllMovement(MapObjectManager *mapObjMan)
{
    MapObjectMan_SetStatusFlagOff(mapObjMan, MAP_OBJ_STATUS_1 | MAP_OBJ_STATUS_START_MOVEMENT);
}

void MapObjectMan_PauseAllMovement(MapObjectManager *mapObjMan)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_IsActive(mapObj)) {
            MapObject_SetPauseMovementOn(mapObj);
        }

        mapObj++;
        maxObjects--;
    } while (maxObjects);
}

void MapObjectMan_UnpauseAllMovement(MapObjectManager *mapObjMan)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_IsActive(mapObj)) {
            MapObject_SetPauseMovementOff(mapObj);
        }

        mapObj++;
        maxObjects--;
    } while (maxObjects);
}

int MapObjectMan_IsDrawInitialized(const MapObjectManager *mapObjMan)
{
    if (MapObjectMan_CheckStatus(mapObjMan, MAP_OBJ_STATUS_0)) {
        return TRUE;
    }

    return FALSE;
}

u32 MapObject_CheckManagerStatus(const MapObject *mapObj, u32 flag)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    return MapObjectMan_CheckStatus(mapObjMan, flag);
}

void MapObjectMan_SetEndMovement(MapObjectManager *mapObjMan, int param1)
{
    if (param1 == FALSE) {
        MapObjectMan_SetStatusFlagOn(mapObjMan, MAP_OBJ_STATUS_END_MOVEMENT);
    } else {
        MapObjectMan_SetStatusFlagOff(mapObjMan, MAP_OBJ_STATUS_END_MOVEMENT);
    }
}

int MapObjectMan_IsMovementRunning(const MapObjectManager *mapObjMan)
{
    if (MapObjectMan_CheckStatus(mapObjMan, MAP_OBJ_STATUS_END_MOVEMENT)) {
        return FALSE;
    }

    return TRUE;
}

int MapObject_IsActive(const MapObject *mapObj)
{
    return MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_0);
}

void MapObject_SetStatus1(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_1);
}

void MapObject_ClearStatus1(MapObject *mapObj)
{
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_1);
}

int MapObject_IsMoving(const MapObject *mapObj)
{
    return MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_1);
}

void MapObject_SetStartMovement(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_START_MOVEMENT);
}

void MapObject_SetEndMovementOff(MapObject *mapObj)
{
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_END_MOVEMENT);
}

void MapObject_SetStatus14(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_14);
}

int MapObject_CheckStatus14(const MapObject *mapObj)
{
    return MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_14);
}

int MapObject_IsHidden(const MapObject *mapObj)
{
    return MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_HIDE);
}

void MapObject_SetHidden(MapObject *mapObj, int hidden)
{
    if (hidden == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_HIDE);
    }
}

// Note: the argument is inverted -- TRUE clears MAP_OBJ_STATUS_18.
void MapObject_SetStatus18(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_18);
    } else {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_18);
    }
}

// Note: returns TRUE when MAP_OBJ_STATUS_19 is *clear*.
int MapObject_CheckStatus19(MapObject *mapObj)
{
    if (MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_19) == TRUE) {
        return FALSE;
    }

    return TRUE;
}

void MapObject_SetStatus19(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_19);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_19);
    }
}

void MapObject_SetPauseMovementOn(MapObject *mapObj)
{
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_PAUSE_MOVEMENT);
}

void MapObject_SetPauseMovementOff(MapObject *mapObj)
{
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_PAUSE_MOVEMENT);
}

int MapObject_IsMovementPaused(const MapObject *mapObj)
{
    if (MapObject_CheckStatusFlag(mapObj, MAP_OBJ_STATUS_PAUSE_MOVEMENT) == TRUE) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_IsDrawReady(const MapObject *mapObj)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    if (MapObjectMan_IsDrawInitialized(mapObjMan) == FALSE) {
        return FALSE;
    }

    if (!MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_14)) {
        return FALSE;
    }

    return TRUE;
}

void MapObject_SetHeightCalculationDisabled(MapObject *mapObj, BOOL heightCalculationDisabled)
{
    if (heightCalculationDisabled == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_HEIGHT_CALCULATION_DISABLED);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_HEIGHT_CALCULATION_DISABLED);
    }
}

int MapObject_IsHeightCalculationDisabled(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_HEIGHT_CALCULATION_DISABLED)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetFlagIsPersistent(MapObject *mapObj, BOOL flag)
{
    if (flag == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_PERSISTENT);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_PERSISTENT);
    }
}

void MapObject_SetStatus25(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_25);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_25);
    }
}

int MapObject_CheckStatus25(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_25)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetStatus26(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_26);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_26);
    }
}

int MapObject_CheckStatus26(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_26)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetFlagDoNotSinkIntoTerrain(MapObject *mapObj, BOOL flag)
{
    if (flag == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_DO_NOT_SINK_INTO_TERRAIN);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_DO_NOT_SINK_INTO_TERRAIN);
    }
}

int MapObject_CheckFlagDoNotSinkIntoTerrain(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_DO_NOT_SINK_INTO_TERRAIN)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetElevatedBridgeStatus(MapObject *mapObj, BOOL isOnElevatedBridge)
{
    if (isOnElevatedBridge == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_ON_ELEVATED_BRIDGE);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_ON_ELEVATED_BRIDGE);
    }
}

int MapObject_IsStatusOnElevatedBridge(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_ON_ELEVATED_BRIDGE)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetStatus24(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_24);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_24);
    }
}

int MapObject_CheckStatus24(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_24)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_CheckStatus4(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_4)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetDynamicHeightCalculationEnabled(MapObject *mapObj, int enabled)
{
    if (enabled == TRUE) {
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_DYNAMIC_HEIGHT_CALCULATION_ENABLED);
    } else {
        MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_DYNAMIC_HEIGHT_CALCULATION_ENABLED);
    }
}

int MapObject_IsDynamicHeightCalculationEnabled(const MapObject *mapObj)
{
    if (MapObject_CheckStatus(mapObj, MAP_OBJ_DYNAMIC_HEIGHT_CALCULATION_ENABLED)) {
        return TRUE;
    }

    return FALSE;
}

void MapObject_SetFlags2Bit2(MapObject *mapObj, int param1)
{
    if (param1 == TRUE) {
        MapObject_SetFlags2FlagOn(mapObj, 1 << 2);
    } else {
        MapObject_SetFlags2FlagOff(mapObj, 1 << 2);
    }
}

int MapObject_CheckFlags2Bit2(const MapObject *mapObj)
{
    if (MapObject_CheckFlags2(mapObj, 1 << 2)) {
        return TRUE;
    }

    return FALSE;
}

int MapObject_GetXInitial(const MapObject *mapObj)
{
    return mapObj->xInitial;
}

void MapObject_SetXInitial(MapObject *mapObj, int x)
{
    mapObj->xInitial = x;
}

int MapObject_GetYInitial(const MapObject *mapObj)
{
    return mapObj->yInitial;
}

void MapObject_SetYInitial(MapObject *mapObj, int y)
{
    mapObj->yInitial = y;
}

int MapObject_GetZInitial(const MapObject *mapObj)
{
    return mapObj->zInitial;
}

void MapObject_SetZInitial(MapObject *mapObj, int z)
{
    mapObj->zInitial = z;
}

int MapObject_GetXPrev(const MapObject *mapObj)
{
    return mapObj->xPrev;
}

void MapObject_SetXPrev(MapObject *mapObj, int x)
{
    mapObj->xPrev = x;
}

int MapObject_GetYPrev(const MapObject *mapObj)
{
    return mapObj->yPrev;
}

void MapObject_SetYPrev(MapObject *mapObj, int y)
{
    mapObj->yPrev = y;
}

int MapObject_GetZPrev(const MapObject *mapObj)
{
    return mapObj->zPrev;
}

void MapObject_SetZPrev(MapObject *mapObj, int z)
{
    mapObj->zPrev = z;
}

int MapObject_GetX(const MapObject *mapObj)
{
    return mapObj->x;
}

void MapObject_SetX(MapObject *mapObj, int x)
{
    mapObj->x = x;
}

void MapObject_AddX(MapObject *mapObj, int dx)
{
    mapObj->x += dx;
}

int MapObject_GetY(const MapObject *mapObj)
{
    return mapObj->y;
}

void MapObject_SetY(MapObject *mapObj, int y)
{
    mapObj->y = y;
}

void MapObject_AddY(MapObject *mapObj, int dy)
{
    mapObj->y += dy;
}

int MapObject_GetZ(const MapObject *mapObj)
{
    return mapObj->z;
}

void MapObject_SetZ(MapObject *mapObj, int z)
{
    mapObj->z = z;
}

void MapObject_AddZ(MapObject *mapObj, int dz)
{
    mapObj->z += dz;
}

void MapObject_GetPosPtr(const MapObject *mapObj, VecFx32 *pos)
{
    *pos = mapObj->pos;
}

void MapObject_SetPos(MapObject *mapObj, const VecFx32 *pos)
{
    mapObj->pos = *pos;
}

const VecFx32 *MapObject_GetPos(const MapObject *mapObj)
{
    return &mapObj->pos;
}

fx32 MapObject_GetPosY(const MapObject *mapObj)
{
    return mapObj->pos.y;
}

void MapObject_GetSpriteJumpOffset(const MapObject *mapObj, VecFx32 *vec)
{
    *vec = mapObj->spriteJumpOffset;
}

void MapObject_SetSpriteJumpOffset(MapObject *mapObj, const VecFx32 *vec)
{
    mapObj->spriteJumpOffset = *vec;
}

VecFx32 *MapObject_GetSpriteJumpOffset1(MapObject *mapObj)
{
    return &mapObj->spriteJumpOffset;
}

void MapObject_GetSpritePosOffset(const MapObject *mapObj, VecFx32 *vec)
{
    *vec = mapObj->spritePosOffset;
}

void MapObject_SetSpritePosOffset(MapObject *mapObj, const VecFx32 *vec)
{
    mapObj->spritePosOffset = *vec;
}

void MapObject_GetSpriteTerrainOffset(const MapObject *mapObj, VecFx32 *spriteOffset)
{
    *spriteOffset = mapObj->spriteTerrainOffset;
}

void MapObject_SetSpriteTerrainOffset(MapObject *mapObj, const VecFx32 *spriteOffset)
{
    mapObj->spriteTerrainOffset = *spriteOffset;
}

// Converts the object's fixed-point world Y position back to an integer map
// height (the Y axis uses a different scale from X/Z).
int MapObject_GetYFromPos(const MapObject *mapObj)
{
    fx32 v0 = MapObject_GetPosY(mapObj);
    int v1 = ((v0) >> 3) / FX32_ONE;

    return v1;
}

void ObjectEvent_SetLocalID(ObjectEvent *objectEvent, int localID)
{
    objectEvent->localID = localID;
}

int ObjectEvent_GetLocalID(const ObjectEvent *objectEvent)
{
    return objectEvent->localID;
}

void ObjectEvent_SetGraphicsID(ObjectEvent *objectEvent, int graphicsID)
{
    objectEvent->graphicsID = graphicsID;
}

int ObjectEvent_GetGraphicsID(const ObjectEvent *objectEvent)
{
    return objectEvent->graphicsID;
}

void ObjectEvent_SetMovementType(ObjectEvent *objectEvent, int movementType)
{
    objectEvent->movementType = movementType;
}

int ObjectEvent_GetMovementType(const ObjectEvent *objectEvent)
{
    return objectEvent->movementType;
}

void ObjectEvent_SetTrainerType(ObjectEvent *objectEvent, int trainerType)
{
    objectEvent->trainerType = trainerType;
}

int ObjectEvent_GetTrainerType(const ObjectEvent *objectEvent)
{
    return objectEvent->trainerType;
}

void ObjectEvent_SetHiddenFlag(ObjectEvent *objectEvent, int flag)
{
    objectEvent->hiddenFlag = flag;
}

int ObjectEvent_GetHiddenFlag(const ObjectEvent *objectEvent)
{
    return objectEvent->hiddenFlag;
}

void ObjectEvent_SetScript(ObjectEvent *objectEvent, int script)
{
    objectEvent->script = script;
}

int ObjectEvent_GetScript(const ObjectEvent *objectEvent)
{
    return objectEvent->script;
}

void ObjectEvent_SetInitialDir(ObjectEvent *objectEvent, int initialDir)
{
    objectEvent->dir = initialDir;
}

int ObjectEvent_GetInitialDir(const ObjectEvent *objectEvent)
{
    return objectEvent->dir;
}

void ObjectEvent_SetDataAt(ObjectEvent *objectEvent, int value, int index)
{
    switch (index) {
    case 0:
        objectEvent->data[0] = value;
        break;
    case 1:
        objectEvent->data[1] = value;
        break;
    case 2:
        objectEvent->data[2] = value;
        break;
    default:
        GF_ASSERT(FALSE);
    }
}

int ObjectEvent_GetDataAt(const ObjectEvent *objectEvent, int index)
{
    switch (index) {
    case 0:
        return objectEvent->data[0];
    case 1:
        return objectEvent->data[1];
    case 2:
        return objectEvent->data[2];
    }

    GF_ASSERT(FALSE);
    return FALSE;
}

void ObjectEvent_SetMovementRangeX(ObjectEvent *objectEvent, int movementRangeX)
{
    objectEvent->movementRangeX = movementRangeX;
}

int ObjectEvent_GetMovementRangeX(const ObjectEvent *objectEvent)
{
    return objectEvent->movementRangeX;
}

void ObjectEvent_SetMovementRangeZ(ObjectEvent *objectEvent, int movementRangeZ)
{
    objectEvent->movementRangeZ = movementRangeZ;
}

int ObjectEvent_GetMovementRangeZ(const ObjectEvent *objectEvent)
{
    return objectEvent->movementRangeZ;
}

void ObjectEvent_SetX(ObjectEvent *objectEvent, int x)
{
    objectEvent->x = x;
}

int ObjectEvent_GetX(const ObjectEvent *objectEvent)
{
    return objectEvent->x;
}

void ObjectEvent_SetY(ObjectEvent *objectEvent, int y)
{
    objectEvent->y = y;
}

int ObjectEvent_GetY(const ObjectEvent *objectEvent)
{
    return objectEvent->y;
}

void ObjectEvent_SetZ(ObjectEvent *objectEvent, int z)
{
    objectEvent->z = z;
}

int ObjectEvent_GetZ(const ObjectEvent *objectEvent)
{
    return objectEvent->z;
}

static const ObjectEvent *ObjectEvent_FindByLocalID(int localID, int objEventCount, const ObjectEvent *objectEvent)
{
    int i = 0;

    do {
        if (ObjectEvent_HasNoScript(&objectEvent[i]) == FALSE && ObjectEvent_GetLocalID(&objectEvent[i]) == localID) {
            return &objectEvent[i];
        }

        i++;
    } while (i < objEventCount);

    return NULL;
}

static int ObjectEvent_HasNoScript(const ObjectEvent *objectEvent)
{
    u16 script = (u16)ObjectEvent_GetScript(objectEvent);

    if (script == 0xffff) {
        return TRUE;
    }

    return FALSE;
}

static int ObjectEvent_GetHiddenFlagNoScript(const ObjectEvent *objectEvent)
{
    GF_ASSERT(ObjectEvent_HasNoScript(objectEvent) == TRUE);
    return ObjectEvent_GetHiddenFlag(objectEvent);
}

static const UnkStruct_020EDF0C *MovementType_GetCallbackStruct(u32 param0)
{
    GF_ASSERT(param0 < 0x44);
    return Unk_020EE3A8[param0];
}

static UnkFuncPtr_020EDF0C MovementType_GetInitCallback(const UnkStruct_020EDF0C *param0)
{
    return param0->unk_04;
}

static UnkFuncPtr_020EDF0C_1 MovementType_GetUpdateCallback(const UnkStruct_020EDF0C *param0)
{
    return param0->unk_08;
}

static UnkFuncPtr_020EDF0C_2 MovementType_GetFreeCallback(const UnkStruct_020EDF0C *param0)
{
    return param0->unk_0C;
}

static MapObjectRendererInitFunc Renderer_GetInitCallback(const MapObjectRendererCallbacks *param0)
{
    return param0->init;
}

static MapObjectRendererDrawFunc Renderer_GetDrawCallback(const MapObjectRendererCallbacks *param0)
{
    return param0->draw;
}

static MapObjectRendererFreeFunc Renderer_GetFreeCallback(const MapObjectRendererCallbacks *param0)
{
    return param0->free;
}

static MapObjectRendererUnloadFunc Renderer_GetUnloadCallback(const MapObjectRendererCallbacks *param0)
{
    return param0->unload;
}

static MapObjectRendererLoadFunc Renderer_GetLoadCallback(const MapObjectRendererCallbacks *param0)
{
    return param0->load;
}

static const MapObjectRendererCallbacks *Renderer_GetByGraphicsID(u32 param0)
{
    const ObjectEventGfxRendererEntry *v0 = gObjectEventGfxRenderersTable;

    do {
        if (v0->graphicsID == param0) {
            return v0->renderer;
        }

        v0++;
    } while (v0->graphicsID != 0xffff);

    GF_ASSERT(FALSE);
    return NULL;
}

// Returns the first active object occupying tile (x, z). If `param3` is
// non-zero, an object's previous tile is also accepted (used while it is
// mid-step between tiles).
MapObject *MapObjectMan_FindObjectAtCoords(const MapObjectManager *mapObjMan, int x, int z, int param3)
{
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);
    MapObject *mapObj = MapObjectMan_GetMapObject(mapObjMan);

    do {
        if (MapObject_CheckStatus(mapObj, MAP_OBJ_STATUS_0)) {
            if (param3 && MapObject_GetXPrev(mapObj) == x && MapObject_GetZPrev(mapObj) == z) {
                return mapObj;
            }

            if (MapObject_GetX(mapObj) == x && MapObject_GetZ(mapObj) == z) {
                return mapObj;
            }
        }

        mapObj++;
        maxObjects--;
    } while (maxObjects);

    return NULL;
}

void MapObject_SetPosDirFromVec(MapObject *mapObj, const VecFx32 *pos, int dir)
{
    int x, y, z;

    x = ((pos->x) >> 4) / FX32_ONE;
    MapObject_SetX(mapObj, x);

    y = ((pos->y) >> 3) / FX32_ONE;
    MapObject_SetY(mapObj, y);

    z = ((pos->z) >> 4) / FX32_ONE;
    MapObject_SetZ(mapObj, z);

    MapObject_SetPos(mapObj, pos);
    MapObject_UpdateCoords(mapObj);

    MapObject_Face(mapObj, dir);

    sub_020656DC(mapObj);
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_START_MOVEMENT);
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_1 | MAP_OBJ_STATUS_END_MOVEMENT);
}

void MapObject_SetPosDirFromCoords(MapObject *mapObj, int x, int y, int z, int dir)
{
    VecFx32 pos;

    pos.x = ((x << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);
    MapObject_SetX(mapObj, x);

    pos.y = ((y << 3) * FX32_ONE) + 0;
    MapObject_SetY(mapObj, y);

    pos.z = ((z << 4) * FX32_ONE) + ((16 * FX32_ONE) >> 1);
    MapObject_SetZ(mapObj, z);

    MapObject_SetPos(mapObj, &pos);
    MapObject_UpdateCoords(mapObj);

    MapObject_Face(mapObj, dir);

    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_START_MOVEMENT);
    MapObject_SetStatusFlagOff(mapObj, MAP_OBJ_STATUS_1 | MAP_OBJ_STATUS_END_MOVEMENT);

    sub_020656DC(mapObj);
}

void MapObject_SwitchMovementType(MapObject *mapObj, u32 movementType)
{
    MapObject_CallMovementFree(mapObj);
    MapObject_SetMovementType(mapObj, movementType);
    MapObject_LoadMovementCallbacks(mapObj);
    MapObject_InitMove(mapObj);
}

void MapObject_SetLocalIDAndStartMovement(MapObject *mapObj, int localID)
{
    MapObject_SetLocalID(mapObj, localID);

    MapObject_SetStartMovement(mapObj);
    MapObject_ResetShadowFlags(mapObj);
}

void MapObjectMovement_NoOp1(MapObject *mapObj)
{
    return;
}

void MapObjectMovement_NoOp2(MapObject *mapObj)
{
    return;
}

void MapObjectMovement_NoOp3(MapObject *mapObj)
{
    return;
}

void MapObjectMovement_NoOp4(MapObject *mapObj)
{
    return;
}

void MapObjectRenderer_NoOp1(MapObject *mapObj)
{
    return;
}

void MapObjectRenderer_NoOp2(MapObject *mapObj)
{
    return;
}

void MapObjectRenderer_NoOp3(MapObject *mapObj)
{
    return;
}

void MapObjectRenderer_NoOp4(MapObject *mapObj)
{
    return;
}
