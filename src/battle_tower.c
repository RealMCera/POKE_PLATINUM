#include "battle_tower.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle_tower.h"
#include "generated/battle_tower_modes.h"
#include "generated/frontier_trainers.h"
#include "generated/game_records.h"
#include "generated/trainer_score_events.h"

#include "struct_defs/battle_frontier.h"
#include "struct_defs/battle_tower.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "applications/party_menu/defs.h"
#include "field/field_system.h"
#include "savedata/save_table.h"

#include "battle_frontier_save.h"
#include "battle_frontier_stats.h"
#include "field_overworld_state.h"
#include "field_task.h"
#include "game_records.h"
#include "heap.h"
#include "inlines.h"
#include "journal.h"
#include "location.h"
#include "main.h"
#include "math_util.h"
#include "party.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "system_flags.h"
#include "system_vars.h"
#include "trainer_info.h"
#include "tv_segment.h"
#include "underground.h"
#include "unk_0204AEE8.h"
#include "unk_0206B9D8.h"
#include "vars_flags.h"
#include "wifi_battle_tower_save.h"

// Battle Tower challenge state. This module owns the BattleTower object that
// tracks an in-progress Battle Tower run: the chosen challenge mode, the
// player's selected party, the generated opponent trainer IDs and the streak
// bookkeeping that feeds the Battle Frontier save, game records and the WiFi
// Battle Tower record. The field scripts drive it through the
// BT_FUNC_* commands handled in scrcmd_battle_tower.c.

// A species/held-item pair used when validating a Battle Tower party. Held
// items are only compared when the challenge requires unique held items.
typedef struct {
    u16 species;
    u16 heldItem;
} BattleTowerMonEntry;

static u16 BattleTower_GiveRibbonToParty(SaveData *saveData, enum PokemonDataParam param, BattleTower *battleTower);
static u16 BattleTower_UpdateAbilityRibbonFlag(BattleTower *battleTower);
static void BattleTower_SaveTeamToWifiRecord(BattleTower *battleTower, SaveData *saveData, int teamIdx);

u16 BattleTower_GetPartySizeForChallengeMode(u16 challengeMode)
{
    switch (challengeMode) {
    case BATTLE_TOWER_MODE_SINGLE:
    case BATTLE_TOWER_MODE_WIFI:
    case BATTLE_TOWER_MODE_5:
        return 3;
    case BATTLE_TOWER_MODE_DOUBLE:
        return 4;
    case BATTLE_TOWER_MODE_MULTI:
    case BATTLE_TOWER_MODE_LINK_MULTI:
    case BATTLE_TOWER_MODE_6:
        return 2;
    }

    return 0;
}

// Returns TRUE if a matching species (and, when heldItem is non-zero, a
// matching held item) is already present in the first count entries.
static BOOL BattleTower_IsSpeciesAndItemInList(BattleTowerMonEntry *entries, u16 species, u16 heldItem, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (entries[i].species == species) {
            if (heldItem == 0) {
                continue;
            }

            if (entries[i].heldItem == heldItem) {
                return 1;
            }
        }
    }

    return 0;
}

// Returns TRUE if no two entries share a species and no two entries share a
// non-zero held item.
static BOOL BattleTower_AreSpeciesAndItemsUnique(BattleTowerMonEntry *entries, int count)
{
    int i, j;

    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (entries[i].species == entries[j].species) {
                return 0;
            }

            if (entries[i].heldItem == 0) {
                continue;
            }

            if (entries[i].heldItem == entries[j].heldItem) {
                return 0;
            }
        }
    }

    return 1;
}

