#include "union_room.h"

#include <nitro.h>

#include "constants/union_room_message_types.h"
#include "constants/versions.h"

#include "struct_decls/struct_0205B43C_decl.h"
#include "struct_defs/struct_0203330C.h"
#include "struct_defs/struct_0205B4F8.h"

#include "field/field_system.h"

#include "appearance.h"
#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "easy_chat_sentence.h"
#include "easy_chat_words.h"
#include "field_system.h"
#include "field_task.h"
#include "heap.h"
#include "journal.h"
#include "math_util.h"
#include "message.h"
#include "save_player.h"
#include "savedata.h"
#include "string_gf.h"
#include "string_template.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "trainer_case.h"
#include "trainer_info.h"
#include "comm_server_client.h"
#include "union_room_drawing_comm.h"
#include "comm_field_cmd.h"

#include "constdata/const_020ED570.h"
#include "res/text/bank/country_names.h"
#include "res/text/bank/greetings.h"
#include "res/text/bank/union_room.h"

// Union Room communication state. The Union Room lets two players interact
// wirelessly: they can greet each other, show trainer cases, battle, trade,
// draw, mix records or spin trade. This module owns the connection state
// machine (driven by UnionRoom_Update) and the activity handshake that the
// Union Room scripts drive through the ScrCmd_* commands.
//
// The state machine is a single callback (stateFunc) plus a frame countdown
// (stateDelay); UnionRoom_SetState installs the next callback and delay.

// Callback invoked once per frame by the Union Room system task.
typedef void (*UnionRoomStateFunc)(UnionRoom *);

// Unused scratch entries; the original code never reads or writes them.
typedef struct {
    u16 unk_00;
    u16 unk_02;
} UnkStruct_0205B43C_sub1;

// Unused scratch entries; the original code never reads or writes them.
typedef struct {
    u16 unk_00;
    u16 unk_02;
} UnkStruct_0205B43C_sub2;

struct UnionRoom {
    FieldSystem *fieldSystem;
    SaveData *saveData;
    TrainerInfo *trainerInfo; // Local player's trainer info
    SysTask *task; // System task that drives the state machine
    UnionRoomStateFunc stateFunc; // Current state callback
    int stateDelay; // Frames to wait before the state callback runs
    u32 connectionID; // Net ID of the peer we are connecting to
    u32 connectState; // Connection result: 0 pending, 1 connected, 2 failed, 3 connecting
    int pendingRequest; // Pending activity request: 0 none, 1 connect/mix records/spin trade, 2 draw
    int unk_24; // Unused (written but never read)
    int unk_28; // Unused (written but never read)
    int unk_2C; // Unused (written but never read)
    u32 activity; // Activity requested by the peer, or being started locally
    u32 selectedActivity; // Activity the local player picked from the menu (1-based)
    u32 unk_38; // Unused
    u32 unk_3C; // Unused (written but never read)
    u32 peerActivity; // Activity the peer agreed to (7 = declined/busy)
    u32 peerNoActivity; // Set when the peer reports it has no activity
    UnkStruct_0205B43C_sub1 unk_48[10]; // Unused
    UnkStruct_0205B43C_sub2 unk_70[40]; // Unused
    WMbssDesc *bssDesc[16]; // Wireless beacon descriptors, indexed by net ID
    MATHRandContext32 rng; // RNG used to pick random busy messages
    int unk_168; // Unused
    int unk_16C; // Unused
    int unk_170; // Unused
    u16 unk_174; // Unused (written but never read)
    u8 menuChoice[2]; // Per-player party menu choice (1 = confirm, 2 = cancel)
    EasyChatSentence easyChatSentence; // Pending easy chat sentence to broadcast
    BOOL hasEasyChatSentence; // Whether easyChatSentence holds a pending sentence
    TrainerCase *trainerCase; // Local player's trainer case
    TrainerCase *trainerCases[2]; // Trainer cases exchanged with the peer
};

static UnionRoom *UnionRoom_New(FieldSystem *fieldSystem);
static void UnionRoom_StateConnect(UnionRoom *param0);
static void UnionRoom_StateRestart(UnionRoom *param0);
static void UnionRoom_SetState(UnionRoom *param0, UnionRoomStateFunc param1, int param2);
static void UnionRoom_StateExit(UnionRoom *param0);
static void UnionRoom_StateWaitExit(UnionRoom *param0);
static void UnionRoom_Free(UnionRoom *param0);
static void UnionRoom_StateConnectClient(UnionRoom *param0);
static void UnionRoom_StateWaitClient(UnionRoom *param0);
static void UnionRoom_StateWaitClientReady(UnionRoom *param0);
static void UnionRoom_StateWaitDisconnect(UnionRoom *param0);
static void UnionRoom_StateWaitClientInfo(UnionRoom *param0);
static void UnionRoom_StateConnectFailed(UnionRoom *param0);
static void UnionRoom_ResetState(UnionRoom *param0);
static int UnionRoom_HasConnectedTrainer(void);
static int UnionRoom_GetTrainerBusyMessage(UnionRoom *param0, int param1);
static int UnionRoom_GetStartMessage(int param0, int gender, StringTemplate *strTemplate);
static void UnionRoom_StateInit(UnionRoom *param0);
static void UnionRoom_ResetGameInfo(UnkStruct_0205B4F8 *param0);

// Allocates the Union Room state and starts its state machine. Called when the
// player enters the Union Room. Returns NULL if it is already initialised.
UnionRoom *FieldSystem_InitCommUnionRoom(FieldSystem *fieldSystem)
{
    UnionRoom *v0 = NULL;

    GF_ASSERT(fieldSystem != NULL);

    if (fieldSystem->unk_7C != NULL) {
        return NULL;
    }

    if (Heap_CreateAtEnd(HEAP_ID_APPLICATION, HEAP_ID_31, 0xa80)) {
        (void)0;
    }

    v0 = UnionRoom_New(fieldSystem);

    if (v0 == NULL) {
        v0 = fieldSystem->unk_7C;
    }

    CommFieldCmd_Init((void *)fieldSystem);
    CommManager_SetMaxNumConnections(2);
    UnionRoom_SetState(v0, UnionRoom_StateInit, 40);

    return v0;
}

// Begins tearing down the Union Room state machine.
void UnionRoom_Exit(FieldSystem *fieldSystem)
{
    if (fieldSystem->unk_7C == NULL) {
        return;
    }

    UnionRoom_SetState(fieldSystem->unk_7C, UnionRoom_StateExit, 5);
}

