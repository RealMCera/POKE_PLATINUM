#ifndef POKEPLATINUM_COMM_PLAYER_MANAGER_H
#define POKEPLATINUM_COMM_PLAYER_MANAGER_H

#include "struct_defs/comm_player_location.h"

#include "field/field_system_decl.h"
#include "underground/player_status.h"

#include "communication_system.h"
#include "overworld_anim_manager.h"
#include "player_avatar.h"
#include "sys_task_manager.h"
#include "trainer_info.h"
#include "underground.h"

enum PauseBit {
    PAUSE_BIT_STOLE_FLAG = 1,
    PAUSE_BIT_TALK_WITH_FLAG = 1 << 1,
    PAUSE_BIT_LOST_FLAG = 1 << 2,
    PAUSE_BIT_TRAPS = 1 << 4,
    PAUSE_BIT_BURIED_OBJECT_WITH_FLAG = 1 << 5,
    PAUSE_BIT_LINK_PC = 1 << 6,
    PAUSE_BIT_TOUCH_RADAR = 1 << 7,
};

enum Emote {
    EMOTE_NONE = 0,
    EMOTE_FLAG,
    EMOTE_EXCLAMATION,
    EMOTE_OK,
};

// Unused placeholder retained for layout; no code reads or writes it.
typedef struct DummiedTalkStruct {
    u8 field0 : 4;
    u8 field1 : 4;
    u8 field2 : 1;
    u8 field3 : 1;
} DummiedTalkStruct;

// A captured flag's owner, as exchanged over the link connection.
typedef struct HeldFlagInfo {
    u8 ownerInfo[sizeof(TrainerInfo)];
    u16 netID;
} HeldFlagInfo;

// Tracks every player in a link session: their avatars, the positions the
// server and clients believe them to be at, and the per-player movement state
// used to keep the two in sync. A single instance is owned by the field system
// and reached through CommPlayerMan_Get.
typedef struct CommPlayerManager {
    u32 pauseBits; // bitmask of PauseBit reasons the field system is paused
    UndergroundPlayerStatuses *playerStatuses; // only allocated underground
    PlayerAvatar *playerAvatar[MAX_CONNECTED_PLAYERS];
    OverworldAnimManager *animManager[MAX_CONNECTED_PLAYERS];
    u8 isActive[MAX_CONNECTED_PLAYERS];
    SysTask *task;
    FieldSystem *fieldSystem;
    DummiedTalkStruct dummy;
    u8 talkCount[MAX_CONNECTED_PLAYERS];
    CommPlayerLocation playerLocationServer[MAX_CONNECTED_PLAYERS]; // authoritative position (server)
    CommPlayerLocation playerLocation[MAX_CONNECTED_PLAYERS]; // position received from the owning client
    u8 movementEnabled[MAX_CONNECTED_PLAYERS];
    u8 movementEnabled2[MAX_CONNECTED_PLAYERS];
    u8 onBattleGrid[MAX_CONNECTED_PLAYERS]; // player is locked onto a battle-room grid tile
    u8 emote[MAX_CONNECTED_PLAYERS];
    s8 slideAnimationDir[MAX_CONNECTED_PLAYERS];
    u8 slideTilesLeft[MAX_CONNECTED_PLAYERS];
    u8 slideDir[MAX_CONNECTED_PLAYERS];
    u8 alteredMovementStepsLeft[MAX_CONNECTED_PLAYERS];
    u8 holeMovementsLeft[MAX_CONNECTED_PLAYERS];
    u8 hurlTrapTriggered[MAX_CONNECTED_PLAYERS];
    u8 movementChanged[MAX_CONNECTED_PLAYERS];
    u8 moveTimerServer[MAX_CONNECTED_PLAYERS];
    u8 moveTimer[MAX_CONNECTED_PLAYERS];
    HeldFlagInfo heldFlagInfo[MAX_CONNECTED_PLAYERS + 1];
    TrainerInfo *registeredFlagOwnerInfoUnused[MAX_CAPTURED_FLAG_RECORDS]; // this is never read from and presumably superseded by the fields in the underground struct
    TrainerInfo *heldFlagOwnerInfo[MAX_CONNECTED_PLAYERS];
    u16 unk_2B0;
    u16 flagsRegisteredInCurrentSession;
    u8 battleRoomState[4]; // battle-room state reported by each player (0/1)
    u8 menuOpen;
    u8 linksReceivedHeldFlagData;
    u8 idlePositionSent; // position already broadcast for the current idle state
    u8 sendAllPos;
    u8 isFieldSystemActive;
    u8 isDisabled;
    u8 isUnderground;
    u8 resumeHandled; // field-system resume/pause handshake already performed
    u8 forceDirTimer;
    u8 processInput; // local player is allowed to process input this frame
    u8 updatingHeldFlags;
    u8 inSecretBaseTransition; // suppress own location updates while entering/leaving a base
} CommPlayerManager;

