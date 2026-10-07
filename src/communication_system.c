#include "communication_system.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "constants/heap.h"

#include "struct_defs/comm_queue_man.h"
#include "struct_defs/struct_020322D8.h"
#include "struct_defs/struct_0203233C.h"

#include "nintendo_wfc/main.h"

#include "comm_manager.h"
#include "comm_queue.h"
#include "comm_ring.h"
#include "communication_information.h"
#include "heap.h"
#include "rtc.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "system.h"
#include "unk_0203266C.h"
#include "unk_02032798.h"
#include "comm_server_client.h"
#include "unk_020363E8.h"
#include "wireless_manager.h"

// The communication system is the transport layer shared by every multiplayer
// mode (local wireless, Wi-Fi battle/plaza and the union room). It owns the
// send/receive ring buffers, the per-frame input (held-keys) exchange, and the
// queue of commands that higher-level code sends to peers.
//
// It supports two topologies, selected by `transmissionType`:
//   - SERVER_CLIENT: one host (net ID 0) relays commands between clients.
//   - PARALLEL:      every console sends to and accepts data from every other.
// A running session can switch between the two through the
// SWITCH_TO_* transitional states; see CommSys_Transmission.

enum TransmissionType {
    TRANSMISSION_TYPE_SERVER_CLIENT,
    TRANSMISSION_TYPE_PARALLEL,
    TRANSMISSION_TYPE_SWITCH_TO_SERVER_CLIENT,
    TRANSMISSION_TYPE_SWITCH_TO_PARALLEL
};

// How CommSys_ApplyMovementModifiers rewrites the local player's held D-pad
// input before it is broadcast to the other consoles.
enum MovementState {
    MOVEMENT_STATE_NORMAL = 0,
    MOVEMENT_STATE_RANDOM,
    MOVEMENT_STATE_REVERSE,
};

// Unused duplicate of CommQueueEntry (the queue-entry type defined in
// struct_defs/struct_020322D8.h). Left in place to avoid touching the build.
typedef struct {
    u8 *unk_00;
    CommQueueEntry *unk_04;
    CommQueueEntry *unk_08;
    u16 unk_0C;
    u8 unk_0E;
    u8 unk_0F_0 : 1;
    u8 unk_0F_1 : 1;
    u8 : 6;
} UnkStruct_020322D8_t;

// In-progress reassembly state for a single incoming command. A command whose
// payload spans several packets (or several frames) is accumulated here until
// `receivedSize` reaches `packetSize`, at which point it is dispatched.
typedef struct {
    int receivedSize; // bytes of the payload received so far
    u8 *dataBuffer;   // buffer used when the command needs reassembly
    u16 packetSize;   // expected payload size; 0xFFFF while still unknown
    u8 packetCommand; // command byte; 0xEE when no command is in progress
} CommRecvPackage;

typedef struct {
    u8 sendBuffer[2][64];
    u8 sendBufferServer[2][192];
    u8 sendBufferCommRing[COMM_RING_BUFFER_SIZE];
    u8 sendBufferCommRingServer[384];
    u8 *relayRingBuffer;    // backing storage for relayRing
    u8 *recvBufferRingServer;
    u8 *recvBufferRing;
    u8 *tempBuffer;
    CommRing sendRing;
    CommRing recvRing;
    CommRing relayRing[8];  // per-player relay rings (parallel mode)
    CommRing sendRingServer;
    CommRing sendRingClient[8];
    SysTask *vBlankTask;
    CommQueueMan commQueueManSend;
    CommQueueMan commQueueManSendServer;
    CommRecvPackage commRecvServer[8];
    CommRecvPackage commRecvClient;
    MATHRandContext32 rand;
    u16 receivedKeys[8];    // D-pad bits decoded from each player's input
    u8 recvSpeed[8];
    u16 sendHeldKeys;
    u8 unk_656;             // never written; blocks input send when non-zero
    u8 sendSpeed;
    u8 playerMovementState;
    s8 randomPadKeyTimer;
    u16 randomPadKey;
    BOOL recvLimitEnabled;  // wait for every peer before building the next packet
    volatile int sendCount; // outstanding Wi-Fi sends (throttles to 4)
    volatile int sendCountPerPlayer[8];
    int allocSize;
    int maxPacketSize;
    u16 connectedBitmap;    // bit per connected net ID, relayed by the server
    u8 sendSequence;        // rolling sequence nibble in the input packet header
    u8 unk_68F[8];          // per-player receive counter; written, never read
    u8 waitingForPacketStart[8];
    u8 battlePositions[4];  // net ID for each battle position; 0xFF = unused
    u8 transmissionState;
    u8 pendingTransitionType;
    u8 transmissionType;
    u8 unk_6A6;             // set to 38 once, never read
    u8 sendBufferIndex;     // which of the two sendBuffer slots is active
    u8 serverSendBufferIndex;
    u8 inputSendTimer;      // frames the current input packet stays "fresh"
    u8 waitingForPacketStartClient;
    u8 unk_6AB;             // set to 1 once, never read
    u8 partialSend;         // a command was only partially written last time
    u8 serverPartialSend;
    u8 isAlone;             // no other consoles are present
    u8 wifiConnected;
    u8 unk_6B0;             // cleared once, never read
    u8 commError;           // set when an invalid/oversized command arrives
    u8 shuttingDown;
    u8 unk_6B3;             // read to pause receiving, but never set
    u8 sendInterval;        // input packets are sent every Nth frame (0 = off)
    u8 frameCounter;
} CommunicationSystem;

static void CommSys_ResetState(void);
static void CommSys_VBlankTask(SysTask *param0, void *param1);
static void CommSys_TransmitInput(void);
static void CommSys_UpdateServerClient(void);
static void CommSys_TransmitInputServer(void);
static void CommSys_SendCallbackClient(BOOL param0);
static void CommSys_SendCallbackServer(BOOL param0);
static void CommSys_RecvInputClient(u16 param0, u16 *param1, u16 param2);
static void CommSys_RecvInputServer(u16 param0, u16 *param1, u16 param2);
static BOOL CommSys_CheckRecvLimit(void);
static void CommSys_ApplyMovementModifiers(void);
static void CommSys_TransmitInputWireless(void);
static void CommSys_RecvData(void);
static void CommSys_RecvDataServer(void);
static BOOL CommSys_BuildInputPacket(u8 *param0);
static void CommSys_BuildServerPacket(u8 *param0);
static BOOL CommSys_DecodeInput(u8 *param0, int param1);
static BOOL CommSys_EncodeInput(u8 *param0);
static void CommSys_Transmission(void);
static BOOL CommSys_ShouldThrottleSend(void);

static u32 sCommSystemRawAlloc = 0; // unaligned allocation, kept for Heap_Free
static CommunicationSystem *sCommunicationSystem = NULL;
static volatile u8 sVBlankWorkPending = 0; // main loop asks the VBlank task to run
static volatile u8 sSendStateServer = 4;   // server send state machine; 4 = idle
static volatile u8 sSendStateClient = 4;   // client send state machine; 4 = idle
static u8 sServerBufferPrepared = 0;       // server packet already built this cycle

