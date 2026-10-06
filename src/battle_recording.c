#include "battle_recording.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle.h"
#include "constants/species.h"

#include "struct_defs/battle_recording.h"
#include "struct_defs/struct_0202F298.h"
#include "struct_defs/struct_0202F298_sub1.h"
#include "struct_defs/struct_0202F41C.h"
#include "struct_defs/struct_0202FAA8.h"
#include "struct_defs/struct_0202FD30.h"
#include "struct_defs/struct_02030A80.h"
#include "struct_defs/struct_02078B40.h"

#include "savedata/save_table.h"

#include "battle_regulation.h"
#include "enums.h"
#include "field_battle_data_transfer.h"
#include "game_options.h"
#include "heap.h"
#include "math_util.h"
#include "party.h"
#include "pokedex.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "sound_chatot.h"
#include "system.h"
#include "trainer_info.h"

// The recording currently loaded into memory, or NULL if none is loaded.
BattleRecording *gBattleRecording = NULL;

static void BattleRecording_SerializeParty(const Party *party, BattleRecordingParty *recordingParty);
static void BattleRecording_DeserializeParty(BattleRecordingParty *recordingParty, Party *party);
static BOOL BattleRecording_IsInvalid(SaveData *saveData, BattleRecording *battleRecording);
static BOOL BattleRecording_IsValid(SaveData *saveData, const BattleRecording *battleRecording);
static void BattleRecording_Decode(void *data, u32 size, u32 key);
static void BattleRecording_BuildSummary(SaveData *saveData, BattleRecordingSummary *summary, const BattleRecordingData *data, int battleType, int battleNumber);

int BattleRecording_SaveSize(void)
{
    GF_ASSERT(sizeof(BattleRecording) < 0x1000 * 2);

    return sizeof(BattleRecording);
}

void BattleRecording_Init(BattleRecording *battleRecording)
{
    MI_CpuClear32(battleRecording, sizeof(BattleRecording));
    // Mark the recording as empty; the field is never read back.
    battleRecording->unk_00 = 0xFFFFFFFF;
}

void BattleRecording_New(SaveData *saveData, enum HeapID heapID, int *resultCode)
{
    if (gBattleRecording != NULL) {
        Heap_Free(gBattleRecording);
        gBattleRecording = NULL;
    }

    gBattleRecording = SaveData_GetBattleRecording(saveData, heapID, resultCode, 0);
    BattleRecording_Init(gBattleRecording);
}

void BattleRecording_Free(void)
{
    GF_ASSERT(gBattleRecording);

    Heap_Free(gBattleRecording);
    gBattleRecording = NULL;
}

BOOL BattleRecording_Exists(void)
{
    return gBattleRecording != NULL;
}

BattleRecording *BattleRecording_Get(void)
{
    GF_ASSERT(gBattleRecording);
    return gBattleRecording;
}

void *BattleRecording_GetData(void)
{
    u8 *v0;

    GF_ASSERT(gBattleRecording);

    // Skip the leading u32 so callers see the header/summary/data payload.
    v0 = (u8 *)gBattleRecording;
    return &v0[sizeof(u32)];
}

BOOL BattleRecording_Load(SaveData *saveData, int heapID, int *resultCode, FieldBattleDTO *dto, int recNum)
{
    BattleRecordingData *v0;
    BattleRecordingSummary *v1;

    if (gBattleRecording) {
        Heap_Free(gBattleRecording);
        gBattleRecording = NULL;
    }

    gBattleRecording = SaveData_GetBattleRecording(saveData, heapID, resultCode, recNum);

    // The extra save sector reports 1 when the recording was read successfully.
    if (*resultCode != 1) {
        *resultCode = 3;
        return 1;
    }

    v0 = &gBattleRecording->data;
    v1 = &gBattleRecording->summary;

    // The battle data is encrypted with a key derived from its own checksum
    // (the checksum and its complement packed into 32 bits).
    BattleRecording_Decode(v0, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)), v0->checksum.checksum + ((v0->checksum.checksum ^ 0xffff) << 16));

    if (BattleRecording_IsInvalid(saveData, gBattleRecording) == 1) {
        *resultCode = 0;
        return 1;
    }

    if (BattleRecording_IsValid(saveData, gBattleRecording) == 0) {
        *resultCode = 2;
        return 1;
    }

    if (dto) {
        BattleRecording_RestoreBattleInfo(dto, saveData);
    }

    *resultCode = 1;
    return 1;
}