// Allocates and initialises the Union Room state. The state machine starts in
// the idle state and is driven by UnionRoom_Update.
static UnionRoom *UnionRoom_New(FieldSystem *fieldSystem)
{
    void *v0;
    SaveData *saveData;
    UnionRoom *v2 = NULL;

    if (fieldSystem->unk_7C != NULL) {
        return NULL;
    }

    saveData = FieldSystem_GetSaveData(fieldSystem);
    CommManager_StartUnion(saveData);

    v2 = (UnionRoom *)Heap_Alloc(HEAP_ID_31, sizeof(UnionRoom));
    MI_CpuClear8(v2, sizeof(UnionRoom));

    v2->stateFunc = NULL;
    v2->stateDelay = 40;
    v2->task = SysTask_Start(UnionRoom_Update, v2, 10);
    v2->fieldSystem = fieldSystem;
    v2->saveData = saveData;
    v2->trainerInfo = SaveData_GetTrainerInfo(saveData);

    UnionRoom_ResetState(v2);
    CommSys_Seed(&v2->rng);

    return v2;
}

// Initial state: wait until the wireless server/client is ready, then publish
// the player's easy chat sentence and move on to the connection state.
static void UnionRoom_StateInit(UnionRoom *param0)
{
    EasyChatSentence v0;

    if (CommServerClient_IsInitialized()) {
        EasyChatSentence_InitWithEnteredUnionRoom(&v0);
        UnionRoom_InitGameInfo(&v0);
        UnionRoom_SetEasyChatSentence(param0, &v0);
        UnionRoom_SetState(param0, UnionRoom_StateConnect, 40);
    }
}

// Write-only counter; the original code increments and resets it but never
// reads it.
static int Unk_021C0850;

// Idle connection state. If the player has requested an activity, start the
// matching connection (mix records, spin trade, draw or a plain union
// connection) and wait for it to complete.
static void UnionRoom_StateConnect(UnionRoom *param0)
{
    if (CommManager_IsConnectUnionServer()) {
        Unk_021C0850 = 0;
        UnionRoom_SetState(param0, UnionRoom_StateWaitClient, 0);
        return;
    }

    if (param0->pendingRequest != 0) {
        param0->unk_28 = 2;

        if (param0->pendingRequest == 1) {
            if (param0->activity == 5) {
                CommManager_StartMixRecordsClient(param0->connectionID);
            } else if (param0->activity == 6) {
                CommManager_StartSpinTradeClient(param0->connectionID);
            } else {
                CommManager_ConnectUnion(param0->connectionID);
            }
        } else if (param0->pendingRequest == 2) {
            UnionRoomDrawing_RegisterCommHandlers(NULL);
            CommManager_StartDrawClient(param0->connectionID);
        }

        UnionRoom_SetState(param0, UnionRoom_StateConnectClient, 12);
        return;
    }
}

// Wait for the union search to restart, then return to the connection state.
static void UnionRoom_StateRestart(UnionRoom *param0)
{
    if (CommManager_UnionRestartSuccess() == 1) {
        CommFieldCmd_Init((void *)param0->fieldSystem);
        UnionRoom_SetState(param0, UnionRoom_StateConnect, 2);
    }
}

// TRUE if at least one of the other net IDs (1-4) has a trainer registered.
static int UnionRoom_HasConnectedTrainer(void)
{
    int v0, v1;
    TrainerInfo *v2;

    v1 = 0;

    for (v0 = 1; v0 < 5; v0++) {
        v2 = CommInfo_TrainerInfo(v0);

        if (v2 != NULL) {
            v1++;
        }
    }

    return v1 >= 1;
}

// Wait for a client to connect. Once one is present, send our player info and
// broadcast that we are busy (11) while the handshake completes. If the union
// server is lost, restart the search.
static void UnionRoom_StateWaitClient(UnionRoom *param0)
{
    UnkStruct_0205B4F8 *v0;

    if (param0->stateDelay > 0) {
        param0->stateDelay--;
        return;
    }

    Unk_021C0850++;
    v0 = CommServerClient_GetBattleRegulation();

    if (CommSys_IsClientConnecting() && (UnionRoom_HasConnectedTrainer() == 1) && (v0->unk_1C != 4)) {
        CommInfo_SendPlayerInfo();
        CommManager_SetErrorHandling(1, 1);
        UnionRoom_BroadcastActivity(11);
        UnionRoom_SetState(param0, UnionRoom_StateWaitClientReady, 0);
    }

    if (CommManager_IsConnectUnionServer() == 0) {
        CommManager_UnionRestartSearch();
        UnionRoom_ResetState(param0);
        UnionRoom_BroadcastActivity(0);
        UnionRoom_SetState(param0, UnionRoom_StateRestart, 2);
    }
}

// Wait for the client handshake to finish. If the client disconnects, restart
// the union search.
static void UnionRoom_StateWaitClientReady(UnionRoom *param0)
{
    if (CommManager_CheckError() && (0 == CommSys_IsClientConnecting())) {
        return;
    }

    if (0 == CommSys_IsClientConnecting()) {
        CommManager_UnionRestartSearch();
        UnionRoom_ResetState(param0);
        UnionRoom_BroadcastActivity(0);
        UnionRoom_SetState(param0, UnionRoom_StateRestart, 2);
    }
}

// Installs the next state callback and the number of frames to wait before it
// runs.
static void UnionRoom_SetState(UnionRoom *param0, UnionRoomStateFunc param1, int param2)
{
    param0->stateFunc = param1;
    param0->stateDelay = param2;
}

// Write-only mirror of the beacon descriptors cached in UnionRoom_Update.
static WMBssDesc *Unk_021C085C[16];

// System task callback: refreshes the cached wireless beacon descriptors and
// runs the current state callback.
void UnionRoom_Update(SysTask *param0, void *param1)
{
    UnionRoom *v0 = (UnionRoom *)param1;

    if (v0 == NULL) {
        SysTask_Done(param0);
    } else {
        int v1;
        WMBssDesc *v2;

        for (v1 = 0; v1 < 16; v1++) {
            v0->bssDesc[v1] = CommServerClient_GetServerBssDesc(v1);
            Unk_021C085C[v1] = v0->bssDesc[v1];
        }

        if (v0->stateFunc != NULL) {
            v0->stateFunc(v0);
        }
    }
}

// Exit state: leave the union after a short delay, then wait for the comm
// system to shut down.
static void UnionRoom_StateExit(UnionRoom *param0)
{
    if (param0->stateDelay != 0) {
        param0->stateDelay--;
        return;
    }

    CommManager_ExitUnion();
    UnionRoom_SetState(param0, UnionRoom_StateWaitExit, 0);
}

// Wait for the comm system to finish shutting down, then free the state.
static void UnionRoom_StateWaitExit(UnionRoom *param0)
{
    if (CommSys_IsInitialized()) {
        return;
    }

    UnionRoom_Free(param0);
}

// Wait for the union client connection to succeed. On success, send our player
// info and wait for the peer's trainer info; on failure, restart the search.
static void UnionRoom_StateConnectClient(UnionRoom *param0)
{
    if (1 == CommManager_IsConnectedUnionClientSuccess()) {
        CommInfo_SendPlayerInfo();
        UnionRoom_SetState(param0, UnionRoom_StateWaitClientInfo, 3);
        return;
    } else if (CommSys_IsClientConnecting()) {
        param0->pendingRequest = 0;
        param0->connectState = 3;

        UnionRoom_SetState(param0, UnionRoom_StateWaitClient, 0);
    }

    if (0 == CommManager_IsConnectedUnionClientSuccess()) {
        return;
    }

    UnionRoom_SetState(param0, UnionRoom_StateConnectFailed, 2);

    param0->unk_24 = 0;
    param0->connectState = 2;
    param0->pendingRequest = 0;
    param0->peerNoActivity = 0;
}

