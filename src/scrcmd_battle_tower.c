#include "scrcmd_battle_tower.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle_tower.h"
#include "constants/battle_tower_functions.h"
#include "generated/battle_tower_modes.h"
#include "generated/game_records.h"
#include "generated/items.h"
#include "generated/object_events_gfx.h"

#include "struct_defs/battle_tower.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "field/field_system.h"
#include "overlay005/field_menu.h"

#include "battle_frontier_stats.h"
#include "bg_window.h"
#include "communication_system.h"
#include "field_script_context.h"
#include "field_system.h"
#include "game_records.h"
#include "inlines.h"
#include "savedata.h"
#include "script_manager.h"
#include "trainer_info.h"
#include "unk_020363E8.h"
#include "battle_tower.h"
#include "unk_0204AEE8.h"
#include "unk_0206B9D8.h"
#include "unk_0209BA80.h"
#include "wifi_battle_tower_save.h"

// Battle Tower script commands. These ScrCmd_* handlers are the scripting front
// end for the BattleTower state owned by battle_tower.c: they create and tear
// down the state, query its data, exchange data with a link partner, and manage
// Battle Points. The BattleTower struct itself is defined in
// struct_defs/battle_tower.h.

static u16 BattleTower_GetPartnerParam(BattleTower *battleTower, u8 param1);

BOOL ScrCmd_InitBattleTower(ScriptContext *ctx)
{
    u16 resumeFlag = ScriptContext_ReadHalfWord(ctx);
    u16 challengeMode = ScriptContext_ReadHalfWord(ctx);

    // resumeFlag == 0 starts a fresh challenge; non-zero resumes the saved one.
    ctx->fieldSystem->battleTower = BattleTower_Init(FieldSystem_GetSaveData(ctx->fieldSystem), resumeFlag, challengeMode);
    return FALSE;
}

BOOL ScrCmd_SetBattleTowerNull(ScriptContext *ctx)
{
    BattleTower_SetNull(&(ctx->fieldSystem->battleTower));
    return FALSE;
}

BOOL ScrCmd_FreeBattleTower(ScriptContext *ctx)
{
    BattleTower_Free(ctx->fieldSystem->battleTower);
    ctx->fieldSystem->battleTower = NULL;

    return FALSE;
}

