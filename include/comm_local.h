#ifndef POKEPLATINUM_COMM_LOCAL_H
#define POKEPLATINUM_COMM_LOCAL_H

u16 CommLocal_MaxMachines(u16 commType);
u16 CommLocal_MinMachines(u16 commType);
BOOL CommLocal_IsUnionGroup(int commType);
BOOL CommLocal_IsWifiGroup(int commType);
BOOL CommLocal_IsWifiConferenceGroup(int commType);
BOOL CommLocal_IsFriendOnlyWifiGroup(int commType);
BOOL CommLocal_IsBattleOrContestGroup(int commType);

#endif // POKEPLATINUM_COMM_LOCAL_H