// Allocates (on first call) and prepares the communication system. When
// `shouldAlloc` is FALSE the existing instance is reused, which happens when a
// session is re-initialized without tearing down its buffers.
static BOOL CommSys_Init(BOOL shouldAlloc, int maxPacketSize)
{
    int i;
    BOOL reinit = FALSE;

    sVBlankWorkPending = FALSE;

    if (shouldAlloc) {
        int maxMachines = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

        if (sCommunicationSystem != NULL) {
            return TRUE;
        }

        CommTool_Init(HEAP_ID_COMMUNICATION);

        sCommSystemRawAlloc = (u32)Heap_Alloc(HEAP_ID_COMMUNICATION, sizeof(CommunicationSystem) + 32);
        // Round the allocation up to a 32-byte boundary.
        sCommunicationSystem = (CommunicationSystem *)(32 - (sCommSystemRawAlloc % 32) + sCommSystemRawAlloc);

        MI_CpuClear8(sCommunicationSystem, sizeof(CommunicationSystem));

        if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
            sCommunicationSystem->maxPacketSize = maxPacketSize * 2 + 64;
        } else {
            sCommunicationSystem->maxPacketSize = maxPacketSize + 64;
        }

        sCommunicationSystem->allocSize = sCommunicationSystem->maxPacketSize * maxMachines;
        sCommunicationSystem->transmissionType = TRANSMISSION_TYPE_SERVER_CLIENT;
        sCommunicationSystem->unk_6A6 = 38;
        // Per-player receive rings are carved out of these two allocations.
        sCommunicationSystem->recvBufferRing = Heap_Alloc(HEAP_ID_COMMUNICATION, sCommunicationSystem->maxPacketSize * 2);
        sCommunicationSystem->tempBuffer = Heap_Alloc(HEAP_ID_COMMUNICATION, sCommunicationSystem->maxPacketSize);
        sCommunicationSystem->recvBufferRingServer = Heap_Alloc(HEAP_ID_COMMUNICATION, sCommunicationSystem->allocSize);
        sCommunicationSystem->relayRingBuffer = Heap_Alloc(HEAP_ID_COMMUNICATION, sCommunicationSystem->allocSize);

        if (CommManager_GetCommType() == 10) {
            CommQueueMan_Init(&sCommunicationSystem->commQueueManSend, 100, &sCommunicationSystem->sendRing);
            CommQueueMan_Init(&sCommunicationSystem->commQueueManSendServer, 800, &sCommunicationSystem->sendRingServer);
        } else {
            CommQueueMan_Init(&sCommunicationSystem->commQueueManSend, 20, &sCommunicationSystem->sendRing);
            CommQueueMan_Init(&sCommunicationSystem->commQueueManSendServer, 280, &sCommunicationSystem->sendRingServer);
        }
    } else {
        reinit = TRUE;
        GF_ASSERT(sCommunicationSystem);
    }

    sCommunicationSystem->connectedBitmap = 0;

    for (i = 0; i < 4; i++) {
        sCommunicationSystem->battlePositions[i] = 0xff;
    }

    if (!reinit) {
        CommSys_ResetState();
    }

    CommSys_Seed(&sCommunicationSystem->rand);

    if (!reinit) {
        sCommunicationSystem->vBlankTask = SysTask_ExecuteOnVBlank(CommSys_VBlankTask, NULL, 0);
    }

    sCommunicationSystem->wifiConnected = FALSE;
    return TRUE;
}

// Resets every buffer, ring and reassembly slot to its initial state. Called
// when a session starts, resets, or changes transmission topology.
static void CommSys_ClearData(void)
{
    int netId, size;
    int maxMachines = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    sCommunicationSystem->playerMovementState = MOVEMENT_STATE_NORMAL;
    sCommunicationSystem->randomPadKeyTimer = 0;

    MI_CpuClear8(sCommunicationSystem->recvBufferRingServer, sCommunicationSystem->allocSize);
    MI_CpuClear8(sCommunicationSystem->sendRingClient, sizeof(CommRing) * (7 + 1));

    size = sCommunicationSystem->allocSize / maxMachines;

    for (netId = 0; netId < maxMachines; netId++) {
        CommRing_Init(&sCommunicationSystem->sendRingClient[netId], &sCommunicationSystem->recvBufferRingServer[netId * size], size);
    }

    MI_CpuClear8(sCommunicationSystem->relayRingBuffer, sCommunicationSystem->allocSize);
    MI_CpuClear8(sCommunicationSystem->relayRing, sizeof(CommRing) * (7 + 1));

    for (netId = 0; netId < maxMachines; netId++) {
        CommRing_Init(&sCommunicationSystem->relayRing[netId], &sCommunicationSystem->relayRingBuffer[netId * size], size);
    }

    MI_CpuClear8(sCommunicationSystem->sendBufferCommRingServer, (192 * 2));
    CommRing_Init(&sCommunicationSystem->sendRingServer, sCommunicationSystem->sendBufferCommRingServer, (192 * 2));

    MI_CpuFill8(sCommunicationSystem->sendBufferServer[0], 0xee, (192 * 2));
    MI_CpuFill8(sCommunicationSystem->sendBufferServer[1], 0xee, (192 * 2));
    MI_CpuClear8(sCommunicationSystem->sendBufferCommRing, (12 * 22));

    CommRing_Init(&sCommunicationSystem->sendRing, sCommunicationSystem->sendBufferCommRing, (12 * 22));

    MI_CpuFill8(sCommunicationSystem->sendBuffer[0], 0xee, 38);
    MI_CpuFill8(sCommunicationSystem->sendBuffer[1], 0xee, 38);

    sCommunicationSystem->sendBuffer[0][0] = 0xff;
    sCommunicationSystem->sendBuffer[1][0] = 0xff;

    MI_CpuClear8(sCommunicationSystem->recvBufferRing, sCommunicationSystem->maxPacketSize * 2);
    CommRing_Init(&sCommunicationSystem->recvRing, sCommunicationSystem->recvBufferRing, sCommunicationSystem->maxPacketSize * 2);

    sCommunicationSystem->partialSend = 0;
    sCommunicationSystem->serverPartialSend = 0;

    for (netId = 0; netId < (7 + 1); netId++) {
        sCommunicationSystem->unk_68F[netId] = 0;
        sCommunicationSystem->waitingForPacketStart[netId] = 1;
        sCommunicationSystem->receivedKeys[netId] = 0;
        sCommunicationSystem->commRecvServer[netId].packetCommand = 0xee;
        sCommunicationSystem->commRecvServer[netId].packetSize = 0xffff;
        sCommunicationSystem->commRecvServer[netId].dataBuffer = NULL;
        sCommunicationSystem->commRecvServer[netId].receivedSize = 0;
        sCommunicationSystem->sendCountPerPlayer[netId] = 0;
    }

    sCommunicationSystem->sendCount = 0;
    sCommunicationSystem->commRecvClient.packetCommand = 0xee;
    sCommunicationSystem->commRecvClient.packetSize = 0xffff;
    sCommunicationSystem->commRecvClient.dataBuffer = NULL;
    sCommunicationSystem->commRecvClient.receivedSize = 0;
    sCommunicationSystem->waitingForPacketStartClient = 1;
    sCommunicationSystem->unk_6AB = 1;
    sServerBufferPrepared = 0;

    CommQueueMan_Reset(&sCommunicationSystem->commQueueManSend);
    CommQueueMan_Reset(&sCommunicationSystem->commQueueManSendServer);

    sCommunicationSystem->unk_6B0 = 0;
}

// Full reset of the running session: rewinds both send buffers and clears all
// data, then marks both send state machines idle.
static void CommSys_ResetState(void)
{
    sCommunicationSystem->sendBufferIndex = 0;
    sCommunicationSystem->serverSendBufferIndex = 0;
    sCommunicationSystem->recvLimitEnabled = 1;

    CommSys_ClearData();

    sSendStateServer = 4;
    sSendStateClient = 4;
}

// Reinitializes the shared buffers after a topology switch; the pending
// transmission was already accounted for before this is called.
static void CommSys_ClearDataOnTransition(void)
{
    CommSys_ClearData();
}

// Forgets all reassembly state for one player's incoming command stream. Used
// when a player joins so their first packet is treated as a fresh start.
static void CommSys_ClearServerRecvData(int netId)
{
    sCommunicationSystem->unk_68F[netId] = 0;
    sCommunicationSystem->waitingForPacketStart[netId] = 1;
    sCommunicationSystem->sendCountPerPlayer[netId] = 0;

    int v0 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;
    int v1 = sCommunicationSystem->allocSize / v0;

    CommRing_Init(&sCommunicationSystem->relayRing[netId], &sCommunicationSystem->relayRingBuffer[netId * v1], v1);
    CommRing_Init(&sCommunicationSystem->sendRingClient[netId], &sCommunicationSystem->recvBufferRingServer[netId * v1], v1);

    sCommunicationSystem->commRecvServer[netId].packetCommand = 0xee;
    sCommunicationSystem->commRecvServer[netId].packetSize = 0xffff;
    sCommunicationSystem->commRecvServer[netId].dataBuffer = NULL;
    sCommunicationSystem->commRecvServer[netId].receivedSize = 0;
}

// Drops the reassembly state of clients that have disconnected, so they start
// clean if they reconnect.
static void CommSys_ClearDisconnectedClientData(void)
{
    int netId;

    for (netId = 1; netId < (7 + 1); netId++) {
        if (!CommSys_IsPlayerConnected(netId) && !sCommunicationSystem->waitingForPacketStart[netId] && !CommSys_IsAlone()) {
            CommSys_ClearServerRecvData(netId);
        }
    }
}

// WirelessManager connect callback: a client with the given net ID connected.
static void CommSys_OnClientConnect(int param0)
{
    CommSys_ClearServerRecvData(param0);
}

BOOL CommSys_InitServer(BOOL param0, BOOL param1, int param2, BOOL param3)
{
    BOOL ret = TRUE;

    if (!CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        ret = CommServerClient_InitServer(param0, param1, param3);
        WirelessManager_SetConnectCallback(CommSys_OnClientConnect);
    }

    CommSys_Init(param0, param2);
    return ret;
}

