#include "wifi_menu.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "generated/trainer_score_events.h"

#include "struct_defs/struct_0207DE04.h"

#include "field/field_system.h"
#include "overlay065/ov65_0222DCE0.h"
#include "overlay065/ov65_0223648C.h"
#include "overlay066/struct_ov66_02231134.h"
#include "overlay098/struct_ov98_02247168.h"
#include "overlay115/ov115_02260440.h"
#include "overlay115/struct_ov115_02260440.h"
#include "overlay116/ov116_022604C4.h"
#include "overlay117/ov117_02260440.h"
#include "overlay117/struct_ov117_02260440.h"
#include "wfc_settings/wfc_settings.h"

#include "battle_frontier.h"
#include "communication_system.h"
#include "encounter.h"
#include "field_system.h"
#include "field_task.h"
#include "game_overlay.h"
#include "game_records.h"
#include "heap.h"
#include "poffin_berry_selection_context.h"
#include "system_flags.h"
#include "unk_02038FFC.h"
#include "field_system_apps.h"
#include "vars_flags.h"
#include "wifi_overlays.h"

FS_EXTERN_OVERLAY(overlay65);
FS_EXTERN_OVERLAY(overlay114);
FS_EXTERN_OVERLAY(overlay115);
FS_EXTERN_OVERLAY(overlay116);
FS_EXTERN_OVERLAY(overlay117);

// Field task that drives the Nintendo WFC menu. It runs the overlay065 menu
// app, then dispatches to the activity the player picked: a WiFi battle, a
// Union Room trade, the WFC settings, the Battle Frontier WiFi facility
// selector, Poffin cooking, or one of the three WiFi Plaza minigames.
typedef struct {
    // Arguments handed to the menu app and to the communication app. The menu
    // app writes the selected activity into `appArgs->unk_04`.
    UnkStruct_ov98_02247168 *appArgs;
    // State machine state (see WiFiMenu_Task).
    int state;
    // Script variable that receives the result of the WiFi app entry point.
    u16 *result;
    // Level the player's party is normalized to for a WiFi battle; 0 means
    // open level (no normalization).
    u8 normalizedLevel;
    // WiFi battle type: 0 = singles, 1 = doubles.
    u8 wifiBattleType;
    // Data for the currently running child application.
    void *appData;
    // Result reported by the communication app, forwarded to the follow-up app.
    u32 appResult;
} WiFiMenuWork;

static BOOL WiFiMenu_Task(FieldTask *param0);
static void WiFiMenu_LaunchCommApp(WiFiMenuWork *param0, FieldSystem *fieldSystem, enum HeapID heapID, u32 param3);
static u32 WiFiMenu_HandleCommAppResult(WiFiMenuWork *param0);
static UnkStruct_ov115_02260440 *WiFiMenu_LaunchSwalotMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2);
static void WiFiMenu_FreeSwalotMinigame(UnkStruct_ov115_02260440 *param0);
static UnkStruct_ov66_02231134 *WiFiMenu_LaunchMimeJrMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2);
static void WiFiMenu_FreeMimeJrMinigame(UnkStruct_ov66_02231134 *param0);
static UnkStruct_ov117_02260440 *WiFiMenu_LaunchWobbuffetMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2);
static void WiFiMenu_FreeWobbuffetMinigame(UnkStruct_ov115_02260440 *param0);
static void WiFiMenu_IncrementTrainerScore(FieldSystem *fieldSystem);

// The overlay065 menu app (ov65_0222E2A8) that lets the player choose a WiFi
// activity.
static const ApplicationManagerTemplate sWiFiMenuAppTemplate = {
    ov65_0222E2A8,
    ov65_0222E3FC,
    ov65_0222E548,
    FS_OVERLAY_ID(overlay65)
};

// The overlay065 communication app (ov65_0223648C) that runs the chosen
// activity and reports its outcome through WiFiCommAppArgs.
static const ApplicationManagerTemplate sWiFiCommAppTemplate = {
    ov65_0223648C,
    ov65_02236548,
    ov65_0223668C,
    FS_OVERLAY_ID(overlay65)
};

// Number of players each activity requires, indexed by WiFiCommAppArgs.appType
// (Poffin cooking needs 3, the three minigames need 4).
static const u8 sRequiredPlayers[4] = {
    0x3,
    0x4,
    0x4,
    0x4
};

