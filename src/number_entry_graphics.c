#include "number_entry_graphics.h"

#include <nitro.h>
#include <string.h>

#include "struct_decls/font_oam.h"
#include "struct_defs/struct_020127E8.h"
#include "struct_defs/struct_02089688.h"
#include "struct_defs/struct_0208AF44.h"

#include "bg_window.h"
#include "char_transfer.h"
#include "font.h"
#include "game_options.h"
#include "graphics.h"
#include "inlines.h"
#include "message.h"
#include "narc.h"
#include "palette.h"
#include "render_window.h"
#include "sprite_system.h"
#include "string_gf.h"
#include "text.h"
#include "font_oam.h"

// Graphics and rendering for the number-entry screen. The input handling and
// phase state machine live in number_entry_screen.c; this module loads the
// screen's tiles, palettes and sprite resources, creates and lays out the
// digit/divider/control/button sprites, and draws the BACK/OK button labels.

// Allocates the sprite system and manager used by the screen and initialises
// them with the capacities needed for the digit, divider, control and button
// sprites.
void NumberEntryGraphics_InitSpriteSystem(NumberEntryScreen *param0)
{
    param0->graphics.spriteSystem = SpriteSystem_Alloc(HEAP_ID_101);
    {
        const RenderOamTemplate v0 = {
            0,
            128,
            0,
            32,
            0,
            128,
            0,
            32,
        };
        const CharTransferTemplateWithModes v1 = {
            48 + 48, 1024 * 0x40, 512 * 0x20, GX_OBJVRAMMODE_CHAR_1D_64K, GX_OBJVRAMMODE_CHAR_1D_32K
        };

        SpriteSystem_Init(param0->graphics.spriteSystem, &v0, &v1, 16 + 16);
    }

    {
        BOOL v2;
        const SpriteResourceCapacities v3 = {
            48 + 48,
            16 + 16,
            64,
            64,
            16,
            16,
        };

        param0->graphics.spriteManager = SpriteManager_New(param0->graphics.spriteSystem);

        v2 = SpriteSystem_InitSprites(param0->graphics.spriteSystem, param0->graphics.spriteManager, 64 + 64);
        GF_ASSERT(v2);

        v2 = SpriteSystem_InitManagerWithCapacities(param0->graphics.spriteSystem, param0->graphics.spriteManager, &v3);
        GF_ASSERT(v2);
    }
}

