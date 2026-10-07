#include "union_room_comm.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"
#include "struct_defs/struct_0209BDF8.h"
#include "struct_defs/struct_0209BF64.h"
#include "struct_defs/struct_0209C0F0.h"
#include "struct_defs/struct_0209C194.h"

#include "functypes/funcptr_020F8E60.h"
#include "overlay109/ov109_021D0D80.h"
#include "overlay109/ov109_021D3D50.h"
#include "overlay109/struct_ov109_021D1048.h"
#include "overlay109/struct_ov109_021D17EC.h"

#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "heap.h"
#include "comm_cmd.h"
#include "union_room.h"
#include "union_room_trainers.h"
#include "comm_field_cmd.h"
#include "wireless_manager.h"

// Size of one serialized party: six 236-byte Pokémon plus four u16 fields.
#define TRAINER_DATA_SIZE (236 * 6 + 4 * 2)

// Size of the generic command envelope: a u32 sub-command followed by up to
// 20 bytes of payload.
#define COMMAND_PACKET_SIZE 24

// Number of sub-commands handled by sUnionRoomCommHandlers.
#define COMMAND_COUNT 18

// Envelope for the generic command (command 130). The sub-command selects one
// of the handlers in sUnionRoomCommHandlers.
typedef struct UnionRoomCommPacket {
    u32 command;
    u8 payload[20];
} UnionRoomCommPacket;

static BOOL UnionRoomComm_SendCommand(UnionRoomComm *comm, u32 command, const void *data, u32 size);

static const CommCmdTable sUnionRoomCommCmdTable[135];
static const CommCmdHandler sUnionRoomCommHandlers[COMMAND_COUNT];

UnionRoomComm *UnionRoomComm_New(UnionRoomSpinTradeSession *app, enum HeapID heapID)
{
    UnionRoomComm *comm = Heap_Alloc(heapID, sizeof(UnionRoomComm));
    GF_ASSERT(comm != NULL);
    memset(comm, 0, sizeof(UnionRoomComm));

    comm->app = app;
    comm->sendTrainerData = Heap_Alloc(heapID, 5 * TRAINER_DATA_SIZE);
    comm->recvTrainerData = Heap_Alloc(heapID, 5 * TRAINER_DATA_SIZE);

    return comm;
}

void UnionRoomComm_Free(UnionRoomComm *comm)
{
    Heap_Free(comm->sendTrainerData);
    Heap_Free(comm->recvTrainerData);
    Heap_Free(comm);
}

void UnionRoomComm_Init(UnionRoomComm *comm)
{
    CommCmd_Init(sUnionRoomCommCmdTable, 135, comm);
}

void UnionRoomComm_Reset(UnionRoomComm *comm)
{
    CommManager_SetMaxNumConnections(2);
    CommManager_UnionRestartSearch();
    UnionRoomTrainers_Reset(comm->app->context.trainers);
    UnionRoom_BroadcastActivity(0);
}

// Builds a generic command envelope in the comm's send buffer and transmits it
// as command 130. `command` selects the sub-handler on the receiving side and
// `size` must leave room for the 4-byte sub-command header.
static BOOL UnionRoomComm_SendCommand(UnionRoomComm *comm, u32 command, const void *data, u32 size)
{
    GF_ASSERT(command < COMMAND_COUNT);
    GF_ASSERT(size + 4 <= COMMAND_PACKET_SIZE);

    {
        BOOL result;
        UnionRoomCommPacket *packet = (void *)comm->sendBuffer;

        packet->command = command;
        memcpy(packet->payload, data, size);
        result = CommSys_SendData(130, packet, COMMAND_PACKET_SIZE);

        return result;
    }
}

BOOL UnionRoomComm_Send(UnionRoomComm *comm, u32 command, const void *data, u32 size)
{
    if (comm->sendDisabled == 1) {
        return 0;
    }

    return UnionRoomComm_SendCommand(comm, command, data, size);
}

// Receives a generic command envelope (command 130) and dispatches to the
// handler selected by the sub-command.
static void UnionRoomComm_HandleCommand(int netId, int unused, void *data, void *context)
{
    UnionRoomCommPacket *packet = data;

    if (packet->command >= COMMAND_COUNT) {
        GF_ASSERT(FALSE);
        return;
    }

    sUnionRoomCommHandlers[packet->command](netId, unused, packet->payload, context);
}

// Receives a player's serialized party (command 131) and stores it in the
// per-player receive buffer.
static void UnionRoomComm_HandleTrainerData(int netId, int unused, void *data, void *context)
{
    void *dest;
    UnionRoomComm *comm = context;

    comm->trainerDataBitmap |= 1 << netId;
    dest = UnionRoomComm_GetRecvTrainerData(comm, netId);
    memcpy(dest, data, TRAINER_DATA_SIZE);
}

