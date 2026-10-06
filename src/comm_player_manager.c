#include "comm_player_manager.h"

#include <nitro.h>
#include <nitro/os.h>
#include <nnsys/g3d/glbstate.h>
#include <string.h>

#include "constants/communication/comm_packets.h"
#include "constants/heap.h"
#include "generated/movement_actions.h"
#include "generated/trainer_score_events.h"

#include "struct_decls/map_object.h"
#include "struct_defs/comm_player_location.h"
#include "struct_defs/struct_020590C4.h"

#include "field/field_system.h"
#include "overlay005/ov5_021F55CC.h"
#include "underground/comm_manager.h"
#include "underground/manager.h"
#include "underground/mining.h"
#include "underground/player.h"
#include "underground/player_status.h"
#include "underground/secret_bases.h"
#include "underground/traps.h"

#include "comm_manager.h"
#include "communication_information.h"
#include "communication_system.h"
#include "field_system.h"
#include "game_records.h"
#include "heap.h"
#include "location.h"
#include "map_header_data.h"
#include "map_object.h"
#include "map_object_move.h"
#include "player_avatar.h"
#include "player_move.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "system.h"
#include "terrain_collision_manager.h"
#include "trainer_info.h"
#include "tv_segment.h"
#include "underground.h"
#include "map_object_animation.h"

// Manages the avatars and positions of every player in a link session. The
// server (netId 0) owns the authoritative positions in playerLocationServer
// and simulates movement for all players; clients send their own position and
// receive the server's view. The manager also drives the battle-room grid
// handshake and the underground-specific per-player state.

static int CommPlayerMan_GetSlideMovementSpeed(int param0, int param1);
static BOOL CommPlayer_MoveSlide(int netId, int param1);
static BOOL CommPlayer_SlideAnimation(int netId, int param1, int unused, int animSpeed);
static void CommPlayer_SendDataTask(void *data);
static void CommPlayer_SendChangedPositions(void *data);
static void CommPlayer_Add(u8 netId);
static void CommPlayer_Move(SysTask *unused0, void *unused1);
static void Task_CommPlayerManagerRun(SysTask *task, void *data);
static void CommPlayer_UpdateClients(void *unused);
static void CommPlayer_MoveClient(int netId);
static void CommPlayerMan_CheckBattleGridServer(void);

static CommPlayerManager *sCommPlayerManager = NULL;

CommPlayerManager *CommPlayerMan_Get(void)
{
    return sCommPlayerManager;
}

BOOL CommPlayerMan_Init(void *dest, FieldSystem *fieldSystem, BOOL isUnderground)
{
    if (sCommPlayerManager != NULL) {
        return FALSE;
    }

    if (sCommPlayerManager == NULL) {
        sCommPlayerManager = dest;
        MI_CpuFill8(sCommPlayerManager, 0, sizeof(CommPlayerManager));
    }

    sCommPlayerManager->isUnderground = isUnderground;
    sCommPlayerManager->playerStatuses = NULL;

    if (isUnderground) {
        sCommPlayerManager->playerStatuses = Heap_Alloc(HEAP_ID_COMMUNICATION, UndergroundPlayerStatuses_Size());
        UndergroundPlayerStatuses_Init(sCommPlayerManager->playerStatuses);
    }

    sCommPlayerManager->fieldSystem = fieldSystem;

    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        sCommPlayerManager->playerLocation[netId].dir = DIR_NONE;
        sCommPlayerManager->playerLocation[netId].x = 0xffff;
        sCommPlayerManager->playerLocation[netId].z = 0xffff;
        sCommPlayerManager->playerLocation[netId].moveSpeed = 2;
        sCommPlayerManager->playerLocationServer[netId].dir = DIR_NONE;
        sCommPlayerManager->playerLocationServer[netId].x = 0xffff;
        sCommPlayerManager->playerLocationServer[netId].z = 0xffff;
        sCommPlayerManager->playerLocationServer[netId].moveSpeed = 2;
        sCommPlayerManager->slideAnimationDir[netId] = DIR_NONE;
        sCommPlayerManager->movementEnabled[netId] = FALSE;
        sCommPlayerManager->movementEnabled2[netId] = TRUE;
        sCommPlayerManager->onBattleGrid[netId] = 0;
        sCommPlayerManager->heldFlagInfo[netId].netID = NETID_NONE;
    }

    sCommPlayerManager->isFieldSystemActive = FALSE;
    sCommPlayerManager->resumeHandled = 0;
    sCommPlayerManager->task = SysTask_Start(Task_CommPlayerManagerRun, NULL, 100 + 100);

    CommSys_EnableSendMovementData();
    CommPlayer_InitPersonal();

    return TRUE;
}

// Tears down every player's avatar. Underground keeps the manager around (it
// is only marked disabled) so it can be restarted when reconnecting.
void CommPlayerMan_Disable(void)
{
    if (sCommPlayerManager == NULL) {
        return;
    }

    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->isUnderground) {
            CommPlayer_Destroy(netId, TRUE, FALSE);
        } else {
            CommPlayer_Destroy(netId, TRUE, TRUE);
        }
    }

    if (sCommPlayerManager->isUnderground) {
        sCommPlayerManager->isDisabled = TRUE;
    }
}

void CommPlayerMan_Restart(void)
{
    if (sCommPlayerManager == NULL) {
        return;
    }

    sCommPlayerManager->sendAllPos = TRUE;
    sCommPlayerManager->isDisabled = FALSE;

    CommPlayer_InitPersonal();
    CommPlayer_SendPos(TRUE);
}

// Frees the manager and all owned data. deletePlayerData controls whether the
// underlying player save data is also released.
void CommPlayerMan_Delete(BOOL deletePlayerData)
{
    if (sCommPlayerManager != NULL) {
        for (int i = 0; i < MAX_CAPTURED_FLAG_RECORDS; i++) {
            if (sCommPlayerManager->registeredFlagOwnerInfoUnused[i]) {
                Heap_Free(sCommPlayerManager->registeredFlagOwnerInfoUnused[i]);
                sCommPlayerManager->registeredFlagOwnerInfoUnused[i] = NULL;
            }
        }

        if (sCommPlayerManager->flagsRegisteredInCurrentSession >= 5) {
            sub_0206DF60(sCommPlayerManager->fieldSystem, sCommPlayerManager->flagsRegisteredInCurrentSession);
        }

        for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            CommPlayer_Destroy(netId, FALSE, deletePlayerData);
        }

        SysTask_Done(sCommPlayerManager->task);

        if (sCommPlayerManager->playerStatuses) {
            UndergroundPlayerStatuses_Dummy(sCommPlayerManager->playerStatuses);
            Heap_Free(sCommPlayerManager->playerStatuses);
        }

        Heap_Free(sCommPlayerManager);
        sCommPlayerManager = NULL;
    }
}

void CommPlayerMan_Reinit(void)
{
    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->isUnderground) {
            UndergroundPlayer_StopDisplayingEmote(netId);
            UndergroundPlayer_RevertStatusToNormal(netId);

            if (netId != 0) {
                UndergroundPlayer_DeleteHeldFlagDataClient(netId);
            }
        }

        if (sCommPlayerManager->playerAvatar[netId] != NULL) {
            if (sCommPlayerManager->fieldSystem->playerAvatar != sCommPlayerManager->playerAvatar[netId]) {
                if (sCommPlayerManager->isUnderground) {
                    PlayerAvatar_Delete(sCommPlayerManager->playerAvatar[netId]);
                }
            }

            sCommPlayerManager->playerAvatar[netId] = NULL;
        }

        sCommPlayerManager->isActive[netId] = FALSE;
        sCommPlayerManager->playerLocation[netId].dir = DIR_NONE;
        sCommPlayerManager->playerLocation[netId].x = 0xffff;
        sCommPlayerManager->playerLocation[netId].z = 0xffff;
        sCommPlayerManager->playerLocation[netId].moveSpeed = 2;
        sCommPlayerManager->playerLocationServer[netId].dir = DIR_NONE;
        sCommPlayerManager->playerLocationServer[netId].x = 0xffff;
        sCommPlayerManager->playerLocationServer[netId].z = 0xffff;
        sCommPlayerManager->playerLocationServer[netId].moveSpeed = 2;
        sCommPlayerManager->slideAnimationDir[netId] = DIR_NONE;
        sCommPlayerManager->slideTilesLeft[netId] = 0;
        sCommPlayerManager->movementEnabled[netId] = FALSE;
        sCommPlayerManager->movementEnabled2[netId] = TRUE;
        sCommPlayerManager->onBattleGrid[netId] = 0;
        sCommPlayerManager->moveTimerServer[netId] = 0;
        sCommPlayerManager->moveTimer[netId] = 0;
    }

    sCommPlayerManager->resumeHandled = 0;
    CommPlayer_InitPersonal();

    if (sCommPlayerManager->task == NULL) {
        sCommPlayerManager->task = SysTask_Start(Task_CommPlayerManagerRun, NULL, 100 + 100);
    }
}

