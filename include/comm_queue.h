#ifndef POKEPLATINUM_COMM_QUEUE_H
#define POKEPLATINUM_COMM_QUEUE_H

#include "struct_defs/comm_queue_man.h"
#include "struct_defs/struct_0203233C.h"

#include "comm_ring.h"

BOOL CommQueue_IsEmpty(CommQueueMan *queueMan);
BOOL CommQueue_Write(CommQueueMan *queueMan, int cmd, u8 *data, int size, BOOL usePrimaryQueue, BOOL copyToRing);
BOOL CommQueueMan_Flush(CommQueueMan *queueMan, CommQueueWriter *writer, BOOL force);
void CommQueueMan_Init(CommQueueMan *queueMan, int capacity, CommRing *ring);
void CommQueueMan_Reset(CommQueueMan *queueMan);
void CommQueueMan_Delete(CommQueueMan *queueMan);
BOOL CommQueueMan_IsCmdInQueue(CommQueueMan *queueMan, int cmd);

#endif // POKEPLATINUM_COMM_QUEUE_H
