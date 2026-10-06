#ifndef POKEPLATINUM_STRUCT_0202E7E4_H
#define POKEPLATINUM_STRUCT_0202E7E4_H

typedef struct {
    u8 active; // Set when a walk is recorded, cleared once the segment is saved.
    u8 padding_01;
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 hasNickname;
    u16 nickname[11];
    u8 nature;
    u8 foundType; // 0 = nothing, 1 = item, 2 = Contest accessory.
    u8 foundAccessory;
    u8 padding_21;
    u16 foundItem;
} TVSegment_AmitySquareWatchData;

#endif // POKEPLATINUM_STRUCT_0202E7E4_H
