#include "comm_field_cmd.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"

#include "underground/manager.h"
#include "underground/mining.h"
#include "underground/pc.h"
#include "underground/player.h"
#include "underground/player_talk.h"
#include "underground/records.h"
#include "underground/secret_bases.h"
#include "underground/spheres.h"
#include "underground/traps.h"

#include "comm_player_manager.h"
#include "field_comm_manager.h"
#include "scrcmd_battle_arcade.h"
#include "scrcmd_battle_castle.h"
#include "scrcmd_battle_hall.h"
#include "trainer_case.h"
#include "trainer_info.h"
#include "comm_cmd.h"
#include "colosseum.h"
#include "union_room.h"

static int CommPacketSizeOf_TrainerCase(void);
static int CommPacketSizeOf_ConnectionConfirm(void);
static int CommPacketSizeOf_DrawingConnAck(void);

// Packet-size callbacks for commands that are received but ignored. The comm
// system still needs to know how many bytes to consume, so each of these
// returns the size of the payload the sender used.

static int CommPacketSizeOf_3Bytes_Unused(void)
{
    return 3;
}

// Command 130: the Union Room's generic command envelope (a u32 sub-command
// followed by up to 20 bytes of payload).
static int CommPacketSizeOf_UnionRoomCommand(void)
{
    return 24;
}

// Command 131: one serialized party (six 236-byte Pokémon plus four u16
// fields).
static int CommPacketSizeOf_UnionRoomPartyData(void)
{
    return 236 * 6 + 4 * 2;
}

