#ifndef POKEPLATINUM_STRUCT_02089688_SUB1_H
#define POKEPLATINUM_STRUCT_02089688_SUB1_H

#include "struct_decls/font_oam.h"
#include "struct_decls/struct_02012744_decl.h"

#include "bg_window.h"
#include "char_transfer.h"
#include "narc.h"
#include "palette.h"
#include "sprite_system.h"
#include "touch_screen.h"
#include "touch_screen_actions.h"

// Graphics resources and touch-screen state for the number-entry screen.
typedef struct {
    NARC *narc;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    BgConfig *bgConfig;
    PaletteData *paletteData;
    TouchScreenActions *touchScreenActions;
    // Touch rectangles for the digit slots (0-15) and the BACK/OK buttons
    // (26/27).
    TouchScreenRect touchRects[28];
    // TRUE while the screen is in touch-input mode: the OK button is hidden and
    // the first key press only leaves touch mode.
    BOOL touchMode;
    // Font manager used to draw the BACK/OK button labels.
    FontOAMManager *fontManager;
    // Font OAMs for the BACK (0) and OK (1) button labels.
    FontOAM *buttonLabelOAMs[2];
    // Character-transfer allocations backing the button labels.
    CharTransferAllocation buttonLabelAllocs[2];
    // Window holding the prompt message at the bottom of the screen.
    Window messageWindow;
} NumberEntryScreen_Graphics;

#endif // POKEPLATINUM_STRUCT_02089688_SUB1_H