// Loads the screen's background layers (main BG 1 and sub BG 5) and their
// palettes, the three sprite resource sets (tags 1000-1002), and the
// message-box and font graphics.
void NumberEntryGraphics_LoadResources(NumberEntryScreen *param0)
{
    NARC *v0;
    BgConfig *v1;
    SpriteSystem *v2 = param0->graphics.spriteSystem;
    SpriteManager *v3 = param0->graphics.spriteManager;
    PaletteData *v4 = param0->graphics.paletteData;
    v1 = param0->graphics.bgConfig;
    v0 = param0->graphics.narc;

    Graphics_LoadTilesToBgLayerFromOpenNARC(v0, 12, v1, 1, 0, 0, 0, HEAP_ID_101);
    Graphics_LoadTilemapToBgLayerFromOpenNARC(v0, 14, v1, 1, 0, 0, 0, HEAP_ID_101);
    PaletteData_LoadBufferFromFileStart(v4, NARC_INDEX_ARC__CODEIN_GRA, 13, HEAP_ID_101, PLTTBUF_MAIN_BG, PALETTE_SIZE_BYTES, PLTT_DEST(0));

    Graphics_LoadTilesToBgLayerFromOpenNARC(v0, 15, v1, 5, 0, 0, 0, HEAP_ID_101);
    Graphics_LoadTilemapToBgLayerFromOpenNARC(v0, 17, v1, 5, 0, 0, 0, HEAP_ID_101);
    PaletteData_LoadBufferFromFileStart(v4, NARC_INDEX_ARC__CODEIN_GRA, 16, HEAP_ID_101, PLTTBUF_SUB_BG, PALETTE_SIZE_BYTES, PLTT_DEST(0));

    // Sprite resource set 1000: the digit sprites.
    SpriteSystem_LoadPaletteBufferFromOpenNarc(v4, PLTTBUF_MAIN_OBJ, v2, v3, v0, 1, FALSE, 1, NNS_G2D_VRAM_TYPE_2DMAIN, 1000);
    SpriteSystem_LoadCharResObjFromOpenNarc(v2, v3, v0, 0, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 1000);
    SpriteSystem_LoadCellResObjFromOpenNarc(v2, v3, v0, 2, FALSE, 1000);
    SpriteSystem_LoadAnimResObjFromOpenNarc(v2, v3, v0, 3, FALSE, 1000);

    // Sprite resource set 1001: the cursor and button controls.
    SpriteSystem_LoadPaletteBufferFromOpenNarc(v4, PLTTBUF_MAIN_OBJ, v2, v3, v0, 5, FALSE, 1, NNS_G2D_VRAM_TYPE_2DMAIN, 1001);
    SpriteSystem_LoadCharResObjFromOpenNarc(v2, v3, v0, 4, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 1001);
    SpriteSystem_LoadCellResObjFromOpenNarc(v2, v3, v0, 6, FALSE, 1001);
    SpriteSystem_LoadAnimResObjFromOpenNarc(v2, v3, v0, 7, FALSE, 1001);

    // Sprite resource set 1002: the button press effects.
    SpriteSystem_LoadPaletteBufferFromOpenNarc(v4, PLTTBUF_MAIN_OBJ, v2, v3, v0, 9, FALSE, 2, NNS_G2D_VRAM_TYPE_2DMAIN, 1002);
    SpriteSystem_LoadCharResObjFromOpenNarc(v2, v3, v0, 8, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 1002);
    SpriteSystem_LoadCellResObjFromOpenNarc(v2, v3, v0, 10, FALSE, 1002);
    SpriteSystem_LoadAnimResObjFromOpenNarc(v2, v3, v0, 11, FALSE, 1002);

    int v5 = Options_Frame(param0->args.options);

    LoadMessageBoxGraphics(v1, BG_LAYER_SUB_0, 1, 10, v5, HEAP_ID_101);
    PaletteData_LoadBufferFromFileStart(v4, NARC_INDEX_GRAPHIC__PL_WINFRAME, GetMessageBoxPaletteNARCMember(v5), HEAP_ID_101, PLTTBUF_SUB_BG, PALETTE_SIZE_BYTES, PLTT_DEST(11));
    PaletteData_LoadBufferFromFileStart(v4, NARC_INDEX_GRAPHIC__PL_FONT, 7, HEAP_ID_101, PLTTBUF_SUB_BG, PALETTE_SIZE_BYTES, PLTT_DEST(12));
}

// Frees every sprite created for the screen, then the button-label font
// resources and the message window.
void NumberEntryGraphics_Free(NumberEntryScreen *param0)
{
    int v0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        Sprite_DeleteAndFreeResources(param0->digits[v0].sprite);
    }

    for (v0 = 0; v0 < param0->dividerCount; v0++) {
        Sprite_DeleteAndFreeResources(param0->dividers[v0].sprite);
    }

    for (v0 = 0; v0 < 2; v0++) {
        Sprite_DeleteAndFreeResources(param0->buttonEffects[v0].sprite);
    }

    for (v0 = 0; v0 < 3; v0++) {
        Sprite_DeleteAndFreeResources(param0->controls[v0].sprite);
    }

    NumberEntryGraphics_FreeFont(param0);
    Window_Remove(&param0->graphics.messageWindow);
}

