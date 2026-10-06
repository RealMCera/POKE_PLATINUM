#include "number_entry_screen.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_02089688.h"

#include "heap.h"
#include "network_icon.h"
#include "palette.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "sprite_system.h"
#include "string_gf.h"
#include "system.h"
#include "touch_screen.h"
#include "touch_screen_actions.h"
#include "number_entry_graphics.h"

// Number-entry screen. The player is shown a row of digit slots, split into
// groups (for example a Friend Code as XXXX-XXXX-XXXX), and moves a cursor
// between the slots to enter a number. The screen is driven by a small phase
// state machine (see sPhaseFuncs) and reports the entered number back to the
// launching application through args.unk_1C.

// Records `param1` as the highlighted group and caches the slot ranges of both
// the new and the previous group. NumberEntry_AnimateSelection uses the two
// ranges to grow the newly selected group's digits and shrink the old one's.
void NumberEntry_SetSelectedGroup(NumberEntryScreen *param0, int param1)
{
    int v0;

    param0->prevSelectedGroup = param0->selectedGroup;
    param0->selectedGroup = param1;
    param0->selectedGroupStart = 0;
    param0->selectedGroupEnd = 0;
    param0->prevGroupStart = 0;
    param0->prevGroupEnd = 0;

    if (param0->selectedGroup != 0) {
        param0->selectedGroupStart = param0->groupRanges[param0->selectedGroup - 1][0];
        param0->selectedGroupEnd = param0->groupRanges[param0->selectedGroup - 1][1];
    }

    if (param0->prevSelectedGroup != 0) {
        param0->prevGroupStart = param0->groupRanges[param0->prevSelectedGroup - 1][0];
        param0->prevGroupEnd = param0->groupRanges[param0->prevSelectedGroup - 1][1];
    }
}

// Derives the screen layout from the launching application's arguments:
// per-group slot ranges, the total digit count, the number of group separators,
// the x origin of each group's row, the separator positions, each slot's group,
// and how many leading slots are pre-filled and therefore locked.
void NumberEntry_InitLayout(NumberEntryScreen *param0)
{
    int v0;
    param0->graphics.touchMode = 1;
    u16 v1 = 0;
    int v2, v3, v4, v5;

    // groupRanges[g] = [first slot, one past last slot) for group g.
    for (v0 = 0; v0 < 4 + 1; v0++) {
        param0->groupRanges[v0][0] = v1;
        v1 += param0->args.unk_04[v0];
        param0->groupRanges[v0][1] = v1;
    }

    // args.unk_24 is the number of pre-filled groups; highlight the group after
    // them (1-based).
    NumberEntry_SetSelectedGroup(param0, param0->args.unk_24 + 1);

    // Count the non-empty groups and the total number of digit slots.
    for (v0 = 0; v0 < 4; v0++) {
        if (param0->args.unk_04[v0] == 0) {
            break;
        }

        param0->digitCount += param0->args.unk_04[v0];
        param0->dividerCount++;
    }

    // There is one separator fewer than there are groups.
    param0->dividerCount--;
    v2 = 8 * (param0->digitCount + param0->dividerCount);
    param0->groupXPos[0] = 112 - v2 / 2;

    // Centre each group's row, accounting for the wider spacing of the
    // currently selected group.
    for (v0 = 0; v0 < 4; v0++) {
        v2 = 8 * param0->dividerCount + (8 * (param0->digitCount - param0->args.unk_04[v0]) + 32 * param0->args.unk_04[v0]);

        param0->groupXPos[v0 + 1] = 112 - v2 / 2;
    }

    param0->groupXPos[1] += 12;
    v3 = 0;

    // A separator sits after the last slot of each group except the last one.
    for (v0 = 0; v0 < param0->dividerCount; v0++) {
        v3 += param0->args.unk_04[v0];
        param0->dividers[v0].value = v3 - 1;
    }

    v5 = 0;
    v0 = 0;

    // Tag every slot with its 1-based group number.
    do {
        for (v4 = 0; v4 < param0->args.unk_04[v5]; v4++) {
            param0->digits[v0].group = v5 + 1;
            v0++;
        }
        v5++;
    } while (v0 < param0->digitCount);

    // The first args.unk_24 groups are pre-filled and cannot be edited.
    for (v0 = 0; v0 < param0->args.unk_24; v0++) {
        param0->prefilledDigitCount += param0->args.unk_04[v0];
    }
}

