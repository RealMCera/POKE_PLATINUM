#ifndef POKEPLATINUM_STRUCT_0202E828_H
#define POKEPLATINUM_STRUCT_0202E828_H

typedef struct {
    u8 active; // Set when Battle Points are earned, cleared once the segment is saved.
    u8 padding_01[3];
    u32 battlePoints;
} TVSegment_BattlePointsRecordData;

#endif // POKEPLATINUM_STRUCT_0202E828_H