BOOL CommSys_InitClient(BOOL param0, BOOL param1, int param2)
{
    BOOL v0 = TRUE;

    if (!CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        v0 = CommServerClient_InitClient(param0, param1);
    }

    CommSys_Init(param0, param2);
    sSendStateClient = 4;

    return v0;
}

// Applies a topology switch once the local send state machine is idle, then
// advances the switch handshake. The SWITCH_TO_* states hold the old topology
// until both sides have acknowledged; CommSys_TransmissionType reports the old
// value during that window.
static void CommSys_UpdateTransitionType(void)
{
    BOOL changed = FALSE;

    if (CommSys_CurNetId() == 0) {
        if (sSendStateServer != 4) {
            return;
        }
    } else if (sSendStateClient != 4) {
        return;
    }

    if (sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_SWITCH_TO_SERVER_CLIENT) {
        sCommunicationSystem->transmissionType = TRANSMISSION_TYPE_SERVER_CLIENT;
        changed = TRUE;
    }

    if (sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_SWITCH_TO_PARALLEL) {
        sCommunicationSystem->transmissionType = TRANSMISSION_TYPE_PARALLEL;
        changed = TRUE;
    }

    if (changed) {
        CommSys_ClearDataOnTransition();
    }

    CommSys_Transmission();
}

// Requests a topology change. The actual change is deferred (SWITCH_TO_*)
// until the peer handshake completes.
static void CommSys_SwitchTransitionType(int type)
{
    if ((sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_SERVER_CLIENT) && (type == TRANSMISSION_TYPE_PARALLEL)) {
        sCommunicationSystem->transmissionType = TRANSMISSION_TYPE_SWITCH_TO_PARALLEL;
        return;
    }

    if ((sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_PARALLEL) && (type == TRANSMISSION_TYPE_SERVER_CLIENT)) {
        sCommunicationSystem->transmissionType = TRANSMISSION_TYPE_SWITCH_TO_SERVER_CLIENT;
        return;
    }
}

void CommSys_SwitchTransitionTypeToParallel(void)
{
    CommSys_SwitchTransitionType(TRANSMISSION_TYPE_PARALLEL);
}

void CommSys_SwitchTransitionTypeToServerClient(void)
{
    CommSys_SwitchTransitionType(TRANSMISSION_TYPE_SERVER_CLIENT);
}

// The effective topology: while switching, the previous topology still applies.
static int CommSys_TransmissionType(void)
{
    if (sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_SWITCH_TO_SERVER_CLIENT) {
        return TRANSMISSION_TYPE_PARALLEL;
    }

    if (sCommunicationSystem->transmissionType == TRANSMISSION_TYPE_SWITCH_TO_PARALLEL) {
        return TRANSMISSION_TYPE_SERVER_CLIENT;
    }

    return sCommunicationSystem->transmissionType;
}

BOOL CommSys_TransitionTypeIsParallel(void)
{
    if (TRANSMISSION_TYPE_PARALLEL == CommSys_TransmissionType()) {
        return TRUE;
    }

    return FALSE;
}

// Shuts down the underlying transport and, once it has finished, frees every
// buffer owned by the communication system.
void CommSys_Delete(void)
{
    BOOL v0 = FALSE;

    if (sCommunicationSystem) {
        if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
            NintendoWFC_Stop();
            v0 = 1;
        } else {
            if (CommServerClient_Shutdown()) {
                v0 = 1;
            }
        }
    }

    if (v0) {
        CommTool_Delete();
        CommInfo_Delete();

        sVBlankWorkPending = 0;
        SysTask_Done(sCommunicationSystem->vBlankTask);
        sCommunicationSystem->vBlankTask = NULL;

        Heap_Free(sCommunicationSystem->recvBufferRing);
        Heap_Free(sCommunicationSystem->tempBuffer);
        Heap_Free(sCommunicationSystem->recvBufferRingServer);
        Heap_Free(sCommunicationSystem->relayRingBuffer);
        CommQueueMan_Delete(&sCommunicationSystem->commQueueManSendServer);
        CommQueueMan_Delete(&sCommunicationSystem->commQueueManSend);
        Heap_Free((void *)sCommSystemRawAlloc);

        sCommunicationSystem = NULL;
        sCommSystemRawAlloc = 0;
    }
}

BOOL CommSys_ConnectToServer(u16 param0)
{
    return CommServerClient_ConnectToServer(param0);
}

// Runs on VBlank after CommSys_Update has prepared the frame: flushes the
// local-wireless input packet and, on the host, the server broadcast.
static void CommSys_VBlankTask(SysTask *param0, void *param1)
{
    if (sVBlankWorkPending) {
        CommSys_TransmitInputWireless();

        if (((CommSys_CurNetId() == 0) && (CommSys_IsPlayerConnected(0))) || CommSys_IsAlone()) {
            CommSys_UpdateServerClient();
        }

        sVBlankWorkPending = 0;
    }
}

// Deletes the system once the transport reports the session is over. The host
// waits for any half-finished client connection first.
static void CommSys_CheckShutdown(void)
{
    if (!CommServerClient_IsFinished()) {
        return;
    }

    if (CommSys_CurNetId() == 0) {
        if (CommServerClient_IsClientConnecting()) {
            return;
        }

        CommSys_Delete();
    } else {
        CommSys_Delete();
    }
}

// Per-frame entry point: refreshes the state machines, folds the local input
// into the outgoing packet and services both the server and client receive
// paths. The actual wireless sends happen in the VBlank task.
BOOL CommSys_Update(void)
{
    CommManager_Update();

    if (sCommunicationSystem != NULL) {
        if (!sCommunicationSystem->shuttingDown) {
            sCommunicationSystem->frameCounter++;
            sVBlankWorkPending = 0;
            CommSys_UpdateTransitionType();
            // Accumulate the newly held keys; bit 15 means "send movement data".
            sCommunicationSystem->sendHeldKeys |= (gSystem.heldKeys & 0x7fff);
            CommSys_ApplyMovementModifiers();
            CommSys_TransmitInput();
            // Keep only the enable bit so keys do not leak into the next frame.
            sCommunicationSystem->sendHeldKeys &= 0x8000;

            if (CommSys_TransmissionType() == TRANSMISSION_TYPE_SERVER_CLIENT) {
                CommSys_RecvData();
            }

            if ((CommSys_CurNetId() == 0 && CommSys_IsPlayerConnected(0) || CommSys_IsAlone()) && !sub_0203272C(CommManager_GetCommType())) {
                CommSys_TransmitInputServer();
            }

            if ((CommSys_CurNetId() == 0) || (CommSys_TransmissionType() == TRANSMISSION_TYPE_PARALLEL) || CommSys_IsAlone()) {
                CommSys_RecvDataServer();
            }

            sVBlankWorkPending = 1;
        }

        CommServerClient_Update(sCommunicationSystem->connectedBitmap);

        if (CommSys_CurNetId() == 0) {
            CommSys_ClearDisconnectedClientData();
        }

        CommSys_CheckShutdown();
    } else {
        CommServerClient_Update(0);
    }

    CommManager_DisplayError(0);
    sub_0203650C();

    return TRUE;
}

void CommSys_Reset(void)
{
    BOOL v0 = sVBlankWorkPending;

    sVBlankWorkPending = 0;

    if (sCommunicationSystem) {
        CommSys_ResetState();
    }

    sVBlankWorkPending = v0;
}

void CommSys_ResetDS(void)
{
    BOOL v0 = sVBlankWorkPending;

    sVBlankWorkPending = 0;

    if (sCommunicationSystem) {
        sCommunicationSystem->transmissionType = 1;
        CommSys_ResetState();
    }

    sVBlankWorkPending = v0;
}

void CommSys_ResetBattleClient(void)
{
    BOOL v0 = sVBlankWorkPending;

    sVBlankWorkPending = 0;

    if (sCommunicationSystem) {
        CommSys_ResetState();
        CommServerClient_ClearScanResults();
    }

    sVBlankWorkPending = v0;
}

