#ifndef POKEPLATINUM_STRUCT_02096274_H
#define POKEPLATINUM_STRUCT_02096274_H

// Connection-confirm packet exchanged on command 112. A client sends type 0 to
// ask the server to confirm it; the server echoes the packet back with
// `accepted` set. Type 1 announces that a player is leaving.
typedef struct {
    u8 netId; // Net ID of the player being confirmed.
    u8 playerCount; // Player count known to the sender.
    u8 type; // 0 = confirm request, 1 = player leaving.
    u8 accepted; // Server reply: 1 if the player was accepted.
} MixRecordsConnectionConfirm;

#endif // POKEPLATINUM_STRUCT_02096274_H