// Command table for the field (overworld) communication system. It covers the
// commands used by the Underground, the Union Room, the Colosseum and the
// Battle Frontier facilities. CommCmd_Init indexes the table by
// (command - 22), so entry N handles command N + 22; the trailing comments mark
// the command number of selected entries.
//
// Most entries dispatch to a handler defined in the owning subsystem (see the
// includes above). The block at the end of the table covers commands that are
// received but ignored: they use CommFieldCmd_NoOp as the handler and only
// declare a packet size so the comm system can consume the payload. The
// non-trivial ignored commands are:
//   112 - connection-confirm handshake (4 bytes)
//   116 - mixed record (3008 bytes)
//   118 - drawing canvas chunk (1008 bytes)
//   119 - drawing player status (10 bytes)
//   120 - drawing all-player statuses (50 bytes)
//   126 - drawing connection acknowledgement (4 bytes)
//   130 - Union Room generic command envelope (24 bytes)
//   131 - Union Room serialized party (1424 bytes)
static const CommCmdTable sCommFieldCmdTable[] = {
    { CommPlayer_RecvLocation, CommPacketSizeOf_RecvLocation, NULL },
    { CommPlayer_RecvLocationAndInit, CommPacketSizeOf_RecvLocationAndInit, NULL },
    { UndergroundMan_ProcessVendorTalkRequest, CommPacketSizeOf_NetId, NULL },
    { UndergroundPlayer_ProcessVendorTalk, CommPacketSizeOf_Nothing, NULL }, // 25
    { UndergroundPlayer_ProcessVendorTalkServer, CommPacketSizeOf_NetId, NULL },
    { UndergroundPlayer_ProcessOpenMenuRequest, CommPacketSizeOf_Nothing, NULL },
    { UndergroundMan_ProcessInteractEvent, CommPacketSizeOf_InteractEvent, NULL },
    { UndergroundPlayer_ProcessOpenMenuEvent, CommPacketSizeOf_Variable, NULL },
    { UndergroundPlayer_ProcessTalkEvent, CommPacketSizeOf_Variable, NULL }, // 30
    { UndergroundMan_ProcessAllDataSentMessage, CommPacketSizeOf_Nothing, NULL },
    { Traps_TryPlaceTrap, CommPacketSizeOf_NetId, NULL },
    { Traps_RemoveBuriedTrapAtIndex_Unused, CommPacketSizeOf_2Bytes_Unused, NULL }, // corresponding cmd never sent
    { Traps_ProcessPlaceTrapResult, CommPacketSizeOf_PlaceTrapResult, NULL },
    { Traps_LoadLinkPlacedTraps, CommPacketSizeOf_AllTrapsPlacedPlayer, NULL }, // 35
    { Traps_ReceiveLoadTrapsResult, CommPacketSizeOf_LoadTrapsResult, NULL },
    { Traps_HandleTriggeredTrap, CommPacketSizeOf_TriggeredTrap2, NULL },
    { Traps_CallSecondTrapEffectServerFunc, CommPacketSizeOf_NetId, NULL },
    { Traps_StartLinkSlideAnimation_Unused, CommPacketSizeOf_3Bytes_Unused, NULL }, // corresponding cmd never sent
    { Traps_EscapeHole, CommPacketSizeOf_NetId, NULL },
    { Traps_EscapeTrapServer, CommPacketSizeOf_Nothing, NULL },
    { Traps_ProcessEscapedTrap, CommPacketSizeOf_EscapedTrap, NULL },
    { Traps_EndCurrentTrapEffectServer, CommPacketSizeOf_Nothing, NULL },
    { Traps_ProcessTrapHelp, CommPacketSizeOf_TrapHelpData, NULL },
    { Traps_ProcessTriggeredTrapBits, CommPacketSizeOf_NetId, NULL },
    { Traps_QueueSendTrapRadarResults, CommPacketSizeOf_Nothing, NULL },
    { Traps_ReceiveTrapRadarResults, CommPacketSizeOf_TrapRadarResult, NULL },
    { UndergroundMan_ProcessTouchInput, CommPacketSizeOf_CoordinatesU16, NULL },
    { UndergroundMan_ProcessTouchRadarTrapResults, CommPacketSizeOf_Variable, NULL },
    { UndergroundMan_ProcessTouchRadarMiningSpotResults, CommPacketSizeOf_Variable, NULL }, // 50
    { Traps_ProcessDisengagedTrap, CommPacketSizeOf_TriggeredTrap, NULL },
    { CommPlayer_RecvDelete, CommPacketSizeOf_NetId, NULL },
    { SecretBases_ProcessBaseInfo, CommPacketSizeOf_SecretBaseInfo, NULL },
    { SecretBases_ProcessBaseEnter, CommPacketSizeOf_SecretBaseInfo, NULL },
    { SecretBases_ProcessBaseEntrancesBuffer, CommPacketSizeOf_BaseEntrancesBuffer, NULL }, // 55
    { SecretBases_ClearTransitioningStatus, CommPacketSizeOf_Nothing, NULL },
    { SecretBases_ProcessBaseExitEvent, CommPacketSizeOf_BaseExitEvent, NULL },
    { SecretBases_ProcessBaseTransitionPromptEvent, CommPacketSizeOf_BaseTransitionPromptEvent, NULL },
    { SecretBases_ProcessBaseTransitionEvent, CommPacketSizeOf_BaseTransitionEvent, NULL },
    { SecretBases_ProcessGoodInteractionEvent, CommPacketSizeOf_GoodInteractionEvent, NULL }, // 60
    { SecretBases_ProcessFailedBaseEnter, CommPacketSizeOf_NetId, NULL },
    { CommPlayer_RecvMovementEnabled, CommPacketSizeOf_NetId, NULL },
    { Spheres_ProcessRetrieveBuriedSphereRequest, CommPacketSizeOf_NetId, NULL },
    { Mining_ProcessMiningSpotInteract, CommPacketSizeOf_NetId, NULL },
    { Mining_ProcessConfirmStartMiningResult, CommPacketSizeOf_NetId, NULL }, // 65
    { Mining_ProcessStartMiningConfirm, CommPacketSizeOf_NetId, NULL },
    { Mining_ProcessMiningGameEnd, CommPacketSizeOf_NetId, NULL },
    { Mining_ProcessLinkInput, CommPacketSizeOf_MiningLinkInput, NULL },
    { Mining_ProcessLinkInputServer, CommPacketSizeOf_MiningLinkInputWithNetID, NULL },
    { UndergroundMan_ProcessPlayerState, CommPacketSizeOf_UndergroundPlayerState, NULL },
    { Mining_ProcessTreasureRadarStart, CommPacketSizeOf_Nothing, NULL },
    { Mining_ProcessMiningSpotRadarResult, CommPacketSizeOf_MiningSpotRadarResult, NULL },
    { UndergroundTalk_RequestLinkTalkStateUpdateServer, CommPacketSizeOf_TalkStateChangeRequest, NULL },
    { UndergroundTalkResponse_RequestLinkTalkStateUpdateServer, CommPacketSizeOf_TalkStateChangeRequest, NULL },
    { UndergroundTalkResponse_HandleLinkTalkStateUpdateServer, CommPacketSizeOf_TalkStateChangeRequest, NULL }, // 75
    { UndergroundTalk_HandleLinkTalkStateUpdateServer, CommPacketSizeOf_TalkStateChangeRequest, NULL },
    { UndergroundTalk_SendGiftServer, CommPacketSizeOf_Gift, NULL },
    { UndergroundTalkResponse_ReceiveGiftOffer, CommPacketSizeOf_Gift, NULL },
    { UndergroundTalk_SendTalkMessageServer, CommPacketSizeOf_TalkMessage, NULL },
    { UndergroundTalk_ReceiveTalkMessage, CommPacketSizeOf_TalkMessage, NULL },
    { UndergroundRecords_SendRecordServer, CommPacketSizeOf_Variable, NULL },
    { UndergroundRecords_ProcessLinkRecord, CommPacketSizeOf_Variable, NULL },
    { UndergroundPC_ProcessPCInteraction, CommPacketSizeOf_PCInteraction, NULL }, // 83
    { UndergroundPlayer_ProcessFlagEventType, CommPacketSizeOf_NetId, NULL },
    { UndergroundPlayer_ProcessFlagEvent, CommPacketSizeOf_FlagEvent, NULL },
    { SecretBases_ProcessBaseCreateRequest, CommPacketSizeOf_NetId, NULL },
    { SecretBases_ProcessBaseCreateEvent, CommPacketSizeOf_SecretBaseCreateEvent, NULL },
    { FieldCommManager_SetTrainerCaseCopiedFlag, CommPacketSizeOf_TrainerCase, FieldCommManager_GetTrainerCase },
    { UndergroundPC_ProcessTakeFlagAttempt, CommPacketSizeOf_PCInteraction, NULL },
    { UndergroundPC_ProcessTakenFlag, CommPacketSizeOf_PCInteraction, NULL }, // 90
    { UndergroundPlayer_ProcessHeldFlagOwnerInfo, CommPacketSizeOf_TrainerInfo, NULL },
    { UndergroundPlayer_ProcessHeldFlagOwnerInfoServer, CommPacketSizeOf_HeldFlagInfo, UndergroundPlayer_GetHeldFlagInfoBuffer },
    { UndergroundPlayer_ProcessHeldFlagOwnerInfoAck, CommPacketSizeOf_NetId, NULL },
    { CommPlayer_RecvBattleRoomState, CommPacketSizeOf_NetId, NULL },
    { FieldCommManager_UpdateBattleRoomMovement, CommPacketSizeOf_NetId, NULL }, // 95
    { SecretBases_ProcessFlagRankUp, CommPacketSizeOf_NetId, NULL },
    { SecretBases_ProcessFlagRankUpEvent, CommPacketSizeOf_FlagRankUpEvent, NULL },
    { UnionRoom_HandleNoOpTrainerInfo, TrainerInfo_Size, NULL },
    { UnionRoom_HandleSetActivity, CommPacketSizeOf_NetId, NULL },
    { UnionRoom_HandleNoOpNetId, CommPacketSizeOf_NetId, NULL },
    { UnionRoom_HandleMenuChoice, CommPacketSizeOf_NetId, NULL },
    { UnionRoom_HandleResetState, CommPacketSizeOf_Nothing, NULL },
    { UnionRoom_HandlePeerActivity, CommPacketSizeOf_NetId, NULL },
    { UnionRoom_HandlePeerNoActivity, CommPacketSizeOf_Nothing, NULL },
    { UnionRoom_HandleTrainerCase, CommPacketSizeOf_TrainerCase, UnionRoom_GetTrainerCaseBuffer },
    { Colosseum_HandleReceivedParty, Colosseum_PartyExchangeSize, Colosseum_GetPartyExchangeBuffer },
    { Colosseum_HandleReceivedSlot, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_ConnectionConfirm, NULL }, // 112
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommFieldCmd_PacketSizeOf_RecordData, NULL }, // 116
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommFieldCmd_PacketSizeOf_DrawingChunk, NULL }, // 118
    { CommFieldCmd_NoOp, CommFieldCmd_PacketSizeOf_DrawingPlayerStatus, NULL }, // 119
    { CommFieldCmd_NoOp, CommFieldCmd_PacketSizeOf_DrawingAllStatuses, NULL }, // 120
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_NetId, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_DrawingConnAck, NULL }, // 126
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_Nothing, NULL },
    { CommFieldCmd_NoOp, CommPacketSizeOf_UnionRoomCommand, NULL }, // 130
    { CommFieldCmd_NoOp, CommPacketSizeOf_UnionRoomPartyData, NULL }, // 131
    { BattleHall_ProcessSelectedSpeciesMsg, CommPacketSizeOf_Variable, NULL },
    { BattleCastle_ProcessSpeciesCheckMsg, CommPacketSizeOf_Variable, NULL },
    { BattleArcade_ProcessSpeciesCheckMsg, CommPacketSizeOf_Variable, NULL }
};

