#ifndef POKEPLATINUM_STRUCT_02089688_SUB2_H
#define POKEPLATINUM_STRUCT_02089688_SUB2_H

// A selection action queued by the touch handler or key input and consumed by
// NumberEntry_ProcessSelectionAction.
typedef struct {
    // Action type: 0 = none, 1 = select a group, 2 = select a digit slot,
    // 0xFF = action handled.
    int type;
    // Group number (type 1) or digit-slot index (type 2).
    int value;
    // When selecting a group, TRUE moves the cursor to the group's last slot
    // instead of its first.
    int selectLastDigit;
} NumberEntryScreen_SelectionAction;

#endif // POKEPLATINUM_STRUCT_02089688_SUB2_H