// Creates a sprite for every digit slot and group divider, laid out left to
// right. The leading locked slots are pre-filled from args.prefilledDigits, one decimal
// digit per slot.
void NumberEntryGraphics_CreateDigitSprites(NumberEntryScreen *param0)
{
    int i;
    int v0 = 0, v1 = 0;

    SpriteTemplate v2;
    SpriteSystem *v3 = param0->graphics.spriteSystem;
    SpriteManager *v4 = param0->graphics.spriteManager;

    v2.x = 0;
    v2.y = 0;
    v2.z = 0;
    v2.animIdx = 0;
    v2.priority = 10;
    v2.vramType = NNS_G2D_VRAM_TYPE_2DMAIN;
    v2.bgPriority = 0;
    v2.vramTransfer = FALSE;
    v2.plttIdx = 0;
    v2.resources[0] = 1000;
    v2.resources[1] = 1000;
    v2.resources[2] = 1000;
    v2.resources[3] = 1000;
    v2.resources[4] = SPRITE_RESOURCE_NONE;
    v2.resources[5] = SPRITE_RESOURCE_NONE;

    // Fill the locked leading slots from the least significant digit up.
    u32 v5 = param0->args.prefilledDigits;
    for (i = param0->prefilledDigitCount - 1; i >= 0; i--) {
        param0->digits[i].value = (v5 % 10) + 1;
        v5 /= 10;
    }

    // Walk the combined digit/divider row. A divider is inserted after the last
    // slot of each group; dividers[v0].value holds that slot index.
    for (i = 0; i < param0->digitCount + param0->dividerCount; i++) {
        if (param0->dividerCount != 0 && i == param0->dividers[v0].value + v0 + 1) {
            param0->dividers[v0].sprite = SpriteSystem_NewSprite(v3, v4, &v2);
            ManagedSprite_SetPositionXY(param0->dividers[v0].sprite, 76 + i * 8, 24);
            ManagedSprite_SetAnim(param0->dividers[v0].sprite, 22);
            ManagedSprite_TickFrame(param0->dividers[v0].sprite);
            v0++;
        } else {
            param0->digits[v1].sprite = SpriteSystem_NewSprite(v3, v4, &v2);
            ManagedSprite_SetPositionXY(param0->digits[v1].sprite, 76 + i * 8, 24);
            ManagedSprite_SetAnim(param0->digits[v1].sprite, NumberEntryGraphics_GetDigitAnim(param0->digits[v1].value, param0->digits[v1].isSelected));
            ManagedSprite_SetAffineOverwriteMode(param0->digits[v1].sprite, AFFINE_OVERWRITE_MODE_DOUBLE);
            ManagedSprite_TickFrame(param0->digits[v1].sprite);
            v1++;
        }
    }
}

// Creates the three control sprites: the digit cursor (0) and the BACK/OK
// button highlights (1, 2). The button highlights start hidden.
void NumberEntryGraphics_CreateControlSprites(NumberEntryScreen *param0)
{
    SpriteTemplate v0;
    SpriteSystem *v1 = param0->graphics.spriteSystem;
    SpriteManager *v2 = param0->graphics.spriteManager;
    PaletteData *v3 = param0->graphics.paletteData;

    v0.x = 0;
    v0.y = 0;
    v0.z = 0;
    v0.animIdx = 0;
    v0.priority = 0;
    v0.vramType = NNS_G2D_VRAM_TYPE_2DMAIN;
    v0.bgPriority = 0;
    v0.vramTransfer = FALSE;
    v0.plttIdx = 0;
    v0.resources[0] = 1001;
    v0.resources[1] = 1001;
    v0.resources[2] = 1001;
    v0.resources[3] = 1001;
    v0.resources[4] = SPRITE_RESOURCE_NONE;
    v0.resources[5] = SPRITE_RESOURCE_NONE;

    param0->controls[0].sprite = SpriteSystem_NewSprite(v1, v2, &v0);
    param0->controls[1].sprite = SpriteSystem_NewSprite(v1, v2, &v0);
    param0->controls[2].sprite = SpriteSystem_NewSprite(v1, v2, &v0);

    NumberEntryGraphics_MoveCursorToDigit(param0, param0->prefilledDigitCount);
    ManagedSprite_SetAnim(param0->controls[0].sprite, 0);
    ManagedSprite_TickFrame(param0->controls[0].sprite);

    // controls[1] is the keypad cursor; its anim.offsetX/offsetY hold the grid
    // column/row and value holds the idle animation index.
    param0->controls[1].anim.offsetX = 0;
    param0->controls[1].anim.offsetY = 0;
    param0->controls[1].value = 1;

    NumberEntryGraphics_MoveKeyCursor(param0, 0);
    ManagedSprite_SetAnim(param0->controls[1].sprite, param0->controls[1].value);
    ManagedSprite_TickFrame(param0->controls[1].sprite);
    ManagedSprite_SetExplicitOamMode(param0->controls[1].sprite, GX_OAM_MODE_XLU);

    param0->controls[2].anim.offsetX = 0;
    param0->controls[2].anim.offsetY = 0;
    param0->controls[2].value = 1;

    NumberEntryGraphics_MoveKeyCursor(param0, 0);
    ManagedSprite_SetAnim(param0->controls[2].sprite, param0->controls[2].value);
    ManagedSprite_TickFrame(param0->controls[2].sprite);
    ManagedSprite_SetExplicitOamMode(param0->controls[2].sprite, GX_OAM_MODE_XLU);
    NumberEntryGraphics_SetControlVisible(param0, 1, 0);
    NumberEntryGraphics_SetControlVisible(param0, 2, 0);
}

