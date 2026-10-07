#ifndef POKEPLATINUM_COMM_TOOL_H
#define POKEPLATINUM_COMM_TOOL_H

#include "constants/heap.h"

void CommTool_Init(enum HeapID heapID);
void CommTool_Delete(void);
BOOL CommTool_IsInitialized(void);
void CommCmd_16(int netId, int param1, void *param2, void *param3);
void CommCmd_18(int netId, int param1, void *param2, void *param3);
void CommCmd_17(int netId, int param1, void *param2, void *param3);
void CommTiming_StartSync(u8 syncNo);
void CommTiming_Update(void);
BOOL CommTiming_IsSyncState(u8 syncState);
int CommTool_GetSyncNo(int netId);
void CommList_RecvEntry(int netId, int param1, void *param2, void *param3);
int CommList_EntrySize(void);
void CommList_Set(u8 key, u8 value);
int CommList_Get(int netId, u8 key);
void CommList_Refresh(void);
void CommTool_ClearReceivedTempDataAllPlayers(void);
BOOL CommTool_SendTempData(int netId, const void *data);
const void *CommTool_GetReceivedTempData(int netId);
void CommTool_RecvTempData(int netId, int param1, void *param2, void *param3);
int CommTool_TempDataSize(void);

#endif // POKEPLATINUM_COMM_TOOL_H