// Placeholder handler for commands that are received but have no effect. It is
// shared by every command table in the game that needs to reserve an entry.
void CommFieldCmd_NoOp(int param0, int param1, void *param2, void *param3)
{
    return;
}

// Registers the field command table. `param0` is the field system passed back
// to every handler as its context.
void CommFieldCmd_Init(void *param0)
{
    int v0 = sizeof(sCommFieldCmdTable) / sizeof(CommCmdTable);
    CommCmd_Init(sCommFieldCmdTable, v0, param0);
}

// Command 88: the trainer case exchanged when a player joins the field.
static int CommPacketSizeOf_TrainerCase(void)
{
    return sizeof(TrainerCase);
}

// Command 116: one player's mixed record (a 3000-byte record plus a checksum
// and an LCRNG seed).
int CommFieldCmd_PacketSizeOf_RecordData(void)
{
    return 3000 + 8;
}

// Command 118: one 1000-byte slice of the shared drawing canvas plus its
// checksum and index.
int CommFieldCmd_PacketSizeOf_DrawingChunk(void)
{
    return 1008;
}

// Command 119: a single player's pen/cursor status.
int CommFieldCmd_PacketSizeOf_DrawingPlayerStatus(void)
{
    return 10;
}

// Command 120: all five players' pen/cursor statuses relayed at once.
int CommFieldCmd_PacketSizeOf_DrawingAllStatuses(void)
{
    return 10 * 5;
}

// Command 112: the connection-confirm handshake.
static int CommPacketSizeOf_ConnectionConfirm(void)
{
    return 4;
}

// Command 126: the drawing session's connection acknowledgement.
static int CommPacketSizeOf_DrawingConnAck(void)
{
    return 4;
}