// Builds and sends the 38-byte input packet for this frame. Wi-Fi battle
// types use per-console sends; other Wi-Fi modes use the server broadcast;
// local wireless delegates to CommSys_TransmitInputWireless.
static void CommSys_TransmitInput(void)
{
    if (sub_0203272C(CommManager_GetCommType())) {
        if (sCommunicationSystem->wifiConnected) {
            if (sCommunicationSystem->recvLimitEnabled) {
                if (!CommSys_CheckRecvLimit()) {
                    return;
                }

                if (sSendStateClient == 4) {
                    CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[0]);
                    sSendStateClient = 2;
                }
            } else {
                if (sSendStateClient == 4) {
                    if (!CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[0])) {
                        return;
                    }

                    sSendStateClient = 2;
                }
            }

            if (CommSys_ShouldThrottleSend()) {
                return;
            }

            if (NintendoWFC_SendData(sCommunicationSystem->sendBuffer[0], 38)) {
                int i;
                int v1 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

                for (i = 0; i < v1; i++) {
                    if (CommSys_IsPlayerConnected(i)) {
                        sCommunicationSystem->sendCountPerPlayer[i]++;
                    }
                }

                sSendStateClient = 4;
            }
        }
    } else if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        if (sCommunicationSystem->wifiConnected) {
            if (sCommunicationSystem->recvLimitEnabled) {
                if (sCommunicationSystem->sendCount > 3) {
                    return;
                }

                if (sSendStateClient == 4) {
                    CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[0]);
                    sSendStateClient = 2;
                }
            } else {
                if (sSendStateClient == 4) {
                    if (!CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[0])) {
                        return;
                    }

                    sSendStateClient = 2;
                }
            }

            if (CommSys_ShouldThrottleSend()) {
                return;
            }

            if (NintendoWFC_SendData_Server(sCommunicationSystem->sendBuffer[0], 38)) {
                sSendStateClient = 4;
                sCommunicationSystem->sendCount++;
            }
        }
    } else if (((WirelessManager_GetState() == 4) && (CommSys_IsPlayerConnected(CommSys_CurNetId()))) || CommSys_IsAlone()) {
        while (TRUE) {
            if (sSendStateClient != 4) {
                break;
            }

            if (sCommunicationSystem->sendCount > 3) {
                break;
            }

            CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[sCommunicationSystem->sendBufferIndex]);
            CommSys_BuildInputPacket(sCommunicationSystem->sendBuffer[1 - sCommunicationSystem->sendBufferIndex]);
            sSendStateClient = 0;
            break;
        }

        CommSys_TransmitInputWireless();
    }
}

// Fills one server broadcast buffer by copying a fixed-size block from each
// connected player's relay ring. A slot is prefixed 0xE when it carries data
// and 0xFF when the player is absent. Returns FALSE if every slot is empty.
static BOOL CommSys_FillServerSendBuffer(int param0)
{
    int v0;
    int v1;
    int i, v3, v4 = 0;

    v0 = CommSys_PlayerBlockSize(CommManager_GetCommType());
    v1 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    for (i = 0; i < v1; i++) {
        CommRing_UpdateEndPos(&sCommunicationSystem->relayRing[i]);

        if (CommSys_IsPlayerConnected(i)) {
            sCommunicationSystem->sendBufferServer[param0][i * v0] = 0xe;
        } else {
            sCommunicationSystem->sendBufferServer[param0][i * v0] = 0xff;
            v4++;
            continue;
        }

        v3 = CommRing_Read(&sCommunicationSystem->relayRing[i], &sCommunicationSystem->sendBufferServer[param0][i * v0], v0);

        if (sCommunicationSystem->sendBufferServer[param0][i * v0] == 0xe) {
            v4++;
        }
    }

    if (v4 == v1) {
        return FALSE;
    }

    return TRUE;
}

// Host-side, parallel-mode broadcast state machine. It alternates between the
// two 192-byte server buffers, sending one over wireless while the other is
// being refilled and locally consumed.
static void CommSys_UpdateServerClient(void)
{
    int i, v2, v3;
    int v1 = 0;

    if (!sCommunicationSystem) {
        return;
    }

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        return;
    }

    v2 = CommSys_PlayerBlockSize(CommManager_GetCommType());
    v3 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    if ((sSendStateServer == 2) || (sSendStateServer == 0)) {
        sSendStateServer++;

        if (CommSys_TransmissionType() == 1 && sServerBufferPrepared == 0) {
            CommSys_FillServerSendBuffer(sCommunicationSystem->serverSendBufferIndex);
            sServerBufferPrepared = 1;
        }

        if (WirelessManager_GetState() == 4
            && !CommSys_IsAlone()
            && !WirelessManager_SendMessage(sCommunicationSystem->sendBufferServer[sCommunicationSystem->serverSendBufferIndex], 192, 14, CommSys_SendCallbackServer)) {
            sSendStateServer--;
        }

        if ((sSendStateServer == 1) || (sSendStateServer == 3)) {
            sServerBufferPrepared = 0;

            for (i = 0; i < v3; i++) {
                if (CommSys_IsPlayerConnected(i)) {
                    sCommunicationSystem->sendCountPerPlayer[i]++;
                } else if (CommSys_IsAlone() && (i == 0)) {
                    sCommunicationSystem->sendCountPerPlayer[i]++;
                }
            }

            CommSys_RecvInputClient(0, (u16 *)sCommunicationSystem->sendBufferServer[sCommunicationSystem->serverSendBufferIndex], 192);
            sCommunicationSystem->serverSendBufferIndex = 1 - sCommunicationSystem->serverSendBufferIndex;
        }

        if ((WirelessManager_GetState() != 4) || CommSys_IsAlone()) {
            sSendStateServer++;
        }
    }
}

// TRUE while every connected player has at most 3 unacknowledged sends. Used
// to avoid running too far ahead of the slowest peer in recv-limit mode.
static BOOL CommSys_CheckRecvLimit(void)
{
    int i;
    int v1 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    for (i = 1; i < v1; i++) {
        if (CommSys_IsPlayerConnected(i) && sCommunicationSystem->sendCountPerPlayer[i] > 3) {
            return FALSE;
        }
    }

    return TRUE;
}

// Host-side input send. In Wi-Fi modes the host sends the 192-byte server
// packet; in local wireless it builds the broadcast and hands off to
// CommSys_UpdateServerClient.
static void CommSys_TransmitInputServer(void)
{
    int i;
    int v1 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        if (CommSys_IsPlayerConnected(0)) {
            if (sCommunicationSystem->recvLimitEnabled) {
                if (!CommSys_CheckRecvLimit()) {
                    return;
                }

                if (sSendStateServer == 4) {
                    if (CommSys_TransmissionType() == 1) {
                        CommSys_FillServerSendBuffer(0);
                    }

                    sSendStateServer = 2;
                }
            } else {
                if (sSendStateServer == 4 && CommSys_TransmissionType() == 1 && !CommSys_FillServerSendBuffer(0)) {
                    return;
                }

                sSendStateServer = 2;
            }

            if (NintendoWFC_SendData_Client(sCommunicationSystem->sendBufferServer[0], 192)) {
                sSendStateServer = 4;

                for (i = 0; i < v1; i++) {
                    if (CommSys_IsPlayerConnected(i)) {
                        sCommunicationSystem->sendCountPerPlayer[i]++;
                    }
                }
            } else {
                (void)0;
            }
        }
    } else if ((WirelessManager_GetState() == 4) || (CommSys_IsAlone())) {
        if (sSendStateServer != 4) {
            return;
        }

        if (!CommSys_CheckRecvLimit()) {
            return;
        }

        if (CommSys_TransmissionType() == 0) {
            CommSys_BuildServerPacket(sCommunicationSystem->sendBufferServer[sCommunicationSystem->serverSendBufferIndex]);
            CommSys_BuildServerPacket(sCommunicationSystem->sendBufferServer[1 - sCommunicationSystem->serverSendBufferIndex]);
        }

        sSendStateServer = 0;

        CommSys_UpdateServerClient();
    }
}

// NintendoWFC client data-transfer callback (also the local-wireless receive
// function for clients).
void CommSys_ClientRecvCallback(u16 param0, u16 *param1, u16 param2)
{
    CommSys_RecvInputClient(param0, param1, param2);
}