BOOL ScrCmd_CallBattleTowerFunction(ScriptContext *ctx)
{
    void **partyMenu;

    u16 functionIndex = ScriptContext_ReadHalfWord(ctx);
    u16 functionArgument = ScriptContext_GetVar(ctx);
    u16 varID = ScriptContext_ReadHalfWord(ctx);
    u16 *destVar = FieldSystem_GetVarPointer(ctx->fieldSystem, varID);
    BattleTower *battleTower = ctx->fieldSystem->battleTower;

    // Dispatch on the BT_FUNC_* index; the result (if any) is written to destVar.
    switch (functionIndex) {
    case BT_FUNC_CHECK_ENOUGH_VALID_POKEMON: // enough pokemon?
        if (functionArgument == 0) {
            *destVar = BattleTower_HasEnoughValidPokemon(battleTower->partySize, ctx->fieldSystem->saveData, 1);
        } else {
            *destVar = BattleTower_HasEnoughValidPokemon(functionArgument, ctx->fieldSystem->saveData, 1);
        }
        break;
    case BT_FUNC_RESET_SYSTEM:
        BattleTower_ResetSystem();
        break;
    case BT_FUNC_UNK_03:
        // Reset / query the in-progress WiFi Battle Tower save.
        BattleTower_InitWifiSave(SaveData_GetWifiBattleTowerSave(ctx->fieldSystem->saveData));
        break;
    case BT_FUNC_UNK_04:
        *destVar = BattleTower_IsWifiChallengeInProgress(SaveData_GetWifiBattleTowerSave(ctx->fieldSystem->saveData));
        break;
    case BT_FUNC_SET_COMMUNICATION_CLUB_ACCESSIBLE:
        BattleTower_SetCommunicationClubAccessible(ctx->fieldSystem);
        break;
    case BT_FUNC_CLEAR_COMMUNICATION_CLUB_ACCESSIBLE:
        BattleTower_ClearCommunicationClubAccessible(ctx->fieldSystem);
        break;
    case BT_FUNC_UNK_08:
        *destVar = BattleTower_GetLatestStreak(ctx->fieldSystem->saveData, functionArgument);
        break;
    case BT_FUNC_UNK_09:
        *destVar = BattleTower_UpdateRank(NULL, ctx->fieldSystem->saveData, 2);
        break;
    case BT_FUNC_UNK_10:
        *destVar = BattleTower_UpdateRank(NULL, ctx->fieldSystem->saveData, 0);
        break;
    case BT_FUNC_UNK_11:
        BattleTower_SetWifiResultsPending(ctx->fieldSystem->saveData, functionArgument);
        break;
    case BT_FUNC_UNK_12:
        *destVar = BattleTower_HasWifiResultsPending(ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UNK_14:
        *destVar = BattleTower_ResetWifiProgress(ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UNK_15:
        *destVar = BattleTower_HasWifiOpponentData(ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UNK_16:
        // Hand off to a field task that writes the result to destVar later.
        sub_0206BCE4(ctx->task, functionArgument, varID, *destVar);
        return TRUE;
    case BT_FUNC_UNK_30:
        // Open the party menu for the player to pick their Battle Tower party.
        partyMenu = FieldSystem_GetScriptMemberPtr(ctx->fieldSystem, SCRIPT_MANAGER_PARTY_MANAGEMENT_DATA);
        BattleTower_StartPartyMenu(battleTower, ctx->task, partyMenu);
        return TRUE;
    case BT_FUNC_UNK_31:
        partyMenu = FieldSystem_GetScriptMemberPtr(ctx->fieldSystem, SCRIPT_MANAGER_PARTY_MANAGEMENT_DATA);
        *destVar = BattleTower_ReadPartyMenuSelection(battleTower, partyMenu, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_CHECK_DUPLICATE_SPECIES_AND_HELD_ITEMS:
        *destVar = BattleTower_CheckDuplicateSpeciesAndHeldItems(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_HAS_DEFEATED_SEVEN_TRAINERS:
        *destVar = BattleTower_HasDefeatedSevenTrainers(battleTower);
        break;
    case BT_FUNC_UPDATE_GAME_RECORDS:
        BattleTower_UpdateGameRecords(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UPDATE_GAME_RECORDS_AND_JOURNAL:
        BattleTower_UpdateGameRecordsAndJournal(battleTower, ctx->fieldSystem->saveData, ctx->fieldSystem->journalEntry);
        break;
    case BT_FUNC_UNK_39:
        BattleTower_SaveWifiState(battleTower);
        break;
    case BT_FUNC_UNK_56:
        BattleTower_BuildPartnerData(battleTower);
        break;
    case BT_FUNC_GET_OPPONENT_OBJECT_ID:
        *destVar = BattleTower_GetObjectIDFromOpponentID(battleTower, functionArgument);
        break;
    case BT_FUNC_GET_CHALLENGE_MODE:
        *destVar = (u16)BattleTower_GetChallengeMode(battleTower);
        break;
    case BT_FUNC_GET_BEAT_PALMER:
        *destVar = BattleTower_GetBeatPalmer(battleTower);
        break;
    case BT_FUNC_UNK_47:
        BattleTower_IsPalmerBattleAvailable(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UNK_48:
        *destVar = BattleTower_GivePalmerRibbon(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_UNK_49:
        *destVar = BattleTower_GiveModeAbilityRibbon(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_SET_PARTNER_ID:
        battleTower->partnerID = functionArgument;
        break;
    case BT_FUNC_GET_PARTNER_ID:
        *destVar = battleTower->partnerID;
        break;
    case BT_FUNC_UNK_52:
        BattleTower_GenerateOpponentTrainerIDs(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_GET_SLOT_INDEX:
        *destVar = battleTower->unk_2A[functionArgument];
        break;
    case BT_FUNC_UNK_54:
        *destVar = BattleTower_UpdateRank(battleTower, ctx->fieldSystem->saveData, 1);
        break;
    case BT_FUNC_GET_PARTNER_PARAM:
        *destVar = BattleTower_GetPartnerParam(battleTower, functionArgument);
        break;
    case BT_FUNC_UNK_57:
        *destVar = BattleTower_UpdateRandomSeed(battleTower, ctx->fieldSystem->saveData);
        break;
    case BT_FUNC_CHECK_IS_NULL:
        if (battleTower == NULL) {
            *destVar = TRUE;
        } else {
            *destVar = FALSE;
        }
        break;
    case BT_FUNC_UNK_58:
        // Clear the 35-entry u16 comm buffer.
        MI_CpuClear8(battleTower->unk_884, 70);
        break;
    default:
        GF_ASSERT(FALSE);
        *destVar = 0;
        break;
    }

    return FALSE;
}

BOOL ScrCmd_GetBattleTowerPartnerSpeciesAndMove(ScriptContext *ctx)
{
    u16 partnerID, monID;
    u16 *destVar1, *destVar2;
    BattleTower *battleTower = ctx->fieldSystem->battleTower;

    partnerID = ScriptContext_GetVar(ctx);
    monID = ScriptContext_GetVar(ctx);
    destVar1 = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));
    destVar2 = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));

    // Expose the partner's species and first move to the script.
    *destVar1 = battleTower->partnersDataDTO[partnerID].pokemon[monID].species;
    *destVar2 = battleTower->partnersDataDTO[partnerID].pokemon[monID].moves[0];

    return FALSE;
}

BOOL ScrCmd_1DF(ScriptContext *ctx)
{
    u16 v0, v1, v2;
    u16 *v3;

    v0 = ScriptContext_ReadHalfWord(ctx);
    v3 = FieldSystem_GetVarPointer(ctx->fieldSystem, v0);
    // Result is a Battle Tower reward state (0-4) derived from the streak and
    // the Underground goods storage.
    *v3 = sub_0206BDBC(ctx->fieldSystem->saveData);

    return FALSE;
}

BOOL ScrCmd_1E0(ScriptContext *ctx)
{
    u16 v0, v1, v2;
    u16 *v3;

    v0 = ScriptContext_ReadHalfWord(ctx);
    v3 = FieldSystem_GetVarPointer(ctx->fieldSystem, v0);
    // Companion to ScrCmd_1DF; see sub_0206BF04.
    *v3 = sub_0206BF04(ctx->fieldSystem->saveData);

    return FALSE;
}

BOOL ScrCmd_1E1(ScriptContext *ctx)
{
    int cmd, packetSize;
    const TrainerInfo *v2;
    u16 commandType = ScriptContext_GetVar(ctx);
    u16 argument = ScriptContext_GetVar(ctx);
    u16 *destVar = ScriptContext_GetVarPointer(ctx);
    BattleTower *battleTower = ctx->fieldSystem->battleTower;

    *destVar = 0;

    // Build the outgoing packet for the requested command type.
    switch (commandType) {
    case 0:
        cmd = 62;
        sub_0204B060(ctx->fieldSystem->battleTower, ctx->fieldSystem->saveData);
        break;
    case 1:
        cmd = 63;
        sub_0204B0BC(ctx->fieldSystem->battleTower);
        break;
    case 2:
        cmd = 64;
        sub_0204B0D4(ctx->fieldSystem->battleTower, argument);
        break;
    }

    if (sub_0205E6D8(ctx->fieldSystem->saveData) == 1) {
        // Debug build (VERSION_NONE game code): send through the comm tool.
        if (sub_02036614(CommSys_CurNetId(), battleTower->unk_83E) == 1) {
            *destVar = 1;
        } else {
            return TRUE;
        }
    } else {
        sub_0209BA80(battleTower);

        packetSize = 70;

        if (CommSys_SendData(cmd, battleTower->unk_83E, packetSize) == 1) {
            *destVar = 1;
        }
    }

    return FALSE;
}

static BOOL BattleTower_WaitForResponse(ScriptContext *ctx);

BOOL ScrCmd_1E2(ScriptContext *ctx)
{
    u16 destVarID;
    u16 commandType;
    BattleTower *battleTower = ctx->fieldSystem->battleTower;

    commandType = ScriptContext_GetVar(ctx);
    destVarID = ScriptContext_ReadHalfWord(ctx);

    if (sub_0205E6D8(ctx->fieldSystem->saveData) == 1) {
        // Debug build: the comm tool's reply is handled by a field task.
        sub_0206BD88(ctx->fieldSystem->task, commandType, destVarID);
    } else {
        battleTower->unk_8DA = destVarID;
        battleTower->unk_8D5 = commandType;

        ScriptContext_Pause(ctx, BattleTower_WaitForResponse);
    }

    return TRUE;
}

static BOOL BattleTower_WaitForResponse(ScriptContext *ctx)
{
    u8 expectedMsgs;
    BattleTower *battleTower = ctx->fieldSystem->battleTower;
    u16 *destVar = FieldSystem_GetVarPointer(ctx->fieldSystem, battleTower->unk_8DA);

    // Command type 1 (trainer ID list) expects a single reply; the others two.
    if (battleTower->unk_8D5 == 1) {
        expectedMsgs = 1;
    } else {
        expectedMsgs = 2;
    }

    if (battleTower->msgsReceived == expectedMsgs) {
        battleTower->msgsReceived = 0;
        *destVar = battleTower->unk_8D8;

        return TRUE;
    }

    return FALSE;
}

BOOL ScrCmd_1E3(ScriptContext *ctx)
{
    WifiBattleTowerIndices indices;
    u16 *rank = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));
    u16 *opponentIdx = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));

    // Expose the downloaded WiFi match's rank and opponent index.
    WifiBattleTowerDownloadData_GetMatchIndices(SaveData_GetWifiBattleTowerDownloadData(ctx->fieldSystem->saveData), &indices);

    *rank = indices.rank;
    *opponentIdx = indices.opponentIdx;

    return FALSE;
}

BOOL ScrCmd_1E4(ScriptContext *ctx)
{
    u16 *destVar = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));

    *destVar = WifiBattleTowerDownloadData_HasMatchListData(SaveData_GetWifiBattleTowerDownloadData(ctx->fieldSystem->saveData));
    return FALSE;
}