void CommPlayerMan_Stop(void)
{
    if (sCommPlayerManager->task) {
        Task_CommPlayerManagerRun(NULL, NULL);
        SysTask_Done(sCommPlayerManager->task);
    }

    sCommPlayerManager->task = NULL;
}

// Registers the local player's avatar and seeds both position copies from it.
void CommPlayer_InitPersonal(void)
{
    sCommPlayerManager->playerAvatar[CommSys_CurNetId()] = sCommPlayerManager->fieldSystem->playerAvatar;
    sCommPlayerManager->isActive[CommSys_CurNetId()] = TRUE;
    sCommPlayerManager->playerLocation[CommSys_CurNetId()].x = PlayerAvatar_GetXPos(sCommPlayerManager->fieldSystem->playerAvatar);
    sCommPlayerManager->playerLocation[CommSys_CurNetId()].z = PlayerAvatar_GetZPos(sCommPlayerManager->fieldSystem->playerAvatar);
    sCommPlayerManager->playerLocation[CommSys_CurNetId()].dir = PlayerAvatar_GetFacingDir(sCommPlayerManager->fieldSystem->playerAvatar);
    sCommPlayerManager->playerLocationServer[CommSys_CurNetId()].x = PlayerAvatar_GetXPos(sCommPlayerManager->fieldSystem->playerAvatar);
    sCommPlayerManager->playerLocationServer[CommSys_CurNetId()].z = PlayerAvatar_GetZPos(sCommPlayerManager->fieldSystem->playerAvatar);
    sCommPlayerManager->playerLocationServer[CommSys_CurNetId()].dir = PlayerAvatar_GetFacingDir(sCommPlayerManager->fieldSystem->playerAvatar);
}

// Adopts netJd's position and held-flag ownership as the local player's, used
// when a client takes over the server's slot.
void CommPlayer_CopyPersonal(int netJd)
{
    sCommPlayerManager->resumeHandled = FALSE;
    int netId = CommSys_CurNetId();

    sCommPlayerManager->playerAvatar[netId] = sCommPlayerManager->fieldSystem->playerAvatar;
    sCommPlayerManager->isActive[netId] = TRUE;
    sCommPlayerManager->playerLocation[netId].x = sCommPlayerManager->playerLocation[netJd].x;
    sCommPlayerManager->playerLocation[netId].z = sCommPlayerManager->playerLocation[netJd].z;
    sCommPlayerManager->playerLocation[netId].dir = sCommPlayerManager->playerLocation[netJd].dir;
    sCommPlayerManager->playerLocationServer[netId].x = sCommPlayerManager->playerLocationServer[netJd].x;
    sCommPlayerManager->playerLocationServer[netId].z = sCommPlayerManager->playerLocationServer[netJd].z;
    sCommPlayerManager->playerLocationServer[netId].dir = sCommPlayerManager->playerLocationServer[netJd].dir;
    sCommPlayerManager->heldFlagOwnerInfo[netId] = sCommPlayerManager->heldFlagOwnerInfo[netJd];
    sCommPlayerManager->heldFlagOwnerInfo[netJd] = NULL;

    if (sCommPlayerManager->heldFlagOwnerInfo[netId]) {
        TrainerInfo_Copy(sCommPlayerManager->heldFlagOwnerInfo[netId], (TrainerInfo *)&sCommPlayerManager->heldFlagInfo[netId].ownerInfo);
    }

    sCommPlayerManager->heldFlagInfo[netJd].netID = NETID_NONE;
}

// Packet 22: the local player's X/Z and facing direction. param0 sets the high
// bit of the direction byte, which asks the receiver to resend all positions.
void CommPlayer_SendXZPos(BOOL param0, int x, int z)
{
    u8 data[5 + 1];
    int dir = PlayerAvatar_GetFacingDir(sCommPlayerManager->fieldSystem->playerAvatar);

    data[0] = x;
    data[1] = x >> 8;
    data[2] = z;
    data[3] = z >> 8;
    data[4] = dir;

    if (param0) {
        data[4] = data[4] | 0x80;
    }

    CommSys_SendDataFixedSize(22, data);
    sCommPlayerManager->sendAllPos = TRUE;
}

void CommPlayer_SendPos(BOOL param0)
{
    int x = PlayerAvatar_GetXPos(sCommPlayerManager->fieldSystem->playerAvatar);
    int z = PlayerAvatar_GetZPos(sCommPlayerManager->fieldSystem->playerAvatar);

    CommPlayer_SendXZPos(param0, x, z);
}

void CommPlayer_SendPosServer(BOOL param0)
{
    int x = sCommPlayerManager->playerLocationServer[CommSys_CurNetId()].x;
    int z = sCommPlayerManager->playerLocationServer[CommSys_CurNetId()].z;

    CommPlayer_SendXZPos(param0, x, z);
}

// Packet 23 (server -> clients): one player's position, packed into four bytes.
// Coordinates are clamped to the 0xf000 range the format can represent.
static void CommPlayer_SendPosNetId(int netId, const CommPlayerLocation *playerLocation)
{
    u8 data[COMM_PACKET_SIZE_POS_NETID + 1];
    int x = playerLocation->x, z = playerLocation->z;

    if (playerLocation->x < 0) {
        x = 0;
    } else if (playerLocation->x >= 0xf000) {
        x = 0xf000 - 1;
    }

    if (playerLocation->z < 0) {
        z = 0;
    } else if (playerLocation->z >= 0xf000) {
        z = 0xf000 - 1;
    }

    data[0] = netId & 0xf;
    data[1] = x;
    data[2] = ((x >> 8) & 0x1) + ((z >> 7) & 0x2);
    data[3] = z;

    if (playerLocation->collisionFlag) {
        data[2] |= 0x80;
    }

    data[0] += ((playerLocation->dir % 4) << 4);
    data[0] += (playerLocation->moveSpeed << 6);

    CommSys_SendDataServer(23, data, 0);
}

// While the local player is standing still, broadcast the position once; reset
// the latch when they start moving again.
void CommPlayer_SendDataTask(void *data)
{
    int v0 = PlayerAvatar_GetMoveState(sCommPlayerManager->fieldSystem->playerAvatar);
    int moveState = PlayerAvatar_GetPlayerMoveState(sCommPlayerManager->fieldSystem->playerAvatar);

    if ((0 == v0) && CommSys_IsPlayerConnected(CommSys_CurNetId())) {
        if (!sCommPlayerManager->idlePositionSent) {
            CommPlayer_SendPos(TRUE);
            sCommPlayerManager->idlePositionSent = 1;
        }
    }

    if ((1 == v0) && (1 == moveState)) {
        sCommPlayerManager->idlePositionSent = 0;
    }
}

// Server: send packet 23 for every active player whose position changed, or
// for everyone when a full resend was requested.
static void CommPlayer_SendChangedPositions(void *data)
{
    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->isActive[netId]) {
            CommPlayerLocation *location = &sCommPlayerManager->playerLocationServer[netId];

            if (sCommPlayerManager->movementChanged[netId] || sCommPlayerManager->sendAllPos) {
                sCommPlayerManager->movementChanged[netId] = 0;
                CommPlayer_SendPosNetId(netId, location);
            }
        }
    }

    sCommPlayerManager->sendAllPos = FALSE;
}

u32 CommPlayer_Size(void)
{
    return sizeof(CommPlayerManager);
}

