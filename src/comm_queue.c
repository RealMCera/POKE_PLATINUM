#include "comm_queue.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_queue_man.h"
#include "struct_defs/struct_020322D8.h"
#include "struct_defs/struct_02032318.h"
#include "struct_defs/struct_0203233C.h"

#include "comm_ring.h"
#include "communication_system.h"
#include "heap.h"
#include "unk_02032798.h"

// Returns the first unused entry in the manager's array, or NULL if every entry
// is occupied. A command byte of 0 marks a free entry.
static CommQueueEntry *CommQueueMan_FindFreeEntry(CommQueueMan *queueMan)
{
    CommQueueEntry *entry = queueMan->entries;
    int i;

    for (i = 0; i < queueMan->capacity; i++) {
        if (entry->command == 0) {
            return entry;
        }

        entry++;
    }

    return NULL;
}

BOOL CommQueue_IsEmpty(CommQueueMan *queueMan)
{
    CommQueueEntry *entry = queueMan->entries;
    int i;

    for (i = 0; i < queueMan->capacity; i++) {
        if (entry->command != 0) {
            return 0;
        }

        entry++;
    }

    return 1;
}

// Unlinks the head entry of `list`. Returns TRUE if the list was non-empty.
static BOOL CommQueueList_PopFront(CommQueueList *list)
{
    if (list->head != NULL) {
        if (list->head->next != NULL) {
            list->head = list->head->next;
            list->head->prev = NULL;
        } else {
            list->head = NULL;
            list->tail = NULL;
        }

        return 1;
    }

    return 0;
}

// Writes one byte to the output buffer and advances the cursor. Returns TRUE
// once the buffer has no room left.
static BOOL CommQueueWriter_WriteByte(CommQueueWriter *writer, u8 byte)
{
    *writer->cursor = byte;
    writer->cursor++;
    writer->remaining--;

    if (writer->remaining == 0) {
        return 1;
    }

    return 0;
}

// Emits the command header for `entry` into `writer`: the command byte, plus a
// two-byte payload length when the command has a variable size. Returns TRUE if
// the header did not fit, leaving the entry's header flag cleared.
static BOOL CommQueueEntry_WriteHeader(CommQueueEntry *entry, CommQueueWriter *writer)
{
    int packetSize = CommCmd_PacketSizeOf(entry->command);

    if (packetSize == PACKET_SIZE_VARIABLE) {
        if (writer->remaining < 3) {
            entry->headerWritten = 0;
            return 1;
        }
    } else {
        if (writer->remaining < 1) {
            entry->headerWritten = 0;
            return 1;
        }
    }

    CommQueueWriter_WriteByte(writer, entry->command);

    if (packetSize == PACKET_SIZE_VARIABLE) {
        CommQueueWriter_WriteByte(writer, (entry->remainingSize >> 8) & 0xff);
        CommQueueWriter_WriteByte(writer, entry->remainingSize & 0xff);
    } else {
        entry->remainingSize = packetSize;
    }

    entry->headerWritten = 1;
    return 0;
}

// Copies as much of `entry`'s payload as fits into `writer`. When the payload
// was staged in the comm ring, bytes are read from `ring`; otherwise they are
// copied straight from the entry's data pointer. `force` allows a partial copy
// even when the whole entry would not fit. Returns TRUE if any bytes were
// written.
static BOOL CommQueueEntry_Flush(CommQueueEntry *entry, CommQueueWriter *writer, CommRing *ring, BOOL force)
{
    int i;
    int headerSize;
    int packetSize = CommCmd_PacketSizeOf(entry->command);

    if (packetSize == PACKET_SIZE_VARIABLE) {
        headerSize = 3;
    } else {
        headerSize = 1;
    }

    if ((writer->remaining < (entry->remainingSize + headerSize)) && (!force)) {
        return 0;
    }

    if (entry->headerWritten != 1) {
        if (CommQueueEntry_WriteHeader(entry, writer)) {
            return 0;
        }
    }

    if (writer->remaining < entry->remainingSize) {
        // The output buffer fills up before the payload is exhausted: copy what
        // fits and mark the writer full with the -1 sentinel.
        if (entry->dataInRing) {
            CommRing_Read(ring, writer->cursor, writer->remaining);
        } else {
            for (i = 0; i < writer->remaining; i++) {
                writer->cursor[i] = entry->data[i];
            }
        }

        entry->data += writer->remaining;
        entry->remainingSize -= writer->remaining;
        writer->remaining = -1;

        return 1;
    }

    // The whole remaining payload fits.
    if (entry->dataInRing) {
        CommRing_Read(ring, writer->cursor, entry->remainingSize);
    } else {
        MI_CpuCopy8(entry->data, writer->cursor, entry->remainingSize);
    }

    writer->cursor += entry->remainingSize;
    writer->remaining -= entry->remainingSize;

    return 1;
}

