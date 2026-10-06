#ifndef POKEPLATINUM_STRUCT_0209C0F0_H
#define POKEPLATINUM_STRUCT_0209C0F0_H

// Snapshot of a player's spin-trade animation, sent with command 11 and
// applied to the remote player's model.
typedef struct UnionRoomCommSpinTradePos {
    u16 unk_00; // Unused.
    u16 state; // Animation state.
    s16 posX; // Horizontal position.
    u16 posY; // Vertical position.
} UnionRoomCommSpinTradePos;

#endif // POKEPLATINUM_STRUCT_0209C0F0_H