// Creates the avatar for a newly connected player, unless the manager is
// disabled.
static void CommPlayer_Add(u8 netId)
{
    PlayerAvatar *playerAvatar;

    if (sCommPlayerManager->playerAvatar[netId] != NULL) {
        return;
    }

    if (sCommPlayerManager->isDisabled) {
        return;
    }

    TrainerInfo *trainerInfo = CommInfo_TrainerInfo(netId);

    if (trainerInfo) {
        if (!sCommPlayerManager->isUnderground && netId != CommSys_CurNetId()) {
            MapObject *obj = MapObjMan_LocalMapObjByIndex(sCommPlayerManager->fieldSystem->mapObjMan, 0xff + netId + 1);

            if (obj != NULL) {
                MapObject_Delete(obj);
            }
        }

        int version = 0;

        if (0 == TrainerInfo_GameCode(trainerInfo)) {
            version = 1;
        }

        playerAvatar = PlayerAvatar_New(sCommPlayerManager->fieldSystem->mapObjMan, sCommPlayerManager->playerLocation[netId].x, sCommPlayerManager->playerLocation[netId].z, sCommPlayerManager->playerLocation[netId].dir, 0x0, TrainerInfo_Gender(trainerInfo), version, NULL);

        GF_ASSERT(playerAvatar != NULL);
        sCommPlayerManager->playerAvatar[netId] = playerAvatar;

        MapObject_SetLocalID(PlayerAvatar_GetMapObject(playerAvatar), 0xff + netId + 1);

        if (sCommPlayerManager->isUnderground) {
            UndergroundMan_SetLeftUndergroundMessage(netId);
        }

        if (sCommPlayerManager->isUnderground && !sCommPlayerManager->isActive[netId]) {
            if (!sCommPlayerManager->isDisabled) {
                ov5_021F5634(sCommPlayerManager->fieldSystem, sCommPlayerManager->playerLocation[netId].x, 0, sCommPlayerManager->playerLocation[netId].z);
            }

            sCommPlayerManager->isActive[netId] = TRUE;
        } else if (!sCommPlayerManager->isUnderground) {
            sCommPlayerManager->isActive[netId] = TRUE;
        }
    }
}

// Removes a player's avatar. param1 keeps the player marked active (used when
// only the avatar is being torn down); param2 forces a full delete rather than
// a free.
void CommPlayer_Destroy(u8 netId, BOOL param1, BOOL param2)
{
    if (sCommPlayerManager == NULL) {
        return;
    }

    MI_CpuClear8(sCommPlayerManager->talkCount, MAX_CONNECTED_PLAYERS);

    if (sCommPlayerManager->isUnderground) {
        UndergroundPlayer_StopDisplayingEmote(netId);
    }

    if (sCommPlayerManager->playerAvatar[netId] != NULL) {
        if (sCommPlayerManager->fieldSystem->playerAvatar != sCommPlayerManager->playerAvatar[netId]) {
            if (sCommPlayerManager->isUnderground || param2) {
                PlayerAvatar_Delete(sCommPlayerManager->playerAvatar[netId]);
            } else {
                PlayerAvatar_Free(sCommPlayerManager->playerAvatar[netId]);
            }
        }

        sCommPlayerManager->playerAvatar[netId] = NULL;
    }

    if ((sCommPlayerManager->isActive[netId]) && (!param1)) {
        sCommPlayerManager->isActive[netId] = 0;
    }

    sCommPlayerManager->movementChanged[netId] = 1;

    if (!param1) {
        sCommPlayerManager->emote[netId] = 0;

        if (sCommPlayerManager->isUnderground) {
            UndergroundPlayer_DeleteHeldFlagDataClient(netId);
        }
    }
}

// Holding B makes the player walk, so report the slower speed to peers.
static void CommPlayer_SendMoveSpeed()
{
    u8 moveSpeed = 2;

    if (PAD_BUTTON_B & gSystem.heldKeys) {
        moveSpeed = 1;
    }

    CommSys_SetSendSpeed(moveSpeed);
}

// Per-frame task: broadcast movement, simulate the server's view of all
// players, and update clients.
static void Task_CommPlayerManagerRun(SysTask *task, void *data)
{
    if (CommSys_IsInitialized()) {
        CommPlayer_SendMoveSpeed();

        if (CommSys_CurNetId() == 0) {
            CommPlayer_SendChangedPositions(data);
            CommPlayer_SendDataTask(data);

            if (sCommPlayerManager->playerStatuses) {
                UndergroundPlayerStatuses_UpdateUnusedTimers(sCommPlayerManager->playerStatuses);
            }
        } else {
            CommPlayer_SendDataTask(data);
        }

        CommPlayer_UpdateClients(data);
    }

    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->isUnderground) {
            if (CommSys_CurNetId() == 0) {
                if (NULL == CommInfo_TrainerInfo(netId)) {
                    SecretBases_ClearBaseEntranceData(netId);
                }
            }
        }
    }
}

// Drives each connected player's client-side movement; disconnects are cleaned
// up by the server.
static void CommPlayer_UpdateClients(void *unused)
{
    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (!CommSys_IsPlayerConnected(netId)) {
            if (!(CommSys_IsAlone() && (netId == 0))) {
                if ((CommSys_CurNetId() == 0) && (sCommPlayerManager->isUnderground)) {
                    SecretBases_HandleDisconnectedPlayers(netId);
                }
            }
        }

        if (CommSys_IsPlayerConnected(netId) || (CommSys_IsAlone() && (netId == 0))) {
            CommPlayer_MoveClient(netId);

            if (sCommPlayerManager->isUnderground) {
                UndergroundPlayer_HandleEmoteDisplay(netId);
            }
        } else if (sCommPlayerManager->isActive[netId]) {
            if (CommSys_CurNetId() == 0 && sCommPlayerManager->isUnderground) {
                Traps_RemoveLinkData(netId);
                Mining_ResetPlayerData(netId);
            }

            u8 netId_u8 = netId;
            CommPlayer_RecvDelete(0, 1, &netId_u8, sCommPlayerManager->fieldSystem);
        }
    }
}

BOOL CommPlayerMan_IsFieldSystemActive(void)
{
    if (sCommPlayerManager != NULL) {
        return sCommPlayerManager->isFieldSystemActive;
    }

    return FALSE;
}

// Packet 62: tell peers whether this player's field system is active (i.e.
// movement allowed). Always sent.
void CommPlayerMan_BroadcastFieldSystemActive(BOOL param0)
{
    if (sCommPlayerManager != NULL) {
        if (sCommPlayerManager->isFieldSystemActive != param0) {
            sCommPlayerManager->isFieldSystemActive = param0;
        }

        CommSys_SendDataFixedSize(62, &sCommPlayerManager->isFieldSystemActive);
    }
}

// Same as CommPlayerMan_BroadcastFieldSystemActive but only sends when the
// value actually changes.
static void CommPlayerMan_SetFieldSystemActiveIfChanged(BOOL param0)
{
    if (sCommPlayerManager != NULL) {
        if (sCommPlayerManager->isFieldSystemActive != param0) {
            sCommPlayerManager->isFieldSystemActive = param0;
            CommSys_SendDataFixedSize(62, &sCommPlayerManager->isFieldSystemActive);
        }
    }
}

// Packet 62 handler: apply the sender's movement-enabled state.
void CommPlayer_RecvMovementEnabled(int netId, int param1, void *param2, void *unused)
{
    u8 *buffer = (u8 *)param2;

    if (sCommPlayerManager) {
        GF_ASSERT(param1 == 1);
        GF_ASSERT(netId < MAX_CONNECTED_PLAYERS);
        GF_ASSERT((buffer[0] == 1) || (buffer[0] == 0));

        CommPlayerMan_SetMovementEnabled(netId, buffer[0]);
    }
}

// Called each frame from field input handling; runs the server's movement
// simulation and battle-grid check.
void CommPlayerMan_Update(FieldSystem *fieldSystem, BOOL param1)
{
    if (sCommPlayerManager == NULL) {
        return;
    }

    if (!sCommPlayerManager->isUnderground) {
        CommPlayerMan_SetFieldSystemActiveIfChanged(param1);
        sCommPlayerManager->processInput = param1;
    }

    if (fieldSystem->playerAvatar) {
        sCommPlayerManager->playerAvatar[CommSys_CurNetId()] = fieldSystem->playerAvatar;
    }

    if (CommSys_IsInitialized() && (CommSys_CurNetId() == 0)) {
        CommPlayer_Move(NULL, sCommPlayerManager);
    }

    if (CommSys_IsInitialized() && (CommSys_CurNetId() == 0)) {
        if (!sCommPlayerManager->isUnderground) {
            CommPlayerMan_CheckBattleGridServer();
        }
    }
}

