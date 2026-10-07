#ifndef POKEPLATINUM_FIELD_MOVE_MON_H
#define POKEPLATINUM_FIELD_MOVE_MON_H

#include "struct_defs/struct_020711C8.h"

#include "savedata.h"

FieldMoveMon *FieldMoveMon_New(const enum HeapID heapID, const u8 fieldMonId, SaveData *saveData);

#endif // POKEPLATINUM_FIELD_MOVE_MON_H
