#ifndef POKEPLATINUM_COMM_SERVER_CLIENT_H
#define POKEPLATINUM_COMM_SERVER_CLIENT_H

#include <nitro/wm.h>

#include "struct_defs/struct_0203330C.h"

#include "easy_chat_sentence.h"
#include "trainer_info.h"

// Wireless server/client manager for local (non-Wi-Fi) multiplayer. See
// src/comm_server_client.c for the implementation.

void CommServerClient_Init(TrainerInfo *trainerInfo, BOOL isNotListening);
BOOL CommServerClient_IsAllocated(void);
void WirelessDriver_Init(void);
BOOL WirelessDriver_IsReady(void);
BOOL WirelessDriver_Initialized(void);
void WirelessDriver_Shutdown(void);
void CommServerClient_ClearScanResults(void);
BOOL CommServerClient_InitServer(BOOL param0, BOOL incrementTGID, BOOL entryFlag);
BOOL CommServerClient_InitClient(BOOL param0, BOOL clearScanResults);
BOOL CommServerClient_ShutdownWirelessManager(void);
BOOL CommServerClient_Shutdown(void);
void CommServerClient_SetSecretBaseClosedState(BOOL isClosed);
int CommServerClient_CountDiscoveredServers(void);
int CommServerClient_GetNthServerIndex(int index);
BOOL CommServerClient_IsServerListUpdated(void);
void CommServerClient_ClearServerListUpdated(void);
int CommServerClient_GetServerPlayerCount(int index);
int CommServerClient_FindServerToJoin(void);
int CommServerClient_FindAnyServer(void);
void CommServerClient_CopyServerTrainerInfo(int index, TrainerInfo *dest);
BOOL CommServerClient_ConnectToServer(u16 index);
void CommServerClient_UpdateServerList(void);
void CommServerClient_Update(u16 timestamp);
BOOL CommServerClient_IsInClosedSecretBase(void);
BOOL CommServerClient_IsInitialized(void);
BOOL CommServerClient_IsIdle(void);
BOOL CommServerClient_IsClientConnecting(void);
BOOL CommServerClient_IsDisconnected(void);
BOOL CommServerClient_CheckError(void);
void CommServerClient_SetErrorDisconnect(BOOL errorDisconnect);
void CommServerClient_SetErrorTimeout(BOOL enabled);
WMBssDesc *CommServerClient_GetServerBssDesc(int index);
UnkStruct_0203330C *CommServerClient_GetServerGameInfo(int index);
TrainerInfo *CommServerClient_GetPersonalTrainerInfo(void);
TrainerInfo *CommServerClient_GetServerTrainerInfo(int index);
void CommServerClient_SetPlayerMacAddress(u8 *macAddress, int netId);
BOOL CommServerClient_IsFinished(void);
void CommServerClient_SetFinished(void);
void CommServerClient_SetEasyChatSentence(EasyChatSentence *sentence);
void CommServerClient_SetBattleRegulation(void *regulation);
void *CommServerClient_GetBattleRegulation(void);
void CommServerClient_SendGameInfo(void);
int CommServerClient_CountPlayersByCommType(int commType);
BOOL CommServerClient_ServerSentAllBeacons(void);
void CommServerClient_SetMysteryGiftEventData(void *eventData);
const void *CommServerClient_GetMysteryGiftEventData(int index);

#endif // POKEPLATINUM_COMM_SERVER_CLIENT_H
