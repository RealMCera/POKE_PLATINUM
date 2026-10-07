#ifndef POKEPLATINUM_COMMUNICATION_INFORMATION_H
#define POKEPLATINUM_COMMUNICATION_INFORMATION_H

#include <dwc.h>

#include "battle_regulation.h"
#include "savedata.h"
#include "trainer_info.h"

// Buffers the identity and receive state of every player in a multiplayer
// session. See src/communication_information.c for the state machine.

void CommInfo_Init(SaveData *saveData, const BattleRegulation *regulation);
void CommInfo_Delete(void);
BOOL CommInfo_IsInitialized(void);
void CommInfo_SendPlayerInfo(void);
int CommPlayerInfo_Size(void);
void CommInfo_FinishReading(int unused0, int unused1, void *unused2, void *unused3);
BOOL CommInfo_IsDataFinishedReading(void);
void CommInfo_RecvPlayerDataArray(int netId, int unused1, void *src, void *unused3);
void CommInfo_RecvPlayerData(int netId, int unused1, void *src, void *unused3);
BOOL CommInfo_ServerSendArray(void);
BOOL CommInfo_IsReceivingData(void);
void CommInfo_InitPlayer(int netId);
BOOL CommInfo_HasNewData(int netId);
BOOL CommInfo_HasPlayerData(int netId);
BOOL CommInfo_IsDataRead(int netId);
void CommInfo_MarkDataRead(int netId);
void CommInfo_SetReceiveEnd(int netId);
int CommInfo_NewNetworkId(void);
int CommInfo_CountReceived(void);
BOOL CommInfo_ClearDisconnectedPlayers(void);
TrainerInfo *CommInfo_TrainerInfo(int netId);
DWCFriendData *CommInfo_DWCFriendData(int netId);
int CommInfo_FindFriendSlotForNetId(int netId);
u16 *CommInfo_GroupName(int netId);
int CommInfo_PlayerCountry(int netId);
int CommInfo_PlayerRegion(int netId);
BOOL CommInfo_PlayerHasGiftPenalty(int netID);
BOOL CommInfo_CheckBattleRegulation(void);
void CommInfo_SavePlayerRecord(SaveData *saveData);
void CommInfo_RecordBattleResult(SaveData *saveData, int result);
void CommInfo_SetTradeResult(SaveData *saveData, int count);
void CommInfo_SetPersonalTrainerInfo(TrainerInfo *trainerInfo);

#endif // POKEPLATINUM_COMMUNICATION_INFORMATION_H