// Wait for the field task to stop, then restart the union search.
static void UnionRoom_StateConnectFailed(UnionRoom *param0)
{
    if (!FieldSystem_IsRunningTask(param0->fieldSystem)) {
        CommManager_UnionRestartSearch();
        UnionRoom_ResetState(param0);
        UnionRoom_BroadcastActivity(0);
        UnionRoom_SetState(param0, UnionRoom_StateRestart, 2);
    }
}

// Wait for the connected client's trainer info to become available. Once it
// is, the connection is complete and we wait for the peer to disconnect.
static void UnionRoom_StateWaitClientInfo(UnionRoom *param0)
{
    if (1 == CommManager_IsConnectedUnionClientSuccess()) {
        if (CommInfo_TrainerInfo(CommSys_CurNetId()) != NULL) {
            param0->pendingRequest = 0;
            param0->connectState = 1;
            param0->peerNoActivity = 0;

            CommManager_SetErrorHandling(1, 1);
            UnionRoom_SetState(param0, UnionRoom_StateWaitDisconnect, 3);
        }
    } else if (0 == CommManager_IsConnectedUnionClientSuccess()) {
        CommManager_UnionRestartSearch();
        UnionRoom_ResetState(param0);
        UnionRoom_SetState(param0, UnionRoom_StateRestart, 2);

        param0->unk_24 = 0;
        param0->connectState = 2;
        param0->pendingRequest = 0;
        param0->peerNoActivity = 0;
    }
}

// Wait for the peer to disconnect, then restart the union search.
static void UnionRoom_StateWaitDisconnect(UnionRoom *param0)
{
    if (0 == CommManager_IsConnectedUnionClientSuccess()) {
        CommManager_UnionRestartSearch();
        UnionRoom_ResetState(param0);
        UnionRoom_SetState(param0, UnionRoom_StateRestart, 2);

        return;
    }
}

// Stops the state machine task and frees the Union Room state.
static void UnionRoom_Free(UnionRoom *param0)
{
    void *v0;

    if (param0 == NULL) {
        return;
    }

    SysTask_Done(param0->task);
    Heap_Free(param0);
    Heap_Destroy(HEAP_ID_31);
}

// Returns the field system that owns this Union Room state.
FieldSystem *UnionRoom_GetFieldSystem(UnionRoom *param0)
{
    return param0->fieldSystem;
}

// Returns the wireless beacon descriptor for the given net ID.
WMBssDesc *UnionRoom_GetBssDesc(UnionRoom *param0, int param1)
{
    return param0->bssDesc[param1];
}

// Write-only cache of the peer's broadcast game info.
static UnkStruct_0205B4F8 *Unk_021C0854;

// Returns the activity status of the trainer at the given local ID (1-based):
// 1 = greet, 2 = draw, 3 = mix records, 4 = spin trade, 5 = busy. The status
// is read from the peer's broadcast game info.
int UnionRoom_GetTrainerStatus(UnionRoom *param0, int param1)
{
    TrainerInfo *v0;
    UnkStruct_0203330C *v1;
    UnkStruct_0205B4F8 *v2;

    param1--;
    v0 = CommServerClient_GetServerTrainerInfo(param1);

    UnionRoom_ResetActivity(param0);

    if (v0 == NULL) {
        return 5;
    }

    if (param0->bssDesc[param1] == NULL) {
        return 5;
    }

    v1 = (UnkStruct_0203330C *)param0->bssDesc[param1]->gameInfo.userGameInfo;
    v2 = (UnkStruct_0205B4F8 *)v1->unk_30;

    Unk_021C0854 = v2;

    switch (v2->unk_1C) {
    case 0:
        return 1;
        break;
    case 1:
        return 2;
        break;
    case 2:
        return 3;
        break;
    case 13:
    case 3:
        return 4;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 11:
        return 5;
        break;
    }

    return 5;
}

// Attempts to start an activity with the trainer at the given local ID
// (1-based). param2 is the activity the local player wants (1 = greet,
// 2 = draw, 3 = mix records, 4 = spin trade). Returns 1 on success, 5 if the
// peer is busy or the requested activity does not match what they are doing.
int UnionRoom_RequestActivity(UnionRoom *param0, int param1, u16 param2)
{
    TrainerInfo *v0;
    UnkStruct_0203330C *v1;
    UnkStruct_0205B4F8 *v2;

    param1--;

    if (param0->bssDesc[param1] == NULL) {
        return 5;
    }

    v1 = (UnkStruct_0203330C *)param0->bssDesc[param1]->gameInfo.userGameInfo;
    v2 = (UnkStruct_0205B4F8 *)v1->unk_30;

    Unk_021C0854 = v2;

    switch (v2->unk_1C) {
    case 2:
        if (param2 != 3) {
            return 5;
        }

        param0->activity = 5;
        param0->connectionID = param1;
        param0->pendingRequest = 1;
        param0->unk_24 = 0;
        param0->connectState = 0;
        return 1;
        break;
    case 0:
        if (param2 != 1) {
            return 5;
        }

        param0->connectionID = param1;
        param0->pendingRequest = 1;
        param0->unk_24 = 0;
        param0->connectState = 0;
        return 1;
        break;
    case 1:
        if (param2 != 2) {
            return 5;
        }

        param0->connectionID = param1;
        param0->pendingRequest = 2;
        param0->unk_24 = 0;
        param0->connectState = 0;
        return 1;
        break;
    case 13:
    case 3:
        if (param2 != 4) {
            return 5;
        }

        param0->activity = 6;
        param0->connectionID = param1;
        param0->pendingRequest = 1;
        param0->connectState = 0;
        return 1;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 11:
        return 5;
        break;
    default:
        return 5;
        break;
    }

    GF_ASSERT(FALSE);

    return 0;
}

// Returns the connection result: 0 pending, 1 connected, 2 failed, 3 connecting.
u32 UnionRoom_GetConnectState(UnionRoom *param0)
{
    return param0->connectState;
}

// Returns the activity the peer agreed to, or 7 if there is no peer or the
// peer reported no activity.
u32 UnionRoom_GetPeerActivity(UnionRoom *param0)
{
    if (param0->peerNoActivity) {
        return 7;
    }

    if (CommSys_ConnectedCount() < 2) {
        return 7;
    }

    if (CommSys_CurNetId() == 0) {
        if (CommManager_IsConnectUnionServer() == 1) {
            return param0->peerActivity;
        }
    } else {
        if (CommManager_IsConnectedUnionClientSuccess() == 1) {
            return param0->peerActivity;
        }
    }

    return 7;
}

