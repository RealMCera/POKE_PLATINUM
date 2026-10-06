#include "colosseum.h"

#include <nitro.h>
#include <string.h>

#include "applications/party_menu/defs.h"
#include "applications/party_menu/main.h"
#include "applications/pokemon_summary_screen/main.h"
#include "field/field_system.h"
#include "functypes/funcptr_0205AB10.h"
#include "overlay005/fieldmap.h"

#include "bag.h"
#include "battle_regulation.h"
#include "bg_window.h"
#include "colored_arrow.h"
#include "comm_manager.h"
#include "comm_player_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "dexmode_checker.h"
#include "field_comm_manager.h"
#include "field_message.h"
#include "field_system.h"
#include "field_task.h"
#include "heap.h"
#include "map_object.h"
#include "message.h"
#include "party.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "render_window.h"
#include "ribbon_save_data.h"
#include "save_player.h"
#include "savedata.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "string_template.h"
#include "system.h"
#include "text.h"
#include "trainer_case.h"
#include "trainer_info.h"
#include "unk_020363E8.h"
#include "field_system_apps.h"
#include "map_object_animation.h"

#include "constdata/const_020F410C.h"

// Communication Club Colosseum (battle room) interactions.
//
// The Colosseum is the Communication Club's battle room. This module owns two
// responsibilities:
//
//   - The battle-setup sequence (Colosseum_StartBattle / Colosseum_BattleTask).
//     It asks each player to choose a party, exchanges the chosen parties, and
//     for a Mix Battle lets each player pick one of the opponent's Pokémon to
//     swap in before the battle. The field communication manager calls
//     Colosseum_StartBattle when a player is waiting in the Colosseum; the
//     callback it passes starts or cancels the actual battle.
//   - The Trainer Case viewer (Colosseum_ViewTrainerCase). Pressing the
//     interact button while facing another connected player shows their
//     Trainer Case.
//
// Colosseum_BattleTask is a FieldTask state machine advanced one step per
// frame. Its `state` values are grouped by purpose:
//   0-8    wait for every player to be ready, then start (or cancel) the battle
//   9-22   choose the party through the party menu and summary screen
//   23-25  Mix Battle only: ask for the three participating Pokémon
//   26-44  Mix Battle party exchange and opponent-Pokémon selection
//
// Players synchronize through CommTiming_StartSync/CommTiming_IsSyncState. The
// exchanged data travels as comm command 106 (three Pokémon plus a
// PartyExchangeTail) and command 107 (the selected opponent's party slot).

// State for the Communication Club's Colosseum (battle room) interactions.
// One instance is created by Colosseum_StartBattle and driven by
// Colosseum_BattleTask. It owns the battle-setup state machine, the party
// exchange used by Mix Battles, and the menu windows shown while choosing
// Pokémon.
typedef struct Colosseum {
    PokemonSummary *monSummary; // Summary screen shown when reviewing a party member
    PartyMenu *partyMenu; // Party menu used to pick the battle party
    UnkFuncPtr_0205AB10 *startBattleCallback; // Callback invoked to start or cancel the battle
    String *message; // Scratch string holding a loaded message
    String *formattedMessage; // Scratch string holding a formatted message
    Window messageWindow; // Window used for the module's messages
    FieldSystem *fieldSystem;
    StringTemplate *strTemplate; // Template used to insert player/species names
    MessageLoader *msgLoader; // Loads the Communication Club text bank
    int printerID; // Text printer for the current message
    int state; // Battle-setup state machine
    int partyMenuResult; // Party menu outcome: 0 cancel, 1 confirm, 2 view summary
    u8 partyMenuSelectedSlot; // Party slot returned by the party menu
    u8 selectionOrder[6]; // Party slots chosen in the party menu, in order
    u8 timer; // Generic frame countdown
    u8 delayTimer; // Delay before showing the next message
    u8 *partyExchangeRecv; // Buffer holding the peer's exchanged party data
    u8 *partyExchangeSend; // Buffer holding our exchanged party data
    Party *exchangeParty; // Party built from the exchanged Pokémon
    Window partyListWindow; // Window listing the peer's three Pokémon
    Window confirmWindow; // Window holding the Summary/OK/Cancel menu
    TrainerInfo *peerTrainerInfo; // Trainer info of the other player
    ColoredArrow *arrow; // Cursor arrow for the list windows
    Window *menuWindow; // Window the arrow is currently drawn on
    u8 menuItemCount; // Number of entries in the active menu
    s8 menuCursor; // Selected entry in the active menu
    u8 wantsMixBattle; // Whether we agreed to a Mix Battle
    u8 peerWantsMixBattle; // Whether the peer agreed to a Mix Battle
    u8 selectedOpponentSlot; // Peer party slot we picked (255 = none/cancel)
    u8 peerSelectedSlot; // Peer party slot the other player picked (255 = none/cancel)
    u16 netId; // Our own net ID
    u8 commType; // Communication type (COMM_TYPE_*)
    u8 exchangeFlags; // Exchange progress: bit 0 sent, bit 1 received
} Colosseum;

// Trailer appended to the party-exchange buffer. It records whether the sender
// actually included a party (a player who cancels the Mix Battle still takes
// part in the exchange handshake but sends no Pokémon).
typedef struct PartyExchangeTail {
    u32 hasParty;
} PartyExchangeTail;

// State for the task that shows another player's Trainer Case when the player
// interacts with them in the Colosseum.
typedef struct TrainerCaseViewer {
    String *message; // Scratch string holding a loaded message
    String *formattedMessage; // Scratch string holding a formatted message
    Window window; // Message window
    StringTemplate *strTemplate; // Template used to insert the player's name
    MessageLoader *msgLoader; // Loads the Communication Club text bank
    int printerID; // Text printer for the current message
    int netId; // Net ID of the player whose Trainer Case is shown
    int state; // Task state machine
} TrainerCaseViewer;

