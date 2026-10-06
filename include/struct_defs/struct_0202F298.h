#ifndef POKEPLATINUM_STRUCT_0202F298_H
#define POKEPLATINUM_STRUCT_0202F298_H

#include "struct_defs/struct_0202F298_sub1.h"
#include "struct_defs/struct_0202FAA8.h"
#include "struct_defs/struct_0202FAA8_sub1.h"
#include "struct_defs/struct_0202FD30.h"

#include "game_options.h"
#include "trainer_info.h"

// The full payload of a Vs. Recorder battle recording. The leading fields are
// encrypted with BattleRecording_Encode/Decode; the trailing checksum is kept
// in the clear so the recording can be validated without decoding it.
typedef struct BattleRecordingData {
    // Battle setup copied from the FieldBattleDTO (battle type, trainers, seed,
    // weather, ...).
    BattleRecordingBattleInfo battleInfo;
    // One 1024-byte input log per battler.
    BattleRecordingInputLog inputLog;
    // Serialized parties, one per battler.
    BattleRecordingParty parties[4];
    // Trainer info for each battler.
    TrainerInfo trainerInfo[4];
    // Game options at the time the battle was recorded.
    Options options;
    // Integrity magic; must equal 0xE281 for the recording to be valid.
    u16 magic;
    // Checksum over the preceding fields.
    BattleRecordingChecksum checksum;
} BattleRecordingData;

#endif // POKEPLATINUM_STRUCT_0202F298_H
