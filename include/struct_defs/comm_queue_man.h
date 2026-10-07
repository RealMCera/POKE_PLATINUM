#ifndef POKEPLATINUM_STRUCT_DEF_COMM_QUEUE_MAN_H
#define POKEPLATINUM_STRUCT_DEF_COMM_QUEUE_MAN_H

#include "struct_defs/struct_020322D8.h"
#include "struct_defs/struct_02032318.h"

#include "comm_ring.h"

// Manages the outgoing command queue for one communication channel. Commands
// are stored in a fixed-size array of CommQueueEntry; `queue` links the entries
// waiting to be sent and `current` holds the entry that was partially
// transmitted by the last flush.
typedef struct CommQueueMan {
    CommQueueList queue;     // entries waiting to be sent
    CommQueueList queueAlt;  // second list; never used (see CommQueue_Write)
    CommQueueEntry *current; // entry left partially sent by the last flush
    CommRing *ring;          // ring holding payloads copied out of `data`
    CommQueueEntry *entries; // backing array of `capacity` entries
    int capacity;            // number of entries in `entries`
} CommQueueMan;

#endif // POKEPLATINUM_STRUCT_DEF_COMM_QUEUE_MAN_H