static void Colosseum_FreeResources(Colosseum *param0);
static void Colosseum_BuildPartyExchangeData(Colosseum *param0, BOOL param1);
static BOOL Colosseum_SendPartyExchangeData(Colosseum *param0);
static BOOL Colosseum_HasPartyExchangeFinished(Colosseum *param0);
static void Colosseum_DrawPartyListWindow(Colosseum *param0, int param1);
static int Colosseum_HandleMenuInput(Colosseum *param0);
static void Colosseum_EraseMenuWindow(Colosseum *param0);
static BOOL Colosseum_SendSelectedSlot(Colosseum *param0);
static BOOL Colosseum_HasSelectedSlotExchangeFinished(Colosseum *param0);
static void Colosseum_ApplyExchangedParty(Colosseum *param0);
static void Colosseum_LoadReceivedParty(Colosseum *param0);
static int Colosseum_PrintMessage(Colosseum *param0, const String *param1);
static void Colosseum_RemoveWindows(Colosseum *param0, BOOL param1);
static void Colosseum_DrawConfirmWindow(Colosseum *param0, int param1);
static BOOL Colosseum_PeerSentParty(Colosseum *param0);

// Opens the Pokémon summary screen for one party member. `mode` is passed
// straight through to the summary screen (0 = review, 1 = pick for exchange).
static void Colosseum_OpenMonSummary(Colosseum *param0, FieldSystem *fieldSystem, Party *param2, int slot, int param4, enum HeapID heapID)
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
    PokemonSummaryScreen_SetPlayerProfile(monSummary, SaveData_GetTrainerInfo(fieldSystem->saveData));

    monSummary->dexMode = SaveData_GetDexMode(saveData);
    monSummary->showContest = PokemonSummaryScreen_ShowContestData(saveData);
    monSummary->options = SaveData_GetOptions(saveData);
    monSummary->monData = param2;
    monSummary->dataType = SUMMARY_DATA_PARTY_MON;
    monSummary->monIndex = slot;
    monSummary->monMax = Party_GetCurrentCount(monSummary->monData);
    monSummary->move = 0;
    monSummary->mode = param4;
    monSummary->specialRibbons = SaveData_GetRibbons(saveData);

    PokemonSummaryScreen_FlagVisiblePages(monSummary, visiblePages);
    FieldSystem_StartChildProcess(fieldSystem, &gPokemonSummaryScreenApp, monSummary);

    param0->monSummary = monSummary;
}

// Builds and starts the party menu in select-confirm mode. The player must
// choose exactly the number of Pokémon the battle regulation (or the default
// of three) requires, and the previous selection is restored.
static void Colosseum_OpenPartyMenu(Colosseum *param0, enum HeapID heapID)
{
    PartyMenu *partyMenu = Heap_Alloc(heapID, sizeof(PartyMenu));

    MI_CpuClear8(partyMenu, sizeof(PartyMenu));

    partyMenu->options = SaveData_GetOptions(param0->fieldSystem->saveData);
    partyMenu->battleRegulation = (void *)param0->fieldSystem->battleRegulation;
    partyMenu->party = SaveData_GetParty(param0->fieldSystem->saveData);
    partyMenu->bag = SaveData_GetBag(param0->fieldSystem->saveData);
    partyMenu->type = PARTY_MENU_TYPE_BASIC;
    partyMenu->mode = PARTY_MENU_MODE_SELECT_CONFIRM;

    if (param0->fieldSystem->battleRegulation) {
        partyMenu->minSelectionSlots = BattleRegulation_GetRuleValue(param0->fieldSystem->battleRegulation, BATTLE_REGULATION_RULE_TEAM_SIZE);
        partyMenu->maxSelectionSlots = partyMenu->minSelectionSlots;
    } else {
        partyMenu->minSelectionSlots = 3;
        partyMenu->maxSelectionSlots = 3;
    }

    partyMenu->reqLevel = 100;
    partyMenu->selectedMonSlot = param0->partyMenuSelectedSlot;

    for (int i = 0; i < 6; i++) {
        partyMenu->selectionOrder[i] = param0->selectionOrder[i];
    }

    FieldSystem_StartChildProcess(param0->fieldSystem, &gPokemonPartyAppTemplate, partyMenu);
    param0->partyMenu = partyMenu;
}

// Waits for the party menu to close, then decodes its result. selectedMonSlot
// 6 is the menu's CONFIRM button, 7 is CANCEL, and 0-5 is a specific party
// member whose summary the caller should show.
static BOOL Colosseum_FinishPartyMenu(Colosseum *param0, FieldSystem *fieldSystem)
{
    if (FieldSystem_IsRunningApplication(fieldSystem)) {
        return 0;
    }

    MI_CpuCopy8(param0->partyMenu->selectionOrder, param0->selectionOrder, 6);

    switch (param0->partyMenu->selectedMonSlot) {
    case 7:
        param0->partyMenuResult = 0;
        break;
    case 6:
        param0->partyMenuResult = 1;
        break;
    default:
        param0->partyMenuResult = 2;
        break;
    }

    param0->partyMenuSelectedSlot = param0->partyMenu->selectedMonSlot;
    Heap_Free(param0->partyMenu);
    param0->partyMenu = NULL;

    return 1;
}

