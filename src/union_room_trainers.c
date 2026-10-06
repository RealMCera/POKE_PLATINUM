#include "union_room_trainers.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"
#include "generated/movement_types.h"

#include "struct_decls/map_object.h"
#include "struct_decls/map_object_manager.h"
#include "struct_decls/struct_0205B43C_decl.h"
#include "struct_defs/struct_0203330C.h"
#include "struct_defs/struct_0205B4F8.h"
#include "struct_defs/struct_0205C22C.h"
#include "struct_defs/struct_0205C680.h"
#include "struct_defs/struct_0205C924.h"
#include "struct_defs/struct_0205C95C.h"

#include "field/field_system.h"
#include "overlay005/ov5_021F134C.h"
#include "overlay005/ov5_021F600C.h"

#include "comm_manager.h"
#include "easy_chat_sentence.h"
#include "field_task.h"
#include "heap.h"
#include "map_object.h"
#include "overworld_anim_manager.h"
#include "pal_pad.h"
#include "player_avatar.h"
#include "savedata.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "trainer_info.h"
#include "union_room.h"
#include "map_object_animation.h"

#include "constdata/const_020ED570.h"

// Union Room trainer manager. The Union Room (union_room.c) broadcasts a
// wireless beacon descriptor (WMBssDesc) for each connected player. This module
// mirrors those descriptors onto the map objects that represent the other
// players: it reads each player's appearance and Pal Pad friend status, warps
// their map object in when they connect and out when they leave, and drives the
// small warp/friend visual effects. It also owns a ring buffer of the most
// recent easy chat messages (UnionRoomChatLog) shown by the Union Room chat
// overlay in overlay056.
//
// Slots 0-49 correspond to the other players' map objects; slot 50 is the
// player's own map object, which only needs the effect timer ticked.

static void UnionRoomTrainers_Update(SysTask *task, void *param1);
static void UnionRoomTrainers_UpdateFromBssDescs(UnionRoomTrainers *trainers, UnionRoom *unionRoom, MapObjectManager *mapObjMan, PalPad *palPad);
static int UnionRoomTrainers_UpdateGroup(UnionRoomTrainers *trainers, int group, WMBssDesc *bssDesc, PalPad *palPad);
static void UnionRoomTrainers_UpdateMapObjects(UnionRoomTrainers *trainers, MapObjectManager *mapObjMan);
static void UnionRoomTrainers_FinishEffects(UnionRoomTrainer *trainer, int finishFriendEffect);
static void UnionRoomTrainers_TickEffectTimer(UnionRoomTrainer *trainer);
static void UnionRoomTrainers_WarpIn(UnionRoomTrainer *trainer, MapObject *mapObj, int playerX, int playerZ);
static void UnionRoomTrainers_WarpOut(UnionRoomTrainer *trainer, MapObject *mapObj);
static void UnionRoomTrainers_StartWarpEffect(UnionRoomTrainer *trainer, MapObject *mapObj);
static void UnionRoomTrainers_HideMapObjects(MapObjectManager *mapObjMan, int first, int last);
static void UnionRoomTrainers_SetDesiredState(UnionRoomTrainer trainers[], int slot, int state);
void UnionRoomChatLog_Free(UnionRoomChatLog *chatLog);
UnionRoomChatLog *UnionRoomChatLog_New(enum HeapID heapID);
static void UnionRoomChatLog_FreeAllEntries(UnionRoomChatLog *chatLog);
static void UnionRoomChatLog_FreeEntry(UnionRoomChatLogEntry *entry);
static void UnionRoomChatLog_Init(UnionRoomChatLog *chatLog);
static void UnionRoomChatLog_InitEntry(UnionRoomChatLogEntry *entry);

