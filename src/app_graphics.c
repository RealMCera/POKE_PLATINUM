#include "app_graphics.h"

#include <nitro.h>
#include <string.h>

#include "bg_window.h"
#include "heap.h"
#include "narc.h"
#include "screen_fade.h"
#include "system.h"

// Sizes, in bytes, of various uncompressed graphic resources, indexed by an
// identifier chosen by the caller. Index 6 (0x100 bytes) is the size of a
// type-icon's character data, as used by the battle subscreen.
__attribute__((aligned(4))) static const u16 sGraphicSizes[] = {
    0x20,
    0x80,
    0x200,
    0x800,
    0x40,
    0x80,
    0x100,
    0x400,
    0x40,
    0x80,
    0x100,
    0x400
};

int App_GetGraphicSize(int graphicIndex)
{
    graphicIndex -= 0;
    return sGraphicSizes[graphicIndex];
}

// Computes the integer Euclidean distance sqrt(dx^2 + dy^2). The sum of squares
// is shifted left by 4 before the fixed-point square root and the result is
// shifted right by 2, which cancels out to an integer square root.
u32 App_Distance(u32 dx, u32 dy)
{
    u32 sumSquares = (dx * dx) + (dy * dy);
    sumSquares = SVC_Sqrt(sumSquares << 4);

    return sumSquares >> 2;
}

u8 App_PixelCount(u32 cur, u32 max, u8 maxPixels)
{
    // Scale cur / max to the bar's pixel width, rounding down. A non-zero value
    // always shows at least one pixel so that a partially-filled bar is visible.
    u8 pixels = cur * maxPixels / max;
    if (pixels == 0 && cur > 0) {
        pixels = 1;
    }

    return pixels;
}

u8 App_BarColor(u32 cur, u32 max)
{
    // Both values are scaled by 256; the comparisons below are equivalent to
    // comparing the unscaled ratios.
    cur <<= 8;
    max <<= 8;

    // More than half full: green. More than one fifth full: yellow. Any other
    // non-zero value: red. The bar is empty only when cur is zero.
    if (cur > max / 2) {
        return BARCOLOR_GREEN;
    }

    if (cur > max / 5) {
        return BARCOLOR_YELLOW;
    }

    if (cur > 0) {
        return BARCOLOR_RED;
    }

    return BARCOLOR_EMPTY;
}

u8 HealthBar_Color(u16 curHP, u16 maxHP, u32 barSize)
{
    // A full health bar uses its own color rather than the ratio-based colors.
    if (curHP == maxHP) {
        return BARCOLOR_MAX;
    }

    return App_BarColor(App_PixelCount(curHP, maxHP, barSize), barSize);
}

void App_StartScreenFade(u8 fadeOut, enum HeapID heapID)
{
    if (fadeOut == FALSE) {
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, COLOR_BLACK, 6, 1, heapID);
    } else {
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 6, 1, heapID);
    }
}

/**
 * @brief Adjust a value with the D-pad, wrapping around at the bounds.
 *
 * Up and Right increase the value, Down and Left decrease it; Left and Right
 * step by 10 while Up and Down step by 1. The value wraps to the opposite bound
 * when it would leave the range [1, max].
 *
 * @param value Pointer to the value to adjust in place.
 * @param max   The maximum value; the minimum is 1.
 * @return 0 if the value did not change, 1 if it increased, 2 if it decreased.
 */
u8 App_AdjustValueWithDPad(s16 *value, u16 max)
{
    s16 previous = *value;

    if (gSystem.pressedKeysRepeatable & PAD_KEY_UP) {
        *value += 1;

        if (*value > max) {
            *value = 1;
        }

        if (*value == previous) {
            return 0;
        }

        return 1;
    }

    if (gSystem.pressedKeysRepeatable & PAD_KEY_DOWN) {
        *value -= 1;

        if (*value <= 0) {
            *value = max;
        }

        if (*value == previous) {
            return 0;
        }

        return 2;
    }

    if (gSystem.pressedKeysRepeatable & PAD_KEY_LEFT) {
        *value -= 10;

        if (*value <= 0) {
            *value = 1;
        }

        if (*value == previous) {
            return 0;
        }

        return 2;
    }

    if (gSystem.pressedKeysRepeatable & PAD_KEY_RIGHT) {
        *value += 10;

        if (*value > max) {
            *value = max;
        }

        if (*value == previous) {
            return 0;
        }

        return 1;
    }

    return 0;
}

void App_LoadGraphicMember(BgConfig *bgConfig, enum HeapID heapID, NARC *narc, int unused, int memberIndex, int bgLayer, enum GraphicMemberType memberType, u16 memberSize, u16 offset)
{
    u32 narcSize;
    void *dest;
    NNSG2dCharacterData *ppCharData;
    NNSG2dScreenData *ppScrData;
    NNSG2dPaletteData *ppPltData;

    narcSize = NARC_GetMemberSize(narc, memberIndex);
    dest = Heap_AllocAtEnd(heapID, narcSize);

    NARC_ReadWholeMember(narc, memberIndex, (void *)dest);

    switch (memberType) {
    case GRAPHICSMEMBER_TILES:
        NNS_G2dGetUnpackedCharacterData(dest, &ppCharData);

        if (memberSize == 0) {
            memberSize = ppCharData->szByte;
        }

        Bg_LoadTiles(bgConfig, bgLayer, ppCharData->pRawData, memberSize, offset);
        break;
    case GRAPHICSMEMBER_TILEMAP:
        NNS_G2dGetUnpackedScreenData(dest, &ppScrData);

        if (memberSize == 0) {
            memberSize = ppScrData->szByte;
        }

        if (Bg_GetTilemapBuffer(bgConfig, bgLayer) != NULL) {
            Bg_LoadTilemapBuffer(bgConfig, bgLayer, ppScrData->rawData, memberSize);
        }

        Bg_CopyTilemapBufferRangeToVRAM(bgConfig, bgLayer, ppScrData->rawData, memberSize, offset);
        break;
    case GRAPHICSMEMBER_PALETTE:
        NNS_G2dGetUnpackedPaletteData(dest, &ppPltData);

        if (memberSize == 0) {
            memberSize = ppPltData->szByte;
        }

        Bg_LoadPalette(bgLayer, ppPltData->pRawData, memberSize, offset);
    }

    Heap_Free(dest);
}

void *App_LoadScreenData(NARC *narc, enum NarcID unused, int memberIdx, NNSG2dScreenData **dst, enum HeapID heapID)
{
    int size = NARC_GetMemberSize(narc, memberIdx);
    void *file = Heap_Alloc(heapID, size);

    NARC_ReadWholeMember(narc, memberIdx, file);
    NNS_G2dGetUnpackedScreenData(file, dst);

    return file;
}
