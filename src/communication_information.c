#include "communication_information.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "constants/heap.h"

#include "struct_decls/wi_fi_list.h"
#include "struct_defs/wi_fi_history.h"

#include "battle_regulation.h"
#include "communication_system.h"
#include "heap.h"
#include "record_mixed_rng.h"
#include "save_player.h"
#include "savedata.h"
#include "trainer_info.h"
#include "underground.h"
#include "comm_server_client.h"
#include "wifi_friend_registration.h"
#include "wifi_history_save_data.h"
#include "wifi_list.h"

// Tracks the identity and per-player state of everyone connected in a
// multiplayer session. Communication code broadcasts each connected player's
// CommPlayerInfo to the others; this module buffers the results and exposes
// them to the rest of the game (overworld, battles, Wi-Fi club, ...).

// The wire format broadcast for a single player. It bundles every piece of
// identity and session metadata other players need.
typedef struct CommPlayerInfo {
    u8 regulationBuffer[32]; // BattleRegulation_Copy destination
    u8 trainerInfoBuffer[32]; // backing store for trainerInfo[netId]
    DWCFriendData friendData; // built from this player's Wi-Fi user data
    u16 groupName[8]; // UNION_GROUP_NAME_LEN + 1, from the mixed-records entry
    u8 macAddress[6];
    u8 netId;
    u8 country;
    u8 region;
    u8 hasGiftPenalty; // 1 when gift exchange is disabled for this player
} CommPlayerInfo;

// Link battle / trade tally accumulated against a connected player. It is
// merged into that player's Wi-Fi friend record by CommInfo_SavePlayerRecord.
typedef struct CommPlayerRecord {
    u16 win;
    u16 lose;
    u16 trades;
} CommPlayerRecord;

// Per-player receive state machine. A player starts EMPTY. When their data
// arrives the state becomes BEGIN_RECEIVE; the game consumes it and calls
// CommInfo_MarkDataRead to move to RECEIVE; CommInfo_SetReceiveEnd moves it to
// END_RECEIVE once it has been fully handled.
enum InfoState {
    INFO_STATE_EMPTY = 0,
    INFO_STATE_BEGIN_RECEIVE,
    INFO_STATE_RECEIVE,
    INFO_STATE_END_RECEIVE,
    INFO_STATE_MAX
};

// Global session state, allocated once by CommInfo_Init. There is one entry per
// connected-player index (netId).
typedef struct CommunicationInformation {
    TrainerInfo *personalTrainerInfo; // optional override; see CommInfo_SetPersonalTrainerInfo
    const BattleRegulation *regulation; // shared regulation, or NULL to send none
    SaveData *saveData;
    CommPlayerInfo playerInfo[MAX_CONNECTED_PLAYERS];
    TrainerInfo *trainerInfo[MAX_CONNECTED_PLAYERS]; // aliases into playerInfo[].trainerInfoBuffer
    CommPlayerRecord playerRecord[MAX_CONNECTED_PLAYERS];
    u8 infoState[MAX_CONNECTED_PLAYERS]; // enum InfoState
    u8 dataFinishedReading;
    u8 dataRecvFlag; // set when remote data arrives; drives the server broadcast
    u8 curNetId; // most recently received netId
} CommunicationInformation;

static CommunicationInformation *sCommInfo;

// Allocates the global session state and seeds player 0 (the local player) from
// save data. Safe to call more than once; later calls are ignored.
void CommInfo_Init(SaveData *saveData, const BattleRegulation *regulation)
{
    int netId;
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);

    if (sCommInfo) {
        return;
    }

    sCommInfo = Heap_Alloc(HEAP_ID_COMMUNICATION, sizeof(CommunicationInformation));
    MI_CpuClear8(sCommInfo, sizeof(CommunicationInformation));

    // Point each trainerInfo slot at its backing store inside playerInfo.
    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        sCommInfo->trainerInfo[netId] = (TrainerInfo *)&sCommInfo->playerInfo[netId].trainerInfoBuffer[0];
        CommInfo_InitPlayer(netId);
    }

    sCommInfo->dataFinishedReading = FALSE;
    sCommInfo->dataRecvFlag = FALSE;
    sCommInfo->curNetId = 0;
    sCommInfo->saveData = saveData;
    sCommInfo->regulation = regulation;

    TrainerInfo_Copy(trainerInfo, sCommInfo->trainerInfo[0]);
}

