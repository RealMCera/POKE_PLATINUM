#include "battle_frontier.h"

#include <nitro.h>

#include "constants/battle_frontier.h"
#include "constants/heap.h"

#include "field/field_system.h"
#include "overlay104/defs.h"
#include "overlay104/frontier_graphics.h"
#include "overlay104/frontier_script_manager.h"
#include "overlay104/struct_ov104_0222E8C8.h"
#include "overlay104/struct_ov104_02230BE4.h"
#include "overlay104/struct_ov104_0223C634.h"
#include "overlay104/struct_ov104_0223C688.h"
#include "overlay104/struct_ov104_0223D3B0.h"
#include "overlay104/struct_ov104_0223D3B0_sub1.h"
#include "overlay104/struct_ov104_0223D8F0.h"

#include "field_system.h"
#include "game_overlay.h"
#include "heap.h"
#include "overlay_manager.h"
#include "save_player.h"
#include "system.h"

// This module is the application manager for the Battle Frontier. It owns the
// Frontier script manager and graphics layer and drives a small state machine
// (see BattleFrontier_Main) that runs the current scene's script, launches
// sub-applications on request, and swaps scenes. It also provides the entry
// point used by the Wi-Fi menu to open the WFC facility selector.
//
// Sub-apps (battle facilities, naming screens, party menus, ...) are launched
// through BattleFrontier_RunSubApp. While a sub-app runs, the Frontier graphics
// and overlays are torn down and later rebuilt, so the app caches the loaded
// object graphics and the state of its managed sprites and restores them when
// the sub-app returns.

FS_EXTERN_OVERLAY(overlay63);
FS_EXTERN_OVERLAY(overlay104);
FS_EXTERN_OVERLAY(battle_factory_app);

typedef struct BattleFrontier {
    FieldFrontierDTO *fieldData; // data handed over from the field system
    ApplicationManager *appMan; // active sub-app, or NULL when none is running
    void *appArgs; // arguments passed to the active sub-app
    BattleFrontierSubAppCallback finishCallback; // called when the sub-app finishes
    u8 freeArgsAfter; // whether appArgs should be freed once the sub-app finishes
    FrontierScriptManager *scriptMan; // runs the current scene's script
    FrontierGraphics *graphics; // Frontier rendering layer
    u8 unused;
    u8 isGraphicsInitialized; // graphics are currently allocated
    u8 changeScript; // a scene change has been requested
    u16 offsetID; // entry point offset for the pending scene change
    u8 exitBattleFrontier; // the whole Frontier app has been asked to exit
    FrontierObjectGfx objectGfxList[24]; // cached object-event graphics resources
    FrontierObject objects[32]; // Frontier map objects
    FrontierObjectMovement unk_78C[32]; // unused
    FrontierSpriteStateBuffer spriteStateBuffer; // saved managed-sprite state
} BattleFrontier;

static BOOL BattleFrontier_Init(ApplicationManager *appMan, int *state);
int BattleFrontier_Main(ApplicationManager *appMan, int *state);
int BattleFrontier_Exit(ApplicationManager *appMan, int *state);
static void InitFrontierGraphics(BattleFrontier *frontier);
static void FreeFrontierGraphics(BattleFrontier *frontier);
static void LoadBattleFrontierOverlays(void);
static void UnloadBattleFrontierOverlays(void);
static void ClearFrontierObjects(BattleFrontier *frontier);

const ApplicationManagerTemplate gBattleFrontierAppTemplate = {
    BattleFrontier_Init,
    BattleFrontier_Main,
    BattleFrontier_Exit,
    FS_OVERLAY_ID_NONE
};

static BOOL BattleFrontier_Init(ApplicationManager *appMan, int *state)
{
    LoadBattleFrontierOverlays();

    BattleFrontier *frontier = ApplicationManager_NewData(appMan, sizeof(BattleFrontier), HEAP_ID_FIELD2);
    MI_CpuClear8(frontier, sizeof(BattleFrontier));

    ClearFrontierObjects(frontier);
    BattleFrontier_ClearSpriteStateBuffer(frontier);

    frontier->fieldData = ApplicationManager_Args(appMan);
    GF_ASSERT(frontier->fieldData != NULL);

    frontier->scriptMan = FrontierScriptManager_New(frontier, HEAP_ID_FIELD2, frontier->fieldData->sceneID);
    FrontierScriptManager_Load(frontier->scriptMan, frontier->fieldData->sceneID, 0);

    InitFrontierGraphics(frontier);

    return TRUE;
}

