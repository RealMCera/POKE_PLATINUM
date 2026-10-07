#include "union_room_spin_trade.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_0209C194.h"
#include "struct_defs/struct_0209C194_1.h"

#include "applications/party_menu/defs.h"
#include "applications/pokemon_summary_screen/main.h"
#include "field/field_system.h"
#include "overlay109/ov109_021D0D80.h"
#include "overlay109/ov109_021D3D50.h"

#include "comm_manager.h"
#include "field_system.h"
#include "field_task.h"
#include "game_options.h"
#include "game_records.h"
#include "heap.h"
#include "journal.h"
#include "save_player.h"
#include "field_system_apps.h"
#include "union_room_comm.h"

FS_EXTERN_OVERLAY(overlay109);

// Field task that runs one Union Room spin trade. It is created by ScrCmd_2C6
// once two players agree to a spin trade, and drives the whole exchange:
//   0. launch the group confirmation app (overlay109) that gathers the players,
//   1. wait for it, then open the party menu to choose the egg to offer,
//   2. either view the chosen egg's summary or launch the spin trade app,
//   3. return to the party menu after the summary,
//   4. wait for the spin trade app to finish,
//   5. free the session.
// Both overlay apps receive the same UnionRoomSpinTradeSession as their
// argument and report back through it.
typedef struct UnionRoomSpinTradeTask {
    int state; // Index into sSpinTradeTaskStates.
    int selectedMonSlot; // Party slot of the egg, kept while the menu/summary are open.
    UnionRoomSpinTradeContext context; // Game state handed to the session.
    UnionRoomSpinTradeSession *session; // Session shared with both overlay apps.
    FieldSystem *fieldSystem;
    PartyMenu *partyMenu; // Non-NULL while the egg menu is open.
    PokemonSummary *monSummary; // Non-NULL while the egg summary is open.
} UnionRoomSpinTradeTask;

static BOOL (*const sSpinTradeTaskStates[6])(UnionRoomSpinTradeTask *);
static const ApplicationManagerTemplate sSpinTradeGroupAppTemplate;
static const ApplicationManagerTemplate sSpinTradeAppTemplate;

UnionRoomSpinTradeSession *UnionRoomSpinTrade_NewSession(UnionRoomSpinTradeContext *param0, enum HeapID heapID)
{
    UnionRoomSpinTradeSession *v0 = Heap_Alloc(heapID, sizeof(UnionRoomSpinTradeSession));
    memset(v0, 0, sizeof(UnionRoomSpinTradeSession));
    v0->context = *param0;
    v0->comm = UnionRoomComm_New(v0, heapID);

    return v0;
}

void UnionRoomSpinTrade_FreeSession(UnionRoomSpinTradeSession *param0)
{
    UnionRoomComm_Reset(param0->comm);
    UnionRoomComm_Free(param0->comm);
    Heap_Free(param0);
}

BOOL UnionRoomSpinTrade_IsGroupConfirmed(UnionRoomSpinTradeSession *param0)
{
    return param0->groupConfirmed;
}

void *UnionRoomSpinTrade_New(FieldSystem *fieldSystem)
{
    UnionRoomSpinTradeTask *v0 = Heap_Alloc(HEAP_ID_FIELD2, sizeof(UnionRoomSpinTradeTask));
    memset(v0, 0, sizeof(UnionRoomSpinTradeTask));

    v0->fieldSystem = fieldSystem;
    v0->context.saveData = fieldSystem->saveData;
    v0->context.trainers = fieldSystem->unk_80;
    v0->context.options = SaveData_GetOptions(fieldSystem->saveData);
    v0->context.records = SaveData_GetGameRecords(fieldSystem->saveData);
    v0->context.journalEntry = SaveData_GetJournal(fieldSystem->saveData);
    v0->context.messageBoxFrame = Options_Frame(v0->context.options);
    v0->context.fieldSystem = fieldSystem;

    return v0;
}

// Advances the state machine one step. Runs the current state handler; when it
// reports completion the task state is freed and the caller is told to stop.
BOOL UnionRoomSpinTrade_Update(void *param0)
{
    UnionRoomSpinTradeTask *v0 = param0;

    if (sSpinTradeTaskStates[v0->state](v0) == 1) {
        Heap_Free(v0);
        return 1;
    }

    return 0;
}