// Returns the activity requested by the peer, or 7 if we are not the server.
u32 UnionRoom_GetActivity(UnionRoom *param0)
{
    if (CommManager_IsConnectUnionServer() == 1) {
        return param0->activity;
    }

    return 7;
}

// Sends an activity handshake packet. param1 selects the direction:
// 0 = we are requesting an activity (send our selection on command 99),
// 1 = we are answering a request (send our activity on command 103, or 7 to
// decline).
void UnionRoom_SendActivityRequest(UnionRoom *param0, int param1, u32 param2)
{
    u8 v0 = (u8)param2;

    switch (param1) {
    case 0:
        if (param0->peerNoActivity == 0) {
            param0->selectedActivity = v0;
            CommSys_SendData(99, &v0, 1);
        }
        break;
    case 1:
        if (param2 == 0) {
            u8 v1 = param0->activity;

            CommSys_SendDataServer(103, &v1, 1);
            param0->unk_3C = param2;
        } else {
            u8 v2 = 7;

            CommSys_SendDataServer(103, &v2, 1);
            param0->unk_3C = param2;
        }
        break;
    }
}

// Command 98 handler: unused.
void UnionRoom_HandleNoOpTrainerInfo(int param0, int param1, void *param2, void *param3)
{
    return;
}

// Command 100 handler: unused.
void UnionRoom_HandleNoOpNetId(int param0, int param1, void *param2, void *param3)
{
    return;
}

// Command 102 handler: reset the state machine back to the connection state.
void UnionRoom_HandleResetState(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;

    UnionRoom_SetState(fieldSystem->unk_7C, UnionRoom_StateConnect, 2);
    UnionRoom_ResetState(fieldSystem->unk_7C);
}

// Write-only cache of the activity received on command 99.
static int Unk_021C0858;

// Command 99 handler: record the activity the peer requested.
void UnionRoom_HandleSetActivity(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;
    u8 *v1 = (u8 *)param2;

    if (fieldSystem->unk_7C->peerNoActivity == 0) {
        fieldSystem->unk_7C->activity = *v1;
        Unk_021C0858 = *v1;
    }
}

// Command 103 handler: record the activity the peer agreed to. A value of 4
// (draw) also starts the draw server.
void UnionRoom_HandlePeerActivity(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;
    u8 *v1 = (u8 *)param2;

    fieldSystem->unk_7C->unk_2C = 1;
    fieldSystem->unk_7C->peerActivity = *v1;

    if (*v1 == 4) {
        CommManager_StartDrawServer();
    }
}

// Command 104 handler: the peer reports it has no activity.
void UnionRoom_HandlePeerNoActivity(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;

    fieldSystem->unk_7C->peerNoActivity = 1;
}

// Returns whether the peer has reported that it has no activity.
int UnionRoom_GetPeerNoActivity(UnionRoom *param0)
{
    return param0->peerNoActivity;
}

// If we have no activity, tell the peer (command 104) and return param1.
int UnionRoom_CancelActivity(UnionRoom *param0, int param1)
{
    if (param0->activity == 0) {
        CommSys_SendData(104, NULL, 0);
        return param1;
    }

    return 0;
}

// Command 105 handler: receives the peer's trainer case, marks it as copied
// and records a journal entry for the greeting.
void UnionRoom_HandleTrainerCase(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;
    TrainerCase *trainerCase = (TrainerCase *)param2;
    TrainerInfo *trainerInfo = CommInfo_TrainerInfo(CommSys_CurNetId() ^ 1);
    void *journalEntryOnlineEvent;

    int i, v5 = 0;
    u8 *v6 = (u8 *)param2;

    for (i = 0; i < sizeof(TrainerCase); i++) {
        v5 ^= v6[i];
    }

    trainerCase->unk_66A = 1;

    if (param0 != CommSys_CurNetId()) {
        journalEntryOnlineEvent = JournalEntry_CreateEventGreetedInUnionRoom((u16 *)TrainerInfo_Name(trainerInfo), TrainerInfo_Gender(trainerInfo), 31);
        JournalEntry_SaveData(fieldSystem->journalEntry, journalEntryOnlineEvent, JOURNAL_ONLINE_EVENT);
    }
}

// Returns the buffer used to send our trainer case for the given net ID.
u8 *UnionRoom_GetTrainerCaseBuffer(int param0, void *param1, int param2)
{
    FieldSystem *fieldSystem = (FieldSystem *)param1;
    UnionRoom *v1 = fieldSystem->unk_7C;

    return (u8 *)v1->trainerCases[param0];
}

// Command 101 handler: records the peer's party menu choice.
void UnionRoom_HandleMenuChoice(int param0, int param1, void *param2, void *param3)
{
    FieldSystem *fieldSystem = (FieldSystem *)param3;
    UnionRoom *v1 = fieldSystem->unk_7C;
    u8 *v2 = (u8 *)param2;

    v1->menuChoice[param0] = *v2;
}

// Returns 1 if the local player cancelled the party menu, 2 if the peer did,
// or 0 if neither has.
u16 UnionRoom_GetCancelState(UnionRoom *param0)
{
    int v0 = CommSys_CurNetId();

    if (param0->menuChoice[v0] == 2) {
        return 1;
    }

    if (param0->menuChoice[v0 ^ 1] == 2) {
        return 2;
    }

    return 0;
}

// Sends our party menu choice (1 = confirm, 2 = cancel) on command 101.
void UnionRoom_SendMenuChoice(int param0)
{
    u8 v0 = param0;
    CommSys_SendData(101, &v0, 1);
}

// Message tables indexed by [activity][gender]. The activity index matches the
// value passed to ScrCmd_143 and stored in selectedActivity.
static const int sMessagesShowingTrainerCase[][2] = {
    { UnionRoom_Text_ShowTrainerCaseMale, UnionRoom_Text_ShowTrainerCaseFemale },
    { UnionRoom_Text_ShowingTrainerCaseMale, UnionRoom_Text_ShowingTrainerCaseFemale }
};

static const int sMessagesDrawing[][2] = {
    { UnionRoom_Text_DrawingMale1, UnionRoom_Text_DrawingFemale1 },
    { UnionRoom_Text_DrawingMale2, UnionRoom_Text_DrawingFemale2 },
    { UnionRoom_Text_DrawingMale3, UnionRoom_Text_DrawingFemale3 },
    { UnionRoom_Text_DrawingMale4, UnionRoom_Text_DrawingFemale4 }
};

static const int sMessagesBattling[][2] = {
    { UnionRoom_Text_BattlingMale1, UnionRoom_Text_BattlingFemale1 },
    { UnionRoom_Text_BattlingMale2, UnionRoom_Text_BattlingFemale2 },
    { UnionRoom_Text_BattlingMale3, UnionRoom_Text_BattlingFemale3 },
    { UnionRoom_Text_BattlingMale4, UnionRoom_Text_BattlingFemale4 }
};

