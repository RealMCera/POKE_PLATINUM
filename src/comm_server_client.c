#include "comm_server_client.h"

#include <string.h>

#include "constants/heap.h"
#include "constants/net.h"

#include "struct_defs/struct_0203330C.h"
#include "struct_defs/struct_02034168.h"

#include "battle_regulation.h"
#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "easy_chat_sentence.h"
#include "heap.h"
#include "system.h"
#include "trainer_info.h"
#include "comm_local.h"
#include "wireless_manager.h"

// Wireless server/client manager for local (non-Wi-Fi) multiplayer. It wraps
// the WirelessManager driver and owns the list of servers discovered while
// scanning, the game info broadcast to other consoles, and the connection
// state machine driven by CommServerClient_Update.
typedef struct {
    u8 mysteryGiftEventData[84]; // Mystery Gift event data broadcast to clients (see struct_02034168)
    WMbssDesc pendingBssDesc; // Scan result waiting to be added to serverBssDesc
    WMbssDesc serverBssDesc[16]; // Servers found while scanning
    u8 playerMacAddresses[8][6]; // BSSID (MAC address) of each connected player, indexed by net ID
    u16 serverTimeouts[16]; // Frames remaining before each serverBssDesc entry expires
    void *wirelessManagerHeap; // Heap block backing the WirelessManager driver
    EasyChatSentence easyChatSentence; // Greeting broadcast with the game info
    int unk_14F4; // Set to 0 on init; purpose unknown
    u8 serverListUpdated; // Set when a server is added to or removed from serverBssDesc
    u8 unk_14F9; // Unused
    TrainerInfo *personalTrainerInfo; // This console's trainer info
    BattleRegulation *battleRegulation; // Battle regulation broadcast with the game info
    u32 ggid; // Group ID shared by all consoles in the session (0x333)
    u32 gameInfoHeap; // Unaligned heap block backing gameInfo
    u16 *gameInfo; // 32-byte-aligned game info buffer sent to the WirelessManager
    u16 measureChannel; // Channel selected by the last channel measurement
    u16 connectionTimeout; // Timestamp at which the connection attempt times out
    u8 channel; // Channel the connection is using
    u8 measureChannelHold; // Frames to keep using measureChannel before re-measuring
    u8 shutdownState; // Shutdown state: 0 running, 1 shutting down, 2 secret base closed, 3 closed
    u8 recvFunctionSet; // Whether the WirelessManager receive callback has been installed
    u8 wirelessShutdownState; // WirelessManager shutdown state machine: 0 stop scan, 1 finalize, 2 wait idle
    u8 error : 1; // A connection error has occurred
    u8 timeoutEnabled : 1; // Whether connectionTimeout is checked
    u8 errorDisconnect : 1; // Treat a disconnect as an error
    u8 incrementTGID : 1; // Increment the server TGID each time a server is started
    u8 finished : 1; // The session has finished and can be torn down
    u8 entryFlag : 1; // Entry flag passed to WirelessManager_ConnectServer
    u8 pendingBssDescValid : 1; // pendingBssDesc holds a scan result to be added
    u8 : 1;
} CommServerClient;

static void CommServerClient_InitWirelessManager(BOOL isNotListening);
static void CommServerClient_SetIncrementTGID(BOOL increment);
static void CommServerClient_ResetConnectionState(void);
static void CommServerClient_BuildGameInfo(void);
static BOOL CommServerClient_IsBssidSaved(u8 *bssid);
static u16 CommServerClient_GetBeaconPeriod(u16 commType);
static void CommServerClient_ScanCallback(WMBssDesc *bssDesc);
static void WirelessDriver_InitCallback(void *unused, WVRResult result);
static int CommServerClient_CountConnectedClients(void);

static CommServerClient *sCommServerClient = NULL;
static u16 sServerTGID = 0; // TGID assigned to the next server this console starts
static volatile int sWirelessDriverStatus;