// Switches to phase `param1` and resets the per-phase step and animation timer.
void NumberEntry_SetPhase(NumberEntryScreen *param0, int param1)
{
    param0->phase = param1;
    param0->unk_2C4 = 0;
    param0->phaseStep = 0;
    param0->animTimer = 0;
}

// Phase 0: load the graphics, build the sprites and touch rectangles, and start
// the fade-in. Advances to phase 1.
BOOL NumberEntry_Setup(NumberEntryScreen *param0)
{
    NumberEntryGraphics_LoadResources(param0);
    NumberEntryGraphics_UpdateDigitSelection(param0);
    NumberEntryGraphics_CreateDigitSprites(param0);
    NumberEntryGraphics_LayoutDigits(param0, 0);
    NumberEntryGraphics_CreateControlSprites(param0);
    NumberEntryGraphics_CreateButtonEffectSprites(param0);
    NumberEntryGraphics_UpdateDigitTouchRects(param0);
    NumberEntryGraphics_InitFont(param0);
    NumberEntryGraphics_CreateButtonLabels(param0);
    NumberEntryGraphics_InitMessageWindow(param0->graphics.bgConfig, &param0->graphics.messageWindow, 4, 2, 21, 27, 2, 100, param0->args.unk_2C);

    // When the screen is used for a Wi-Fi connection, load the connection
    // strength icon palette.
    if (param0->args.unk_30 != 0) {
        NNSG2dPaletteData *v0;
        void *v1 = NetworkIcon_GetPalette(HEAP_ID_101);

        NNS_G2dGetUnpackedPaletteData(v1, &v0);
        PaletteData_LoadBuffer(param0->graphics.paletteData, v0->pRawData, PLTTBUF_SUB_OBJ, PLTT_DEST(14), PALETTE_SIZE_BYTES);
        Heap_Free(v1);
    }

    NumberEntry_SetPhase(param0, 1);
    StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, COLOR_BLACK, 6, 1, HEAP_ID_101);

    return 0;
}

// Phase 3: fade the screen out, then report completion to the application.
BOOL NumberEntry_FadeOut(NumberEntryScreen *param0)
{
    switch (param0->phaseStep) {
    case 0:
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 6, 1, HEAP_ID_101);
        param0->phaseStep++;
        break;
    case 1:
        if (IsScreenFadeDone() == TRUE) {
            param0->phaseStep++;
        }
        break;
    default:
        return 1;
    }

    return 0;
}

// Phase 1: wait for the fade-in to finish, then consume any queued selection
// action, handle touch input, and read the keypad.
BOOL NumberEntry_UpdateInput(NumberEntryScreen *param0)
{
    switch (param0->phaseStep) {
    case 0:
        if (IsScreenFadeDone() == TRUE) {
            param0->phaseStep++;
        }
        break;
    default:
        NumberEntry_ProcessSelectionAction(param0);
        TouchScreenActions_HandleAction(param0->graphics.touchScreenActions);
        NumberEntry_ProcessInput(param0);
        break;
    }

    return 0;
}

