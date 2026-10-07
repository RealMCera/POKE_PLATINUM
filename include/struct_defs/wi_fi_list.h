#ifndef POKEPLATINUM_STRUCT_DEF_WI_FI_LIST_H
#define POKEPLATINUM_STRUCT_DEF_WI_FI_LIST_H

#include <dwc.h>

// Per-friend data kept alongside the DWC friend data: the friend's group and
// player names, their trainer ID, their battle/trade/poffin/plaza-game records,
// and the date they were last seen.
typedef struct WiFiListFriend {
    u16 groupName[8];
    u16 playerName[8];
    u32 trainerID;
    u16 wins;
    u16 losses;
    u16 trades;
    u16 year;
    u8 month;
    u8 day;
    u8 gender;
    u8 appearance;
    u16 poffinSessions;
    u16 plazaGame0Count;
    u16 plazaGame1Count;
    u16 plazaGame2Count;
} WiFiListFriend;

typedef struct WiFiList {
    DWCUserData userData;
    DWCFriendData friendData[32];
    WiFiListFriend friendEntries[32];
} WiFiList;

#endif // POKEPLATINUM_STRUCT_DEF_WI_FI_LIST_H