// State machine:
//   0 -> 1  first frame
//   1       run the current scene's script; watch for exit, scene change, or
//           a launched sub-app
//   2       finish the application
//   3       tear down graphics/overlays before running a sub-app
//   4       run the sub-app, then rebuild graphics/overlays and restore state
//   5       tear down graphics/objects before a scene change
//   6       rebuild graphics and load the new scene
int BattleFrontier_Main(ApplicationManager *appMan, int *state)
{
    BattleFrontier *frontier = ApplicationManager_Data(appMan);

    switch (*state) {
    case 0:
        *state = 1;
        break;
    case 1:
        if (frontier->exitBattleFrontier == TRUE) {
            *state = 2;
            break;
        }

        if (!frontier->isGraphicsInitialized) {
            break;
        }

        if (frontier->changeScript == TRUE) {
            *state = 5;
            break;
        }

        if (FrontierScriptManager_RunScript(frontier->scriptMan) == TRUE) {
            // The scene's script has ended; B backs out of the Frontier.
            if (JOY_NEW(PAD_BUTTON_B)) {
                *state = 2;
            }
        }

        if (frontier->appMan != NULL) {
            *state = 3;
        }
        break;
    case 2:
        return TRUE;
    case 3:
        // Save the sprite state, then release the graphics and overlays so the
        // sub-app can use them.
        ov104_0223C634(frontier->graphics);
        FreeFrontierGraphics(frontier);
        UnloadBattleFrontierOverlays();
        *state = 4;
        break;
    case 4:
        if (ApplicationManager_Exec(frontier->appMan) == TRUE) {
            ApplicationManager_Free(frontier->appMan);
            LoadBattleFrontierOverlays();

            if (frontier->finishCallback != NULL) {
                frontier->finishCallback(frontier->appArgs);
            }

            if (frontier->appArgs != NULL && frontier->freeArgsAfter == 1) {
                Heap_Free(frontier->appArgs);
            }

            frontier->appMan = NULL;
            frontier->finishCallback = NULL;
            frontier->appArgs = NULL;

            // Rebuild the graphics and restore the sprites saved in state 3.
            InitFrontierGraphics(frontier);
            ov104_0223C688(frontier->graphics);
            *state = 1;
        }
        break;
    case 5:
        FreeFrontierGraphics(frontier);
        ClearFrontierObjects(frontier);
        *state = 6;
        break;
    case 6:
        InitFrontierGraphics(frontier);

        if (frontier->offsetID == NO_NEW_ENTRY_POINT) {
            // Same scene, new message bank only.
            FrontierScriptManager_UpdateMessageLoader(frontier->scriptMan, frontier->fieldData->sceneID, HEAP_ID_FIELD2);
        } else {
            // Rebuild the script manager for the new scene, carrying the local
            // script variables across.
            FrontierScriptLocalVars *v2 = ov104_0222E8C8(frontier->scriptMan, HEAP_ID_FIELD2);
            FrontierScriptManager_Free(frontier->scriptMan);

            frontier->scriptMan = FrontierScriptManager_New(frontier, HEAP_ID_FIELD2, frontier->fieldData->sceneID);
            FrontierScriptManager_Load(frontier->scriptMan, frontier->fieldData->sceneID, frontier->offsetID);
            ov104_0222E8E8(frontier->scriptMan, v2);
        }

        frontier->changeScript = FALSE;
        *state = 1;
        break;
    }

    return FALSE;
}

int BattleFrontier_Exit(ApplicationManager *appMan, int *state)
{
    BattleFrontier *frontier = ApplicationManager_Data(appMan);

    FrontierScriptManager_Free(frontier->scriptMan);

    FreeFrontierGraphics(frontier);
    ApplicationManager_FreeData(appMan);
    UnloadBattleFrontierOverlays();

    return TRUE;
}

static void InitFrontierGraphics(BattleFrontier *frontier)
{
    frontier->graphics = FrontierGraphics_New(frontier);
    frontier->isGraphicsInitialized = TRUE;
}

static void FreeFrontierGraphics(BattleFrontier *frontier)
{
    FrontierGraphics_Free(frontier->graphics);
    frontier->isGraphicsInitialized = FALSE;
}

// Resets the cached object graphics list and the map object array. The object
// graphics slots use 0xFFFF as their empty marker; the objects' local IDs use
// the same marker.
static void ClearFrontierObjects(BattleFrontier *frontier)
{
    for (int v0 = 0; v0 < 24; v0++) {
        frontier->objectGfxList[v0].gfxID = 0xffff;
    }

    MI_CpuClear8(frontier->objects, sizeof(FrontierObject) * 32);

    for (int v0 = 0; v0 < 32; v0++) {
        frontier->objects[v0].params.unk_04 = 0xffff;
    }
}

static void LoadBattleFrontierOverlays(void)
{
    Overlay_LoadByID(FS_OVERLAY_ID(overlay104), OVERLAY_LOAD_ASYNC);
    Overlay_LoadByID(FS_OVERLAY_ID(battle_factory_app), OVERLAY_LOAD_ASYNC);
    Overlay_LoadByID(FS_OVERLAY_ID(overlay63), OVERLAY_LOAD_ASYNC);
}