// Returns a partner-related value selected by a BT_PARAM_* index. In multi
// battles the partner is one of the five stat trainers; otherwise the graphics
// ID falls back to the player's own gender.
static u16 BattleTower_GetPartnerParam(BattleTower *battleTower, u8 param)
{
    static const u16 partnerGraphics[] = {
        OBJ_EVENT_GFX_CHERYL,
        OBJ_EVENT_GFX_MIRA,
        OBJ_EVENT_GFX_RILEY,
        OBJ_EVENT_GFX_MARLEY,
        OBJ_EVENT_GFX_BUCK
    };

    if (param == BT_PARAM_PARTNER_ID) {
        return battleTower->partnerID;
    }

    if (param == BT_PARAM_PARTNER_GRAPHICS_ID) {
        if (battleTower->challengeMode == BATTLE_TOWER_MODE_MULTI) {
            return partnerGraphics[battleTower->partnerID];
        } else {
            if (battleTower->partnerGender) {
                return OBJ_EVENT_GFX_PLAYER_F;
            } else {
                return OBJ_EVENT_GFX_PLAYER_M;
            }
        }
    }

    if (battleTower->playerGender) {
        return OBJ_EVENT_GFX_PLAYER_F;
    } else {
        return OBJ_EVENT_GFX_PLAYER_M;
    }
}