void CommInfo_Delete(void)
{
    int netId;

    if (sCommInfo) {
        for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            sCommInfo->trainerInfo[netId] = NULL;
        }

        if (sCommInfo) {
            Heap_Free(sCommInfo);
        }

        sCommInfo = NULL;
    }
}

BOOL CommInfo_IsInitialized(void)
{
    return sCommInfo != NULL;
}

// Fills in this console's own CommPlayerInfo (trainer info, group name, Wi-Fi
// data, country/region and regulation) and broadcasts it to the other players
// with command 3.
void CommInfo_SendPlayerInfo(void)
{
    u16 netId = CommSys_CurNetId();
    TrainerInfo *trainerInfo;
    const u16 *groupName;
    RecordMixedRNG *recordMixedRNG = SaveData_GetRecordMixedRNG(sCommInfo->saveData);
    WiFiList *wiFiList = SaveData_GetWiFiList(sCommInfo->saveData);
    WiFiHistory *wiFiHistory = SaveData_WiFiHistory(sCommInfo->saveData);

    if (sCommInfo->personalTrainerInfo) {
        trainerInfo = sCommInfo->personalTrainerInfo;
    } else {
        trainerInfo = SaveData_GetTrainerInfo(sCommInfo->saveData);
    }

    TrainerInfo_Copy(trainerInfo, sCommInfo->trainerInfo[netId]);
    OS_GetMacAddress(&sCommInfo->playerInfo[netId].macAddress[0]);

    // Entry 1 is the group currently in use; name choice 0 is the group name.
    groupName = RecordMixedRNG_GetEntryName(recordMixedRNG, 1, 0);

    MI_CpuCopy8(groupName, sCommInfo->playerInfo[netId].groupName, sizeof(sCommInfo->playerInfo[netId].groupName));

    sCommInfo->playerInfo[netId].country = WiFiHistory_GetCountry(wiFiHistory);
    sCommInfo->playerInfo[netId].region = WiFiHistory_GetRegion(wiFiHistory);
    sCommInfo->playerInfo[netId].hasGiftPenalty = Underground_CanExchangeGifts(sCommInfo->saveData);
    // Store the inverse: a gift penalty applies when gifts cannot be exchanged.
    sCommInfo->playerInfo[netId].hasGiftPenalty = 1 - sCommInfo->playerInfo[netId].hasGiftPenalty;

    DWC_CreateExchangeToken(WiFiList_GetUserData(wiFiList), &sCommInfo->playerInfo[netId].friendData);
    MI_CpuClear8(sCommInfo->playerInfo[netId].regulationBuffer, 32);

    if (sCommInfo->regulation) {
        BattleRegulation_Copy(sCommInfo->regulation, (BattleRegulation *)sCommInfo->playerInfo[netId].regulationBuffer);
    }

    CommSys_SendData(3, &sCommInfo->playerInfo[netId], sizeof(CommPlayerInfo));
}

// Size of one player's wire entry, used by the command handlers.
int CommPlayerInfo_Size(void)
{
    return sizeof(CommPlayerInfo);
}

// Command callback signalling that the incoming data stream has been read.
void CommInfo_FinishReading(int unused0, int unused1, void *unused2, void *unused3)
{
    if (sCommInfo) {
        sCommInfo->dataFinishedReading = TRUE;
    } else {
        (void)0;
    }
}

BOOL CommInfo_IsDataFinishedReading(void)
{
    return sCommInfo->dataFinishedReading;
}