// Maps held D-pad keys to a facing direction, or -1 if none.
static int CommPlayer_Direction(u16 unused, u16 keys)
{
    if (keys & PAD_KEY_LEFT) {
        return 2;
    }

    if (keys & PAD_KEY_RIGHT) {
        return 3;
    }

    if (keys & PAD_KEY_UP) {
        return 0;
    }

    if (keys & PAD_KEY_DOWN) {
        return 1;
    }

    return -1;
}

// TRUE if an object event occupies the tile.
BOOL CommPlayer_CheckNPCCollision(int x, int z)
{
    int npcCnt = MapHeaderData_GetNumObjectEvents(sCommPlayerManager->fieldSystem), i;
    const ObjectEvent *objEventList = MapHeaderData_GetObjectEvents(sCommPlayerManager->fieldSystem);

    for (i = 0; i < npcCnt; i++) {
        if ((objEventList[i].x == x) && (objEventList[i].z == z)) {
            return 1;
        }
    }

    return 0;
}

// TRUE if the tile is blocked by another player, an NPC, a secret base, or
// terrain.
static BOOL CommPlayer_CheckCollision(int x, int z, int netIdTarget)
{
    if ((x != 0xffff) && (z != 0xffff)) {
        for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            if (netId == netIdTarget) {
                continue;
            }

            if (x == CommPlayer_GetXServerIfActive(netId) && z == CommPlayer_GetZServerIfActive(netId)) {
                return TRUE;
            }
        }

        if (CommPlayer_CheckNPCCollision(x, z)) {
            return TRUE;
        }

        if (sCommPlayerManager->isUnderground) {
            if (!SecretBases_AreCoordinatesWalkable(x, z)) {
                return TRUE;
            }
        }
    }

    return TerrainCollisionManager_CheckCollision(sCommPlayerManager->fieldSystem, x, z);
}

// Slide speed (in frames per tile) increases as the slide runs out.
static int CommPlayerMan_GetSlideMovementSpeed(int slideTilesLeft, int unused)
{
    if (slideTilesLeft < 5) {
        return 8;
    } else if (slideTilesLeft < 10) {
        return 4;
    }

    return 2;
}

// Frames a single step takes for each move speed.
static int CommPlayer_GetMoveDuration(int param0)
{
    int v0[5] = { 2, 4, 8, 16, 2 };

    GF_ASSERT(param0 < 5);
    return v0[param0];
}

// Server-side movement simulation for every active player.
static void CommPlayer_Move(SysTask *unused0, void *unused1)
{
    u16 keys;
    int dir, x, z, netId, v6;
    CommPlayerLocation *playerLocation;
    PlayerStatus *playerStatus = NULL;

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->isActive[netId] && sCommPlayerManager->movementEnabled[netId] && sCommPlayerManager->movementEnabled2[netId]) {
            playerLocation = &sCommPlayerManager->playerLocationServer[netId];

            if (sCommPlayerManager->playerStatuses) {
                playerStatus = UndergroundPlayerStatuses_GetPlayerStatus(sCommPlayerManager->playerStatuses, netId);
            }

            if (sCommPlayerManager->isUnderground) {
                if (SecretBases_IsPlayerMidBaseTransition(netId)) {
                    continue;
                }
            }

            if (sCommPlayerManager->moveTimerServer[netId] != 0) {
                sCommPlayerManager->moveTimerServer[netId]--;
            }

            if (sCommPlayerManager->moveTimerServer[netId] == 0) {
                if (sCommPlayerManager->isUnderground) {
                    if (Traps_CheckPlayerSteppedOnTrap(netId)) {
                        continue;
                    }

                    if (!UndergroundPlayer_IsAffectedByTrap(netId)) {
                        if (SecretBases_CheckPlayerTriggeredTool(netId)) {
                            continue;
                        }
                    }
                }
            }

            if (playerStatus) {
                if (!UndergroundPlayerStatus_CanMove(playerStatus)) {
                    continue;
                }
            }

            if (!sCommPlayerManager->onBattleGrid[netId]) {
                keys = sub_02035E84(netId);
            } else {
                keys = 0;
            }

            playerLocation->moveSpeed = CommSys_RecvSpeed(netId);

            if (sCommPlayerManager->moveTimerServer[netId] != 0) {
                continue;
            }

            v6 = CommPlayerMan_GetSlideMovementSpeed(sCommPlayerManager->slideTilesLeft[netId], sCommPlayerManager->hurlTrapTriggered[netId]);

            if (CommPlayer_MoveSlide(netId, v6)) {
                sCommPlayerManager->moveTimerServer[netId] = v6;
                sCommPlayerManager->movementChanged[netId] = TRUE;
                continue;
            }

            if (playerLocation->collisionFlag == 1) {
                sCommPlayerManager->movementChanged[netId] = TRUE;
            }

            playerLocation->collisionFlag = 0;

            if (sCommPlayerManager->holeMovementsLeft[netId] == 2) {
                u8 data = netId;

                sCommPlayerManager->holeMovementsLeft[netId] = 1;
                CommSys_SendDataFixedSizeServer(40, &data);
                continue;
            }

            dir = CommPlayer_Direction(keys, keys);
            x = CommPlayer_GetXInFrontOfPlayerServer(netId);
            z = CommPlayer_GetZInFrontOfPlayerServer(netId);

            if ((x != 0xffff) && (z != 0xffff) && (dir != -1)) {
                if (playerLocation->dir != dir) {
                    playerLocation->dir = dir;
                    sCommPlayerManager->moveTimerServer[netId] = 4;
                    sCommPlayerManager->movementChanged[netId] = 1;

                    if (sCommPlayerManager->holeMovementsLeft[netId] > 2) {
                        sCommPlayerManager->holeMovementsLeft[netId]--;
                        continue;
                    }
                } else if (sCommPlayerManager->holeMovementsLeft[netId] != 0) {
                    continue;
                } else if (sCommPlayerManager->isUnderground && (SecretBases_CheckForEnterExit(netId, x, z) == 1)) {
                    continue;
                } else if (CommPlayer_CheckCollision(x, z, netId)) {
                    playerLocation->collisionFlag = 1;
                    sCommPlayerManager->movementChanged[netId] = 1;
                    sCommPlayerManager->moveTimerServer[netId] = 4;
                } else {
                    sCommPlayerManager->movementChanged[netId] = 1;
                    playerLocation->x = x;
                    playerLocation->z = z;
                    playerLocation->dir = dir;

                    sCommPlayerManager->moveTimerServer[netId] = CommPlayer_GetMoveDuration(playerLocation->moveSpeed);

                    if (sCommPlayerManager->alteredMovementStepsLeft[netId] != 0) {
                        sCommPlayerManager->alteredMovementStepsLeft[netId]--;

                        if (sCommPlayerManager->alteredMovementStepsLeft[netId] == 0) {
                            u8 v10 = 1;
                            Traps_EscapeTrapServer(netId, 1, &v10, NULL);
                        }
                    }
                }
            }
        }
    }
}

// Packet 22 handler (server side): store the sender's position. The high bit
// of the direction byte requests a full position resend.
void CommPlayer_RecvLocation(int netId, int unused0, void *src, void *unused1)
{
    u8 *buffer = (u8 *)src;
    CommPlayerLocation *playerLocation;

    if (sCommPlayerManager == NULL) {
        return;
    }

    playerLocation = &sCommPlayerManager->playerLocationServer[netId];

    if (buffer[4] & 0x80) {
        sCommPlayerManager->sendAllPos = TRUE;
        return;
    }

    if (playerLocation->dir == -1) {
        int netId;

        for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
            sCommPlayerManager->movementChanged[netId] = 1;
        }
    }

    playerLocation->x = 0;
    playerLocation->z = 0;
    playerLocation->x += ((u32)buffer[0]) & 0xff;
    playerLocation->x += ((u32)buffer[1] << 8) & 0xff00;
    playerLocation->z += ((u32)buffer[2]) & 0xff;
    playerLocation->z += ((u32)buffer[3] << 8) & 0xff00;
    playerLocation->dir = buffer[4] & 0xf;

    sCommPlayerManager->isActive[netId] = 1;
    sCommPlayerManager->movementChanged[netId] = 1;

    if (TerrainCollisionManager_CheckCollision(sCommPlayerManager->fieldSystem, playerLocation->x, playerLocation->z)) {
        GF_ASSERT(FALSE);
    }
}

