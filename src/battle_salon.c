#include "battle_salon.h"

#include <nitro.h>
#include <string.h>

#include "constants/species.h"

#include "struct_defs/battle_frontier.h"
#include "struct_defs/struct_0206BC70.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "applications/party_menu/defs.h"
#include "applications/party_menu/main.h"
#include "applications/pokemon_summary_screen/main.h"
#include "field/field_system.h"

#include "bag.h"
#include "battle_frontier_save.h"
#include "communication_system.h"
#include "dexmode_checker.h"
#include "field_system.h"
#include "field_task.h"
#include "heap.h"
#include "party.h"
#include "record_mixed_rng.h"
#include "ribbon_save_data.h"
#include "save_player.h"
#include "savedata.h"
#include "script_manager.h"
#include "underground.h"
#include "unk_020363E8.h"
#include "unk_02038FFC.h"
#include "field_system_apps.h"
#include "unk_0204AEE8.h"
#include "wifi_battle_tower_save.h"

#include "constdata/const_020F410C.h"

// Battle Salon field support. The Battle Salon is the Battle Tower room where
// the player picks a party, opens the Pokémon summary screen, connects to the
// WiFi Battle Tower app and exchanges opponent data with a link partner. This
// module owns the field tasks that drive those child applications, the reward
// bookkeeping for the WiFi Battle Tower, and the per-record RNG used to
// generate Battle Tower opponents.

// Environment for the field task that runs the party menu and then the Pokémon
// summary screen, writing the player's selection back into the task.
typedef struct PartyMenuSummaryTask {
    int unk_00; // set when the party menu ends without a selection, but never read
    int state; // field task state
    u8 menuMode; // party menu mode
    u8 summaryMode; // summary screen mode
    u8 minSelectionSlots; // minimum number of party slots the player must pick
    u8 maxSelectionSlots; // maximum number of party slots the player may pick
    u8 reqLevel; // minimum level a selected Pokémon must have
    u8 selectedMonSlot; // currently selected party slot
    u8 selectionOrder[6]; // order in which party slots were selected
    void **app; // active child application data (PartyMenu or PokemonSummary)
} PartyMenuSummaryTask;

// Environment for the field task that opens the WiFi Battle Tower app and
// writes its result into a script variable.
typedef struct WifiBattleTowerTask {
    int result; // result written to the script variable
    int state; // field task state
    UnkStruct_0206BC70 *appState; // WiFi Battle Tower app state
    u16 **unk_0C; // unused
    u16 varID; // script variable that receives the result
    u16 mode; // WiFi Battle Tower app mode
    u16 appParam; // extra parameter passed to the app
} WifiBattleTowerTask;

// Environment for the field task that reads the link partner's opponent data
// and writes it into a script variable.
typedef struct BattleSalonCommTask {
    u16 commandType; // which opponent-data command to run
    u16 varID; // script variable that receives the result
} BattleSalonCommTask;

static int BattleSalon_StartPartyMenu(PartyMenuSummaryTask *param0, FieldSystem *fieldSystem, enum HeapID heapID)
{
    u8 v0;
    SaveData *saveData;
    PartyMenu *partyMenu = Heap_AllocAtEnd(heapID, sizeof(PartyMenu));

    saveData = fieldSystem->saveData;
    MI_CpuClear8(partyMenu, sizeof(PartyMenu));

    partyMenu->options = SaveData_GetOptions(saveData);
    partyMenu->party = SaveData_GetParty(saveData);
    partyMenu->bag = SaveData_GetBag(saveData);
    partyMenu->type = PARTY_MENU_TYPE_BASIC;
    partyMenu->mode = param0->menuMode;
    partyMenu->minSelectionSlots = param0->minSelectionSlots;
    partyMenu->maxSelectionSlots = param0->maxSelectionSlots;
    partyMenu->reqLevel = param0->reqLevel;
    partyMenu->selectedMonSlot = param0->selectedMonSlot;

    for (v0 = 0; v0 < 6; v0++) {
        partyMenu->selectionOrder[v0] = param0->selectionOrder[v0];
    }

    FieldSystem_StartChildProcess(fieldSystem, &gPokemonPartyAppTemplate, partyMenu);

    *(param0->app) = partyMenu;
    return 1;
}

static int BattleSalon_WaitPartyMenu(PartyMenuSummaryTask *param0, FieldSystem *fieldSystem)
{
    int v0;
    PartyMenu *partyMenu;

    if (FieldSystem_IsRunningApplication(fieldSystem)) {
        return 1;
    }

    partyMenu = *(param0->app);

    // Slots 6 (empty) and 7 (cancel) abort the selection and end the task.
    switch (partyMenu->selectedMonSlot) {
    case 7:
        param0->unk_00 = 0;
        return 4;
    case 6:
        param0->unk_00 = 1;
        return 4;
    default:
        break;
    }

    MI_CpuCopy8(partyMenu->selectionOrder, param0->selectionOrder, 6);
    param0->selectedMonSlot = partyMenu->selectedMonSlot;
    Heap_Free(partyMenu);
    *(param0->app) = NULL;

    return 2;
}

