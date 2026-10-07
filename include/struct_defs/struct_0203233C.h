#ifndef POKEPLATINUM_STRUCT_0203233C_H
#define POKEPLATINUM_STRUCT_0203233C_H

// Cursor over the packet buffer currently being filled by the send queue.
// `remaining` is set to -1 to signal that the buffer filled up part-way
// through an entry.
typedef struct CommQueueWriter {
    u8 *cursor;    // next byte to write
    int remaining; // bytes left in the buffer; -1 once full mid-entry
} CommQueueWriter;

#endif // POKEPLATINUM_STRUCT_0203233C_H
