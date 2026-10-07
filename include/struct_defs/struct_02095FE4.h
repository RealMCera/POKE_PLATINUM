#ifndef POKEPLATINUM_STRUCT_02095FE4_H
#define POKEPLATINUM_STRUCT_02095FE4_H

// Connection handshake exchanged while joining the drawing session. Clients
// send a request (type 0) or a periodic heartbeat (type 1); the server replies
// with the same packet, setting `accepted` once every player has checked in.
typedef struct {
    u8 netId; // sender's net ID
    u8 connectedCount; // number of connected players the sender saw
    u8 type; // 0 = join request, 1 = heartbeat
    u8 accepted; // server: whether the join request was accepted
} UnionRoomDrawingConnAck;

#endif // POKEPLATINUM_STRUCT_02095FE4_H
