#ifndef POKEPLATINUM_STRUCT_020F568C_H
#define POKEPLATINUM_STRUCT_020F568C_H

// Data for a single move contest effect. The two line messages are the short
// "Perform first / next turn." style text shown when the move is selected; the
// five message pairs are the longer acting-competition messages, each with the
// string-template argument type needed to fill in its placeholders.
typedef struct {
    u16 lineOneEffectMessageID;
    u16 lineTwoEffectMessageID;
    s8 appealPoints; // appeal points awarded; POINTS_PER_APPEAL_HEART per heart
    u16 messageID1;
    u8 messageArgType1;
    u16 messageID2;
    u8 messageArgType2;
    u16 messageID3;
    u8 messageArgType3;
    u16 messageID4;
    u8 messageArgType4;
    u16 messageID5;
    u8 messageArgType5;
} ContestEffectData;

#endif // POKEPLATINUM_STRUCT_020F568C_H
