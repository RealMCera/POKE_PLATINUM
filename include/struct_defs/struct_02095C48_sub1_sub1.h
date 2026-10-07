#ifndef POKEPLATINUM_STRUCT_02095C48_SUB1_SUB1_H
#define POKEPLATINUM_STRUCT_02095C48_SUB1_SUB1_H

// A contestant's scores and final placement.
typedef struct {
    s16 visualScore; // contest stat score used for the visual competition
    s16 danceScore; // dance score derived from the contest photo
    s16 danceCompetitionScore; // score earned in the dance competition
    s16 actingScore; // score earned in the acting competition
    u8 contestPlacement; // 0 is first place
    u8 padding_09[3];
} ContestantResult;

#endif // POKEPLATINUM_STRUCT_02095C48_SUB1_SUB1_H
