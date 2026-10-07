#include "contest_comm.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"
#include "struct_defs/struct_02029C88.h"
#include "struct_defs/struct_02095B28.h"
#include "struct_defs/struct_02095C60.h"

#include "overlay006/ov6_022489E4.h"
#include "overlay017/ov17_02252CEC.h"
#include "overlay017/struct_ov17_0224EDE0.h"

#include "communication_system.h"
#include "contest.h"
#include "heap.h"
#include "image_clips.h"
#include "comm_cmd.h"

// Contest communication. This module registers the contest's communication
// commands and implements the two exchanges that happen over the link cable:
// the contest photos taken after the visual competition, and the visual
// competition scoring sync. The command manager indexes the table with
// (cmd - 22), so entry 0 handles command 22 and entry 15 handles command 37.

static void ContestComm_RecvNoOp(int netId, int size, void *data, void *commState);
static u8 *ContestComm_GetRecvBuffer(int netId, void *commState, int size);
static void ContestComm_RecvPhoto(int netId, int size, void *data, void *commState);
static void ContestComm_RecvPhotoSet(int netId, int size, void *data, void *commState);
static void ContestComm_RecvScore(int netId, int size, void *data, void *commState);
static void ContestComm_RecvFinished(int netId, int size, void *data, void *commState);
static int ContestComm_DancePacketSize(void);
static int ContestComm_ScoringPacketSize(void);

// Commands 22-37. Entries 0-9 and 14-15 are handled by the competition apps
// (overlays 006 and 017); the contest-specific photo and scoring commands are
// entries 10-13, i.e. commands 32-35.
static const CommCmdTable sContestCommCmdTable[] = {
    { NULL, CommPacketSizeOf_Nothing, NULL }, // 22
    { ContestComm_RecvNoOp, CommPacketSizeOf_Nothing, NULL }, // 23
    { ov17_02252CEC, CommPacketSizeOf_Variable, ContestComm_GetRecvBuffer }, // 24: dance data
    { ov17_02252D7C, ContestComm_DancePacketSize, NULL }, // 25: dance packet
    { ov6_022489E4, CommPacketSizeOf_Variable, NULL }, // 26
    { ov6_02248AC8, CommPacketSizeOf_Variable, NULL }, // 27
    { ov6_02248B30, CommPacketSizeOf_Variable, ContestComm_GetRecvBuffer }, // 28
    { ov6_02248BC0, CommPacketSizeOf_Variable, NULL }, // 29
    { ov6_02248C28, CommPacketSizeOf_Variable, NULL }, // 30
    { ov6_02248CBC, CommPacketSizeOf_Variable, NULL }, // 31
    { ContestComm_RecvPhoto, CommPacketSizeOf_Variable, NULL }, // 32: single photo
    { ContestComm_RecvPhotoSet, CommPacketSizeOf_Variable, ContestComm_GetRecvBuffer }, // 33: all photos
    { ContestComm_RecvScore, ContestComm_ScoringPacketSize, NULL }, // 34: score value
    { ContestComm_RecvFinished, ContestComm_ScoringPacketSize, NULL }, // 35: finished flag
    { ov6_02248D38, CommPacketSizeOf_Variable, ContestComm_GetRecvBuffer }, // 36
    { ov6_02248DA0, CommPacketSizeOf_Variable, NULL } // 37
};

// Registers the contest command table with the command manager, using the
// Contest as the context passed to every handler.
void ContestComm_Init(void *commState)
{
    int cmdCount = sizeof(sContestCommCmdTable) / sizeof(CommCmdTable);
    CommCmd_Init(sContestCommCmdTable, cmdCount, commState);
}

// Returns the per-player receive buffer for a variable-length command. The
// command manager calls this to obtain the destination for incoming data.
static u8 *ContestComm_GetRecvBuffer(int netId, void *commState, int size)
{
    Contest *contest = commState;

    GF_ASSERT(size < 1024);
    return contest->commRecvBuf[netId];
}

// Packet-size callbacks for the command table.

static int ContestComm_DancePacketSize(void)
{
    return sizeof(UnkStruct_ov17_0224EDE0);
}

static int ContestComm_ScoringPacketSize(void)
{
    return sizeof(ContestCommValue);
}