// Searches every combination of chooseCount entries drawn from the first
// entryCount entries for one whose species and held items are all unique.
// startCount bounds how many starting positions are tried; the caller passes
// entryCount - chooseCount + 1, which is just enough to cover every
// combination.
static BOOL BattleTower_HasUniqueCombination(BattleTowerMonEntry *entries, int chooseCount, int entryCount, int startCount)
{
    int i, j, k, l;
    BattleTowerMonEntry combination[4];

    MI_CpuClear8(combination, sizeof(BattleTowerMonEntry) * 4);

    for (i = 0; i < startCount; i++) {
        combination[0] = entries[i];

        for (j = i + 1; j < entryCount; j++) {
            combination[1] = entries[j];

            if (chooseCount == 2) {
                if (BattleTower_AreSpeciesAndItemsUnique(combination, chooseCount)) {
                    return 1;
                }

                continue;
            }

            for (k = j + 1; k < entryCount; k++) {
                combination[2] = entries[k];

                if (chooseCount == 3) {
                    if (BattleTower_AreSpeciesAndItemsUnique(combination, chooseCount)) {
                        return 1;
                    }

                    continue;
                }

                for (l = k + 1; l < entryCount; l++) {
                    combination[3] = entries[l];

                    if (BattleTower_AreSpeciesAndItemsUnique(combination, chooseCount)) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

// Returns TRUE if the party contains at least requiredCount eligible Pokémon
// that can be arranged into a team with no duplicate species (and, when
// checkHeldItems is set, no duplicate held items). Eggs and Pokémon on the
// Battle Frontier banlist are ignored.
BOOL BattleTower_HasEnoughValidPokemon(u16 requiredCount, SaveData *saveData, u8 checkHeldItems)
{
    u8 i, validCount, partyCount;
    u16 species, heldItem;
    Party *party;
    Pokemon *mon;
    BattleTowerMonEntry entries[6];

    party = SaveData_GetParty(saveData);
    partyCount = Party_GetCurrentCount(party);

    if (partyCount < requiredCount) {
        return 0;
    }

    for (i = 0, validCount = 0; i < partyCount; i++) {
        mon = Party_GetPokemonBySlotIndex(party, i);
        species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
        heldItem = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);

        if (checkHeldItems == 0) {
            heldItem = 0;
        }

        if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) != 0) {
            continue;
        }

        if (Pokemon_IsOnBattleFrontierBanlist(species) == 1) {
            continue;
        }

        if (checkHeldItems == 1) {
            if (BattleTower_IsSpeciesAndItemInList(entries, species, heldItem, validCount) == 1) {
                continue;
            }
        }

        entries[validCount].species = species;
        entries[validCount].heldItem = heldItem;
        validCount++;
    }

    if (validCount < requiredCount) {
        return 0;
    }

    return BattleTower_HasUniqueCombination(entries, requiredCount, validCount, (validCount - requiredCount) + 1);
}

void BattleTower_ResetSystem(void)
{
    OS_ResetSystem(RESET_CLEAN);
}

void BattleTower_InitWifiSave(WifiBattleTowerSave *save)
{
    WifiBattleTowerSave_Init(save);
}

BOOL BattleTower_IsWifiChallengeInProgress(WifiBattleTowerSave *save)
{
    return WifiBattleTowerSave_GetIsInProgress(save);
}

void BattleTower_SetCommunicationClubAccessible(FieldSystem *fieldSystem)
{
    Location *specialLocation = FieldOverworldState_GetSpecialLocation(SaveData_GetFieldOverworldState(fieldSystem->saveData));

    Location_Set(specialLocation, fieldSystem->location->mapHeaderID, -1, PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar), 0);
    SystemFlag_SetCommunicationClubAccessible(SaveData_GetVarsFlags(fieldSystem->saveData));

    return;
}

void BattleTower_ClearCommunicationClubAccessible(FieldSystem *fieldSystem)
{
    SystemFlag_ClearCommunicationClubAccessible(SaveData_GetVarsFlags(fieldSystem->saveData));
}

// Returns the player's latest win streak for the given challenge mode. Modes
// 5 (unused) and 6 (WiFi) store their streak in different save slots.
u16 BattleTower_GetLatestStreak(SaveData *saveData, u16 challengeMode)
{
    u16 streak;

    if (challengeMode == 5) {
        return 0;
    }

    if (challengeMode == 6) {
        streak = BattleFrontierSave_GetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_LATEST_STREAK_MODE_6);
        return streak;
    }

    streak = BattleFrontierSave_GetStat(SaveData_GetBattleFrontier(saveData), 1 + challengeMode * 2, 0xff);

    return streak;
}

// Sets or clears the WiFi record's "results pending upload" flag (bit 5).
void BattleTower_SetWifiResultsPending(SaveData *saveData, u8 pending)
{
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);

    if (pending == 0) {
        WifiBattleTowerRecord_UpdateBitFlag(record, 5, 2);
    } else {
        WifiBattleTowerRecord_UpdateBitFlag(record, 5, 1);
    }
}

u16 BattleTower_HasWifiResultsPending(SaveData *saveData)
{
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    return (u16)WifiBattleTowerRecord_UpdateBitFlag(record, 5, 0);
}

// Clears the saved WiFi challenge progress for the mode stored in the WiFi
// save and returns that mode. Mode 5 has no progress to clear.
u16 BattleTower_ResetWifiProgress(SaveData *saveData)
{
    u8 challengeMode;
    WifiBattleTowerSave *save = SaveData_GetWifiBattleTowerSave(saveData);
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    challengeMode = (u8)WifiBattleTowerSave_GetField(save, 0, NULL);

    if (challengeMode == 5) {
        return challengeMode;
    }

    if (challengeMode == 6) {
        BattleFrontierSave_SetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE, 0);
    } else {
        WifiBattleTowerRecord_UpdateBitFlag(record, 8 + challengeMode, 2);
    }

    WifiBattleTowerRecord_UpdateRoomNum(record, challengeMode, 2);
    BattleFrontierSave_SetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), BattleFrontierStats_GetTowerLatestStreakIndex(challengeMode), 0);

    if ((challengeMode != 4) && (challengeMode != 6)) {
        sub_0206C02C(saveData);
    }

    return challengeMode;
}

