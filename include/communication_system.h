#ifndef POKEPLATINUM_COMMUNICATION_SYSTEM_H
#define POKEPLATINUM_COMMUNICATION_SYSTEM_H

#include <nitro/math.h>

#define MAX_CONNECTED_PLAYERS 8

#define NETID_NONE 0xFF

#define PACKET_SIZE_VARIABLE 0xFFFF

#define COMM_RING_BUFFER_SIZE 264

BOOL CommSys_InitServer(BOOL param0, BOOL param1, int param2, BOOL param3);
BOOL CommSys_InitClient(BOOL param0, BOOL param1, int param2);
void CommSys_SwitchTransitionTypeToParallel(void);
void CommSys_SwitchTransitionTypeToServerClient(void);
BOOL CommSys_TransitionTypeIsParallel(void);
void CommSys_Delete(void);
BOOL CommSys_ConnectToServer(u16 param0);
BOOL CommSys_Update(void);
void CommSys_Reset(void);
void CommSys_ResetDS(void);
void CommSys_ResetBattleClient(void);
void CommSys_ClientRecvCallback(u16 param0, u16 *param1, u16 param2);
void CommSys_ServerRecvCallback(u16 param0, u16 *buffer, u16 param2);
void CommSys_RecvInputWifiGroup(u16 param0, u16 *param1, u16 param2);
void CommSys_RandomizePlayerMovement(void);
void CommSys_ReversePlayerMovement(void);
void CommSys_RevertPlayerMovementToNormal(void);
void CommSys_Dummy(void);
void CommSys_SetSendInterval(u8 param0);
BOOL CommSys_SendDataHuge(int cmd, const void *data, int size);
BOOL CommSys_SendData(int cmd, const void *data, int size);
BOOL CommSys_SendDataHugeServer(int cmd, const void *data, int size);
BOOL CommSys_SendDataServer(int cmd, const void *data, int size);
BOOL CommSys_SendDataFixedSizeServer(int cmd, const void *data);
int CommSys_SendRingRemainingSize(void);
BOOL CommSys_IsPlayerConnected(u16 netId);
int CommSys_ConnectedCount(void);
BOOL CommSys_IsInitialized(void);
void CommSys_SetSendSpeed(u8 param0);
u8 CommSys_RecvSpeed(int param0);
u16 CommSys_GetMovementKeys(int param0);
void CommSys_EnableSendMovementData(void);
void CommSys_DisableSendMovementData(void);
BOOL CommSys_IsSendingMovementData(void);
BOOL CommSys_WriteToQueueServer(int cmd, const void *data, int param2);
BOOL CommSys_WriteToQueue(int cmd, const void *data, int size);
void CommSys_HandleSwitchRequest(int unused0, int unused1, void *param2, void *unused3);
void CommSys_HandleSwitchPrepare(int unused0, int unused1, void *param2, void *unused3);
void CommSys_HandleSwitchAck(int unused0, int unused1, void *param2, void *unused3);
u16 CommSys_CurNetId(void);
BOOL CommSys_SendDataFixedSize(int cmd, const void *data);
BOOL CommSys_SendMessage(int cmd);
BOOL CommSys_IsClientConnecting(void);
BOOL CommSys_CheckError(void);
u16 CommSys_PlayerBlockSize(u16 param0);
int CommType_MaxPlayers(int param0);
int CommType_MinPlayers(int param0);
void CommSys_SetAlone(BOOL param0);
BOOL CommSys_IsAlone(void);
void CommSys_HandleFinishConnection(int param0, int param1, void *param2, void *param3);
void CommSys_Seed(MATHRandContext32 *rand);
BOOL CommSys_IsCmdQueuedServer(int cmd);
BOOL CommSys_IsCmdQueued(int cmd);
BOOL CommSys_IsServerQueueEmpty(void);
BOOL CommSys_IsQueueEmpty(void);
void CommSys_SetWifiConnected(BOOL param0);
BOOL CommSys_WifiConnected(void);
void CommSys_SetBattlePosition(int param0, int param1);
int CommSys_GetBattlePosition(int networkId);
BOOL CommSys_IsVoiceChatEnabled(void);
void CommSys_SetRecvLimitEnabled(BOOL param0);
void CommSys_SetBattleVoiceChat(BOOL param0);
BOOL CommSys_IsInputPending(void);
void CommSys_SetError(void);
void CommSys_StartShutdown(void);

#endif // POKEPLATINUM_COMMUNICATION_SYSTEM_H
