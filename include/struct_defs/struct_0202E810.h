#ifndef POKEPLATINUM_STRUCT_0202E810_H
#define POKEPLATINUM_STRUCT_0202E810_H

typedef struct {
    u8 active; // Set when a battle is won, cleared once the segment is saved.
    u8 padding_01;
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 hasNickname;
    u16 nickname[11];
} TVSegment_BattleFrontierFrontlineNewsSingleData;

#endif // POKEPLATINUM_STRUCT_0202E810_H
