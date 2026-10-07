#ifndef POKEPLATINUM_STRUCT_DEF_COMM_CMD_TABLE_H
#define POKEPLATINUM_STRUCT_DEF_COMM_CMD_TABLE_H

#include "functypes/funcptr_02032868.h"
#include "functypes/funcptr_0203290C.h"
#include "functypes/funcptr_020F8E60.h"

typedef struct CommCmdTable {
    CommCmdHandler handler; // dispatches the command
    CommCmdPacketSizeFunc packetSize; // returns the payload size in bytes
    CommCmdRecvBufferFunc recvBuffer; // optional receive buffer provider
} CommCmdTable;

#endif // POKEPLATINUM_STRUCT_DEF_COMM_CMD_TABLE_H