// Packet 52 handler: a player left; tear down their avatar and reset their
// info.
void CommPlayer_RecvDelete(int unused0, int unused1, void *src, void *unused2)
{
    u8 *buffer = (u8 *)src;
    u8 netId = buffer[0];

    if (sCommPlayerManager == NULL) {
        return;
    }

    if (netId == CommSys_CurNetId()) {
        return;
    }

    if (sCommPlayerManager->isUnderground) {
        UndergroundMan_SetPlayerLeft(netId);

        if (!sCommPlayerManager->isDisabled) {
            ov5_021F5634(sCommPlayerManager->fieldSystem, sCommPlayerManager->playerLocation[netId].x, 0, sCommPlayerManager->playerLocation[netId].z);
        }
    }

    CommPlayer_Destroy(netId, 0, 0);
    CommInfo_InitPlayer(netId);
}

int CommPacketSizeOf_RecvLocation(void)
{
    return COMM_PACKET_SIZE_LOCATION;
}

// Packet 23 handler (client side): unpack a player's position and create their
// avatar if needed.
void CommPlayer_RecvLocationAndInit(int netId, int size, void *src, void *unused)
{
    u8 *buffer = (u8 *)src;
    CommPlayerLocation *playerLocation;
    int netJd = buffer[0] & 0xf;

    if (sCommPlayerManager == NULL) {
        return;
    }

    if ((netId == CommSys_CurNetId()) && sCommPlayerManager->inSecretBaseTransition) {
        return;
    }

    playerLocation = &sCommPlayerManager->playerLocation[netJd];

    if (buffer[2] & 0x80) {
        playerLocation->collisionFlag = 1;
    } else {
        playerLocation->collisionFlag = 0;
    }

    playerLocation->x = 0;
    playerLocation->z = 0;
    playerLocation->x += ((u32)buffer[1]) & 0xff;
    playerLocation->x += ((u32)buffer[2] << 8) & 0x100;
    playerLocation->z += ((u32)buffer[3]) & 0xff;
    playerLocation->z += ((u32)buffer[2] << 7) & 0x100;
    playerLocation->dir = ((buffer[0] >> 4) & 0x3);
    playerLocation->moveSpeed = ((buffer[0] >> 6) & 0x3);

    CommPlayer_Add(netJd);
}

// Counts the local player's underground steps; every 50 awards a trainer
// score.
static void CommPlayer_RecordUndergroundStep(int netId)
{
    Underground *underground = SaveData_GetUnderground(FieldSystem_GetSaveData(sCommPlayerManager->fieldSystem));
    SaveData_GetUndergroundRecord(FieldSystem_GetSaveData(sCommPlayerManager->fieldSystem));

    if (sCommPlayerManager->isUnderground) {
        if (netId == CommSys_CurNetId()) {
            Underground_IncrementStepCount(underground);

            if (Underground_GetStepCount(underground) == 50) {
                GameRecords_IncrementTrainerScore(SaveData_GetGameRecords(sCommPlayerManager->fieldSystem->saveData), TRAINER_SCORE_EVENT_UNDERGROUND_100_STEPS);
            }
        }
    }
}

// Plays the slide/walk animation for a player being pushed. animSpeed selects
// the walk speed row.
static BOOL CommPlayer_SlideAnimation(int netId, int param1, int unused, int animSpeed)
{
    u8 walkAnimationCode[] = {
        MOVEMENT_ACTION_WALK_FASTER_NORTH, MOVEMENT_ACTION_WALK_FASTER_SOUTH, MOVEMENT_ACTION_WALK_FASTER_WEST, MOVEMENT_ACTION_WALK_FASTER_EAST, MOVEMENT_ACTION_WALK_FAST_NORTH, MOVEMENT_ACTION_WALK_FAST_SOUTH, MOVEMENT_ACTION_WALK_FAST_WEST, MOVEMENT_ACTION_WALK_FAST_EAST, MOVEMENT_ACTION_WALK_NORMAL_NORTH, MOVEMENT_ACTION_WALK_NORMAL_SOUTH, MOVEMENT_ACTION_WALK_NORMAL_WEST, MOVEMENT_ACTION_WALK_NORMAL_EAST
    };

    if ((sCommPlayerManager->slideAnimationDir[netId] == DIR_NONE) || (sCommPlayerManager->playerAvatar[netId] == NULL)) {
        return FALSE;
    }

    if (param1 == 0) {
        return TRUE;
    }

    MapObject *obj = PlayerAvatar_GetMapObject(sCommPlayerManager->playerAvatar[netId]);

    if (PlayerAvatar_GetFacingDir(sCommPlayerManager->playerAvatar[netId]) != CommPlayer_GetOppositeDir(sCommPlayerManager->slideAnimationDir[netId])) {
        MapObject_SetStatusFlagOff(obj, MAP_OBJ_STATUS_LOCK_DIR);
        PlayerAvatar_TryFace(sCommPlayerManager->playerAvatar[netId], CommPlayer_GetOppositeDir(sCommPlayerManager->slideAnimationDir[netId]));
        MapObject_SetStatusFlagOn(obj, MAP_OBJ_STATUS_LOCK_DIR);
    }

    LocalMapObj_CheckAnimationFinished(obj);

    if (LocalMapObj_IsAnimationSet(obj) == 1) {
        CommPlayer_RecordUndergroundStep(netId);

        switch (animSpeed) {
        case 0:
            LocalMapObj_SetAnimationCode(obj, walkAnimationCode[sCommPlayerManager->slideAnimationDir[netId]]);
            break;
        case 1:
            LocalMapObj_SetAnimationCode(obj, walkAnimationCode[sCommPlayerManager->slideAnimationDir[netId] + 4]);
            break;
        case 2:
            LocalMapObj_SetAnimationCode(obj, walkAnimationCode[sCommPlayerManager->slideAnimationDir[netId] + 8]);
            break;
        }
    }

    return TRUE;
}