// Creates the two button press-effect sprites, positioned over the BACK (0) and
// OK (1) touch rectangles.
void NumberEntryGraphics_CreateButtonEffectSprites(NumberEntryScreen *param0)
{
    SpriteTemplate v0;
    SpriteSystem *v1 = param0->graphics.spriteSystem;
    SpriteManager *v2 = param0->graphics.spriteManager;
    PaletteData *v3 = param0->graphics.paletteData;

    v0.x = 0;
    v0.y = 0;
    v0.z = 0;
    v0.animIdx = 0;
    v0.priority = 10;
    v0.vramType = NNS_G2D_VRAM_TYPE_2DMAIN;
    v0.bgPriority = 0;
    v0.vramTransfer = FALSE;
    v0.plttIdx = 0;
    v0.resources[0] = 1002;
    v0.resources[1] = 1002;
    v0.resources[2] = 1002;
    v0.resources[3] = 1002;
    v0.resources[4] = SPRITE_RESOURCE_NONE;
    v0.resources[5] = SPRITE_RESOURCE_NONE;
    v0.plttIdx = 0;
    param0->buttonEffects[0].sprite = SpriteSystem_NewSprite(v1, v2, &v0);
    v0.plttIdx = 1;
    param0->buttonEffects[1].sprite = SpriteSystem_NewSprite(v1, v2, &v0);

    {
        s16 v4, v5;

        v4 = (param0->graphics.touchRects[26].rect.left + param0->graphics.touchRects[26].rect.right) / 2;
        v5 = (param0->graphics.touchRects[26].rect.top + param0->graphics.touchRects[26].rect.bottom) / 2;

        ManagedSprite_SetPositionXY(param0->buttonEffects[0].sprite, v4, v5);
        ManagedSprite_SetAnim(param0->buttonEffects[0].sprite, 0);
        ManagedSprite_TickFrame(param0->buttonEffects[0].sprite);

        v4 = (param0->graphics.touchRects[27].rect.left + param0->graphics.touchRects[27].rect.right) / 2;
        v5 = (param0->graphics.touchRects[27].rect.top + param0->graphics.touchRects[27].rect.bottom) / 2;

        ManagedSprite_SetPositionXY(param0->buttonEffects[1].sprite, v4, v5);
        ManagedSprite_SetAnim(param0->buttonEffects[1].sprite, 0);
        ManagedSprite_TickFrame(param0->buttonEffects[1].sprite);
    }
}

// Shows or hides control sprite param1.
void NumberEntryGraphics_SetControlVisible(NumberEntryScreen *param0, int param1, BOOL param2)
{
    if (param2 == 1) {
        ManagedSprite_SetDrawFlag(param0->controls[param1].sprite, 1);
    } else {
        ManagedSprite_SetDrawFlag(param0->controls[param1].sprite, 0);
    }
}