CommPlayerManager *CommPlayerMan_Get(void);
BOOL CommPlayerMan_Init(void *dest, FieldSystem *fieldSystem, BOOL isUnderground);
void CommPlayerMan_Disable(void);
void CommPlayerMan_Restart(void);
void CommPlayerMan_Delete(BOOL deletePlayerData);
void CommPlayerMan_Reinit(void);
void CommPlayerMan_Stop(void);
void CommPlayer_InitPersonal(void);
void CommPlayer_CopyPersonal(int netJd);
void CommPlayer_SendXZPos(BOOL param0, int x, int z);
void CommPlayer_SendPos(BOOL param0);
void CommPlayer_SendPosServer(BOOL param0);
u32 CommPlayer_Size(void);
void CommPlayer_Destroy(u8 netId, BOOL param1, BOOL param2);
BOOL CommPlayerMan_IsFieldSystemActive(void);
void CommPlayerMan_BroadcastFieldSystemActive(BOOL param0);
void CommPlayer_RecvMovementEnabled(int netId, int param1, void *param2, void *unused);
void CommPlayerMan_Update(FieldSystem *fieldSystem, BOOL param1);
BOOL CommPlayer_CheckNPCCollision(int x, int z);
void CommPlayer_RecvLocation(int netId, int unused0, void *src, void *unused1);
void CommPlayer_RecvDelete(int unused0, int unused1, void *src, void *unused2);
int CommPacketSizeOf_RecvLocation(void);
void CommPlayer_RecvLocationAndInit(int netId, int size, void *src, void *unused);
void CommPlayer_StartSlide(int netId, int dir, BOOL isHurlTrap);
void CommPlayer_StopSlide(int netId);
void CommPlayer_EndCurrentSlide(int netId);
void CommPlayer_StartSlideAnimation(int netId, int dir, BOOL unused);
void CommPlayer_StopSlideAnimation(int netId);
int CommPacketSizeOf_RecvLocationAndInit(void);
BOOL CommPlayerMan_IsInputAllowed(void);
BOOL CommPlayer_IsActive(int netId);
int CommPlayer_GetXIfActive(int netId);
int CommPlayer_GetZIfActive(int netId);
int CommPlayer_GetX(int netId);
int CommPlayer_GetZ(int netId);
int CommPlayer_GetXInFrontOfPlayer(int netId);
int CommPlayer_GetZInFrontOfPlayer(int netId);
int CommPlayer_GetXServerIfActive(int netId);
int CommPlayer_GetZServerIfActive(int netId);
int CommPlayer_GetXServer(int netId);
int CommPlayer_GetZServer(int netId);
int CommPlayer_GetXInFrontOfPlayerServer(int netId);
int CommPlayer_GetZInFrontOfPlayerServer(int netId);
int CommPlayer_Dir(int netId);
int CommPlayer_DirServer(int netId);
void CommPlayer_LookTowardsServer(int netIdTarget, int netIdSet);
void CommPlayer_LookTowards(int netIdTarget, int netIdSet);
int CommPlayerMan_GetLinkNetIDAtLocation(int xPos, int zPos);
void CommPlayerMan_SetMovementEnabled(int netId, BOOL movementEnabled);
BOOL CommPlayerMan_IsMovementEnabled(int netId);
BOOL CommPlayerMan_CheckBattleGridPositions(void);
void CommPlayer_RecvBattleRoomState(int netId, int unused0, void *src, void *unused3);
void CommPlayer_SetBattleDir(void);
BOOL CommPlayerMan_StepBackFromBattleGrid(void);
int CommPlayer_GetOppositeDir(int dir);
void CommPlayerMan_SetPlayerAlteredMovement(int netId, int duration);
void CommPlayerMan_EndPlayerAlteredMovement(int netId);
void CommPlayerMan_PutPlayerInHole(int netId, int movementsToEscape);
void CommPlayerMan_RemovePlayerFromHole(int netId);
int CommPlayer_GetMovementTimer(int netId);
int CommPlayer_GetMovementTimerServer(int netId);
void CommPlayer_SetDir(int dir);
void CommPlayer_SetDirClient(int netId, int dir);
int CommPlayer_DirClient(int netId);
void CommPlayerMan_PauseFieldSystemWithContextBit(int contextBit);
void CommPlayerMan_ResumeFieldSystemWithContextBit(int contextBit);
void CommPlayerMan_ClearPauseContextBits(void);
void CommPlayerMan_PauseFieldSystem(void);
void CommPlayerMan_ResumeFieldSystem(void);
void CommPlayerMan_TryResumeFieldSystem(void);
void CommPlayerMan_TryPauseFieldSystem(void);
void CommPlayerMan_ForcePos(void);
void CommPlayerMan_ForceDir(void);
void CommPlayerMan_SetInSecretBaseTransition(BOOL param0);

#endif // POKEPLATINUM_COMM_PLAYER_MANAGER_H