// Client-side: walk the local avatar toward the position the server reported.
static void CommPlayer_MoveClient(int netId)
{
    u16 pad = 0;
    CommPlayerLocation *playerLocation = &sCommPlayerManager->playerLocation[netId];
    PlayerAvatar *playerAvatar;
    int moveSpeed;

    if (sCommPlayerManager->moveTimer[netId] != 0) {
        sCommPlayerManager->moveTimer[netId]--;
        return;
    }

    if ((playerLocation->x == 0xffff) || (playerLocation->z == 0xffff)) {
        return;
    }

    if (sCommPlayerManager->isDisabled) {
        return;
    }

    if (!sCommPlayerManager->isUnderground) {
        if (netId == CommSys_CurNetId()) {
            if (!sCommPlayerManager->processInput) {
                return;
            }
        }
    }

    playerAvatar = sCommPlayerManager->playerAvatar[netId];

    if (playerAvatar) {
        int dx = PlayerAvatar_GetXPos(playerAvatar) - playerLocation->x;
        int dy = PlayerAvatar_GetZPos(playerAvatar) - playerLocation->z;
        int dir = PlayerAvatar_GetFacingDir(playerAvatar);

        if ((dx == 0) && (dy == 0)) {
            pad = 0;
        } else if (abs(dx) > abs(dy)) {
            if (dx > 0) {
                pad = PAD_KEY_LEFT;
            } else {
                pad = PAD_KEY_RIGHT;
            }
        } else {
            if (dy > 0) {
                pad = PAD_KEY_UP;
            } else {
                pad = PAD_KEY_DOWN;
            }
        }

        if (abs(dx) + abs(dy) > 2) {
        }

        if (CommPlayer_SlideAnimation(netId, pad, pad, playerLocation->moveSpeed)) {
            return;
        }

        u32 animCode = 0xff;
        int v10;

        moveSpeed = playerLocation->moveSpeed;

        switch (playerLocation->moveSpeed) {
        case 0:
            v10 = 5;
            break;
        case 1:
            v10 = 4;
            pad |= PAD_BUTTON_B;
            break;
        case 2:
            v10 = 2;
            break;
        case 3:
            v10 = 1;
            break;
        }

        if (sCommPlayerManager->forceDirTimer && (netId == CommSys_CurNetId())) {
            sCommPlayerManager->forceDirTimer--;
        } else if (!CommSys_IsSendingMovementData() && sCommPlayerManager->isUnderground && (netId == CommSys_CurNetId())) {
            animCode = PlayerAvatar_GetMovementActionAnimCode(playerAvatar, pad, pad, v10, 1, 0);
        } else if (((pad & ~PAD_BUTTON_B) == 0) && (playerLocation->collisionFlag)) {
            moveSpeed = 3;

            switch (playerLocation->dir) {
            case DIR_NORTH:
                animCode = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH;
                break;
            case DIR_SOUTH:
            default:
                animCode = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_SOUTH;
                break;
            case DIR_WEST:
                animCode = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_WEST;
                break;
            case DIR_EAST:
                animCode = MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_EAST;
                break;
            }
        } else if (((pad & ~PAD_BUTTON_B) == 0) && (playerLocation->dir != dir)) {
            animCode = MovementAction_TurnActionTowardsDir(playerLocation->dir, MOVEMENT_ACTION_WALK_ON_SPOT_FAST_NORTH);
        } else {
            animCode = PlayerAvatar_GetMovementActionAnimCode(playerAvatar, pad, pad, v10, 1, 0);
        }

        if (PlayerAvatar_IsMapObjectAnimationSet(playerAvatar) == 0) {
            if (!PlayerAvatar_IsAnimationSetOrWalkOnSpotSlow(playerAvatar)) {
                return;
            }
        }

        if (animCode != 0xff) {
            PlayerAvatar_SetMapObjMovement(playerAvatar, animCode, 1);

            if (pad & ~PAD_BUTTON_B) {
                if (sCommPlayerManager->moveTimer[netId] == 0) {
                    sCommPlayerManager->moveTimer[netId] = CommPlayer_GetMoveDuration(moveSpeed);
                }

                if (sCommPlayerManager->moveTimer[netId] != 0) {
                    sCommPlayerManager->moveTimer[netId]--;
                }

                CommPlayer_RecordUndergroundStep(netId);
            }
        }
    }
}

// Advances a player being pushed one tile per call. Returns TRUE while the
// slide is still in progress.
static BOOL CommPlayer_MoveSlide(int netId, int speed)
{
    // int x, z;
    CommPlayerLocation *playerLocation;

    if (sCommPlayerManager->slideTilesLeft[netId] == 0) {
        return FALSE;
    }

    if (sCommPlayerManager->slideTilesLeft[netId] == 0xff) {
        return TRUE;
    }

    sCommPlayerManager->slideTilesLeft[netId]--;

    // unreachable, only called with 2, 4, or 8
    if (speed == 1) {
        return TRUE;
    }

    int x = CommPlayer_GetXServerIfActive(netId);
    int z = CommPlayer_GetZServerIfActive(netId);
    x += MapObject_GetDxFromDir(sCommPlayerManager->slideDir[netId]);
    z += MapObject_GetDzFromDir(sCommPlayerManager->slideDir[netId]);

    if (sCommPlayerManager->slideTilesLeft[netId] != 0) {
        if (CommPlayer_CheckCollision(x, z, netId)) {
            sCommPlayerManager->slideTilesLeft[netId] = 1;
        }
    }

    if (sCommPlayerManager->slideTilesLeft[netId] == 1) {
        return TRUE;
    } else if (sCommPlayerManager->slideTilesLeft[netId] == 0) {
        u8 data = 1;

        Traps_EscapeTrapServer(netId, 1, &data, NULL);
        return TRUE;
    }

    playerLocation = &sCommPlayerManager->playerLocationServer[netId];
    playerLocation->x = x;
    playerLocation->z = z;

    switch (speed) {
    case 2:
        playerLocation->moveSpeed = 0;
        break;
    case 4:
        playerLocation->moveSpeed = 1;
        break;
    case 8:
        playerLocation->moveSpeed = 2;
        break;
    }

    return TRUE;
}

// Begins a slide (trap push) in dir; hurl traps push further than normal
// traps.
void CommPlayer_StartSlide(int netId, int dir, BOOL isHurlTrap)
{
    CommPlayerLocation *playerLocation = &sCommPlayerManager->playerLocationServer[netId];

    playerLocation->dir = CommPlayer_GetOppositeDir(dir);
    sCommPlayerManager->slideDir[netId] = dir;

    if (isHurlTrap) {
        sCommPlayerManager->hurlTrapTriggered[netId] = TRUE;
        sCommPlayerManager->slideTilesLeft[netId] = 45;
    } else {
        sCommPlayerManager->hurlTrapTriggered[netId] = FALSE;
        sCommPlayerManager->slideTilesLeft[netId] = 27;
    }

    sCommPlayerManager->movementChanged[netId] = TRUE;
}

// Cancels an in-progress slide.
void CommPlayer_StopSlide(int netId)
{
    sCommPlayerManager->slideTilesLeft[netId] = 0;
}

// Marks the slide as finished (0xff) so the next MoveSlide call returns
// immediately.
void CommPlayer_EndCurrentSlide(int netId)
{
    sCommPlayerManager->slideTilesLeft[netId] = 0xff;
}

// Locks the avatar's facing/animation while a slide animation plays.
void CommPlayer_StartSlideAnimation(int netId, int dir, BOOL unused)
{
    if (sCommPlayerManager->playerAvatar[netId] == NULL) {
        return;
    }

    MapObject *obj = PlayerAvatar_GetMapObject(sCommPlayerManager->playerAvatar[netId]);

    MapObject_SetStatusFlagOn(obj, MAP_OBJ_STATUS_LOCK_DIR);
    MapObject_SetStatusFlagOn(obj, MAP_OBJ_STATUS_PAUSE_ANIMATION);

    sCommPlayerManager->slideAnimationDir[netId] = dir;
}

// Restores the avatar's facing/animation after a slide.
void CommPlayer_StopSlideAnimation(int netId)
{
    if (sCommPlayerManager->playerAvatar[netId] == NULL) {
        return;
    }

    if (sCommPlayerManager->slideAnimationDir[netId] != DIR_NONE) {
        MapObject *obj = PlayerAvatar_GetMapObject(sCommPlayerManager->playerAvatar[netId]);

        LocalMapObj_ClearAnimation(obj);
        MapObject_SetStatusFlagOff(obj, MAP_OBJ_STATUS_LOCK_DIR);
        MapObject_SetStatusFlagOff(obj, MAP_OBJ_STATUS_PAUSE_ANIMATION);

        sCommPlayerManager->slideAnimationDir[netId] = DIR_NONE;
    }
}

int CommPacketSizeOf_RecvLocationAndInit(void)
{
    return COMM_PACKET_SIZE_POS_NETID;
}

// TRUE when the local player may act: alone, connected, and (underground)
// input allowed.
BOOL CommPlayerMan_IsInputAllowed(void)
{
    if (CommSys_IsAlone()) {
        return TRUE;
    }

    if (!sCommPlayerManager) {
        return FALSE;
    }

    if (!CommSys_IsPlayerConnected(CommSys_CurNetId())) {
        return FALSE;
    }

    if (sCommPlayerManager->isUnderground && !CommManUnderground_IsInputAllowed()) {
        return FALSE;
    }

    return TRUE;
}

BOOL CommPlayer_IsActive(int netId)
{
    return sCommPlayerManager->isActive[netId];
}

int CommPlayer_GetXIfActive(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    } else if (CommSys_CurNetId() == netId) {
        return sCommPlayerManager->playerLocation[netId].x;
    } else if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].x;
}

int CommPlayer_GetZIfActive(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    } else if (CommSys_CurNetId() == netId) {
        return sCommPlayerManager->playerLocation[netId].z;
    } else if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].z;
}

