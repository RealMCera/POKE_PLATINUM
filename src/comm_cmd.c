#include "comm_cmd.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"

#include "functypes/funcptr_02032868.h"
#include "functypes/funcptr_0203290C.h"
#include "functypes/funcptr_020F8E60.h"

#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "heap.h"
#include "unk_020363E8.h"

// Commands 0..21 are shared by every comm application and are handled by the
// built-in table below. Application-specific commands start at 22 and are
// looked up in the table registered with CommCmd_Init.
#define COMM_CMD_BUILTIN_COUNT 22

// One ready flag per netId; the barrier tracks at most MAX_CONNECTED_PLAYERS
// (8) players.
#define COMM_CMD_MAX_PLAYERS 8

typedef struct {
    const CommCmdTable *cmdTable; // application-specific command table
    int cmdCount; // number of entries in cmdTable
    void *context; // opaque context forwarded to command handlers
    u8 playerReady[COMM_CMD_MAX_PLAYERS]; // per-netId ready flags for the barrier
    u8 unk_14; // set when the manager is torn down; never read
} CommCmdManager;

static void CommCmd_HandlePlayerReady(int netId, int unused, void *data, void *context);
static void CommCmd_HandleAllPlayersReady(int netId, int unused, void *data, void *context);
static void CommCmd_HandleReadyAck(int netId, int unused, void *data, void *context);
static int CommPacketSizeOf_Two(void);

