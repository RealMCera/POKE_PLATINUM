#ifndef POKEPLATINUM_STRUCT_0202F298_SUB1_H
#define POKEPLATINUM_STRUCT_0202F298_SUB1_H

// Checksum footer appended to the recording structs. Only the first half is
// ever written; the second half is unused padding.
typedef struct {
    u16 checksum;
    u16 unk_02;
} BattleRecordingChecksum;

#endif // POKEPLATINUM_STRUCT_0202F298_SUB1_H