static const int sMessagesTrading[][2] = {
    { UnionRoom_Text_TradingMale1, UnionRoom_Text_TradingFemale1 },
    { UnionRoom_Text_TradingMale2, UnionRoom_Text_TradingFemale2 }
};

static const int sMessagesMixingRecords[][2] = {
    { UnionRoom_Text_MixingRecordsMale1, UnionRoom_Text_MixingRecordsFemale1 },
    { UnionRoom_Text_MixingRecordsMale2, UnionRoom_Text_MixingRecordsFemale2 },
    { UnionRoom_Text_MixingRecordsMale3, UnionRoom_Text_MixingRecordsFemale3 },
    { UnionRoom_Text_MixingRecordsMale4, UnionRoom_Text_MixingRecordsFemale4 }
};

static const int sMessagesSpinTrading[][2] = {
    { UnionRoom_Text_SpinTradingMale1, UnionRoom_Text_SpinTradingFemale1 },
    { UnionRoom_Text_SpinTradingMale2, UnionRoom_Text_SpinTradingFemale2 },
    { UnionRoom_Text_SpinTradingMale3, UnionRoom_Text_SpinTradingFemale3 },
    { UnionRoom_Text_SpinTradingMale4, UnionRoom_Text_SpinTradingFemale4 }
};

static const int sMessagesStartActivity[][2] = {
    { UnionRoom_Text_ShowTrainerCaseMale, UnionRoom_Text_ShowTrainerCaseFemale },
    { UnionRoom_Text_LetsStartBattleMale, UnionRoom_Text_LetsStartBattleFemale },
    { UnionRoom_Text_LetsStartTradeMale, UnionRoom_Text_LetsStartTradeFemale },
    { UnionRoom_Text_LetsStartDrawMale, UnionRoom_Text_LetsStartDrawFemale },
    { UnionRoom_Text_LetsStartMixRecordsMale, UnionRoom_Text_LetsStartMixRecordsFemale },
    { UnionRoom_Text_LetsStartSpinTradeMale, UnionRoom_Text_LetsStartSpinTradeFemale },
    { UnionRoom_Text_LetsStartSpinTradeMale, UnionRoom_Text_LetsStartSpinTradeFemale }
};

static const int sMessagesThisIsPlayerAskDoSomething[2] = {
    UnionRoom_Text_ThisIsPlayerSomethingToDoMale,
    UnionRoom_Text_ItsPlayerDoSomethingFemale
};

static const int sMessagesTrainerAppearsBusy[2] = {
    UnionRoom_Text_TrainersAppearsBusy,
    UnionRoom_Text_TrainersAppearsBusyFemale
};

static const int sMessagesDeclinedStartActivity[][2] = {
    { UnionRoom_Text_DeclinedShowTrainerCaseMale, UnionRoom_Text_DeclinedShowTrainerCaseFemale },
    { UnionRoom_Text_DeclinedDrawMale, UnionRoom_Text_DeclinedDrawFemale },
    { UnionRoom_Text_DeclinedBattleMale, UnionRoom_Text_DeclinedBattleFemale },
    { UnionRoom_Text_DeclinedTradeMale, UnionRoom_Text_DeclinedTradeFemale },
    { UnionRoom_Text_DeclinedUnusedMale, UnionRoom_Text_DeclinedUnusedFemale },
    { UnionRoom_Text_DeclinedSpinTradeMixRecordsMale, UnionRoom_Text_DeclinedSpinTradeMixRecordsFemale }
};

static const int sMessagesRequirements[][2] = {
    { UnionRoom_Text_NeedTwoLv30PokemonToBattleMale, UnionRoom_Text_NeedTwoLv30PokemonToBattleFemale },
    { UnionRoom_Text_CantTradeIfOnePokemonMale, UnionRoom_Text_CantTradeIfOnePokemonFemale },
    { UnionRoom_Text_NeedEggToSpinTradeMale, UnionRoom_Text_NeedEggToSpinTradeFemale }
};

static const int sMessagesWaitForAnswer[][2] = {
    { UnionRoom_Text_HeresMyTrainerCase, UnionRoom_Text_IllShowMyTrainerCase },
    { UnionRoom_Text_WaitForBattleAnswerMale, UnionRoom_Text_WaitForBattleAnswerFemale },
    { UnionRoom_Text_WaitForTradeAnswerMale, UnionRoom_Text_WaitForTradeAnswerFemale },
    { UnionRoom_Text_WaitForDrawAnswerMale, UnionRoom_Text_WaitForDrawAnswerFemale },
    { UnionRoom_Text_WaitForMixRecordsAnswerMale, UnionRoom_Text_WaitForMixRecordsAnswerFemale },
    { UnionRoom_Text_WaitForSpinTradeAnswerMale, UnionRoom_Text_WaitForSpinTradeAnswerFemale },
    { UnionRoom_Text_WaitForSpinTradeAnswerMale, UnionRoom_Text_WaitForSpinTradeAnswerFemale }
};

static const int sMessagesAskJoinActivity[][2] = {
    { UnionRoom_Text_AskJoinDrawMale, UnionRoom_Text_AskJoinDrawFemale },
    { UnionRoom_Text_AskJoinMixRecordsMale, UnionRoom_Text_AskJoinMixRecordsFemale },
    { UnionRoom_Text_AskJoinSpinTradeMale, UnionRoom_Text_AskJoinSpinTradeFemale }
};

static const int sMessagesJoinedActivity[][2] = {
    { UnionRoom_Text_LetsDrawTogetherMale, UnionRoom_Text_LetsGetDrawingFemale },
    { UnionRoom_Text_LetsMixRecordsTogetherMale, UnionRoom_Text_LetsMixRecordsTogetherFemale },
    { UnionRoom_Text_LetsSpinTradeTogetherMale, UnionRoom_Text_LetsSpinTradeTogetherFemale }
};

static const int sMessagesDeclinedJoinActivity[][2] = {
    { UnionRoom_Text_DeclinedJoinDrawMale, UnionRoom_Text_DeclinedJoinDrawFemale },
    { UnionRoom_Text_DeclinedJoinMixRecordsMale, UnionRoom_Text_DeclinedJoinMixRecordsFemale },
    { UnionRoom_Text_DeclinedJoinSpinTradeMale, UnionRoom_Text_DeclinedJoinSpinTradeFemale }
};

static const int sMessagesDoSomethingElse[] = {
    UnionRoom_Text_DoAnythingElseMale,
    UnionRoom_Text_DoSomethingElseFemale
};

static const int sMessagesAskIfYouWantSomething[2] = {
    UnionRoom_Text_ShoutIfYouWantSomethingMale,
    UnionRoom_Text_AskIfYouWantSomethingFemale
};

static const int sMessagesSomethingElseToDo[2] = {
    UnionRoom_Text_SorrySomethingElseToDoMale,
    UnionRoom_Text_SorrySomethingElseToDoFemale
};

