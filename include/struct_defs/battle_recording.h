#ifndef POKEPLATINUM_STRUCT_DEF_BATTLE_RECORDING_H
#define POKEPLATINUM_STRUCT_DEF_BATTLE_RECORDING_H

#include "struct_defs/struct_0202F298.h"
#include "struct_defs/struct_0202F41C.h"
#include "struct_defs/struct_02030A80.h"

// A single Vs. Recorder battle recording. The recording is split into three
// parts so that the summary (used to list recordings) can be read without
// decoding the much larger battle data.
typedef struct BattleRecording {
    // Set to 0xFFFFFFFF by BattleRecording_Init; never read back.
    u32 unk_00;
    // Player profile shown alongside the recording (trainer card data).
    UnkStruct_02030A80 header;
    // Compact summary: the party preview and battle regulation.
    BattleRecordingSummary summary;
    // The full recorded battle: parties, trainer info, options and input log.
    BattleRecordingData data;
} BattleRecording;

#endif // POKEPLATINUM_STRUCT_DEF_BATTLE_RECORDING_H