BOOL BattleRecording_Validate(SaveData *saveData, int heapID, int *resultCode, int recNum)
{
    BattleRecordingData *v0;
    BattleRecordingSummary *v1;
    BattleRecording *v2 = SaveData_GetBattleRecording(saveData, heapID, resultCode, recNum);

    if (*resultCode != 1) {
        *resultCode = 3;
        Heap_Free(v2);
        return 0;
    }

    v0 = &v2->data;
    v1 = &v2->summary;

    // See BattleRecording_Load: the battle data is encrypted with a key
    // derived from its own checksum.
    BattleRecording_Decode(v0, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)), v0->checksum.checksum + ((v0->checksum.checksum ^ 0xffff) << 16));

    if (BattleRecording_IsInvalid(saveData, v2) == 1) {
        *resultCode = 0;
        Heap_Free(v2);
        return 0;
    }

    if (BattleRecording_IsValid(saveData, v2) == 0) {
        *resultCode = 2;
        Heap_Free(v2);
        return 0;
    }

    *resultCode = 1;
    Heap_Free(v2);
    return 1;
}

int BattleRecording_Save(SaveData *saveData, BattleRecording *battleRecording, int recNum, u16 *saveState)
{
    int v0;

    // Two-step save: write the recording to the extra sector, then flush the
    // save state. `saveState` is reset to 0 once the flush completes.
    switch (*saveState) {
    case 0:
        ResetLock(RESET_LOCK_0x8);
        InitHeapCanary(11);

        v0 = SaveData_SaveBattleRecording(saveData, battleRecording, recNum);

        if (v0 == 2) {
            SaveData_SaveStateInit(saveData, 2);
            (*saveState)++;
            return 0;
        }

        ResetUnlock(RESET_LOCK_0x8);
        return v0;
    case 1:
        v0 = SaveData_SaveStateMain(saveData);

        if ((v0 == 2) || (v0 == 3)) {
            (*saveState) = 0;
            FreeHeapCanary();
            ResetUnlock(RESET_LOCK_0x8);
        }

        return v0;
    }

    return 0;
}

int BattleRecording_SaveWithSummary(SaveData *saveData, int battleType, int battleNumber, int recNum, u16 *state, u16 *saveState)
{
    BattleRecordingSummary *v0;
    BattleRecordingData *v1;
    int v2;

    switch (*state) {
    case 0:

        if (gBattleRecording == NULL) {
            return 3;
        }

        v0 = &gBattleRecording->summary;
        v1 = &gBattleRecording->data;

        BattleRecording_BuildSummary(saveData, v0, v1, battleType, battleNumber);

        // Stamp both halves with the integrity magic and checksum them. The
        // summary checksum excludes its own trailing u64 (unk_58).
        v0->magic = 0xe281;
        v0->checksum.checksum = SaveData_CalculateChecksum(saveData, v0, sizeof(BattleRecordingSummary) - (sizeof(BattleRecordingChecksum)) - (sizeof(u64)));
        v1->magic = 0xe281;
        v1->checksum.checksum = SaveData_CalculateChecksum(saveData, v1, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)));

        // Encrypt the battle data with the key derived from its checksum.
        BattleRecording_Encode(v1, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)), v1->checksum.checksum + ((v1->checksum.checksum ^ 0xffff) << 16));

        *saveState = 0;
        (*state)++;
        break;
    case 1:
        v2 = BattleRecording_Save(saveData, gBattleRecording, recNum, saveState);
        return v2;
    }

    return 0;
}

void BattleRecording_GetPartyLayout(int battleType, int *battlerCount, int *partySize)
{
    // Multi battles (the listed types) use four battlers with three Pokémon
    // each; every other battle uses two battlers with a full party of six.
    switch (battleType) {
    case (UnkEnum_0202F510_17):
    case (UnkEnum_0202F510_20):
    case (UnkEnum_0202F510_23):
    case (UnkEnum_0202F510_26):
    case (UnkEnum_0202F510_29):
    case (UnkEnum_0202F510_32):
    case (UnkEnum_0202F510_14):
        *battlerCount = 4;
        *partySize = 6 / 2;
        break;
    default:
        *battlerCount = 4 / 2;
        *partySize = 6;
        break;
    }
}