static BOOL WiFiMenu_Task(FieldTask *task)
{
    int v0;
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    WiFiMenuWork *v2 = FieldTask_GetEnv(task);

    switch (v2->state) {
    case 0:
        v2->appArgs->saveData = fieldSystem->saveData;
    case 1:
        v2->state++;

        // The WiFi app entry point (WiFiMenu_StartWithResult) skips the menu
        // when the player is already logged in to Nintendo WFC and reports a
        // result of 0.
        if (v2->appArgs->unk_04 == 1) {
            if (WiFiList_HasValidLogin(fieldSystem->saveData)) {
                v2->state = 10;
                *(v2->result) = 0;
            }
        }
        break;
    case 2:
        FieldTask_RunApplication(task, &sWiFiMenuAppTemplate, v2->appArgs);
        v2->state++;
        break;
    case 3:
        if (WiFiList_HasValidLogin(fieldSystem->saveData)) {
            SystemFlag_SetConnectedToWiFi(SaveData_GetVarsFlags(fieldSystem->saveData));
        }

        // The menu app has written the selected activity into appArgs->unk_04.
        switch (v2->appArgs->unk_04) {
        case 3:
            // Open-level singles battle.
            v2->normalizedLevel = 0;
            v2->wifiBattleType = 0;
            v2->state = 4;
            break;
        case 1:
            // Level-50 singles battle.
            v2->normalizedLevel = 50;
            v2->wifiBattleType = 0;
            v2->state = 4;
            break;
        case 2:
            // Level-100 singles battle.
            v2->normalizedLevel = 100;
            v2->wifiBattleType = 0;
            v2->state = 4;
            break;
        case 6:
            // Open-level doubles battle.
            v2->normalizedLevel = 0;
            v2->wifiBattleType = 1;
            v2->state = 4;
            break;
        case 4:
            // Level-50 doubles battle.
            v2->normalizedLevel = 50;
            v2->wifiBattleType = 1;
            v2->state = 4;
            break;
        case 5:
            // Level-100 doubles battle.
            v2->normalizedLevel = 100;
            v2->wifiBattleType = 1;
            v2->state = 4;
            break;
        case 7:
            // Union Room trade.
            v2->state = 6;
            break;
        case 10:
            // The player chose to log out; report a result of 1.
            *(v2->result) = 1;
            v2->state = 11;
            break;
        case 8:
            // Exit the menu.
            v2->state = 9;
            break;
        case 11:
            // Poffin cooking.
            v2->state = 12;
            break;
        case 12:
            // Battle Frontier WiFi facility selector.
            v2->state = 16;
            break;
        case 13:
            // Swalot minigame.
            v2->state = 18;
            break;
        case 14:
            // Mime Jr. minigame.
            v2->state = 22;
            break;
        case 15:
            // Wobbuffet minigame.
            v2->state = 26;
            break;
        case 9:
            // Open the WFC settings.
            v2->state = 8;
            break;
        }
        break;
    case 4:
        Encounter_NewVsWiFi(task, v2->appArgs->unk_08, v2->normalizedLevel, v2->wifiBattleType);
        v2->state++;
        break;
    case 5:
        v2->state = 2;
        break;
    case 6:
        FieldTask_StartUnionRoomTrade(task);
        v2->state++;
        break;
    case 7:
        v2->state = 2;
        break;
    case 8:
        // Launch the WFC settings application and reboot into it.
        Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_54, DWC_UTILITY_WORK_SIZE + 0x100);
        Overlay_LoadWFCSettingsOverlay();
        WFCSettings_StartApplication(HEAP_ID_54);
        OS_ResetSystem(0);
        break;
    case 9:
    case 11:
    case 10:
        Heap_Free(v2->appArgs);
        Heap_Free(v2);
        v2->state++;
        return 1;
    case 12:
        WiFiMenu_LaunchCommApp(v2, fieldSystem, HEAP_ID_FIELD2, 0);
        v2->state++;
        break;
    case 13:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = WiFiMenu_HandleCommAppResult(v2);
        }
        break;
    case 14:
        v2->appData = PoffinBerrySelectionContext_CreateVoiceChat(fieldSystem, HEAP_ID_FIELD2, v2->appResult);
        v2->state++;
        break;
    case 15:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            Heap_Free(v2->appData);
            v2->state = 2;
        }
        break;
    case 16:
        CommSys_SetRecvLimitEnabled(0);
        v2->appData = BattleFrontier_LaunchWFCFacilitySelector(fieldSystem, NULL);
        v2->state++;
        break;
    case 17:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            Heap_Free(v2->appData);
            v2->state = 2;
        }
        break;
    case 18:
        WiFiMenu_LaunchCommApp(v2, fieldSystem, HEAP_ID_FIELD2, 1);
        v2->state++;
        break;
    case 19:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = WiFiMenu_HandleCommAppResult(v2);
        }
        break;
    case 20:
        WiFiMenu_IncrementTrainerScore(fieldSystem);
        v2->appData = WiFiMenu_LaunchSwalotMinigame(fieldSystem, HEAP_ID_FIELD2, v2->appResult);
        v2->state++;
        break;
    case 21:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = 2;
            WiFiMenu_FreeSwalotMinigame(v2->appData);
        }
        break;
    case 22:
        WiFiMenu_LaunchCommApp(v2, fieldSystem, HEAP_ID_FIELD2, 2);
        v2->state++;
        break;
    case 23:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = WiFiMenu_HandleCommAppResult(v2);
        }
        break;
    case 24:
        WiFiMenu_IncrementTrainerScore(fieldSystem);
        v2->appData = WiFiMenu_LaunchMimeJrMinigame(fieldSystem, HEAP_ID_FIELD2, v2->appResult);
        v2->state++;
        break;
    case 25:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = 2;
            WiFiMenu_FreeMimeJrMinigame(v2->appData);
        }
        break;
    case 26:
        WiFiMenu_LaunchCommApp(v2, fieldSystem, HEAP_ID_FIELD2, 3);
        v2->state++;
        break;
    case 27:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = WiFiMenu_HandleCommAppResult(v2);
        }
        break;
    case 28:
        WiFiMenu_IncrementTrainerScore(fieldSystem);
        v2->appData = WiFiMenu_LaunchWobbuffetMinigame(fieldSystem, HEAP_ID_FIELD2, v2->appResult);
        v2->state++;
        break;
    case 29:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v2->state = 2;
            WiFiMenu_FreeWobbuffetMinigame(v2->appData);
        }
        break;
    default:
        return 1;
    }

    return 0;
}

