#ifndef POKEPLATINUM_VS_RECORDER_GRAPHICS_H
#define POKEPLATINUM_VS_RECORDER_GRAPHICS_H

#include "struct_decls/struct_020F3DCC_decl.h"
#include "struct_defs/struct_0208C06C.h"

const VsRecorderMenuEntry *VsRecorderGraphics_GetSubmenu(int menuIndex);
const VsRecorderMenuEntry *VsRecorderGraphics_GetMenuForMode(UnkStruct_0208C06C *state, int mode);
void VsRecorderGraphics_CountEnabledEntries(UnkStruct_0208C06C *state);

#endif // POKEPLATINUM_VS_RECORDER_GRAPHICS_H