static BOOL UnionRoomSpinTrade_StateStart(UnionRoomSpinTradeTask *param0)
{
    param0->session = UnionRoomSpinTrade_NewSession(&param0->context, HEAP_ID_FIELD2);
    param0->state = 1;
    FieldSystem_StartChildProcess(param0->fieldSystem, &sSpinTradeGroupAppTemplate, param0->session);
    return 0;
}

// The group confirmation app has exited. If the group was not confirmed there
// is nothing to trade, so skip to cleanup; otherwise open the egg menu.
static BOOL UnionRoomSpinTrade_StateGroupDone(UnionRoomSpinTradeTask *param0)
{
    if (FieldSystem_IsRunningApplication(param0->fieldSystem) == 0) {
        if (UnionRoomSpinTrade_IsGroupConfirmed(param0->session) == 0) {
            param0->state = 5;
        } else {
            CommManager_SetErrorHandling(1, 1);

            param0->partyMenu = FieldSystem_OpenPartyMenu_SelectForSpinTrade(param0->fieldSystem, param0->selectedMonSlot);
            param0->session->unk_00 = 1;
            param0->state = 2;
        }
    }

    return 0;
}

// The egg menu has closed. menuSelectionResult == PARTY_MENU_EXIT_CODE_SUMMARY
// means the player opened the summary rather than confirming, so show it and
// return to the menu afterwards. Otherwise the chosen slot is stored and the
// spin trade app is launched.
static BOOL UnionRoomSpinTrade_StateEggMenuDone(UnionRoomSpinTradeTask *param0)
{
    if (FieldSystem_IsRunningApplication(param0->fieldSystem) == 0) {
        int v0 = param0->partyMenu->selectedMonSlot;

        Heap_Free(param0->partyMenu);

        // The result is read after the menu is freed, matching the original.
        if (param0->partyMenu->menuSelectionResult == 1) {
            param0->monSummary = FieldSystem_CreatePartyMonSummary(param0->fieldSystem, HEAP_ID_APPLICATION, SUMMARY_MODE_NORMAL);
            param0->selectedMonSlot = v0;
            param0->monSummary->monIndex = v0;
            FieldSystem_OpenSummaryScreen(param0->fieldSystem, param0->monSummary);
            param0->state = 3;
        } else {
            param0->session->selectedMonSlot = v0;
            FieldSystem_StartChildProcess(param0->fieldSystem, &sSpinTradeAppTemplate, param0->session);
            param0->session->unk_00 = 3;
            param0->state = 4;
        }
    }

    return 0;
}

// The summary screen has closed; reopen the egg menu so the player can confirm.
static BOOL UnionRoomSpinTrade_StateSummaryDone(UnionRoomSpinTradeTask *param0)
{
    if (FieldSystem_IsRunningApplication(param0->fieldSystem) == 0) {
        Heap_Free(param0->monSummary);
        param0->partyMenu = FieldSystem_OpenPartyMenu_SelectForSpinTrade(param0->fieldSystem, param0->selectedMonSlot);
        param0->state = 2;
    }

    return 0;
}

static BOOL UnionRoomSpinTrade_StateSpinDone(UnionRoomSpinTradeTask *param0)
{
    if (FieldSystem_IsRunningApplication(param0->fieldSystem) == 0) {
        param0->state = 5;
    }

    return 0;
}

// Frees the session; returning 1 makes UnionRoomSpinTrade_Update free the task.
static BOOL UnionRoomSpinTrade_StateFinish(UnionRoomSpinTradeTask *param0)
{
    UnionRoomSpinTrade_FreeSession(param0->session);
    return 1;
}

static BOOL (*const sSpinTradeTaskStates[6])(UnionRoomSpinTradeTask *) = {
    UnionRoomSpinTrade_StateStart,
    UnionRoomSpinTrade_StateGroupDone,
    UnionRoomSpinTrade_StateEggMenuDone,
    UnionRoomSpinTrade_StateSummaryDone,
    UnionRoomSpinTrade_StateSpinDone,
    UnionRoomSpinTrade_StateFinish,
};

// Group confirmation app: gathers the players and asks them to confirm the
// spin trade before the egg is chosen.
static const ApplicationManagerTemplate sSpinTradeGroupAppTemplate = {
    ov109_021D3D50,
    ov109_021D3EB0,
    ov109_021D3F9C,
    FS_OVERLAY_ID(overlay109)
};

// Spin trade app: performs the 3D egg exchange.
static const ApplicationManagerTemplate sSpinTradeAppTemplate = {
    ov109_021D0D80,
    ov109_021D0F2C,
    ov109_021D0EB4,
    FS_OVERLAY_ID(overlay109)
};
