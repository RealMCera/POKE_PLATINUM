#ifndef POKEPLATINUM_UNION_ROOM_COMM_H
#define POKEPLATINUM_UNION_ROOM_COMM_H

#include "constants/heap.h"

#include "struct_decls/struct_0209C194_decl.h"
#include "struct_defs/struct_0209BDF8.h"

// Communication command handler for the Union Room (overlay 109). It owns the
// command table registered with CommCmd_Init and exposes the send entry point
// plus the per-player party-data buffers used by the spin-trade flow.
UnionRoomComm *UnionRoomComm_New(UnionRoomSpinTradeSession *app, enum HeapID heapID);
void UnionRoomComm_Free(UnionRoomComm *comm);
void UnionRoomComm_Init(UnionRoomComm *comm);
void UnionRoomComm_Reset(UnionRoomComm *comm);
BOOL UnionRoomComm_Send(UnionRoomComm *comm, u32 command, const void *data, u32 size);
int UnionRoomComm_CountConnectedTrainers(void);
void *UnionRoomComm_GetRecvTrainerData(UnionRoomComm *comm, int netId);

#endif // POKEPLATINUM_UNION_ROOM_COMM_H
