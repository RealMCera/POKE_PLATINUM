#ifndef POKEPLATINUM_STRUCT_0209BDF8_H
#define POKEPLATINUM_STRUCT_0209BDF8_H

#include "struct_decls/struct_0209C194_decl.h"

// Communication state for the Union Room (overlay 109). One instance is owned
// by the Union Room application and registered as the active command handler
// via CommCmd_Init. It tracks the local player's readiness, the set of players
// taking part in a trade, and per-player party data buffers.
typedef struct UnionRoomComm {
    UnkStruct_0209C194 *app; // Owning Union Room application.
    u8 sendBuffer[24]; // Staging area for an outgoing command packet.
    int sendDisabled; // When 1, UnionRoomComm_Send refuses to send. Never set.
    int receivedCount; // Incremented by the (never-sent) count command.
    int disconnected; // Set when a peer drops out of the room.
    int confirmed; // Local player's confirm flag (command 7).
    int playerCount; // Expected number of connected players.
    u32 confirmedBitmap; // Server-side bitmap of players that confirmed.
    int unk_34; // Written but never read.
    u16 serverPlayerCount; // Player count reported by the server.
    int unk_3C; // Read but never written.
    u16 stageFlags; // Bitmap of completed connection stages (command 8).
    u16 participantBitmap; // Bitmap of players taking part in the trade (command 9).
    u16 unk_44; // Unused.
    s16 trainerDataBitmap; // Bitmap of players whose party data has arrived.
    u16 badEggBitmap; // Bitmap of players reporting a bad egg (command 16).
    u16 eggOkBitmap; // Bitmap of players reporting their eggs are OK (command 17).
    u8 *sendTrainerData; // Per-player outgoing party buffers (5 slots).
    u8 *recvTrainerData; // Per-player incoming party buffers (5 slots).
    int unk_54; // Unused.
} UnionRoomComm;

#endif // POKEPLATINUM_STRUCT_0209BDF8_H