// Built-in command table for commands 0..21. Entries 8 and 9 are unused, and
// entries 13..15 implement the ready barrier handled by the functions below.
static const CommCmdTable sBuiltInCmdTable[] = {
    { NULL, CommPacketSizeOf_Nothing, NULL },
    { NULL, CommPacketSizeOf_Nothing, NULL },
    { CommSys_HandleFinishConnection, CommPacketSizeOf_Nothing, NULL },
    { CommInfo_RecvPlayerData, CommPlayerInfo_Size, NULL },
    { CommInfo_RecvPlayerDataArray, CommPlayerInfo_Size, NULL },
    { CommInfo_FinishReading, CommPacketSizeOf_Nothing, NULL },
    { CommManager_ValidateConfirmationMessage, CommManager_ConfirmationMessage_sizeof, NULL },
    { CommManager_ValidateConfirmationResponseMessage, CommManager_ConfirmationMessage_sizeof, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { CommSys_HandleSwitchRequest, CommPacketSizeOf_NetId, NULL },
    { CommSys_HandleSwitchPrepare, CommPacketSizeOf_NetId, NULL },
    { CommSys_HandleSwitchAck, CommPacketSizeOf_NetId, NULL },
    { CommCmd_HandlePlayerReady, CommPacketSizeOf_Nothing, NULL },
    { CommCmd_HandleAllPlayersReady, CommPacketSizeOf_Nothing, NULL },
    { CommCmd_HandleReadyAck, CommPacketSizeOf_Nothing, NULL },
    { CommCmd_16, CommPacketSizeOf_NetId, NULL },
    { CommCmd_17, CommPacketSizeOf_NetId, NULL },
    { CommCmd_18, CommPacketSizeOf_Two, NULL },
    { sub_02036574, sub_02036590, NULL },
    { sub_02036670, CommTool_TempDataSize, NULL },
    { CommManager_DisconnectWifi, CommPacketSizeOf_Nothing, NULL }
};

static CommCmdManager *sCommCmdManager = NULL;

void CommCmd_Init(const CommCmdTable *cmdTable, int cmdCount, void *context)
{
    int i;

    if (!sCommCmdManager) {
        sCommCmdManager = Heap_Alloc(HEAP_ID_COMMUNICATION, sizeof(CommCmdManager));
    }

    sCommCmdManager->cmdTable = cmdTable;
    sCommCmdManager->cmdCount = cmdCount;
    sCommCmdManager->context = context;

    for (i = 0; i < COMM_CMD_MAX_PLAYERS; i++) {
        sCommCmdManager->playerReady[i] = 0;
    }

    sCommCmdManager->unk_14 = 0;
}

void CommCmd_Free(void)
{
    if (sCommCmdManager) {
        Heap_Free(sCommCmdManager);
        sCommCmdManager = NULL;
    }
}

// Dispatches `cmd` to its handler. Commands below COMM_CMD_BUILTIN_COUNT use
// the built-in table; higher commands index the registered table at
// (cmd - COMM_CMD_BUILTIN_COUNT). The handler receives the manager's context
// as its fourth argument.
void CommCmd_Callback(int netId, int cmd, int size, void *data)
{
    CommCmdHandler handler;

    if (cmd < COMM_CMD_BUILTIN_COUNT) {
        handler = sBuiltInCmdTable[cmd].handler;
    } else {
        GF_ASSERT(sCommCmdManager);

        if (cmd > (sCommCmdManager->cmdCount + COMM_CMD_BUILTIN_COUNT)) {
            CommSys_SetError();
            return;
        }

        handler = sCommCmdManager->cmdTable[cmd - COMM_CMD_BUILTIN_COUNT].handler;
    }

    if (handler != NULL) {
        if (sCommCmdManager) {
            handler(netId, size, data, sCommCmdManager->context);
        } else {
            handler(netId, size, data, NULL);
        }
    }
}

// Returns the payload size in bytes for `cmd`, or PACKET_SIZE_VARIABLE when the
// size is encoded in the packet itself.
int CommCmd_PacketSizeOf(int cmd)
{
    int size = 0;
    CommCmdPacketSizeFunc packetSize;

    if (cmd < COMM_CMD_BUILTIN_COUNT) {
        packetSize = sBuiltInCmdTable[cmd].packetSize;
    } else {
        GF_ASSERT(sCommCmdManager);

        if (sCommCmdManager == NULL) {
            CommSys_SetError();
            return size;
        }

        if (cmd > (sCommCmdManager->cmdCount + COMM_CMD_BUILTIN_COUNT)) {
            GF_ASSERT(FALSE);
            CommSys_SetError();
            return size;
        }

        packetSize = sCommCmdManager->cmdTable[cmd - COMM_CMD_BUILTIN_COUNT].packetSize;
    }

    if (packetSize != NULL) {
        size = packetSize();
    }

    return size;
}

// Returns TRUE if `cmd` provides a receive buffer via its recvBuffer callback.
BOOL CommCmd_HasRecvBuffer(int cmd)
{
    if (cmd < COMM_CMD_BUILTIN_COUNT) {
        return sBuiltInCmdTable[cmd].recvBuffer != NULL;
    }

    return sCommCmdManager->cmdTable[cmd - COMM_CMD_BUILTIN_COUNT].recvBuffer != NULL;
}

// Asks `cmd`'s recvBuffer callback for the buffer that will receive a packet of
// `size` bytes. The manager's context is passed through for application
// commands; built-in commands receive NULL.
void *CommCmd_GetRecvBuffer(int cmd, int netId, int size)
{
    CommCmdRecvBufferFunc recvBuffer;

    if (cmd < COMM_CMD_BUILTIN_COUNT) {
        recvBuffer = sBuiltInCmdTable[cmd].recvBuffer;
        return recvBuffer(netId, NULL, size);
    } else {
        recvBuffer = sCommCmdManager->cmdTable[cmd - COMM_CMD_BUILTIN_COUNT].recvBuffer;
        return recvBuffer(netId, sCommCmdManager->context, size);
    }

    return NULL;
}

int CommPacketSizeOf_Variable(void)
{
    return PACKET_SIZE_VARIABLE;
}

int CommPacketSizeOf_Nothing(void)
{
    return 0;
}

int CommPacketSizeOf_NetId(void)
{
    return 1;
}

// Command 18 carries a netId followed by a sync number (2 bytes).
static int CommPacketSizeOf_Two(void)
{
    return 2;
}

// Command 13: a player reports ready. Only the server (netId 0) collects the
// flags; once every currently-connected player has reported, it broadcasts
// command 14.
static void CommCmd_HandlePlayerReady(int netId, int unused, void *data, void *context)
{
    u8 *unusedData = data;
    int i;

    if (CommSys_CurNetId() != 0) {
        return;
    }

    sCommCmdManager->playerReady[netId] = 1;

    for (i = 0; i < COMM_CMD_MAX_PLAYERS; i++) {
        if (!CommSys_IsPlayerConnected(i)) {
            continue;
        }

        if (!sCommCmdManager->playerReady[i]) {
            return;
        }
    }

    CommSys_SendDataServer(14, NULL, 0);
}

// Command 14: every player is ready. Tear down the registered command table and
// echo command 15 back so the server can clear its ready flags.
static void CommCmd_HandleAllPlayersReady(int netId, int unused, void *data, void *context)
{
    u8 *unusedData = data;
    int unusedLocal;

    sCommCmdManager->cmdTable = NULL;
    sCommCmdManager->cmdCount = 0;
    sCommCmdManager->context = NULL;
    sCommCmdManager->unk_14 = 1;

    CommSys_SendDataFixedSize(15, data);
}

// Command 15: server-side acknowledgement that clears the sender's ready flag.
static void CommCmd_HandleReadyAck(int netId, int unused, void *data, void *context)
{
    u8 *unusedData = data;
    int unusedLocal;

    if (CommSys_CurNetId() != 0) {
        return;
    }

    sCommCmdManager->playerReady[netId] = 0;
}