static int BattleSalon_StartSummaryScreen(PartyMenuSummaryTask *param0, FieldSystem *fieldSystem, enum HeapID heapID)
{
    static const u8 visiblePages[] = {
        SUMMARY_PAGE_INFO,
        SUMMARY_PAGE_MEMO,
        SUMMARY_PAGE_SKILLS,
        SUMMARY_PAGE_CONDITION,
        SUMMARY_PAGE_BATTLE_MOVES,
        SUMMARY_PAGE_CONTEST_MOVES,
        SUMMARY_PAGE_RIBBONS,
        SUMMARY_PAGE_EXIT,
        SUMMARY_PAGE_MAX,
    };

    SaveData *saveData = fieldSystem->saveData;
    PokemonSummary *monSummary = Heap_AllocAtEnd(heapID, sizeof(PokemonSummary));
    MI_CpuClear8(monSummary, sizeof(PokemonSummary));

    monSummary->options = SaveData_GetOptions(saveData);
    monSummary->monData = SaveData_GetParty(saveData);
    monSummary->dexMode = SaveData_GetDexMode(saveData);
    monSummary->showContest = PokemonSummaryScreen_ShowContestData(saveData);
    monSummary->dataType = SUMMARY_DATA_PARTY_MON;
    monSummary->monIndex = param0->selectedMonSlot;
    monSummary->monMax = Party_GetCurrentCount(monSummary->monData);
    monSummary->move = 0;
    monSummary->mode = param0->summaryMode;
    monSummary->specialRibbons = SaveData_GetRibbons(saveData);

    PokemonSummaryScreen_FlagVisiblePages(monSummary, visiblePages);
    PokemonSummaryScreen_SetPlayerProfile(monSummary, SaveData_GetTrainerInfo(saveData));
    FieldSystem_StartChildProcess(fieldSystem, &gPokemonSummaryScreenApp, monSummary);
    *param0->app = monSummary;

    return 3;
}

static int BattleSalon_WaitSummaryScreen(PartyMenuSummaryTask *param0, FieldSystem *fieldSystem)
{
    if (FieldSystem_IsRunningApplication(fieldSystem)) {
        return 3;
    }

    PokemonSummary *monSummary = *param0->app;
    param0->selectedMonSlot = monSummary->monIndex;
    Heap_Free(monSummary);
    *param0->app = NULL;

    return 0;
}

static BOOL BattleSalon_PartyMenuTask(FieldTask *param0)
{
    FieldSystem *v0 = FieldTask_GetFieldSystem(param0);
    PartyMenuSummaryTask *v1 = FieldTask_GetEnv(param0);

    // Runs the party menu, then the summary screen, then frees the task.
    switch (v1->state) {
    case 0:
        v1->state = BattleSalon_StartPartyMenu(v1, v0, HEAP_ID_FIELD2);
        break;
    case 1:
        v1->state = BattleSalon_WaitPartyMenu(v1, v0);
        break;
    case 2:
        v1->state = BattleSalon_StartSummaryScreen(v1, v0, HEAP_ID_FIELD2);
        break;
    case 3:
        v1->state = BattleSalon_WaitSummaryScreen(v1, v0);
        break;
    case 4:
        Heap_Free(v1);
        return 1;
    }

    return 0;
}

void BattleSalon_StartPartyMenuTask(FieldTask *task, void **appData, u8 menuMode, u8 summaryMode, u8 minSelectionSlots, u8 maxSelectionSlots, u8 reqLevel, u8 selectedMonSlot)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    PartyMenuSummaryTask *taskEnv = Heap_Alloc(HEAP_ID_FIELD2, sizeof(PartyMenuSummaryTask));

    MI_CpuClear8(taskEnv, sizeof(PartyMenuSummaryTask));

    taskEnv->menuMode = menuMode;
    taskEnv->summaryMode = summaryMode;
    taskEnv->minSelectionSlots = minSelectionSlots;
    taskEnv->maxSelectionSlots = maxSelectionSlots;
    taskEnv->reqLevel = reqLevel;
    taskEnv->selectedMonSlot = selectedMonSlot;
    taskEnv->app = appData;

    FieldTask_InitCall(fieldSystem->task, BattleSalon_PartyMenuTask, taskEnv);
}

static int BattleSalon_OpenWifiApp(WifiBattleTowerTask *param0, FieldSystem *fieldSystem)
{
    // Without a valid WiFi login the app cannot be opened; report failure.
    if (WiFiList_HasValidLogin(fieldSystem->saveData)) {
        param0->appState = FieldSystem_OpenWifiBattleTowerApp(fieldSystem, param0->mode, param0->appParam);
        return 1;
    }

    param0->result = 1;
    return 2;
}