BOOL ScrCmd_ShowBattlePoints(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    u8 tilemapLeft = ScriptContext_ReadByte(ctx);
    u8 tilemapTop = ScriptContext_ReadByte(ctx);
    Window **bpWindow = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_SPECIAL_CURRENCY_WINDOW);
    *bpWindow = FieldMenu_DrawBPWindow(ctx->fieldSystem, tilemapLeft, tilemapTop);

    return FALSE;
}

BOOL ScrCmd_HideBattlePoints(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    Window **bpWindow = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_SPECIAL_CURRENCY_WINDOW);

    FieldMenu_DeleteSpecialCurrencyWindow(*bpWindow);
    return FALSE;
}

BOOL ScrCmd_UpdateBPDisplay(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    Window **bpWindow = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_SPECIAL_CURRENCY_WINDOW);

    FieldMenu_PrintBPToWindow(ctx->fieldSystem, *bpWindow);
    return FALSE;
}

BOOL ScrCmd_GetBattlePoints(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    SaveData *saveData = fieldSystem->saveData;
    u16 *destVar = ScriptContext_GetVarPointer(ctx);

    *destVar = WifiBattleTowerRecord_UpdateBattlePoints(SaveData_GetWifiBattleTowerRecord(saveData), 0, BATTLE_POINTS_FUNC_NONE);
    return FALSE;
}

BOOL ScrCmd_GiveBattlePoints(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    SaveData *saveData = fieldSystem->saveData;
    u16 value = ScriptContext_GetVar(ctx);

    GameRecords_AddToRecordValue(SaveData_GetGameRecords(ctx->fieldSystem->saveData), RECORD_BATTLE_POINTS_RECEIVED, value);
    WifiBattleTowerRecord_UpdateBattlePoints(SaveData_GetWifiBattleTowerRecord(saveData), value, BATTLE_POINTS_FUNC_ADD);

    return FALSE;
}