// Allocates the manager and its buffers, then initialises the WirelessManager
// driver. Does nothing if the manager already exists.
void CommServerClient_Init(TrainerInfo *trainerInfo, BOOL isNotListening)
{
    if (sCommServerClient != NULL) {
        return;
    }

    sCommServerClient = (CommServerClient *)Heap_Alloc(HEAP_ID_COMMUNICATION, sizeof(CommServerClient));
    MI_CpuClear8(sCommServerClient, sizeof(CommServerClient));

    sCommServerClient->wirelessManagerHeap = Heap_Alloc(HEAP_ID_COMMUNICATION, WirelessManager_GetHeapSize());
    MI_CpuClear8(sCommServerClient->wirelessManagerHeap, WirelessManager_GetHeapSize());

    sCommServerClient->battleRegulation = Heap_Alloc(HEAP_ID_COMMUNICATION, BattleRegulation_Size());
    MI_CpuClear8(sCommServerClient->battleRegulation, BattleRegulation_Size());

    sCommServerClient->gameInfoHeap = (u32)Heap_Alloc(HEAP_ID_COMMUNICATION, WM_SIZE_USER_GAMEINFO + 32);
    // The game info buffer must be 32-byte aligned for the WirelessManager.
    sCommServerClient->gameInfo = (u16 *)(32 - (sCommServerClient->gameInfoHeap % 32) + sCommServerClient->gameInfoHeap);

    sCommServerClient->ggid = 0x333;
    sCommServerClient->personalTrainerInfo = trainerInfo;

    EasyChatSentence_Init((EasyChatSentence *)&sCommServerClient->easyChatSentence);
    CommServerClient_InitWirelessManager(isNotListening);
}

// TRUE once CommServerClient_Init has allocated the manager. Equivalent to
// CommServerClient_IsInitialized; kept separate for the matching build.
BOOL CommServerClient_IsAllocated(void)
{
    if (sCommServerClient) {
        return 1;
    }

    return 0;
}

static BOOL CommServerClient_CompareBytes(const u8 *a, const u8 *b, int length)
{
    const u8 *v1 = a;
    const u8 *v2 = b;

    for (int i = 0; i < length; i++) {
        if (*v1 != *v2) {
            return 0;
        }

        v1++;
        v2++;
    }

    return 1;
}

// Scan callback invoked by the WirelessManager for every beacon received while
// scanning. Stores the beacon as pendingBssDesc if its game info is compatible
// with the local comm type and contest regulation.
static void CommServerClient_ScanCallback(WMBssDesc *bssDesc)
{
    UnkStruct_0203330C *gameInfo;
    int commType = CommManager_GetCommType();
    int contestRegulation = CommManager_GetContestRegulation();

    gameInfo = (UnkStruct_0203330C *)bssDesc->gameInfo.userGameInfo;

    if (commType == 14) {
        (void)0;
    } else if (CommLocal_IsUnionGroup(gameInfo->unk_04) && CommLocal_IsUnionGroup(commType)) {
        // Both comm types are in the same "compatible group".
        (void)0;
    } else if (gameInfo->unk_54 && gameInfo->unk_04 == 10) {
        return;
    } else if (gameInfo->unk_04 != commType) {
        return;
    }

    if (commType != 14 && gameInfo->unk_05 != contestRegulation) {
        return;
    }

    MI_CpuCopy8(bssDesc, &sCommServerClient->pendingBssDesc, sizeof(WMBssDesc));
    sCommServerClient->pendingBssDescValid = 1;
}

// Moves pendingBssDesc into serverBssDesc. If the server is already known its
// timeout is refreshed; otherwise it takes the first free slot and the server
// list is marked updated.
static void CommServerClient_AddPendingServer(void)
{
    WMBssDesc *bssDesc = &sCommServerClient->pendingBssDesc;

    if (!sCommServerClient->pendingBssDescValid) {
        return;
    }

    sCommServerClient->pendingBssDescValid = 0;
    int i;

    for (i = 0; i < 16; ++i) {
        if (sCommServerClient->serverTimeouts[i] == 0) {
            continue;
        }

        if (CommServerClient_CompareBytes(sCommServerClient->serverBssDesc[i].bssid, bssDesc->bssid, WM_SIZE_BSSID)) {
            sCommServerClient->serverTimeouts[i] = (30 * 10);
            MI_CpuCopy8(bssDesc, &sCommServerClient->serverBssDesc[i], sizeof(WMBssDesc));
            return;
        }
    }

    for (i = 0; i < 16; ++i) {
        if (sCommServerClient->serverTimeouts[i] == 0) {
            break;
        }
    }

    if (i >= 16) {
        return;
    }

    sCommServerClient->serverTimeouts[i] = (30 * 10);
    MI_CpuCopy8(bssDesc, &sCommServerClient->serverBssDesc[i], sizeof(WMBssDesc));
    sCommServerClient->serverListUpdated = 1;
}