u16 BattleTower_HasWifiOpponentData(SaveData *saveData)
{
    WifiBattleTowerDownloadData *downloadData = SaveData_GetWifiBattleTowerDownloadData(saveData);
    return (u16)WifiBattleTowerDownloadData_HasOpponentData(downloadData);
}

void BattleTower_SetNull(BattleTower **battleTower)
{
    GF_ASSERT(*battleTower == NULL);
    *battleTower = NULL;
}

// Allocates and initialises a BattleTower. When isResume is FALSE a fresh
// challenge is started for challengeMode; otherwise the in-progress WiFi
// challenge is restored from the WiFi save.
BattleTower *BattleTower_Init(SaveData *saveData, u16 isResume, u16 challengeMode)
{
    u8 mode;
    u16 i, streakActive;
    BattleTower *battleTower;
    BattleFrontierSave *frontier;
    GameRecords *gameRecords;

    battleTower = Heap_Alloc(HEAP_ID_FIELD2, sizeof(BattleTower));
    MI_CpuClear8(battleTower, sizeof(BattleTower));

    battleTower->heapID = HEAP_ID_FIELD2;
    battleTower->wifiBattleTowerSave = SaveData_GetWifiBattleTowerSave(saveData);
    battleTower->unk_74 = SaveData_GetWifiBattleTowerRecord(saveData);
    battleTower->unk_00 = 0x12345678;

    WifiBattleTowerSave_SetIsInProgress(battleTower->wifiBattleTowerSave, 0);

    if (isResume == 0) {
        battleTower->challengeMode = challengeMode;
        battleTower->partySize = (u8)BattleTower_GetPartySizeForChallengeMode(battleTower->challengeMode);
        battleTower->nextOpponentNum = 1;
        battleTower->unk_0D = 0;

        for (i = 0; i < 4; i++) {
            battleTower->unk_2A[i] = 0xFF;
        }

        for (i = 0; i < BT_OPPONENTS_COUNT * 2; i++) {
            battleTower->trainerIDs[i] = 0xFFFF;
        }

        WifiBattleTowerSave_Init(battleTower->wifiBattleTowerSave);
        mode = battleTower->challengeMode;
        WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 0, &mode);
    } else {
        battleTower->challengeMode = (u8)WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 0, NULL);
        battleTower->nextOpponentNum = (u8)WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 1, NULL);
        battleTower->unk_0D = battleTower->nextOpponentNum - 1;
        battleTower->partySize = (u8)BattleTower_GetPartySizeForChallengeMode(battleTower->challengeMode);

        WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 5, battleTower->unk_2A);
        WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 8, battleTower->trainerIDs);

        battleTower->unk_08 = WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 10, NULL);

        if (battleTower->challengeMode == BATTLE_TOWER_MODE_MULTI) {
            battleTower->partnerID = (u8)WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 9, NULL);

            WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 6, &(battleTower->unk_7E8[battleTower->partnerID]));
            sub_0204B404(battleTower, &battleTower->partnersDataDTO[battleTower->partnerID], FRONTIER_TRAINER_TRAINER_CHERYL_CHERYL + battleTower->partnerID, WifiBattleTowerSave_GetField(battleTower->wifiBattleTowerSave, 7, NULL), &(battleTower->unk_7E8[battleTower->partnerID]), battleTower->heapID);
        }
    }

    battleTower->playerGender = TrainerInfo_Gender(SaveData_GetTrainerInfo(saveData));

    if (battleTower->challengeMode != BATTLE_TOWER_MODE_5) {
        frontier = SaveData_GetBattleFrontier(saveData);
        gameRecords = SaveData_GetGameRecords(saveData);

        if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
            streakActive = SystemVars_GetWiFiFrontierCleared(SaveData_GetVarsFlags(saveData));
        } else {
            streakActive = WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 0);
        }

        if (streakActive) {
            if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
                battleTower->unk_1A = BattleFrontierSave_GetStatAutoHostIdx(frontier, 113);
            } else {
                battleTower->unk_1A = BattleFrontierSave_GetStat(
                    frontier, 1 + battleTower->challengeMode * 2, 0xff);
            }

            battleTower->roomNum = WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, battleTower->challengeMode, 0);
        }

        battleTower->unk_20 = GameRecords_GetRecordValue(gameRecords, RECORD_BATTLE_TOWER_VICTORIES);
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        battleTower->roomNum = WifiBattleTowerRecord_SetRoomNum(battleTower->unk_74, BATTLE_TOWER_MODE_6, battleTower->unk_1A / 7);
    }

    return battleTower;
}

void BattleTower_Free(BattleTower *battleTower)
{
    if (battleTower == NULL) {
        return;
    }

    GF_ASSERT(battleTower->unk_00 == 0x12345678);

    MI_CpuClear8(battleTower, sizeof(BattleTower));
    Heap_Free(battleTower);

    battleTower = NULL;
}

// Opens the party menu so the player can choose their Battle Tower party.
void BattleTower_StartPartyMenu(BattleTower *battleTower, FieldTask *task, void **partyMenu)
{
    sub_0206BBFC(task, partyMenu, 17, 0, battleTower->partySize, battleTower->partySize, 100, 0);
}