BOOL ScrCmd_RemoveBattlePoints(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    SaveData *saveData = fieldSystem->saveData;
    u16 value = ScriptContext_GetVar(ctx);

    GameRecords_AddToRecordValue(SaveData_GetGameRecords(ctx->fieldSystem->saveData), RECORD_BATTLE_POINTS_SPENT, value);
    WifiBattleTowerRecord_UpdateBattlePoints(SaveData_GetWifiBattleTowerRecord(saveData), value, BATTLE_POINTS_FUNC_SUB);

    return FALSE;
}

BOOL ScrCmd_CheckBattlePoints(ScriptContext *ctx)
{
    u16 battlePoints;
    FieldSystem *fieldSystem = ctx->fieldSystem;
    SaveData *saveData = fieldSystem->saveData;
    u16 value = ScriptContext_GetVar(ctx);
    u16 *destVar = ScriptContext_GetVarPointer(ctx);

    battlePoints = WifiBattleTowerRecord_UpdateBattlePoints(SaveData_GetWifiBattleTowerRecord(saveData), 0, BATTLE_POINTS_FUNC_NONE);

    if (battlePoints < value) {
        *destVar = FALSE;
    } else {
        *destVar = TRUE;
    }

    return FALSE;
}

#define FRONTIER_MART_ITEMS_START_ID 0
#define FRONTIER_MART_TMS_START_ID   26

// Looks up a Battle Frontier exchange-service prize. The table is split into a
// held-item section and a TM section; martID selects which section prizeID
// indexes into.
BOOL ScrCmd_GetExchangeServiceCornerItemAndCost(ScriptContext *ctx)
{
    u8 startID = FRONTIER_MART_ITEMS_START_ID;
    u16 martID = ScriptContext_GetVar(ctx);
    u16 prizeID = ScriptContext_GetVar(ctx);
    u16 *item = ScriptContext_GetVarPointer(ctx);
    u16 *cost = ScriptContext_GetVarPointer(ctx);
    static const u16 prizeList[][2] = {
        [FRONTIER_MART_ITEMS_START_ID] = { ITEM_PROTEIN, 1 },
        { ITEM_CALCIUM, 1 },
        { ITEM_IRON, 1 },
        { ITEM_ZINC, 1 },
        { ITEM_CARBOS, 1 },
        { ITEM_HP_UP, 1 },
        { ITEM_POWER_BRACER, 16 },
        { ITEM_POWER_BELT, 16 },
        { ITEM_POWER_LENS, 16 },
        { ITEM_POWER_BAND, 16 },
        { ITEM_POWER_ANKLET, 16 },
        { ITEM_POWER_WEIGHT, 16 },
        { ITEM_TOXIC_ORB, 16 },
        { ITEM_FLAME_ORB, 16 },
        { ITEM_WHITE_HERB, 32 },
        { ITEM_POWER_HERB, 32 },
        { ITEM_BRIGHTPOWDER, 48 },
        { ITEM_CHOICE_BAND, 48 },
        { ITEM_FOCUS_BAND, 48 },
        { ITEM_SCOPE_LENS, 48 },
        { ITEM_MUSCLE_BAND, 48 },
        { ITEM_FOCUS_SASH, 48 },
        { ITEM_CHOICE_SCARF, 48 },
        { ITEM_RAZOR_CLAW, 48 },
        { ITEM_RAZOR_FANG, 48 },
        { ITEM_RARE_CANDY, 48 },
        [FRONTIER_MART_TMS_START_ID] = { ITEM_TM06, 32 }, // update FRONTIER_MART_TMS_START_ID when adding entries above this line
        { ITEM_TM73, 32 },
        { ITEM_TM61, 32 },
        { ITEM_TM45, 32 },
        { ITEM_TM40, 40 },
        { ITEM_TM31, 40 },
        { ITEM_TM08, 48 },
        { ITEM_TM04, 48 },
        { ITEM_TM81, 64 },
        { ITEM_TM30, 64 },
        { ITEM_TM53, 64 },
        { ITEM_TM36, 80 },
        { ITEM_TM59, 80 },
        { ITEM_TM71, 80 },
        { ITEM_TM26, 80 }
    };

    if (martID == 1) {
        startID = FRONTIER_MART_TMS_START_ID;
    } else {
        startID = FRONTIER_MART_ITEMS_START_ID;
    }

    *item = prizeList[startID + prizeID][0];
    *cost = prizeList[startID + prizeID][1];

    return FALSE;
}
