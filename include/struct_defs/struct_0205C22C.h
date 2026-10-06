#ifndef POKEPLATINUM_STRUCT_0205C22C_H
#define POKEPLATINUM_STRUCT_0205C22C_H

#include "struct_decls/struct_0205B43C_decl.h"
#include "struct_decls/struct_0205C95C_decl.h"
#include "struct_defs/struct_0205C680.h"

#include "field/field_system_decl.h"

#include "pal_pad.h"
#include "player_avatar.h"
#include "sys_task_manager.h"

typedef struct UnionRoomTrainers {
    UnionRoom *unionRoom;
    SysTask *task;
    PlayerAvatar *playerAvatar;
    UnionRoomTrainer slots[50 + 1]; // 50 remote trainers, plus the player at slot 50
    FieldSystem *fieldSystem;
    PalPad *palPad;
    UnionRoomChatLog *chatLog;
    int unk_47C;
    int unk_480;
} UnionRoomTrainers;

#endif // POKEPLATINUM_STRUCT_0205C22C_H
