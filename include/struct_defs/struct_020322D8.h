#ifndef POKEPLATINUM_STRUCT_020322D8_H
#define POKEPLATINUM_STRUCT_020322D8_H

#include "struct_defs/struct_020322D8.h"

// One queued outgoing communication command. Entries live in a fixed-size
// array owned by a CommQueueMan and are linked into a CommQueueList while they
// wait to be transmitted.
typedef struct CommQueueEntry {
    u8 *data;                     // payload cursor, advanced as bytes are sent
    struct CommQueueEntry *prev;  // previous entry in the send list
    struct CommQueueEntry *next;  // next entry in the send list
    u16 remainingSize;            // payload bytes not yet written to the output
    u8 command;                   // command byte; 0 marks a free (unused) entry
    u8 headerWritten : 1;         // command header has been emitted for this entry
    u8 dataInRing : 1;            // payload was staged in the CommRing, not `data`
    u8 : 6;
} CommQueueEntry;

#endif // POKEPLATINUM_STRUCT_020322D8_H
