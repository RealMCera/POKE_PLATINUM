#ifndef POKEPLATINUM_LINK_BATTLE_COMM_H
#define POKEPLATINUM_LINK_BATTLE_COMM_H

#include "struct_decls/battle_system.h"
#include "struct_defs/link_battle_comm_state.h"

// Link battle communication commands and handshake. See link_battle_comm.c.

void LinkBattleComm_Init(void *battleSys);
void LinkBattleComm_InitCommands(void *commState);
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
BOOL LinkBattleComm_PreparePalPad(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_SendPalPad(LinkBattleCommState *linkBattleCommState);
BOOL LinkBattleComm_PrepareTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot);
BOOL LinkBattleComm_SendTrainerSlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState);
BOOL LinkBattleComm_PreparePartySlot(LinkBattleCommState *linkBattleCommState, int slot);
BOOL LinkBattleComm_SendPartySlot(LinkBattleCommState *linkBattleCommState, int slot, int syncState);

#endif // POKEPLATINUM_LINK_BATTLE_COMM_H