static const int sMessagesBadEgg[2] = {
    UnionRoom_Text_BadEggInPartyMale,
    UnionRoom_Text_BadEggInPartyFemale
};

// Base map object slot for each group of four Union Room trainers. The ten
// groups cover slots 10-49; UnionRoom_GetTrainerGroup maps a slot back to its
// group index.
const u16 gUnionRoomTrainerGroupBaseSlots[] = {
    10,
    14,
    18,
    22,
    26,
    30,
    34,
    38,
    42,
    46
};

// Returns the group index (0-9) containing the given map object slot, or -1.
static int UnionRoom_GetTrainerGroup(int param0)
{
    int v0, v1, v2;

    for (v0 = 0; v0 < 10; v0++) {
        if ((gUnionRoomTrainerGroupBaseSlots[v0] <= param0) && ((gUnionRoomTrainerGroupBaseSlots[v0] + 4) > param0)) {
            return v0;
        }
    }

    return -1;
}

// Returns the "trainer is busy" message for the trainer at the given local ID
// (1-based). The message depends on the activity the trainer is broadcasting;
// a random variant is chosen for most activities.
static int UnionRoom_GetTrainerBusyMessage(UnionRoom *param0, int param1)
{
    int gender, v1;

    if (param1 > 9) {
        v1 = UnionRoom_GetTrainerGroup(param1);
        GF_ASSERT(param1 != -1);
    } else {
        v1 = param1;
    }

    if (param0->bssDesc[v1] == NULL) {
        return UnionRoom_Text_TrainersAppearsBusy;
    }

    TrainerInfo *trainerInfo = CommServerClient_GetServerTrainerInfo(v1);
    UnkStruct_0203330C *v3 = (UnkStruct_0203330C *)param0->bssDesc[v1]->gameInfo.userGameInfo;
    UnkStruct_0205B4F8 *v4 = (UnkStruct_0205B4F8 *)v3->unk_30;

    if (trainerInfo == NULL) {
        return UnionRoom_Text_TrainersAppearsBusy;
    }

    if (param1 > 9) {
        gender = v4->unk_18[(param1 - 10) % 4];
        gender = gender >> 7;
    } else {
        gender = TrainerInfo_Gender(trainerInfo);
    }

    switch (v4->unk_1C) {
    case 4:
    case 11:
        return sMessagesTrainerAppearsBusy[gender];
        break;
    case 5:
        return sMessagesShowingTrainerCase[LCRNG_Next() % SNELEMS(sMessagesShowingTrainerCase)][gender];
        break;
    case 6:
        return sMessagesBattling[LCRNG_Next() % SNELEMS(sMessagesBattling)][gender];
        break;
    case 7:
        return sMessagesTrading[LCRNG_Next() % SNELEMS(sMessagesTrading)][gender];
        break;
    case 8:
    case 1:
        return sMessagesDrawing[LCRNG_Next() % SNELEMS(sMessagesDrawing)][gender];
        break;
    case 9:
    case 2:
        return sMessagesMixingRecords[LCRNG_Next() % SNELEMS(sMessagesMixingRecords)][gender];
        break;
    case 10:
    case 3:
    case 12:
    case 13:
        return sMessagesSpinTrading[LCRNG_Next() % SNELEMS(sMessagesSpinTrading)][gender];
        break;
    }

    return UnionRoom_Text_TrainersAppearsBusy;
}

// Returns the message describing where the peer's trainer case is from,
// based on the player's and peer's country/region.
int UnionRoom_GetTrainerCasePlayerMessage(StringTemplate *strTemplate)
{
    u8 playerCountry = CommInfo_PlayerCountry(CommSys_CurNetId());
    u8 commCountry = CommInfo_PlayerCountry(CommSys_CurNetId() ^ 1);
    u8 playerRegion = CommInfo_PlayerRegion(CommSys_CurNetId());
    u8 commRegion = CommInfo_PlayerRegion(CommSys_CurNetId() ^ 1);

    if (commCountry == Country_Text_None) {
        return UnionRoom_Text_PlayersTrainerCase;
    }

    if (commCountry != 0) {
        StringTemplate_SetCountryName(strTemplate, 3, commCountry);

        if (commRegion != 0) {
            StringTemplate_SetCityName(strTemplate, 4, commCountry, commRegion);
        }
    }

    if (playerCountry != commCountry) {
        if (commRegion == 0) {
            return UnionRoom_Text_TrainerCasePlayerFromCountry;
        }

        if (playerRegion == commRegion) {
            return UnionRoom_Text_TrainerCasePlayerFromCountry;
        }

        return UnionRoom_Text_TrainerCasePlayerFromCityCountry;
    }

    if (playerRegion != commRegion) {
        return UnionRoom_Text_TrainerCasePlayerFromCity;
    }

    return UnionRoom_Text_PlayersTrainerCase;
}

// Returns the "let's start <activity>" message. Activity 0 is the trainer case
// greeting, which has its own location-aware message.
static int UnionRoom_GetStartMessage(int param0, int gender, StringTemplate *strTemplate)
{
    if (param0 != 0) {
        return sMessagesStartActivity[param0][gender];
    }

    return UnionRoom_GetTrainerCasePlayerMessage(strTemplate);
}

