#ifndef POKEPLATINUM_STRUCT_02089438_H
#define POKEPLATINUM_STRUCT_02089438_H

#include "game_options.h"
#include "string_gf.h"

// Arguments passed by a launching application to the number-entry screen. They
// describe how many digit slots each group has, which leading groups are
// pre-filled, the prompt message to show, and whether to display the Wi-Fi
// connection-strength icon.
typedef struct {
    // Total number of digit slots across all groups.
    int digitCount;
    // Number of digit slots in each group. Only the first four entries are
    // supplied by the caller; index 4 repeats the last group's count and acts
    // as a sentinel when the screen derives the group slot ranges.
    int digitsPerGroup[5];
    int unk_18; // Unused; never set or read.
    // Buffer that receives the entered number as a string.
    String *numberString;
    Options *options;
    // Number of leading groups that are pre-filled and cannot be edited.
    int prefilledGroupCount;
    // Value used to pre-fill the locked leading digit slots, one decimal digit
    // per slot.
    u32 prefilledDigits;
    // Message bank entry printed as the screen's prompt.
    u32 messageEntry;
    // Non-zero to show the Wi-Fi connection-strength icon.
    u32 showNetworkIcon;
} NumberEntryArgs;

#endif // POKEPLATINUM_STRUCT_02089438_H