// Waits for the summary screen to close and frees it, restoring the party
// slot it was showing.
static BOOL Colosseum_FinishMonSummary(Colosseum *param0, FieldSystem *fieldSystem)
{
    if (FieldSystem_IsRunningApplication(fieldSystem)) {
        return 0;
    }

    param0->partyMenuSelectedSlot = param0->monSummary->monIndex;
    Heap_Free(param0->monSummary);
    param0->monSummary = NULL;

    return 1;
}

// Once the field map is running again, fades back in and restarts the
// communication player manager. Returns TRUE when the field is ready.
static BOOL Colosseum_TryResumeField(Colosseum *param0)
{
    if (FieldSystem_IsRunningFieldMap(param0->fieldSystem)) {
        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
        CommPlayerMan_Restart();
        return 1;
    }

    return 0;
}

// TRUE if any connected player has reached sync 94, which marks a player who
// has left the battle room.
static BOOL Colosseum_AnyPlayerLeftRoom(void)
{
    int i;
    int v1 = CommSys_ConnectedCount();

    for (i = 0; i < v1; i++) {
        if (CommTool_GetSyncNo(i) == 94) {
            return 1;
        }
    }

    return 0;
}

// FieldTask state machine for the battle-setup sequence. See the state groups
// in the module overview above.
static BOOL Colosseum_BattleTask(FieldTask *param0)
{
    Colosseum *v0 = FieldTask_GetEnv(param0);
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);

    switch (v0->state) {
    case 0: // wait a moment, then face the battle direction
        v0->timer--;

        if (v0->timer == 0) {
            v0->state = 1;
            CommPlayer_SetBattleDir();
        }
        break;
    case 1:
        // "Please wait a moment... Press the B Button to cancel."
        MessageLoader_GetString(v0->msgLoader, 1, v0->message);
        v0->printerID = Colosseum_PrintMessage(v0, v0->message);
        v0->state = 2;
        break;
    case 2:
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            CommTiming_StartSync(93);
            v0->state = 3;
        }
        break;
    case 3:
        if (CommTiming_IsSyncState(93)) {
            // Everyone is ready: start the battle.
            v0->state = 7;
            MapObjectMan_StopAllMovement(v0->fieldSystem->mapObjMan);
            v0->startBattleCallback(1, v0->exchangeParty);
        } else if (gSystem.pressedKeys & PAD_BUTTON_B) {
            // The player pressed B: tell the others we are canceling.
            v0->state = 4;
            CommTiming_StartSync(92);
            v0->timer = 5;
        }
        break;
    case 4:
        if (CommTiming_IsSyncState(93)) {
            v0->state = 7;
            MapObjectMan_StopAllMovement(v0->fieldSystem->mapObjMan);
            v0->startBattleCallback(1, v0->exchangeParty);
        }

        v0->timer--;

        if (v0->timer == 0) {
            v0->state = 8;
        }
        break;
    case 7: // battle is starting: tear down and finish the task
        Colosseum_FreeResources(v0);
        Heap_Free(v0);
        return 1;
    case 5: // battle canceled: tear down and hand control back to the field
        Colosseum_FreeResources(v0);
        Heap_Free(v0);
        CommPlayerMan_ResumeFieldSystem();
        return 1;
    case 8: // tell the others whether the cancellation was agreed
        if (CommTiming_IsSyncState(93)) {
            v0->state = 5;
            v0->startBattleCallback(1, v0->exchangeParty);
        } else {
            v0->startBattleCallback(0, v0->exchangeParty);
            v0->state = 5;
        }
        break;
    case 9: // multi battle / battle regulation: pause, then choose a party
        v0->state = 10;
        v0->delayTimer = 5;
        break;
    case 10: // wait for the battle-direction animation to finish
        if (v0->delayTimer != 0) {
            v0->delayTimer--;
        } else {
            if (LocalMapObj_CheckAnimationFinished(PlayerAvatar_GetMapObject(fieldSystem->playerAvatar))) {
                v0->state = 11;
            }
        }
        break;
    case 11: // "Please choose the Pokémon to be entered."
        CommPlayer_SetBattleDir();
        MessageLoader_GetString(v0->msgLoader, 13, v0->message);

        v0->printerID = Colosseum_PrintMessage(v0, v0->message);
        v0->state = 12;
        break;
    case 12:
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            v0->state = 13;
        }
        break;
    case 13: // fade out to open the party menu or summary screen
        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
        v0->state = 14;
        break;
    case 14:
        v0->timer--;

        if (v0->timer == 0) {
            CommPlayer_SetBattleDir();
            v0->state = 15;
        }
        break;
    case 15: // open the party menu
        Colosseum_RemoveWindows(v0, 0);
        Colosseum_OpenPartyMenu(v0, HEAP_ID_FIELD2);
        v0->state = 16;
        break;
    case 16: // decode the party menu result
        if (Colosseum_FinishPartyMenu(v0, v0->fieldSystem)) {
            switch (v0->partyMenuResult) {
            case 0:
                v0->state = 20;
                break;
            case 1:
                v0->state = 19;
                break;
            case 2:
                v0->state = 17;
                break;
            }
        }
        break;
    case 17: // the player picked a member: show its summary, then return to the menu
        Colosseum_OpenMonSummary(v0, v0->fieldSystem, SaveData_GetParty(v0->fieldSystem->saveData), v0->partyMenuSelectedSlot, 0, HEAP_ID_FIELD2);
        v0->state = 18;
        break;
    case 18:
        if (Colosseum_FinishMonSummary(v0, v0->fieldSystem)) {
            v0->state = 15;
        }
        break;
    case 19: // confirmed: hand the chosen party to the field and continue
        FieldSystem_StartFieldMap(v0->fieldSystem);

        if (v0->commType != 3) {
            v0->timer = 5;
            v0->state = 21;
        } else {
            v0->state = 26;
        }
        break;
    case 21:
        CommManager_SetParty(v0->selectionOrder);

        if (Colosseum_TryResumeField(v0)) {
            v0->timer = 5;
            v0->state = 0;
        }
        break;
    case 20: // canceled: start the field map so we can back out
        FieldSystem_StartFieldMap(v0->fieldSystem);

        if (v0->commType != 3) {
            v0->state = 22;
        } else {
            v0->state = 26;
        }
        break;
    case 22:
        if (Colosseum_TryResumeField(v0)) {
            v0->state = 8;
        }
        break;
    case 23: // Mix Battle: pause, then ask for the participants
        v0->delayTimer--;

        if (v0->delayTimer == 0) {
            v0->state = 24;
        }
        break;
    case 24: // "Please select the three Pokémon that will participate."
        CommPlayer_SetBattleDir();
        MessageLoader_GetString(v0->msgLoader, 19, v0->message);
        v0->printerID = Colosseum_PrintMessage(v0, v0->message);
        v0->state = 25;
        break;
    case 25:
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            v0->state = 13;
        }
        break;
    case 26: // Mix Battle: build our exchange data and announce we are waiting
        if (Colosseum_TryResumeField(v0)) {
            if (Colosseum_AnyPlayerLeftRoom()) {
                v0->state = 5;
            } else {
                v0->wantsMixBattle = (v0->partyMenuResult != 0);
                Colosseum_BuildPartyExchangeData(v0, v0->wantsMixBattle);
                CommTiming_StartSync(0);
                StringTemplate_SetPlayerName(v0->strTemplate, 0, v0->peerTrainerInfo);
                MessageLoader_GetString(v0->msgLoader, 14, v0->message);
                StringTemplate_Format(v0->strTemplate, v0->formattedMessage, v0->message);
                v0->printerID = Colosseum_PrintMessage(v0, v0->formattedMessage);
                v0->state = 27;
            }
        }
        break;
    case 27: // send our party once every player has reached sync 0
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            if (Colosseum_AnyPlayerLeftRoom()) {
                v0->state = 5;
            } else if (CommTiming_IsSyncState(0)) {
                Colosseum_SendPartyExchangeData(v0);
                v0->state = 28;
            }
        }
        break;
    case 28: // wait for our party and the peer's party to be exchanged
        if (Colosseum_HasPartyExchangeFinished(v0)) {
            CommTiming_StartSync(1);
            v0->state = 29;
        }
        break;
    case 29: // decide whether both sides want a Mix Battle
        if (CommTiming_IsSyncState(1)) {
            v0->peerWantsMixBattle = Colosseum_PeerSentParty(v0);

            if (v0->wantsMixBattle && v0->peerWantsMixBattle) {
                MessageLoader_GetString(v0->msgLoader, 20, v0->message);
                v0->printerID = Colosseum_PrintMessage(v0, v0->message);
                v0->state = 30;
            } else {
                v0->state = 42;
            }
        }
        break;
    case 30: // explain Mix Battles, then start picking the opponent's Pokémon
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            v0->exchangeFlags = 0;
            MessageLoader_GetString(v0->msgLoader, 17, v0->message);
            v0->printerID = Colosseum_PrintMessage(v0, v0->message);
            v0->selectedOpponentSlot = 0;
            v0->state = 31;
        }
        break;
    case 31: // draw the peer's party list
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            Colosseum_LoadReceivedParty(v0);
            Colosseum_DrawPartyListWindow(v0, v0->selectedOpponentSlot);
            v0->state = 32;
        }
        break;
    case 32: // handle the party list: 1 = picked a Pokémon, 2 = cancel
        switch (Colosseum_HandleMenuInput(v0)) {
        case 1:
            Colosseum_EraseMenuWindow(v0);
            v0->selectedOpponentSlot = v0->menuCursor;
            v0->state = 36;
            break;
        case 2:
            Colosseum_EraseMenuWindow(v0);
            v0->selectedOpponentSlot = 255;
            MessageLoader_GetString(v0->msgLoader, 15, v0->message);
            v0->printerID = Colosseum_PrintMessage(v0, v0->message);
            CommTiming_StartSync(2);
            v0->state = 39;
            break;
        }
        break;
    case 36: // confirm the chosen opponent Pokémon
        StringTemplate_SetSpeciesName(v0->strTemplate, 1, Pokemon_GetBoxPokemon(Party_GetPokemonBySlotIndex(v0->exchangeParty, v0->selectedOpponentSlot)));
        MessageLoader_GetString(v0->msgLoader, 18, v0->message);
        StringTemplate_Format(v0->strTemplate, v0->formattedMessage, v0->message);
        v0->printerID = Colosseum_PrintMessage(v0, v0->formattedMessage);
        v0->state = 37;
        break;
    case 37: // draw the Summary/OK/Cancel menu
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            Colosseum_DrawConfirmWindow(v0, 0);
            v0->state = 38;
        }
        break;
    case 38: // confirm menu: 1 = Summary/OK, 2 = Cancel/B
        switch (Colosseum_HandleMenuInput(v0)) {
        case 2:
            Colosseum_EraseMenuWindow(v0);
            MessageLoader_GetString(v0->msgLoader, 17, v0->message);
            v0->printerID = Colosseum_PrintMessage(v0, v0->message);
            v0->state = 31;
            break;
        case 1:
            if (v0->menuCursor == 1) {
                Colosseum_EraseMenuWindow(v0);
                MessageLoader_GetString(v0->msgLoader, 14, v0->message);
                StringTemplate_Format(v0->strTemplate, v0->formattedMessage, v0->message);
                v0->printerID = Colosseum_PrintMessage(v0, v0->formattedMessage);
                CommTiming_StartSync(2);
                v0->state = 39;
            } else {
                FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
                v0->state = 33;
            }
            break;
        }
        break;
    case 33: // show the summary of the opponent's chosen Pokémon
        if (IsScreenFadeDone()) {
            Colosseum_RemoveWindows(v0, 0);
            Colosseum_OpenMonSummary(v0, v0->fieldSystem, v0->exchangeParty, v0->selectedOpponentSlot, 1, HEAP_ID_FIELD2);
            v0->state = 34;
        }
        break;
    case 34:
        if (Colosseum_FinishMonSummary(v0, v0->fieldSystem)) {
            FieldSystem_StartFieldMap(v0->fieldSystem);
            v0->state = 35;
        }
        break;
    case 35:
        if (Colosseum_TryResumeField(v0)) {
            v0->state = 36;
        }
        break;
    case 39: // once the sync is met, send our chosen slot
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            if (CommTiming_IsSyncState(2)) {
                Colosseum_SendSelectedSlot(v0);
                v0->state = 41;
            }
        }
        break;
    case 41: // when both slots are known, swap the Pokémon and go back to the ready check
        if (Colosseum_HasSelectedSlotExchangeFinished(v0)) {
            if (v0->selectedOpponentSlot == 255) {
                CommTiming_StartSync(4);
                v0->state = 44;
            } else if (v0->peerSelectedSlot == 255) {
                v0->state = 42;
            } else {
                Colosseum_ApplyExchangedParty(v0);
                CommTiming_StartSync(93);
                v0->state = 2;
            }
        }
        break;
    case 42: // "The battle has been canceled."
        MessageLoader_GetString(v0->msgLoader, 15, v0->message);
        v0->printerID = Colosseum_PrintMessage(v0, v0->message);
        v0->timer = 0;
        v0->state = 43;
        break;
    case 43: // hold the cancellation message for 60 frames
        if (FieldMessage_FinishedPrinting(v0->printerID)) {
            if (++(v0->timer) > 60) {
                CommTiming_StartSync(4);
                v0->state = 44;
            }
        }
        break;
    case 44: // cancellation sync complete: exit and hand control back
        if (CommTiming_IsSyncState(4)) {
            Window_EraseMessageBox(&(v0->messageWindow), 0);
            v0->startBattleCallback(0, NULL);
            v0->state = 5;
        }
        break;
    }

    return 0;
}

