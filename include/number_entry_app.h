#ifndef POKEPLATINUM_NUMBER_ENTRY_APP_H
#define POKEPLATINUM_NUMBER_ENTRY_APP_H

#include "struct_defs/struct_02089438.h"

#include "game_options.h"

// Arguments for the number-entry screen. See NumberEntryArgs in
// struct_defs/struct_02089438.h.
NumberEntryArgs *NumberEntryArgs_New(enum HeapID heapID, int digitCount, int digitsPerGroup[], Options *options, u32 messageEntry, u32 showNetworkIcon);
NumberEntryArgs *NumberEntryArgs_NewWithPrefilled(enum HeapID heapID, int digitCount, int digitsPerGroup[], Options *options, u32 messageEntry, u32 showNetworkIcon, u32 prefilledGroupCount, u32 prefilledDigits);
void NumberEntryArgs_Free(NumberEntryArgs *args);

#endif // POKEPLATINUM_NUMBER_ENTRY_APP_H