// Command 23 is registered but unused.
static void ContestComm_RecvNoOp(int netId, int size, void *data, void *commState)
{
    return;
}

// Command 32: receives one contestant's photo. The sender appends its
// contestant ID after the photo data so the receiver knows which slot to fill.
static void ContestComm_RecvPhoto(int netId, int size, void *data, void *commState)
{
    Contest *contest = commState;
    int photoSize;
    int contestantID;
    u8 *packet;

    photoSize = ContestPhoto_Size();
    packet = data;
    contestantID = packet[photoSize];

    MI_CpuCopy8(data, contest->data.photos[contestantID], photoSize);

    contest->commRecvCount++;
}

// Command 32: sends one contestant's photo, appending the contestant ID.
BOOL ContestComm_SendPhoto(Contest *contest, int contestantID, const ContestPhoto *photo)
{
    u8 *packet;
    int photoSize;
    int success;

    photoSize = ContestPhoto_Size();
    packet = Heap_Alloc(HEAP_ID_20, photoSize + 1);
    MI_CpuCopy8(photo, packet, photoSize);
    packet[photoSize] = contestantID;

    if (CommSys_SendData(32, packet, photoSize + 1) == 1) {
        success = 1;
    } else {
        success = 0;
    }

    Heap_Free(packet);
    return success;
}

// Command 33: receives all four contestants' photos in one packet, broadcast
// by the leader.
static void ContestComm_RecvPhotoSet(int netId, int size, void *data, void *commState)
{
    Contest *contest = commState;
    int photoSize, setSize;
    u8 *packet;
    int i;

    photoSize = ContestPhoto_Size();
    setSize = photoSize * 4;
    packet = data;

    GF_ASSERT(setSize < 1024);

    for (i = 0; i < 4; i++) {
        MI_CpuCopy8(&packet[photoSize * i], contest->data.photos[i], photoSize);
    }

    contest->commRecvCount++;
}

// Command 33: the leader packs all four photos into the large send buffer and
// broadcasts them at once.
BOOL ContestComm_SendPhotoSet(Contest *contest, ContestPhoto **photos)
{
    u8 *packet;
    int photoSize, setSize;
    int success;
    int i;

    photoSize = ContestPhoto_Size();
    setSize = photoSize * 4;

    GF_ASSERT(setSize < 1024);

    packet = contest->commSendBuf;

    for (i = 0; i < 4; i++) {
        MI_CpuCopy8(photos[i], &packet[photoSize * i], photoSize);
    }

    if (CommSys_SendDataHuge(33, packet, setSize) == 1) {
        success = 1;
    } else {
        success = 0;
    }

    return success;
}

// Command 34: receives the leader's score value into the local score.
static void ContestComm_RecvScore(int netId, int size, void *data, void *commState)
{
    Contest *contest = commState;
    MI_CpuCopy8(data, &contest->scoringCommState.localValue, size);
}

// Command 34: publishes the visual competition score. Outside a link contest
// the value is stored locally; in a link contest only the leader broadcasts it.
BOOL ContestComm_SendScore(ContestScoringCommState *scoringState, u32 value)
{
    if (scoringState->isLinkContest == 0) {
        scoringState->localValue.value = value;
        return 1;
    }

    if (scoringState->leaderContestantID != scoringState->netID) {
        return 0;
    }

    scoringState->sendValue.value = value;

    if (CommSys_SendData(34, &scoringState->sendValue, sizeof(ContestCommValue)) == 1) {
        return 1;
    }

    return 0;
}

// Command 35: records that the contestant with the given net ID has finished
// scoring.
static void ContestComm_RecvFinished(int netId, int size, void *data, void *commState)
{
    Contest *contest = commState;
    ContestCommValue *value = data;
    contest->scoringCommState.receivedValues[netId] = value->finished;
}

// Command 35: publishes this contestant's finished flag. Outside a link contest
// it is stored locally; in a link contest it is broadcast to the others.
BOOL ContestComm_SendFinished(ContestScoringCommState *scoringState, int finished)
{
    if (scoringState->isLinkContest == 0) {
        scoringState->receivedValues[0] = finished;
        return 1;
    }

    scoringState->sendValue.finished = finished;

    if (CommSys_SendData(35, &scoringState->sendValue, sizeof(ContestCommValue)) == 1) {
        return 1;
    }

    return 0;
}