// Prints a message in the module's message window, creating the window on
// first use. Returns the text printer ID.
static int Colosseum_PrintMessage(Colosseum *param0, const String *param1)
{
    Window *v0 = &(param0->messageWindow);

    if (Window_IsInUse(v0) == 0) {
        FieldMessage_AddWindow(param0->fieldSystem->bgConfig, v0, 3);
        FieldMessage_DrawWindow(v0, SaveData_GetOptions(param0->fieldSystem->saveData));
    } else {
        FieldMessage_ClearWindow(v0);
    }

    return FieldMessage_Print(v0, (String *)param1, SaveData_GetOptions(param0->fieldSystem->saveData), 1);
}

// Removes and re-initialises the module's windows. When `erase` is TRUE the
// message box is also erased from VRAM.
static void Colosseum_RemoveWindows(Colosseum *param0, BOOL param1)
{
    if (Window_IsInUse(&(param0->messageWindow))) {
        if (param1) {
            Window_EraseMessageBox(&param0->messageWindow, 0);
            Window_ClearAndCopyToVRAM(&param0->messageWindow);
        }

        Window_Remove(&param0->messageWindow);
        Window_Init(&param0->messageWindow);
    }

    if (Window_IsInUse(&(param0->partyListWindow))) {
        Window_Remove(&param0->partyListWindow);
        Window_Init(&param0->partyListWindow);
    }

    if (Window_IsInUse(&(param0->confirmWindow))) {
        Window_Remove(&param0->confirmWindow);
        Window_Init(&param0->confirmWindow);
    }
}