UnionRoomTrainers *UnionRoomTrainers_New(UnionRoom *unionRoom)
{
    UnionRoomTrainers *trainers = (UnionRoomTrainers *)Heap_Alloc(HEAP_ID_31, sizeof(UnionRoomTrainers));

    MI_CpuClearFast(trainers, sizeof(UnionRoomTrainers));

    trainers->unionRoom = unionRoom;
    trainers->unk_47C = 1;
    trainers->task = SysTask_Start(UnionRoomTrainers_Update, trainers, 11);
    trainers->fieldSystem = UnionRoom_GetFieldSystem(unionRoom);
    trainers->palPad = SaveData_SaveTable(trainers->fieldSystem->saveData, SAVE_TABLE_ENTRY_PAL_PAD);
    trainers->playerAvatar = trainers->fieldSystem->playerAvatar;

    Heap_CreateAtEnd(HEAP_ID_FIELD2, HEAP_ID_89, 10000);
    trainers->chatLog = UnionRoomChatLog_New(HEAP_ID_89);
    UnionRoomTrainers_Reset(trainers);

    return trainers;
}

// Requests that every trainer currently on the map warp out. Called when the
// player leaves the Union Room or restarts the search.
void UnionRoomTrainers_RequestAllLeave(UnionRoomTrainers *trainers)
{
    int i;

    for (i = 0; i < 50 + 1; i++) {
        if (trainers->slots[i].phase != 0) {
            trainers->slots[i].desiredState = 3;
        }
    }
}

// Clears every trainer slot, including the player's (slot 50).
void UnionRoomTrainers_Reset(UnionRoomTrainers *trainers)
{
    int i;

    for (i = 0; i < 50 + 1; i++) {
        trainers->slots[i].desiredState = 0;
        trainers->slots[i].phase = 0;
        trainers->slots[i].friendRank = 0;
        trainers->slots[i].startWarpEffect = 0;
    }
}

void UnionRoomTrainers_Free(UnionRoomTrainers *trainers)
{
    SysTask_Done(trainers->task);
    UnionRoomChatLog_Free(trainers->chatLog);
    Heap_Destroy(HEAP_ID_89);
    Heap_Free(trainers);
}

// SysTask callback: while no field task is running, refresh the trainer slots
// from the Union Room's beacon descriptors and advance their map objects.
static void UnionRoomTrainers_Update(SysTask *task, void *param1)
{
    UnionRoomTrainers *trainers = (UnionRoomTrainers *)param1;
    UnionRoom *unionRoom = trainers->unionRoom;

    if (!FieldSystem_IsRunningTask(trainers->fieldSystem)) {
        trainers->playerAvatar = trainers->fieldSystem->playerAvatar;
        UnionRoomTrainers_UpdateFromBssDescs(trainers, unionRoom, trainers->fieldSystem->mapObjMan, trainers->palPad);
        UnionRoomTrainers_UpdateMapObjects(trainers, trainers->fieldSystem->mapObjMan);
    }
}