static void WirelessDriver_InitCallback(void *unused, WVRResult result)
{
    if (result != WVR_RESULT_SUCCESS) {
        OS_Terminate();
    } else {
        (void)0;
    }

    sWirelessDriverStatus = WIRELESS_DRIVER_STATUS_CONNECTED;
}

static void WirelessDriver_ShutdownCallback(void *unused, WVRResult result)
{
    sWirelessDriverStatus = WIRELESS_DRIVER_STATUS_DISCONNECTED;
    SleepUnlock(4);
}

// Starts the ARM7 wireless driver asynchronously. Blocks the calling thread on
// sleep lock 4 until WirelessDriver_ShutdownCallback releases it.
void WirelessDriver_Init(void)
{
    SleepLock(4);

    sWirelessDriverStatus = WIRELESS_DRIVER_STATUS_CONNECTING;

    if (WVR_RESULT_OPERATING != WVR_StartUpAsync(GX_VRAM_ARM7_128_D, WirelessDriver_InitCallback, NULL)) {
        OS_Terminate();
    } else {
        (void)0;
    }
}

BOOL WirelessDriver_IsReady(void)
{
    return sWirelessDriverStatus == WIRELESS_DRIVER_STATUS_CONNECTED;
}

BOOL WirelessDriver_Initialized(void)
{
    return sWirelessDriverStatus != WIRELESS_DRIVER_STATUS_DISCONNECTED;
}

void WirelessDriver_Shutdown(void)
{
    WVR_TerminateAsync(WirelessDriver_ShutdownCallback, NULL);
}

// Initialises the WirelessManager driver on the 32-byte-aligned portion of its
// heap block and sets the shared group ID.
static void CommServerClient_InitWirelessManager(BOOL isNotListening)
{
    sCommServerClient->unk_14F4 = 0;
    u32 heap = (u32)sCommServerClient->wirelessManagerHeap;

    heap = 32 - (heap % 32) + heap;
    (void)WirelessManager_Initialize((void *)heap, isNotListening);

    WirelessManager_SetParentParamGGID(sCommServerClient->ggid);
}

// Forgets every discovered server.
void CommServerClient_ClearScanResults(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        sCommServerClient->serverTimeouts[i] = 0;
    }

    MI_CpuClear8(sCommServerClient->serverBssDesc, sizeof(WMBssDesc) * 16);
}

static void CommServerClient_SetIncrementTGID(BOOL increment)
{
    sCommServerClient->incrementTGID = increment;
}

// Resets the per-connection flags and state machines before a new server or
// client session starts.
static void CommServerClient_ResetConnectionState(void)
{
    sCommServerClient->serverListUpdated = 0;
    sCommServerClient->error = 0;
    sCommServerClient->errorDisconnect = 0;
    sCommServerClient->shutdownState = 0;
    sCommServerClient->finished = 0;
    sCommServerClient->wirelessShutdownState = 0;
    sCommServerClient->recvFunctionSet = 0;
}

// Prepares the manager to act as a server. incrementTGID controls whether the
// TGID is bumped on each server start; entryFlag is forwarded to
// WirelessManager_ConnectServer.
BOOL CommServerClient_InitServer(BOOL param0, BOOL incrementTGID, BOOL entryFlag)
{
    CommServerClient_ResetConnectionState();
    CommServerClient_SetIncrementTGID(incrementTGID);
    WirelessManager_ResetBeaconSentCount();

    if (!sCommServerClient->recvFunctionSet) {
        WirelessManager_SetRecvFunction(CommSys_ServerRecvCallback, 14);
        sCommServerClient->recvFunctionSet = 1;
    }

    sCommServerClient->entryFlag = entryFlag;

    if (WirelessManager_GetState() == 1) {
        if (WirelessManager_StartMeasureChannel()) {
            return 1;
        }
    }

    return 0;
}