// Reads the party menu result into the BattleTower's party slots. Returns
// FALSE if the player cancelled or selected the empty slot.
BOOL BattleTower_ReadPartyMenuSelection(BattleTower *battleTower, void **partyMenuPtr, SaveData *saveData)
{
    u16 i = 0;
    PartyMenu *partyMenu = *partyMenuPtr;
    Party *party;
    Pokemon *mon;

    if ((partyMenu->menuSelectionResult != 0) || (partyMenu->selectedMonSlot == 7)) {
        Heap_Free(*partyMenuPtr);
        *partyMenuPtr = NULL;
        return 0;
    }

    party = SaveData_GetParty(saveData);

    for (i = 0; i < battleTower->partySize; i++) {
        battleTower->unk_2A[i] = partyMenu->selectionOrder[i] - 1;
        mon = Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[i]);
        battleTower->unk_2E[i] = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
        battleTower->unk_36[i] = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);
    }

    Heap_Free(*partyMenuPtr);
    *partyMenuPtr = NULL;
    return 1;
}

// Returns 1 if the selected party has a duplicate species, 2 if it has a
// duplicate held item, or 0 if it is valid.
int BattleTower_CheckDuplicateSpeciesAndHeldItems(BattleTower *battleTower, SaveData *saveData)
{
    u16 i = 0, j = 0;
    u16 species[4], heldItems[4];
    Party *party = SaveData_GetParty(saveData);

    for (i = 0; i < battleTower->partySize; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[i]);
        species[i] = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
        heldItems[i] = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);

        if (i == 0) {
            continue;
        }

        for (j = 0; j < i; j++) {
            if (species[i] == species[j]) {
                return 1;
            }

            if (heldItems[i] != 0 && heldItems[i] == heldItems[j]) {
                return 2;
            }
        }
    }

    return 0;
}

static BOOL BattleTower_IsTrainerAlreadyUsed(u16 *trainerIDs, u16 trainerID, u16 currOpponentNum)
{
    u16 opponentNum;

    for (opponentNum = 0; opponentNum < currOpponentNum; opponentNum++) {
        if (trainerIDs[opponentNum] == trainerID) {
            return TRUE;
        }
    }

    return FALSE;
}

// Fills in the opponent trainer IDs for the current room, retrying whenever a
// generated ID collides with one already used earlier in the run. Multi and
// WiFi modes use two trainers per opponent (a pair per battle).
void BattleTower_GenerateOpponentTrainerIDs(BattleTower *battleTower, SaveData *saveData)
{
    int opponentNum;
    u16 trainerID, roomNum;

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_MULTI || battleTower->challengeMode == BATTLE_TOWER_MODE_6 || battleTower->challengeMode == BATTLE_TOWER_MODE_LINK_MULTI) {
        if ((battleTower->challengeMode == BATTLE_TOWER_MODE_LINK_MULTI && battleTower->unk_14 > battleTower->roomNum) || (battleTower->challengeMode == BATTLE_TOWER_MODE_6 && battleTower->unk_14 > battleTower->roomNum)) {
            roomNum = battleTower->unk_14;
        } else {
            roomNum = battleTower->roomNum;
        }

        for (opponentNum = 0; opponentNum < BT_OPPONENTS_COUNT * 2; opponentNum++) {
            do {
                trainerID = BattleTower_GetTrainerIDForRoomAndOpponentNum(battleTower, roomNum, opponentNum / 2, battleTower->challengeMode);
            } while (BattleTower_IsTrainerAlreadyUsed(battleTower->trainerIDs, trainerID, opponentNum));

            battleTower->trainerIDs[opponentNum] = trainerID;
        }
    } else {
        for (opponentNum = 0; opponentNum < BT_OPPONENTS_COUNT; opponentNum++) {
            do {
                trainerID = BattleTower_GetTrainerIDForRoomAndOpponentNum(battleTower, battleTower->roomNum, opponentNum, battleTower->challengeMode);
            } while (BattleTower_IsTrainerAlreadyUsed(battleTower->trainerIDs, trainerID, opponentNum));

            battleTower->trainerIDs[opponentNum] = trainerID;
        }
    }
}

u16 BattleTower_GetNextOpponentNum(BattleTower *battleTower)
{
    return battleTower->nextOpponentNum;
}

BOOL BattleTower_HasDefeatedSevenTrainers(BattleTower *battleTower)
{
    if (battleTower->defeatedSevenTrainers) {
        return TRUE;
    }

    if (battleTower->nextOpponentNum > 7) {
        battleTower->defeatedSevenTrainers = TRUE;
        return TRUE;
    }

    return FALSE;
}

