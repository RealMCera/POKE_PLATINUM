#ifndef POKEPLATINUM_STRUCT_0208AF44_H
#define POKEPLATINUM_STRUCT_0208AF44_H

#include "struct_defs/struct_0208AF44_sub1.h"

#include "sprite_system.h"
#include "touch_screen.h"

// One managed sprite on the number-entry screen, together with the touch
// rectangle it responds to and its animation state. Used for digit slots,
// group dividers, the cursor/button controls and the button press effects.
typedef struct {
    // Context-dependent state: a digit's value + 1 (0 = empty), a divider's
    // slot index, or a control's/button effect's animation index.
    int value;
    // 1-based group a digit slot belongs to (digits only).
    int group;
    // TRUE while a digit slot is in the highlighted group (digits only).
    BOOL isSelected;
    ManagedSprite *sprite;
    TouchScreenRect *touchRect;
    NumberEntrySpriteAnim anim;
} NumberEntrySprite;

#endif // POKEPLATINUM_STRUCT_0208AF44_H