static WiFiMenuWork *WiFiMenu_New(void)
{
    WiFiMenuWork *v0 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(WiFiMenuWork));

    MI_CpuClear8(v0, sizeof(WiFiMenuWork));
    v0->appArgs = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(UnkStruct_ov98_02247168));
    MI_CpuClear8(v0->appArgs, sizeof(UnkStruct_ov98_02247168));
    return v0;
}

// Starts the WiFi menu from the WiFi Club script. The task runs the menu and
// launches whichever activity the player picks.
void WiFiMenu_Start(FieldTask *param0)
{
    WiFiMenuWork *v0 = WiFiMenu_New();

    v0->appArgs->unk_04 = 2;
    FieldTask_InitCall(param0, WiFiMenu_Task, v0);
}

// Starts the WiFi menu from the GTS / WiFi Plaza / Battle Tower scripts. The
// result of the WiFi app is written back to `param1`.
void WiFiMenu_StartWithResult(FieldTask *param0, u16 *param1)
{
    WiFiMenuWork *v0 = WiFiMenu_New();

    v0->appArgs->unk_04 = 1;
    v0->result = param1;

    FieldTask_InitCall(param0, WiFiMenu_Task, v0);
}

// Sets up the arguments for the overlay065 communication app and starts it.
// `param3` selects the activity (see WiFiCommAppArgs.appType).
static void WiFiMenu_LaunchCommApp(WiFiMenuWork *param0, FieldSystem *fieldSystem, enum HeapID heapID, u32 param3)
{
    WiFiCommAppArgs *v0 = Heap_Alloc(heapID, sizeof(WiFiCommAppArgs));

    v0->appType = param3;
    v0->minPlayers = 2;
    v0->requiredPlayers = sRequiredPlayers[param3];
    v0->success = 0;
    v0->result = 0;
    v0->saveData = fieldSystem->saveData;

    param0->appData = v0;

    FieldSystem_StartChildProcess(fieldSystem, &sWiFiCommAppTemplate, v0);
}

// Reads the outcome the communication app wrote into its arguments and returns
// the next state: the follow-up app for the activity on success, or back to
// the menu on failure.
static u32 WiFiMenu_HandleCommAppResult(WiFiMenuWork *param0)
{
    WiFiCommAppArgs *v0 = param0->appData;

    if (v0->success == 1) {
        switch (v0->appType) {
        case 0:
            param0->state = 14;
            break;
        case 1:
            param0->state = 20;
            break;
        case 2:
            param0->state = 24;
            break;
        case 3:
            param0->state = 28;
            break;
        }
    } else {
        param0->state = 1;
    }

    param0->appResult = v0->result;
    Heap_Free(param0->appData);

    return param0->state;
}