// Sub-command 7: records the local player's confirm flag.
static void UnionRoomComm_HandleConfirm(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    u8 *value = (u8 *)data;

    if (netId == 0) {
        if (comm->confirmed != *value) {
            (void)0;
        }

        comm->confirmed = *value;
    }
}

// Sub-command 6: counts players that have reported in. The command is never
// sent, so this handler is currently unreachable.
static void UnionRoomComm_HandleIncrementCount(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;

    comm->receivedCount++;
}

// Sub-command 5: a peer has dropped out of the room.
static void UnionRoomComm_HandleDisconnect(int netId, int unused, void *data, void *context)
{
    if (CommSys_CurNetId() != 0) {
        UnionRoomComm *comm = context;

        comm->receivedCount = 0;
        comm->disconnected = 1;

        ov109_021D5140(comm->app->groupAppData, 31, netId);
    }
}

// Sub-command 1: closes the message box. The command is never sent, so this
// handler is currently unreachable.
static void UnionRoomComm_HandleEraseMessage(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    ov109_021D5140(comm->app->groupAppData, 2, 0);
}

// Sub-command 2: connection-confirmation handshake. A client asks the server
// to confirm it; the server validates the player count and replies with an
// accept/reject flag. Type 1 announces a player leaving.
static void UnionRoomComm_HandleConnectionConfirm(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    UnionRoomCommConfirm reply;
    UnionRoomCommConfirm *request = data;

    if (netId != 0) {
        if (CommSys_CurNetId() == 0) {
            reply = *request;
            reply.netId = netId;
            reply.playerCount = comm->playerCount;

            switch (request->type) {
            case 0:
                if ((comm->playerCount != CommSys_ConnectedCount()) || (comm->playerCount != UnionRoomComm_CountConnectedTrainers()) || (comm->playerCount != MATH_CountPopulation(WirelessManager_GetConnectedBitmap()))) {
                    reply.accepted = 0;
                } else {
                    comm->confirmedBitmap |= 1 << netId;
                    reply.accepted = 1;

                    CommManager_SetMaxNumConnections(CommSys_ConnectedCount());
                }
                break;
            case 1:
                break;
            }

            UnionRoomComm_Send(comm, 2, &reply, sizeof(UnionRoomCommConfirm));
        }
    } else {
        switch (request->type) {
        case 0:
            if (request->netId == CommSys_CurNetId()) {
                if (request->accepted == 0) {
                    ov109_021D5140(
                        comm->app->groupAppData, 8, request->netId);
                } else {
                    comm->serverPlayerCount = request->playerCount;
                    ov109_021D5140(
                        comm->app->groupAppData, 7, request->netId);
                }
            }
            break;
        case 1:
            ov109_021D5140(comm->app->groupAppData, 19, request->netId);
            break;
        }
    }
}

// Sub-command 0: a player has become ready. The server also clears its
// pending-confirmation counter.
static void UnionRoomComm_HandlePlayerReady(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    u8 readyNetId = *(u8 *)data;
    ov109_021D5258(comm->app->groupAppData, 1, readyNetId);

    if (CommSys_CurNetId() == 0) {
        comm->unk_34 = 0;
    }
}

// Sub-command 3: a client cancelled the trade.
static void UnionRoomComm_HandleCancelTrade(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;

    if (CommSys_CurNetId() != 0) {
        ov109_021D5140(comm->app->groupAppData, 13, 0);
    }
}

// Sub-command 4: a client is ready. The server rebroadcasts it as sub-command
// 0 so every client learns the player's net ID.
static void UnionRoomComm_HandleReadyRequest(int netId, int unused, void *data, void *context)
{
    u8 readyNetId;
    UnionRoomComm *comm = context;

    if (CommSys_CurNetId() == 0) {
        readyNetId = netId;
        UnionRoomComm_Send(comm, 0, &readyNetId, 1);
    }
}

// Sub-command 8: ORs a stage bitmask into the completed-stage flags.
static void UnionRoomComm_HandleSetStageFlags(int netId, int unused, void *data, void *context)
{
    u16 *flags = data;
    UnionRoomComm *comm = context;

    comm->stageFlags |= *flags;
}

// Sub-command 9: marks a player as taking part in the trade.
static void UnionRoomComm_HandleSetParticipant(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    comm->participantBitmap |= 1 << netId;
}

// Sub-command 12: stores a player's position in the trade order.
static void UnionRoomComm_HandlePlayerOrder(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    UnkStruct_ov109_021D1048 *order = data;

    ov109_021D3B24(comm->app->spinAppData, order);
}

// Sub-command 13: sets the expected player count.
static void UnionRoomComm_HandleSetPlayerCount(int netId, int unused, void *data, void *context)
{
    int count = *(int *)data;
    UnionRoomComm *comm = context;

    ov109_021D3B50(comm->app->spinAppData, count);
}

