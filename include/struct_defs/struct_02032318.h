#ifndef POKEPLATINUM_STRUCT_02032318_H
#define POKEPLATINUM_STRUCT_02032318_H

#include "struct_defs/struct_020322D8.h"

// Head and tail of a doubly-linked list of CommQueueEntry. Entries are
// unlinked from the front as they are transmitted.
typedef struct CommQueueList {
    CommQueueEntry *head;
    CommQueueEntry *tail;
} CommQueueList;

#endif // POKEPLATINUM_STRUCT_02032318_H
