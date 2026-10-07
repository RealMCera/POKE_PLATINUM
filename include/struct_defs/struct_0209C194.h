#ifndef POKEPLATINUM_STRUCT_0209C194_H
#define POKEPLATINUM_STRUCT_0209C194_H

#include "struct_defs/struct_0209BDF8.h"
#include "struct_defs/struct_0209C194_1.h"

#include "overlay109/struct_ov109_021D0F70_decl.h"
#include "overlay109/struct_ov109_021D5140_decl.h"

// Shared state for one Union Room spin trade session. The spin trade field
// task (union_room_spin_trade.c) creates it, then passes it as the argument to
// the two overlay109 apps that make up the session: the group confirmation
// app, followed by the spin trade app. UnionRoomComm also holds a pointer to
// it and dispatches incoming commands to whichever app is active.
typedef struct UnionRoomSpinTradeSession {
    int unk_00; // Written by the field task (1 = egg menu, 3 = spin app) but never read.
    int selectedMonSlot; // Party slot of the egg chosen for the trade.
    int connectedCount; // Number of connected trainers, written when the group app exits.
    u32 connectedBitmap; // Net ID bitmap of the connected trainers.
    BOOL groupConfirmed; // Set when the group app confirms the trade; lets the field task continue.
    UnionRoomSpinTradeContext context; // Savedata, options, records, journal and trainer manager.
    UnionRoomComm *comm; // Communication handler shared by both apps.
    UnkStruct_ov109_021D0F70 *spinAppData; // Data for the 3D spin trade app (ov109_021D0D80).
    UnkStruct_ov109_021D5140 *groupAppData; // Data for the group confirmation app (ov109_021D3D50).
} UnionRoomSpinTradeSession;

#endif // POKEPLATINUM_STRUCT_0209C194_H
