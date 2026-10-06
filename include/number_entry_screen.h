#ifndef POKEPLATINUM_NUMBER_ENTRY_SCREEN_H
#define POKEPLATINUM_NUMBER_ENTRY_SCREEN_H

#include "struct_defs/struct_02089688.h"

#include "touch_screen.h"

// Number-entry screen: a grouped row of digit slots with BACK/OK buttons,
// used to enter Friend Codes, Wii numbers, registration codes, etc.

void NumberEntry_SetSelectedGroup(NumberEntryScreen *param0, int param1);
void NumberEntry_InitLayout(NumberEntryScreen *param0);
void NumberEntry_SetPhase(NumberEntryScreen *param0, int param1);
BOOL NumberEntry_Setup(NumberEntryScreen *param0);
BOOL NumberEntry_FadeOut(NumberEntryScreen *param0);
BOOL NumberEntry_UpdateInput(NumberEntryScreen *param0);
BOOL NumberEntry_AnimateSelection(NumberEntryScreen *param0);
BOOL NumberEntry_Update(NumberEntryScreen *param0);
void NumberEntry_ProcessInput(NumberEntryScreen *param0);
void NumberEntry_Confirm(NumberEntryScreen *param0);
void NumberEntry_Cancel(NumberEntryScreen *param0);
void NumberEntry_InitTouchScreen(NumberEntryScreen *param0);
void NumberEntry_TouchCallback(u32 param0, enum TouchScreenButtonState param1, void *param2);
void NumberEntry_ProcessSelectionAction(NumberEntryScreen *param0);
void NumberEntry_ClearSelectionAction(NumberEntryScreen *param0);
int NumberEntry_GetFirstDigitInGroup(NumberEntryScreen *param0, int param1);
int NumberEntry_GetLastDigitInGroup(NumberEntryScreen *param0, int param1);

#endif // POKEPLATINUM_NUMBER_ENTRY_SCREEN_H
