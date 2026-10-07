#include "comm_tool.h"

#include <nitro.h>
#include <string.h>

#include "constants/heap.h"

#include "communication_system.h"
#include "heap.h"

// A single key/value entry in the per-player communication list. Entries are
// broadcast with command 19 and read back with CommList_Get.
typedef struct {
    u8 key;
    u8 value;
} CommListEntry;

#define COMM_TOOL_TEMP_DATA_SIZE 70

// Per-player communication state shared by the CommTool_*, CommTiming_* and
// CommList_* APIs. A single instance is allocated by CommTool_Init.
typedef struct {
    CommListEntry list[MAX_CONNECTED_PLAYERS]; // key/value entry per player
    u8 syncNo[MAX_CONNECTED_PLAYERS]; // last sync number reported by each player
    u8 tempData[MAX_CONNECTED_PLAYERS][COMM_TOOL_TEMP_DATA_SIZE + 2]; // received temp-data payloads
    u8 hasReceivedTempData[MAX_CONNECTED_PLAYERS]; // whether tempData holds a valid payload
    u8 syncState; // sync number all players have agreed on
    u8 syncNoPersonal; // this player's sync number, sent with command 16
    u8 sendTiming; // set while a command 16 send is pending
} CommTool;

static CommTool *sCommTool = NULL;

void CommTool_Init(enum HeapID heapID)
{
    if (!sCommTool) {
        sCommTool = Heap_Alloc(heapID, sizeof(CommTool));
        MI_CpuFill8(sCommTool, 0, sizeof(CommTool));
    }

    for (int netJd = 0; netJd < MAX_CONNECTED_PLAYERS; netJd++) {
        sCommTool->syncNo[netJd] = 0xff;
    }

    sCommTool->syncState = 0xff;
    sCommTool->syncNoPersonal = 0xff;
    sCommTool->sendTiming = 0;
}

void CommTool_Delete(void)
{
    Heap_Free(sCommTool);
    sCommTool = NULL;
}

BOOL CommTool_IsInitialized(void)
{
    if (sCommTool) {
        return 1;
    }

    return 0;
}

// Command 16: a client reports the sync number it wants to synchronize on.
// Only the server (netId 0) acts on it: it rebroadcasts the number to every
// player with command 18, records it, and once all connected players report the
// same number, broadcasts command 17 to release the barrier.
void CommCmd_16(int netId, int param1, void *param2, void *param3)
{
    u8 *buff = param2;
    u8 syncNo = buff[0];
    u8 v2[2];
    int netJd;

    if (CommSys_CurNetId() == 0) {
        v2[0] = netId;
        v2[1] = syncNo;
        CommSys_SendDataFixedSizeServer(18, &v2);

        sCommTool->syncNo[netId] = syncNo;

        for (netJd = 0; netJd < MAX_CONNECTED_PLAYERS; netJd++) {
            if (CommSys_IsPlayerConnected(netJd)) {
                if (syncNo != sCommTool->syncNo[netJd]) {
                    return;
                }
            }
        }

        CommSys_SendDataFixedSizeServer(17, &syncNo);
    }
}

// Command 18: server broadcast of (netId, syncNo); records the sync number
// reported by that player.
void CommCmd_18(int netId, int param1, void *param2, void *param3)
{
    u8 *v0 = param2;
    sCommTool->syncNo[v0[0]] = v0[1];
}

// Command 17: server broadcast that all players agreed on a sync number; this
// becomes the state observed by CommTiming_IsSyncState.
void CommCmd_17(int netId, int param1, void *param2, void *param3)
{
    u8 *v0 = param2;
    u8 v1 = v0[0];

    sCommTool->syncState = v1;
}

// Requests synchronization on `syncNo`. The request is transmitted by
// CommTiming_Update on the next frame.
void CommTiming_StartSync(u8 syncNo)
{
    sCommTool->syncNoPersonal = syncNo;
    sCommTool->sendTiming = TRUE;
}

// Per-frame pump for CommTiming_StartSync: sends command 16 with this player's
// sync number and clears the pending flag once the send succeeds.
void CommTiming_Update(void)
{
    if (sCommTool) {
        if (sCommTool->sendTiming) {
            if (CommSys_SendDataFixedSize(16, &sCommTool->syncNoPersonal)) {
                sCommTool->sendTiming = 0;
            }
        }
    }
}

// Returns TRUE once the agreed sync state equals `syncState` (or when the tool
// is not initialized, so callers can proceed without a link).
BOOL CommTiming_IsSyncState(u8 syncState)
{
    if (sCommTool == NULL) {
        return TRUE;
    }

    if (sCommTool->syncState == syncState) {
        return TRUE;
    }

    return FALSE;
}

int CommTool_GetSyncNo(int netId)
{
    return sCommTool->syncNo[netId];
}

// Command 19: stores the key/value entry sent by `netId`.
void CommList_RecvEntry(int netId, int param1, void *param2, void *param3)
{
    CommListEntry *entry = param2;

    sCommTool->list[netId].key = entry->key;
    sCommTool->list[netId].value = entry->value;
}

// Packet size of a command 19 entry.
int CommList_EntrySize(void)
{
    return sizeof(CommListEntry);
}

// Broadcasts a key/value entry to every player via command 19.
void CommList_Set(u8 key, u8 value)
{
    CommListEntry entry;

    entry.key = key;
    entry.value = value;

    CommSys_SendDataFixedSize(19, &entry);
}

// Returns the value stored for `key` by player `netId`, or -1 if the tool is
// uninitialized or the player's entry has a different key.
int CommList_Get(int netId, u8 key)
{
    if (!sCommTool) {
        return -1;
    }

    if (sCommTool->list[netId].key == key) {
        return sCommTool->list[netId].value;
    }

    return -1;
}

// Clears every player's list entry.
void CommList_Refresh(void)
{
    int i;

    for (i = 0; i < MAX_CONNECTED_PLAYERS; i++) {
        MI_CpuFill8(&sCommTool->list[i], 0, sizeof(CommListEntry));
    }
}

void CommTool_ClearReceivedTempDataAllPlayers(void)
{
    for (int i = 0; i < MAX_CONNECTED_PLAYERS; i++) {
        sCommTool->hasReceivedTempData[i] = 0;
    }
}

// Copies `data` (COMM_TOOL_TEMP_DATA_SIZE bytes) into `netId`'s temp-data slot
// and broadcasts it with command 20. Returns FALSE if the tool is not
// initialized.
BOOL CommTool_SendTempData(int netId, const void *data)
{
    if (sCommTool) {
        MI_CpuCopy8(data, sCommTool->tempData[netId], COMM_TOOL_TEMP_DATA_SIZE);
        CommSys_SendDataFixedSize(20, sCommTool->tempData[netId]);
        return 1;
    }

    return 0;
}

// Returns the temp-data payload received from `netId`, or NULL if none has
// arrived yet.
const void *CommTool_GetReceivedTempData(int netId)
{
    if (sCommTool->hasReceivedTempData[netId]) {
        return &sCommTool->tempData[netId];
    }

    return NULL;
}

// Command 20: stores the temp-data payload received from `netId`.
void CommTool_RecvTempData(int netId, int param1, void *param2, void *param3)
{
    sCommTool->hasReceivedTempData[netId] = TRUE;
    MI_CpuCopy8(param2, sCommTool->tempData[netId], COMM_TOOL_TEMP_DATA_SIZE);
}

int CommTool_TempDataSize(void)
{
    return COMM_TOOL_TEMP_DATA_SIZE;
}