// Applies one group's beacon descriptor to its four map object slots. Returns
// whether any slot in the group is occupied.
static int UnionRoomTrainers_UpdateGroup(UnionRoomTrainers *trainers, int group, WMBssDesc *bssDesc, PalPad *palPad)
{
    int i, slot, anyPresent = 0;
    UnkStruct_0203330C *gameInfo;
    UnkStruct_0205B4F8 *activity;
    MapObject *mapObj;

    if (bssDesc == NULL) {
        // No beacon for this group: request all four slots to leave.
        for (i = 0; i < 4; i++) {
            slot = gUnionRoomTrainerGroupBaseSlots[group] + i;
            UnionRoomTrainers_SetDesiredState(trainers->slots, slot, 3);
        }

        return 0;
    }

    gameInfo = (UnkStruct_0203330C *)bssDesc->gameInfo.userGameInfo;
    activity = (UnkStruct_0205B4F8 *)gameInfo->unk_30;

    if (trainers->slots[group].trainerId != gameInfo->unk_00) {
        // The group leader changed; drop the whole group and let it respawn.
        for (i = 0; i < 4; i++) {
            slot = gUnionRoomTrainerGroupBaseSlots[group] + i;
            UnionRoomTrainers_SetDesiredState(trainers->slots, slot, 3);
        }

        return 0;
    }

    for (i = 0; i < 4; i++) {
        slot = gUnionRoomTrainerGroupBaseSlots[group] + i;

        switch (trainers->slots[slot].phase) {
        case 0:
            if (activity->unk_18[i] != 0) {
                // A player occupies this slot; show them.
                trainers->slots[slot].desiredState = 2;
                trainers->slots[slot].appearance = (activity->unk_18[i] & 0x7f);
                trainers->slots[slot].friendRank = PalPad_TrainerIsFriend(palPad, activity->unk_00[i]);
                anyPresent = 1;
            }
            break;
        case 2:
            if (activity->unk_18[i] == 0) {
                // The player left; request the slot to warp out.
                UnionRoomTrainers_SetDesiredState(trainers->slots, slot, 3);
                {
                    int j;

                    // No-op loop retained from the original decompilation.
                    for (j = 0; j < 4; j++) {
                        (void)0;
                    }
                }
            } else {
                anyPresent = 1;
            }
            break;
        case 4:
            trainers->slots[slot].desiredState = 0;
            break;
        }
    }

    return anyPresent;
}

static void UnionRoomTrainers_SetDesiredState(UnionRoomTrainer trainers[], int slot, int state)
{
    trainers[slot].desiredState = state;
}

// Refreshes the ten trainer groups from the Union Room's beacon descriptors.
static void UnionRoomTrainers_UpdateFromBssDescs(UnionRoomTrainers *trainers, UnionRoom *unionRoom, MapObjectManager *mapObjMan, PalPad *palPad)
{
    WMBssDesc *bssDesc;
    int group;
    UnkStruct_0203330C *gameInfo;
    TrainerInfo *trainerInfo;

    for (group = 0; group < 10; group++) {
        bssDesc = UnionRoom_GetBssDesc(unionRoom, group);

        if (bssDesc != NULL) {
            gameInfo = (UnkStruct_0203330C *)bssDesc->gameInfo.userGameInfo;
            trainerInfo = (TrainerInfo *)gameInfo->unk_10;
        } else {
            gameInfo = NULL;
            trainerInfo = NULL;
        }

        switch (trainers->slots[group].phase) {
        case 0:
            if (bssDesc != NULL) {
                trainers->slots[group].appearance = TrainerInfo_Appearance(trainerInfo);
                trainers->slots[group].friendRank = PalPad_TrainerIsFriend(palPad, TrainerInfo_ID(trainerInfo));
                trainers->slots[group].trainerId = gameInfo->unk_00;

                if (UnionRoomTrainers_UpdateGroup(trainers, group, bssDesc, palPad)) {
                    trainers->slots[group].desiredState = 2;
                } else {
                    trainers->slots[group].desiredState = 1;
                }
            }
            break;
        case 2:
            if (bssDesc == NULL) {
                UnionRoomTrainers_SetDesiredState(trainers->slots, group, 3);
            } else {
                if (trainers->slots[group].trainerId != gameInfo->unk_00) {
                    UnionRoomTrainers_SetDesiredState(trainers->slots, group, 3);
                }
            }

            if (UnionRoomTrainers_UpdateGroup(trainers, group, bssDesc, palPad)) {
                if (trainers->slots[group].movementSet == 1) {
                    trainers->slots[group].desiredState = 3;
                }
            }
            break;
        case 4:
            trainers->slots[group].desiredState = 0;
            break;
        }
    }
}

