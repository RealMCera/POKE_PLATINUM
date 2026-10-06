#ifndef POKEPLATINUM_STRUCT_0202F41C_H
#define POKEPLATINUM_STRUCT_0202F41C_H

#include "struct_defs/struct_0202F298_sub1.h"

#include "battle_regulation.h"

// Compact summary of a battle recording, used to list and preview recordings
// without decoding the full battle data. It holds the party preview (species
// and forms of the Pokémon that took part) plus the battle regulation.
typedef struct BattleRecordingSummary {
    // Species of the party-preview slots, laid out by BattleRecording_BuildSummary.
    u16 species[12];
    // Form of each party-preview slot.
    u8 forms[12];
    // Battle number shown in the Vs. Recorder UI (clamped to 9999).
    u16 battleNumber;
    // Battle type (an UnkEnum_0202F510 value).
    u8 battleType;
    // Unclear flag; set to 1 when a recording is saved and read back by
    // BattleRecording_GetSummaryValue.
    u8 unk_27;
    BattleRegulation battleRegulation;
    // Integrity magic; must equal 0xE281 for the summary to be valid.
    u16 magic;
    // Unused padding.
    u8 unk_4A[14];
    // Unclear 64-bit value; compared for equality between recordings and shown
    // as a 12-digit number in the Vs. Recorder UI.
    u64 unk_58;
    // Checksum over the preceding fields.
    BattleRecordingChecksum checksum;
} BattleRecordingSummary;

#endif // POKEPLATINUM_STRUCT_0202F41C_H