// Entry point for the battle-setup sequence. Called by the field communication
// manager when a player is waiting in the Colosseum. `callback` is invoked with
// TRUE to start the battle or FALSE to cancel it. Does nothing if a field task
// is already running.
void Colosseum_StartBattle(FieldSystem *fieldSystem, UnkFuncPtr_0205AB10 *param1)
{
    Colosseum *v0;
    FieldTask *v1 = fieldSystem->task;

    if (v1) {
        return;
    }

    v0 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(Colosseum));
    MI_CpuClear8(v0, sizeof(Colosseum));

    v0->timer = 5;
    v0->fieldSystem = fieldSystem;
    v0->startBattleCallback = param1;
    v0->strTemplate = StringTemplate_Default(HEAP_ID_FIELD2);
    v0->msgLoader = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_COMMUNICATION_CLUB, HEAP_ID_FIELD2);
    v0->message = String_Init(100 * 2, HEAP_ID_FIELD2);
    v0->formattedMessage = String_Init(100 * 2, HEAP_ID_FIELD2);

    Window_Init(&v0->messageWindow);
    Window_Init(&v0->partyListWindow);
    Window_Init(&v0->confirmWindow);

    v0->arrow = ColoredArrow_New(HEAP_ID_FIELD2);
    v0->commType = CommManager_GetCommType();
    v0->partyExchangeSend = NULL;
    v0->partyExchangeRecv = NULL;
    v0->exchangeParty = NULL;
    v0->exchangeFlags = 0;
    v0->netId = CommSys_CurNetId();
    v0->peerTrainerInfo = CommInfo_TrainerInfo(v0->netId ^ 1);

    switch (v0->commType) {
    case 3: { // COMM_TYPE_MIX_BATTLE: allocate the party-exchange buffers
        u32 v2 = Colosseum_PartyExchangeSize();

        v0->partyExchangeSend = Heap_AllocAtEnd(HEAP_ID_FIELD2, v2);
        v0->partyExchangeRecv = Heap_AllocAtEnd(HEAP_ID_FIELD2, v2);
        v0->exchangeParty = Party_New(HEAP_ID_FIELD2);

        Party_InitWithCapacity(v0->exchangeParty, 3);

        v0->delayTimer = 5;
        v0->state = 23;
    } break;
    case 4: // COMM_TYPE_MULTI_BATTLE_1: choose a party first
        v0->state = 9;
        break;
    default:
        if (v0->fieldSystem->battleRegulation) {
            v0->state = 9; // battle regulation: choose a party first
        } else {
            v0->state = 0; // plain battle: wait for everyone to be ready
        }
        break;
    }

    FieldSystem_CreateTask(fieldSystem, Colosseum_BattleTask, v0);
}

