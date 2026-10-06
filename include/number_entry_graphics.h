#ifndef POKEPLATINUM_NUMBER_ENTRY_GRAPHICS_H
#define POKEPLATINUM_NUMBER_ENTRY_GRAPHICS_H

#include "struct_defs/struct_02089688.h"

#include "bg_window.h"

void NumberEntryGraphics_InitSpriteSystem(NumberEntryScreen *param0);
void NumberEntryGraphics_LoadResources(NumberEntryScreen *param0);
void NumberEntryGraphics_Free(NumberEntryScreen *param0);
void NumberEntryGraphics_CreateDigitSprites(NumberEntryScreen *param0);
void NumberEntryGraphics_CreateControlSprites(NumberEntryScreen *param0);
void NumberEntryGraphics_CreateButtonEffectSprites(NumberEntryScreen *param0);
void NumberEntryGraphics_SetControlVisible(NumberEntryScreen *param0, int param1, BOOL param2);
void NumberEntryGraphics_MoveCursorToDigit(NumberEntryScreen *param0, int param1);
void NumberEntryGraphics_MoveKeyCursor(NumberEntryScreen *param0, int param1);
void NumberEntryGraphics_PositionControlAtKey(NumberEntryScreen *param0, int param1, int param2);
void NumberEntryGraphics_UpdateControls(NumberEntryScreen *param0);
void NumberEntryGraphics_UpdateButtonEffects(NumberEntryScreen *param0);
int NumberEntryGraphics_GetDigitAnim(int param0, BOOL param1);
void NumberEntryGraphics_UpdateDigitSelection(NumberEntryScreen *param0);
void NumberEntryGraphics_LayoutDigits(NumberEntryScreen *param0, int param1);
void NumberEntryGraphics_UpdateDigitTouchRects(NumberEntryScreen *param0);
void NumberEntryGraphics_InitFont(NumberEntryScreen *param0);
void NumberEntryGraphics_FreeFont(NumberEntryScreen *param0);
void NumberEntryGraphics_LoadButtonLabelPalette(NumberEntryScreen *param0);
void NumberEntryGraphics_CreateButtonLabels(NumberEntryScreen *param0);
void NumberEntryGraphics_CreateButtonLabel(NumberEntryScreen *param0, int param1, int param2, int param3, int param4);
void NumberEntryGraphics_InitMessageWindow(BgConfig *param0, Window *param1, int param2, int param3, int param4, int param5, int param6, int param7, int param8);
void NumberEntryGraphics_DrawMessage(Window *param0, int param1);

#endif // POKEPLATINUM_NUMBER_ENTRY_GRAPHICS_H
