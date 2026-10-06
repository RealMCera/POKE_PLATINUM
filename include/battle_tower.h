#ifndef POKEPLATINUM_BATTLE_TOWER_H
#define POKEPLATINUM_BATTLE_TOWER_H

#include "struct_defs/battle_tower.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "field/field_system_decl.h"

#include "field_task.h"
#include "journal.h"
#include "savedata.h"

u16 BattleTower_GetPartySizeForChallengeMode(u16 challengeMode);
BOOL BattleTower_HasEnoughValidPokemon(u16 requiredCount, SaveData *saveData, u8 checkHeldItems);
void BattleTower_ResetSystem(void);
void BattleTower_InitWifiSave(WifiBattleTowerSave *save);
BOOL BattleTower_IsWifiChallengeInProgress(WifiBattleTowerSave *save);
void BattleTower_SetCommunicationClubAccessible(FieldSystem *fieldSystem);
void BattleTower_ClearCommunicationClubAccessible(FieldSystem *fieldSystem);
u16 BattleTower_GetLatestStreak(SaveData *saveData, u16 challengeMode);
void BattleTower_SetWifiResultsPending(SaveData *saveData, u8 pending);
u16 BattleTower_HasWifiResultsPending(SaveData *saveData);
u16 BattleTower_ResetWifiProgress(SaveData *saveData);
u16 BattleTower_HasWifiOpponentData(SaveData *saveData);
void BattleTower_SetNull(BattleTower **battleTower);
BattleTower *BattleTower_Init(SaveData *saveData, u16 isResume, u16 challengeMode);
void BattleTower_Free(BattleTower *battleTower);
void BattleTower_StartPartyMenu(BattleTower *battleTower, FieldTask *task, void **partyMenu);
BOOL BattleTower_ReadPartyMenuSelection(BattleTower *battleTower, void **partyMenuPtr, SaveData *saveData);
int BattleTower_CheckDuplicateSpeciesAndHeldItems(BattleTower *battleTower, SaveData *saveData);
void BattleTower_GenerateOpponentTrainerIDs(BattleTower *battleTower, SaveData *saveData);
u16 BattleTower_GetNextOpponentNum(BattleTower *battleTower);
BOOL BattleTower_HasDefeatedSevenTrainers(BattleTower *battleTower);
void BattleTower_UpdateGameRecords(BattleTower *battleTower, SaveData *saveData);
void BattleTower_UpdateGameRecordsAndJournal(BattleTower *battleTower, SaveData *saveData, JournalEntry *journalEntry);
void BattleTower_SaveWifiState(BattleTower *battleTower);
void BattleTower_BuildPartnerData(BattleTower *battleTower);
u16 BattleTower_GetObjectIDFromOpponentID(BattleTower *battleTower, u16 opponentID);
u16 BattleTower_GetChallengeMode(BattleTower *battleTower);
u16 BattleTower_GetBeatPalmer(BattleTower *battleTower);
u16 BattleTower_GiveBattlePointsReward(BattleTower *battleTower);
u16 BattleTower_IsPalmerBattleAvailable(BattleTower *battleTower, SaveData *saveData);
u16 BattleTower_UpdateRank(BattleTower *battleTower, SaveData *saveData, u8 operation);
u16 BattleTower_GivePalmerRibbon(BattleTower *battleTower, SaveData *saveData);
u16 BattleTower_GiveModeAbilityRibbon(BattleTower *battleTower, SaveData *saveData);
u16 BattleTower_UpdateRandomSeed(BattleTower *battleTower, SaveData *saveData);
u8 BattleTower_GetIVsFromTrainerID(u16 battleTowerID);
u16 BattleTower_GetRandom(BattleTower *battleTower);

#endif // POKEPLATINUM_BATTLE_TOWER_H
