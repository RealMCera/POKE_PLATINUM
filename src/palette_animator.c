#include "palette_animator.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "bg_window.h"
#include "hardware_palette.h"
#include "palette.h"
#include "sys_task_extensions.h"
#include "sys_task_manager.h"

// A PaletteAnimator runs a SysTask that blinks a single 16-color palette slot
// of a background layer. It snapshots the slot at creation, then rebuilds a
// working copy of it every 32-frame cycle and uploads that copy. The palette is
// written either into a PaletteData buffer or straight into the DS background
// palette in hardware, depending on whether a PaletteData was supplied.
// Callers use it to blink UI icons (battle cursors, the naming screen's touch
// icon, ...).

// Entries 11-14 are the blink entries: during the "off" part of the cycle they
// are all replaced with the color stored in entry 15.
#define BLINK_ENTRIES_MASK 0x7800

static void PaletteAnimator_Tick(SysTask *task, void *data);
static u8 PaletteAnimator_UpdateBlinkPalette(PaletteAnimator *animator);
static void PaletteAnimator_LoadMainBgToHardware(PaletteAnimator *animator, u16 *palette);
static void PaletteAnimator_LoadSubBgToHardware(PaletteAnimator *animator, u16 *palette);
static void PaletteAnimator_LoadMainBgToBuffer(PaletteAnimator *animator, u16 *palette);
static void PaletteAnimator_LoadSubBgToBuffer(PaletteAnimator *animator, u16 *palette);

PaletteAnimator *PaletteAnimator_New(PaletteData *paletteData, u16 bgLayer, u16 paletteSlot, enum HeapID heapID)
{
    PaletteAnimator *animator;
    SysTask *task;
    u16 *srcPalette;

    task = SysTask_StartAndAllocateParam(PaletteAnimator_Tick, sizeof(PaletteAnimator), 0, heapID);
    animator = (PaletteAnimator *)SysTask_GetParam(task);

    // With no PaletteData, edit the hardware BG palette directly; otherwise use
    // the unfaded buffer for the requested layer.
    if (paletteData != NULL) {
        if (bgLayer == 0) {
            srcPalette = PaletteData_GetUnfadedBuffer(paletteData, 0);
            animator->loadFunc = PaletteAnimator_LoadMainBgToBuffer;
        } else {
            srcPalette = PaletteData_GetUnfadedBuffer(paletteData, 1);
            animator->loadFunc = PaletteAnimator_LoadSubBgToBuffer;
        }
    } else {
        if (bgLayer == 0) {
            srcPalette = (u16 *)GetHardwareMainBgPaletteAddress();
            animator->loadFunc = PaletteAnimator_LoadMainBgToHardware;
        } else {
            srcPalette = (u16 *)GetHardwareSubBgPaletteAddress();
            animator->loadFunc = PaletteAnimator_LoadSubBgToHardware;
        }
    }

    // Copy the slot into both the untouched snapshot and the working copy.
    MI_CpuCopy16(&srcPalette[paletteSlot * 16], animator->originalPalette, 0x20);
    MI_CpuCopy16(&srcPalette[paletteSlot * 16], animator->workingPalette, 0x20);

    animator->task = task;
    animator->paletteData = paletteData;
    animator->bgLayer = bgLayer;
    animator->paletteSlot = paletteSlot;
    animator->state = 1;
    animator->frameCounter = 0;

    return animator;
}

// Changes how the blink task behaves:
//   0 - restart the blink cycle
//   1 - pause, holding the current frame
//   2 - stop, restoring the original palette and ending the task
void PaletteAnimator_SetMode(PaletteAnimator *animator, u8 mode)
{
    switch (mode) {
    case 0:
        animator->state = 0;
        break;
    case 1:
        animator->state = 2;
        break;
    case 2:
        animator->state = 3;
    }
}

void PaletteAnimator_Free(PaletteAnimator *animator)
{
    SysTask_FinishAndFreeParam(animator->task);
}

// Task state machine (struct field `state`):
//   0 - restart: clear the frame counter and move to state 1
//   1 - blinking: upload the working palette on the two update frames
//   2 - paused: leave the current palette untouched
//   3 - stopping: restore the original palette and end the task
static void PaletteAnimator_Tick(SysTask *task, void *data)
{
    PaletteAnimator *animator = (PaletteAnimator *)data;

    switch (animator->state) {
    case 0:
        animator->frameCounter = 0;
        animator->state = 1;
        break;
    case 1:
        if (PaletteAnimator_UpdateBlinkPalette(animator) == 1) {
            animator->loadFunc(animator, animator->workingPalette);
        }

        // Advance through the 32-frame blink cycle.
        animator->frameCounter++;

        if (animator->frameCounter == 32) {
            animator->frameCounter = 0;
        }
        break;
    case 2:
        break;
    case 3:
        animator->loadFunc(animator, animator->originalPalette);
        SysTask_FinishAndFreeParam(task);
    }
}

// Rewrites the blink entries of the working palette. Returns TRUE on the two
// frames that change it (0 and 24), which is when the caller should upload it.
static u8 PaletteAnimator_UpdateBlinkPalette(PaletteAnimator *animator)
{
    u32 i;

    if (animator->frameCounter == 0) {
        // Frame 0: restore the blink entries to their own colors.
        for (i = 0; i < 16; i++) {
            if ((BLINK_ENTRIES_MASK & (1 << i)) == 0) {
                continue;
            }

            animator->workingPalette[i] = animator->originalPalette[i];
        }

        return 1;
    } else if (animator->frameCounter == 24) {
        // Frame 24 (the last 8 frames of the cycle): make the blink entries use
        // the color stored in entry 15.
        for (i = 0; i < 16; i++) {
            if ((BLINK_ENTRIES_MASK & (1 << i)) == 0) {
                continue;
            }

            animator->workingPalette[i] = animator->originalPalette[15];
        }

        return 1;
    }

    return 0;
}

static void PaletteAnimator_LoadMainBgToHardware(PaletteAnimator *animator, u16 *palette)
{
    Bg_LoadPalette(BG_LAYER_MAIN_0, palette, PALETTE_SIZE_BYTES, PLTT_OFFSET(animator->paletteSlot));
}

static void PaletteAnimator_LoadSubBgToHardware(PaletteAnimator *animator, u16 *palette)
{
    Bg_LoadPalette(BG_LAYER_SUB_0, palette, PALETTE_SIZE_BYTES, PLTT_OFFSET(animator->paletteSlot));
}

static void PaletteAnimator_LoadMainBgToBuffer(PaletteAnimator *animator, u16 *palette)
{
    PaletteData_LoadBuffer(animator->paletteData, palette, PLTTBUF_MAIN_BG, PLTT_DEST(animator->paletteSlot), PALETTE_SIZE_BYTES);
}

static void PaletteAnimator_LoadSubBgToBuffer(PaletteAnimator *animator, u16 *palette)
{
    PaletteData_LoadBuffer(animator->paletteData, palette, PLTTBUF_SUB_BG, PLTT_DEST(animator->paletteSlot), PALETTE_SIZE_BYTES);
}