// Launches the Swalot berry-throwing WiFi Plaza minigame (overlay115).
static UnkStruct_ov115_02260440 *WiFiMenu_LaunchSwalotMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2)
{
    UnkStruct_ov115_02260440 *v0;

    FS_EXTERN_OVERLAY(overlay115);
    FS_EXTERN_OVERLAY(overlay114);

    {
        static const ApplicationManagerTemplate v1 = {
            ov115_02260440,
            ov115_0226048C,
            ov115_022608E4,
            FS_OVERLAY_ID(overlay115),
        };

        v0 = Heap_Alloc(heapID, sizeof(UnkStruct_ov115_02260440));
        memset(v0, 0, sizeof(UnkStruct_ov115_02260440));

        v0->unk_38 = param2;
        v0->unk_39 = 0;
        v0->saveData = fieldSystem->saveData;

        Overlay_LoadByID(FS_OVERLAY_ID(overlay114), 2);
        FieldSystem_StartChildProcess(fieldSystem, &v1, v0);
    }
    return v0;
}

static void WiFiMenu_FreeSwalotMinigame(UnkStruct_ov115_02260440 *param0)
{
    FS_EXTERN_OVERLAY(overlay114);

    Heap_Free(param0);
    Overlay_UnloadByID(FS_OVERLAY_ID(overlay114));
}

// Launches the Mime Jr. ball-rolling WiFi Plaza minigame (overlay116).
static UnkStruct_ov66_02231134 *WiFiMenu_LaunchMimeJrMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2)
{
    UnkStruct_ov66_02231134 *v0;

    FS_EXTERN_OVERLAY(overlay116);
    FS_EXTERN_OVERLAY(overlay114);
    {
        static const ApplicationManagerTemplate v1 = {
            ov116_022609B4,
            ov116_02260CF4,
            ov116_0226126C,
            FS_OVERLAY_ID(overlay116),
        };

        v0 = Heap_Alloc(heapID, sizeof(UnkStruct_ov66_02231134));
        memset(v0, 0, sizeof(UnkStruct_ov66_02231134));
        v0->unk_3C = param2;
        v0->unk_38 = 0;
        v0->saveData = fieldSystem->saveData;

        Overlay_LoadByID(FS_OVERLAY_ID(overlay114), 2);
        FieldSystem_StartChildProcess(fieldSystem, &v1, v0);
    }
    return v0;
}

static void WiFiMenu_FreeMimeJrMinigame(UnkStruct_ov66_02231134 *param0)
{
    FS_EXTERN_OVERLAY(overlay114);

    Heap_Free(param0);
    Overlay_UnloadByID(FS_OVERLAY_ID(overlay114));
}

// Launches the Wobbuffet balloon-popping WiFi Plaza minigame (overlay117).
static UnkStruct_ov117_02260440 *WiFiMenu_LaunchWobbuffetMinigame(FieldSystem *fieldSystem, enum HeapID heapID, u32 param2)
{
    UnkStruct_ov117_02260440 *v0;

    FS_EXTERN_OVERLAY(overlay117);
    FS_EXTERN_OVERLAY(overlay114);

    {
        static const ApplicationManagerTemplate v1 = {
            ov117_02260440,
            ov117_02260474,
            ov117_022605C0,
            FS_OVERLAY_ID(overlay117),
        };

        v0 = Heap_Alloc(heapID, sizeof(UnkStruct_ov117_02260440));
        MI_CpuClear8(v0, sizeof(UnkStruct_ov117_02260440));

        v0->unk_38 = param2;
        v0->unk_39 = 0;
        v0->saveData = fieldSystem->saveData;

        Overlay_LoadByID(FS_OVERLAY_ID(overlay114), 2);
        FieldSystem_StartChildProcess(fieldSystem, &v1, v0);
    }

    return v0;
}

static void WiFiMenu_FreeWobbuffetMinigame(UnkStruct_ov115_02260440 *param0)
{
    FS_EXTERN_OVERLAY(overlay114);

    Heap_Free(param0);
    Overlay_UnloadByID(FS_OVERLAY_ID(overlay114));
}

// Records that the player took part in a WiFi Plaza minigame.
static void WiFiMenu_IncrementTrainerScore(FieldSystem *fieldSystem)
{
    GameRecords *v0 = SaveData_GetGameRecords(fieldSystem->saveData);
    GameRecords_IncrementTrainerScore(v0, TRAINER_SCORE_EVENT_UNK_50);
}
