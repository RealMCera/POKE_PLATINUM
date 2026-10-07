#ifndef POKEPLATINUM_COMM_CMD_H
#define POKEPLATINUM_COMM_CMD_H

#include "struct_defs/comm_cmd_table.h"

void CommCmd_Init(const CommCmdTable *cmdTable, int cmdCount, void *context);
void CommCmd_Free(void);
void CommCmd_Callback(int netId, int cmd, int size, void *data);
int CommCmd_PacketSizeOf(int cmd);
BOOL CommCmd_HasRecvBuffer(int cmd);
void *CommCmd_GetRecvBuffer(int cmd, int netId, int size);
int CommPacketSizeOf_Variable(void);
int CommPacketSizeOf_Nothing(void);
int CommPacketSizeOf_NetId(void);

#endif // POKEPLATINUM_COMM_CMD_H
