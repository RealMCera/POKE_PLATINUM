#include "wifi_friend_registration.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "struct_decls/wi_fi_list.h"

#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "savedata.h"
#include "string_gf.h"
#include "trainer_info.h"
#include "wifi_list.h"

// Nintendo WFC friend-list registration. This module matches the DWC friend
// data of players connected over the link cable against the 32 saved friend
// slots, and writes the connected player's identity and record into a slot.
// It is shared by the Pal Pad (field and trade room), the WiFi Plaza and the
// Union Room.

// Searches the saved friend list for a slot matching friendData and writes the
// slot index to outSlot. Returns how the slot was matched (see enum
// WiFiFriendMatch). When there is no match, outSlot receives the first free
// slot, or -1 if the list is full.
enum WiFiFriendMatch WiFiFriend_FindSlot(SaveData *saveData, DWCFriendData *friendData, int *outSlot)
{
    int i;
    DWCUserData *userData = WiFiList_GetUserData(SaveData_GetWiFiList(saveData));
    DWCFriendData *friendList = WiFiList_GetFriendData(SaveData_GetWiFiList(saveData), 0);

    *outSlot = -1;

    if (!DWC_IsValidFriendData(friendData)) {
        return WIFI_FRIEND_MATCH_INVALID;
    }

    for (i = 0; i < MAX_FRIENDS; i++) {
        if (DWC_IsEqualFriendData(friendData, friendList + i)) {
            *outSlot = i;
            return WIFI_FRIEND_MATCH_FOUND;
        } else if ((DWC_GetGsProfileId(userData, friendData) > 0) && (DWC_GetGsProfileId(userData, friendData) == DWC_GetGsProfileId(userData, friendList + i))) {
            *outSlot = i;
            return WIFI_FRIEND_MATCH_PROFILE;
        } else if ((*outSlot < 0) && !DWC_IsValidFriendData(friendList + i)) {
            // Remember the first empty slot in case no match is found.
            *outSlot = i;
        }
    }

    return WIFI_FRIEND_MATCH_NONE;
}

// Searches the saved friend list for a slot whose WFC profile ID matches the
// friend key. Returns how the slot was matched (see enum WiFiFriendMatch);
// outSlot receives the slot index, or the first free slot when there is no
// match.
enum WiFiFriendMatch WiFiFriend_FindSlotByFriendKey(SaveData *saveData, u64 friendKey, int *outSlot)
{
    int i;
    DWCUserData *userData = WiFiList_GetUserData(SaveData_GetWiFiList(saveData));
    DWCFriendData *friendList = WiFiList_GetFriendData(SaveData_GetWiFiList(saveData), 0);
    DWCFriendData token;

    if (!DWC_CheckFriendKey(userData, friendKey)) {
        return WIFI_FRIEND_MATCH_INVALID;
    }

    DWC_CreateFriendKeyToken(&token, friendKey);

    if (DWC_GetGsProfileId(userData, &token) <= 0) {
        return WIFI_FRIEND_MATCH_INVALID;
    }

    *outSlot = -1;

    for (i = 0; i < MAX_FRIENDS; i++) {
        if (DWC_GetGsProfileId(userData, &token) == DWC_GetGsProfileId(userData, friendList + i)) {
            *outSlot = i;
            return WIFI_FRIEND_MATCH_FOUND;
        } else if ((*outSlot < 0) && !DWC_IsValidFriendData(friendList + i)) {
            *outSlot = i;
        }
    }

    return WIFI_FRIEND_MATCH_NONE;
}