// Receives a batch of CommPlayerInfo entries (command 4), sent by the host via
// CommInfo_ServerSendArray. `src` is a single entry; the authoritative netId is
// read from the payload itself.
void CommInfo_RecvPlayerDataArray(int netId, int unused1, void *src, void *unused3)
{
    CommPlayerInfo *playerInfo = (CommPlayerInfo *)src;

    if (!sCommInfo) {
        return;
    }

    if (!CommSys_IsPlayerConnected(netId)) {
        return;
    }

    MI_CpuCopy8(src, &sCommInfo->playerInfo[playerInfo->netId], sizeof(CommPlayerInfo));
    sCommInfo->curNetId = playerInfo->netId;

    if (TrainerInfo_HasNoName(sCommInfo->trainerInfo[sCommInfo->curNetId]) == 1) {
        return;
    }

    // Only a player that has not already been marked as read (or beyond) is
    // announced; this console's own entry is considered handled immediately.
    if (sCommInfo->infoState[sCommInfo->curNetId] < INFO_STATE_RECEIVE) {
        sCommInfo->infoState[sCommInfo->curNetId] = INFO_STATE_BEGIN_RECEIVE;

        if (CommSys_CurNetId() == sCommInfo->curNetId) {
            sCommInfo->infoState[sCommInfo->curNetId] = INFO_STATE_END_RECEIVE;
        }
    }
}

// Receives one player's CommPlayerInfo sent directly (command 3). Unlike
// CommInfo_RecvPlayerDataArray, the netId comes from the caller/command
// dispatch rather than the payload.
void CommInfo_RecvPlayerData(int netId, int unused1, void *src, void *unused3)
{
    if (!sCommInfo) {
        return;
    }

    MI_CpuCopy8(src, &sCommInfo->playerInfo[netId], sizeof(CommPlayerInfo));
    CommServerClient_SetPlayerMacAddress(&sCommInfo->playerInfo[netId].macAddress[0], netId);

    sCommInfo->infoState[netId] = INFO_STATE_BEGIN_RECEIVE;

    if (CommSys_CurNetId() == netId) {
        sCommInfo->infoState[netId] = INFO_STATE_END_RECEIVE;
    } else {
        // Another player's data arrived: ask the host to rebroadcast the full
        // roster so every client learns about everyone.
        sCommInfo->dataRecvFlag = TRUE;
    }
}

// Host-side (netId 0) broadcast of the full roster. Called once new remote data
// has arrived (dataRecvFlag) and no array send is already queued. Each known
// player is queued as command 4, then command 5 marks the array complete.
BOOL CommInfo_ServerSendArray(void)
{
    int netId;

    if (!sCommInfo->dataRecvFlag) {
        return FALSE;
    }

    if (CommSys_CurNetId() != 0) {
        return FALSE;
    }

    if (!CommSys_IsCmdQueuedServer(5)) {
        for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            if (sCommInfo->infoState[netId] != INFO_STATE_EMPTY) {
                sCommInfo->playerInfo[netId].netId = netId;
                MI_CpuCopy8(sCommInfo->trainerInfo[netId], sCommInfo->playerInfo[netId].trainerInfoBuffer, TrainerInfo_Size());
                CommSys_WriteToQueueServer(4, &sCommInfo->playerInfo[netId], sizeof(CommPlayerInfo));
            }
        }

        CommSys_WriteToQueueServer(5, NULL, 0);
        sCommInfo->dataRecvFlag = FALSE;
        return TRUE;
    }

    return FALSE;
}

// True while a remote roster broadcast is pending.
BOOL CommInfo_IsReceivingData(void)
{
    return sCommInfo->dataRecvFlag;
}

// Clears one player's slot back to the EMPTY state.
void CommInfo_InitPlayer(int netId)
{
    TrainerInfo_Init(sCommInfo->trainerInfo[netId]);
    sCommInfo->infoState[netId] = INFO_STATE_EMPTY;
}

// True if this player's data has arrived but has not yet been consumed.
BOOL CommInfo_HasNewData(int netId)
{
    return sCommInfo->infoState[netId] == INFO_STATE_BEGIN_RECEIVE;
}

// True while a player's data is available to read (received but not finalized).
BOOL CommInfo_HasPlayerData(int netId)
{
    return sCommInfo->infoState[netId] == INFO_STATE_RECEIVE || sCommInfo->infoState[netId] == INFO_STATE_BEGIN_RECEIVE;
}

// True once the game has consumed this player's data.
BOOL CommInfo_IsDataRead(int netId)
{
    return sCommInfo->infoState[netId] == INFO_STATE_RECEIVE;
}

// Marks a player's received data as consumed by the game.
void CommInfo_MarkDataRead(int netId)
{
    sCommInfo->infoState[netId] = INFO_STATE_RECEIVE;
}