// Records the player's streak and lead Pokémon in the TV broadcast for the
// single and double challenge modes.
static void BattleTower_RecordStreakToTV(BattleTower *battleTower, SaveData *saveData, u16 streak)
{
    Party *party;

    if (battleTower->challengeMode != BATTLE_TOWER_MODE_SINGLE && battleTower->challengeMode != BATTLE_TOWER_MODE_DOUBLE) {
        return;
    }

    party = SaveData_GetParty(saveData);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_SINGLE) {
        sub_0206DBB0(saveData, streak, Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[0]), 1);
    } else {
        sub_0206DBB0(saveData, streak, Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[0]), 0);
    }
}

// Per-mode bookkeeping after a battle: saves the team, records the streak in
// the TV broadcast and, for WiFi, updates the saved challenge state.
static void BattleTower_UpdatePostBattleData(BattleTower *battleTower, SaveData *saveData, u8 tvWin, u16 streak)
{
    u8 mode;

    switch (battleTower->challengeMode) {
    case BATTLE_TOWER_MODE_SINGLE:
        BattleTower_SaveTeamToWifiRecord(battleTower, saveData, 0);
        // fallthrough
    case BATTLE_TOWER_MODE_DOUBLE:
        if (streak >= 7) {
            sub_0206CFE4(SaveData_GetTVBroadcast(saveData), tvWin, streak);
        }
        break;
    case BATTLE_TOWER_MODE_WIFI:
        BattleTower_SaveTeamToWifiRecord(battleTower, saveData, 1);
        WifiBattleTowerSave_AddCounters(battleTower->wifiBattleTowerSave, battleTower->unk_28, battleTower->unk_24, battleTower->unk_26);

        mode = battleTower->challengeMode;
        WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 0, &mode);

        mode = battleTower->nextOpponentNum;
        WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 1, &mode);
        WifiBattleTowerRecord_CalcRatingScore(battleTower->unk_74, battleTower->wifiBattleTowerSave);
        break;
    default:
        break;
    }
}

// Updates the Battle Frontier stats, game records and WiFi record after the
// player loses a challenge (or quits mid-run).
void BattleTower_UpdateGameRecords(BattleTower *battleTower, SaveData *saveData)
{
    u32 streakValue = 0;
    int statIndex;
    u16 prevRecord, newRecord, streakActive;
    GameRecords *gameRecords = SaveData_GetGameRecords(saveData);
    BattleFrontierSave *frontier = SaveData_GetBattleFrontier(saveData);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_5) {
        return;
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        statIndex = STAT_TOWER_RECORD_STREAK_MODE_6;
    } else {
        statIndex = battleTower->challengeMode * 2;
    }

    prevRecord = BattleFrontierSave_GetStatAutoHostIdx(frontier, statIndex);
    newRecord = BattleFrontierSave_SetIfBetterAutoHostIdx(frontier, statIndex, battleTower->unk_1A + battleTower->unk_0D);

    if (newRecord > 1) {
        if (prevRecord < newRecord || (prevRecord == newRecord && newRecord % 7 == 0)) {
            BattleTower_RecordStreakToTV(battleTower, saveData, newRecord);
        }
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        streakActive = BattleFrontierSave_GetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE);
    } else {
        streakActive = WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 0);
    }

    streakValue = BattleFrontierSave_SetStatAutoHostIdx(frontier, statIndex + 1, battleTower->unk_1A + battleTower->unk_0D);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        BattleFrontierSave_SetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE, 0);
    } else {
        WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 2);
    }

    GameRecords_AddToRecordValue(gameRecords, RECORD_BATTLE_TOWER_VICTORIES, battleTower->unk_0D);
    WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, battleTower->challengeMode, 2);

    if (battleTower->challengeMode != BATTLE_TOWER_MODE_6) {
        GameRecords_AddToRecordValue(SaveData_GetGameRecords(saveData), RECORD_BATTLE_TOWER_CHALLENGES, 1);
    }

    BattleTower_UpdateAbilityRibbonFlag(battleTower);

    streakValue += 1;

    if (streakValue > 9999) {
        streakValue = 9999;
    }

    BattleTower_UpdatePostBattleData(battleTower, saveData, 0, streakValue);
}