static void BattleRecording_BuildSummary(SaveData *saveData, BattleRecordingSummary *summary, const BattleRecordingData *data, int battleType, int battleNumber)
{
    int v0, v1, v2, v3, v4, v5, v6;
    const UnkStruct_02078B40 *v7;
    const u8 v8[2][4] = {
        { 0, 2, 3, 1 },
        { 3, 1, 0, 2 },
    };
    const u8 v9[4] = { 0, 2, 1, 3 };

    MI_CpuClear8(summary, sizeof(BattleRecordingSummary));
    BattleRecording_GetPartyLayout(battleType, &v2, &v3);

    v4 = 0;

    // For link battles the battler order in the recording does not match the
    // order the party preview should be shown in, so the battler index is
    // remapped below. `v6` is the local player's network ID (doubled for
    // battles that use the second permutation table).
    if (data->battleInfo.battleType & 0x4) {
        if (data->battleInfo.battleType & 0x80) {
            v6 = data->battleInfo.networkID * 2;
        } else {
            v6 = data->battleInfo.networkID;
        }
    } else {
        v6 = 0;
    }

    for (v0 = 0; v0 < v2; v0++) {
        if ((data->battleInfo.battleType & 0x8) && ((data->battleInfo.battleType & 0x80) == 0)) {
            // Link battle: find the battler whose link position matches the
            // permutation of the local player's position.
            for (v5 = 0; v5 < v2; v5++) {
                if (data->battleInfo.linkPlayerPositions[v5] == v8[data->battleInfo.linkPlayerPositions[v6] & 1][v0]) {
                    break;
                }
            }
        } else if ((data->battleInfo.battleType & 0x8) && (data->battleInfo.battleType & 0x80)) {
            v5 = v9[v0];
        } else {
            // Single-player: swap the first two battlers when the local player
            // is on the second side.
            v5 = v0;

            if (v6 & 1) {
                v5 ^= 1;
            }
        }

        for (v1 = 0; v1 < v3; v1++) {
            v7 = &(data->parties[v5].mons[v1]);

            // Eggs and corrupted slots are left as empty (zero) preview slots.
            if ((v7->isEgg == 0) && (v7->checksumFailed == 0)) {
                summary->species[v4] = v7->species;
                summary->forms[v4] = v7->form;
            }

            v4++;
        }
    }

    switch (battleType) {
    case (UnkEnum_0202F510_01):
    case (UnkEnum_0202F510_08):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 0));
        break;
    case (UnkEnum_0202F510_02):
    case (UnkEnum_0202F510_09):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 1));
        break;
    case (UnkEnum_0202F510_03):
    case (UnkEnum_0202F510_10):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 2));
        break;
    case (UnkEnum_0202F510_04):
    case (UnkEnum_0202F510_11):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 3));
        break;
    case (UnkEnum_0202F510_05):
    case (UnkEnum_0202F510_12):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 4));
        break;
    case (UnkEnum_0202F510_06):
    case (UnkEnum_0202F510_13):
        summary->battleRegulation = *(BattleRegulation_GetByIndex(saveData, 5));
        break;
    case (UnkEnum_0202F510_00):
    case (UnkEnum_0202F510_07):
    default:
        summary->battleRegulation = *(BattleRegulation_GetDefault());
        break;
    }

    summary->battleNumber = battleNumber;
    summary->battleType = battleType;
}

static BOOL BattleRecording_IsInvalid(SaveData *saveData, BattleRecording *battleRecording)
{
    BattleRecordingData *v0 = &battleRecording->data;
    BattleRecordingSummary *v1 = &battleRecording->summary;

    // A recording is only meaningful once the extra save block has been
    // initialized.
    if (SaveData_MiscSaveBlock_InitFlag(saveData) == 0) {
        return 1;
    }

    // Both halves must carry the integrity magic.
    if ((v0->magic != 0xe281) || (v1->magic != 0xe281)) {
        return 1;
    }

    return 0;
}

