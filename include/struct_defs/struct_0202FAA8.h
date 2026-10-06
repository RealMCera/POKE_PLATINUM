#ifndef POKEPLATINUM_STRUCT_0202FAA8_H
#define POKEPLATINUM_STRUCT_0202FAA8_H

#include "struct_defs/trainer.h"

// Battle setup stored at the head of a recording. The fields mirror the
// matching members of FieldBattleDTO and are copied to/from it by
// BattleRecording_StoreBattleInfo and BattleRecording_RestoreBattleInfo.
typedef struct {
    u32 battleType;
    int resultMask;
    int trainerIDs[4];
    Trainer trainer[4];
    int background;
    int terrain;
    int mapLabelTextID;
    int mapHeaderID;
    int timeOfDay;
    int mapEvolutionMethod;
    int visitedContestHall;
    int metBebe;
    int caughtBattlerIdx;
    int fieldWeather;
    int leveledUpMonsMask;
    u32 systemVersion[4];
    u32 battleStatusMask;
    int countSafariBalls;
    u32 rulesetMask;
    u32 seed;
    int linkPlayerPositions[4];
    u16 networkID;
    u16 dummy18B;
    int totalTurnsElapsed;
    u8 recordedChatter[4];
} BattleRecordingBattleInfo;

#endif // POKEPLATINUM_STRUCT_0202FAA8_H