// Updates the same records as BattleTower_UpdateGameRecords, but for the case
// where the player defeated all seven trainers. Also writes a journal entry
// for WiFi challenges.
void BattleTower_UpdateGameRecordsAndJournal(BattleTower *battleTower, SaveData *saveData, JournalEntry *journalEntry)
{
    u32 streakValue = 0;
    int statIndex;
    void *journalEntryOnlineEvent;
    u16 prevRecord, newRecord, streakActive;
    GameRecords *gameRecords;
    BattleFrontierSave *frontier;

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_5) {
        return;
    }

    gameRecords = SaveData_GetGameRecords(saveData);
    frontier = SaveData_GetBattleFrontier(saveData);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        statIndex = STAT_TOWER_RECORD_STREAK_MODE_6;
    } else {
        statIndex = battleTower->challengeMode * 2;
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        streakActive = BattleFrontierSave_GetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE);
    } else {
        streakActive = WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 0);
    }

    streakValue = BattleFrontierSave_SetStatAutoHostIdx(frontier, statIndex + 1, battleTower->unk_1A + battleTower->unk_0D);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        BattleFrontierSave_SetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE, 1);
    } else {
        WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 1);
    }

    prevRecord = BattleFrontierSave_GetStatAutoHostIdx(frontier, statIndex);
    newRecord = BattleFrontierSave_SetIfBetterAutoHostIdx(frontier, statIndex, streakValue);

    GameRecords_AddToRecordValue(gameRecords, RECORD_BATTLE_TOWER_VICTORIES, 7);
    WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, battleTower->challengeMode, 3);

    if (battleTower->challengeMode != BATTLE_TOWER_MODE_6) {
        GameRecords_AddToRecordValue(gameRecords, RECORD_BATTLE_TOWER_CHALLENGES, 1);
    }

    GameRecords_IncrementTrainerScore(gameRecords, TRAINER_SCORE_EVENT_UNK_14);
    BattleTower_UpdateAbilityRibbonFlag(battleTower);
    BattleTower_UpdatePostBattleData(battleTower, saveData, 1, streakValue);

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_WIFI) {
        journalEntryOnlineEvent = JournalEntry_CreateEventBattleRoom(battleTower->heapID);
        JournalEntry_SaveData(journalEntry, journalEntryOnlineEvent, JOURNAL_ONLINE_EVENT);
    }
}

// Writes the current challenge state into the WiFi save so it can be resumed.
void BattleTower_SaveWifiState(BattleTower *battleTower)
{
    u8 modeBuf[4];

    modeBuf[0] = battleTower->challengeMode;
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 0, modeBuf);

    modeBuf[0] = battleTower->nextOpponentNum;
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 1, modeBuf);

    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 5, battleTower->unk_2A);
    WifiBattleTowerSave_AddCounters(battleTower->wifiBattleTowerSave, battleTower->unk_28, battleTower->unk_24, battleTower->unk_26);
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 8, battleTower->trainerIDs);
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 10, &(battleTower->unk_08));
    WifiBattleTowerSave_SetIsInProgress(battleTower->wifiBattleTowerSave, 1);

    if (battleTower->challengeMode != BATTLE_TOWER_MODE_MULTI) {
        return;
    }

    modeBuf[0] = battleTower->partnerID;
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 9, modeBuf);

    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 6, &(battleTower->unk_7E8[battleTower->partnerID]));
    WifiBattleTowerSave_SetField(battleTower->wifiBattleTowerSave, 7, &(battleTower->unk_838[battleTower->partnerID]));
}

// Builds the partner data (team and graphics) for every possible multi-battle
// partner.
void BattleTower_BuildPartnerData(BattleTower *battleTower)
{
    for (int partnerID = 0; partnerID < BT_PARTNERS_COUNT; partnerID++) {
        battleTower->unk_838[partnerID] = (u8)sub_0204B3B8(battleTower, &(battleTower->partnersDataDTO[partnerID]), FRONTIER_TRAINER_TRAINER_CHERYL_CHERYL + partnerID, battleTower->partySize, battleTower->unk_2E, battleTower->unk_36, &(battleTower->unk_7E8[partnerID]), battleTower->heapID);
    }
}

u16 BattleTower_GetObjectIDFromOpponentID(BattleTower *battleTower, u16 opponentID)
{
    return BattleTower_GetObjectIDFromTrainerClass(battleTower->opponentsDataDTO[opponentID].trainer.trainerType);
}

u16 BattleTower_GetChallengeMode(BattleTower *battleTower)
{
    return battleTower->challengeMode;
}

u16 BattleTower_GetBeatPalmer(BattleTower *battleTower)
{
    return (u16)battleTower->beatPalmer;
}

// Awards Battle Points based on the challenge mode and current rank/room, then
// adds them to the WiFi record. Returns the amount awarded.
u16 BattleTower_GiveBattlePointsReward(BattleTower *battleTower)
{
    u16 roomNum;
    u16 battlePoints = 0;
    static const u8 wifiBattlePoints[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10 };
    static const u8 soloBattlePoints[] = { 0, 3, 3, 4, 4, 5, 5, 7 };
    static const u8 linkMultiBattlePoints[] = { 0, 8, 9, 11, 12, 14, 15, 18 };

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_5) {
        return 0;
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_WIFI) {
        battlePoints = wifiBattlePoints[WifiBattleTowerRecord_UpdateRank(battleTower->unk_74, 0)];
    } else {
        if (battleTower->challengeMode == BATTLE_TOWER_MODE_LINK_MULTI || battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
            roomNum = WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, battleTower->challengeMode, 0);

            if (roomNum >= 7) {
                battlePoints = 18;
            } else {
                battlePoints = linkMultiBattlePoints[roomNum];
            }
        } else {
            roomNum = WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, battleTower->challengeMode, 0);

            if (battleTower->beatPalmer) {
                battlePoints = 20;
            } else if (roomNum >= 7) {
                battlePoints = 7;
            } else {
                battlePoints = soloBattlePoints[roomNum];
            }
        }
    }

    WifiBattleTowerRecord_UpdateBattlePoints(battleTower->unk_74, battlePoints, BATTLE_POINTS_FUNC_ADD);
    return battlePoints;
}