// Frees everything allocated by Colosseum_StartBattle.
static void Colosseum_FreeResources(Colosseum *param0)
{
    if (param0->exchangeParty) {
        Heap_Free(param0->exchangeParty);
    }

    if (param0->partyExchangeSend) {
        Heap_Free(param0->partyExchangeSend);
    }

    if (param0->partyExchangeRecv) {
        Heap_Free(param0->partyExchangeRecv);
    }

    MessageLoader_Free(param0->msgLoader);
    StringTemplate_Free(param0->strTemplate);
    String_Free(param0->message);
    String_Free(param0->formattedMessage);
    ColoredArrow_Free(param0->arrow);

    Colosseum_RemoveWindows(param0, 1);
}

// Returns the Colosseum state attached to the field task.
static Colosseum *Colosseum_GetTaskEnv(FieldSystem *fieldSystem)
{
    return FieldTask_GetEnv(fieldSystem->task);
}

// Fills the outgoing exchange buffer with the three chosen Pokémon (in
// selection order) followed by a PartyExchangeTail. When `includeParty` is
// FALSE the Pokémon are omitted and only the tail is written.
static void Colosseum_BuildPartyExchangeData(Colosseum *param0, BOOL param1)
{
    Party *v0;
    PartyExchangeTail *v1;
    u8 *v2;
    int v3, v4;

    v0 = SaveData_GetParty(param0->fieldSystem->saveData);
    v2 = param0->partyExchangeSend;
    v4 = Pokemon_GetStructSize();
    v1 = (PartyExchangeTail *)(v2 + v4 * 3);
    v1->hasParty = param1;

    if (v1->hasParty) {
        for (v3 = 0; v3 < 3; v3++) {
            MI_CpuCopy8(Party_GetPokemonBySlotIndex(v0, param0->selectionOrder[v3] - 1), v2, v4);
            v2 += v4;
        }
    }
}

// Sends the outgoing exchange buffer as comm command 106. Returns TRUE once
// the send has been queued; the sent bit is remembered in exchangeFlags.
static BOOL Colosseum_SendPartyExchangeData(Colosseum *param0)
{
    if (param0->exchangeFlags & 1) {
        return 1;
    } else {
        BOOL v0;
        u8 *v1;
        u32 v2;

        v1 = param0->partyExchangeSend;
        v2 = Colosseum_PartyExchangeSize();

        if (param0->netId == 0) {
            v0 = CommSys_SendDataHugeServer(106, v1, v2);
        } else {
            v0 = CommSys_SendDataHuge(106, v1, v2);
        }

        if (v0) {
            param0->exchangeFlags |= 1;
        }

        return v0;
    }
}

// TRUE once the party exchange has both sent our data and received the peer's.
static BOOL Colosseum_HasPartyExchangeFinished(Colosseum *param0)
{
    if (param0->exchangeFlags == 3) {
        return 1;
    }

    return 0;
}

// Reads the PartyExchangeTail of the received buffer to learn whether the peer
// included a party.
static BOOL Colosseum_PeerSentParty(Colosseum *param0)
{
    PartyExchangeTail *v0 = (PartyExchangeTail *)((u8 *)(param0->partyExchangeRecv) + (Pokemon_GetStructSize() * 3));
    return v0->hasParty;
}

// Sends the selected opponent party slot as comm command 107.
static BOOL Colosseum_SendSelectedSlot(Colosseum *param0)
{
    BOOL v0;

    if (param0->netId == 0) {
        v0 = CommSys_SendDataServer(107, &(param0->selectedOpponentSlot), 1);
    } else {
        v0 = CommSys_SendData(107, &(param0->selectedOpponentSlot), 1);
    }

    if (v0) {
        param0->exchangeFlags |= 1;
    }

    return v0;
}

// TRUE once the selected-slot exchange has both sent and received.
static BOOL Colosseum_HasSelectedSlotExchangeFinished(Colosseum *param0)
{
    if (param0->exchangeFlags == 3) {
        return 1;
    }

    return 0;
}

// Rebuilds exchangeParty from the three Pokémon in the received buffer.
static void Colosseum_LoadReceivedParty(Colosseum *param0)
{
    u32 v0;
    int v1;

    v0 = Pokemon_GetStructSize();
    Party_InitWithCapacity(param0->exchangeParty, 3);

    for (v1 = 0; v1 < 3; v1++) {
        Party_AddPokemon(param0->exchangeParty, (Pokemon *)(&param0->partyExchangeRecv[v1 * v0]));
    }
}

