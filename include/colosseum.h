#ifndef POKEPLATINUM_COLOSSEUM_H
#define POKEPLATINUM_COLOSSEUM_H

#include "field/field_system_decl.h"
#include "functypes/funcptr_0205AB10.h"

// Battle-setup sequence and Trainer Case viewer for the Communication Club's
// Colosseum. See colosseum.c for details.
void Colosseum_StartBattle(FieldSystem *fieldSystem, UnkFuncPtr_0205AB10 *param1);
void Colosseum_HandleReceivedParty(int param0, int param1, void *param2, void *param3);
int Colosseum_PartyExchangeSize(void);
u8 *Colosseum_GetPartyExchangeBuffer(int param0, void *param1, int param2);
void Colosseum_HandleReceivedSlot(int param0, int param1, void *param2, void *param3);
void Colosseum_ViewTrainerCase(FieldSystem *fieldSystem);

#endif // POKEPLATINUM_COLOSSEUM_H