// Prepares the manager to act as a client and starts scanning for servers.
// clearScanResults discards any previously discovered servers first.
BOOL CommServerClient_InitClient(BOOL param0, BOOL clearScanResults)
{
    CommServerClient_ResetConnectionState();

    if (clearScanResults) {
        CommServerClient_ClearScanResults();
    }

    if (!sCommServerClient->recvFunctionSet) {
        WirelessManager_SetRecvFunction(CommSys_ClientRecvCallback, 14);
        sCommServerClient->recvFunctionSet = 1;
    }

    if (WirelessManager_GetState() == 1) {
        const u8 v0[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

        if (WirelessManager_ConnectClientScanCallback(CommServerClient_ScanCallback, v0, 0)) {
            return 1;
        }
    }

    return 0;
}

// Steps the WirelessManager shutdown state machine. Returns TRUE once the
// driver is idle and can be reinitialised.
BOOL CommServerClient_ShutdownWirelessManager(void)
{
    if (!sCommServerClient) {
        return 1;
    }

    switch (sCommServerClient->wirelessShutdownState) {
    case 0:
        if (WirelessManager_IsScanning()) {
            WirelessManager_StopScan();
            sCommServerClient->wirelessShutdownState = 1;
            break;
        }
        if (WirelessManager_IsBusy()) {
            (void)0;
        } else {
            WirelessManager_Finalize();
            sCommServerClient->wirelessShutdownState = 2;
        }
        break;
    case 1:
        if (!WirelessManager_IsBusy()) {
            WirelessManager_Finalize();
            sCommServerClient->wirelessShutdownState = 2;
        }
        break;
    case 2:
        if (WirelessManager_IsIdle()) {
            return 1;
        }

        if (WirelessManager_IsError()) {
            sCommServerClient->wirelessShutdownState = 1;
        }
        break;
    }

    return 0;
}

// Begins shutting the manager down. Returns TRUE if the shutdown was started,
// FALSE if it was already in progress.
BOOL CommServerClient_Shutdown(void)
{
    if (sCommServerClient) {
        if (sCommServerClient->shutdownState == 0) {
            sCommServerClient->shutdownState = 1;
            WirelessManager_Finalize();
            return 1;
        }
    }

    return 0;
}

// Marks the secret base as closed (or reopens it). While closed, the manager
// tears down its wireless connection and refuses new ones.
void CommServerClient_SetSecretBaseClosedState(BOOL isClosed)
{
    if (!sCommServerClient) {
        return;
    }

    if (isClosed) {
        sCommServerClient->shutdownState = 2;
    } else {
        sCommServerClient->shutdownState = 0;
        CommServerClient_InitWirelessManager(1);
    }
}

static void CommServerClient_Free(void)
{
    Heap_Free(sCommServerClient->battleRegulation);
    Heap_Free(sCommServerClient->wirelessManagerHeap);
    Heap_Free((void *)sCommServerClient->gameInfoHeap);
    Heap_Free(sCommServerClient);

    sCommServerClient = NULL;
}

// Number of servers currently in serverBssDesc.
int CommServerClient_CountDiscoveredServers(void)
{
    if (!CommSys_IsInitialized()) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i < 16; ++i) {
        if (sCommServerClient->serverTimeouts[i] != 0) {
            count++;
        }
    }

    return count;
}

// Returns the serverBssDesc index of the index-th discovered server.
int CommServerClient_GetNthServerIndex(int index)
{
    int i, count = 0;

    for (i = 0; i < 16; i++) {
        if (sCommServerClient->serverTimeouts[i] != 0) {
            if (count == index) {
                return i;
            }

            count++;
        }
    }

    GF_ASSERT(FALSE);
    return 0;
}

BOOL CommServerClient_IsServerListUpdated(void)
{
    return sCommServerClient->serverListUpdated;
}

void CommServerClient_ClearServerListUpdated(void)
{
    sCommServerClient->serverListUpdated = 0;
}

// Number of players in the server at the given index, as reported in its game
// info. A server always counts as at least one player.
int CommServerClient_GetServerPlayerCount(int index)
{
    if (sCommServerClient->serverTimeouts[index] != 0) {
        UnkStruct_0203330C *gameInfo = (UnkStruct_0203330C *)sCommServerClient->serverBssDesc[index].gameInfo.userGameInfo;

        if (gameInfo->unk_06 == 0) {
            return 1;
        }

        return gameInfo->unk_06;
    }

    return 0;
}