// Phase 2: animate the change of highlighted group. The newly selected group's
// digits grow (v1) while the previously selected group's digits shrink (v2);
// both tables are indexed by an animation step that advances once per frame.
// Once the animation finishes, the cursor is moved into the new group and the
// screen returns to phase 1.
BOOL NumberEntry_AnimateSelection(NumberEntryScreen *param0)
{
    int v0;
    static f32 v1[] = {
        0.5f, 0.2f, 0.5f, 1.0f, 1.2f, 1.0f, 1.0f
    };
    static f32 v2[] = {
        0.8f, 0.6f, 0.4f, 0.2f, 0.8f, 1.0f, 1.0f
    };

    switch (param0->phaseStep) {
    case 0:
        NumberEntryGraphics_SetControlVisible(param0, 0, 0);
        {
            for (v0 = 0; v0 < param0->digitCount; v0++) {
                if (param0->digits[v0].anim.timer == 0) {
                    continue;
                }

                ManagedSprite_OffsetPositionXY(param0->digits[v0].sprite, param0->digits[v0].anim.offsetX, param0->digits[v0].anim.offsetY);
                param0->digits[v0].anim.timer--;

                if ((v0 >= param0->selectedGroupStart) && (v0 < param0->selectedGroupEnd)) {
                    ManagedSprite_SetAffineScale(param0->digits[v0].sprite, v1[param0->digits[v0].anim.step], v1[param0->digits[v0].anim.step]);
                    param0->digits[v0].anim.step++;
                }

                if ((v0 >= param0->prevGroupStart) && (v0 < param0->prevGroupEnd)) {
                    ManagedSprite_SetAffineScale(param0->digits[v0].sprite, v2[param0->digits[v0].anim.step], v2[param0->digits[v0].anim.step]);
                    param0->digits[v0].anim.step++;
                }
            }

            for (v0 = 0; v0 < param0->dividerCount; v0++) {
                if (param0->dividers[v0].anim.timer == 0) {
                    continue;
                }

                ManagedSprite_OffsetPositionXY(param0->dividers[v0].sprite, param0->dividers[v0].anim.offsetX, param0->dividers[v0].anim.offsetY);
                param0->dividers[v0].anim.timer--;
            }

            if (param0->digits[0].anim.timer == 0) {
                for (v0 = param0->selectedGroupStart; v0 < param0->selectedGroupEnd; v0++) {
                    ManagedSprite_SetAnim(param0->digits[v0].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v0].value, param0->digits[v0].isSelected));
                    ManagedSprite_TickFrame(param0->digits[v0].sprite);
                }

                for (v0 = param0->prevGroupStart; v0 < param0->prevGroupEnd; v0++) {
                    ManagedSprite_SetAnim(param0->digits[v0].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v0].value, param0->digits[v0].isSelected));
                    ManagedSprite_TickFrame(param0->digits[v0].sprite);
                }

                param0->phaseStep++;
            }

            param0->animTimer++;
        }
        break;
    case 1:
        for (v0 = param0->selectedGroupStart; v0 < param0->selectedGroupEnd; v0++) {
            if (param0->digits[v0].anim.step == 6) {
                continue;
            }

            ManagedSprite_SetAffineScale(param0->digits[v0].sprite, v1[param0->digits[v0].anim.step], v1[param0->digits[v0].anim.step]);
            param0->digits[v0].anim.step++;
        }

        for (v0 = param0->prevGroupStart; v0 < param0->prevGroupEnd; v0++) {
            if (param0->digits[v0].anim.step == 6) {
                continue;
            }

            ManagedSprite_SetAffineScale(param0->digits[v0].sprite, v2[param0->digits[v0].anim.step], v2[param0->digits[v0].anim.step]);
            param0->digits[v0].anim.step++;
        }

        param0->animTimer++;

        if (param0->animTimer == 6) {
            param0->phaseStep++;
        }
        break;
    default:
        NumberEntryGraphics_UpdateDigitTouchRects(param0);

        // Move the cursor to the first or last slot of the newly selected
        // group, depending on the direction the player moved.
        if (param0->selectionAction.selectLastDigit == 0) {
            NumberEntryGraphics_MoveCursorToDigit(param0, NumberEntry_GetFirstDigitInGroup(param0, param0->selectionAction.value));
        } else {
            NumberEntryGraphics_MoveCursorToDigit(param0, NumberEntry_GetLastDigitInGroup(param0, param0->selectionAction.value));
        }

        if (param0->selectedGroup != 0) {
            NumberEntryGraphics_SetControlVisible(param0, 0, 1);
        }

        NumberEntry_ClearSelectionAction(param0);
        NumberEntry_SetPhase(param0, 1);
        break;
    }

    return 0;
}

// Phase handlers, indexed by NumberEntryScreen.phase.
static BOOL (*const sPhaseFuncs[])(NumberEntryScreen *) = {
    NumberEntry_Setup,
    NumberEntry_UpdateInput,
    NumberEntry_AnimateSelection,
    NumberEntry_FadeOut,
};