// Handles one packet received by a client. In parallel mode the packet is a
// concatenation of per-player blocks (0xFF = absent, 0xE = empty) that are
// demultiplexed into each player's ring; in server/client mode it is the
// server broadcast, whose header carries the connected bitmap and a length.
static void CommSys_RecvInputClient(u16 param0, u16 *param1, u16 param2)
{
    u8 *v0 = (u8 *)param1;
    int i;
    int v2 = param2;

    sCommunicationSystem->sendCount--;

    if (v0 == NULL) {
        return;
    }

    if (v0[0] == 0xb) {
        if (CommSys_TransmissionType() == 1) {
            return;
        }

        v0++;
        v2--;
    } else if (CommSys_TransmissionType() == 0) {
        return;
    }

    if ((sCommunicationSystem->waitingForPacketStartClient) && (v0[0] & 0x1)) {
        return;
    }

    sCommunicationSystem->waitingForPacketStartClient = 0;

    if (CommSys_TransmissionType() == 1) {
        int v3 = CommSys_PlayerBlockSize(CommManager_GetCommType());
        int v4 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

        for (i = 0; i < v4; i++) {
            if (v0[0] == 0xff) {
                sCommunicationSystem->connectedBitmap = sCommunicationSystem->connectedBitmap & ~(1 << i);
            } else {
                sCommunicationSystem->connectedBitmap = sCommunicationSystem->connectedBitmap | (1 << i);
            }

            if (v0[0] == 0xff) {
                v0 += v3;
            } else if (v0[0] == 0xe) {
                v0 += v3;
            } else if ((sCommunicationSystem->waitingForPacketStart[i]) && (v0[0] & 0x1)) {
                v0 += v3;
            } else {
                v0++;
                CommRring_Write(&sCommunicationSystem->sendRingClient[i], v0, v3 - 1, 1360 + i);
                v0 += (v3 - 1);
                sCommunicationSystem->waitingForPacketStart[i] = 0;
            }
        }
    } else {
        v0++;
        sCommunicationSystem->connectedBitmap = v0[0];
        sCommunicationSystem->connectedBitmap *= 256;

        v0++;
        sCommunicationSystem->connectedBitmap += v0[0];

        v0++;
        v2 -= 3;
        v2 = v0[0];

        v0++;
        CommRring_Write(&sCommunicationSystem->recvRing, v0, v2, 1380);
    }
}

// NintendoWFC server data-transfer callback (also the local-wireless receive
// function for servers): one packet from a single client.
void CommSys_ServerRecvCallback(u16 param0, u16 *buffer, u16 param2)
{
    CommSys_RecvInputServer(param0, buffer, param2);
}

// Handles one packet received from client `param0`. In parallel mode the data
// is queued for rebroadcast; in server/client mode it is decoded as input and
// queued as a command stream.
static void CommSys_RecvInputServer(u16 param0, u16 *_buffer, u16 param2)
{
    u8 *buffer = (u8 *)_buffer;
    int v1;

    sCommunicationSystem->sendCountPerPlayer[param0]--;

    if (buffer == NULL) {
        return;
    }

    if ((sCommunicationSystem->waitingForPacketStart[param0]) && (buffer[0] & 0x1)) {
        v1 = 0;
        return;
    }

    sCommunicationSystem->waitingForPacketStart[param0] = 0;

    if (CommSys_TransmissionType() == 1) {
        int v2 = CommSys_PlayerBlockSize(CommManager_GetCommType());
        int v3 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

        if (!(buffer[0] & 0x2)) {
            CommRring_Write(&sCommunicationSystem->relayRing[param0], buffer, v2, 1449);
        }

        sCommunicationSystem->unk_68F[param0]++;
    } else {
        CommSys_DecodeInput(buffer, param0);

        if (buffer[0] & 0x2) {
            return;
        }

        buffer++;
        CommRring_Write(&sCommunicationSystem->sendRingClient[param0], buffer, (12 - 1), 1458);
    }
}

// Receive function used by the Wi-Fi plaza/poffin/club games, which run in
// parallel mode: refresh the connected bitmap and queue the player's block.
void CommSys_RecvInputWifiGroup(u16 param0, u16 *param1, u16 param2)
{
    u8 *buffer = (u8 *)param1;
    int v1;

    sCommunicationSystem->sendCountPerPlayer[param0]--;

    if (buffer == NULL) {
        return;
    }

    if ((sCommunicationSystem->waitingForPacketStart[param0]) && (buffer[0] & 0x1)) {
        v1 = 0;
        return;
    }

    sCommunicationSystem->waitingForPacketStart[param0] = 0;

    if (CommSys_TransmissionType() == 1) {
        int v2 = CommSys_PlayerBlockSize(CommManager_GetCommType());
        int v3 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

        if (buffer[0] == 0xff) {
            sCommunicationSystem->connectedBitmap = sCommunicationSystem->connectedBitmap & ~(1 << param0);
        } else {
            sCommunicationSystem->connectedBitmap = sCommunicationSystem->connectedBitmap | (1 << param0);
        }

        if (buffer[0] == 0xff) {
            (void)0;
        } else if (buffer[0] == 0x2) {
            (void)0;
        } else if (buffer[0] == 0xe) {
            (void)0;
        } else if ((sCommunicationSystem->waitingForPacketStart[param0]) && (buffer[0] & 0x1)) {
            (void)0;
        } else {
            buffer++;
            CommRring_Write(&sCommunicationSystem->sendRingClient[param0], buffer, v2 - 1, 1515);
            sCommunicationSystem->waitingForPacketStart[param0] = 0;
        }
    }
}

// WirelessManager send callbacks: advance the corresponding send state once
// the hardware confirms the transmission.
static void CommSys_SendCallbackClient(BOOL param0)
{
    if (param0) {
        sSendStateClient++;
    } else {
        GF_ASSERT(FALSE);
    }
}

static void CommSys_SendCallbackServer(BOOL param0)
{
    if (param0) {
        sSendStateServer++;
    } else {
        GF_ASSERT(FALSE);
    }
}

// Local-wireless input send. Unlike Wi-Fi, every console broadcasts its own
// packet; the host additionally relays and locally consumes its own buffer.
static void CommSys_TransmitInputWireless(void)
{
    if (!sCommunicationSystem) {
        return;
    }

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        return;
    }

    int v3 = CommSys_PlayerBlockSize(CommManager_GetCommType());
    int v4 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    if (CommSys_IsAlone()) {
        if (sSendStateClient == 2 || sSendStateClient == 0) {
            sSendStateClient++;
            CommSys_SendCallbackClient(1);

            CommSys_RecvInputServer(0, (u16 *)sCommunicationSystem->sendBuffer[sCommunicationSystem->sendBufferIndex], v3);
            sCommunicationSystem->sendBufferIndex = 1 - sCommunicationSystem->sendBufferIndex;
            sCommunicationSystem->sendCount++;
            return;
        }
    }

    if (WirelessManager_GetState() == 4) {
        if (!CommSys_IsPlayerConnected(CommSys_CurNetId())) {
            if (CommSys_CurNetId() == 1) {
                (void)0;
            }

            return;
        }

        if (sSendStateClient == 2 || sSendStateClient == 0) {
            if (CommSys_CurNetId() != 0) {
                sSendStateClient++;

                if (!WirelessManager_SendMessage(sCommunicationSystem->sendBuffer[sCommunicationSystem->sendBufferIndex], v3, 14, CommSys_SendCallbackClient)) {
                    sSendStateClient--;
                } else {
                    sCommunicationSystem->sendBufferIndex = 1 - sCommunicationSystem->sendBufferIndex;
                    sCommunicationSystem->sendCount++;
                }
            } else if (WirelessManager_GetConnectedBitmap() & 0xfffe) {
                sSendStateClient++;
                CommSys_SendCallbackClient(1);
                CommSys_RecvInputServer(0, (u16 *)sCommunicationSystem->sendBuffer[sCommunicationSystem->sendBufferIndex], v3);
                sCommunicationSystem->sendBufferIndex = 1 - sCommunicationSystem->sendBufferIndex;
                sCommunicationSystem->sendCount++;
            }
        }
    }
}

// Rewrites the outgoing D-pad bits according to playerMovementState: reverse
// flips the directions, random replaces them with a held random direction.
static void CommSys_ApplyMovementModifiers(void)
{
    u16 newHeldKeys = 0;

    if (sCommunicationSystem->playerMovementState == MOVEMENT_STATE_NORMAL) {
        return;
    }

    if (!(sCommunicationSystem->sendHeldKeys & (PAD_KEY_LEFT | PAD_KEY_RIGHT | PAD_KEY_UP | PAD_KEY_DOWN))) {
        return;
    }

    if (sCommunicationSystem->playerMovementState == MOVEMENT_STATE_REVERSE) {
        if (sCommunicationSystem->sendHeldKeys & PAD_KEY_LEFT) {
            newHeldKeys |= PAD_KEY_RIGHT;
        }

        if (sCommunicationSystem->sendHeldKeys & PAD_KEY_RIGHT) {
            newHeldKeys |= PAD_KEY_LEFT;
        }

        if (sCommunicationSystem->sendHeldKeys & PAD_KEY_UP) {
            newHeldKeys |= PAD_KEY_DOWN;
        }

        if (sCommunicationSystem->sendHeldKeys & PAD_KEY_DOWN) {
            newHeldKeys |= PAD_KEY_UP;
        }
    } else {
        if (sCommunicationSystem->randomPadKey) {
            newHeldKeys = sCommunicationSystem->randomPadKey;
            sCommunicationSystem->randomPadKeyTimer--;

            if (sCommunicationSystem->randomPadKeyTimer < 0) {
                sCommunicationSystem->randomPadKey = 0;
            }
        } else {
            switch (MATH_Rand32(&sCommunicationSystem->rand, 4)) {
            case 0:
                newHeldKeys = PAD_KEY_LEFT;
                break;
            case 1:
                newHeldKeys = PAD_KEY_RIGHT;
                break;
            case 2:
                newHeldKeys = PAD_KEY_UP;
                break;
            case 3:
                newHeldKeys = PAD_KEY_DOWN;
                break;
            }

            sCommunicationSystem->randomPadKeyTimer = MATH_Rand32(&sCommunicationSystem->rand, 16);
            sCommunicationSystem->randomPadKey = newHeldKeys;
        }
    }

    sCommunicationSystem->sendHeldKeys &= ~(PAD_KEY_LEFT | PAD_KEY_RIGHT | PAD_KEY_UP | PAD_KEY_DOWN);
    sCommunicationSystem->sendHeldKeys += newHeldKeys;
}