static int BattleSalon_WaitWifiApp(WifiBattleTowerTask *param0, FieldSystem *fieldSystem)
{
    if (FieldSystem_IsRunningApplication(fieldSystem)) {
        return 1;
    }

    param0->result = param0->appState->unk_20;
    Heap_Free(param0->appState);

    return 2;
}

static BOOL BattleSalon_WifiAppTask(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    WifiBattleTowerTask *v2 = FieldTask_GetEnv(taskMan);

    switch (v2->state) {
    case 0:
        v2->state = BattleSalon_OpenWifiApp(v2, fieldSystem);
        break;
    case 1:
        v2->state = BattleSalon_WaitWifiApp(v2, fieldSystem);
        break;
    case 2:
        u16 *v0 = FieldSystem_GetVarPointer(fieldSystem, v2->varID);
        *v0 = v2->result;
        Heap_Free(v2);
        return 1;
    }

    return 0;
}

void BattleSalon_StartWifiAppTask(FieldTask *taskMan, u16 mode, u16 varID, u16 appParam)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    WifiBattleTowerTask *v1 = Heap_Alloc(HEAP_ID_FIELD2, sizeof(WifiBattleTowerTask));

    MI_CpuClear8(v1, sizeof(WifiBattleTowerTask));

    v1->mode = mode;
    v1->appParam = appParam;
    v1->varID = varID;

    FieldTask_InitCall(fieldSystem->task, BattleSalon_WifiAppTask, v1);
}

static BOOL BattleSalon_CommTask(FieldTask *param0)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    BattleSalonCommTask *v3 = FieldTask_GetEnv(param0);

    // Wait until the link partner's data has arrived.
    const void *v1 = sub_0203664C(1 - CommSys_CurNetId());

    if (v1 == NULL) {
        return 0;
    }

    u16 *v0 = FieldSystem_GetVarPointer(fieldSystem, v3->varID);

    switch (v3->commandType) {
    case 0:
        *v0 = sub_0204AFC4(fieldSystem, v1);
        break;
    case 1:
        *v0 = sub_0204B020(fieldSystem, v1);
        break;
    case 2:
        *v0 = sub_0204B044(fieldSystem, v1);
    }

    Heap_Free(v3);
    return 1;
}

void BattleSalon_StartCommTask(FieldTask *param0, u16 commandType, u16 varID)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    BattleSalonCommTask *v1 = Heap_Alloc(HEAP_ID_FIELD2, sizeof(BattleSalonCommTask));

    MI_CpuClear8(v1, sizeof(BattleSalonCommTask));

    v1->commandType = commandType;
    v1->varID = varID;

    FieldTask_InitCall(fieldSystem->task, BattleSalon_CommTask, v1);
}

// Grants the next WiFi Battle Tower reward earned by the player's single-room
// record streak. Returns 1/2/3 for the reward just granted, 4 when the reward
// could not be stored because the Underground PC is full, or 0 when no reward
// is due. The reward flags live in the WiFi Battle Tower record.
u16 BattleSalon_GrantReward(SaveData *saveData)
{
    BattleFrontierSave *frontier = SaveData_GetBattleFrontier(saveData);
    u16 streak = BattleFrontierSave_GetStat(frontier, STAT_TOWER_RECORD_STREAK_SINGLE, 0xff);

    if (streak < 20) {
        return 0;
    }

    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    u8 reward1Given = WifiBattleTowerRecord_UpdateBitFlag(record, 13, 0);
    u8 reward2Given = WifiBattleTowerRecord_UpdateBitFlag(record, 0, 0);
    u8 reward3Given = WifiBattleTowerRecord_UpdateBitFlag(record, 1, 0);
    u8 reward1FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 14, 0);
    u8 reward2FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 2, 0);
    u8 reward3FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 3, 0);

    if (reward1Given && reward2Given && reward3Given) {
        return 0;
    }

    Underground *underground = SaveData_GetUnderground(saveData);

    // Reward 1 unlocks at a 20-win streak.
    if (!reward1Given) {
        if (Underground_IsRoomForGoodsInPC(underground, 85)) {
            WifiBattleTowerRecord_UpdateBitFlag(record, 13, 1);
            return 1;
        }

        if (!reward1FullPC) {
            WifiBattleTowerRecord_UpdateBitFlag(record, 14, 1);
        }

        return 4;
    }

    if (streak < 50) {
        return 0;
    }

    // Reward 2 unlocks at a 50-win streak.
    if (!reward2Given) {
        if (Underground_IsRoomForGoodsInPC(underground, 86)) {
            WifiBattleTowerRecord_UpdateBitFlag(record, 0, 1);
            return 2;
        }

        if (!reward2FullPC) {
            WifiBattleTowerRecord_UpdateBitFlag(record, 2, 1);
        }

        return 4;
    }

    if ((streak < 100) || reward3Given) {
        return 0;
    }

    // Reward 3 unlocks at a 100-win streak.
    if (Underground_IsRoomForGoodsInPC(underground, 87)) {
        WifiBattleTowerRecord_UpdateBitFlag(record, 1, 1);
        return 3;
    }

    if (!reward3FullPC) {
        WifiBattleTowerRecord_UpdateBitFlag(record, 3, 1);
    }

    return 4;
}

