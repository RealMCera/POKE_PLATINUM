#include "wifi_list.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "struct_defs/wi_fi_list.h"

#include "assert.h"
#include "rtc.h"
#include "savedata.h"
#include "string_gf.h"
#include "wifi_list_util.h"

int WiFiList_SaveSize(void)
{
    return sizeof(WiFiList);
}

// Clears the list and marks every friend slot empty: both names are set to the
// 0xFFFF terminator and the gender is set to 2 (unknown).
void WiFiList_Init(WiFiList *wiFiList)
{
    int v0;

    MI_CpuClearFast(wiFiList, sizeof(WiFiList));

    for (v0 = 0; v0 < 32; v0++) {
        wiFiList->friendEntries[v0].playerName[0] = 0xffff;
        wiFiList->friendEntries[v0].groupName[0] = 0xffff;
        wiFiList->friendEntries[v0].gender = 2;
    }

    WiFiList_InitUserData(wiFiList);
}

DWCUserData *WiFiList_GetUserData(WiFiList *wiFiList)
{
    return &(wiFiList->userData);
}

// Reads one field of a friend's record. See enum WiFiListFriendField for the
// selector values.
u32 WiFiList_GetFriendField(WiFiList *wiFiList, int param1, int param2)
{
    u32 v0;

    GF_ASSERT(param1 < 32);

    switch (param2) {
    case WIFI_LIST_FRIEND_FIELD_TRAINER_ID:
        v0 = wiFiList->friendEntries[param1].trainerID;
        break;
    case WIFI_LIST_FRIEND_FIELD_WINS:
        v0 = wiFiList->friendEntries[param1].wins;
        break;
    case WIFI_LIST_FRIEND_FIELD_LOSSES:
        v0 = wiFiList->friendEntries[param1].losses;
        break;
    case WIFI_LIST_FRIEND_FIELD_TRADES:
        v0 = wiFiList->friendEntries[param1].trades;
        break;
    case WIFI_LIST_FRIEND_FIELD_YEAR:
        v0 = wiFiList->friendEntries[param1].year;
        break;
    case WIFI_LIST_FRIEND_FIELD_MONTH:
        v0 = wiFiList->friendEntries[param1].month;
        break;
    case WIFI_LIST_FRIEND_FIELD_DAY:
        v0 = wiFiList->friendEntries[param1].day;
        break;
    case WIFI_LIST_FRIEND_FIELD_GENDER:
        v0 = wiFiList->friendEntries[param1].gender;
        break;
    case WIFI_LIST_FRIEND_FIELD_APPEARANCE:
        v0 = wiFiList->friendEntries[param1].appearance;
        break;
    case WIFI_LIST_FRIEND_FIELD_POFFIN_SESSIONS:
        v0 = wiFiList->friendEntries[param1].poffinSessions;
        break;
    case WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_0:
        v0 = wiFiList->friendEntries[param1].plazaGame0Count;
        break;
    case WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_1:
        v0 = wiFiList->friendEntries[param1].plazaGame1Count;
        break;
    case WIFI_LIST_FRIEND_FIELD_PLAZA_GAME_2:
        v0 = wiFiList->friendEntries[param1].plazaGame2Count;
        break;
    }

    return v0;
}

// Writes one field of a friend's record. Only the writable fields are handled;
// the record counters are read-only here and must be updated through the
// dedicated Add* functions.
void WiFiList_SetFriendField(WiFiList *wiFiList, int param1, int param2, u32 param3)
{
    GF_ASSERT(param1 < 32);

    switch (param2) {
    case WIFI_LIST_FRIEND_FIELD_TRAINER_ID:
        wiFiList->friendEntries[param1].trainerID = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_WINS:
        GF_ASSERT(FALSE);
        break;
    case WIFI_LIST_FRIEND_FIELD_LOSSES:
        GF_ASSERT(FALSE);
        break;
    case WIFI_LIST_FRIEND_FIELD_TRADES:
        GF_ASSERT(FALSE);
        break;
    case WIFI_LIST_FRIEND_FIELD_YEAR:
        wiFiList->friendEntries[param1].year = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_MONTH:
        wiFiList->friendEntries[param1].month = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_DAY:
        wiFiList->friendEntries[param1].day = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_GENDER:
        wiFiList->friendEntries[param1].gender = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_APPEARANCE:
        wiFiList->friendEntries[param1].appearance = param3;
        break;
    case WIFI_LIST_FRIEND_FIELD_POFFIN_SESSIONS:
        GF_ASSERT(FALSE);
        break;
    }
}

