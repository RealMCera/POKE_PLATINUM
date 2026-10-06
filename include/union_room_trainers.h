#ifndef POKEPLATINUM_UNION_ROOM_TRAINERS_H
#define POKEPLATINUM_UNION_ROOM_TRAINERS_H

#include "constants/heap.h"

#include "struct_decls/map_object_manager.h"
#include "struct_decls/struct_0205B43C_decl.h"
#include "struct_decls/struct_0205C22C_decl.h"
#include "struct_decls/struct_0205C95C_decl.h"

UnionRoomTrainers *UnionRoomTrainers_New(UnionRoom *unionRoom);
void UnionRoomTrainers_RequestAllLeave(UnionRoomTrainers *trainers);
void UnionRoomTrainers_Reset(UnionRoomTrainers *trainers);
void UnionRoomTrainers_Free(UnionRoomTrainers *trainers);
void UnionRoomTrainers_ShowConnected(MapObjectManager *mapObjMan, UnionRoomTrainers *trainers);
UnionRoomChatLog *UnionRoomChatLog_New(enum HeapID heapID);
void UnionRoomChatLog_Free(UnionRoomChatLog *chatLog);

#endif // POKEPLATINUM_UNION_ROOM_TRAINERS_H
