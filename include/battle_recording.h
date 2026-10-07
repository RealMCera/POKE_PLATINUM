#ifndef POKEPLATINUM_BATTLE_RECORDING_H
#define POKEPLATINUM_BATTLE_RECORDING_H

#include "struct_decls/struct_0202F298_decl.h"
#include "struct_decls/struct_0202F41C_decl.h"
#include "struct_decls/struct_02030A80_decl.h"
#include "struct_defs/battle_recording.h"

#include "field_battle_data_transfer.h"
#include "savedata.h"

// Vs. Recorder battle recordings. A recording is stored in one of the extra
// save sectors and mirrored into a heap buffer (gBattleRecording) while it is
// being viewed or written. The recording is split into a header (player
// profile), a summary (party preview + regulation) and the full battle data.
int BattleRecording_SaveSize(void);
void BattleRecording_Init(BattleRecording *battleRecording);
void BattleRecording_New(SaveData *saveData, enum HeapID heapID, int *resultCode);
void BattleRecording_Free(void);
BOOL BattleRecording_Exists(void);
BattleRecording *BattleRecording_Get(void);
// Returns a pointer to the recording payload, i.e. everything after the
// leading u32 of BattleRecording.
void *BattleRecording_GetData(void);
// Loads recording `recNum` into gBattleRecording, validates it and, if `dto`
// is non-NULL, restores the battle setup into it. `resultCode` receives 0
// (invalid), 1 (ok), 2 (checksum mismatch) or 3 (no recording).
BOOL BattleRecording_Load(SaveData *saveData, int heapID, int *resultCode, FieldBattleDTO *dto, int recNum);
// Like BattleRecording_Load, but frees the recording afterwards and returns
// whether it was valid.
BOOL BattleRecording_Validate(SaveData *saveData, int heapID, int *resultCode, int recNum);
// Saves gBattleRecording to the extra save sector, driving the save state
// machine in `saveState`.
int BattleRecording_Save(SaveData *saveData, BattleRecording *battleRecording, int recNum, u16 *saveState);
// Builds the summary and checksums from the current battle data, then saves
// the recording. `state` drives the two-step process.
int BattleRecording_SaveWithSummary(SaveData *saveData, int battleType, int battleNumber, int recNum, u16 *state, u16 *saveState);
// Returns the number of battlers and the party size for a battle type.
void BattleRecording_GetPartyLayout(int battleType, int *battlerCount, int *partySize);
void BattleRecording_Encode(void *data, u32 size, u32 key);
void BattleRecording_WriteInput(int battler, int pos, u8 value);
u8 BattleRecording_ReadInput(int battler, int pos);
void BattleRecording_StoreBattleInfo(FieldBattleDTO *dto);
void BattleRecording_SetSystemVersion(int battler, u32 version);
BOOL BattleRecording_CheckSystemVersions(void);
void BattleRecording_RestoreBattleInfo(FieldBattleDTO *dto, SaveData *saveData);
BattleRecordingSummary *BattleRecording_CloneSummary(enum HeapID heapID);
PlayerProfile *BattleRecording_CloneHeader(enum HeapID heapID);
PlayerProfile *BattleRecording_GetHeader(void);
BattleRecordingSummary *BattleRecording_GetSummary(void);
void BattleRecording_Store(PlayerProfile *header, BattleRecordingSummary *summary, BattleRecordingData *data, FieldBattleDTO *dto, SaveData *saveData);
// Reads one field of the summary: 0/1 = species/form at `index`, 2 = battle
// number, 3 = battle type, 4 = the opaque u64, 5 = the opaque flag.
u64 BattleRecording_GetSummaryValue(BattleRecordingSummary *summary, int field, int index);
BattleRecordingSummary *BattleRecording_NewSummary(enum HeapID heapID);
void BattleRecording_FreeSummary(BattleRecordingSummary *summary);

#endif // POKEPLATINUM_BATTLE_RECORDING_H