// Advances the map object for each trainer slot according to its phase.
static void UnionRoomTrainers_UpdateMapObjects(UnionRoomTrainers *trainers, MapObjectManager *mapObjMan)
{
    MapObject *mapObj;
    int i, playerX, playerZ;

    GF_ASSERT(trainers->playerAvatar != NULL);

    playerX = PlayerAvatar_GetXPos(trainers->playerAvatar);
    playerZ = PlayerAvatar_GetZPos(trainers->playerAvatar);

    for (i = 0; i < 50; i++) {
        mapObj = MapObjMan_LocalMapObjByIndex(mapObjMan, i + 1);

        if (mapObj == NULL) {
            GF_ASSERT(FALSE);
        }

        switch (trainers->slots[i].phase) {
        case 0:
            // Idle: warp the trainer in once the map object's animation is ready.
            if (LocalMapObj_IsAnimationSet(mapObj) == 1) {
                int state = trainers->slots[i].desiredState;

                if ((state == 1) || (state == 2)) {
                    UnionRoomTrainers_WarpIn(&trainers->slots[i], mapObj, playerX, playerZ);
                }
            }
            break;
        case 1:
            // Warping in: wait for the warp-in animation to finish.
            if (LocalMapObj_IsAnimationSet(mapObj) == 1) {
                if (trainers->slots[i].desiredState == 3) {
                    // Left before finishing; reset and hide.
                    trainers->slots[i].phase = 0;
                    trainers->slots[i].desiredState = 0;
                    UnionRoomTrainers_FinishEffects(&trainers->slots[i], 1);
                    continue;
                }

                LocalMapObj_ClearAnimation(mapObj);
                MapObject_SetStatus19(mapObj, 0);

                if ((trainers->slots[i].desiredState == 1) && (trainers->slots[i].movementSet == 0)) {
                    // A newly arrived trainer wanders around its spawn point.
                    MapObject_SwitchMovementType(mapObj, MOVEMENT_TYPE_WANDER_AROUND);
                    MapObject_SetMovementRangeX(mapObj, 1);
                    MapObject_SetMovementRangeZ(mapObj, 1);
                    trainers->slots[i].movementSet = 1;
                }

                trainers->slots[i].phase = 2;
                trainers->slots[i].desiredState = 0;
            }
            break;
        case 2:
            // Present: run the warp effect and, if requested, warp out.
            if (LocalMapObj_IsAnimationSet(mapObj) == 1) {
                UnionRoomTrainers_StartWarpEffect(&trainers->slots[i], mapObj);

                if (trainers->slots[i].desiredState == 3) {
                    UnionRoomTrainers_WarpOut(&trainers->slots[i], mapObj);
                }

                UnionRoomTrainers_TickEffectTimer(&trainers->slots[i]);
            }
            break;
        case 3:
            // Warping out: hide the map object once the animation finishes.
            if (LocalMapObj_IsAnimationSet(mapObj) == 1) {
                LocalMapObj_ClearAnimation(mapObj);

                trainers->slots[i].phase = 4;
                trainers->slots[i].desiredState = 0;
                trainers->slots[i].movementSet = 0;

                MapObject_SetHidden(mapObj, 1);
                MapObject_SetStatus18(mapObj, 0);
            }
            break;
        case 4:
            // Hidden: ready to be reused.
            trainers->slots[i].phase = 0;
            break;
        }
    }

    // Slot 50 is the player's own map object; it only needs the effect timer.
    UnionRoomTrainers_StartWarpEffect(&trainers->slots[50], PlayerAvatar_GetMapObject(trainers->playerAvatar));
    UnionRoomTrainers_TickEffectTimer(&trainers->slots[50]);
}

// Finishes the warp effect, and optionally the friend effect, for a slot.
static void UnionRoomTrainers_FinishEffects(UnionRoomTrainer *trainer, int finishFriendEffect)
{
    if (trainer->warpEffect != NULL) {
        if (OverworldAnimManager_IsActive(trainer->warpEffect)) {
            OverworldAnimManager_Finish(trainer->warpEffect);
        }

        trainer->warpEffect = NULL;
    }

    if (finishFriendEffect) {
        if (trainer->friendEffect != NULL) {
            if (OverworldAnimManager_IsActive(trainer->friendEffect)) {
                OverworldAnimManager_Finish(trainer->friendEffect);
            }

            trainer->friendEffect = NULL;
        }
    }
}

