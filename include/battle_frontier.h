#ifndef POKEPLATINUM_BATTLE_FRONTIER_H
#define POKEPLATINUM_BATTLE_FRONTIER_H

#include "struct_decls/battle_frontier_decl.h"

#include "overlay104/frontier_graphics.h"
#include "overlay104/struct_ov104_02230BE4.h"
#include "overlay104/struct_ov104_0223C634.h"
#include "overlay104/struct_ov104_0223C688.h"
#include "overlay104/struct_ov104_0223D3B0.h"

#include "overlay_manager.h"

// Passed as the entry point offset to BattleFrontier_ChangeScene to keep the
// current script and only reload the scene's messages.
#define NO_NEW_ENTRY_POINT 0xffff

// Called with the sub-app's arguments once a sub-app launched through
// BattleFrontier_RunSubApp has finished.
typedef void (*BattleFrontierSubAppCallback)(void *);

extern const ApplicationManagerTemplate gBattleFrontierAppTemplate;

FieldFrontierDTO *BattleFrontier_GetFieldData(BattleFrontier *frontier);
FrontierGraphics *BattleFrontier_GetGraphics(BattleFrontier *frontier);
void *BattleFrontier_GetFacilityStruct(BattleFrontier *frontier);
void BattleFrontier_SetFacilityStruct(BattleFrontier *frontier, void *facilityData);
void BattleFrontier_RunSubApp(BattleFrontier *frontier, const ApplicationManagerTemplate *appTemplate, void *appArgs, BOOL freeArgsAfter, BattleFrontierSubAppCallback finishCallback);
void BattleFrontier_ExitFrontier(BattleFrontier *frontier);
void BattleFrontier_ChangeScene(BattleFrontier *frontier, u16 sceneID, u16 entryPointOffset);
FrontierObjectGfx *BattleFrontier_GetObjectGfxList(BattleFrontier *frontier);
FrontierObject *BattleFrontier_GetObjects(BattleFrontier *frontier);
FrontierObject *BattleFrontier_GetObject(BattleFrontier *frontier, int index);
FrontierSpriteStateBuffer *BattleFrontier_GetSpriteStateBuffer(BattleFrontier *frontier);
void BattleFrontier_ClearSpriteStateBuffer(BattleFrontier *frontier);
FieldFrontierDTO *BattleFrontier_LaunchWFCFacilitySelector(FieldSystem *fieldSystem, void *data);

#endif // POKEPLATINUM_BATTLE_FRONTIER_H
