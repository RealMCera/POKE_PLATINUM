#include "union_room_drawing_comm.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"
#include "struct_defs/struct_02095EAC_sub1.h"
#include "struct_defs/struct_02095EAC_t.h"
#include "struct_defs/struct_02095FE4.h"

#include "overlay058/ov58_021D0D80.h"
#include "overlay058/struct_ov58_021D2820.h"

#include "bg_window.h"
#include "comm_manager.h"
#include "communication_system.h"
#include "comm_cmd.h"
#include "comm_field_cmd.h"
#include "wireless_manager.h"

// Communication command handlers for the Union Room drawing (oekaki) app
// (overlay058). Up to five players share a 30x15-tile canvas: the server
// streams the canvas to the clients in 1000-byte chunks (command 118) while
// the players exchange their pen/cursor status (commands 119 and 120). The
// remaining commands drive the join handshake and the start/end of a drawing
// session.
//
// CommCmd_Init indexes the table below by (command - 22), so entry N handles
// command N + 22.

typedef struct UnionRoomDrawing UnionRoomDrawing;

static u8 *UnionRoomDrawing_GetChunkBuffer(int netId, void *app, int requestedSize);
static int UnionRoomDrawing_GetHandshakePacketSize(void);
static int UnionRoomDrawing_GetConnAckPacketSize(void);
static void UnionRoomDrawing_SendChunk(UnionRoomDrawing *app, int chunkIndex);
static void UnionRoomDrawing_HandleResetDrawingState(int senderNetId, int unused, void *data, void *app);

static const CommCmdTable sUnionRoomDrawingCommHandlers[] = {
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, UnionRoomDrawing_GetHandshakePacketSize, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { UnionRoomDrawing_HandleChunkTransfer, CommFieldCmd_PacketSizeOf_DrawingChunk, UnionRoomDrawing_GetChunkBuffer },
    { UnionRoomDrawing_ReceivePlayerStatus, CommFieldCmd_PacketSizeOf_DrawingPlayerStatus },
    { UnionRoomDrawing_ReceiveAllStatuses, CommFieldCmd_PacketSizeOf_DrawingAllStatuses },
    { UnionRoomDrawing_ReceiveUnusedCmd121, CommPacketSizeOf_NetId },
    { UnionRoomDrawing_ReceiveUnusedCmd122, CommPacketSizeOf_NetId },
    { UnionRoomDrawing_HandleBeginDrawing, CommPacketSizeOf_NetId },
    { UnionRoomDrawing_HandleTransferComplete, CommPacketSizeOf_Nothing },
    { UnionRoomDrawing_ReceiveUnusedCmd125, CommPacketSizeOf_Nothing },
    { UnionRoomDrawing_HandleConnectionAck, UnionRoomDrawing_GetConnAckPacketSize },
    { UnionRoomDrawing_HandleServerEndDrawing, CommPacketSizeOf_Nothing },
    { UnionRoomDrawing_HandleClientReady, CommPacketSizeOf_Nothing },
    { UnionRoomDrawing_HandleResetDrawingState, CommPacketSizeOf_Nothing },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL }
};

// Registers the drawing app's command handlers. `app` is the UnionRoomDrawing
// state passed back to every handler as its final argument.
void UnionRoomDrawing_RegisterCommHandlers(void *app)
{
    int handlerCount = sizeof(sUnionRoomDrawingCommHandlers) / sizeof(CommCmdTable);
    CommCmd_Init(sUnionRoomDrawingCommHandlers, handlerCount, app);
}