static BOOL BattleRecording_IsValid(SaveData *saveData, const BattleRecording *battleRecording)
{
    const BattleRecordingData *v0 = &battleRecording->data;
    const BattleRecordingSummary *v1 = &battleRecording->summary;
    u16 v2;

    if ((v0->magic != 0xe281) || (v1->magic != 0xe281)) {
        return 0;
    }

    // Verify both checksums before trusting any of the data.
    v2 = SaveData_CalculateChecksum(saveData, v1, sizeof(BattleRecordingSummary) - (sizeof(BattleRecordingChecksum)) - (sizeof(u64)));

    if (v2 != v1->checksum.checksum) {
        return 0;
    }

    v2 = SaveData_CalculateChecksum(saveData, v0, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)));

    if (v2 != v0->checksum.checksum) {
        return 0;
    }

    {
        int v3, v4, v5;
        const UnkStruct_02078B40 *v6;

        // Reject out-of-range species, held items and moves so a corrupted
        // recording cannot be loaded into the battle engine.
        for (v3 = 0; v3 < 4; v3++) {
            for (v4 = 0; v4 < 6; v4++) {
                v6 = &(v0->parties[v3].mons[v4]);

                if (v6->species > MAX_SPECIES) {
                    return 0;
                }

                if (v6->heldItem > 467) {
                    return 0;
                }

                for (v5 = 0; v5 < 4; v5++) {
                    if (v6->unk_1C[v5] > 467) {
                        return 0;
                    }
                }
            }
        }
    }

    return 1;
}

// Thin wrappers around the save-data cipher so callers do not need to include
// the cipher header directly.
void BattleRecording_Encode(void *data, u32 size, u32 key)
{
    EncodeData(data, size, key);
}

static void BattleRecording_Decode(void *data, u32 size, u32 key)
{
    DecodeData(data, size, key);
}

void BattleRecording_WriteInput(int battler, int pos, u8 value)
{
    if (gBattleRecording == NULL) {
        return;
    }

    gBattleRecording->data.inputLog.inputs[battler][pos] = value;
}

u8 BattleRecording_ReadInput(int battler, int pos)
{
    GF_ASSERT(gBattleRecording != NULL);
    return gBattleRecording->data.inputLog.inputs[battler][pos];
}

void BattleRecording_StoreBattleInfo(FieldBattleDTO *dto)
{
    int v0;
    BattleRecordingData *v1;
    BattleRecordingBattleInfo *v2;

    if (gBattleRecording == NULL) {
        return;
    }

    v1 = &gBattleRecording->data;
    v2 = &v1->battleInfo;

    // Copy the battle setup from the DTO into the recording.
    v2->battleType = dto->battleType;
    v2->resultMask = dto->resultMask;
    v2->background = dto->background;
    v2->terrain = dto->terrain;
    v2->mapLabelTextID = dto->mapLabelTextID;
    v2->mapHeaderID = dto->mapHeaderID;
    v2->timeOfDay = dto->timeOfDay;
    v2->mapEvolutionMethod = dto->mapEvolutionMethod;
    v2->visitedContestHall = dto->visitedContestHall;
    v2->metBebe = dto->metBebe;
    v2->caughtBattlerIdx = dto->caughtBattlerIdx;
    v2->fieldWeather = dto->fieldWeather;
    v2->leveledUpMonsMask = dto->leveledUpMonsMask;
    v2->battleStatusMask = dto->battleStatusMask;
    v2->countSafariBalls = dto->countSafariBalls;
    v2->rulesetMask = dto->rulesetMask;
    v2->seed = dto->seed;
    v2->networkID = dto->networkID;
    v2->dummy18B = dto->dummy18B;
    v2->totalTurnsElapsed = dto->totalTurnsElapsed;

    for (v0 = 0; v0 < 4; v0++) {
        v2->trainerIDs[v0] = dto->trainerIDs[v0];
        v2->trainer[v0] = dto->trainer[v0];

        if (dto->systemVersion[v0] == 0) {
            // Unknown system version; record the current maximum instead.
            v2->systemVersion[v0] = 0x140;
        } else {
            v2->systemVersion[v0] = dto->systemVersion[v0];
        }

        v2->linkPlayerPositions[v0] = dto->linkPlayerPositions[v0];
        v2->recordedChatter[v0] = dto->recordedChatter[v0];
    }

    for (v0 = 0; v0 < 4; v0++) {
        BattleRecording_SerializeParty(dto->parties[v0], &v1->parties[v0]);
        TrainerInfo_Copy(dto->trainerInfo[v0], &v1->trainerInfo[v0]);

        // The chatter byte is the Chatot activation parameter, not the raw
        // recorded chatter copied above.
        v2->recordedChatter[v0] = Sound_GetChatterActivationParameter(dto->chatotCries[v0]);
    }

    Options_Copy(dto->options, &v1->options);
}

