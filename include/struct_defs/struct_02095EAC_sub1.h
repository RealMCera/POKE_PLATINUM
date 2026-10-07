#ifndef POKEPLATINUM_STRUCT_02095EAC_SUB1_H
#define POKEPLATINUM_STRUCT_02095EAC_SUB1_H

// One 1000-byte slice of the shared drawing canvas, sent as a huge transfer.
typedef struct {
    u8 data[1000];
    u32 checksum; // XOR of the chunk's 32-bit words, used to detect corruption
    u8 index; // 0-based chunk index within the canvas
    u8 padding[3];
} UnionRoomDrawingChunk;

#endif // POKEPLATINUM_STRUCT_02095EAC_SUB1_H
