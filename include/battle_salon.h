#ifndef POKEPLATINUM_BATTLE_SALON_H
#define POKEPLATINUM_BATTLE_SALON_H

#include "field/field_system_decl.h"

#include "field_task.h"
#include "savedata.h"

// Battle Salon field tasks and helpers. See battle_salon.c for details.

void BattleSalon_StartPartyMenuTask(FieldTask *task, void **appData, u8 menuMode, u8 summaryMode, u8 minSelectionSlots, u8 maxSelectionSlots, u8 reqLevel, u8 selectedMonSlot);
void BattleSalon_StartWifiAppTask(FieldTask *task, u16 mode, u16 varID, u16 appParam);
void BattleSalon_StartCommTask(FieldTask *task, u16 commandType, u16 varID);
u16 BattleSalon_GrantReward(SaveData *saveData);
u16 BattleSalon_GetRewardStatus(SaveData *saveData);
u32 BattleSalon_AdvanceRng(u32 value);
u32 BattleSalon_AdvanceRngState(u32 state);
u32 BattleSalon_SeedRng(SaveData *saveData);
u32 BattleSalon_GetFreshRng(SaveData *saveData);
u32 BattleSalon_GetStreakRng(SaveData *saveData);
BOOL FieldSystem_IsInBattleTowerSalon(FieldSystem *fieldSystem);

#endif // POKEPLATINUM_BATTLE_SALON_H