void BattleRecording_SetSystemVersion(int battler, u32 version)
{
    BattleRecordingData *v0;
    BattleRecordingBattleInfo *v1;

    if (gBattleRecording == NULL) {
        return;
    }

    v0 = &gBattleRecording->data;
    v1 = &v0->battleInfo;

    v1->systemVersion[battler] = version;
}

BOOL BattleRecording_CheckSystemVersions(void)
{
    int v0;
    BattleRecordingData *v1;
    BattleRecordingBattleInfo *v2;

    if (gBattleRecording == NULL) {
        return 1;
    }

    v1 = &gBattleRecording->data;
    v2 = &v1->battleInfo;

    for (v0 = 0; v0 < 4; v0++) {
        if (v2->systemVersion[v0] > 0x140) {
            return 0;
        }
    }

    return 1;
}

void BattleRecording_RestoreBattleInfo(FieldBattleDTO *dto, SaveData *saveData)
{
    int i;
    BattleRecordingData *v1 = &gBattleRecording->data;

    dto->battleType = v1->battleInfo.battleType;
    dto->background = v1->battleInfo.background;
    dto->terrain = v1->battleInfo.terrain;
    dto->mapLabelTextID = v1->battleInfo.mapLabelTextID;
    dto->mapHeaderID = v1->battleInfo.mapHeaderID;
    dto->timeOfDay = v1->battleInfo.timeOfDay;
    dto->mapEvolutionMethod = v1->battleInfo.mapEvolutionMethod;
    dto->visitedContestHall = v1->battleInfo.visitedContestHall;
    dto->metBebe = v1->battleInfo.metBebe;
    dto->caughtBattlerIdx = v1->battleInfo.caughtBattlerIdx;
    dto->fieldWeather = v1->battleInfo.fieldWeather;
    // Replaying a recording must not start another recording.
    dto->battleStatusMask = v1->battleInfo.battleStatusMask | BATTLE_STATUS_RECORDING;
    dto->countSafariBalls = v1->battleInfo.countSafariBalls;
    dto->rulesetMask = v1->battleInfo.rulesetMask;
    dto->seed = v1->battleInfo.seed;
    dto->networkID = v1->battleInfo.networkID;
    dto->resultMask = BATTLE_IN_PROGRESS;
    dto->leveledUpMonsMask = 0;

    Pokedex_Copy(SaveData_GetPokedex(saveData), dto->pokedex);

    for (i = 0; i < 4; i++) {
        dto->trainerIDs[i] = v1->battleInfo.trainerIDs[i];
        dto->trainer[i] = v1->battleInfo.trainer[i];
        dto->systemVersion[i] = v1->battleInfo.systemVersion[i];
        dto->linkPlayerPositions[i] = v1->battleInfo.linkPlayerPositions[i];

        BattleRecording_DeserializeParty(&v1->parties[i], dto->parties[i]);
        TrainerInfo_Copy(&v1->trainerInfo[i], dto->trainerInfo[i]);

        dto->recordedChatter[i] = v1->battleInfo.recordedChatter[i];
    }

    Options_Copy(SaveData_GetOptions(saveData), dto->options);
    dto->options->frame = v1->options.frame;

    // The recorded frame may not exist in the current game version.
    if (dto->options->frame >= 20) {
        dto->options->frame = 0;
    }
}

static void BattleRecording_SerializeParty(const Party *party, BattleRecordingParty *recordingParty)
{
    int v0;
    Pokemon *v1;

    MI_CpuClear8(recordingParty, sizeof(BattleRecordingParty));

    recordingParty->capacity = Party_GetCapacity(party);
    recordingParty->count = Party_GetCurrentCount(party);

    // Store each Pokémon in the compact box format used by the recording.
    for (v0 = 0; v0 < recordingParty->count; v0++) {
        v1 = Party_GetPokemonBySlotIndex(party, v0);
        sub_02078B40(v1, &recordingParty->mons[v0]);
    }
}

