#include "mix_records_comm.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"
#include "struct_defs/struct_020961E8_t.h"
#include "struct_defs/struct_02096274.h"

#include "overlay059/ov59_021D0D80.h"
#include "overlay059/struct_ov59_021D30E0.h"

#include "comm_manager.h"
#include "communication_system.h"
#include "math_util.h"
#include "sound_playback.h"
#include "unk_02032798.h"
#include "unk_02099500.h"
#include "wireless_manager.h"

typedef struct MixRecordsComm MixRecordsComm;

static int MixRecordsComm_PacketSizeOf_Nothing(void);
static int MixRecordsComm_PacketSizeOf_NetId(void);
static u8 *MixRecordsComm_GetRecordBuffer(int param0, void *param1, int param2);
static int MixRecordsComm_ConnectionConfirmPacketSize(void);
static void MixRecordsComm_SendRecord(MixRecordsComm *param0, int param1);
void MixRecordsComm_HandleRecordData(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleConfirm(int param0, int param1, void *param2, void *param3);

// Command table for the Mix Records app (overlay 059). CommCmd_Init indexes
// this table by (command - 22), so the first 86 entries cover commands 22..107,
// which the app does not use. The initialized entries are:
//   108, 109 - unused (never sent)
//   110 - player-ready notification
//   111 - erase the message box (never sent)
//   112 - connection-confirm handshake
//   113 - cancel the record mix
//   114 - ready request (the server rebroadcasts it as command 110)
//   115 - a peer disconnected
//   116 - record data (3008 bytes, one slot per player)
//   117 - confirm flag
// The remaining entries are zero-initialized and unused.
static const CommCmdTable sMixRecordsCommCmdTable[] = {
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { MixRecordsComm_HandleUnused108, MixRecordsComm_PacketSizeOf_NetId },
    { MixRecordsComm_HandleUnused109, MixRecordsComm_PacketSizeOf_NetId },
    { MixRecordsComm_HandlePlayerReady, MixRecordsComm_PacketSizeOf_NetId },
    { MixRecordsComm_HandleEraseMessage, MixRecordsComm_PacketSizeOf_Nothing },
    { MixRecordsComm_HandleConnectionConfirm, MixRecordsComm_ConnectionConfirmPacketSize },
    { MixRecordsComm_HandleCancelTrade, MixRecordsComm_PacketSizeOf_Nothing },
    { MixRecordsComm_HandleReadyRequest, MixRecordsComm_PacketSizeOf_Nothing },
    { MixRecordsComm_HandleDisconnect, MixRecordsComm_PacketSizeOf_Nothing },
    { MixRecordsComm_HandleRecordData, sub_02099530, MixRecordsComm_GetRecordBuffer },
    { MixRecordsComm_HandleConfirm, MixRecordsComm_PacketSizeOf_NetId },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL },
    { sub_02099510, MixRecordsComm_PacketSizeOf_Nothing, NULL }
};

// Registers the Mix Records command table with the communication system.
// `param0` is the app's MixRecordsComm instance, passed back to every handler
// as its context.
void MixRecordsComm_Init(void *param0)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param0;
    int v1;
    int v2 = sizeof(sMixRecordsCommCmdTable) / sizeof(CommCmdTable);

    CommCmd_Init(sMixRecordsCommCmdTable, v2, param0);

    for (v1 = 0; v1 < 5; v1++) {
        (void)0;
    }
}

// Command 116: a player's record has arrived. The comm system has already
// copied the 3008-byte payload into the slot returned by
// MixRecordsComm_GetRecordBuffer, so only the received count is bumped here.
void MixRecordsComm_HandleRecordData(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;
    v0->receivedRecordCount++;
}

// Command 117: records the local player's confirm flag. Only the server
// (net ID 0) tracks it.
void MixRecordsComm_HandleConfirm(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;
    u8 *v1 = (u8 *)param2;

    if (param0 == 0) {
        if (v0->confirmFlag != *v1) {
            (void)0;
        }

        v0->confirmFlag = *v1;
    }
}

// Command 115: a peer dropped out. Sends the local record, tears down the
// exchange and reports the error.
void MixRecordsComm_HandleDisconnect(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;

    v0->receivedRecordCount = 0;
    v0->disconnected = 1;

    MixRecordsComm_SendRecord(v0, CommSys_CurNetId());

    ov59_021D1D40(v0);
    ov59_021D2204(v0, 25, param0);

    CommManager_SetErrorHandling(1, 1);
    Sound_PlayEffect(SEQ_SE_DP_F209_sseq);
}

