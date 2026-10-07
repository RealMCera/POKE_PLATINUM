#ifndef POKEPLATINUM_STRUCT_020961E8_SUB1_H
#define POKEPLATINUM_STRUCT_020961E8_SUB1_H

// A player's trainer ID together with a validity flag. The whole union is
// compared as a u64 to detect when a player's trainer info changes between
// state updates.
typedef union {
    u64 raw; // Whole value, used for the change comparison.
    struct {
        u32 trainerId; // Trainer ID from the player's TrainerInfo.
        u32 valid; // 1 when the player is present.
    } parts;
} MixRecordsTrainerId;

#endif // POKEPLATINUM_STRUCT_020961E8_SUB1_H
