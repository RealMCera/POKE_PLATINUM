#ifndef POKEPLATINUM_STRUCT_0202E81C_H
#define POKEPLATINUM_STRUCT_0202E81C_H

typedef struct {
    u8 active; // Set when a multi battle ends, cleared once the segment is saved.
    u8 facility; // Battle Frontier facility ID (see TVSegment_LoadMessage_BattleFrontierFrontlineNews_Multi).
    u8 gender;
    u8 language;
    u8 gameCode;
    u8 padding_05;
    u16 trainerName[8];
} TVSegment_BattleFrontierFrontlineNewsMultiData;

#endif // POKEPLATINUM_STRUCT_0202E81C_H
