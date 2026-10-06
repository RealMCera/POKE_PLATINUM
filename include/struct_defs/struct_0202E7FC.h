#ifndef POKEPLATINUM_STRUCT_0202E7FC_H
#define POKEPLATINUM_STRUCT_0202E7FC_H

typedef struct {
    u8 active; // Set when a Battle Tower run ends, cleared once the segment is saved.
    u8 win;
    u16 winStreak;
} TVSegment_BattleTowerCornerData;

#endif // POKEPLATINUM_STRUCT_0202E7FC_H