int CommPlayer_GetX(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].x;
}

int CommPlayer_GetZ(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].z;
}

int CommPlayer_GetXInFrontOfPlayer(int netId)
{
    if (CommPlayer_GetXIfActive(netId) == 0xffff) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].x + MapObject_GetDxFromDir(sCommPlayerManager->playerLocation[netId].dir);
}

int CommPlayer_GetZInFrontOfPlayer(int netId)
{
    if (CommPlayer_GetZIfActive(netId) == 0xffff) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocation[netId].z + MapObject_GetDzFromDir(sCommPlayerManager->playerLocation[netId].dir);
}

int CommPlayer_GetXServerIfActive(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    } else if (CommSys_CurNetId() == netId) {
        return sCommPlayerManager->playerLocationServer[netId].x;
    } else if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].x;
}

int CommPlayer_GetZServerIfActive(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    } else if (CommSys_CurNetId() == netId) {
        return sCommPlayerManager->playerLocationServer[netId].z;
    } else if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].z;
}

int CommPlayer_GetXServer(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].x;
}

int CommPlayer_GetZServer(int netId)
{
    if (!sCommPlayerManager) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].z;
}

int CommPlayer_GetXInFrontOfPlayerServer(int netId)
{
    if (CommPlayer_GetXServerIfActive(netId) == 0xffff) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].x + MapObject_GetDxFromDir(sCommPlayerManager->playerLocationServer[netId].dir);
}

int CommPlayer_GetZInFrontOfPlayerServer(int netId)
{
    if (CommPlayer_GetZServerIfActive(netId) == 0xffff) {
        return 0xffff;
    }

    return sCommPlayerManager->playerLocationServer[netId].z + MapObject_GetDzFromDir(sCommPlayerManager->playerLocationServer[netId].dir);
}

int CommPlayer_Dir(int netId)
{
    if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return -1;
    }

    return sCommPlayerManager->playerLocation[netId].dir;
}

int CommPlayer_DirServer(int netId)
{
    if (!CommPlayerMan_IsInputAllowed() || !sCommPlayerManager->isActive[netId]) {
        return -1;
    }

    return sCommPlayerManager->playerLocationServer[netId].dir;
}

// Turn netIdSet to face netIdTarget (server copy).
void CommPlayer_LookTowardsServer(int netIdTarget, int netIdSet)
{
    int dir = CommPlayer_GetOppositeDir(sCommPlayerManager->playerLocationServer[netIdTarget].dir);

    sCommPlayerManager->playerLocationServer[netIdSet].dir = dir;
    sCommPlayerManager->movementChanged[netIdSet] = TRUE;
}

void CommPlayer_LookTowards(int netIdTarget, int netIdSet)
{
    int dir = CommPlayer_GetOppositeDir(sCommPlayerManager->playerLocation[netIdTarget].dir);
    sCommPlayerManager->playerLocation[netIdSet].dir = dir;
}

int CommPlayerMan_GetLinkNetIDAtLocation(int xPos, int zPos)
{
    if (!sCommPlayerManager) {
        return NETID_NONE;
    }

    for (int netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (!sCommPlayerManager->isActive[netId]) {
            continue;
        }

        int x = sCommPlayerManager->playerLocationServer[netId].x;
        int z = sCommPlayerManager->playerLocationServer[netId].z;

        if (xPos == x && zPos == z) {
            return netId;
        }
    }

    return NETID_NONE;
}

// Enabling/disabling movement resets the player's collision flag and speed.
void CommPlayerMan_SetMovementEnabled(int netId, BOOL movementEnabled)
{
    if (sCommPlayerManager->movementEnabled[netId] != movementEnabled) {
        sCommPlayerManager->movementEnabled[netId] = movementEnabled;
        sCommPlayerManager->playerLocationServer[netId].collisionFlag = FALSE;
        sCommPlayerManager->movementChanged[netId] = TRUE;
        sCommPlayerManager->playerLocationServer[netId].moveSpeed = 2;
    }
}

// Movement is suppressed while held-flag data is being updated.
BOOL CommPlayerMan_IsMovementEnabled(int netId)
{
    if (sCommPlayerManager->updatingHeldFlags) {
        return FALSE;
    }

    if (!sCommPlayerManager->movementEnabled2[netId]) {
        return FALSE;
    }

    return sCommPlayerManager->movementEnabled[netId];
}

// Battle-room starting tiles, indexed by the order players are assigned to
// them. 1v1 uses two tiles; 2v2 uses four.
static BattleGridPosition sBattleGrid1v1[] = {
    { 0x4, 0x7 },
    { 0xB, 0x7 }
};

static BattleGridPosition sBattleGrid2v2[] = {
    { 0x4, 0x6 },
    { 0xB, 0x6 },
    { 0x4, 0x8 },
    { 0xB, 0x8 }
};

// Client-side check: returns TRUE once the local player is standing on one of
// the battle-room tiles. When every connected player is on a tile, the tile
// index each player occupies is recorded as their battle position.
BOOL CommPlayerMan_CheckBattleGridPositions(void)
{
    BattleGridPosition battleGrid1v1[] = {
        { 4, 7 },
        { 11, 7 }
    };
    BattleGridPosition battleGrid2v2[] = {
        { 4, 6 },
        { 11, 6 },
        { 4, 8 },
        { 11, 8 }
    };
    int connectedPlayers = CommType_MaxPlayers(CommManager_GetCommType());
    int netId, netJd, playerCnt = 0, battlePos[4];
    int localPlayerOnGrid = 0;
    BattleGridPosition *grid;

    if (connectedPlayers == 2) {
        grid = battleGrid1v1;
    } else {
        grid = battleGrid2v2;
    }

    for (netId = 0; netId < connectedPlayers; netId++) {
        for (netJd = 0; netJd < connectedPlayers; netJd++) {
            if ((CommPlayer_GetXIfActive(netJd) == grid[netId].x) && (CommPlayer_GetZIfActive(netJd) == grid[netId].z)) {
                playerCnt++;
                battlePos[netJd] = netId;

                if (netJd == CommSys_CurNetId()) {
                    localPlayerOnGrid = 1;
                }
                break;
            }
        }
    }

    if (playerCnt == connectedPlayers) {
        for (netId = 0; netId < connectedPlayers; netId++) {
            sub_020362DC(battlePos[netId], netId);
        }
    }

    return localPlayerOnGrid;
}

void CommPlayer_RecvBattleRoomState(int netId, int unused0, void *src, void *unused3)
{
    u8 *buffer = src;

    // State 3 means the player has left the battle grid; any other value is
    // stashed until the server confirms the player reached their tile.
    if (buffer[0] == 3) {
        sCommPlayerManager->onBattleGrid[netId] = 0;
    } else {
        sCommPlayerManager->battleRoomState[netId] = buffer[0];
    }
}

// Server-side counterpart to CommPlayerMan_CheckBattleGridPositions: once a
// player that reported a battle-room state is standing on a grid tile, lock
// their input and tell them (packet 95) that they are in position.
static void CommPlayerMan_CheckBattleGridServer(void)
{
    int connectedPlayers = CommType_MaxPlayers(CommManager_GetCommType());
    int netJd = 0;
    BattleGridPosition *grid;
    u8 netId;

    if (connectedPlayers == 2) {
        grid = sBattleGrid1v1;
    } else {
        grid = sBattleGrid2v2;
    }

    for (netId = 0; netId < connectedPlayers; netId++) {
        if (!sCommPlayerManager->battleRoomState[netId]) {
            continue;
        }

        for (netJd = 0; netJd < connectedPlayers; netJd++) {
            if ((CommPlayer_GetXServerIfActive(netId) == grid[netJd].x) && (CommPlayer_GetZServerIfActive(netId) == grid[netJd].z)) {
                sCommPlayerManager->onBattleGrid[netId] = 1;
                CommSys_SendDataFixedSizeServer(95, &netId);
            }
        }
    }
}