// Counts down the warp effect timer and finishes the effect when it expires.
static void UnionRoomTrainers_TickEffectTimer(UnionRoomTrainer *trainer)
{
    if (trainer->effectActive) {
        trainer->effectTimer--;

        if (trainer->effectTimer == 0) {
            UnionRoomTrainers_FinishEffects(trainer, 0);
            trainer->effectActive = 0;
        }
    }
}

// Warps a trainer's map object in at its spawn point, playing the teleport
// sound and spawning the friend effect if the trainer is a Pal Pad friend.
static void UnionRoomTrainers_WarpIn(UnionRoomTrainer *trainer, MapObject *mapObj, int playerX, int playerZ)
{
    int x = MapObject_GetXInitial(mapObj);
    int y = MapObject_GetYInitial(mapObj);
    int z = MapObject_GetZInitial(mapObj);

    if ((x == playerX) && (z == playerZ)) {
        // The trainer would spawn on top of the player; skip.
        return;
    }

    Sound_PlayEffect(SEQ_SE_DP_TELE2_sseq);
    MapObject_ChangeGraphics(mapObj, trainer->appearance);
    UnionRoomTrainers_FinishEffects(trainer, 0);
    MapObject_SetPosDirFromCoords(mapObj, x, y, z, 1);
    MapObject_Face(mapObj, 1);
    LocalMapObj_SetAnimationCode(mapObj, MOVEMENT_ACTION_WARP_IN);
    MapObject_SetHidden(mapObj, 0);
    MapObject_SetStatus18(mapObj, 1);

    trainer->phase = 1;

    if (trainer->friendRank != 0) {
        if (trainer->friendRank == 1) {
            trainer->friendEffect = ov5_021F16D4(mapObj, 1);
        } else if (trainer->friendRank >= 2) {
            trainer->friendEffect = ov5_021F16D4(mapObj, 2);
        }

        trainer->friendRank = 0;
    }
}

// Warps a trainer's map object out and stops its movement.
static void UnionRoomTrainers_WarpOut(UnionRoomTrainer *trainer, MapObject *mapObj)
{
    LocalMapObj_SetAnimationCode(mapObj, MOVEMENT_ACTION_WARP_OUT);
    MapObject_SetStatus19(mapObj, 1);
    MapObject_SwitchMovementType(mapObj, MOVEMENT_TYPE_NONE);
    UnionRoomTrainers_FinishEffects(trainer, 1);

    trainer->effectActive = 0;
    trainer->effectTimer = 0;
    trainer->phase = 3;
}

// Starts the warp-in visual effect once, when the slot is flagged for it.
static void UnionRoomTrainers_StartWarpEffect(UnionRoomTrainer *trainer, MapObject *mapObj)
{
    if (trainer->startWarpEffect == 1) {
        if (trainer->effectActive == 0) {
            trainer->warpEffect = ov5_021F6094(mapObj);
            trainer->effectTimer = 30;
            trainer->startWarpEffect = 0;
            trainer->effectActive = 1;
        }
    }
}

// Hides the map objects in the half-open slot range [first, last).
static void UnionRoomTrainers_HideMapObjects(MapObjectManager *mapObjMan, int first, int last)
{
    int i;
    MapObject *mapObj;

    for (i = first; i < last; i++) {
        mapObj = MapObjMan_LocalMapObjByIndex(mapObjMan, i);

        if (mapObj == NULL) {
            GF_ASSERT(FALSE);
        }

        MapObject_SetHidden(mapObj, 1);
        MapObject_SetStatus18(mapObj, 0);
        MapObject_SetStatus19(mapObj, 1);
    }
}