// Moves the digit cursor (controls[0]) onto slot param1, 16 pixels below the
// digit. Does nothing for pre-filled slots.
void NumberEntryGraphics_MoveCursorToDigit(NumberEntryScreen *param0, int param1)
{
    s16 v0, v1;
    ManagedSprite *v2;

    if (param1 < param0->prefilledDigitCount) {
        return;
    }

    v2 = param0->digits[param1].sprite;
    param0->controls[0].value = param1;

    ManagedSprite_GetPositionXY(v2, &v0, &v1);
    ManagedSprite_SetPositionXY(param0->controls[0].sprite, v0, v1 + 16);
}

// Centres the keypad cursor (controls[1]) on the touch rectangle of key param1
// (the digit keys start at touch rectangle 16).
void NumberEntryGraphics_MoveKeyCursor(NumberEntryScreen *param0, int param1)
{
    s16 v0 = (param0->graphics.touchRects[param1 + 16].rect.left + param0->graphics.touchRects[param1 + 16].rect.right) / 2;
    s16 v1 = (param0->graphics.touchRects[param1 + 16].rect.top + param0->graphics.touchRects[param1 + 16].rect.bottom) / 2;

    ManagedSprite_SetPositionXY(param0->controls[1].sprite, v0, v1);
}

// Centres control sprite param2 on the touch rectangle of key param1.
void NumberEntryGraphics_PositionControlAtKey(NumberEntryScreen *param0, int param1, int param2)
{
    s16 v0 = (param0->graphics.touchRects[param1 + 16].rect.left + param0->graphics.touchRects[param1 + 16].rect.right) / 2;
    s16 v1 = (param0->graphics.touchRects[param1 + 16].rect.top + param0->graphics.touchRects[param1 + 16].rect.bottom) / 2;

    ManagedSprite_SetPositionXY(param0->controls[param2].sprite, v0, v1);
}

// Advances the control sprites' animations and restores each button highlight's
// idle animation once its press animation (animation 3) has finished.
void NumberEntryGraphics_UpdateControls(NumberEntryScreen *param0)
{
    ManagedSprite_TickFrame(param0->controls[0].sprite);
    ManagedSprite_TickFrame(param0->controls[1].sprite);
    ManagedSprite_TickFrame(param0->controls[2].sprite);

    {
        int v0 = 2;
        int v1;
        BOOL v2;

        for (v0 = 1; v0 < 3; v0++) {
            v1 = ManagedSprite_GetActiveAnim(param0->controls[v0].sprite);

            if (v1 == 3) {
                v2 = ManagedSprite_IsAnimated(param0->controls[v0].sprite);

                if (v2 == 0) {
                    ManagedSprite_SetAnim(param0->controls[v0].sprite, param0->controls[v0].value);

                    if (param0->graphics.touchMode == 1) {
                        NumberEntryGraphics_SetControlVisible(param0, 1, 0);
                    } else {
                        NumberEntryGraphics_SetControlVisible(param0, 1, 1);
                    }

                    NumberEntryGraphics_SetControlVisible(param0, 2, 0);
                }
            } else {
                if (v1 != param0->controls[v0].value) {
                    ManagedSprite_SetAnim(param0->controls[v0].sprite, param0->controls[v0].value);
                }

                {
                    v1 = ManagedSprite_GetActiveAnim(param0->controls[2].sprite);

                    if (v1 != 3) {
                        if (param0->graphics.touchMode == 1) {
                            NumberEntryGraphics_SetControlVisible(param0, 1, 0);
                        } else {
                            NumberEntryGraphics_SetControlVisible(param0, 1, 1);
                        }
                    }
                }
            }
        }
    }
}

// Moves a button label's font OAM, tolerating a NULL OAM.
static void NumberEntryGraphics_SetButtonLabelPosition(FontOAM *param0, int param1, int param2)
{
    int v0;
    int v1;

    if (param0 != NULL) {
        FontOAM_SetXY(param0, param1, param2);
    }
}