// Scans the players connected over the link cable and updates the saved friend
// list for those that already have a slot. Writes each player's match result
// into matchResults (indexed by net ID) and returns TRUE if at least one
// connected player has no slot yet, so the caller must register them.
BOOL WiFiFriend_UpdateConnectedPlayers(SaveData *saveData, int *matchResults, enum HeapID heapID)
{
    int netId, needsRegistration = 0, slot;
    DWCFriendData *friendList = WiFiList_GetFriendData(SaveData_GetWiFiList(saveData), 0);
    DWCFriendData *friendData;

    for (netId = 0; netId < CommSys_ConnectedCount(); netId++) {
        if (CommSys_CurNetId() == netId) {
            continue;
        }

        friendData = CommInfo_DWCFriendData(netId);

        if (friendData == NULL) {
            continue;
        }

        matchResults[netId] = WiFiFriend_FindSlot(saveData, friendData, &slot);

        GF_ASSERT(matchResults[netId] != WIFI_FRIEND_MATCH_INVALID);

        if (matchResults[netId] == WIFI_FRIEND_MATCH_FOUND) {
            // Already registered: refresh the record and group name.
            WiFiFriend_SavePlayerToSlot(saveData, netId, slot, heapID, WIFI_FRIEND_SAVE_RECORD);
            CommInfo_SavePlayerRecord(saveData);
        } else if (matchResults[netId] == WIFI_FRIEND_MATCH_PROFILE) {
            // Same WFC profile but stale data. Only overwrite it when not
            // connected to Nintendo WFC, so local wireless data is not clobbered.
            if (!CommManager_IsConnectedToWifi()) {
                WiFiFriend_SavePlayerToSlot(saveData, netId, slot, heapID, WIFI_FRIEND_SAVE_PROFILE);
                MI_CpuCopy8(friendData, &friendList[slot], sizeof(DWCFriendData));
                CommInfo_SavePlayerRecord(saveData);
            }
        } else if (matchResults[netId] == WIFI_FRIEND_MATCH_NONE) {
            needsRegistration = 1;
        }
    }

    return needsRegistration;
}

// Writes a connected player into a friend slot. mode selects how much of the
// slot is overwritten (see enum WiFiFriendSaveMode). The group name, appearance
// and player record are always refreshed.
void WiFiFriend_SavePlayerToSlot(SaveData *saveData, int netId, int slot, enum HeapID heapID, enum WiFiFriendSaveMode mode)
{
    WiFiList *wiFiList = SaveData_GetWiFiList(saveData);
    DWCFriendData *storedFriendData = WiFiList_GetFriendData(wiFiList, slot);
    TrainerInfo *trainerInfo = CommInfo_TrainerInfo(netId);
    DWCFriendData *friendData;
    String *string;

    if (mode != WIFI_FRIEND_SAVE_RECORD) {
        friendData = CommInfo_DWCFriendData(netId);
        MI_CpuCopy8(friendData, storedFriendData, sizeof(DWCFriendData));
    }

    if (mode == WIFI_FRIEND_SAVE_NEW) {
        string = TrainerInfo_NameNewString(trainerInfo, heapID);
        WiFiList_SetFriendPlayerName(wiFiList, slot, string);
        String_Free(string);
        WiFiList_SetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_GENDER, TrainerInfo_Gender(trainerInfo));
        WiFiList_SetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_TRAINER_ID, TrainerInfo_ID(trainerInfo));
    } else if (mode == WIFI_FRIEND_SAVE_PROFILE) {
        // A gender field of 2 marks a slot that has not been registered yet
        // (see ov64_0222E09C), so only then is the identity refreshed.
        if (WiFiList_GetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_GENDER) == 2) {
            WiFiList_SetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_GENDER, TrainerInfo_Gender(trainerInfo));
            WiFiList_SetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_TRAINER_ID, TrainerInfo_ID(trainerInfo));
        }
    }

    string = String_Init(120, heapID);
    String_CopyChars(string, sub_02032F54(netId));
    WiFiList_SetFriendGroupName(wiFiList, slot, string);
    String_Free(string);
    WiFiList_SetFriendField(wiFiList, slot, WIFI_LIST_FRIEND_FIELD_APPEARANCE, TrainerInfo_Appearance(trainerInfo));
    CommInfo_SavePlayerRecord(saveData);
}

// Returns the friend-list slot holding the given connected player's friend
// data, or MAX_FRIENDS if the player is not in the list.
int WiFiFriend_FindSlotForNetId(SaveData *saveData, int netId)
{
    int i;
    DWCFriendData *friendData = CommInfo_DWCFriendData(netId);
    WiFiList *wiFiList = SaveData_GetWiFiList(saveData);

    for (i = 0; i < MAX_FRIENDS; i++) {
        if (DWC_IsEqualFriendData(friendData, WiFiList_GetFriendData(wiFiList, i))) {
            return i;
        }
    }

    return MAX_FRIENDS;
}