// Shows the map objects for the trainers that are already connected when the
// player enters the Union Room. Called from the field script that opens the
// room. If we are not connected to the Union Room server/client, every remote
// slot is hidden instead.
void UnionRoomTrainers_ShowConnected(MapObjectManager *mapObjMan, UnionRoomTrainers *trainers)
{
    MapObject *mapObj;
    UnionRoomTrainer *trainer;

    mapObj = MapObjMan_LocalMapObjByIndex(mapObjMan, 0);

    if (mapObj == NULL) {
        GF_ASSERT(FALSE);
    }

    if (LocalMapObj_IsAnimationSet(mapObj) == 1) {
        if (CommManager_IsConnectUnionServer() || CommManager_IsConnectedUnionClientSuccess()) {
            int i;

            for (i = 0; i < 10; i++) {
                trainer = &trainers->slots[i];

                if (trainer->phase != 1) {
                    continue;
                }

                mapObj = MapObjMan_LocalMapObjByIndex(mapObjMan, i + 1);

                if (mapObj == NULL) {
                    GF_ASSERT(FALSE);
                }

                MapObject_ChangeGraphics(mapObj, trainer->appearance);
                MapObject_Face(mapObj, 1);
                LocalMapObj_SetAnimationCode(mapObj, MOVEMENT_ACTION_WARP_IN);
                MapObject_SetHidden(mapObj, 0);
                MapObject_SetStatus18(mapObj, 1);

                trainer->phase = 1;

                if (trainer->friendRank != 0) {
                    if (trainer->friendRank == 1) {
                        trainer->friendEffect = ov5_021F16D4(mapObj, 1);
                    } else if (trainer->friendRank >= 2) {
                        trainer->friendEffect = ov5_021F16D4(mapObj, 2);
                    }

                    trainer->friendRank = 0;
                }
            }

            UnionRoomTrainers_HideMapObjects(mapObjMan, 11, 51);
        } else {
            UnionRoomTrainers_HideMapObjects(mapObjMan, 1, 51);
        }
    }
}

static void UnionRoomChatLog_InitEntry(UnionRoomChatLogEntry *entry)
{
    entry->trainerName = String_Init(7 + 1, HEAP_ID_89);
    entry->sentenceString = NULL;
    entry->palPadString = NULL;

    EasyChatSentence_InitWithType(&entry->sentence, EASY_CHAT_SENTENCE_TYPE_PRE_BATTLE);

    entry->gender = 0;
    entry->trainerId = 0;
}

static void UnionRoomChatLog_Init(UnionRoomChatLog *chatLog)
{
    int i;

    for (i = 0; i < 30; i++) {
        UnionRoomChatLog_InitEntry(&chatLog->entries[i]);
    }

    chatLog->count = 0;
    chatLog->startIndex = 0;
}

static void UnionRoomChatLog_FreeEntry(UnionRoomChatLogEntry *entry)
{
    Heap_Free(entry->trainerName);

    if (entry->sentenceString != NULL) {
        String_Free(entry->sentenceString);
    }

    if (entry->palPadString != NULL) {
        String_Free(entry->palPadString);
    }
}

static void UnionRoomChatLog_FreeAllEntries(UnionRoomChatLog *chatLog)
{
    int i;

    for (i = 0; i < 30; i++) {
        UnionRoomChatLog_FreeEntry(&chatLog->entries[i]);
    }
}

UnionRoomChatLog *UnionRoomChatLog_New(enum HeapID heapID)
{
    UnionRoomChatLog *chatLog = Heap_Alloc(heapID, sizeof(UnionRoomChatLog));

    UnionRoomChatLog_Init(chatLog);
    return chatLog;
}

void UnionRoomChatLog_Free(UnionRoomChatLog *chatLog)
{
    UnionRoomChatLog_FreeAllEntries(chatLog);
    Heap_Free(chatLog);
}
