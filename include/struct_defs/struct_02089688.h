#ifndef POKEPLATINUM_STRUCT_02089688_H
#define POKEPLATINUM_STRUCT_02089688_H

#include "struct_defs/struct_02089438.h"
#include "struct_defs/struct_02089688_sub1.h"
#include "struct_defs/struct_02089688_sub2.h"
#include "struct_defs/struct_0208AF44.h"

// State for the number-entry screen. The screen lays out a row of digit slots
// (grouped, e.g. a Friend Code as XXXX-XXXX-XXXX) plus BACK and OK buttons, and
// lets the player move a cursor between the slots to enter a number.
typedef struct {
    // Digit slots. `value` holds the digit value + 1 (0 means "not entered")
    // and `group` holds the 1-based group the slot belongs to.
    NumberEntrySprite digits[16];
    // Separator sprites drawn between adjacent digit groups (one fewer than
    // the number of groups).
    NumberEntrySprite dividers[3];
    // The cursor sprite and the OK/BACK button sprites.
    NumberEntrySprite controls[3];
    // Press animations for the BACK (0) and OK (1) buttons.
    NumberEntrySprite buttonEffects[2];
    // Left edge of each group's row, indexed by group number (0 = no group).
    s16 groupXPos[5];
    // [group][0] = first digit slot in the group, [group][1] = one past the
    // last slot. Group numbers are 1-based; index 0 is unused.
    u16 groupRanges[5][2];
    // Index into the phase function table (see sPhaseFuncs).
    int phase;
    int unk_2C4; // Unused; written once in NumberEntry_SetPhase.
    // Sub-step within the current phase.
    int phaseStep;
    // Frame counter used by the group-change animation.
    int animTimer;
    // Total number of digit slots.
    int digitCount;
    // Currently highlighted group (1-based), or 0 if none.
    int selectedGroup;
    // Group highlighted before the current one, used to animate the transition.
    int prevSelectedGroup;
    // First digit slot of the currently highlighted group.
    int selectedGroupStart;
    // One past the last digit slot of the currently highlighted group.
    int selectedGroupEnd;
    // First digit slot of the previously highlighted group.
    int prevGroupStart;
    // One past the last digit slot of the previously highlighted group.
    int prevGroupEnd;
    // Graphics resources and touch-screen state.
    NumberEntryScreen_Graphics graphics;
    // Pending selection action produced by the touch handler or key input.
    NumberEntryScreen_SelectionAction selectionAction;
    // Arguments supplied by the launching application (digit counts per group,
    // prompt message, pre-filled digits, ...).
    UnkStruct_02089438 args;
    // Number of group separators (number of groups - 1).
    int dividerCount;
    // Number of leading digit slots that are pre-filled and cannot be changed.
    int prefilledDigitCount;
} NumberEntryScreen;

#endif // POKEPLATINUM_STRUCT_02089688_H