// Runs the BACK/OK button press animation: the effect sprite plays frames 1 and
// 2 while the label is nudged up, then returns to frame 0.
void NumberEntryGraphics_UpdateButtonEffects(NumberEntryScreen *param0)
{
    int v0;
    s16 v1, v2;

    for (v0 = 0; v0 < 2; v0++) {
        v1 = (param0->graphics.touchRects[v0 + 26].rect.left + param0->graphics.touchRects[v0 + 26].rect.right) / 2;
        v1 -= 40;
        v2 = (param0->graphics.touchRects[v0 + 26].rect.top + param0->graphics.touchRects[v0 + 26].rect.bottom) / 2;
        v2 -= 7;

        switch (param0->buttonEffects[v0].value) {
        case 0:
            param0->buttonEffects[v0].anim.timer = 0;
            break;
        case 1:
            param0->buttonEffects[v0].anim.timer++;

            if (param0->buttonEffects[v0].anim.timer == 1) {
                ManagedSprite_SetAnim(param0->buttonEffects[v0].sprite, 1);
                NumberEntryGraphics_SetButtonLabelPosition(param0->graphics.buttonLabelOAMs[v0], v1, v2 - 0);
            } else if (param0->buttonEffects[v0].anim.timer == 2) {
                ManagedSprite_SetAnim(param0->buttonEffects[v0].sprite, 2);
                NumberEntryGraphics_SetButtonLabelPosition(param0->graphics.buttonLabelOAMs[v0], v1, v2 - 1);
            } else if (param0->buttonEffects[v0].anim.timer == 10) {
                ManagedSprite_SetAnim(param0->buttonEffects[v0].sprite, 0);
                NumberEntryGraphics_SetButtonLabelPosition(param0->graphics.buttonLabelOAMs[v0], v1, v2 + 0);
                param0->buttonEffects[v0].value++;
            }
            break;
        default:
            ManagedSprite_SetAnim(param0->buttonEffects[v0].sprite, 0);
            NumberEntryGraphics_SetButtonLabelPosition(param0->graphics.buttonLabelOAMs[v0], v1, v2);
            param0->buttonEffects[v0].value = 0;
            break;
        }
    }
}

// Returns the sprite animation index for a digit. Selected digits use
// animations 1-10; unselected digits use 12-21 (offset by 11).
int NumberEntryGraphics_GetDigitAnim(int param0, BOOL param1)
{
    int v0 = 0;

    if (param1 == 0) {
        v0 = 11;
    }

    v0 += param0;

    return v0;
}

// Marks each digit slot as selected if it lies in the highlighted group.
void NumberEntryGraphics_UpdateDigitSelection(NumberEntryScreen *param0)
{
    int v0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if ((v0 >= param0->selectedGroupStart) && (v0 < param0->selectedGroupEnd)) {
            param0->digits[v0].isSelected = 1;
        } else {
            param0->digits[v0].isSelected = 0;
        }
    }
}

// Lays out the digit and divider sprites horizontally. When param1 is 0 the
// sprites are moved immediately; otherwise each sprite is given a per-frame
// offset and a 2-frame countdown so the change animates.
void NumberEntryGraphics_LayoutDigits(NumberEntryScreen *param0, int param1)
{
    int v0;
    int v1;
    s16 v2;
    s16 v3, v4;

    v2 = param0->groupXPos[param0->selectedGroup];
    v1 = 0;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if ((v0 >= param0->selectedGroupStart) && (v0 < param0->selectedGroupEnd)) {
            // Slots in the highlighted group are spaced 32 pixels apart.
            if (v0 == param0->selectedGroupStart) {
                v2 += ((32 + 8) / 2);
            } else {
                v2 += 32;
            }
        } else {
            if (v0 == 0) {
                v2 += ((32 + 8) / 2);
            } else {
                v2 += 8;
            }
        }

        ManagedSprite_GetPositionXY(param0->digits[v0].sprite, &v3, &v4);

        if (param1 == 0) {
            ManagedSprite_SetPositionXY(param0->digits[v0].sprite, v2, v4);
        } else {
            param0->digits[v0].anim.offsetX = (v2 - v3) / 2;
            param0->digits[v0].anim.offsetY = 0;
            param0->digits[v0].anim.timer = 2;
            param0->digits[v0].anim.step = 0;
        }

        if ((v0 == param0->dividers[v1].value) && (v1 != param0->dividerCount)) {
            ManagedSprite_GetPositionXY(param0->dividers[v1].sprite, &v3, &v4);

            if (param0->selectedGroupStart == param0->selectedGroupEnd) {
                v2 += 8;
            } else {
                if ((v0 > param0->selectedGroupStart) && (v0 < param0->selectedGroupEnd)) {
                    v2 += ((32 + 8) / 2);
                } else {
                    v2 += 8;
                }
            }

            if (param1 == 0) {
                ManagedSprite_SetPositionXY(param0->dividers[v1].sprite, v2, v4);
            } else {
                param0->dividers[v1].anim.offsetX = (v2 - v3) / 2;
                param0->dividers[v1].anim.offsetY = 0;
                param0->dividers[v1].anim.timer = 2;
            }

            v1++;
        }
    }
}

