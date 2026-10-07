#ifndef POKEPLATINUM_WIFI_FRIEND_REGISTRATION_H
#define POKEPLATINUM_WIFI_FRIEND_REGISTRATION_H

#include <dwc.h>

#include "savedata.h"

// Result codes shared by WiFiFriend_FindSlot and
// WiFiFriend_FindSlotByFriendKey.
enum WiFiFriendMatch {
    // A matching slot was found. For FindSlot the friend data is identical;
    // for FindSlotByFriendKey the WFC profile ID matches.
    WIFI_FRIEND_MATCH_FOUND = 0,
    // FindSlot only: the WFC profile ID matches but the stored friend data
    // differs, so the slot holds stale data for this player.
    WIFI_FRIEND_MATCH_PROFILE,
    // No matching slot; outSlot holds the first free slot, or -1 if the list
    // is full.
    WIFI_FRIEND_MATCH_NONE,
    // The supplied friend data or friend key is not valid.
    WIFI_FRIEND_MATCH_INVALID,
};

// Controls how WiFiFriend_SavePlayerToSlot writes a connected player into a
// friend slot.
enum WiFiFriendSaveMode {
    // New friend: copy the friend data and write the player's name, gender and
    // trainer ID.
    WIFI_FRIEND_SAVE_NEW = 0,
    // Profile match: copy the friend data, but only refresh the gender and
    // trainer ID if the slot has not been registered yet.
    WIFI_FRIEND_SAVE_PROFILE,
    // Existing friend: keep the stored friend data and only refresh the group
    // name, appearance and player record.
    WIFI_FRIEND_SAVE_RECORD,
};

enum WiFiFriendMatch WiFiFriend_FindSlot(SaveData *saveData, DWCFriendData *friendData, int *outSlot);
enum WiFiFriendMatch WiFiFriend_FindSlotByFriendKey(SaveData *saveData, u64 friendKey, int *outSlot);
BOOL WiFiFriend_UpdateConnectedPlayers(SaveData *saveData, int *matchResults, enum HeapID heapID);
void WiFiFriend_SavePlayerToSlot(SaveData *saveData, int netId, int slot, enum HeapID heapID, enum WiFiFriendSaveMode mode);
int WiFiFriend_FindSlotForNetId(SaveData *saveData, int netId);

#endif // POKEPLATINUM_WIFI_FRIEND_REGISTRATION_H