// Copies the peer's chosen Pokémon into our outgoing buffer in place of the
// one we gave up, then rebuilds exchangeParty from the result.
static void Colosseum_ApplyExchangedParty(Colosseum *param0)
{
    u32 v0;
    u8 *v1, *v2;
    int v3;

    v0 = Pokemon_GetStructSize();
    v1 = &param0->partyExchangeSend[param0->peerSelectedSlot * v0];
    v2 = &param0->partyExchangeRecv[param0->selectedOpponentSlot * v0];

    MI_CpuCopy8(v2, v1, v0);

    Party_InitWithCapacity(param0->exchangeParty, 3);

    for (v3 = 0; v3 < 3; v3++) {
        Party_AddPokemon(param0->exchangeParty, (Pokemon *)(&param0->partyExchangeSend[v3 * v0]));
    }
}

// Draws the window listing the peer's three Pokémon plus a CANCEL entry, with
// the cursor on `cursor`.
static void Colosseum_DrawPartyListWindow(Colosseum *param0, int param1)
{
    Window *v0 = &(param0->partyListWindow);

    if (Window_IsInUse(v0) == 0) {
        int v1, v2, v3;
        MessageLoader *v4;

        v4 = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_SPECIES_NAME, HEAP_ID_FIELD1);
        v3 = Pokemon_GetStructSize();

        Window_Add(param0->fieldSystem->bgConfig, v0, 3, 21, 9, 10, 8, 13, 10);
        LoadStandardWindowGraphics(param0->fieldSystem->bgConfig, 3, 1, 11, 0, HEAP_ID_FIELD1);
        Window_FillTilemap(v0, 15);

        for (v1 = 0; v1 < 3; v1++) {
            v2 = Pokemon_GetValue((Pokemon *)(&param0->partyExchangeRecv[v1 * v3]), MON_DATA_SPECIES, NULL);

            MessageLoader_GetString(v4, v2, param0->message);
            Text_AddPrinterWithParams(v0, FONT_SYSTEM, param0->message, 16, v1 * 16, TEXT_SPEED_NO_TRANSFER, NULL);
        }

        MessageLoader_GetString(param0->msgLoader, 21, param0->message); // "CANCEL"
        Text_AddPrinterWithParams(v0, FONT_SYSTEM, param0->message, 16, v1 * 16, TEXT_SPEED_NO_TRANSFER, NULL);
        MessageLoader_Free(v4);
    }

    Window_FillRectWithColor(v0, 15, 0, 0, 16, v0->height * 8);
    ColoredArrow_Print(param0->arrow, &param0->partyListWindow, 0, param1 * 16);
    Window_DrawStandardFrame(&param0->partyListWindow, 0, 1, 11);

    param0->menuCursor = param1;
    param0->menuItemCount = 3 + 1;
    param0->menuWindow = v0;
}

// Draws the Summary/OK/Cancel window with the cursor on `cursor`.
static void Colosseum_DrawConfirmWindow(Colosseum *param0, int param1)
{
    Window *v0 = &(param0->confirmWindow);

    if (Window_IsInUse(v0) == 0) {
        int v1;

        Window_Add(param0->fieldSystem->bgConfig, v0, 3, 20, 11, 11, 6, 13, 90);
        LoadStandardWindowGraphics(param0->fieldSystem->bgConfig, 3, 1, 11, 0, HEAP_ID_FIELD1);
        Window_FillTilemap(v0, 15);

        for (v1 = 0; v1 < 3; v1++) {
            // Messages 22-24 are "SUMMARY", "OK" and "CANCEL".
            MessageLoader_GetString(param0->msgLoader, 22 + v1, param0->message);
            Text_AddPrinterWithParams(v0, FONT_SYSTEM, param0->message, 16, v1 * 16, TEXT_SPEED_NO_TRANSFER, NULL);
        }
    }

    param0->menuItemCount = 3;
    param0->menuWindow = v0;
    param0->menuCursor = param1;

    Window_FillRectWithColor(v0, 15, 0, 0, 16, v0->height * 8);
    ColoredArrow_Print(param0->arrow, param0->menuWindow, 0, param1 * 16);
    Window_DrawStandardFrame(param0->menuWindow, 0, 1, 11);
}

// Polls input for the currently displayed menu. Returns 1 when a non-last
// entry is confirmed, 2 when the last entry or B is pressed, and 0 otherwise.
// Moving the cursor redraws the arrow.
static int Colosseum_HandleMenuInput(Colosseum *param0)
{
    do {
        if (gSystem.pressedKeys & PAD_KEY_UP) {
            param0->menuCursor = ((param0->menuCursor == 0) ? (param0->menuItemCount - 1) : (param0->menuCursor - 1));
            break;
        }

        if (gSystem.pressedKeys & PAD_KEY_DOWN) {
            param0->menuCursor = (param0->menuCursor == (param0->menuItemCount - 1)) ? 0 : (param0->menuCursor + 1);
            break;
        }

        if (gSystem.pressedKeys & PAD_BUTTON_A) {
            Sound_PlayEffect(SE_CONFIRM_sseq_3);

            if (param0->menuCursor < (param0->menuItemCount - 1)) {
                return 1;
            } else {
                return 2;
            }
        }

        if (gSystem.pressedKeys & PAD_BUTTON_B) {
            Sound_PlayEffect(SE_CONFIRM_sseq_3);
            return 2;
        }

        return 0;
    } while (0);

    Sound_PlayEffect(SE_CONFIRM_sseq_3);
    Window_FillRectWithColor(param0->menuWindow, 15, 0, 0, 16, param0->menuWindow->height * 8);
    ColoredArrow_Print(param0->arrow, param0->menuWindow, 0, param0->menuCursor * 16);
    Window_LoadTiles(param0->menuWindow);

    return 0;
}