// Runs the current phase handler, then updates the button sprites and draws
// everything. Returns TRUE once the screen is finished.
BOOL NumberEntry_Update(NumberEntryScreen *param0)
{
    BOOL v0 = sPhaseFuncs[param0->phase](param0);

    NumberEntryGraphics_UpdateControls(param0);
    NumberEntryGraphics_UpdateButtonEffects(param0);
    SpriteSystem_DrawSprites(param0->graphics.spriteManager);

    return v0;
}

// Reads the keypad. The cursor is laid out on a 5x3 grid: the top two rows are
// the ten digit keys (0-9) and the bottom row holds the BACK and OK buttons
// (cells 10 and 11). The cursor position is stored in controls[1].anim.
void NumberEntry_ProcessInput(NumberEntryScreen *param0)
{
    const int sCursorGrid[][5] = {
        { 0, 1, 2, 3, 4 },
        { 5, 6, 7, 8, 9 },
        { 10, 10, 10, 11, 11 },
    };
    BOOL v1 = FALSE;
    int v2 = sCursorGrid[param0->controls[1].anim.offsetY][param0->controls[1].anim.offsetX];

    if (param0->phase != 1 || param0->selectionAction.type == 1) {
        return;
    }

    // In touch mode the OK button is hidden; the first key press only leaves
    // touch mode and is otherwise ignored.
    if (param0->graphics.touchMode == TRUE) {
        if (gSystem.pressedKeys && !TouchScreen_Touched()) {
            param0->graphics.touchMode = FALSE;
            NumberEntryGraphics_MoveKeyCursor(param0, v2);
            if (v2 == 10 || v2 == 11) {
                if (param0->controls[1].value != 2) {
                    param0->controls[1].value = 2;
                }
            } else {
                if (param0->controls[1].value != 1) {
                    param0->controls[1].value = 1;
                }
            }
        }
        return;
    }

    if (gSystem.pressedKeysRepeatable & PAD_KEY_UP) {
        if (param0->controls[1].anim.offsetY > 0) {
            param0->controls[1].anim.offsetY--;
        } else {
            param0->controls[1].anim.offsetY = 2;
        }
        v1 = TRUE;
    } else if (gSystem.pressedKeysRepeatable & PAD_KEY_DOWN) {
        param0->controls[1].anim.offsetY++;
        param0->controls[1].anim.offsetY %= 3;
        v1 = TRUE;
    } else if (gSystem.pressedKeysRepeatable & PAD_KEY_RIGHT) {

        if (v2 == 10) {
            param0->controls[1].anim.offsetX = 3;
        } else if (v2 == 11) {
            param0->controls[1].anim.offsetX = 0;
        } else {
            param0->controls[1].anim.offsetX++;
            param0->controls[1].anim.offsetX %= 5;
        }
        v1 = TRUE;
    } else if (gSystem.pressedKeysRepeatable & PAD_KEY_LEFT) {

        if (v2 == 10) {
            param0->controls[1].anim.offsetX = 3;
        } else if (v2 == 11) {
            param0->controls[1].anim.offsetX = 0;
        } else {
            if (param0->controls[1].anim.offsetX > 0) {
                param0->controls[1].anim.offsetX--;
            } else {
                param0->controls[1].anim.offsetX = 4;
            }
        }
        v1 = TRUE;
    } else if (gSystem.pressedKeys & PAD_BUTTON_A) {
        int v3;
        int v4;
        int v5;

        if (v2 == 10) {
            NumberEntry_Cancel(param0);

            Sound_PlayEffect(1509);
        } else if (v2 == 11) {
            NumberEntry_Confirm(param0);

            Sound_PlayEffect(1506);
        } else {
            if (param0->selectedGroup == 0) {
                return;
            }

            // Write the chosen digit into the current slot and advance the
            // cursor. If the next slot belongs to a different group, queue a
            // group change instead of a plain cursor move.
            v3 = param0->controls[0].value;
            param0->digits[v3].value = v2 + 1;
            NumberEntryGraphics_SetControlVisible(param0, 1, FALSE);
            NumberEntryGraphics_SetControlVisible(param0, 2, TRUE);
            NumberEntryGraphics_PositionControlAtKey(param0, v2, 2);
            ManagedSprite_SetAnim(param0->digits[v3].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v3].value, param0->digits[v3].isSelected));
            ManagedSprite_SetAnim(param0->controls[2].sprite, 3);

            v4 = param0->digits[v3].group;
            v3++;
            if (v3 == param0->digitCount) {

                param0->selectionAction.type = 1;
                param0->selectionAction.value = 0;
                param0->controls[1].anim.offsetX = 3;
                param0->controls[1].anim.offsetY = 2;
                v1 = TRUE;
            } else {
                v5 = param0->digits[v3].group;

                if (v4 != v5) {

                    param0->selectionAction.type = 1;
                    param0->selectionAction.value = v5;
                } else {
                    param0->selectionAction.type = 2;
                    param0->selectionAction.value = v3;
                }
                Sound_PlayEffect(1509);
            }
        }
    } else if (gSystem.pressedKeys & PAD_BUTTON_B) {
        NumberEntry_Cancel(param0);
        Sound_PlayEffect(1509);
    } else if (gSystem.pressedKeysRepeatable & PAD_BUTTON_L) {
        int v6 = param0->controls[0].value;

        // L/R wrap around the digit slots, skipping the pre-filled ones.
        if (v6 == param0->prefilledDigitCount) {
            param0->controls[0].value = param0->digitCount - 1;
        } else {
            param0->controls[0].value--;
        }
        v6 = param0->controls[0].value;

        if (param0->digits[v6].isSelected == 1) {
            param0->selectionAction.type = 2;
            param0->selectionAction.value = v6;
        } else {
            param0->selectionAction.type = 1;
            param0->selectionAction.value = param0->digits[v6].group;
            param0->selectionAction.selectLastDigit = 1;
        }
        Sound_PlayEffect(1504);
    } else if (gSystem.pressedKeysRepeatable & PAD_BUTTON_R) {
        int v7 = param0->controls[0].value;

        if (v7 == param0->digitCount - 1) {
            param0->controls[0].value = param0->prefilledDigitCount;
        } else {
            param0->controls[0].value++;
        }
        v7 = param0->controls[0].value;

        if (param0->digits[v7].isSelected == 1) {
            param0->selectionAction.type = 2;
            param0->selectionAction.value = v7;
        } else {
            param0->selectionAction.type = 1;
            param0->selectionAction.value = param0->digits[v7].group;
        }
        Sound_PlayEffect(1504);
    }

    if (v1 == TRUE) {
        Sound_PlayEffect(1504);

        v2 = sCursorGrid[param0->controls[1].anim.offsetY][param0->controls[1].anim.offsetX];
        NumberEntryGraphics_MoveKeyCursor(param0, v2);

        if (v2 == 10 || v2 == 11) {
            if (param0->controls[1].value != 2) {
                param0->controls[1].value = 2;
            }
        } else {
            if (param0->controls[1].value != 1) {
                param0->controls[1].value = 1;
            }
        }
    }
}