// Returns TRUE if the player has reached a 50-win streak and has not yet been
// offered the corresponding Palmer battle.
u16 BattleTower_IsPalmerBattleAvailable(BattleTower *battleTower, SaveData *saveData)
{
    u16 streak;

    streak = BattleTower_GetLatestStreak(saveData, battleTower->challengeMode);

    if (streak < 50) {
        return 0;
    }

    if (streak >= 100) {
        if (WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 1, 0)) {
            return 0;
        }
    } else {
        if (WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 0, 0)) {
            return 0;
        }
    }

    return 1;
}

// Updates the WiFi rank after a battle. operation is 0 to read the current
// rank, 1 after a win and 2 after a loss. Returns the new rank, or 0 when the
// rank did not change.
u16 BattleTower_UpdateRank(BattleTower *battleTower, SaveData *saveData, u8 operation)
{
    u8 lossStreak, rank;
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);
    static const u8 lossStreakThresholds[] = {
        0,
        5,
        4,
        4,
        3,
        3,
        2,
        2,
        1,
        1,
    };

    switch (operation) {
    case 0:
        return (u16)WifiBattleTowerRecord_UpdateRank(record, 0);
    case 1:
        WifiBattleTowerRecord_UpdateBitFlag(record, 4, 2);
        rank = WifiBattleTowerRecord_UpdateRank(record, 0);

        if (rank == 10) {
            battleTower->unk_10_4 = 1;
            return 0;
        }

        WifiBattleTowerRecord_UpdateRank(record, 3);

        if (rank + 1 >= 5) {
            battleTower->unk_10_4 = 1;
        }

        return 1;
    case 2:
        lossStreak = WifiBattleTowerRecord_UpdateLossStreak(record, 3);
        rank = WifiBattleTowerRecord_UpdateRank(record, 0);

        if (rank == 1) {
            return 0;
        }

        if (lossStreak >= lossStreakThresholds[rank - 1]) {
            WifiBattleTowerRecord_UpdateRank(record, 4);
            WifiBattleTowerRecord_UpdateLossStreak(record, 2);
            WifiBattleTowerRecord_UpdateBitFlag(record, 4, 2);

            return 1;
        }

        return 0;
    }

    return 0;
}

// Gives the single-mode ability ribbon after defeating Palmer. Returns TRUE if
// at least one party Pokémon received it.
u16 BattleTower_GivePalmerRibbon(BattleTower *battleTower, SaveData *saveData)
{
    if (battleTower->challengeMode != BATTLE_TOWER_MODE_SINGLE) {
        return 0;
    }

    switch (battleTower->beatPalmer) {
    case 1:
        return BattleTower_GiveRibbonToParty(saveData, MON_DATA_ABILITY_RIBBON, battleTower);
    case 2:
        return BattleTower_GiveRibbonToParty(saveData, MON_DATA_GREAT_ABILITY_RIBBON, battleTower);
    }

    return 0;
}

// Gives the challenge-mode-specific ability ribbon once the player has reached
// the required streak (tracked by unk_10_4).
u16 BattleTower_GiveModeAbilityRibbon(BattleTower *battleTower, SaveData *saveData)
{
    enum PokemonDataParam param;

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_5) {
        return 0;
    }

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        return 0;
    }

    if (!battleTower->unk_10_4) {
        return 0;
    }

    switch (battleTower->challengeMode) {
    case BATTLE_TOWER_MODE_DOUBLE:
        param = MON_DATA_DOUBLE_ABILITY_RIBBON;
        break;
    case BATTLE_TOWER_MODE_MULTI:
        param = MON_DATA_MULTI_ABILITY_RIBBON;
        break;
    case BATTLE_TOWER_MODE_LINK_MULTI:
        param = MON_DATA_PAIR_ABILITY_RIBBON;
        break;
    case BATTLE_TOWER_MODE_WIFI:
        param = MON_DATA_WORLD_ABILITY_RIBBON;
        break;
    }

    return BattleTower_GiveRibbonToParty(saveData, param, battleTower);
}

// Seeds the BattleTower RNG. A fresh seed is used when no streak is active;
// otherwise the seed is advanced by the current room number.
u16 BattleTower_UpdateRandomSeed(BattleTower *battleTower, SaveData *saveData)
{
    u8 streakActive;

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        streakActive = BattleFrontierSave_GetStatAutoHostIdx(SaveData_GetBattleFrontier(saveData), STAT_TOWER_WFC_STREAK_ACTIVE);
    } else {
        streakActive = WifiBattleTowerRecord_UpdateBitFlag(battleTower->unk_74, 8 + battleTower->challengeMode, 0);
    }

    if (!streakActive) {
        battleTower->unk_08 = sub_0206C02C(saveData);
    } else {
        battleTower->unk_08 = sub_0206C068(saveData);
    }

    return battleTower->unk_08 / 65535;
}