static void BattleRecording_DeserializeParty(BattleRecordingParty *recordingParty, Party *party)
{
    int v0;
    Pokemon *v1;
    u8 v2 = 0;

    v1 = Pokemon_New(HEAP_ID_FIELD2);

    Party_InitWithCapacity(party, recordingParty->capacity);

    for (v0 = 0; v0 < recordingParty->count; v0++) {
        sub_02078E0C(&recordingParty->mons[v0], v1);
        // Ball capsules are not part of a recording.
        Pokemon_SetValue(v1, MON_DATA_BALL_CAPSULE_ID, &v2);
        Party_AddPokemon(party, v1);
    }

    Heap_Free(v1);
}

BattleRecordingSummary *BattleRecording_CloneSummary(enum HeapID heapID)
{
    BattleRecordingSummary *v0;

    GF_ASSERT(gBattleRecording != NULL);

    v0 = Heap_Alloc(heapID, sizeof(BattleRecordingSummary));
    MI_CpuCopy32(&gBattleRecording->summary, v0, sizeof(BattleRecordingSummary));

    return v0;
}

UnkStruct_02030A80 *BattleRecording_CloneHeader(enum HeapID heapID)
{
    UnkStruct_02030A80 *v0;

    GF_ASSERT(gBattleRecording != NULL);

    v0 = Heap_Alloc(heapID, sizeof(UnkStruct_02030A80));
    MI_CpuCopy32(&gBattleRecording->header, v0, sizeof(UnkStruct_02030A80));

    return v0;
}

UnkStruct_02030A80 *BattleRecording_GetHeader(void)
{
    GF_ASSERT(gBattleRecording != NULL);
    return &gBattleRecording->header;
}

BattleRecordingSummary *BattleRecording_GetSummary(void)
{
    GF_ASSERT(gBattleRecording != NULL);
    return &gBattleRecording->summary;
}

void BattleRecording_Store(UnkStruct_02030A80 *header, BattleRecordingSummary *summary, BattleRecordingData *data, FieldBattleDTO *dto, SaveData *saveData)
{
    GF_ASSERT(gBattleRecording != NULL);

    // Replace the whole recording with the supplied parts, then decrypt the
    // battle data so it can be read back.
    MI_CpuCopy8(summary, &gBattleRecording->summary, sizeof(BattleRecordingSummary));
    MI_CpuCopy8(data, &gBattleRecording->data, sizeof(BattleRecordingData));
    MI_CpuCopy8(header, &gBattleRecording->header, sizeof(UnkStruct_02030A80));

    BattleRecording_Decode(&gBattleRecording->data, sizeof(BattleRecordingData) - (sizeof(BattleRecordingChecksum)), gBattleRecording->data.checksum.checksum + ((gBattleRecording->data.checksum.checksum ^ 0xffff) << 16));

    if (dto != NULL) {
        BattleRecording_RestoreBattleInfo(dto, saveData);
    }
}

u64 BattleRecording_GetSummaryValue(BattleRecordingSummary *summary, int field, int index)
{
    GF_ASSERT((sizeof(u64)) <= sizeof(u64));

    switch (field) {
    case 0:
        GF_ASSERT(index < 12);

        // Treat an out-of-range species as an empty preview slot.
        if (summary->species[index] > NATIONAL_DEX_COUNT) {
            return 0;
        }

        return summary->species[index];
    case 1:
        GF_ASSERT(index < 12);
        return summary->forms[index];
    case 2:
        if (summary->battleNumber > 9999) {
            return 9999;
        }

        return summary->battleNumber;
    case 3:
        if (summary->battleType >= (UnkEnum_0202F510_32 + 1)) {
            return UnkEnum_0202F510_00;
        }

        return summary->battleType;
    case 4:
        return summary->unk_58;
    case 5:
        return summary->unk_27;
    }

    GF_ASSERT(FALSE);
    return 0;
}

BattleRecordingSummary *BattleRecording_NewSummary(enum HeapID heapID)
{
    BattleRecordingSummary *v0 = Heap_Alloc(heapID, sizeof(BattleRecordingSummary));
    MI_CpuClear8(v0, sizeof(BattleRecordingSummary));

    return v0;
}

void BattleRecording_FreeSummary(BattleRecordingSummary *summary)
{
    Heap_Free(summary);
}
