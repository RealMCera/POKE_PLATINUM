#ifndef POKEPLATINUM_MAP_OBJECT_H
#define POKEPLATINUM_MAP_OBJECT_H

#include <nitro/fx/fx.h>

#include "constants/field/map.h"
#include "constants/map_object.h"
#include "generated/map_headers.h"
#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"
#include "struct_decls/map_object_manager.h"

#include "field/field_system_decl.h"
#include "functypes/funcptr_020EDF0C.h"
#include "functypes/funcptr_020EDF0C_1.h"
#include "functypes/funcptr_020EDF0C_2.h"
#include "overlay005/struct_ov5_021ED0A4.h"

#include "map_header_data.h"
#include "narc.h"
#include "sys_task_manager.h"

#define MAP_OBJECT_COORD_CENTER_TO_FX32(coord) (((coord) << 4) * FX32_ONE) + (MAP_OBJECT_TILE_SIZE >> 1)
#define MAP_OBJECT_COORD_EDGE_TO_FX32(coord)   (((coord) << 4) * FX32_ONE)

typedef void (*MapObjectRendererInitFunc)(MapObject *);
typedef void (*MapObjectRendererDrawFunc)(MapObject *);
typedef void (*MapObjectRendererFreeFunc)(MapObject *);
typedef void (*MapObjectRendererUnloadFunc)(MapObject *);
typedef void (*MapObjectRendererLoadFunc)(MapObject *);

// Per-graphics-ID renderer callbacks. A map object copies the entry selected
// from gObjectEventGfxRenderersTable and invokes these at the corresponding
// points in its lifecycle.
typedef struct MapObjectRendererCallbacks {
    MapObjectRendererInitFunc init;
    MapObjectRendererDrawFunc draw;
    MapObjectRendererFreeFunc free;
    MapObjectRendererUnloadFunc unload;
    MapObjectRendererLoadFunc load;
} MapObjectRendererCallbacks;

// Persisted form of a MapObject, written to and restored from the save file.
typedef struct MapObjectSave {
    u32 status;
    u32 flags2;
    u8 localID;
    u8 movementType;
    s8 movementRangeX;
    s8 movementRangeZ;
    s8 initialDir;
    s8 facingDir;
    s8 movingDir;
    u8 padding_0F;
    u16 mapID;
    u16 graphicsID;
    u16 trainerType;
    u16 flag;
    u16 script;
    s16 param[3];
    s16 xInitial;
    s16 yInitial;
    s16 zInitial;
    s16 x;
    s16 y;
    s16 z;
    fx32 vecY;
    u8 unk_30[16];
    u8 unk_40[16];
} MapObjectSave;