static void UnloadBattleFrontierOverlays(void)
{
    Overlay_UnloadByID(FS_OVERLAY_ID(overlay104));
    Overlay_UnloadByID(FS_OVERLAY_ID(battle_factory_app));
    Overlay_UnloadByID(FS_OVERLAY_ID(overlay63));
}

FieldFrontierDTO *BattleFrontier_GetFieldData(BattleFrontier *frontier)
{
    return frontier->fieldData;
}

FrontierGraphics *BattleFrontier_GetGraphics(BattleFrontier *frontier)
{
    return frontier->graphics;
}

void *BattleFrontier_GetFacilityStruct(BattleFrontier *frontier)
{
    return frontier->fieldData->facilityData;
}

void BattleFrontier_SetFacilityStruct(BattleFrontier *frontier, void *facilityData)
{
    frontier->fieldData->facilityData = facilityData;
}

// Queues a sub-application to run. The sub-app is executed from state 4 of
// BattleFrontier_Main, after the Frontier graphics and overlays have been
// released. finishCallback (if any) is invoked with appArgs once it finishes,
// and appArgs is freed afterwards when freeArgsAfter is set.
void BattleFrontier_RunSubApp(BattleFrontier *frontier, const ApplicationManagerTemplate *appTemplate, void *appArgs, BOOL freeArgsAfter, BattleFrontierSubAppCallback finishCallback)
{
    GF_ASSERT(frontier->appMan == NULL);
    frontier->appMan = ApplicationManager_New(appTemplate, appArgs, HEAP_ID_FIELD2);
    frontier->appArgs = appArgs;
    frontier->freeArgsAfter = freeArgsAfter;
    frontier->finishCallback = finishCallback;
}

void BattleFrontier_ExitFrontier(BattleFrontier *frontier)
{
    frontier->exitBattleFrontier = TRUE;
}

// Requests a scene change. The new scene is loaded from state 6 of
// BattleFrontier_Main; entryPointOffset selects the script entry point, or
// NO_NEW_ENTRY_POINT to keep the current script and only reload messages.
void BattleFrontier_ChangeScene(BattleFrontier *frontier, u16 sceneID, u16 entryPointOffset)
{
    frontier->fieldData->sceneID = sceneID;
    frontier->changeScript = TRUE;
    frontier->offsetID = entryPointOffset;
}

FrontierObjectGfx *BattleFrontier_GetObjectGfxList(BattleFrontier *frontier)
{
    return frontier->objectGfxList;
}

FrontierObject *BattleFrontier_GetObjects(BattleFrontier *frontier)
{
    return frontier->objects;
}

FrontierObject *BattleFrontier_GetObject(BattleFrontier *frontier, int index)
{
    return &frontier->objects[index];
}

FrontierSpriteStateBuffer *BattleFrontier_GetSpriteStateBuffer(BattleFrontier *frontier)
{
    return &frontier->spriteStateBuffer;
}

// Empties the saved managed-sprite state. Called on init and after the state
// has been restored following a sub-app.
void BattleFrontier_ClearSpriteStateBuffer(BattleFrontier *frontier)
{
    MI_CpuClear8(&frontier->spriteStateBuffer, sizeof(FrontierSpriteState));

    for (int v0 = 0; v0 < 8; v0++) {
        frontier->spriteStateBuffer.spriteIDs[v0] = 0xffff;
    }
}

// Entry point used by the Wi-Fi menu to open the WFC facility selector. Builds
// the Frontier DTO from the field system and starts the Frontier app as a child
// process, beginning at the WFC facility selector scene.
FieldFrontierDTO *BattleFrontier_LaunchWFCFacilitySelector(FieldSystem *fieldSystem, void *data)
{
    FieldFrontierDTO *fieldData = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(FieldFrontierDTO));

    MI_CpuClear8(fieldData, sizeof(FieldFrontierDTO));

    fieldData->facilityData = data;
    fieldData->options = SaveData_GetOptions(fieldSystem->saveData);
    fieldData->saveData = fieldSystem->saveData;
    fieldData->journalEntry = fieldSystem->journalEntry;
    fieldData->bagCursor = fieldSystem->bagCursor;
    fieldData->subscreenCursorOn = fieldSystem->battleSubscreenCursorOn;
    fieldData->unk_14 = 0;
    fieldData->unk_18 = 0;
    fieldData->mapHeaderID = fieldSystem->location->mapHeaderID;
    fieldData->sceneID = FRONTIER_SCENE_WFC_FACILITY_SELECTOR;
    fieldData->fieldSystem = fieldSystem;

    FieldSystem_StartChildProcess(fieldSystem, &gBattleFrontierAppTemplate, fieldData);

    return fieldData;
}