// Returns the Union Room message for the given message type. msgType is one of
// the UR_MSG_* constants; param1 is the target trainer's local ID (1-based).
int UnionRoom_GetMessage(UnionRoom *param0, int param1, int msgType, StringTemplate *strTemplate)
{
    param1--;

    if (msgType == 0) {
        return UnionRoom_GetTrainerBusyMessage(param0, param1);
    }

    TrainerInfo *trainerInfo = CommServerClient_GetServerTrainerInfo(param1);

    if (trainerInfo == NULL) {
        CommManager_SetErrorHandling(1, 1);
        CommManager_SetCommError(1);
        return 0;
    }

    int gender = TrainerInfo_Gender(trainerInfo);

    switch (msgType) {
    case UR_MSG_LETS_START:
        return UnionRoom_GetStartMessage(param0->selectedActivity - 1, gender, strTemplate);
        break;
    case UR_MSG_THIS_IS_PLAYER_ASK_DO_SOMETHING:
        return sMessagesThisIsPlayerAskDoSomething[gender];
        break;
    case UR_MSG_WAIT_FOR_ANSWER:
        if (param0->selectedActivity == 0) {
            return 0;
        }
        return sMessagesWaitForAnswer[param0->selectedActivity - 1][gender];
        break;
    case UR_MSG_NEED_TWO_LV_30_POKEMON_TO_BATTLE:
    case UR_MSG_CANT_TRADE_IF_ONE_POKEMON:
    case UR_MSG_NEED_EGG_TO_SPIN_TRADE:
        return sMessagesRequirements[msgType - UR_MSG_NEED_TWO_LV_30_POKEMON_TO_BATTLE][gender];
        break;
    case UR_MSG_DECLINED_GREET:
    case UR_MSG_DECLINED_DRAW:
    case UR_MSG_DECLINED_BATTLE:
    case UR_MSG_DECLINED_TRADE:
    case UR_MSG_DECLINED_UNUSED:
    case UR_MSG_DECLINED_SPIN_TRADE_MIX_RECORDS:
        return sMessagesDeclinedStartActivity[msgType - UR_MSG_DECLINED_GREET][gender];
        break;
    case UR_MSG_ASK_JOIN_DRAW:
    case UR_MSG_ASK_JOIN_MIX_RECORDS:
    case UR_MSG_ASK_JOIN_SPIN_TRADE:
        return sMessagesAskJoinActivity[msgType - UR_MSG_ASK_JOIN_DRAW][gender];
        break;
    case UR_MSG_JOINED_DRAW:
    case UR_MSG_JOINED_MIX_RECORDS:
    case UR_MSG_JOINED_SPIN_TRADE:
        return sMessagesJoinedActivity[msgType - UR_MSG_JOINED_DRAW][gender];
        break;
    case UR_MSG_DECLINED_JOIN_DRAW:
    case UR_MSG_DECLINED_JOIN_MIX_RECORDS:
    case UR_MSG_DECLINED_JOIN_SPIN_TRADE:
        return sMessagesDeclinedJoinActivity[msgType - UR_MSG_DECLINED_JOIN_DRAW][gender];
        break;
    case UR_MSG_DO_SOMETHING_ELSE:
        return sMessagesDoSomethingElse[gender];
        break;
    case UR_MSG_ASK_IF_YOU_WANT_SOMETHING:
        return sMessagesAskIfYouWantSomething[gender];
        break;
    case UR_MSG_SOMETHING_ELSE_TO_DO:
        return sMessagesSomethingElseToDo[gender];
        break;
    case UR_MSG_CANT_SPIN_TRADE_DIAMOND_PEARL:
        return UnionRoom_Text_CantSpinTradeDiamondPearl;
        break;
    case UR_MSG_CANT_SPIN_TRADE_BAD_EGG:
        return sMessagesBadEgg[gender];
        break;
    }

    GF_ASSERT(FALSE);
    return UnionRoom_Text_TrainersAppearsBusy;
}

// Returns the game code of the connected peer.
u8 UnionRoom_GetCommInfoGameCode(void)
{
    TrainerInfo *trainerInfo = CommInfo_TrainerInfo(CommSys_CurNetId() ^ 1);
    GF_ASSERT(trainerInfo != NULL);
    return TrainerInfo_GameCode(trainerInfo);
}

// Fills the broadcast game info with the trainer IDs and appearance of the
// other players (net IDs 1-4). Only the server (net ID 0) fills this in.
// param1 is unused.
static void UnionRoom_FillTrainerInfo(UnkStruct_0205B4F8 *param0, int param1)
{
    TrainerInfo *v0;
    int v1, v2 = 0;

    for (v1 = 1; v1 < 4 + 1; v1++) {
        int v3 = v1 - 1;

        v0 = CommInfo_TrainerInfo(v1);

        if (v0 != NULL) {
            if (CommSys_CurNetId() == 0) {
                param0->unk_00[v3] = TrainerInfo_ID(v0);
                param0->unk_18[v3] = TrainerInfo_Appearance(v0) | (TrainerInfo_Gender(v0) << 7);
            }
        } else {
            param0->unk_00[v3] = 0;
            param0->unk_18[v3] = 0;
        }
    }
}

// Broadcasts the local player's activity to the wireless manager so other
// players can see what we are doing. The activity codes are the values passed
// to ScrCmd_139 (0 = idle, 1 = draw, 2 = mix records, 5 = trainer case,
// 6 = battle, 7 = trade, 11 = busy, 13 = spin trade).
void UnionRoom_BroadcastActivity(int param0)
{
    UnkStruct_0205B4F8 v0;

    MI_CpuClear8(&v0, sizeof(UnkStruct_0205B4F8));

    switch (param0) {
    case 0:
        break;
    case 4:
        break;
    case 11:
        UnionRoom_FillTrainerInfo(&v0, 2);
        break;
    case 7:
    case 5:
    case 6:
        UnionRoom_FillTrainerInfo(&v0, 2);
        break;
    case 8:
        UnionRoom_FillTrainerInfo(&v0, 5);
        break;
    case 1:
        UnionRoom_FillTrainerInfo(&v0, 4);
        break;
    case 9:
        UnionRoom_FillTrainerInfo(&v0, 5);
        break;
    case 2:
        UnionRoom_FillTrainerInfo(&v0, 4);
        break;
    case 10:
    case 12:
        UnionRoom_FillTrainerInfo(&v0, 5);
        break;
    case 3:
    case 13:
        UnionRoom_FillTrainerInfo(&v0, 4);
        break;
    }

    v0.unk_1C = param0;

    CommServerClient_SetBattleRegulation(&v0);
    CommServerClient_SendGameInfo();
}

static const int sTealaMessages[] = {
    UnionRoom_Text_Teala1,
    UnionRoom_Text_Teala2,
    UnionRoom_Text_Teala3,
    UnionRoom_Text_Teala4,
    UnionRoom_Text_Teala5,
    UnionRoom_Text_Teala6,
    UnionRoom_Text_Teala7,
    UnionRoom_Text_Teala8,
    UnionRoom_Text_Teala9,
    UnionRoom_Text_Teala10,
    UnionRoom_Text_Teala11,
    UnionRoom_Text_Teala12,
    UnionRoom_Text_Teala13,
    UnionRoom_Text_Teala14,
    UnionRoom_Text_Teala15,
    UnionRoom_Text_Teala16,
    UnionRoom_Text_Teala17,
    UnionRoom_Text_Teala18,
    UnionRoom_Text_Teala19,
    UnionRoom_Text_Teala20
};

// Returns the message spoken by Teala, the Union Room guide. If another player
// is present she comments on that; otherwise she reacts to the player's easy
// chat sentence.
int UnionRoom_GetTealaMessage(UnionRoom *param0, StringTemplate *strTemplate)
{
    int v0, v1 = 0;
    u16 v3;

    for (v0 = 0; v0 < 10; v0++) {
        if (param0->bssDesc[v0] != NULL) {
            v1++;
        }
    }

    if (v1 != 0) {
        return UnionRoom_Text_HereComesSomeoneNow;
    }

    if (!EasyChatSentence_IsValid(&param0->easyChatSentence)) {
        return UnionRoom_Text_BoringIfNoOneComes;
    }

    if (EasyChatSentence_GetType(&param0->easyChatSentence) != 4) {
        int appearance = TrainerInfo_Appearance(param0->trainerInfo);
        int gender = TrainerInfo_Gender(param0->trainerInfo);

        StringTemplate_SetTrainerClassName(strTemplate, 0, Appearance_GetData(gender, appearance, APPEARANCE_DATA_TRAINER_CLASS_1));

        return UnionRoom_Text_MistakenForTrainerClass;
    }

    int id = EasyChatSentence_GetID(&param0->easyChatSentence);

    if (id >= 20) {
        id = 0;
    }

    if ((v3 = EasyChatSentence_GetWord(&param0->easyChatSentence, 0)) != WORD_NONE) {
        StringTemplate_SetEasyChatWord(strTemplate, 0, v3);
    }

    return sTealaMessages[id];
}

