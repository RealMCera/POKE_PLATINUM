#ifndef POKEPLATINUM_STRUCT_0202FAA8_SUB1_H
#define POKEPLATINUM_STRUCT_0202FAA8_SUB1_H

// Recorded battle input: one 1024-byte log per battler, written and read one
// byte at a time by BattleRecording_WriteInput/ReadInput.
typedef struct BattleRecordingInputLog {
    u8 inputs[4][1024];
} BattleRecordingInputLog;

#endif // POKEPLATINUM_STRUCT_0202FAA8_SUB1_H