// Resizes a digit slot's touch rectangle around its sprite position.
static inline void NumberEntryGraphics_UpdateDigitTouchRect(NumberEntryScreen *param0, int param1, s16 param2, s16 param3)
{
    s16 v0, v1;
    NumberEntrySprite *v2 = &param0->digits[param1];

    ManagedSprite_GetPositionXY(v2->sprite, &v0, &v1);

    v2->touchRect->rect.top = v1 - param3;
    v2->touchRect->rect.left = v0 - param2;
    v2->touchRect->rect.bottom = v1 + param3;
    v2->touchRect->rect.right = v0 + param2;
}

// Updates every digit slot's touch rectangle, using a larger box for slots in
// the highlighted group.
void NumberEntryGraphics_UpdateDigitTouchRects(NumberEntryScreen *param0)
{
    int v0;
    s16 v1;
    s16 v2;

    for (v0 = 0; v0 < param0->digitCount; v0++) {
        if ((v0 >= param0->selectedGroupStart) && (v0 < param0->selectedGroupEnd)) {
            v1 = 32 / 2;
            v2 = 32 / 2;
        } else {
            v1 = 8 / 2;
            v2 = 8;
        }

        NumberEntryGraphics_UpdateDigitTouchRect(param0, v0, v1, v2);
    }
}

// Creates the font manager and initialises the sub-screen font used for the
// button labels.
void NumberEntryGraphics_InitFont(NumberEntryScreen *param0)
{
    param0->graphics.fontManager = FontOAMManager_New(2, HEAP_ID_101);
    Font_InitManager(FONT_SUBSCREEN, HEAP_ID_101);
}

// Frees the sub-screen font, the button-label OAMs and their character-transfer
// allocations.
void NumberEntryGraphics_FreeFont(NumberEntryScreen *param0)
{
    Font_Free(FONT_SUBSCREEN);
    FontOAM_Free(param0->graphics.buttonLabelOAMs[0]);
    CharTransfer_ClearRange(&param0->graphics.buttonLabelAllocs[0]);
    FontOAM_Free(param0->graphics.buttonLabelOAMs[1]);
    CharTransfer_ClearRange(&param0->graphics.buttonLabelAllocs[1]);
    FontOAMManager_Free(param0->graphics.fontManager);
}

// Loads the palette used by the button-label font OAMs.
void NumberEntryGraphics_LoadButtonLabelPalette(NumberEntryScreen *param0)
{
    SpriteSystem_LoadPaletteBuffer(param0->graphics.paletteData, 2, param0->graphics.spriteSystem, param0->graphics.spriteManager, 14, 7, 0, 1, NNS_G2D_VRAM_TYPE_2DMAIN, 1003);
}

// Creates the BACK and OK button labels.
void NumberEntryGraphics_CreateButtonLabels(NumberEntryScreen *param0)
{
    NumberEntryGraphics_LoadButtonLabelPalette(param0);
    NumberEntryGraphics_CreateButtonLabel(param0, 0, 78, 165, 0);
    NumberEntryGraphics_CreateButtonLabel(param0, 1, 172, 165, 0);
}