// Clears the broadcast game info.
static void UnionRoom_ResetGameInfo(UnkStruct_0205B4F8 *param0)
{
    int v0;

    param0->unk_1C = 0;

    for (v0 = 0; v0 < 4; v0++) {
        param0->unk_00[v0] = 0;
        param0->unk_18[v0] = 0;
        param0->unk_10[v0] = 0;
        param0->unk_14[v0] = 0;
    }
}

// Stores an easy chat sentence to be broadcast later.
void UnionRoom_SetEasyChatSentence(UnionRoom *param0, EasyChatSentence *param1)
{
    EasyChatSentence_Copy(&param0->easyChatSentence, param1);
    param0->hasEasyChatSentence = 1;
}

// Returns the pending easy chat sentence and clears the pending flag, or NULL
// if there is none.
EasyChatSentence *UnionRoom_TakeEasyChatSentence(UnionRoom *param0)
{
    if (param0->hasEasyChatSentence == 0) {
        return NULL;
    }

    param0->hasEasyChatSentence = 0;
    return &param0->easyChatSentence;
}

// Sets up the greeting message for the peer, choosing the greeting text from
// the peer's language and unlocking the matching easy chat greeting word.
// param1 selects the peer: 0 = trainer at local ID param2, 1 = the connected
// peer.
void UnionRoom_DoGreeting(StringTemplate *strTemplate, int param1, int param2, TrainerInfo *playerTrainerInfo, UnlockedEasyChatWords *unlockedWords)
{
    TrainerInfo *commTrainerInfo;
    MessageLoader *msgLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNION_ROOM, HEAP_ID_FIELD1);
    int entryID;

    param2--;

    if (param1 == 0) {
        commTrainerInfo = CommServerClient_GetServerTrainerInfo(param2);
    } else {
        commTrainerInfo = CommInfo_TrainerInfo(CommSys_CurNetId() ^ 1);
    }

    if (commTrainerInfo == NULL) {
        MessageLoader_Free(msgLoader);
        return;
    }

    StringTemplate_SetPlayerName(strTemplate, 0, commTrainerInfo);
    StringTemplate_SetPlayerName(strTemplate, 1, playerTrainerInfo);

    int language = TrainerInfo_Language(commTrainerInfo);

    if (language >= LANGUAGE_JAPANESE && language <= LANGUAGE_SPANISH) {
        static const int greetingBankEntries[] = {
            [LANGUAGE_JAPANESE - 1] = Greetings_Text_Konnichiwa,
            [LANGUAGE_ENGLISH - 1] = Greetings_Text_Hello,
            [LANGUAGE_FRENCH - 1] = Greetings_Text_Bonjour,
            [LANGUAGE_ITALIAN - 1] = Greetings_Text_Ciao,
            [LANGUAGE_GERMAN - 1] = Greetings_Text_Hallo,
            [LANGUAGE_UNUSED_6 - 1] = -1,
            [LANGUAGE_SPANISH - 1] = Greetings_Text_Hola,
        };
        u16 index = language - 1;

        if (index < NELEMS(greetingBankEntries) && greetingBankEntries[index] >= 0) {
            EasyChatWords_UnlockGreeting(unlockedWords, greetingBankEntries[index]);
        }
    }

    switch (language) {
    case LANGUAGE_JAPANESE:
        entryID = UnionRoom_Text_GreetingJapanese;
        break;
    case LANGUAGE_ENGLISH:
        entryID = UnionRoom_Text_GreetingEnglish;
        break;
    case LANGUAGE_FRENCH:
        entryID = UnionRoom_Text_GreetingFrench;
        break;
    case LANGUAGE_ITALIAN:
        entryID = UnionRoom_Text_GreetingItalian;
        break;
    case LANGUAGE_GERMAN:
        entryID = UnionRoom_Text_GreetingGerman;
        break;
    case LANGUAGE_SPANISH:
        entryID = UnionRoom_Text_GreetingSpanish;
        break;
    default:
        entryID = UnionRoom_Text_GreetingDefault;
    }

    String *string = MessageLoader_GetNewString(msgLoader, entryID);

    StringTemplate_SetString(strTemplate, 2, string, 0, TRUE, language);
    Heap_Free(string);
    MessageLoader_Free(msgLoader);
}

// Publishes the player's easy chat sentence and clears the broadcast game info.
void UnionRoom_InitGameInfo(EasyChatSentence *param0)
{
    UnkStruct_0205B4F8 v0;

    UnionRoom_ResetGameInfo(&v0);
    v0.unk_1C = 0;

    CommServerClient_SetEasyChatSentence(param0);
    CommServerClient_SetBattleRegulation(&v0);
    CommServerClient_SendGameInfo();
}

// Clears the local activity handshake state.
void UnionRoom_ResetActivity(UnionRoom *param0)
{
    param0->activity = 0;
    param0->peerActivity = 0;
    param0->peerNoActivity = 0;
}

// Clears the connection and activity handshake state.
static void UnionRoom_ResetState(UnionRoom *param0)
{
    param0->pendingRequest = 0;
    param0->unk_24 = 0;
    param0->unk_2C = 0;
    param0->activity = 0;
    param0->peerActivity = 0;
    param0->unk_174 = 0;
    param0->hasEasyChatSentence = 0;
    param0->peerNoActivity = 0;
}

// Allocates the trainer cases used to exchange trainer cards with the peer and
// returns the buffer for the peer's case.
void *UnionRoom_GetTrainerCase(UnionRoom *param0)
{
    param0->trainerCase = TrainerCase_New(HEAP_ID_SYSTEM);
    param0->trainerCases[0] = TrainerCase_New(HEAP_ID_SYSTEM);
    param0->trainerCases[1] = TrainerCase_New(HEAP_ID_SYSTEM);

    TrainerCase_Init(FALSE, FALSE, 0, Appearance_GetData(TrainerInfo_Gender(param0->trainerInfo), TrainerInfo_Appearance(param0->trainerInfo), 0), param0->fieldSystem, param0->trainerCase);

    return (void *)param0->trainerCases[CommSys_CurNetId() ^ 1];
}

// Frees the trainer cases allocated by UnionRoom_GetTrainerCase.
void UnionRoom_FreeTrainerCase(UnionRoom *param0)
{
    Heap_Free(param0->trainerCases[0]);
    Heap_Free(param0->trainerCases[1]);
    Heap_Free(param0->trainerCase);
}

// Sends the local player's trainer case to the peer on command 105.
void UnionRoom_SendTrainerCase(UnionRoom *param0)
{
    CommSys_SendDataHuge(105, param0->trainerCase, sizeof(TrainerCase));
}