DWCFriendData *WiFiList_GetFriendData(WiFiList *wiFiList, int param1)
{
    GF_ASSERT(param1 < 32);
    return &(wiFiList->friendData[param1]);
}

u16 *WiFiList_GetFriendPlayerName(WiFiList *wiFiList, int param1)
{
    GF_ASSERT(param1 < 32);
    return wiFiList->friendEntries[param1].playerName;
}

void WiFiList_SetFriendPlayerName(WiFiList *wiFiList, int param1, String *param2)
{
    GF_ASSERT(param1 < 32);
    String_ToChars(param2, wiFiList->friendEntries[param1].playerName, sizeof(wiFiList->friendEntries[param1].playerName));
}

u16 *WiFiList_GetFriendGroupName(WiFiList *wiFiList, int param1)
{
    GF_ASSERT(param1 < 32);
    return wiFiList->friendEntries[param1].groupName;
}

void WiFiList_SetFriendGroupName(WiFiList *wiFiList, int param1, String *param2)
{
    GF_ASSERT(param1 < 32);
    String_ToChars(param2, wiFiList->friendEntries[param1].groupName, sizeof(wiFiList->friendEntries[param1].groupName));
}

BOOL WiFiList_IsValidFriendData(WiFiList *wiFiList, int param1)
{
    GF_ASSERT(param1 < 32);
    return DWC_IsValidFriendData(&wiFiList->friendData[param1]);
}

int WiFiList_GetValidFriendsCount(WiFiList *wiFiList)
{
    int i, validFriendsCount = 0;

    for (i = 0; i < 32; i++) {
        if (WiFiList_IsValidFriendData(wiFiList, i)) {
            validFriendsCount++;
        }
    }

    return validFriendsCount;
}

// Returns one past the index of the last valid friend, i.e. the number of
// slots to display. Invalid slots before the last valid friend are counted too.
int WiFiList_GetFriendListSize(WiFiList *wiFiList)
{
    int v0, v1 = 0;

    for (v0 = 0; v0 < 32; v0++) {
        if (WiFiList_IsValidFriendData(wiFiList, v0)) {
            v1 = v0 + 1;
        }
    }

    return v1;
}

// Removes the friend at param1 by shifting every later slot down one and
// clearing the last slot.
void WiFiList_DeleteFriend(WiFiList *wiFiList, int param1)
{
    int v0;

    GF_ASSERT(param1 < 32);

    for (v0 = param1; v0 < (32 - 1); v0++) {
        MI_CpuCopy8(&wiFiList->friendEntries[v0 + 1], &wiFiList->friendEntries[v0], sizeof(WiFiListFriend));
        MI_CpuCopy8(&wiFiList->friendData[v0 + 1], &wiFiList->friendData[v0], sizeof(DWCFriendData));
    }

    v0 = 32 - 1;

    MI_CpuClearFast(&wiFiList->friendEntries[v0], sizeof(WiFiListFriend));
    MI_CpuClearFast(&wiFiList->friendData[v0], sizeof(DWCFriendData));

    wiFiList->friendEntries[v0].playerName[0] = 0xffff;
    wiFiList->friendEntries[v0].groupName[0] = 0xffff;
    wiFiList->friendEntries[v0].gender = 2;
}

