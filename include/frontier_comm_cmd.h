#ifndef POKEPLATINUM_FRONTIER_COMM_CMD_H
#define POKEPLATINUM_FRONTIER_COMM_CMD_H

#include "struct_defs/battle_tower.h"

#include "overlay104/wfc_facility_selector.h"

// Registers the Battle Frontier comm command table with the comm manager.
void FrontierCommCmd_Init(void *context);

// Battle Tower commands.
BOOL BattleTower_SendTrainerIDListCmd(BattleTower *battleTower);

// WFC facility selector commands.
BOOL WFCFacilitySelector_SendFacilityAndLatestStreak(WFCFacilitySelector *selector);
BOOL WFCFacilitySelector_SendDidDropOutCmd(WFCFacilitySelector *selector, u16 didDropOut);
BOOL WFCFacilitySelector_SendSelectedMons(WFCFacilitySelector *selector, u16 selectedSlot1, u16 selectedSlot2);
BOOL WFCFacilitySelector_SendStreakDeletionChoice(WFCFacilitySelector *selector, u16 streakDeletionChoice);
BOOL WFCFacilitySelector_SendPlayAgainChoice(WFCFacilitySelector *selector, u16 notPlayingAgain);

#endif // POKEPLATINUM_FRONTIER_COMM_CMD_H