// Command 111: closes the message box. The command is never sent, so this
// handler is unreachable.
void MixRecordsComm_HandleEraseMessage(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;
    ov59_021D2204(v0, 2, 0);
}

// Command 112: connection-confirm handshake. A client asks the server to
// confirm it; the server validates the player count and replies with an
// accept/reject flag. Type 1 announces a player leaving.
void MixRecordsComm_HandleConnectionConfirm(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;
    MixRecordsConnectionConfirm v1;
    MixRecordsConnectionConfirm *v2 = param2;

    if (param0 != 0) {
        if (CommSys_CurNetId() == 0) {
            v1 = *v2;
            v1.netId = param0;
            v1.playerCount = v0->expectedPlayerCount;

            switch (v2->type) {
            case 0:
                if ((v0->expectedPlayerCount != CommSys_ConnectedCount()) || (v0->expectedPlayerCount != ov59_021D2544()) || (v0->expectedPlayerCount != MATH_CountPopulation(WirelessManager_GetConnectedBitmap()))) {
                    v1.accepted = 0;
                } else {
                    v0->confirmedBitmap |= 1 << param0;
                    v1.accepted = 1;

                    CommManager_SetMaxNumConnections(CommSys_ConnectedCount());
                }
                break;
            case 1:
                break;
            }

            CommSys_SendData(112, &v1, sizeof(MixRecordsConnectionConfirm));
        }
    } else {
        switch (v2->type) {
        case 0:
            if (v2->netId == CommSys_CurNetId()) {
                if (v2->accepted == 0) {
                    ov59_021D2204(v0, 8, v2->netId);
                } else {
                    v0->serverPlayerCount = v2->playerCount;
                    ov59_021D2204(v0, 7, v2->netId);
                }
            }
            break;
        case 1:
            ov59_021D2204(v0, 19, v2->netId);
            break;
        }
    }
}

// Command 110: a player has become ready. The server also clears its
// pending-confirmation counter.
void MixRecordsComm_HandlePlayerReady(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;
    u8 v1 = *(u8 *)param2;

    ov59_021D22EC(v0, 1, v1);

    if (CommSys_CurNetId() == 0) {
        v0->unk_414 = 0;
    }
}

// Command 108: unused. The command is never sent.
void MixRecordsComm_HandleUnused108(int param0, int param1, void *param2, void *param3)
{
    return;
}

// Command 109: unused. The command is never sent.
void MixRecordsComm_HandleUnused109(int param0, int param1, void *param2, void *param3)
{
    return;
}

// Command 113: a client cancelled the record mix.
void MixRecordsComm_HandleCancelTrade(int param0, int param1, void *param2, void *param3)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param3;

    if (CommSys_CurNetId() != 0) {
        ov59_021D2204(v0, 13, 0);
    }
}

// Command 114: a client is ready. The server rebroadcasts it as command 110 so
// every client learns the player's net ID.
void MixRecordsComm_HandleReadyRequest(int param0, int param1, void *param2, void *param3)
{
    u8 v0;

    if (CommSys_CurNetId() == 0) {
        v0 = param0;
        CommSys_SendData(110, &v0, 1);
    }
}

// Computes a XOR checksum over the local player's 3000-byte record, appends a
// fresh LCRNG seed and sends it as command 116.
static void MixRecordsComm_SendRecord(MixRecordsComm *param0, int param1)
{
    {
        int v0;
        u32 *v1, v2;

        v1 = (u32 *)param0->localRecord.unk_00;

        for (v2 = 0, v0 = 0; v0 < 3000 / 4; v0++) {
            v2 ^= v1[v0];
        }

        param0->localRecord.unk_BB8 = v2;
    }

    param0->localRecord.unk_BBC = LCRNG_Next();
    CommSys_SendDataHuge(116, &param0->localRecord, sizeof(UnkStruct_ov59_021D30E0));
}

// Packet size for commands with no payload.
static int MixRecordsComm_PacketSizeOf_Nothing(void)
{
    return 0;
}

// Packet size for commands carrying a single net ID byte.
static int MixRecordsComm_PacketSizeOf_NetId(void)
{
    return 1;
}

// Packet size for the connection-confirm packet (command 112).
static int MixRecordsComm_ConnectionConfirmPacketSize(void)
{
    return sizeof(MixRecordsConnectionConfirm);
}

// Buffer accessor for command 116: returns the per-player slot the comm system
// writes an incoming record into.
static u8 *MixRecordsComm_GetRecordBuffer(int param0, void *param1, int param2)
{
    MixRecordsComm *v0 = (MixRecordsComm *)param1;
    return (u8 *)&v0->receivedRecords[param0];
}