// Face the local player toward the centre of the battle room.
void CommPlayer_SetBattleDir(void)
{
    int netId = CommSys_CurNetId();
    int code;

    if (CommPlayer_GetXIfActive(netId) > 8) {
        CommPlayer_SetDir(2);
        code = MovementAction_TurnActionTowardsDir(DIR_WEST, MOVEMENT_ACTION_WALK_ON_SPOT_FAST_NORTH);
    } else {
        CommPlayer_SetDir(3);
        code = MovementAction_TurnActionTowardsDir(DIR_EAST, MOVEMENT_ACTION_WALK_ON_SPOT_FAST_NORTH);
    }

    PlayerAvatar_SetMapObjMovement(sCommPlayerManager->playerAvatar[netId], code, 1);
    CommPlayerMan_ForceDir();
}

// Once every player has stopped moving, step each one back off the grid and
// re-enable movement. Returns 0 while players are still moving.
BOOL CommPlayerMan_StepBackFromBattleGrid(void)
{
    int connectedPlayers = CommType_MaxPlayers(CommManager_GetCommType());
    int netId = 0, dir;

    for (netId = 0; netId < connectedPlayers; netId++) {
        if ((sCommPlayerManager->isActive[netId] == 0) || (sCommPlayerManager->movementChanged[netId] == 1)) {
            return 0;
        }
    }

    for (netId = 0; netId < connectedPlayers; netId++) {
        CommPlayerLocation *playerLocation = &sCommPlayerManager->playerLocationServer[netId];

        dir = CommPlayer_GetOppositeDir(playerLocation->dir);
        playerLocation->x += MapObject_GetDxFromDir(dir);
        playerLocation->z += MapObject_GetDzFromDir(dir);
        playerLocation->moveSpeed = 2;

        sCommPlayerManager->movementChanged[netId] = 1;
        CommPlayerMan_SetMovementEnabled(netId, 1);
    }

    return 1;
}

// Opposite facing direction.
int CommPlayer_GetOppositeDir(int dir)
{
    if (dir == FACE_UP) {
        return FACE_DOWN;
    } else if (dir == FACE_DOWN) {
        return FACE_UP;
    } else if (dir == FACE_LEFT) {
        return FACE_RIGHT;
    } else if (dir == FACE_RIGHT) {
        return FACE_LEFT;
    }

    GF_ASSERT(FALSE);
    return FACE_LEFT;
}

// Force a player to keep moving for duration steps (e.g. spin trap).
void CommPlayerMan_SetPlayerAlteredMovement(int netId, int duration)
{
    sCommPlayerManager->alteredMovementStepsLeft[netId] = duration;
    sCommPlayerManager->moveTimerServer[netId] = 15;
}

// Stop a player's altered movement.
void CommPlayerMan_EndPlayerAlteredMovement(int netId)
{
    sCommPlayerManager->alteredMovementStepsLeft[netId] = 0;
}

// actual amount of actions required to escape hole is movementsToEscape - 2
void CommPlayerMan_PutPlayerInHole(int netId, int movementsToEscape)
{
    sCommPlayerManager->holeMovementsLeft[netId] = movementsToEscape;
    sCommPlayerManager->moveTimerServer[netId] = 15;
}

void CommPlayerMan_RemovePlayerFromHole(int netId)
{
    sCommPlayerManager->holeMovementsLeft[netId] = 0;
}

int CommPlayer_GetMovementTimer(int netId)
{
    return sCommPlayerManager->moveTimer[netId];
}

int CommPlayer_GetMovementTimerServer(int netId)
{
    return sCommPlayerManager->moveTimerServer[netId];
}

void CommPlayer_SetDir(int dir)
{
    PlayerAvatar_TryFace(sCommPlayerManager->fieldSystem->playerAvatar, dir);
    sCommPlayerManager->playerLocation[CommSys_CurNetId()].dir = dir;
    CommPlayer_SendPos(FALSE);
}

void CommPlayer_SetDirClient(int netId, int dir)
{
    sCommPlayerManager->playerLocation[netId].dir = dir;
    PlayerAvatar_TryFace(sCommPlayerManager->playerAvatar[netId], dir);
}

int CommPlayer_DirClient(int netId)
{
    if (!sCommPlayerManager->playerAvatar[netId]) {
        return -1;
    }

    return PlayerAvatar_GetFacingDir(sCommPlayerManager->playerAvatar[netId]);
}

// Pause the field system, recording why so it can be resumed once all reasons
// clear.
void CommPlayerMan_PauseFieldSystemWithContextBit(int contextBit)
{
    if (contextBit != 0) {
        sCommPlayerManager->pauseBits |= contextBit;
    }

    FieldSystem_PauseProcessing();
    sCommPlayerManager->isFieldSystemActive = FALSE;
}

// Clear one pause reason; resume the field system once none remain.
void CommPlayerMan_ResumeFieldSystemWithContextBit(int contextBit)
{
    if (contextBit != 0) {
        sCommPlayerManager->pauseBits &= ~(contextBit);
    }

    if (sCommPlayerManager->pauseBits == PAUSE_BIT_TRAPS) {
        u8 data = 1;
        CommSys_SendDataFixedSize(62, &data);
    }

    if (sCommPlayerManager->pauseBits == 0) {
        if (contextBit != PAUSE_BIT_TRAPS) {
            FieldSystem_ResumeProcessing();
            CommPlayerMan_BroadcastFieldSystemActive(1);
        } else {
            FieldSystem_ResumeProcessing();
            sCommPlayerManager->isFieldSystemActive = TRUE;
        }
    }
}

void CommPlayerMan_ClearPauseContextBits(void)
{
    sCommPlayerManager->pauseBits = 0;
}

void CommPlayerMan_PauseFieldSystem(void)
{
    FieldSystem_PauseProcessing();
    sCommPlayerManager->isFieldSystemActive = FALSE;
}

void CommPlayerMan_ResumeFieldSystem(void)
{
    FieldSystem_ResumeProcessing();
    CommPlayerMan_BroadcastFieldSystemActive(1);
}

// Resume the field system if it has not been resumed yet and the underground
// state allows it.
void CommPlayerMan_TryResumeFieldSystem(void)
{
    if (sCommPlayerManager->resumeHandled == 0) {
        if (!sCommPlayerManager->isUnderground) {
            CommPlayerMan_ResumeFieldSystem();
            sCommPlayerManager->resumeHandled = 1;
        } else if (UndergroundMan_ShouldFieldSystemBeResumed(CommSys_CurNetId())) {
            CommPlayerMan_ResumeFieldSystem();
            sCommPlayerManager->resumeHandled = 1;
        } else {
            (void)0;
        }
    }
}

// Pause the field system if the underground state says it should not be
// resumed.
void CommPlayerMan_TryPauseFieldSystem(void)
{
    if (sCommPlayerManager->resumeHandled == 0) {
        if (sCommPlayerManager->isUnderground) {
            if (!UndergroundMan_ShouldFieldSystemBeResumed(CommSys_CurNetId())) {
                CommPlayerMan_BroadcastFieldSystemActive(0);
            }
        }
    }
}

// Snap every remote avatar to the position the manager believes it is at.
void CommPlayerMan_ForcePos(void)
{
    int netId, x, z, dir;
    MapObject *obj;

    if (!sCommPlayerManager) {
        return;
    }

    for (netId = 0; netId < MAX_CONNECTED_PLAYERS; netId++) {
        if (sCommPlayerManager->playerAvatar[netId] == NULL) {
            continue;
        }

        if (CommSys_CurNetId() == netId) {
            continue;
        }

        obj = PlayerAvatar_GetMapObject(sCommPlayerManager->playerAvatar[netId]);
        x = sCommPlayerManager->playerLocation[netId].x;
        z = sCommPlayerManager->playerLocation[netId].z;
        dir = sCommPlayerManager->playerLocation[netId].dir;

        MapObject_SetX(obj, x);
        MapObject_SetZ(obj, z);
        MapObject_SetPosDirFromCoords(obj, x, 0, z, dir);
    }
}

// Lock the local player's facing for a few frames after a forced turn.
void CommPlayerMan_ForceDir(void)
{
    sCommPlayerManager->forceDirTimer = 8;
}

// While set, ignore the local player's own location packets during a
// secret-base transition.
void CommPlayerMan_SetInSecretBaseTransition(BOOL param0)
{
    sCommPlayerManager->inSecretBaseTransition = param0;
}