void CommSys_RandomizePlayerMovement(void)
{
    sCommunicationSystem->playerMovementState = MOVEMENT_STATE_RANDOM;
}

void CommSys_ReversePlayerMovement(void)
{
    sCommunicationSystem->playerMovementState = MOVEMENT_STATE_REVERSE;
}

void CommSys_RevertPlayerMovementToNormal(void)
{
    sCommunicationSystem->playerMovementState = MOVEMENT_STATE_NORMAL;
}

// Decodes a player's movement byte into receivedKeys/recvSpeed. Bit 4 marks a
// valid direction, bits 2-3 select it and bits 5-7 carry the move speed.
static BOOL CommSys_DecodeInput(u8 *param0, int param1)
{
    u8 v1[2];

    sCommunicationSystem->receivedKeys[param1] = 0;

    if (0x10 == (*param0 & 0x10)) {
        v1[0] = *param0 & 0xc;

        if (v1[0] == 0x0) {
            sCommunicationSystem->receivedKeys[param1] |= PAD_KEY_UP;
        } else if (v1[0] == 0x4) {
            sCommunicationSystem->receivedKeys[param1] |= PAD_KEY_DOWN;
        } else if (v1[0] == 0x8) {
            sCommunicationSystem->receivedKeys[param1] |= PAD_KEY_LEFT;
        } else if (v1[0] == 0xC) {
            sCommunicationSystem->receivedKeys[param1] |= PAD_KEY_RIGHT;
        }

        sCommunicationSystem->recvSpeed[param1] = (*param0 >> 5) & 0x7;
    }

    return TRUE;
}

void CommSys_Dummy(void)
{
    return;
}

// Packs the local held D-pad direction and move speed into the outgoing
// movement byte. inputSendTimer keeps the direction "recent" for UI purposes.
static BOOL CommSys_EncodeInput(u8 *param0)
{
    if (sCommunicationSystem->unk_656) {
        return FALSE;
    }

    if (CommSys_IsSendingMovementData() == 0) {
        return FALSE;
    }

    if (sCommunicationSystem->inputSendTimer) {
        sCommunicationSystem->inputSendTimer--;
    }

    if (sCommunicationSystem->sendHeldKeys & PAD_KEY_UP) {
        param0[0] = param0[0] | 0x0 | 0x10;
        sCommunicationSystem->inputSendTimer = 8;
    } else if (sCommunicationSystem->sendHeldKeys & PAD_KEY_DOWN) {
        param0[0] = param0[0] | 0x4 | 0x10;
        sCommunicationSystem->inputSendTimer = 8;
    } else if (sCommunicationSystem->sendHeldKeys & PAD_KEY_LEFT) {
        param0[0] = param0[0] | 0x8 | 0x10;
        sCommunicationSystem->inputSendTimer = 8;
    } else if (sCommunicationSystem->sendHeldKeys & PAD_KEY_RIGHT) {
        param0[0] = param0[0] | 0xC | 0x10;
        sCommunicationSystem->inputSendTimer = 8;
    }

    param0[0] |= (sCommunicationSystem->sendSpeed << 5);
    return FALSE;
}

// Builds the per-frame input packet. Byte 0 is a header: bit 0 means "this is
// a continuation of a command split across packets" and bit 1 means "the send
// queue is now empty". In parallel mode the high nibble carries a sequence
// number used by the receivers to order blocks.
static BOOL CommSys_BuildInputPacket(u8 *param0)
{
    int v1 = CommSys_PlayerBlockSize(CommManager_GetCommType());
    int v2 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    if (sCommunicationSystem->partialSend == 0) {
        param0[0] = 0x0;
    } else {
        param0[0] = 0x1;
    }

    if (CommSys_TransmissionType() == 0) {
        CommSys_EncodeInput(param0);
    }

    sCommunicationSystem->partialSend = 0;

    if (CommQueue_IsEmpty(&sCommunicationSystem->commQueueManSend)) {
        param0[0] |= 0x2;

        if (param0[0] == 0x2) {
            return FALSE;
        }
    } else {
        CommQueueWriter v3;

        v3.remaining = v1 - 1;
        v3.cursor = &param0[1];

        if (!CommQueueMan_Flush(&sCommunicationSystem->commQueueManSend, &v3, 1)) {
            sCommunicationSystem->partialSend = 1;
        }

        if (CommSys_TransmissionType() == 1) {
            sCommunicationSystem->sendSequence++;

            param0[0] |= ((sCommunicationSystem->sendSequence << 4) & 0xf0);
        }
    }

    return TRUE;
}

// Builds the 192-byte server broadcast packet: 0xB marker, continuation flag,
// the connected-player bitmap, the payload length and then the queued server
// commands.
static void CommSys_BuildServerPacket(u8 *param0)
{
    param0[0] = 0xb;

    if (sCommunicationSystem->serverPartialSend == 0) {
        param0[1] = 0x0;
    } else {
        param0[1] = 0x1;
    }

    u16 v1 = WirelessManager_GetConnectedBitmap();

    param0[2] = v1 >> 8;
    param0[3] = v1 & 0xff;

    CommQueueWriter v2;

    v2.remaining = 192 - 5;
    v2.cursor = &param0[5];

    if (CommQueueMan_Flush(&sCommunicationSystem->commQueueManSendServer, &v2, 0)) {
        sCommunicationSystem->serverPartialSend = 0;
        param0[4] = (192 - 5) - v2.remaining;
    } else {
        sCommunicationSystem->serverPartialSend = 1;
        param0[4] = 192 - 5;
    }
}

void CommSys_SetSendInterval(u8 param0)
{
    sCommunicationSystem->sendInterval = param0;
}

// TRUE when the send interval is set and the current frame falls on it, which
// makes the caller skip this frame's transmission.
static BOOL CommSys_ShouldThrottleSend(void)
{
    if (sCommunicationSystem->sendInterval == 0) {
        return FALSE;
    }

    if ((sCommunicationSystem->frameCounter % sCommunicationSystem->sendInterval) == 0) {
        return TRUE;
    }

    return FALSE;
}

BOOL CommSys_SendDataHuge(int cmd, const void *data, int size)
{
    if (!CommSys_IsPlayerConnected(CommSys_CurNetId()) && !CommSys_IsAlone()) {
        return FALSE;
    }

    if (CommQueue_Write(&sCommunicationSystem->commQueueManSend, cmd, (u8 *)data, size, 1, 0)) {
        return TRUE;
    }

    if (CommManager_GetCommType() == 10) {
        CommSys_SetError();
    }

    return FALSE;
}

BOOL CommSys_SendData(int cmd, const void *data, int size)
{
    if (!CommSys_IsPlayerConnected(CommSys_CurNetId()) && !CommSys_IsAlone()) {
        return FALSE;
    }

    if (CommQueue_Write(&sCommunicationSystem->commQueueManSend, cmd, (u8 *)data, size, 1, 1)) {
        return TRUE;
    }

    if (CommManager_GetCommType() == 10) {
        CommSys_SetError();
    }

    return FALSE;
}

BOOL CommSys_SendDataHugeServer(int cmd, const void *data, int size)
{
    if (CommSys_CurNetId() != 0) {
        GF_ASSERT(FALSE);
        return FALSE;
    }

    if (!CommSys_IsPlayerConnected(0) && !CommSys_IsAlone()) {
        return FALSE;
    }

    if (CommSys_TransmissionType() == 1) {
        return CommSys_SendDataHuge(cmd, data, size);
    }

    if (CommQueue_Write(&sCommunicationSystem->commQueueManSendServer, cmd, (u8 *)data, size, 1, 0)) {
        return TRUE;
    }

    if (CommManager_GetCommType() == 10) {
        CommSys_SetError();
    }

    return FALSE;
}