// Marks a player as fully handled so it is no longer counted or announced.
void CommInfo_SetReceiveEnd(int netId)
{
    sCommInfo->infoState[netId] = INFO_STATE_END_RECEIVE;
}

// Returns the first netId with unread data, or 0xff when there is none.
int CommInfo_NewNetworkId(void)
{
    int netId;

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommInfo->infoState[netId] == INFO_STATE_BEGIN_RECEIVE) {
            return netId;
        }
    }

    return 0xff;
}

// Counts players whose data has reached at least the read state.
int CommInfo_CountReceived(void)
{
    int netId;
    int count = 0;

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        switch (sCommInfo->infoState[netId]) {
        case INFO_STATE_RECEIVE:
        case INFO_STATE_END_RECEIVE:
            count++;
            break;
        }
    }

    return count;
}

// Drops the cached data of players that are no longer connected. netId 0 is
// kept while alone so the local player's own entry survives a solo session.
// Returns TRUE if at least one player was cleared.
BOOL CommInfo_ClearDisconnectedPlayers(void)
{
    int netId;
    BOOL cleared = FALSE;

    if (sCommInfo) {
        if (CommSys_ConnectedCount() == 0) {
            return cleared;
        }

        for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            if (!CommSys_IsPlayerConnected(netId)
                && !(netId == 0 && CommSys_IsAlone())
                && sCommInfo->infoState[netId] != 0) {
                CommInfo_InitPlayer(netId);
                cleared = TRUE;
            }
        }
    }

    return cleared;
}

// Returns a connected player's trainer info, or NULL if their data has not been
// received or has already been finalized.
TrainerInfo *CommInfo_TrainerInfo(int netId)
{
    if (!sCommInfo) {
        return NULL;
    }

    switch (sCommInfo->infoState[netId]) {
    case INFO_STATE_BEGIN_RECEIVE:
    case INFO_STATE_RECEIVE:
    case INFO_STATE_END_RECEIVE:
        return sCommInfo->trainerInfo[netId];
    }

    return NULL;
}

// Returns a connected player's DWC friend data, or NULL if their data has not
// been received.
DWCFriendData *CommInfo_DWCFriendData(int netId)
{
    if (sCommInfo->infoState[netId] != INFO_STATE_EMPTY) {
        return &sCommInfo->playerInfo[netId].friendData;
    }

    return NULL;
}

// Looks up the Wi-Fi friend-list slot matching the given netId's friend data,
// or MAX_FRIENDS if the player is not in the list.
int CommInfo_FindFriendSlotForNetId(int netId)
{
    return WiFiFriend_FindSlotForNetId(sCommInfo->saveData, netId);
}

// Returns a connected player's mixed-records group name, or NULL if their data
// has not been received.
u16 *CommInfo_GroupName(int netId)
{
    if (sCommInfo->infoState[netId] != 0) {
        return sCommInfo->playerInfo[netId].groupName;
    }

    return NULL;
}

// Returns a connected player's Wi-Fi country code, or 0 if unknown.
int CommInfo_PlayerCountry(int netId)
{
    if (sCommInfo->infoState[netId] != 0) {
        return sCommInfo->playerInfo[netId].country;
    }

    return 0;
}

// Returns a connected player's Wi-Fi region code, or 0 if unknown.
int CommInfo_PlayerRegion(int netId)
{
    if (sCommInfo->infoState[netId] != 0) {
        return sCommInfo->playerInfo[netId].region;
    }

    return 0;
}

// True if gift exchange with this player is penalized.
BOOL CommInfo_PlayerHasGiftPenalty(int netID)
{
    if (sCommInfo->infoState[netID] != INFO_STATE_EMPTY) {
        return sCommInfo->playerInfo[netID].hasGiftPenalty;
    }

    return FALSE;
}

// Verifies that every pair of adjacent connected players is using the same
// battle regulation. Returns TRUE when all known players agree.
BOOL CommInfo_CheckBattleRegulation(void)
{
    int netId, i;

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS - 1; netId++) {
        if (CommSys_IsPlayerConnected(netId) && (sCommInfo->infoState[netId] != 0)) {
            if (CommSys_IsPlayerConnected(netId + 1) && (sCommInfo->infoState[netId + 1] != 0)) {
                for (i = 0; i < 32; i++) {
                    if (sCommInfo->playerInfo[netId].regulationBuffer[i] != sCommInfo->playerInfo[netId + 1].regulationBuffer[i]) {
                        return FALSE;
                    }
                }
            }
        }
    }

    return TRUE;
}