// OK button: fill any empty slots with 0, concatenate every digit into
// args.unk_1C, and advance to the fade-out phase.
void NumberEntry_Confirm(NumberEntryScreen *param0)
{
    int v0;
    u32 v1 = 0;
    String *v2 = String_Init(100, HEAP_ID_101);

    param0->buttonEffects[1].value = 1;
    param0->buttonEffects[1].anim.timer = 0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if (param0->digits[v0].value == 0) {
            param0->digits[v0].value = 1;
            ManagedSprite_SetAnim(param0->digits[v0].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v0].value, param0->digits[v0].isSelected));
        }

        v1 = param0->digits[v0].value - 1;
        String_FormatInt(v2, v1, 1, 1, 1);
        String_Concat(param0->args.unk_1C, v2);
    }

    String_Free(v2);
    NumberEntry_SetPhase(param0, 3);
}

// BACK button: clear the current slot and move the cursor to the previous one,
// queueing a group change if the previous slot is in a different group.
void NumberEntry_Cancel(NumberEntryScreen *param0)
{
    int v0;
    int v1;
    int v2;

    param0->buttonEffects[0].value = 1;
    param0->buttonEffects[0].anim.timer = 0;

    // With no group highlighted, jump to the last slot.
    if (param0->selectedGroup == 0) {
        v0 = param0->controls[0].value = param0->digitCount - 1;
        v2 = param0->digits[v0].group;

        param0->selectionAction.type = 1;
        param0->selectionAction.value = v2;
        param0->selectionAction.selectLastDigit = 1;

        return;
    }

    v0 = param0->controls[0].value;
    param0->digits[v0].value = 0;

    ManagedSprite_SetAnim(param0->digits[v0].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v0].value, param0->digits[v0].isSelected));
    v1 = param0->digits[v0].group;

    if (v0 > param0->prefilledDigitCount) {
        v0--;
        ManagedSprite_SetAnim(param0->digits[v0].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v0].value, param0->digits[v0].isSelected));

        v2 = param0->digits[v0].group;

        if (v1 != v2) {
            param0->selectionAction.type = 1;
            param0->selectionAction.value = v2;
            param0->selectionAction.selectLastDigit = 1;
        } else {
            param0->selectionAction.type = 2;
            param0->selectionAction.value = v0;
        }
    }
}