// Sets the given ribbon on every party Pokémon that does not already have it.
// Returns TRUE if at least one Pokémon was updated.
static u16 BattleTower_GiveRibbonToParty(SaveData *saveData, enum PokemonDataParam param, BattleTower *battleTower)
{
    u8 ribbonValue = 1;
    u8 updatedCount;
    int i;
    Party *party;
    Pokemon *mon;

    party = SaveData_GetParty(saveData);
    updatedCount = 0;

    for (i = 0; i < battleTower->partySize; i++) {
        mon = Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[i]);

        if (Pokemon_GetValue(mon, param, NULL)) {
            continue;
        }

        Pokemon_SetValue(mon, param, &ribbonValue);
        sub_0206DDB8(saveData, mon, param);
        ++updatedCount;
    }

    if (updatedCount == 0) {
        return 0;
    }

    return 1;
}

// Marks the ability ribbon as earned once the current streak reaches 50. Only
// applies to the multi/link-multi challenge modes.
static u16 BattleTower_UpdateAbilityRibbonFlag(BattleTower *battleTower)
{
    u16 streak;

    if (battleTower->challengeMode == BATTLE_TOWER_MODE_5 || battleTower->challengeMode == BATTLE_TOWER_MODE_SINGLE || battleTower->challengeMode == BATTLE_TOWER_MODE_6 || battleTower->challengeMode == BATTLE_TOWER_MODE_WIFI) {
        return 0;
    }

    streak = battleTower->unk_1A + battleTower->unk_0D;

    if (streak < 50) {
        return 0;
    }

    battleTower->unk_10_4 = 1;
    return 1;
}

// Copies the fields needed to reconstruct a Pokémon into a FrontierPokemon.
static void BattleTower_CopyPokemonToFrontierPokemon(FrontierPokemon *dest, Pokemon *mon)
{
    int i;

    dest->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    dest->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    dest->item = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);

    for (i = 0; i < LEARNED_MOVES_MAX; i++) {
        dest->moves[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1 + i, NULL);
        dest->combinedPPUps |= ((Pokemon_GetValue(mon, MON_DATA_MOVE1_PP_UPS + i, NULL)) << (i * 2));
    }

    dest->language = Pokemon_GetValue(mon, MON_DATA_LANGUAGE, NULL);
    dest->otID = Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL);
    dest->personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);
    dest->combinedIVs = Pokemon_GetValue(mon, MON_DATA_COMBINED_IVS, NULL);

    for (i = 0; i < 6; i++) {
        dest->evList[i] = Pokemon_GetValue(mon, MON_DATA_HP_EV + i, NULL);
    }

    dest->ability = Pokemon_GetValue(mon, MON_DATA_ABILITY, NULL);
    dest->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);

    Pokemon_GetValue(mon, MON_DATA_NICKNAME, dest->nickname);
}

// Copies the player's party into a FrontierPokemon team and stores it in the
// WiFi record at teamIdx.
static void BattleTower_SaveTeamToWifiRecord(BattleTower *battleTower, SaveData *saveData, int teamIdx)
{
    FrontierPokemon *mons = Heap_AllocAtEnd(battleTower->heapID, sizeof(FrontierPokemon) * 3);
    MI_CpuClear8(mons, sizeof(FrontierPokemon) * 3);
    Party *party = SaveData_GetParty(saveData);

    for (int i = 0; i < 3; i++) {
        BattleTower_CopyPokemonToFrontierPokemon(&(mons[i]), Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[i]));
    }

    WifiBattleTowerRecord_SetTeam(battleTower->unk_74, teamIdx, mons);
    MI_CpuClear8(mons, sizeof(FrontierPokemon) * 3);
    Heap_Free(mons);
}

// Maps a Battle Tower trainer ID to the IV value its Pokémon are generated
// with. Higher IDs (later rooms) get progressively better IVs.
u8 BattleTower_GetIVsFromTrainerID(u16 battleTowerID)
{
    u8 ivs;

    if (battleTowerID < 100) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 1;
    } else if (battleTowerID < 120) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 2;
    } else if (battleTowerID < 140) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 3;
    } else if (battleTowerID < 160) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 4;
    } else if (battleTowerID < 180) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 5;
    } else if (battleTowerID < 200) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 6;
    } else if (battleTowerID < 220) {
        ivs = MAX_IVS_SINGLE_STAT / 8 * 7;
    } else {
        ivs = MAX_IVS_SINGLE_STAT;
    }

    return ivs;
}

// Advances the BattleTower RNG and returns a value in [0, 65534]. Mode 6 uses
// the global LCRNG instead of the per-record seed.
u16 BattleTower_GetRandom(BattleTower *battleTower)
{
    if (battleTower->challengeMode == BATTLE_TOWER_MODE_6) {
        return LCRNG_Next();
    }

    battleTower->unk_08 = sub_0206BFF0(battleTower->unk_08);
    return battleTower->unk_08 / 65535;
}
