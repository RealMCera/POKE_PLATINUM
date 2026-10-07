#ifndef POKEPLATINUM_WIFI_LIST_H
#define POKEPLATINUM_WIFI_LIST_H

#include <dwc.h>

#include "struct_decls/wi_fi_list.h"

#include "savedata.h"
#include "string_gf.h"

#define MAX_FRIENDS 32

// Save-data manager for the Nintendo WFC friend list. It stores the player's
// own DWC user data plus, for each of the 32 friend slots, the DWC friend data
// and the extra per-friend record (names, trainer ID, battle/trade/poffin/
// plaza-game counts and last-seen date) used by the friend-list UIs.
//
// Field selectors for WiFiList_GetFriendField / WiFiList_SetFriendField. The
// setter rejects the read-only record fields (wins/losses/trades and the
// poffin/plaza-game counters), which are only ever updated by the dedicated
// Add* functions.
enum WiFiListFriendField {
    WIFI_LIST_FRIEND_FIELD_TRAINER_ID = 0,
    WIFI_LIST_FRIEND_FIELD_WINS,
    WIFI_LIST_FRIEND_FIELD_LOSSES,
    WIFI_LIST_FRIEND_FIELD_TRADES,
    WIFI_LIST_FRIEND_FIELD_YEAR,
    WIFI_LIST_FRIEND_FIELD_MONTH,
    WIFI_LIST_FRIEND_FIELD_DAY,
    WIFI_LIST_FRIEND_FIELD_APPEARANCE,
    WIFI_LIST_FRIEND_FIELD_GENDER,
    WIFI_LIST_FRIEND_FIELD_POFFIN_SESSIONS,
    WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_0,
    WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_1,
    WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_2,
};

int WiFiList_SaveSize(void);
void WiFiList_Init(WiFiList *wiFiList);
DWCUserData *WiFiList_GetUserData(WiFiList *wiFiList);
u32 WiFiList_GetFriendField(WiFiList *wiFiList, int param1, int param2);
void WiFiList_SetFriendField(WiFiList *wiFiList, int param1, int param2, u32 param3);
DWCFriendData *WiFiList_GetFriendData(WiFiList *wiFiList, int param1);
u16 *WiFiList_GetFriendPlayerName(WiFiList *wiFiList, int param1);
void WiFiList_SetFriendPlayerName(WiFiList *wiFiList, int param1, String *param2);
u16 *WiFiList_GetFriendGroupName(WiFiList *wiFiList, int param1);
void WiFiList_SetFriendGroupName(WiFiList *wiFiList, int param1, String *param2);
BOOL WiFiList_IsValidFriendData(WiFiList *wiFiList, int param1);
int WiFiList_GetValidFriendsCount(WiFiList *wiFiList);
int WiFiList_GetFriendListSize(WiFiList *wiFiList);
void WiFiList_DeleteFriend(WiFiList *wiFiList, int param1);
void WiFiList_CompactFriendList(WiFiList *wiFiList);
void WiFiList_SetHostFriendCurrentDate(WiFiList *wiFiList, int param1);
void WiFiList_AddFriendRecord(WiFiList *wiFiList, int param1, int param2, int param3, int param4);
void WiFiList_AddFriendPoffinSessions(WiFiList *wiFiList, int param1, int param2);
void WiFiList_AddFriendPlazaGame0Count(WiFiList *wiFiList, int param1, int param2);
void WiFiList_AddFriendPlazaGame1Count(WiFiList *wiFiList, int param1, int param2);
void WiFiList_AddFriendPlazaGame2Count(WiFiList *wiFiList, int param1, int param2);
void WiFiList_MergeFriendData(WiFiList *wiFiList, int param1, int param2);
WiFiList *SaveData_GetWiFiList(SaveData *saveData);

#endif // POKEPLATINUM_WIFI_LIST_H