// Builds the touch rectangles for the digit slots and the BACK/OK buttons and
// registers the touch handler.
void NumberEntry_InitTouchScreen(NumberEntryScreen *param0)
{
    int v0;

    for (v0 = 0; v0 < 15 + 1; v0++) {
        param0->digits[v0].touchRect = &param0->graphics.touchRects[v0];
    }

    {
        // { centre x, centre y, half width, half height } for the ten digit
        // keys followed by the BACK and OK buttons.
        const s16 v1[][4] = {
            { 32, 80, 20, 20 },
            { 80, 80, 20, 20 },
            { 128, 80, 20, 20 },
            { 176, 80, 20, 20 },
            { 224, 80, 20, 20 },
            { 32, 128, 20, 20 },
            { 80, 128, 20, 20 },
            { 128, 128, 20, 20 },
            { 176, 128, 20, 20 },
            { 224, 128, 20, 20 },
            { 64, 176, 60, 12 },
            { 192, 176, 60, 12 },
        };

        for (; v0 < 0x1c; v0++) {
            param0->graphics.touchRects[v0].rect.top = v1[v0 - 16][1] - v1[v0 - 16][3];
            param0->graphics.touchRects[v0].rect.left = v1[v0 - 16][0] - v1[v0 - 16][2];
            param0->graphics.touchRects[v0].rect.bottom = v1[v0 - 16][1] + v1[v0 - 16][3];
            param0->graphics.touchRects[v0].rect.right = v1[v0 - 16][0] + v1[v0 - 16][2];
        }
    }

    param0->graphics.touchScreenActions = TouchScreenActions_RegisterHandler(param0->graphics.touchRects, 0x1c, NumberEntry_TouchCallback, param0, HEAP_ID_101);
}

