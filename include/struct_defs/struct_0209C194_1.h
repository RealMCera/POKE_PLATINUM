#ifndef POKEPLATINUM_STRUCT_0209C194_1_H
#define POKEPLATINUM_STRUCT_0209C194_1_H

#include "struct_decls/struct_0205C22C_decl.h"

#include "field/field_system_decl.h"

#include "game_options.h"
#include "game_records.h"
#include "journal.h"
#include "savedata.h"

// Subset of game state copied into a UnionRoomSpinTradeSession. It is filled
// from the FieldSystem when the spin trade field task starts and read by both
// overlay109 apps.
typedef struct {
    int unk_00; // Unused.
    int messageBoxFrame; // Message box frame style (Options_Frame).
    SaveData *saveData;
    UnionRoomTrainers *trainers; // Union Room trainer manager (FieldSystem::unk_80).
    Options *options;
    GameRecords *records;
    JournalEntry *journalEntry;
    FieldSystem *fieldSystem;
} UnionRoomSpinTradeContext;

#endif // POKEPLATINUM_STRUCT_0209C194_1_H