BOOL CommSys_SendDataServer(int cmd, const void *data, int size)
{
    if (CommSys_CurNetId() != 0) {
        CommSys_SetError();

        return FALSE;
    }

    if (!CommSys_IsPlayerConnected(0) && !CommSys_IsAlone()) {
        return FALSE;
    }

    if (CommSys_TransmissionType() == 1) {
        return CommSys_SendData(cmd, data, size);
    }

    if (CommQueue_Write(&sCommunicationSystem->commQueueManSendServer, cmd, (u8 *)data, size, 1, 1)) {
        return TRUE;
    }

    if (CommManager_GetCommType() == 10) {
        CommSys_SetError();
    }

    return FALSE;
}

BOOL CommSys_SendDataFixedSizeServer(int cmd, const void *data)
{
    return CommSys_SendDataServer(cmd, data, 0);
}

int CommSys_SendRingRemainingSize(void)
{
    return CommRing_RemainingSize(&sCommunicationSystem->sendRing);
}

// Clears the per-packet reassembly state after its command has been dispatched.
static void CommSys_EndCallback(int netId, int command, int param2, void *param3, CommRecvPackage *param4)
{
    CommCmd_Callback(netId, command, param2, param3);
    param4->packetCommand = 0xee;
    param4->packetSize = 0xffff;
    param4->dataBuffer = NULL;
    param4->receivedSize = 0;
}

// Dispatches commands out of one ring. A command may straddle several packets:
// partial state is kept in `param3` so the read can resume where it stopped.
// Command 17 is a hard stop and ends the drain loop.
static void CommSys_RecvDataSingle(CommRing *ring, int netId, u8 *buffer, CommRecvPackage *param3)
{
    int size;
    u8 cmd;
    int v2;
    int v3;

    while (CommRing_DataSize(ring) != 0) {
        v2 = ring->startIndex;

        if (param3->packetCommand != 0xee) {
            cmd = param3->packetCommand;
        } else {
            cmd = CommRing_ReadByte(ring);

            if (cmd == 0xee) {
                continue;
            }
        }

        v2 = ring->startIndex;
        param3->packetCommand = cmd;

        if (param3->packetSize != 0xffff) {
            size = param3->packetSize;
        } else {
            size = CommCmd_PacketSizeOf(cmd);

            if (sCommunicationSystem->commError) {
                return;
            }

            if (size == PACKET_SIZE_VARIABLE) {
                if (CommRing_DataSize(ring) < 1) {
                    ring->startIndex = v2;
                    break;
                }

                size = CommRing_ReadByte(ring) * 0x100;
                size += CommRing_ReadByte(ring);
                v2 = ring->startIndex;
            }

            param3->packetSize = size;
        }

        if (sub_020328D0(cmd)) {
            if (param3->dataBuffer == NULL) {
                param3->dataBuffer = sub_0203290C(cmd, netId, param3->packetSize);
            }

            v3 = CommRing_Read(ring, buffer, size - param3->receivedSize);

            if (param3->dataBuffer) {
                MI_CpuCopy8(buffer, &param3->dataBuffer[param3->receivedSize], v3);
            }

            param3->receivedSize += v3;

            if (param3->receivedSize >= size) {
                CommSys_EndCallback(netId, cmd, size, param3->dataBuffer, param3);

                if (cmd == 17) {
                    break;
                }
            }
        } else {
            if (CommRing_DataSize(ring) >= size) {
                CommRing_Read(ring, buffer, size);
                CommSys_EndCallback(netId, cmd, size, (void *)buffer, param3);

                if (cmd == 17) {
                    break;
                }
            } else {
                ring->startIndex = v2;
                break;
            }
        }
    }
}

// Drains commands that arrived for the local client (server/client mode).
static void CommSys_RecvData(void)
{
    int v0 = 0;

    if (!sCommunicationSystem) {
        return;
    }

    if (sCommunicationSystem->unk_6B3) {
        return;
    }

    CommRing_UpdateEndPos(&sCommunicationSystem->recvRing);

    if (CommRing_DataSize(&sCommunicationSystem->recvRing) > 0) {
        CommSys_RecvDataSingle(&sCommunicationSystem->recvRing, v0, sCommunicationSystem->tempBuffer, &sCommunicationSystem->commRecvClient);
    }
}

// Drains commands that arrived for the server (one ring per client).
static void CommSys_RecvDataServer(void)
{
    int i, v3;

    if (!sCommunicationSystem) {
        return;
    }

    if (sCommunicationSystem->unk_6B3) {
        return;
    }

    v3 = CommLocal_MaxMachines(CommManager_GetCommType()) + 1;

    for (i = 0; i < v3; i++) {
        CommRing_UpdateEndPos(&sCommunicationSystem->sendRingClient[i]);

        if (CommRing_DataSize(&sCommunicationSystem->sendRingClient[i]) > 0) {
            CommSys_RecvDataSingle(&sCommunicationSystem->sendRingClient[i], i, sCommunicationSystem->tempBuffer, &sCommunicationSystem->commRecvServer[i]);
        }
    }
}

// TRUE if the given net ID is present. Wi-Fi modes use the DWC bitmap; local
// wireless uses the host's connected bitmap, or the server-relayed bitmap that
// a client received in the broadcast.
BOOL CommSys_IsPlayerConnected(u16 netId)
{
    if (!sCommunicationSystem) {
        return FALSE;
    }

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        if (sCommunicationSystem->wifiConnected) {
            u16 v0 = DWC_GetAIDBitmap();

            if (v0 & (1 << netId)) {
                return TRUE;
            }
        }

        return FALSE;
    }

    if (!CommSys_IsInitialized()) {
        return FALSE;
    }

    if (WirelessManager_GetState() != 4) {
        return FALSE;
    }

    if (CommSys_CurNetId() == netId) {
        return TRUE;
    } else if (CommSys_CurNetId() == 0) {
        u16 v1 = WirelessManager_GetConnectedBitmap();

        if (v1 & (1 << netId)) {
            return TRUE;
        }
    } else if (sCommunicationSystem->connectedBitmap & (1 << netId)) {
        return TRUE;
    }

    return FALSE;
}

int CommSys_ConnectedCount(void)
{
    int v0 = 0, i;

    for (i = 0; i < (7 + 1); i++) {
        if (CommSys_IsPlayerConnected(i)) {
            v0++;
        }
    }

    return v0;
}

BOOL CommSys_IsInitialized(void)
{
    if (sCommunicationSystem) {
        if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
            return TRUE;
        }
    }

    return CommServerClient_IsInitialized();
}

void CommSys_SetSendSpeed(u8 param0)
{
    sCommunicationSystem->sendSpeed = param0;
}

u8 CommSys_RecvSpeed(int param0)
{
    return sCommunicationSystem->recvSpeed[param0];
}

// Returns and clears the movement keys most recently decoded for a player.
u16 CommSys_GetMovementKeys(int param0)
{
    int v0;

    if (!sCommunicationSystem) {
        return 0;
    }

    v0 = sCommunicationSystem->receivedKeys[param0];
    sCommunicationSystem->receivedKeys[param0] = 0;

    return v0;
}

void CommSys_EnableSendMovementData(void)
{
    if (sCommunicationSystem) {
        sCommunicationSystem->sendHeldKeys |= 0x8000;
    }
}

void CommSys_DisableSendMovementData(void)
{
    if (sCommunicationSystem) {
        sCommunicationSystem->sendHeldKeys = 0;
    }
}

BOOL CommSys_IsSendingMovementData(void)
{
    if (sCommunicationSystem) {
        return sCommunicationSystem->sendHeldKeys & 0x8000;
    }

    return TRUE;
}

BOOL CommSys_WriteToQueueServer(int cmd, const void *data, int size)
{
    if (CommSys_TransmissionType() == 1) {
        return CommQueue_Write(&sCommunicationSystem->commQueueManSend, cmd, (u8 *)data, size, 1, 0);
    } else {
        return CommQueue_Write(&sCommunicationSystem->commQueueManSendServer, cmd, (u8 *)data, size, 1, 0);
    }
}

BOOL CommSys_WriteToQueue(int cmd, const void *data, int size)
{
    return CommQueue_Write(&sCommunicationSystem->commQueueManSend, cmd, (u8 *)data, size, 0, 0);
}