// Finds the highest-indexed server with more than minPlayers players (and
// fewer than the maximum of 8), or -1 if there is none.
static int CommServerClient_FindServerWithMorePlayers(int minPlayers)
{
    for (int i = 16 - 1; i >= 0; i--) {
        int playerCount = CommServerClient_GetServerPlayerCount(i);

        if (playerCount > minPlayers && playerCount < (7 + 1)) {
            return i;
        }
    }

    return -1;
}

// Finds a server this console has connected to before (its BSSID is saved) and
// that has room for more players, or -1 if there is none.
int CommServerClient_FindServerToJoin(void)
{
    if (CommServerClient_CountDiscoveredServers() == 0) {
        return -1;
    }

    for (int i = 16 - 1; i >= 0; i--) {
        if (sCommServerClient->serverTimeouts[i] != 0) {
            if (CommServerClient_IsBssidSaved(&sCommServerClient->serverBssDesc[i].bssid[0])) {
                int playerCount = CommServerClient_GetServerPlayerCount(i);

                if (playerCount > 1 && playerCount < (7 + 1)) {
                    return i;
                }
            }
        }
    }

    return -1;
}

// Finds any server this console has connected to before, falling back to the
// fullest server with room for more players.
int CommServerClient_FindAnyServer(void)
{
    if (CommServerClient_CountDiscoveredServers() == 0) {
        return -1;
    }

    int index;

    for (index = 16 - 1; index >= 0; index--) {
        if (sCommServerClient->serverTimeouts[index] != 0) {
            if (CommServerClient_IsBssidSaved(&sCommServerClient->serverBssDesc[index].bssid[0])) {
                return index;
            }
        }
    }

    index = CommServerClient_FindServerWithMorePlayers(1);

    if (index != -1) {
        return index;
    }

    index = CommServerClient_FindServerWithMorePlayers(0);

    if (index != -1) {
        return index;
    }

    return index;
}

// Copies the trainer info of the index-th discovered server into dest.
void CommServerClient_CopyServerTrainerInfo(int index, TrainerInfo *dest)
{
    int i, count = 0;

    for (i = 0; i < 16; ++i) {
        if (sCommServerClient->serverTimeouts[i] != 0) {
            if (index == count) {
                TrainerInfo_Copy(CommServerClient_GetServerTrainerInfo(i), dest);
                return;
            }

            count++;
        }
    }
}

// Connects to the server at the given serverBssDesc index. Stops scanning
// first if a scan is in progress.
BOOL CommServerClient_ConnectToServer(u16 index)
{
    if (WirelessManager_GetState() == 2) {
        (void)WirelessManager_StopScan();
        return 0;
    }

    if (WirelessManager_GetState() == 1) {
        int commType = CommManager_GetCommType();
        sCommServerClient->channel = sCommServerClient->serverBssDesc[index].channel;

        if (CommLocal_IsUnionGroup(commType)) {
            WirelessManager_ConnectClientAuto(1, sCommServerClient->serverBssDesc[index].bssid, 0);
        } else {
            WirelessManager_ConnectClient(1, &sCommServerClient->serverBssDesc[index]);
        }

        return 1;
    }

    return 0;
}

// Ages the discovered servers: adds any pending scan result and decrements the
// timeout of every known server, marking the list updated when one expires.
void CommServerClient_UpdateServerList(void)
{
    CommServerClient_AddPendingServer();

    for (int i = 0; i < 16; i++) {
        if (sCommServerClient->serverTimeouts[i] == 0) {
            continue;
        }

        if (sCommServerClient->serverTimeouts[i] > 0) {
            sCommServerClient->serverTimeouts[i]--;

            if (sCommServerClient->serverTimeouts[i] == 0) {
                sCommServerClient->serverListUpdated = 1;
            }
        }
    }
}

