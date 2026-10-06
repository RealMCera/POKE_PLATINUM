#include "link_battle_comm.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle.h"

#include "struct_decls/battle_system.h"
#include "struct_defs/chatot_cry.h"
#include "struct_defs/comm_cmd_table.h"
#include "struct_defs/link_battle_comm_state.h"
#include "struct_defs/struct_0207ACB4.h"
#include "struct_defs/struct_0207AD40.h"
#include "struct_defs/trainer.h"

#include "battle/battle_controller.h"
#include "battle/battle_system.h"
#include "battle/message_defs.h"

#include "charcode_util.h"
#include "chatot_cry.h"
#include "communication_system.h"
#include "heap.h"
#include "pal_pad.h"
#include "party.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "trainer_info.h"
#include "battle_recording.h"
#include "unk_02032798.h"
#include "unk_020363E8.h"

// Link battle communication. When a link battle starts, this module registers
// the battle-specific communication commands and starts two SysTasks: a server
// sender that drains the battle system's server message queue onto the wire,
// and a client receiver that feeds incoming server messages back into the
// battle controller. It also implements the link battle handshake, which
// exchanges each player's system version, trainer info, Trainer data, party,
// Chatot cries and Pal Pad before the battle proper begins.

void LinkBattleComm_QueueServerMessage(BattleSystem *battleSys, int recipient, int battler, void *message, u8 size);
BOOL LinkBattleComm_SendSystemVersion(LinkBattleCommState *linkBattleCommState, u32 version);
BOOL LinkBattleComm_PrepareTrainerInfo(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_SendTrainerInfo(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PrepareTrainer(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_SendTrainer(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PrepareParty(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_SendParty(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PrepareChatotCry(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_SendChatotCry(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PrepareTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot);
BOOL LinkBattleComm_SendTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState);
BOOL LinkBattleComm_PreparePartySlot(LinkBattleCommState *linkBattleCommState, int slot);
BOOL LinkBattleComm_SendPartySlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState);
void LinkBattleComm_InitCommands(void *commState);
BOOL LinkBattleComm_SendPalPad(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PreparePalPad(LinkBattleCommState *linkBattleCommState);
static int LinkBattleComm_SystemVersionSize(void);
static int LinkBattleComm_TrainerInfoSize(void);
static int LinkBattleComm_PartySize(void);
static int LinkBattleComm_ChatotCrySize(void);
static int LinkBattleComm_TrainerSize(void);
static int LinkBattleComm_PalPadSize(void);
static u8 *LinkBattleComm_GetTrainerInfo(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetTrainer(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetParty(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetChatotCry(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetTrainerSlot1(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetTrainerSlot3(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetPartySlot1(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetPartySlot3(int battler, void *commState, int unused);
static u8 *LinkBattleComm_GetPalPad(int battler, void *commState, int unused);
static void LinkBattleComm_RecvServerMessage(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvSystemVersion(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvTrainerInfo(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvTrainer(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvParty(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvChatotCry(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvTrainerSlot(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvPartySlot(int netId, int size, void *data, void *commState);
static void LinkBattleComm_RecvEndWait(int netId, int size, void *data, void *commState);
static void LinkBattleComm_ServerSenderTask(SysTask *task, void *taskData);
static void LinkBattleComm_ClientReceiverTask(SysTask *task, void *taskData);
static void LinkBattleComm_RecvPalPad(int netId, int size, void *data, void *commState);
static void PalPad_CreateNetworkObject(TrainerInfo *trainerInfo, PalPad *source, PalPad *destination);

// Commands 22-33. The command manager indexes this table with (cmd - 22), so
// entry 0 handles command 22 and entry 11 handles command 33. Each entry pairs
// a receive handler with a packet-size callback and an optional data getter
// used when sending.
static const CommCmdTable sLinkBattleCommCmdTable[] = {
    { LinkBattleComm_RecvEndWait, CommPacketSizeOf_Variable, NULL }, // 22: end-of-battle wait
    { LinkBattleComm_RecvServerMessage, CommPacketSizeOf_Variable, NULL }, // 23: server message
    { LinkBattleComm_RecvSystemVersion, LinkBattleComm_SystemVersionSize, NULL }, // 24: system version
    { LinkBattleComm_RecvTrainerInfo, LinkBattleComm_TrainerInfoSize, LinkBattleComm_GetTrainerInfo }, // 25: trainer info
    { LinkBattleComm_RecvTrainer, LinkBattleComm_TrainerSize, LinkBattleComm_GetTrainer }, // 26: Trainer data
    { LinkBattleComm_RecvParty, LinkBattleComm_PartySize, LinkBattleComm_GetParty }, // 27: party
    { LinkBattleComm_RecvChatotCry, LinkBattleComm_ChatotCrySize, LinkBattleComm_GetChatotCry }, // 28: Chatot cry
    { LinkBattleComm_RecvTrainerSlot, LinkBattleComm_TrainerSize, LinkBattleComm_GetTrainerSlot1 }, // 29: Trainer slot 1
    { LinkBattleComm_RecvTrainerSlot, LinkBattleComm_TrainerSize, LinkBattleComm_GetTrainerSlot3 }, // 30: Trainer slot 3
    { LinkBattleComm_RecvPartySlot, LinkBattleComm_PartySize, LinkBattleComm_GetPartySlot1 }, // 31: party slot 1
    { LinkBattleComm_RecvPartySlot, LinkBattleComm_PartySize, LinkBattleComm_GetPartySlot3 }, // 32: party slot 3
    { LinkBattleComm_RecvPalPad, LinkBattleComm_PalPadSize, LinkBattleComm_GetPalPad } // 33: Pal Pad
};

// Starts the link battle communication machinery: registers the command table,
// allocates the sender/receiver task state, hands the battle system pointers to
// those states, and starts the two SysTasks.
void LinkBattleComm_Init(void *battleSysPtr)
{
    int v0 = sizeof(sLinkBattleCommCmdTable) / sizeof(CommCmdTable);
    BattleSystem *battleSys;
    LinkBattleCommSender *sender;
    LinkBattleCommReceiver *receiver;

    battleSys = (BattleSystem *)battleSysPtr;

    if (BattleSystem_GetBattleStatusMask(battleSys) & 0x10) {
        return;
    }

    sender = (LinkBattleCommSender *)Heap_Alloc(HEAP_ID_BATTLE, sizeof(LinkBattleCommSender));
    receiver = (LinkBattleCommReceiver *)Heap_Alloc(HEAP_ID_BATTLE, sizeof(LinkBattleCommReceiver));

    CommCmd_Init(sLinkBattleCommCmdTable, v0, battleSysPtr);

    sender->battleSys = battleSys;
    sender->state = 0;
    receiver->battleSys = battleSys;
    receiver->state = 0;

    // The battle system owns the task state bytes so the end-wait command can
    // signal the tasks to stop.
    BattleSystem_SetLinkServerSenderStates(battleSys, &sender->state);
    BattleSystem_SetLinkClientReceiverStates(battleSys, &receiver->state);

    SysTask_Start(LinkBattleComm_ServerSenderTask, sender, 0);
    SysTask_Start(LinkBattleComm_ClientReceiverTask, receiver, 0);
}

// Registers the link battle command table without starting the sender/receiver
// tasks. Used by the link communication screen before the battle begins.
void LinkBattleComm_InitCommands(void *commState)
{
    int v0 = sizeof(sLinkBattleCommCmdTable) / sizeof(CommCmdTable);
    CommCmd_Init(sLinkBattleCommCmdTable, v0, commState);
}

// Packet-size callbacks for the command table.

static int LinkBattleComm_SystemVersionSize(void)
{
    return 4;
}

static int LinkBattleComm_TrainerInfoSize(void)
{
    return TrainerInfo_Size();
}

static int LinkBattleComm_PartySize(void)
{
    return Party_SaveSize();
}

static int LinkBattleComm_ChatotCrySize(void)
{
    return 1000;
}

static int LinkBattleComm_TrainerSize(void)
{
    return sizeof(Trainer);
}

// Data getters for the command table. In a Frontier battle each battler owns
// two slots (a Trainer and its partner), so the battler index is doubled.
static u8 *LinkBattleComm_GetTrainerInfo(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;

    if (linkBattleCommState->dto->battleType & BATTLE_TYPE_FRONTIER) {
        return (u8 *)linkBattleCommState->dto->trainerInfo[battler * 2];
    } else {
        return (u8 *)linkBattleCommState->dto->trainerInfo[battler];
    }
}

static u8 *LinkBattleComm_GetTrainer(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;

    if (linkBattleCommState->dto->battleType & BATTLE_TYPE_FRONTIER) {
        return (u8 *)&linkBattleCommState->dto->trainer[battler * 2];
    } else {
        return (u8 *)&linkBattleCommState->dto->trainer[battler];
    }
}

static u8 *LinkBattleComm_GetParty(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;

    if (linkBattleCommState->dto->battleType & BATTLE_TYPE_FRONTIER) {
        return (u8 *)linkBattleCommState->dto->parties[battler * 2];
    } else {
        return (u8 *)linkBattleCommState->dto->parties[battler];
    }
}

static u8 *LinkBattleComm_GetChatotCry(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;

    if (linkBattleCommState->dto->battleType & BATTLE_TYPE_FRONTIER) {
        return (u8 *)linkBattleCommState->dto->chatotCries[battler * 2];
    } else {
        return (u8 *)linkBattleCommState->dto->chatotCries[battler];
    }
}

// Fixed-slot getters used by the Frontier handshake, which sends the partner
// Trainer/party from slots 1 and 3.
static u8 *LinkBattleComm_GetTrainerSlot1(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;
    return (u8 *)&linkBattleCommState->dto->trainer[1];
}

static u8 *LinkBattleComm_GetTrainerSlot3(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;
    return (u8 *)&linkBattleCommState->dto->trainer[3];
}

static u8 *LinkBattleComm_GetPartySlot1(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;
    return (u8 *)linkBattleCommState->dto->parties[1];
}

static u8 *LinkBattleComm_GetPartySlot3(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;
    return (u8 *)linkBattleCommState->dto->parties[3];
}

static u8 *LinkBattleComm_GetPalPad(int battler, void *commState, int unused)
{
    LinkBattleCommState *linkBattleCommState = commState;
    return (u8 *)linkBattleCommState->palPad[battler];
}

// Appends a battle message to the server message queue. The message is stored
// as a BattleMessageInfo header followed by the payload; the server sender task
// later transmits the whole record as command 23.
void LinkBattleComm_QueueServerMessage(BattleSystem *battleSys, int recipient, int battler, void *message, u8 size)
{
    int i;
    BattleMessageInfo *info;
    u8 *src;
    u8 *messageBuffer;
    u16 *writeIndex;
    u16 *endIndex;

    info = (BattleMessageInfo *)Heap_Alloc(HEAP_ID_BATTLE, sizeof(BattleMessageInfo));
    messageBuffer = BattleSystem_GetServerMessage(battleSys);
    writeIndex = BattleSystem_GetServerWriteIndex(battleSys);
    endIndex = BattleSystem_GetServerEndIndex(battleSys);

    // If the record would run past the end of the 0x1000-byte ring, mark the
    // current write position as the end and wrap back to the start.
    if (writeIndex[0] + sizeof(BattleMessageInfo) + size + 1 > 0x1000) {
        endIndex[0] = writeIndex[0];
        writeIndex[0] = 0;
    }

    info->recipient = recipient;
    info->battler = battler;
    info->size = size;

    src = (u8 *)info;

    for (i = 0; i < sizeof(BattleMessageInfo); i++) {
        messageBuffer[writeIndex[0]] = src[i];
        writeIndex[0]++;
    }

    src = (u8 *)message;

    for (i = 0; i < size; i++) {
        messageBuffer[writeIndex[0]] = src[i];
        writeIndex[0]++;
    }

    Heap_Free(info);
}

// Command 23 handler: a server message arrived, so append it to the client
// message queue for the client receiver task to hand to the battle controller.
static void LinkBattleComm_RecvServerMessage(int netId, int size, void *data, void *commState)
{
    BattleSystem *battleSys = (BattleSystem *)commState;
    int i;
    u8 *src = (u8 *)data;
    u8 *messageBuffer = BattleSystem_GetClientMessage(battleSys);
    u16 *writeIndex = BattleSystem_GetClientWriteIndex(battleSys);
    u16 *endIndex = BattleSystem_GetClientEndIndex(battleSys);

    if (writeIndex[0] + size + 1 > 0x1000) {
        endIndex[0] = writeIndex[0];
        writeIndex[0] = 0;
    }

    for (i = 0; i < size; i++) {
        messageBuffer[writeIndex[0]] = src[i];
        writeIndex[0]++;
    }
}

// Command 24: send the local system version once the peers are synced.
BOOL LinkBattleComm_SendSystemVersion(LinkBattleCommState *linkBattleCommState, u32 version)
{
    Party *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(51) == 0) {
        return 0;
    }

    return CommSys_SendData(24, (void *)&version, 4);
}

// Command 24 handler: record the peer's system version and forward it to the
// battle recording state.
static void LinkBattleComm_RecvSystemVersion(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;

    linkBattleCommState->dto->systemVersion[netId] = *((u32 *)data);
    BattleRecording_SetSystemVersion(netId, linkBattleCommState->dto->systemVersion[netId]);
    linkBattleCommState->recvCount++;
}

// Command 25: copy the local trainer info into the send buffer, then transmit
// it once the peers are synced.
BOOL LinkBattleComm_PrepareTrainerInfo(LinkBattleCommState *linkBattleCommState)
{
    TrainerInfo *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (TrainerInfo *)&linkBattleCommState->sendBuffer[0];
    TrainerInfo_Copy(linkBattleCommState->dto->trainerInfo[0], v0);

    return 1;
}

BOOL LinkBattleComm_SendTrainerInfo(LinkBattleCommState *linkBattleCommState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(52) == 0) {
        return 0;
    }

    return CommSys_SendDataHuge(25, (void *)&linkBattleCommState->sendBuffer[0], TrainerInfo_Size());
}

static void LinkBattleComm_RecvTrainerInfo(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;
    linkBattleCommState->recvCount++;
}

// Command 26: copy the local Trainer data into the send buffer, then transmit
// it once the peers are synced.
BOOL LinkBattleComm_PrepareTrainer(LinkBattleCommState *linkBattleCommState)
{
    Trainer *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (Trainer *)&linkBattleCommState->sendBuffer[0];
    *v0 = linkBattleCommState->dto->trainer[0];

    return 1;
}

BOOL LinkBattleComm_SendTrainer(LinkBattleCommState *linkBattleCommState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(53) == 0) {
        return 0;
    }

    return CommSys_SendDataHuge(26, (void *)&linkBattleCommState->sendBuffer[0], sizeof(Trainer));
}

static void LinkBattleComm_RecvTrainer(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;
    linkBattleCommState->recvCount++;
}

// Command 27: copy the local party into the send buffer, then transmit it once
// the peers are synced.
BOOL LinkBattleComm_PrepareParty(LinkBattleCommState *linkBattleCommState)
{
    Party *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (Party *)&linkBattleCommState->sendBuffer[0];
    Party_Copy(linkBattleCommState->dto->parties[0], v0);

    return 1;
}

BOOL LinkBattleComm_SendParty(LinkBattleCommState *linkBattleCommState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(54) == 0) {
        return 0;
    }

    return CommSys_SendDataHuge(27, (void *)&linkBattleCommState->sendBuffer[0], Party_SaveSize());
}

static void LinkBattleComm_RecvParty(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;

    linkBattleCommState->recvCount++;
}

// Command 28: copy the local Chatot cry into the send buffer, then transmit it
// once the peers are synced.
BOOL LinkBattleComm_PrepareChatotCry(LinkBattleCommState *linkBattleCommState)
{
    ChatotCry *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (ChatotCry *)&linkBattleCommState->sendBuffer[0];
    ChatotCry_Copy(v0, linkBattleCommState->dto->chatotCries[0]);

    return 1;
}

BOOL LinkBattleComm_SendChatotCry(LinkBattleCommState *linkBattleCommState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(55) == 0) {
        return 0;
    }

    return CommSys_SendDataHuge(28, (void *)&linkBattleCommState->sendBuffer[0], 1000);
}

// Command 33: build the local Pal Pad network object into the send buffer and
// allocate the four Pal Pads that will hold the peers' data.
BOOL LinkBattleComm_PreparePalPad(LinkBattleCommState *linkBattleCommState)
{
    PalPad *v0;
    TrainerInfo *v1;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (PalPad *)&linkBattleCommState->sendBuffer[0];

    if (linkBattleCommState->dto->battleType & BATTLE_TYPE_FRONTIER) {
        v1 = linkBattleCommState->dto->trainerInfo[CommSys_CurNetId() * 2];
    } else {
        v1 = linkBattleCommState->dto->trainerInfo[CommSys_CurNetId()];
    }

    PalPad_CreateNetworkObject(v1, linkBattleCommState->dto->palPad, (PalPad *)linkBattleCommState->sendBuffer);

    {
        int v2;

        for (v2 = 0; v2 < 4; v2++) { // 4 pal pads
            linkBattleCommState->palPad[v2] = Heap_Alloc(HEAP_ID_BATTLE, 136);
        }
    }

    return 1;
}

BOOL LinkBattleComm_SendPalPad(LinkBattleCommState *linkBattleCommState) // SEND pal pad data?!
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(56) == 0) {
        return 0;
    }

    return CommSys_SendDataHuge(33, (void *)linkBattleCommState->sendBuffer, 1000);
}

static void LinkBattleComm_RecvChatotCry(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;
    linkBattleCommState->recvCount++;
}

// Commands 29/30: copy the partner Trainer from the given slot into the send
// buffer, then transmit it once the peers are synced.
BOOL LinkBattleComm_PrepareTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot)
{
    Trainer *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (Trainer *)&linkBattleCommState->sendBuffer[0];
    *v0 = linkBattleCommState->dto->trainer[slot];

    return 1;
}

BOOL LinkBattleComm_SendTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(syncState) == 0) {
        return 0;
    }

    if (slot == 1) {
        return CommSys_SendDataHuge(29, (void *)&linkBattleCommState->sendBuffer[0], sizeof(Trainer));
    } else {
        return CommSys_SendDataHuge(30, (void *)&linkBattleCommState->sendBuffer[0], sizeof(Trainer));
    }
}

static void LinkBattleComm_RecvTrainerSlot(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;
    linkBattleCommState->recvCount++;
}

// Commands 31/32: copy the partner party from the given slot into the send
// buffer, then transmit it once the peers are synced.
BOOL LinkBattleComm_PreparePartySlot(LinkBattleCommState *linkBattleCommState, int slot)
{
    Party *v0;

    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    v0 = (Party *)&linkBattleCommState->sendBuffer[0];
    Party_Copy(linkBattleCommState->dto->parties[slot], v0);

    return 1;
}

BOOL LinkBattleComm_SendPartySlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState)
{
    if (CommSys_SendRingRemainingSize() != 264) {
        return 0;
    }

    if (CommTiming_IsSyncState(syncState) == 0) {
        return 0;
    }

    if (slot == 1) {
        return CommSys_SendDataHuge(31, (void *)&linkBattleCommState->sendBuffer[0], Party_SaveSize());
    } else {
        return CommSys_SendDataHuge(32, (void *)&linkBattleCommState->sendBuffer[0], Party_SaveSize());
    }
}

static void LinkBattleComm_RecvPartySlot(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;
    linkBattleCommState->recvCount++;
}

// Server sender task: while the send ring has room, transmit the next queued
// server message as command 23 and advance the read index. A state of 255
// (set by the end-wait command) frees the task.
void LinkBattleComm_ServerSenderTask(SysTask *task, void *taskData)
{
    LinkBattleCommSender *sender = (LinkBattleCommSender *)taskData;
    u8 *messageBuffer;
    u16 *readIndex;
    u16 *writeIndex;
    u16 *endIndex;
    int messageSize;

    messageBuffer = BattleSystem_GetServerMessage(sender->battleSys);
    readIndex = BattleSystem_GetServerReadIndex(sender->battleSys);
    writeIndex = BattleSystem_GetServerWriteIndex(sender->battleSys);
    endIndex = BattleSystem_GetServerEndIndex(sender->battleSys);

    switch (sender->state) {
    case 0:
        if (CommSys_SendRingRemainingSize() != 264) {
            break;
        }

        if (readIndex[0] == writeIndex[0]) {
            break;
        }

        if (readIndex[0] == endIndex[0]) {
            readIndex[0] = 0;
            endIndex[0] = 0;
        }

        // The record size is the header plus the payload size stored in the
        // header's size field.
        messageSize = sizeof(BattleMessageInfo) + (messageBuffer[readIndex[0] + 2] | (messageBuffer[readIndex[0] + 3] << 8));

        if (CommSys_SendData(23, (void *)&messageBuffer[readIndex[0]], messageSize) == 1) {
            readIndex[0] += messageSize;
        }
        break;
    default:
    case 255:
        Heap_Free(taskData);
        SysTask_Done(task);
        break;
    }
}

// Client receiver task: while a complete server message is queued, hand it to
// the battle controller and advance the read index. A state of 255 (set by the
// end-wait command) frees the task.
void LinkBattleComm_ClientReceiverTask(SysTask *task, void *taskData)
{
    LinkBattleCommReceiver *receiver = (LinkBattleCommReceiver *)taskData;
    u8 *messageBuffer;
    u16 *readIndex;
    u16 *writeIndex;
    u16 *endIndex;
    int messageSize;

    messageBuffer = BattleSystem_GetClientMessage(receiver->battleSys);
    readIndex = BattleSystem_GetClientReadIndex(receiver->battleSys);
    writeIndex = BattleSystem_GetClientWriteIndex(receiver->battleSys);
    endIndex = BattleSystem_GetClientEndIndex(receiver->battleSys);

    switch (receiver->state) {
    case 0:
        if (readIndex[0] == writeIndex[0]) {
            break;
        }

        if (readIndex[0] == endIndex[0]) {
            readIndex[0] = 0;
            endIndex[0] = 0;
        }

        if (BattleController_RecvCommMessage(receiver->battleSys, (void *)&messageBuffer[readIndex[0]]) == 1) {
            messageSize = sizeof(BattleMessageInfo) + (messageBuffer[readIndex[0] + 2] | (messageBuffer[readIndex[0] + 3] << 8));
            readIndex[0] += messageSize;
        }
        break;
    default:
    case 255:
        Heap_Free(taskData);
        SysTask_Done(task);
        break;
    }
}

// Command 22 handler: the battle is ending, so tell both tasks to stop and mark
// the command as an end wait.
static void LinkBattleComm_RecvEndWait(int netId, int size, void *data, void *commState)
{
    BattleSystem *battleSys = (BattleSystem *)commState;

    BattleSystem_SetLinkServerSenderState(battleSys, 255);
    BattleSystem_SetLinkClientReceiverState(battleSys, 255);
    BattleSystem_SetCommandIsEndWait(battleSys, 1);
}

// Builds a Pal Pad entry for the local trainer, copying their identity and the
// IDs/game codes/languages/genders of the 16 trainers in their Pal Pad.
static void PalPad_CreateNetworkObject(TrainerInfo *trainerInfo, PalPad *source, PalPad *destination)
{
    CharCode_Copy(destination->trainerName, TrainerInfo_Name(trainerInfo));

    destination->trainerId = TrainerInfo_ID(trainerInfo);
    destination->language = TrainerInfo_Language(trainerInfo);
    destination->gameCode = TrainerInfo_GameCode(trainerInfo);
    destination->gender = TrainerInfo_Gender(trainerInfo);

    for (int i = 0; i < PAL_PAD_ENTRIES; i++) {
        destination->associatedTrainerIds[i] = source[i].trainerId;
        destination->associatedTrainerGameCodes[i] = source[i].gameCode;
        destination->associatedTrainerLanguages[i] = source[i].language;
        destination->associatedTrainerGenders[i] = source[i].gender;
    }
}

// Command 33 handler: merge a peer's Pal Pad into the local one, skipping the
// entry that belongs to this console.
void LinkBattleComm_RecvPalPad(int netId, int size, void *data, void *commState)
{
    LinkBattleCommState *linkBattleCommState = (LinkBattleCommState *)commState;

    if (CommSys_CurNetId() != netId) {
        PalPad_PushEntries(linkBattleCommState->dto->palPad, (PalPad *)data, 1, HEAP_ID_BATTLE);
    }

    linkBattleCommState->recvCount++;
}

static int LinkBattleComm_PalPadSize(void)
{
    return 136;
}