// Sub-command 10: sets a remote animation state.
static void UnionRoomComm_HandleSetState(int netId, int unused, void *data, void *context)
{
    u8 state = *(u8 *)data;
    UnionRoomComm *comm = context;

    ov109_021D3A68(comm->app->spinAppData, state);
}

// Sub-command 11: applies a remote player's spin-trade position.
static void UnionRoomComm_HandleSpinTradePos(int netId, int unused, void *data, void *context)
{
    if (CommSys_CurNetId() != 0) {
        UnionRoomComm *comm = context;
        UnionRoomCommSpinTradePos *pos = data;

        ov109_021D3A70(comm->app->spinAppData, pos);
    }
}

// Sub-command 14: records which party slot a player selected.
static void UnionRoomComm_HandleSetMonSlot(int netId, int unused, void *data, void *context)
{
    int slot = *(int *)data;
    UnionRoomComm *comm = context;

    ov109_021D3BE4(comm->app->spinAppData, netId, slot);
}

// Sub-command 15: stores the final spin-trade result.
static void UnionRoomComm_HandleSpinTradeResult(int netId, int unused, void *data, void *context)
{
    UnionRoomComm *comm = context;
    UnkStruct_ov109_021D17EC *result = data;

    ov109_021D3BEC(comm->app->spinAppData, result);
}

// Sub-command 16: marks a player as having a bad egg.
static void UnionRoomComm_HandleBadEgg(int netId, int unused, void *data, void *context)
{
    u32 bit = 1 << netId;
    UnionRoomComm *comm = context;

    comm->badEggBitmap |= bit;
}

// Sub-command 17: marks a player as having reported their eggs are OK.
static void UnionRoomComm_HandleEggOk(int netId, int unused, void *data, void *context)
{
    u32 bit = 1 << netId;
    UnionRoomComm *comm = context;

    comm->eggOkBitmap |= bit;
}

static int UnionRoomComm_CommandPacketSize(void)
{
    return COMMAND_PACKET_SIZE;
}

static int UnionRoomComm_TrainerDataPacketSize(void)
{
    return TRAINER_DATA_SIZE;
}

// Buffer accessor for the outgoing party-data command (command 131).
static u8 *UnionRoomComm_GetSendTrainerData(int netId, void *context, int unused)
{
    u32 address;
    UnionRoomComm *comm = context;

    address = (u32)(comm->sendTrainerData);
    address += netId * TRAINER_DATA_SIZE;
    return (u8 *)address;
}

int UnionRoomComm_CountConnectedTrainers(void)
{
    int i, result;

    for (result = 0, i = 0; i < 5; i++) {
        if (CommInfo_TrainerInfo(i) != NULL) {
            result++;
        }
    }

    return result;
}

void *UnionRoomComm_GetRecvTrainerData(UnionRoomComm *comm, int netId)
{
    u32 address = (u32)(comm->recvTrainerData);
    address += netId * TRAINER_DATA_SIZE;

    return (void *)address;
}

// Command table for the Union Room. CommCmd_Init indexes this table by
// (command - 22), so the first 108 entries cover commands 22..129, which the
// Union Room does not use. The last two initialized entries are the module's
// commands:
//   130 - generic command envelope, dispatched by sub-command
//   131 - serialized party data
// The remaining entries are zero-initialized and unused.
static const CommCmdTable sUnionRoomCommCmdTable[135] = {
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { UnionRoomComm_HandleCommand, UnionRoomComm_CommandPacketSize, NULL },
    { UnionRoomComm_HandleTrainerData, UnionRoomComm_TrainerDataPacketSize, UnionRoomComm_GetSendTrainerData }
};

// Sub-command handlers for the generic command envelope (command 130). The
// index is the sub-command stored in UnionRoomCommPacket.command.
static const CommCmdHandler sUnionRoomCommHandlers[COMMAND_COUNT] = {
    UnionRoomComm_HandlePlayerReady,
    UnionRoomComm_HandleEraseMessage,
    UnionRoomComm_HandleConnectionConfirm,
    UnionRoomComm_HandleCancelTrade,
    UnionRoomComm_HandleReadyRequest,
    UnionRoomComm_HandleDisconnect,
    UnionRoomComm_HandleIncrementCount,
    UnionRoomComm_HandleConfirm,
    UnionRoomComm_HandleSetStageFlags,
    UnionRoomComm_HandleSetParticipant,
    UnionRoomComm_HandleSetState,
    UnionRoomComm_HandleSpinTradePos,
    UnionRoomComm_HandlePlayerOrder,
    UnionRoomComm_HandleSetPlayerCount,
    UnionRoomComm_HandleSetMonSlot,
    UnionRoomComm_HandleSpinTradeResult,
    UnionRoomComm_HandleBadEgg,
    UnionRoomComm_HandleEggOk
};