// Erases the standard frame of the active menu window.
static void Colosseum_EraseMenuWindow(Colosseum *param0)
{
    Window_EraseStandardFrame(param0->menuWindow, 1);
}

// Comm command 106 receive handler: marks that the peer's party arrived.
void Colosseum_HandleReceivedParty(int param0, int param1, void *param2, void *param3)
{
    Colosseum *v0 = Colosseum_GetTaskEnv(param3);

    if (v0->netId != param0) {
        v0->exchangeFlags |= 2;
    }
}

// Size of the party-exchange buffer: three Pokémon plus the tail.
int Colosseum_PartyExchangeSize(void)
{
    return Pokemon_GetStructSize() * 3 + sizeof(PartyExchangeTail);
}

// Comm command 106 buffer provider: returns the buffer the peer's party should
// be written into, or NULL for our own echo.
u8 *Colosseum_GetPartyExchangeBuffer(int param0, void *param1, int param2)
{
    Colosseum *v0 = Colosseum_GetTaskEnv(param1);

    if (v0->netId != param0) {
        return v0->partyExchangeRecv;
    } else {
        return NULL;
    }
}

// Comm command 107 receive handler: stores the peer's selected party slot.
void Colosseum_HandleReceivedSlot(int param0, int param1, void *param2, void *param3)
{
    Colosseum *v0 = Colosseum_GetTaskEnv(param3);

    if (v0->netId != param0) {
        v0->peerSelectedSlot = *((u8 *)param2);
        v0->exchangeFlags |= 2;
    }
}

// Task that announces and then opens another player's Trainer Case. It starts
// by printing the card-tier message ("...It's a Bronze Card!"), fades out,
// opens the Trainer Case application, and finally fades back into the field.
static BOOL Colosseum_TrainerCaseTask(FieldTask *param0)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    TrainerCaseViewer *v1 = FieldTask_GetEnv(param0);
    TrainerCase *trainerCase = (TrainerCase *)FieldCommManager_GetTrainerCase(v1->netId, NULL, 0);

    switch (v1->state) {
    case 0:
        v1->strTemplate = StringTemplate_Default(HEAP_ID_FIELD1);
        v1->msgLoader = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_COMMUNICATION_CLUB, HEAP_ID_FIELD1);
        v1->message = String_Init(100 * 2, HEAP_ID_FIELD1);
        v1->formattedMessage = String_Init(100 * 2, HEAP_ID_FIELD1);

        // Messages 2-7 are the six Trainer Case card tiers, selected by level.
        MessageLoader_GetString(v1->msgLoader, 2 + trainerCase->cardLevel, v1->message);
        StringTemplate_SetPlayerName(v1->strTemplate, 0, CommInfo_TrainerInfo(v1->netId));
        StringTemplate_Format(v1->strTemplate, v1->formattedMessage, v1->message);
        FieldMessage_AddWindow(fieldSystem->bgConfig, &v1->window, 3);
        FieldMessage_DrawWindow(&v1->window, SaveData_GetOptions(fieldSystem->saveData));

        v1->printerID = FieldMessage_Print(&v1->window, v1->formattedMessage, SaveData_GetOptions(fieldSystem->saveData), 1);
        v1->state++;
        break;
    case 1:
        if (FieldMessage_FinishedPrinting(v1->printerID)) {
            if (gSystem.pressedKeys & PAD_BUTTON_A) {
                MessageLoader_Free(v1->msgLoader);
                StringTemplate_Free(v1->strTemplate);
                String_Free(v1->message);
                String_Free(v1->formattedMessage);
                Window_EraseMessageBox(&v1->window, 0);
                Window_Remove(&v1->window);
                FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
                v1->state++;
            }
        }
        break;
    case 2:
        if (IsScreenFadeDone()) {
            v1->state++;
        }
        break;
    case 3:
        FieldSystem_OpenTrainerCase(fieldSystem, trainerCase);
        v1->state++;
        break;
    case 4:
        if (!FieldSystem_IsRunningApplication(fieldSystem)) {
            v1->state++;
        }
        break;
    case 5:
        FieldSystem_StartFieldMap(fieldSystem);
        v1->state++;
        break;
    case 6:
        if (!FieldSystem_IsRunningFieldMap(fieldSystem)) {
            FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
            CommPlayerMan_Restart();
            v1->state++;
        }
        break;
    case 7:
        CommPlayerMan_ResumeFieldSystem();
        Heap_Free(v1);
        return 1;
    default:
        return 1;
    }

    return 0;
}

// Called when the player presses the interact button in the Colosseum. If
// another connected player is standing on the tile directly in front of the
// player, starts the Trainer Case viewer task for them.
void Colosseum_ViewTrainerCase(FieldSystem *fieldSystem)
{
    int v0;
    int v1 = CommSys_CurNetId();
    int v2 = CommPlayer_GetXInFrontOfPlayer(v1);
    int v3 = CommPlayer_GetZInFrontOfPlayer(v1);

    for (v0 = 0; v0 < CommSys_ConnectedCount(); v0++) {
        if (v0 == v1) {
            continue;
        }

        if ((v2 == CommPlayer_GetXIfActive(v0)) && (v3 == CommPlayer_GetZIfActive(v0))) {
            TrainerCaseViewer *v4 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(TrainerCaseViewer));

            v4->netId = v0;
            v4->state = 0;

            FieldSystem_CreateTask(fieldSystem, Colosseum_TrainerCaseTask, v4);
            FieldSystem_PauseProcessing();
            break;
        }
    }
}