// Topology-switch handshake, driven by transmissionState:
//   1: host sends "prepare" (cmd 11) carrying the target type
//   2: waiting for the client's acknowledgement
//   3: client sends "ack" (cmd 12) and switches locally
static void CommSys_Transmission(void)
{
    BOOL v0 = FALSE;

    if (!sCommunicationSystem) {
        return;
    }

    switch (sCommunicationSystem->transmissionState) {
    case 1:
        if (CommSys_TransmissionType() == 1) {
            v0 = CommSys_SendDataFixedSize(11, &sCommunicationSystem->pendingTransitionType);
        } else {
            v0 = CommSys_SendDataServer(11, &sCommunicationSystem->pendingTransitionType, 1);
        }

        if (v0) {
            sCommunicationSystem->transmissionState = 2;
        }
        break;
    case 3:
        if (CommSys_SendDataFixedSize(12, &sCommunicationSystem->pendingTransitionType)) {
            CommSys_SwitchTransitionType(sCommunicationSystem->pendingTransitionType);
            sCommunicationSystem->transmissionState = 0;
        }
        break;
    }
}

// Command 10 handler (host only): a switch was requested; start the prepare
// step. The requested type is carried in the first payload byte.
void CommSys_HandleSwitchRequest(int unused0, int unused1, void *param2, void *unused3)
{
    u8 *v0 = param2;

    if (CommSys_CurNetId() != 0) {
        return;
    }

    sCommunicationSystem->transmissionState = 1;
    sCommunicationSystem->pendingTransitionType = v0[0];
}

// Command 11 handler (clients only): the host announced the target topology;
// remember it and queue the acknowledgement step.
void CommSys_HandleSwitchPrepare(int unused0, int unused1, void *param2, void *unused3)
{
    u8 *v0 = param2;

    if (CommSys_CurNetId() == 0) {
        return;
    }

    sCommunicationSystem->pendingTransitionType = v0[0];
    sCommunicationSystem->transmissionState = 3;
}

// Command 12 handler (host only): the client acknowledged; complete the
// switch if we were waiting for it.
void CommSys_HandleSwitchAck(int unused0, int unused1, void *param2, void *unused3)
{
    u8 *v0 = param2;

    if (CommSys_CurNetId() != 0) {
        return;
    }

    if (sCommunicationSystem->transmissionState == 2) {
        CommSys_SwitchTransitionType(v0[0]);
        sCommunicationSystem->transmissionState = 0;
    }
}

u16 CommSys_CurNetId(void)
{
    if (sCommunicationSystem) {
        if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
            int netId = NintendoWFC_GetNetID();

            if (netId != -1) {
                return netId;
            }
        } else if (CommSys_IsAlone()) {
            return 0;
        } else {
            return WirelessManager_GetAID();
        }
    }

    return 0;
}

BOOL CommSys_SendDataFixedSize(int cmd, const void *data)
{
    return CommSys_SendData(cmd, data, 0);
}

BOOL CommSys_SendMessage(int cmd)
{
    return CommSys_SendData(cmd, NULL, 0);
}

BOOL CommSys_IsClientConnecting(void)
{
    return CommServerClient_IsClientConnecting();
}

BOOL CommSys_CheckError(void)
{
    if (CommSys_IsAlone()) {
        return FALSE;
    }

    if (sCommunicationSystem && sCommunicationSystem->commError) {
        CommManager_SetErrorHandling(1, 1);
        return TRUE;
    }

    return CommServerClient_CheckError();
}

// Size of one player's block inside a server broadcast: 12 bytes for larger
// groups (or server/client mode) and 38 bytes otherwise.
u16 CommSys_PlayerBlockSize(u16 param0)
{
    if (CommLocal_MaxMachines(param0) >= 5) {
        return 12;
    }

    if (CommSys_TransmissionType() == 0) {
        return 12;
    }

    return 38;
}

int CommType_MaxPlayers(int param0)
{
    return CommLocal_MaxMachines(param0) + 1;
}

int CommType_MinPlayers(int param0)
{
    return sub_02032698(param0) + 1;
}

void CommSys_SetAlone(BOOL param0)
{
    if (sCommunicationSystem) {
        sCommunicationSystem->isAlone = param0;
    }
}

BOOL CommSys_IsAlone(void)
{
    if (sCommunicationSystem) {
        return sCommunicationSystem->isAlone;
    }

    return FALSE;
}

// Command 2 handler: tells the host that this side is finished, then marks the
// transport finished so the shutdown path can run.
void CommSys_HandleFinishConnection(int param0, int param1, void *param2, void *param3)
{
    u8 v0;

    if (!CommServerClient_IsFinished() && CommSys_CurNetId() == 0) {
        CommSys_SendDataFixedSizeServer(2, &v0);
    }

    CommServerClient_SetFinished();
}

// Seeds the movement RNG from the current date/time plus the VBlank counter.
void CommSys_Seed(MATHRandContext32 *rand)
{
    u64 seed = 0;
    RTCDate date;
    RTCTime time;

    GetCurrentDateTime(&date, &time);
    seed = (((((((u64)date.year * 16ULL + date.month) * 32ULL) + date.day) * 32ULL + time.hour) * 64ULL + time.minute) * 64ULL + (time.second + gSystem.vblankCounter));
    MATH_InitRand32(rand, seed);
}

BOOL CommSys_IsCmdQueuedServer(int cmd)
{
    return CommQueueMan_IsCmdInQueue(&sCommunicationSystem->commQueueManSendServer, cmd);
}

BOOL CommSys_IsCmdQueued(int cmd)
{
    return CommQueueMan_IsCmdInQueue(&sCommunicationSystem->commQueueManSend, cmd);
}

BOOL CommSys_IsServerQueueEmpty(void)
{
    return CommQueue_IsEmpty(&sCommunicationSystem->commQueueManSendServer);
}

BOOL CommSys_IsQueueEmpty(void)
{
    return CommQueue_IsEmpty(&sCommunicationSystem->commQueueManSend);
}

void CommSys_SetWifiConnected(BOOL param0)
{
    sCommunicationSystem->wifiConnected = param0;
}

BOOL CommSys_WifiConnected(void)
{
    return sCommunicationSystem->wifiConnected;
}

// Records which battle position a net ID occupies (set once the battle grid is
// resolved). 0xFF means the net ID is not remapped.
void CommSys_SetBattlePosition(int param0, int param1)
{
    if (sCommunicationSystem) {
        sCommunicationSystem->battlePositions[param1] = param0;
    }
}

// Maps a net ID to its battle position, falling back to the net ID itself when
// no mapping has been set.
int CommSys_GetBattlePosition(int networkId)
{
    if (sCommunicationSystem && sCommunicationSystem->battlePositions[networkId] != 0xff) {
        return sCommunicationSystem->battlePositions[networkId];
    }

    return networkId;
}

BOOL CommSys_IsVoiceChatEnabled(void)
{
    if (!CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        return FALSE;
    }

    return NintendoWFC_GetVoiceChatEnabled();
}

// Enables/disables the "wait for every peer" send pacing. Enabling it clears
// the outstanding-send counters so they start from a known state.
void CommSys_SetRecvLimitEnabled(BOOL param0)
{
    int i;

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        if (sCommunicationSystem->recvLimitEnabled == param0) {
            return;
        }

        sCommunicationSystem->recvLimitEnabled = param0;

        if (param0) {
            sCommunicationSystem->sendCount = 0;

            for (i = 0; i < MAX_CONNECTED_PLAYERS; i++) {
                sCommunicationSystem->sendCountPerPlayer[i] = 0;
            }
        }
    }
}

// Sets the recv-limit mode used during battles and configures Wi-Fi battle
// voice chat in the opposite sense: voice chat is disabled while recv-limit
// mode is enabled.
void CommSys_SetBattleVoiceChat(BOOL param0)
{
    CommSys_SetRecvLimitEnabled(param0);

    if (CommLocal_IsWifiGroup(CommManager_GetCommType())) {
        if (param0) {
            NintendoWFC_SetVoiceChatEnabled_Battle(0);
        } else {
            NintendoWFC_SetVoiceChatEnabled_Battle(1);
        }
    }
}

// TRUE while a recently sent movement direction is still considered current.
BOOL CommSys_IsInputPending(void)
{
    if (sCommunicationSystem->inputSendTimer) {
        return TRUE;
    }

    return FALSE;
}

void CommSys_SetError(void)
{
    sCommunicationSystem->commError = 1;
}

void CommSys_StartShutdown(void)
{
    if (sCommunicationSystem) {
        sCommunicationSystem->shuttingDown = 1;
    }
}