// Reports which WiFi Battle Tower reward is currently due without granting it.
// Returns 1/2/3 for an available reward, 4/5/6 when the matching reward could
// not be stored because the Underground PC is full, or 0 when none is due.
u16 BattleSalon_GetRewardStatus(SaveData *saveData)
{
    BattleFrontierSave *frontier = SaveData_GetBattleFrontier(saveData);
    u16 streak = BattleFrontierSave_GetStat(frontier, STAT_TOWER_RECORD_STREAK_SINGLE, 0xff);

    if (streak < 20) {
        return 0;
    }

    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    u8 reward1Given = WifiBattleTowerRecord_UpdateBitFlag(record, 13, 0);
    u8 reward2Given = WifiBattleTowerRecord_UpdateBitFlag(record, 0, 0);
    u8 reward3Given = WifiBattleTowerRecord_UpdateBitFlag(record, 1, 0);
    u8 reward1FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 14, 0);
    u8 reward2FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 2, 0);
    u8 reward3FullPC = WifiBattleTowerRecord_UpdateBitFlag(record, 3, 0);

    if (reward1Given && reward2Given && reward3Given) {
        return 0;
    }

    if (!reward1Given) {
        if (reward1FullPC) {
            return 4;
        }

        return 1;
    }

    if (streak < 50) {
        return 0;
    }

    if (!reward2Given) {
        if (reward2FullPC) {
            return 5;
        }

        return 2;
    }

    if (streak < 100) {
        return 0;
    }

    if (reward3Given) {
        return 0;
    }

    if (reward3FullPC) {
        return 6;
    }

    return 3;
}

// Advances the WiFi Battle Tower RNG value by one step (multiplier 5^11).
u32 BattleSalon_AdvanceRng(u32 value)
{
    return value * 48828125L + 1;
}

// Advances the stored WiFi Battle Tower RNG state by one step.
u32 BattleSalon_AdvanceRngState(u32 state)
{
    return state * 1566083941 + 1;
}

// Seeds the WiFi Battle Tower record's RNG state from the save's mixed RNG.
u32 BattleSalon_SeedRng(SaveData *saveData)
{
    u32 state = RecordMixedRNG_GetRand(SaveData_GetRecordMixedRNG(saveData));
    state = BattleSalon_AdvanceRngState(state);

    WifiBattleTowerRecord_SetRngState(SaveData_GetWifiBattleTowerRecord(saveData), state);

    return state;
}

// Produces a fresh RNG value when no WiFi streak is active: advances the stored
// state and derives a value from it, storing the value in the WiFi save.
u32 BattleSalon_GetFreshRng(SaveData *saveData)
{
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);

    u32 state = WifiBattleTowerRecord_GetRngState(record);
    state = BattleSalon_AdvanceRngState(state);

    WifiBattleTowerRecord_SetRngState(record, state);
    u32 value = BattleSalon_AdvanceRng(state);
    WifiBattleTowerSave_SetField(SaveData_GetWifiBattleTowerSave(saveData), 10, &value);

    return value;
}

// Produces an RNG value when a WiFi streak is active: advances the value once
// per room already cleared (24 steps per room) so the sequence continues from
// where the previous session left off.
u32 BattleSalon_GetStreakRng(SaveData *saveData)
{
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    WifiBattleTowerSave *save = SaveData_GetWifiBattleTowerSave(saveData);

    int i;
    u32 state = WifiBattleTowerRecord_GetRngState(record);
    u32 value = BattleSalon_AdvanceRng(state);
    int steps = WifiBattleTowerRecord_UpdateRoomNum(record, WifiBattleTowerSave_GetField(save, 0, NULL), 0);
    steps *= 24;

    for (i = 0; i < steps; i++) {
        value = BattleSalon_AdvanceRng(value);
    }

    WifiBattleTowerSave_SetField(SaveData_GetWifiBattleTowerSave(saveData), 10, &value);

    return value;
}

BOOL FieldSystem_IsInBattleTowerSalon(FieldSystem *fieldSystem)
{
    if (fieldSystem->location->mapHeaderID == MAP_HEADER_BATTLE_TOWER_BATTLE_SALON) {
        return TRUE;
    }

    return FALSE;
}