// Moves the friend at param2 into slot param1 and clears slot param2.
static void WiFiList_MoveFriendData(WiFiList *wiFiList, int param1, int param2)
{
    MI_CpuCopy8(&wiFiList->friendEntries[param2], &wiFiList->friendEntries[param1], sizeof(WiFiListFriend));
    MI_CpuCopy8(&wiFiList->friendData[param2], &wiFiList->friendData[param1], sizeof(DWCFriendData));
    MI_CpuClearFast(&wiFiList->friendEntries[param2], sizeof(WiFiListFriend));
    MI_CpuClearFast(&wiFiList->friendData[param2], sizeof(DWCFriendData));

    wiFiList->friendEntries[param2].playerName[0] = 0xffff;
    wiFiList->friendEntries[param2].groupName[0] = 0xffff;
    wiFiList->friendEntries[param2].gender = 2;
}

// Packs valid friends towards the front by repeatedly moving the first valid
// friend that follows an invalid slot into that slot, until no invalid slot
// precedes a valid one.
void WiFiList_CompactFriendList(WiFiList *wiFiList)
{
    int i, v1 = -1;

    for (i = 0; i < 32; i++) {
        if (WiFiList_IsValidFriendData(wiFiList, i)) {
            if (v1 != -1) {
                WiFiList_MoveFriendData(wiFiList, v1, i);

                i = -1;
                v1 = -1;
            }
        } else if (v1 == -1) {
            v1 = i;
        }
    }
}

// Stamps the friend's record with today's date. The RTC year is stored with
// the 2000 offset applied.
void WiFiList_SetHostFriendCurrentDate(WiFiList *wiFiList, int hostFriendID)
{
    RTCDate date;

    GetCurrentDate(&date);

    wiFiList->friendEntries[hostFriendID].year = date.year + 2000;
    wiFiList->friendEntries[hostFriendID].month = date.month;
    wiFiList->friendEntries[hostFriendID].day = date.day;
}

// Adds a battle record to the friend's totals, clamping each counter at 9999,
// then refreshes the last-seen date.
void WiFiList_AddFriendRecord(WiFiList *wiFiList, int param1, int param2, int param3, int param4)
{
    wiFiList->friendEntries[param1].wins += param2;

    if (wiFiList->friendEntries[param1].wins > 9999) {
        wiFiList->friendEntries[param1].wins = 9999;
    }

    wiFiList->friendEntries[param1].losses += param3;

    if (wiFiList->friendEntries[param1].losses > 9999) {
        wiFiList->friendEntries[param1].losses = 9999;
    }

    wiFiList->friendEntries[param1].trades += param4;

    if (wiFiList->friendEntries[param1].trades > 9999) {
        wiFiList->friendEntries[param1].trades = 9999;
    }

    WiFiList_SetHostFriendCurrentDate(wiFiList, param1);
}

// Adds to the friend's poffin-session count, clamping at 9999, then refreshes
// the last-seen date.
void WiFiList_AddFriendPoffinSessions(WiFiList *wiFiList, int param1, int param2)
{
    wiFiList->friendEntries[param1].poffinSessions += param2;

    if (wiFiList->friendEntries[param1].poffinSessions > 9999) {
        wiFiList->friendEntries[param1].poffinSessions = 9999;
    }

    WiFiList_SetHostFriendCurrentDate(wiFiList, param1);
}

// Adds to the friend's play count for the first Wi-Fi Plaza minigame, clamping
// at 9999, then refreshes the last-seen date.
void WiFiList_AddFriendPlazaGame0Count(WiFiList *wiFiList, int param1, int param2)
{
    wiFiList->friendEntries[param1].plazaGame0Count += param2;

    if (wiFiList->friendEntries[param1].plazaGame0Count > 9999) {
        wiFiList->friendEntries[param1].plazaGame0Count = 9999;
    }

    WiFiList_SetHostFriendCurrentDate(wiFiList, param1);
}