// Command 118: a 1000-byte slice of the shared canvas. Clients copy the slice
// into their local canvas buffer and redraw the drawing window; the server
// advances the transfer, sending the next slice or signalling completion with
// command 124 once the whole canvas has been sent.
void UnionRoomDrawing_HandleChunkTransfer(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;

    if (CommSys_CurNetId() != 0) {
        UnionRoomDrawingChunk *chunk = (UnionRoomDrawingChunk *)data;

        // The canvas is 30 * 15 tiles of 32 bytes each; the final slice is
        // shorter than a full 1000-byte chunk.
        if (chunk->index * 1000 > 30 * 15 * 32) {
            MI_CpuCopyFast(chunk->data, &app->drawingTiles[chunk->index * 1000], (30 * 15 * 32) % 1000);
        } else {
            MI_CpuCopyFast(chunk->data, &app->drawingTiles[chunk->index * 1000], 1000);
        }

        MI_CpuCopyFast(app->drawingTiles, app->drawingWindow.pixels, 30 * 15 * 32);
        Window_CopyToVRAM(&app->drawingWindow);
    } else {
        UnionRoomDrawingChunk *chunk = (UnionRoomDrawingChunk *)data;

        if (app->sendChunkIndex * 1000 < 30 * 15 * 32) {
            app->sendChunkIndex++;
            UnionRoomDrawing_SendChunk(app, app->sendChunkIndex);
        } else {
            CommSys_SendDataServer(124, NULL, 0);
        }
    }
}

// Command 119: a single player's pen/cursor status, broadcast by that player.
// The server stores it in the per-player array it later relays.
void UnionRoomDrawing_ReceivePlayerStatus(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    UnkStruct_ov58_021D2820 *status = (UnkStruct_ov58_021D2820 *)data;

    if (senderNetId != 0) {
        app->drawingStatusBroadcast[senderNetId] = *status;
    }
}

// Command 120: the server relays all five players' pen/cursor statuses at once.
void UnionRoomDrawing_ReceiveAllStatuses(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    UnkStruct_ov58_021D2820 *statuses = (UnkStruct_ov58_021D2820 *)data;

    if (app == NULL) {
        return;
    }

    if (senderNetId == 0) {
        int i;

        for (i = 0; i < 5; i++) {
            app->drawingStatus[i] = statuses[i];
        }
    }

    if (app->drawingStatus[0].unk_09 == 2) {
        (void)0;
    }
}

// Command 124: the server has finished streaming the canvas. Clear the local
// canvas and, on the server, refresh the connection bookkeeping.
void UnionRoomDrawing_HandleTransferComplete(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;

    ov58_021D2434(app, 3, 0);
    MI_CpuClearFast(app->drawingTiles, 30 * 15 * 32);

    if (CommSys_CurNetId() == 0) {
        app->connectedCount = CommSys_ConnectedCount();
        app->connectedBitmap = WirelessManager_GetConnectedBitmap();
        app->drawingState = 1;
    }
}

// Command 126: join handshake. A client asks to join (type 0) or sends a
// heartbeat (type 1); the server accepts the join once every player agrees on
// the connected count, then echoes the packet back. Clients react to the
// server's reply by advancing their state machine.
void UnionRoomDrawing_HandleConnectionAck(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    UnionRoomDrawingConnAck reply;
    UnionRoomDrawingConnAck *ack = data;

    if (senderNetId != 0) {
        if (CommSys_CurNetId() == 0) {
            reply = *ack;
            reply.netId = senderNetId;
            reply.connectedCount = app->connectedCount;

            switch (ack->type) {
            case 0:
                if ((app->connectedCount != CommSys_ConnectedCount()) || (app->connectedCount != ov58_021D2A4C()) || (app->connectedCount != MATH_CountPopulation(WirelessManager_GetConnectedBitmap()))) {
                    reply.accepted = 0;
                } else {
                    app->ackedNetIds |= 1 << senderNetId;
                    reply.accepted = 1;
                    CommManager_SetMaxNumConnections(CommSys_ConnectedCount());
                }
                break;
            case 1:
                break;
            }

            CommSys_SendDataServer(126, &reply, sizeof(UnionRoomDrawingConnAck));
        }
    } else {
        switch (ack->type) {
        case 0:
            if (ack->netId == CommSys_CurNetId()) {
                if (ack->accepted == 0) {
                    ov58_021D2434(app, 9, ack->netId);
                } else {
                    app->ackedCount = ack->connectedCount;
                    ov58_021D2434(app, 8, ack->netId);
                }
            }
            break;
        case 1:
            ov58_021D2434(app, 21, ack->netId);
            break;
        }
    }
}

