#ifndef POKEPLATINUM_BATTLE_TOWER_PARTNER_H
#define POKEPLATINUM_BATTLE_TOWER_PARTNER_H

#include "struct_defs/battle_tower.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "field/field_system_decl.h"
#include "overlay104/frontier_opponents.h"

#include "savedata.h"
#include "string_template.h"

StringTemplate *BattleFrontier_MakeSeenBanlistSpeciesMsg(SaveData *saveData, u16 numPokemonRequired, u16 unused2, u8 unused3, u8 *outNumBannedSeen);
u16 BattleTower_GetObjectIDFromTrainerClass(u8 trainerClass);
u16 BattleTower_ReceivePartnerData(FieldSystem *fieldSystem, const u16 *partnerData);
u16 BattleTower_ReceivePartnerTrainerIDs(FieldSystem *fieldSystem, const u16 *trainerIDs);
u16 BattleTower_IsPartnerDataReady(FieldSystem *fieldSystem, const u16 *partnerData);
void BattleTower_BuildPartnerDataPacket(BattleTower *battleTower, SaveData *saveData);
void BattleTower_BuildTrainerIDPacket(BattleTower *battleTower);
void BattleTower_SetPartnerReady(BattleTower *battleTower, u16 ready);
u16 BattleTower_GetTrainerIDForRoomAndOpponentNum(BattleTower *battleTower, u8 roomNum, u8 opponentNum, int challengeMode);
BOOL BattleTower_BuildPartnerOpponent(BattleTower *battleTower, FrontierOpponent *opponent, u16 partnerBattleTowerID, int partySize, u16 *partnerSpecies, u16 *partnerItems, BattleTowerPartnerData *partnerData, enum HeapID heapID);
void BattleTower_LoadPartnerOpponent(BattleTower *battleTower, FrontierOpponent *opponent, u16 partnerBattleTowerID, BOOL useDefaultItems, const BattleTowerPartnerData *partnerData, enum HeapID heapID);

#endif // POKEPLATINUM_BATTLE_TOWER_PARTNER_H