BOOL CommQueue_Write(CommQueueMan *queueMan, int cmd, u8 *data, int size, BOOL usePrimaryQueue, BOOL copyToRing)
{
    CommQueueEntry *entry;
    CommQueueEntry *freeEntry = CommQueueMan_FindFreeEntry(queueMan);
    CommQueueList *list;
    int packetSize;

    // The selector is forced on, so every command goes to the primary queue and
    // the alternate queue is never populated.
    usePrimaryQueue = 1;

    if (freeEntry == NULL) {
        return 0;
    }

    GF_ASSERT(size < 65534);
    packetSize = CommCmd_PacketSizeOf(cmd);

    if (packetSize == PACKET_SIZE_VARIABLE) {
        packetSize = size;
    }

    if (copyToRing) {
        int ringRemaining = CommRing_RemainingSize(queueMan->ring);

        if ((packetSize + 3) >= ringRemaining) {
            return 0;
        }

        CommRring_Write(queueMan->ring, data, packetSize, 265);
        CommRing_UpdateEndPos(queueMan->ring);

        freeEntry->dataInRing = 1;
    }

    freeEntry->remainingSize = packetSize;
    freeEntry->command = cmd;
    freeEntry->data = data;

    if (usePrimaryQueue == 1) {
        list = &queueMan->queue;
    } else {
        list = &queueMan->queueAlt;
    }

    if (list->tail == NULL) {
        list->tail = freeEntry;
        list->head = freeEntry;
    } else {
        list->tail->next = freeEntry;
        freeEntry->prev = list->tail;
        list->tail = freeEntry;
    }

    return 1;
}

// Returns the entry to transmit next: the partially-sent `current` entry if
// there is one, otherwise the head of the primary queue, then the alternate.
static CommQueueEntry *CommQueueMan_GetNextEntry(CommQueueMan *queueMan)
{
    if (queueMan->current != NULL) {
        return queueMan->current;
    }

    if (queueMan->queue.head != NULL) {
        return queueMan->queue.head;
    }

    if (queueMan->queueAlt.head != NULL) {
        return queueMan->queueAlt.head;
    }

    return NULL;
}

// Removes the entry returned by CommQueueMan_GetNextEntry: clears `current`, or
// pops the head of the primary queue, falling back to the alternate.
static void CommQueueMan_PopEntry(CommQueueMan *queueMan)
{
    if (queueMan->current != NULL) {
        queueMan->current = NULL;
    } else {
        if (!CommQueueList_PopFront(&queueMan->queue)) {
            CommQueueList_PopFront(&queueMan->queueAlt);
        }
    }
}

// Drains queued commands into `writer` until it is full or the queue is empty.
// The first entry is always allowed to be split across calls; `force` controls
// whether later entries may be split too. Any space left in the buffer is
// padded with 0xEE. Returns FALSE if the buffer filled up part-way through an
// entry, which is kept as `current` for the next call.
BOOL CommQueueMan_Flush(CommQueueMan *queueMan, CommQueueWriter *writer, BOOL force)
{
    int i;
    int forceCurrent = 1;

    while (writer->remaining > 0) {
        CommQueueEntry *entry = CommQueueMan_GetNextEntry(queueMan);

        if (NULL == entry) {
            break;
        }

        CommQueueMan_PopEntry(queueMan);

        if (!CommQueueEntry_Flush(entry, writer, queueMan->ring, forceCurrent)) {
            queueMan->current = entry;
            break;
        }

        if (-1 == writer->remaining) {
            queueMan->current = entry;
            return 0;
        } else {
            MI_CpuFill8(entry, 0, sizeof(CommQueueEntry));
        }

        forceCurrent = force;
    }

    // Pad the unused tail of the packet so its length is deterministic.
    for (i = 0; i < writer->remaining; i++) {
        *writer->cursor = 0xee;
        writer->cursor++;
    }

    return 1;
}

void CommQueueMan_Init(CommQueueMan *queueMan, int capacity, CommRing *ring)
{
    MI_CpuFill8(queueMan, 0, sizeof(CommQueueMan));
    queueMan->entries = Heap_Alloc(HEAP_ID_COMMUNICATION, sizeof(CommQueueEntry) * capacity);

    MI_CpuFill8(queueMan->entries, 0, sizeof(CommQueueEntry) * capacity);
    queueMan->capacity = capacity;
    queueMan->ring = ring;
}

void CommQueueMan_Reset(CommQueueMan *queueMan)
{
    MI_CpuFill8(queueMan->entries, 0, sizeof(CommQueueEntry) * queueMan->capacity);

    queueMan->queue.head = NULL;
    queueMan->queue.tail = NULL;
    queueMan->queueAlt.head = NULL;
    queueMan->queueAlt.tail = NULL;
    queueMan->current = NULL;
}

void CommQueueMan_Delete(CommQueueMan *queueMan)
{
    Heap_Free(queueMan->entries);
}

BOOL CommQueueMan_IsCmdInQueue(CommQueueMan *queueMan, int cmd)
{
    int i;
    CommQueueEntry *entry = queueMan->entries;

    for (i = 0; i < queueMan->capacity; i++) {
        if (entry->command == cmd) {
            return 1;
        }

        entry++;
    }

    return 0;
}