// Adds to the friend's play count for the second Wi-Fi Plaza minigame, clamping
// at 9999, then refreshes the last-seen date.
void WiFiList_AddFriendPlazaGame1Count(WiFiList *wiFiList, int param1, int param2)
{
    wiFiList->friendEntries[param1].plazaGame1Count += param2;

    if (wiFiList->friendEntries[param1].plazaGame1Count > 9999) {
        wiFiList->friendEntries[param1].plazaGame1Count = 9999;
    }

    WiFiList_SetHostFriendCurrentDate(wiFiList, param1);
}

// Adds to the friend's play count for the third Wi-Fi Plaza minigame, clamping
// at 9999, then refreshes the last-seen date.
void WiFiList_AddFriendPlazaGame2Count(WiFiList *wiFiList, int param1, int param2)
{
    wiFiList->friendEntries[param1].plazaGame2Count += param2;

    if (wiFiList->friendEntries[param1].plazaGame2Count > 9999) {
        wiFiList->friendEntries[param1].plazaGame2Count = 9999;
    }

    WiFiList_SetHostFriendCurrentDate(wiFiList, param1);
}

// Merges the record at param1 into the record at param2 (adding every counter
// with the same 9999 clamp and copying the group name), then clears slot
// param1. Used to fold a duplicate friend entry into an existing one.
void WiFiList_MergeFriendData(WiFiList *wiFiList, int param1, int param2)
{
    wiFiList->friendEntries[param2].wins += wiFiList->friendEntries[param1].wins;

    if (wiFiList->friendEntries[param2].wins > 9999) {
        wiFiList->friendEntries[param2].wins = 9999;
    }

    wiFiList->friendEntries[param2].losses += wiFiList->friendEntries[param1].losses;

    if (wiFiList->friendEntries[param2].losses > 9999) {
        wiFiList->friendEntries[param2].losses = 9999;
    }

    wiFiList->friendEntries[param2].trades += wiFiList->friendEntries[param1].trades;

    if (wiFiList->friendEntries[param2].trades > 9999) {
        wiFiList->friendEntries[param2].trades = 9999;
    }

    wiFiList->friendEntries[param2].poffinSessions += wiFiList->friendEntries[param1].poffinSessions;

    if (wiFiList->friendEntries[param2].poffinSessions > 9999) {
        wiFiList->friendEntries[param2].poffinSessions = 9999;
    }

    wiFiList->friendEntries[param2].plazaGame0Count += wiFiList->friendEntries[param1].plazaGame0Count;

    if (wiFiList->friendEntries[param2].plazaGame0Count > 9999) {
        wiFiList->friendEntries[param2].plazaGame0Count = 9999;
    }

    wiFiList->friendEntries[param2].plazaGame1Count += wiFiList->friendEntries[param1].plazaGame1Count;

    if (wiFiList->friendEntries[param2].plazaGame1Count > 9999) {
        wiFiList->friendEntries[param2].plazaGame1Count = 9999;
    }

    wiFiList->friendEntries[param2].plazaGame2Count += wiFiList->friendEntries[param1].plazaGame2Count;

    if (wiFiList->friendEntries[param2].plazaGame2Count > 9999) {
        wiFiList->friendEntries[param2].plazaGame2Count = 9999;
    }

    MI_CpuCopyFast(wiFiList->friendEntries[param1].groupName, wiFiList->friendEntries[param2].groupName, sizeof(u16) * (7 + 1));
    MI_CpuClearFast(&wiFiList->friendEntries[param1], sizeof(WiFiListFriend));

    wiFiList->friendEntries[param1].playerName[0] = 0xffff;
    wiFiList->friendEntries[param1].groupName[0] = 0xffff;
    wiFiList->friendEntries[param1].gender = 2;
}

WiFiList *SaveData_GetWiFiList(SaveData *saveData)
{
    return SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_WIFI_LIST);
}