// Fills the game info buffer with this console's trainer info, battle
// regulation and greeting. Comm type 15 (Mystery Gift) uses a different layout
// that carries the Mystery Gift event data instead.
static void CommServerClient_BuildGameInfo(void)
{
    int commType = CommManager_GetCommType();

    TrainerInfo *trainerInfo = CommServerClient_GetPersonalTrainerInfo();

    if (commType != 15) {
        UnkStruct_0203330C *gameInfo = (UnkStruct_0203330C *)sCommServerClient->gameInfo;

        GF_ASSERT(32 >= BattleRegulation_Size());
        GF_ASSERT(32 == TrainerInfo_Size());
        GF_ASSERT(WM_SIZE_USER_GAMEINFO >= MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)));

        MI_CpuCopy8(trainerInfo, gameInfo->unk_10, TrainerInfo_Size());
        MI_CpuCopy8(sCommServerClient->battleRegulation, gameInfo->unk_30, BattleRegulation_Size());

        gameInfo->unk_00 = TrainerInfo_ID(trainerInfo);
        gameInfo->unk_04 = CommManager_GetCommType();
        gameInfo->unk_05 = CommManager_GetContestRegulation();

        MI_CpuCopy8(&sCommServerClient->easyChatSentence, &gameInfo->unk_08, sizeof(EasyChatSentence));

        gameInfo->unk_54 = WirelessManager_GetPauseConnection();
    } else {
        UnkStruct_02034168 *mysteryGiftInfo = (UnkStruct_02034168 *)sCommServerClient->gameInfo;

        mysteryGiftInfo->unk_00 = TrainerInfo_ID(trainerInfo);
        mysteryGiftInfo->unk_04 = CommManager_GetCommType();
        mysteryGiftInfo->unk_05 = CommManager_GetContestRegulation();

        MI_CpuCopy8(sCommServerClient->mysteryGiftEventData, mysteryGiftInfo->unk_08, 84);
    }

    DC_FlushRange(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)));
    WirelessManager_SetParentParamGameInfoAndLength(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)));
}

// Publishes the current connected-client count in the game info so other
// consoles can see how full this server is.
static void CommServerClient_UpdatePlayerCount(void)
{
    UnkStruct_0203330C *gameInfo = (UnkStruct_0203330C *)sCommServerClient->gameInfo;

    if (CommServerClient_CountConnectedClients() != gameInfo->unk_06) {
        gameInfo->unk_06 = CommServerClient_CountConnectedClients();
        DC_FlushRange(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)));
        WirelessManager_SetParentParamGameInfoAndLength(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)));
        WirelessManager_SetGameInfo(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)), sCommServerClient->ggid, sServerTGID);
    }
}

// Per-frame connection state machine. timestamp is the current frame counter,
// used to time out connection attempts. Handles error detection, the secret
// base shutdown states, and starting a server once a channel has been measured.
static void CommServerClient_UpdateConnection(u16 timestamp)
{
    int state = WirelessManager_GetState();
    int recvFinished = CommInfo_ClearDisconnectedPlayers();

    CommServerClient_UpdatePlayerCount();

    // A server (AID 0) with no clients connected and error-disconnect enabled
    // reports an error.
    if (WirelessManager_GetAID() == 0 && !CommServerClient_IsClientConnecting()) {
        if (sCommServerClient->errorDisconnect) {
            sCommServerClient->error = 1;
        }
    }

    if (sCommServerClient->connectionTimeout == 0xffff) {
        sCommServerClient->connectionTimeout = timestamp;
    }

    if (sCommServerClient->timeoutEnabled) {
        if (sCommServerClient->connectionTimeout > timestamp) {
            sCommServerClient->error = 1;
        }

        if (recvFinished) {
            sCommServerClient->error = 1;
        }
    }

    if (25 == WirelessManager_GetErrorCode()) {
        NetworkError_DisplayFatalError(0);
    }

    switch (state) {
    case 0:
        if (sCommServerClient->shutdownState == 1) {
            CommServerClient_Free();
            return;
        }

        if (sCommServerClient->shutdownState == 2) {
            sCommServerClient->shutdownState = 3;
            return;
        }
        break;
    case 1:
        if (sCommServerClient->shutdownState == 1) {
            if (WirelessManager_End()) {
                return;
            }
        }

        if (sCommServerClient->shutdownState == 2) {
            if (WirelessManager_End()) {
                return;
            }
        }
        break;
    case 8:
    case 9:
        // Bad connection or error: flag it so the session is torn down.
        if (sCommServerClient) {
            sCommServerClient->error = 1;
        }
        break;
    case 7: {
        u16 channel;

        channel = WirelessManager_GetMeasureChannel();

        // Hold the measured channel for a few frames before re-measuring.
        if (sCommServerClient->measureChannelHold == 0) {
            sCommServerClient->measureChannel = channel;
            sCommServerClient->measureChannelHold = 5;
        } else {
            sCommServerClient->measureChannelHold--;
        }

        channel = sCommServerClient->measureChannel;

        if (sCommServerClient->incrementTGID) {
            sServerTGID++;
        }

        CommServerClient_BuildGameInfo();
        (void)WirelessManager_ConnectServer(0, sServerTGID, channel, CommLocal_MaxMachines(CommManager_GetCommType()), CommServerClient_GetBeaconPeriod(CommManager_GetCommType()), sCommServerClient->entryFlag);
        sCommServerClient->channel = channel;
    } break;
    default:
        break;
    }
}

