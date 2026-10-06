#ifndef POKEPLATINUM_STRUCT_0202FD30_H
#define POKEPLATINUM_STRUCT_0202FD30_H

#include "struct_defs/struct_02078B40.h"

// A party serialized into a recording. `mons` holds the box-format Pokémon
// data produced by sub_02078B40 and consumed by sub_02078E0C.
typedef struct {
    u16 capacity;
    u16 count;
    UnkStruct_02078B40 mons[6];
} BattleRecordingParty;

#endif // POKEPLATINUM_STRUCT_0202FD30_H