// Touch handler. Rectangles 0-15 are the digit slots, 16-25 the digit keys, and
// 26/27 the BACK/OK buttons. A touch queues a selection action that
// NumberEntry_ProcessSelectionAction consumes on the next frame.
void NumberEntry_TouchCallback(u32 param0, enum TouchScreenButtonState param1, void *param2)
{
    NumberEntryScreen *v0 = param2;

    if (v0->phase != 1) {
        return;
    }

    if (v0->graphics.touchMode != 1) {
        v0->graphics.touchMode = 1;
    }

    if (param1 == TOUCH_BUTTON_PRESSED) {
        if ((param0 >= 0) && (param0 < 16)) {
            if (param0 < v0->prefilledDigitCount) {
                return;
            }

            if (v0->digits[param0].isSelected == 1) {
                v0->selectionAction.type = 2;
                v0->selectionAction.value = param0;
            } else {
                v0->selectionAction.type = 1;
                v0->selectionAction.value = v0->digits[param0].group;
            }

            Sound_PlayEffect(SEQ_SE_DP_BUTTON3_sseq);
        } else {
            if (param0 == 26) {
                v0->controls[1].anim.offsetX = 0;
                v0->controls[1].anim.offsetY = 2;
                Sound_PlayEffect(SEQ_SE_DP_BUTTON3_sseq);
            } else if (param0 == 27) {
                v0->controls[1].anim.offsetX = 3;
                v0->controls[1].anim.offsetY = 2;
                Sound_PlayEffect(SEQ_SE_DP_PIRORIRO_sseq);
            } else {
                v0->controls[1].anim.offsetX = (param0 - 16) % 5;
                v0->controls[1].anim.offsetY = (param0 - 16) / 5;
                Sound_PlayEffect(SEQ_SE_DP_BUTTON3_sseq);
            }

            if ((param0 >= 16) && (param0 <= 25)) {
                int v1;
                int v2;
                int v3;

                if (v0->selectedGroup == 0) {
                    return;
                }

                v1 = v0->controls[0].value;
                v0->digits[v1].value = param0 - 16 + 1;

                ManagedSprite_SetAnim(v0->digits[v1].sprite, NumberEntryGraphics_GetDigitAnim(v0->digits[v1].value, v0->digits[v1].isSelected));
                NumberEntryGraphics_SetControlVisible(v0, 1, 1);
                NumberEntryGraphics_MoveKeyCursor(v0, param0 - 16);
                NumberEntryGraphics_SetControlVisible(v0, 1, 0);
                NumberEntryGraphics_SetControlVisible(v0, 2, 1);
                NumberEntryGraphics_PositionControlAtKey(v0, param0 - 16, 2);
                ManagedSprite_SetAnim(v0->controls[2].sprite, 3);

                v2 = v0->digits[v1].group;
                v1++;

                if (v1 == v0->digitCount) {
                    v0->selectionAction.type = 1;
                    v0->selectionAction.value = 0;
                    v0->selectionAction.selectLastDigit = 0;
                } else {
                    v3 = v0->digits[v1].group;

                    if (v2 != v3) {
                        v0->selectionAction.type = 1;
                        v0->selectionAction.value = v3;
                        v0->selectionAction.selectLastDigit = 0;
                    } else {
                        v0->selectionAction.type = 2;
                        v0->selectionAction.value = v1;
                    }
                }
            } else {
                if (param0 == 26) {
                    NumberEntry_Cancel(v0);
                } else {
                    NumberEntry_Confirm(v0);
                }
            }
        }
    }
}

// Consumes the queued selection action. Type 1 changes the highlighted group
// (and starts the transition animation); type 2 moves the cursor to a slot.
void NumberEntry_ProcessSelectionAction(NumberEntryScreen *param0)
{
    switch (param0->selectionAction.type) {
    case 0:
        break;
    case 1:
        NumberEntry_SetSelectedGroup(param0, param0->selectionAction.value);
        NumberEntryGraphics_UpdateDigitSelection(param0);
        NumberEntryGraphics_LayoutDigits(param0, 1);
        NumberEntry_SetPhase(param0, 2);
        param0->selectionAction.type = 0xFF;
        break;
    case 2:
        NumberEntryGraphics_MoveCursorToDigit(param0, param0->selectionAction.value);
        NumberEntry_ClearSelectionAction(param0);
        break;
    case 0xFF:
        break;
    }
}

// Clears the queued selection action.
void NumberEntry_ClearSelectionAction(NumberEntryScreen *param0)
{
    param0->selectionAction.type = 0;
    param0->selectionAction.value = 0;
    param0->selectionAction.selectLastDigit = 0;
}

// Returns the index of the first digit slot belonging to group `param1`.
int NumberEntry_GetFirstDigitInGroup(NumberEntryScreen *param0, int param1)
{
    int v0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if (param0->digits[v0].group == param1) {
            return v0;
        }
    }

    return 0;
}

// Returns the index of the last digit slot belonging to group `param1`.
int NumberEntry_GetLastDigitInGroup(NumberEntryScreen *param0, int param1)
{
    int v0;
    int v1 = 0;
    int v2 = 0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if (param0->digits[v0].group == param1) {
            v2 = 1;
        } else {
            if (v2 == 1) {
                return v0 - 1;
            }
        }
    }

    return param0->digitCount - 1;
}
