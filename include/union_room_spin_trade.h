#ifndef POKEPLATINUM_UNION_ROOM_SPIN_TRADE_H
#define POKEPLATINUM_UNION_ROOM_SPIN_TRADE_H

#include "struct_decls/struct_0209C194_decl.h"
#include "struct_defs/struct_0209C194_1.h"

#include "field/field_system_decl.h"

// Creates the field-task state for a spin trade; pair it with
// UnionRoomSpinTrade_Update, which is called each frame until it returns TRUE.
void *UnionRoomSpinTrade_New(FieldSystem *fieldSystem);
BOOL UnionRoomSpinTrade_Update(void *param0);

UnionRoomSpinTradeSession *UnionRoomSpinTrade_NewSession(UnionRoomSpinTradeContext *param0, enum HeapID heapID);
void UnionRoomSpinTrade_FreeSession(UnionRoomSpinTradeSession *param0);
BOOL UnionRoomSpinTrade_IsGroupConfirmed(UnionRoomSpinTradeSession *param0);

#endif // POKEPLATINUM_UNION_ROOM_SPIN_TRADE_H
