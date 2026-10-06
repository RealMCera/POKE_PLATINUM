#ifndef POKEPLATINUM_STRUCT_0205C95C_H
#define POKEPLATINUM_STRUCT_0205C95C_H

#include "struct_defs/struct_0205C924.h"

// Ring buffer of the most recent Union Room chat messages. Once count reaches
// 30, startIndex points at the oldest entry and is overwritten next.
typedef struct UnionRoomChatLog {
    UnionRoomChatLogEntry entries[30];
    int count; // number of stored entries, saturating at 30
    int startIndex; // index of the oldest entry once the buffer is full
} UnionRoomChatLog;

#endif // POKEPLATINUM_STRUCT_0205C95C_H
