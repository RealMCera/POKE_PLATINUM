#ifndef POKEPLATINUM_STRUCT_0209BF64_H
#define POKEPLATINUM_STRUCT_0209BF64_H

// Payload of the connection-confirmation command (command 2). A client sends
// type 0 to ask the server to confirm it; the server echoes the packet back
// with accepted set to 0 or 1. Type 1 announces that a player is leaving.
typedef struct UnionRoomCommConfirm {
    u8 netId; // Net ID of the player being confirmed.
    u8 playerCount; // Player count the server expects.
    u8 type; // 0 = confirm request/response, 1 = leave.
    u8 accepted; // Server response: 1 if the player was accepted.
} UnionRoomCommConfirm;

#endif // POKEPLATINUM_STRUCT_0209BF64_H