void CommServerClient_Update(u16 timestamp)
{
    if (sCommServerClient) {
        CommServerClient_UpdateConnection(timestamp);
    }
}

// TRUE if the client with the given net ID is present in the connection bitmap.
static BOOL CommServerClient_IsClientConnected(u16 netId)
{
    if (!sCommServerClient) {
        return 0;
    }

    if (WirelessManager_GetState() != 4) {
        return 0;
    }

    {
        u16 bitmap = WirelessManager_GetConnectedBitmap();

        if (bitmap & (1 << netId)) {
            return 1;
        }
    }
    return 0;
}

// Counts the connected clients (net IDs 0-7).
static int CommServerClient_CountConnectedClients(void)
{
    int count = 0, netId;

    for (netId = 0; netId < (7 + 1); netId++) {
        if (CommServerClient_IsClientConnected(netId)) {
            count++;
        }
    }

    return count;
}

BOOL CommServerClient_IsInClosedSecretBase(void)
{
    if (sCommServerClient && (sCommServerClient->shutdownState == 3)) {
        return TRUE;
    }

    return FALSE;
}

BOOL CommServerClient_IsInitialized(void)
{
    return sCommServerClient != NULL;
}

// TRUE if the WirelessManager is idle (or the manager does not exist).
BOOL CommServerClient_IsIdle(void)
{
    if (sCommServerClient) {
        return WirelessManager_IsIdle();
    }

    return 1;
}

// TRUE if any client other than net ID 0 is connected.
BOOL CommServerClient_IsClientConnecting(void)
{
    if (sCommServerClient) {
        return WirelessManager_GetConnectedBitmap() & 0xfffe;
    }

    return 0;
}

// TRUE if an error has been flagged and the WirelessManager reports a
// disconnect (error code 20).
BOOL CommServerClient_IsDisconnected(void)
{
    if (CommServerClient_CheckError() && (20 == WirelessManager_GetErrorCode())) {
        return 1;
    }

    return 0;
}

BOOL CommServerClient_CheckError(void)
{
    if (sCommServerClient) {
        if (sCommServerClient->error) {
            return 1;
        }
    }

    return 0;
}

void CommServerClient_SetErrorDisconnect(BOOL errorDisconnect)
{
    if (sCommServerClient) {
        sCommServerClient->errorDisconnect = errorDisconnect;
    }
}

// Enables or disables the connection timeout check and restarts the timeout
// window.
void CommServerClient_SetErrorTimeout(BOOL enabled)
{
    if (sCommServerClient) {
        sCommServerClient->timeoutEnabled = enabled;
        sCommServerClient->connectionTimeout = 0xffff;
    }
}

// Beacon period for the given comm type. Some comm types use a quarter of the
// default dispersion period to be found faster.
u16 CommServerClient_GetBeaconPeriod(u16 commType)
{
    u16 beaconPeriod = WM_GetDispersionBeaconPeriod();

    GF_ASSERT(commType < 37);

    if (10 == commType) {
        return beaconPeriod / 4;
    }

    if (9 == commType || 13 == commType) {
        return beaconPeriod / 4;
    }

    return beaconPeriod;
}

WMBssDesc *CommServerClient_GetServerBssDesc(int index)
{
    if (sCommServerClient && (sCommServerClient->serverTimeouts[index] != 0)) {
        return &sCommServerClient->serverBssDesc[index];
    }

    return NULL;
}