MapObjectManager *MapObjectMan_New(FieldSystem *fieldSystem, int maxObjs, int taskBasePriority);
void MapObjectMan_Delete(MapObjectManager *mapObjMan);
void MapObjectMan_UpdateObjectsForMapChange(MapObjectManager *mapObjMan, enum MapHeaderID oldMapHeaderID, enum MapHeaderID newMapHeaderID, int objEventCount, const ObjectEvent *objectEvent);
MapObject *MapObjectMan_AddMapObjectFromHeader(const MapObjectManager *mapObjMan, const ObjectEvent *objectEvent, enum MapHeaderID mapHeaderID);
MapObject *MapObjectMan_AddMapObject(const MapObjectManager *mapObjMan, int x, int z, int initialDir, int graphicsID, int movementType, enum MapHeaderID mapHeaderID);
MapObject *MapObjectMan_AddMapObjectFromLocalID(const MapObjectManager *mapObjMan, int param1, int objEventCount, enum MapHeaderID mapHeaderID, const ObjectEvent *objectEvent);
void MapObject_SetGraphicsIDAndRefresh(MapObject *mapObj, int graphicsID);
void MapObject_ChangeGraphics(MapObject *mapObj, int param1);
void MapObject_Delete(MapObject *mapObj);
void MapObject_SetFlagAndDeleteObject(MapObject *mapObj);
void MapObject_ClearGraphics(MapObject *mapObj);
void MapObjectMan_DeleteAll(MapObjectManager *mapObjMan);
void MapObjectMan_UnloadAllRenderers(MapObjectManager *mapObjMan);
void MapObjectMan_LoadAllRenderers(MapObjectManager *mapObjMan);
void MapObjectMan_SaveAll(FieldSystem *fieldSystem, const MapObjectManager *mapObjMan, MapObjectSave *mapObjSave, int param3);
void MapObjectMan_LoadAllObjects(const MapObjectManager *mapObjMan, MapObjectSave *mapObjSave, int size);
void MapObjectMan_AddMapObjectsFromEvents(const MapObjectManager *mapObjMan, enum MapHeaderID mapHeaderID, u32 numObjectEvents, const ObjectEvent *objectEvent);
MapObject *MapObjMan_LocalMapObjByIndex(const MapObjectManager *mapObjMan, int index);
MapObject *MapObjMan_GetLocalMapObjByMovementType(const MapObjectManager *mapObjMan, int movementType);
BOOL MapObjectMan_FindObjectWithStatus(const MapObjectManager *mapObjMan, MapObject **mapObj, int *startIdx, u32 status);
int MapObject_HasNoScript(const MapObject *mapObj);
int MapObject_CalculateTaskPriority(const MapObject *mapObj, int priority);
int MapObject_MatchesLocalIDAndMap(const MapObject *mapObj, int param1, enum MapHeaderID param2);
int MapObject_MatchesGraphicsAndLocalID(const MapObject *mapObj, int param1, int param2, enum MapHeaderID param3);
void MapObjectMan_SetMaxObjects(MapObjectManager *mapObjMan, int maxObjs);
int MapObjectMan_GetMaxObjects(const MapObjectManager *mapObjMan);
void MapObjectMan_SetStatusFlagOn(MapObjectManager *mapObjMan, u32 flag);
void MapObjectMan_SetStatusFlagOff(MapObjectManager *mapObjMan, u32 flag);
u32 MapObjectMan_CheckStatus(const MapObjectManager *mapObjMan, u32 flag);
void MapObjectMan_SetTaskBasePriority(MapObjectManager *mapObjMan, int basePriority);
int MapObjectMan_GetTaskBasePriority(const MapObjectManager *mapObjMan);
UnkStruct_ov5_021ED0A4 *MapObjectMan_GetDrawManager(const MapObjectManager *mapObjMan);
void MapObjectMan_SetMapObject(MapObjectManager *mapObjMan, MapObject *mapObj);
const MapObject *MapObjectMan_GetMapObjectConst(const MapObjectManager *mapObjMan);
MapObject *MapObjectMan_GetMapObject(const MapObjectManager *mapObjMan);
void MapObject_AdvancePointer(const MapObject **mapObj);
void MapObjectMan_SetFieldSystem(MapObjectManager *mapObjMan, FieldSystem *fieldSystem);
FieldSystem *MapObjectMan_FieldSystem(const MapObjectManager *mapObjMan);
void MapObjectMan_SetNARC(MapObjectManager *mapObjMan, NARC *narc);
NARC *MapObjectMan_GetNARC(const MapObjectManager *mapObjMan);
void MapObject_SetStatus(MapObject *mapObj, u32 status);
u32 MapObject_GetStatus(const MapObject *mapObj);
void MapObject_SetStatusFlagOn(MapObject *mapObj, u32 flag);
void MapObject_SetStatusFlagOff(MapObject *mapObj, u32 flag);
u32 MapObject_CheckStatus(const MapObject *mapObj, u32 flag);
BOOL MapObject_CheckStatusFlag(const MapObject *mapObj, u32 flag);
void MapObject_SetFlags2(MapObject *mapObj, u32 param1);
u32 MapObject_GetFlags2(const MapObject *mapObj);
void MapObject_SetFlags2FlagOn(MapObject *mapObj, u32 param1);
void MapObject_SetFlags2FlagOff(MapObject *mapObj, u32 param1);
u32 MapObject_CheckFlags2(const MapObject *mapObj, u32 param1);
void MapObject_SetLocalID(MapObject *mapObj, u32 localID);
u32 MapObject_GetLocalID(const MapObject *mapObj);
void MapObject_SetMapHeaderID(MapObject *mapObj, enum MapHeaderID mapHeaderID);
enum MapHeaderID MapObject_GetMapHeaderID(const MapObject *mapObj);
void MapObject_SetGraphicsID(MapObject *mapObj, u32 graphicsID);
u32 MapObject_GetGraphicsID(const MapObject *mapObj);
u32 MapObject_GetEffectiveGraphicsID(const MapObject *mapObj);
void MapObject_SetMovementType(MapObject *mapObj, u32 movementType);
u32 MapObject_GetMovementType(const MapObject *mapObj);
void MapObject_SetTrainerType(MapObject *mapObj, u32 trainerType);
u32 MapObject_GetTrainerType(const MapObject *mapObj);
void MapObject_SetFlag(MapObject *mapObj, u32 flag);
u32 MapObject_GetFlag(const MapObject *mapObj);
void MapObject_SetScript(MapObject *mapObj, u32 script);
u32 MapObject_GetScript(const MapObject *mapObj);
void MapObject_SetInitialDir(MapObject *mapObj, int initialDir);
u32 MapObject_GetInitialDir(const MapObject *mapObj);
void MapObject_Face(MapObject *mapObj, int dir);
void MapObject_TryFace(MapObject *mapObj, int dir);
int MapObject_GetFacingDir(const MapObject *mapObj);
int MapObject_GetPrevFacingDir(const MapObject *mapObj);
void MapObject_Turn(MapObject *mapObj, int dir);
int MapObject_GetMovingDir(const MapObject *mapObj);
void MapObject_TryFaceAndTurn(MapObject *mapObj, int dir);
void MapObject_SetDataAt(MapObject *mapObj, int value, int index);
int MapObject_GetDataAt(const MapObject *mapObj, int index);
void MapObject_SetMovementRangeX(MapObject *mapObj, int movementRangeX);
int MapObject_GetMovementRangeX(const MapObject *mapObj);
void MapObject_SetMovementRangeZ(MapObject *mapObj, int movementRangeZ);
int MapObject_GetMovementRangeZ(const MapObject *mapObj);
void MapObject_SetUnkA0(MapObject *mapObj, u32 param1);
u32 MapObject_GetUnkA0(const MapObject *mapObj);
void MapObject_SetTask(MapObject *mapObj, SysTask *task);
SysTask *MapObject_GetTask(const MapObject *mapObj);
void MapObject_DeleteTask(const MapObject *mapObj);
void MapObject_SetMapObjectManager(MapObject *mapObj, const MapObjectManager *mapObjMan);
const MapObjectManager *MapObject_MapObjectManager(const MapObject *mapObj);
void *MapObject_InitUnkD8(MapObject *mapObj, int size);
void *MapObject_GetUnkD8(MapObject *mapObj);
void *MapObject_InitUnkE8(MapObject *mapObj, int size);
void *MapObject_GetUnkE8(MapObject *mapObj);
void *MapObject_InitMovementData(MapObject *mapObj, int size);
void *MapObject_GetMovementData(MapObject *mapObj);
void *MapObject_InitUnk108(MapObject *mapObj, int size);
void *MapObject_GetUnk108(MapObject *mapObj);
void MapObject_SetMovementInitCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C param1);
void MapObject_CallMovementInit(MapObject *mapObj);
void MapObject_SetMovementUpdateCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C_1 param1);
void MapObject_CallMovementUpdate(MapObject *mapObj);
void MapObject_SetMovementFreeCallback(MapObject *mapObj, UnkFuncPtr_020EDF0C_2 param1);
void MapObject_CallMovementFree(MapObject *mapObj);
void MapObject_CallMovementLoad(MapObject *mapObj);
void MapObject_SetRendererInitCallback(MapObject *mapObj, MapObjectRendererInitFunc param1);
void MapObject_CallRendererInit(MapObject *mapObj);
void MapObject_SetRendererDrawCallback(MapObject *mapObj, MapObjectRendererDrawFunc param1);
void MapObject_CallRendererDraw(MapObject *mapObj);
void MapObject_SetRendererFreeCallback(MapObject *mapObj, MapObjectRendererFreeFunc param1);
void MapObject_CallRendererFree(MapObject *mapObj);
void MapObject_SetRendererUnloadCallback(MapObject *mapObj, MapObjectRendererUnloadFunc param1);
void MapObject_CallRendererUnload(MapObject *mapObj);
void MapObject_SetRendererLoadCallback(MapObject *mapObj, MapObjectRendererLoadFunc param1);
void MapObject_CallRendererLoad(MapObject *mapObj);
void MapObject_SetMovementAction(MapObject *mapObj, enum MovementAction movementAction);
enum MovementAction MapObject_GetMovementAction(const MapObject *mapObj);
void MapObject_SetMovementStep(MapObject *mapObj, int movementStep);
void MapObject_AdvanceMovementStep(MapObject *mapObj);
int MapObject_GetMovementStep(const MapObject *mapObj);
void MapObject_SetCurrTileBehavior(MapObject *mapObj, u32 tileBehavior);
u32 MapObject_GetCurrTileBehavior(const MapObject *mapObj);
void MapObject_SetPrevTileBehavior(MapObject *mapObj, u32 tileBehavior);
u32 MapObject_GetPrevTileBehavior(const MapObject *mapObj);
FieldSystem *MapObject_FieldSystem(const MapObject *mapObj);
int MapObject_GetTaskBasePriority(const MapObject *mapObj);
int MapObject_GetNoScriptFlag(const MapObject *mapObj);
void MapObjectMan_StopAllMovement(MapObjectManager *mapObjMan);
void MapObjectMan_ResumeAllMovement(MapObjectManager *mapObjMan);
void MapObjectMan_PauseAllMovement(MapObjectManager *mapObjMan);
void MapObjectMan_UnpauseAllMovement(MapObjectManager *mapObjMan);
int MapObjectMan_IsDrawInitialized(const MapObjectManager *mapObjMan);
u32 MapObject_CheckManagerStatus(const MapObject *mapObj, u32 flag);
void MapObjectMan_SetEndMovement(MapObjectManager *mapObjMan, int param1);
int MapObjectMan_IsMovementRunning(const MapObjectManager *mapObjMan);
int MapObject_IsActive(const MapObject *mapObj);
void MapObject_SetStatus1(MapObject *mapObj);
void MapObject_ClearStatus1(MapObject *mapObj);
int MapObject_IsMoving(const MapObject *mapObj);
void MapObject_SetStartMovement(MapObject *mapObj);
void MapObject_SetEndMovementOff(MapObject *mapObj);
void MapObject_SetStatus14(MapObject *mapObj);
int MapObject_CheckStatus14(const MapObject *mapObj);
int MapObject_IsHidden(const MapObject *mapObj);
void MapObject_SetHidden(MapObject *mapObj, int hidden);
void MapObject_SetStatus18(MapObject *mapObj, int param1);
int MapObject_CheckStatus19(MapObject *mapObj);
void MapObject_SetStatus19(MapObject *mapObj, int param1);
void MapObject_SetPauseMovementOn(MapObject *mapObj);
void MapObject_SetPauseMovementOff(MapObject *mapObj);
int MapObject_IsMovementPaused(const MapObject *mapObj);
int MapObject_IsDrawReady(const MapObject *mapObj);
void MapObject_SetHeightCalculationDisabled(MapObject *mapObj, BOOL heightCalculationDisabled);
int MapObject_IsHeightCalculationDisabled(const MapObject *mapObj);
void MapObject_SetFlagIsPersistent(MapObject *mapObj, BOOL flag);
void MapObject_SetStatus25(MapObject *mapObj, int param1);
int MapObject_CheckStatus25(const MapObject *mapObj);
void MapObject_SetStatus26(MapObject *mapObj, int param1);
int MapObject_CheckStatus26(const MapObject *mapObj);
void MapObject_SetFlagDoNotSinkIntoTerrain(MapObject *mapObj, BOOL flag);
int MapObject_CheckFlagDoNotSinkIntoTerrain(const MapObject *mapObj);
void MapObject_SetElevatedBridgeStatus(MapObject *mapObj, BOOL isOnBridge);
int MapObject_IsStatusOnElevatedBridge(const MapObject *mapObj);
void MapObject_SetStatus24(MapObject *mapObj, int param1);
int MapObject_CheckStatus24(const MapObject *mapObj);
int MapObject_CheckStatus4(const MapObject *mapObj);
void MapObject_SetDynamicHeightCalculationEnabled(MapObject *mapObj, int enabled);
int MapObject_IsDynamicHeightCalculationEnabled(const MapObject *mapObj);
void MapObject_SetFlags2Bit2(MapObject *mapObj, int param1);
int MapObject_CheckFlags2Bit2(const MapObject *mapObj);
int MapObject_GetXInitial(const MapObject *mapObj);
void MapObject_SetXInitial(MapObject *mapObj, int x);
int MapObject_GetYInitial(const MapObject *mapObj);
void MapObject_SetYInitial(MapObject *mapObj, int y);
int MapObject_GetZInitial(const MapObject *mapObj);
void MapObject_SetZInitial(MapObject *mapObj, int z);
int MapObject_GetXPrev(const MapObject *mapObj);
void MapObject_SetXPrev(MapObject *mapObj, int x);
int MapObject_GetYPrev(const MapObject *mapObj);
void MapObject_SetYPrev(MapObject *mapObj, int y);
int MapObject_GetZPrev(const MapObject *mapObj);
void MapObject_SetZPrev(MapObject *mapObj, int z);
int MapObject_GetX(const MapObject *mapObj);
void MapObject_SetX(MapObject *mapObj, int x);
void MapObject_AddX(MapObject *mapObj, int dx);
int MapObject_GetY(const MapObject *mapObj);
void MapObject_SetY(MapObject *mapObj, int y);
void MapObject_AddY(MapObject *mapObj, int dy);
int MapObject_GetZ(const MapObject *mapObj);
void MapObject_SetZ(MapObject *mapObj, int z);
void MapObject_AddZ(MapObject *mapObj, int dz);
void MapObject_GetPosPtr(const MapObject *mapObj, VecFx32 *pos);
void MapObject_SetPos(MapObject *mapObj, const VecFx32 *pos);
const VecFx32 *MapObject_GetPos(const MapObject *mapObj);
fx32 MapObject_GetPosY(const MapObject *mapObj);
void MapObject_GetSpriteJumpOffset(const MapObject *mapObj, VecFx32 *vec);
void MapObject_SetSpriteJumpOffset(MapObject *mapObj, const VecFx32 *vec);
VecFx32 *MapObject_GetSpriteJumpOffset1(MapObject *mapObj);
void MapObject_GetSpritePosOffset(const MapObject *mapObj, VecFx32 *vec);
void MapObject_SetSpritePosOffset(MapObject *mapObj, const VecFx32 *vec);
void MapObject_GetSpriteTerrainOffset(const MapObject *mapObj, VecFx32 *vec);
void MapObject_SetSpriteTerrainOffset(MapObject *mapObj, const VecFx32 *spriteOffset);
int MapObject_GetYFromPos(const MapObject *mapObj);
void ObjectEvent_SetLocalID(ObjectEvent *objectEvent, int localID);
int ObjectEvent_GetLocalID(const ObjectEvent *objectEvent);
void ObjectEvent_SetGraphicsID(ObjectEvent *objectEvent, int graphicsID);
int ObjectEvent_GetGraphicsID(const ObjectEvent *objectEvent);
void ObjectEvent_SetMovementType(ObjectEvent *objectEvent, int movementType);
int ObjectEvent_GetMovementType(const ObjectEvent *objectEvent);
void ObjectEvent_SetTrainerType(ObjectEvent *objectEvent, int trainerType);
int ObjectEvent_GetTrainerType(const ObjectEvent *objectEvent);
void ObjectEvent_SetHiddenFlag(ObjectEvent *objectEvent, int flag);
int ObjectEvent_GetHiddenFlag(const ObjectEvent *objectEvent);
void ObjectEvent_SetScript(ObjectEvent *objectEvent, int script);
int ObjectEvent_GetScript(const ObjectEvent *objectEvent);
void ObjectEvent_SetInitialDir(ObjectEvent *objectEvent, int initialDir);
int ObjectEvent_GetInitialDir(const ObjectEvent *objectEvent);
void ObjectEvent_SetDataAt(ObjectEvent *objectEvent, int value, int index);
int ObjectEvent_GetDataAt(const ObjectEvent *objectEvent, int index);
void ObjectEvent_SetMovementRangeX(ObjectEvent *objectEvent, int movementRangeX);
int ObjectEvent_GetMovementRangeX(const ObjectEvent *objectEvent);
void ObjectEvent_SetMovementRangeZ(ObjectEvent *objectEvent, int movementRangeZ);
int ObjectEvent_GetMovementRangeZ(const ObjectEvent *objectEvent);
void ObjectEvent_SetX(ObjectEvent *objectEvent, int x);
int ObjectEvent_GetX(const ObjectEvent *objectEvent);
void ObjectEvent_SetY(ObjectEvent *objectEvent, int y);
int ObjectEvent_GetY(const ObjectEvent *objectEvent);
void ObjectEvent_SetZ(ObjectEvent *objectEvent, int z);
int ObjectEvent_GetZ(const ObjectEvent *objectEvent);
MapObject *MapObjectMan_FindObjectAtCoords(const MapObjectManager *mapObjMan, int x, int z, int param3);
void MapObject_SetPosDirFromVec(MapObject *mapObj, const VecFx32 *pos, int dir);
void MapObject_SetPosDirFromCoords(MapObject *mapObj, int x, int y, int z, int dir);
void MapObject_SwitchMovementType(MapObject *mapObj, u32 movementType);
void MapObject_SetLocalIDAndStartMovement(MapObject *mapObj, int localID);
void MapObjectMovement_NoOp1(MapObject *mapObj);
void MapObjectMovement_NoOp2(MapObject *mapObj);
void MapObjectMovement_NoOp3(MapObject *mapObj);
void MapObjectMovement_NoOp4(MapObject *mapObj);
void MapObjectRenderer_NoOp1(MapObject *mapObj);
void MapObjectRenderer_NoOp2(MapObject *mapObj);
void MapObjectRenderer_NoOp3(MapObject *mapObj);
void MapObjectRenderer_NoOp4(MapObject *mapObj);

#endif // POKEPLATINUM_UNK_02061804_H