// Creates one button label: renders its text into a window, allocates
// character-transfer space, and builds a font OAM at the button's position.
void NumberEntryGraphics_CreateButtonLabel(NumberEntryScreen *param0, int param1, int param2, int param3, int param4)
{
    s16 v0, v1;
    UnkStruct_020127E8 v2;
    String *v3;
    int v4;
    int v5;
    MessageLoader *v6;
    Window v7;

    v6 = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0212, HEAP_ID_101);
    v3 = MessageLoader_GetNewString(v6, 2 + param1);

    {
        Window_Init(&v7);
        Window_AddToTopLeftCorner(param0->graphics.bgConfig, &v7, 10, 2, 0, 0);
        Text_AddPrinterWithParamsAndColor(&v7, FONT_SUBSCREEN, v3, Font_CalcCenterAlignment(FONT_SUBSCREEN, v3, 0, 80), 0, TEXT_SPEED_NO_TRANSFER, TEXT_COLOR(15, 13, 2), NULL);
    }

    v4 = 1003;
    v5 = FontOAM_GetWindowSize(&v7, NNS_G2D_VRAM_TYPE_2DMAIN, HEAP_ID_101);

    CharTransfer_AllocRange(v5, 1, NNS_G2D_VRAM_TYPE_2DMAIN, &param0->graphics.buttonLabelAllocs[param1]);

    v0 = (param0->graphics.touchRects[param1 + 26].rect.left + param0->graphics.touchRects[param1 + 26].rect.right) / 2;
    v0 -= 40;
    v1 = (param0->graphics.touchRects[param1 + 26].rect.top + param0->graphics.touchRects[param1 + 26].rect.bottom) / 2;
    v1 -= 7;

    v2.unk_00 = param0->graphics.fontManager;
    v2.unk_04 = &v7;
    v2.unk_08 = SpriteManager_GetSpriteList(param0->graphics.spriteManager);
    v2.unk_0C = SpriteManager_FindPlttResourceProxy(param0->graphics.spriteManager, v4);
    v2.unk_10 = NULL;
    v2.unk_14 = param0->graphics.buttonLabelAllocs[param1].offset;
    v2.unk_18 = v0;
    v2.unk_1C = v1;
    v2.unk_20 = 0;
    v2.unk_24 = 0;
    v2.unk_28 = NNS_G2D_VRAM_TYPE_2DMAIN;
    v2.heapID = HEAP_ID_101;

    param0->graphics.buttonLabelOAMs[param1] = FontOAM_New(&v2);

    FontOAM_SetExplicitPaletteOffsetAutoAdjust(param0->graphics.buttonLabelOAMs[param1], param4);
    String_Free(v3);
    MessageLoader_Free(v6);
    Window_Remove(&v7);
}

// Initialises the message window, draws its frame and prints message param8.
void NumberEntryGraphics_InitMessageWindow(BgConfig *param0, Window *param1, int param2, int param3, int param4, int param5, int param6, int param7, int param8)
{
    Window_Init(param1);
    Window_Add(param0, param1, param2, param3, param4, param5, param6, 12, param7);
    Window_DrawMessageBoxWithScrollCursor(param1, 1, 1, 11);
    Window_FillTilemap(param1, 15);
    Window_CopyToVRAM(param1);
    NumberEntryGraphics_DrawMessage(param1, param8);
}

// Prints message param1 into the window.
void NumberEntryGraphics_DrawMessage(Window *param0, int param1)
{
    MessageLoader *v0;
    String *v1;

    Window_FillTilemap(param0, 15);

    v0 = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0212, HEAP_ID_101);
    v1 = MessageLoader_GetNewString(v0, param1);

    Window_FillTilemap(param0, 15);
    Text_AddPrinterWithParams(param0, FONT_MESSAGE, v1, 0, 0, TEXT_SPEED_INSTANT, NULL);
    Window_CopyToVRAM(param0);

    String_Free(v1);
    MessageLoader_Free(v0);
}