UnkStruct_0203330C *CommServerClient_GetServerGameInfo(int index)
{
    if (sCommServerClient && sCommServerClient->serverTimeouts[index] != 0) {
        return (UnkStruct_0203330C *)sCommServerClient->serverBssDesc[index].gameInfo.userGameInfo;
    }

    return NULL;
}

TrainerInfo *CommServerClient_GetPersonalTrainerInfo(void)
{
    return sCommServerClient->personalTrainerInfo;
}

TrainerInfo *CommServerClient_GetServerTrainerInfo(int index)
{
    if (sCommServerClient->serverTimeouts[index] == 0) {
        return NULL;
    }

    UnkStruct_0203330C *gameInfo = (UnkStruct_0203330C *)sCommServerClient->serverBssDesc[index].gameInfo.userGameInfo;
    TrainerInfo *trainerInfo = (TrainerInfo *)&gameInfo->unk_10[0];

    return trainerInfo;
}

// Records the MAC address of the player with the given net ID so this console
// can recognise servers it has connected to before.
void CommServerClient_SetPlayerMacAddress(u8 *macAddress, int netId)
{
    if (sCommServerClient) {
        GF_ASSERT(netId < (7 + 1));
        MI_CpuCopy8(macAddress, sCommServerClient->playerMacAddresses[netId], WM_SIZE_BSSID);
    }
}

static BOOL CommServerClient_IsBssidSaved(u8 *bssid)
{
    for (int i = 0; i < (7 + 1); i++) {
        if (WM_IsBssidEqual(sCommServerClient->playerMacAddresses[i], bssid)) {
            return 1;
        }
    }

    return 0;
}

BOOL CommServerClient_IsFinished(void)
{
    if (sCommServerClient) {
        return sCommServerClient->finished;
    }

    return 0;
}

void CommServerClient_SetFinished(void)
{
    if (sCommServerClient) {
        sCommServerClient->finished = 1;
    }
}

void CommServerClient_SetEasyChatSentence(EasyChatSentence *sentence)
{
    MI_CpuCopy8(sentence, &sCommServerClient->easyChatSentence, sizeof(EasyChatSentence));
}

void CommServerClient_SetBattleRegulation(void *regulation)
{
    MI_CpuCopy8(regulation, sCommServerClient->battleRegulation, BattleRegulation_Size());
}

void *CommServerClient_GetBattleRegulation(void)
{
    return sCommServerClient->battleRegulation;
}

// Rebuilds the game info and pushes it to the WirelessManager.
void CommServerClient_SendGameInfo(void)
{
    CommServerClient_BuildGameInfo();
    WirelessManager_SetGameInfo(sCommServerClient->gameInfo, MATH_MAX(sizeof(UnkStruct_02034168), sizeof(UnkStruct_0203330C)), sCommServerClient->ggid, sServerTGID);
}

// Sums the player counts of all discovered servers whose comm type matches.
int CommServerClient_CountPlayersByCommType(int commType)
{
    int i, count = 0;

    for (i = 0; i < 16; i++) {
        UnkStruct_0203330C *gameInfo = CommServerClient_GetServerGameInfo(i);

        if (gameInfo) {
            if (gameInfo->unk_04 == commType) {
                count += gameInfo->unk_06;
            }
        }
    }

    return count;
}

BOOL CommServerClient_ServerSentAllBeacons(void)
{
    return WirelessManager_ServerSentAllBeacons();
}

// Stores the Mystery Gift event data to broadcast and refreshes the game info.
void CommServerClient_SetMysteryGiftEventData(void *eventData)
{
    MI_CpuCopy8(eventData, sCommServerClient->mysteryGiftEventData, 84);
    CommServerClient_SendGameInfo();
}

const void *CommServerClient_GetMysteryGiftEventData(int index)
{
    if (sCommServerClient && sCommServerClient->serverTimeouts[index] != 0) {
        UnkStruct_02034168 *mysteryGiftInfo = (UnkStruct_02034168 *)sCommServerClient->serverBssDesc[index].gameInfo.userGameInfo;
        return mysteryGiftInfo->unk_08;
    }

    return NULL;
}
