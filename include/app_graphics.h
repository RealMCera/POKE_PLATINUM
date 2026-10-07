#ifndef POKEPLATINUM_APP_GRAPHICS_H
#define POKEPLATINUM_APP_GRAPHICS_H

#include <nnsys.h>

#include "constants/heap.h"

#include "bg_window.h"
#include "narc.h"

enum BarColor {
    BARCOLOR_EMPTY = 0,
    BARCOLOR_RED,
    BARCOLOR_YELLOW,
    BARCOLOR_GREEN,

    BARCOLOR_MAX,
};

enum GraphicMemberType {
    GRAPHICSMEMBER_TILES = 0,
    GRAPHICSMEMBER_TILEMAP,
    GRAPHICSMEMBER_PALETTE,
};

/**
 * @brief Look up the size, in bytes, of a graphic resource.
 *
 * @param graphicIndex  Identifier of the graphic resource.
 * @return The size of the graphic resource, in bytes.
 */
int App_GetGraphicSize(int graphicIndex);

/**
 * @brief Compute the integer Euclidean distance between two points.
 *
 * @param dx    The horizontal distance between the points.
 * @param dy    The vertical distance between the points.
 * @return The integer distance sqrt(dx^2 + dy^2).
 */
u32 App_Distance(u32 dx, u32 dy);

/**
 * @brief Determine how many pixels are needed to represent a fractional value.
 *
 * @param cur       The current value; the fraction's numerator.
 * @param max       The maximum value; the fraction's denominator.
 * @param maxPixels How many pixels would be used to display max / max.
 * @return The number of pixels needed to display cur / max.
 */
u8 App_PixelCount(u32 cur, u32 max, u8 maxPixels);

/**
 * @brief Determine what color should be used for a value represented by a
 * visual bar, e.g. the health bar in battle.
 *
 * @param cur   The current value of the bar.
 * @param max   The maximum value of the bar.
 * @return The color to be used for the bar's current value.
 */
u8 App_BarColor(u32 cur, u32 max);

/**
 * @brief Determine the color of the health bar.
 *
 * @param curHP     The current HP value.
 * @param maxHP     The maximum HP value.
 * @param barSize   The size of the health bar, in pixels.
 * @return The color to be used for the health bar's current value.
 */
u8 HealthBar_Color(u16 curHP, u16 maxHP, u32 barSize);
void App_StartScreenFade(u8 fadeOut, enum HeapID heapID);

/**
 * @brief Adjust a value with the D-pad, wrapping around at the bounds.
 *
 * @param value Pointer to the value to adjust in place.
 * @param max   The maximum value; the minimum is 1.
 * @return 0 if the value did not change, 1 if it increased, 2 if it decreased.
 */
u8 App_AdjustValueWithDPad(s16 *value, u16 max);
void App_LoadGraphicMember(BgConfig *bgConfig, enum HeapID heapID, NARC *narc, int unused, int memberIndex, int bgLayer, enum GraphicMemberType memberType, u16 memberSize, u16 offset);
void *App_LoadScreenData(NARC *narc, enum NarcID unused, int memberIdx, NNSG2dScreenData **dst, enum HeapID heapID);

#endif // POKEPLATINUM_APP_GRAPHICS_H
