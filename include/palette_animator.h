#ifndef POKEPLATINUM_PALETTE_ANIMATOR_H
#define POKEPLATINUM_PALETTE_ANIMATOR_H

#include <nitro.h>

#include "palette.h"
#include "sys_task_manager.h"

typedef struct PaletteAnimator PaletteAnimator;

// Uploads a 16-color palette to the animator's target: either a PaletteData
// buffer or the background palette in DS hardware.
typedef void (*PaletteAnimatorLoadFunc)(PaletteAnimator *, u16 *);

struct PaletteAnimator {
    SysTask *task;
    PaletteAnimatorLoadFunc loadFunc;
    PaletteData *paletteData;
    u16 originalPalette[16]; // snapshot of the slot; restored when stopping
    u16 workingPalette[16];  // copy that is edited and uploaded while blinking
    u8 bgLayer;              // 0 = main BG, 1 = sub BG
    u8 paletteSlot;          // palette slot index within the layer
    u8 state;                // task state; see PaletteAnimator_Tick
    u8 frameCounter;         // position in the 32-frame blink cycle
};

PaletteAnimator *PaletteAnimator_New(PaletteData *paletteData, u16 bgLayer, u16 paletteSlot, enum HeapID heapID);
void PaletteAnimator_SetMode(PaletteAnimator *animator, u8 mode);
void PaletteAnimator_Free(PaletteAnimator *animator);

#endif // POKEPLATINUM_PALETTE_ANIMATOR_H