// Kind of record CommInfo_UpdatePlayerRecord accumulates.
enum PlayerRecordType {
    PLAYER_RECORD_TYPE_WIN = 0,
    PLAYER_RECORD_TYPE_LOSE,
    PLAYER_RECORD_TYPE_TRADE,
};

// Adds `value` to the appropriate record of every connected player. Win and
// lose are only credited against opponents: a player's battle side is its
// battle-position parity, so players on the same parity are teammates.
static void CommInfo_UpdatePlayerRecord(int recordType, int value)
{
    int netId;
    int ownPosition, playerPosition;

    if (sCommInfo == NULL) {
        return;
    }

    if (recordType != PLAYER_RECORD_TYPE_TRADE) {
        ownPosition = CommSys_GetBattlePosition(CommSys_CurNetId()) & 0x1;
    }

    for (netId = 0; netId < CommSys_ConnectedCount(); netId++) {
        if (CommSys_IsPlayerConnected(netId) && (sCommInfo->infoState[netId] != 0)) {
            if (recordType == PLAYER_RECORD_TYPE_WIN) {
                playerPosition = CommSys_GetBattlePosition(netId) & 0x1;

                if (ownPosition != playerPosition) {
                    sCommInfo->playerRecord[netId].win += value;
                }
            } else if (recordType == PLAYER_RECORD_TYPE_LOSE) {
                playerPosition = CommSys_GetBattlePosition(netId) & 0x1;

                if (ownPosition != playerPosition) {
                    sCommInfo->playerRecord[netId].lose += value;
                }
            } else {
                sCommInfo->playerRecord[netId].trades += value;
            }
        }
    }
}

// Merges the accumulated win/lose/trade tallies into the Wi-Fi friend list,
// then clears them so they are not counted twice.
void CommInfo_SavePlayerRecord(SaveData *saveData)
{
    WiFiList *wiFiList = SaveData_GetWiFiList(saveData);
    int netId, matchResult, slot;

    for (netId = 0; netId < CommSys_ConnectedCount(); netId++) {
        DWCFriendData *friendData = CommInfo_DWCFriendData(netId);

        if (friendData == NULL) {
            continue;
        }

        matchResult = WiFiFriend_FindSlot(saveData, friendData, &slot);

        switch (matchResult) {
        case WIFI_FRIEND_MATCH_FOUND:
        case WIFI_FRIEND_MATCH_PROFILE:
            GF_ASSERT(slot >= 0);

            WiFiList_AddFriendRecord(wiFiList, slot, sCommInfo->playerRecord[netId].win, sCommInfo->playerRecord[netId].lose, sCommInfo->playerRecord[netId].trades);
            break;
        }
    }

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        sCommInfo->playerRecord[netId].win = 0;
        sCommInfo->playerRecord[netId].lose = 0;
        sCommInfo->playerRecord[netId].trades = 0;
    }
}

// Records the result of a link battle: `result` is 1 for a win or -1 for a
// loss, then persists the updated records.
void CommInfo_RecordBattleResult(SaveData *saveData, int result)
{
    if (result == 1) {
        CommInfo_UpdatePlayerRecord(PLAYER_RECORD_TYPE_WIN, 1);
    } else if (result == -1) {
        CommInfo_UpdatePlayerRecord(PLAYER_RECORD_TYPE_LOSE, 1);
    }

    CommInfo_SavePlayerRecord(saveData);
}

// Adds `count` trades against every connected player and persists the records.
void CommInfo_SetTradeResult(SaveData *saveData, int count)
{
    CommInfo_UpdatePlayerRecord(PLAYER_RECORD_TYPE_TRADE, count);
    CommInfo_SavePlayerRecord(saveData);
}

// Overrides the trainer info sent for the local player, bypassing save data.
void CommInfo_SetPersonalTrainerInfo(TrainerInfo *trainerInfo)
{
    sCommInfo->personalTrainerInfo = trainerInfo;
}