// Command 125 is unused.
void UnionRoomDrawing_ReceiveUnusedCmd125(int senderNetId, int unused, void *data, void *app)
{
    return;
}

// Command 123: the server tells everyone that a player has started drawing.
// On the server, if the app is in the drawing state, begin streaming the
// canvas from the first chunk.
void UnionRoomDrawing_HandleBeginDrawing(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    u8 netId = *(u8 *)data;

    ov58_021D2434(app, 1, netId);

    if ((CommSys_CurNetId() == 0) && (app->appState == 1)) {
        app->sendChunkIndex = 0;
        UnionRoomDrawing_SendChunk(app, app->sendChunkIndex);
    }
}

// Command 121 is unused.
void UnionRoomDrawing_ReceiveUnusedCmd121(int senderNetId, int unused, void *data, void *app)
{
    return;
}

// Command 122 is unused.
void UnionRoomDrawing_ReceiveUnusedCmd122(int senderNetId, int unused, void *data, void *app)
{
    return;
}

// Command 127: the server has ended the drawing session; clients advance to
// the closing state.
void UnionRoomDrawing_HandleServerEndDrawing(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;

    if (CommSys_CurNetId() != 0) {
        ov58_021D2434(app, 15, 0);
    }
}

// Command 128: a client reports that it is ready to draw. The server starts
// the session (command 123) once it has seen the first ready client.
void UnionRoomDrawing_HandleClientReady(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    u8 netId;

    GF_ASSERT(CommSys_CurNetId() == 0);

    if (CommSys_CurNetId() == 0) {
        if (app->clientReadySent != 0) {
            netId = senderNetId;
            CommSys_SendDataServer(123, &netId, 1);
        } else {
            app->clientReadySent = 1;
        }
    }

    CommManager_SetErrorHandling(0, 1);
}

// Command 129: reset the drawing state to idle. No sender exists in the app,
// so this handler is effectively unused.
static void UnionRoomDrawing_HandleResetDrawingState(int senderNetId, int unused, void *data, void *appPtr)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    app->drawingState = 1;
}

// Copies one 1000-byte slice out of the drawing window's pixel buffer, stores
// its XOR checksum and index, and sends it to the clients as a huge transfer.
static void UnionRoomDrawing_SendChunk(UnionRoomDrawing *app, int chunkIndex)
{
    u8 *pixels = (u8 *)app->drawingWindow.pixels;

    MI_CpuCopyFast(&pixels[chunkIndex * 1000], app->sendChunk.data, 1000);

    {
        int i;
        u32 *words, checksum;

        words = (u32 *)app->sendChunk.data;

        for (checksum = 0, i = 0; i < 1000 / 4; i++) {
            checksum ^= words[i];
        }

        app->sendChunk.checksum = checksum;
    }

    app->sendChunk.index = chunkIndex;

    CommSys_SendDataHugeServer(118, &app->sendChunk, sizeof(UnionRoomDrawingChunk));
}

// Returns the receive buffer the huge-transfer layer fills for a given net ID.
static u8 *UnionRoomDrawing_GetChunkBuffer(int netId, void *appPtr, int requestedSize)
{
    UnionRoomDrawing *app = (UnionRoomDrawing *)appPtr;
    return (u8 *)&app->recvChunks[netId];
}

// Command 112 carries the same 4-byte handshake as command 126.
static int UnionRoomDrawing_GetHandshakePacketSize(void)
{
    return 4;
}

static int UnionRoomDrawing_GetConnAckPacketSize(void)
{
    return sizeof(UnionRoomDrawingConnAck);
}
