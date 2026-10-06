#ifndef POKEPLATINUM_FONT_OAM_H
#define POKEPLATINUM_FONT_OAM_H

#include <nitro/gx.h>

#include "struct_decls/font_oam.h"
#include "struct_decls/struct_02012744_decl.h"
#include "struct_decls/struct_02012B20_decl.h"
#include "struct_defs/struct_020127E8.h"

#include "bg_window.h"
#include "sprite.h"

// Manager: owns the shared cell banks and the pool of FontOAM instances.
FontOAMManager *FontOAMManager_New(int param0, enum HeapID heapID);
void FontOAMManager_Free(FontOAMManager *param0);

// FontOAM lifecycle.
FontOAM *FontOAM_New(const UnkStruct_020127E8 *param0);
FontOAM *FontOAM_NewFromWindow(const UnkStruct_020127E8 *param0, const FontOAMWindow *param1);
void FontOAM_Free(FontOAM *param0);
void FontOAM_Delete(FontOAM *param0);

// Precomputed window layout, reusable across FontOAMs.
FontOAMWindow *FontOAMWindow_New(const Window *param0, enum HeapID heapID);
void FontOAMWindow_Free(FontOAMWindow *param0);
int FontOAMWindow_GetSize(const FontOAMWindow *param0, int param1);
void FontOAMWindow_UploadToVRAM(FontOAM *param0, const FontOAMWindow *param1, const Window *param2, enum HeapID heapID);

// VRAM size needed to render a window.
int FontOAM_GetWindowSize(const Window *param0, int param1, enum HeapID heapID);

// Position and parent sprite.
void FontOAM_SetXY(FontOAM *fontOAM, int x, int y);
void FontOAM_UpdatePosition(FontOAM *param0);
void FontOAM_GetXY(const FontOAM *fontOAM, int *x, int *y);
void FontOAM_SetParentSprite(FontOAM *param0, const Sprite *param1);

// Per-sprite OAM attributes, applied to every sprite in the FontOAM.
void FontOAM_SetDrawFlag(FontOAM *param0, BOOL param1);
void FontOAM_SetExplicitPriority(FontOAM *param0, u8 param1);
void FontOAM_SetPriority(FontOAM *param0, u32 param1);
void FontOAM_SetExplicitPalette(FontOAM *param0, u32 param1);
void FontOAM_SetExplicitPaletteOffset(FontOAM *param0, u32 param1);
void FontOAM_SetExplicitPaletteOffsetAutoAdjust(FontOAM *param0, u32 param1);
void FontOAM_SetExplicitOAMMode(FontOAM *param0, GXOamMode param1);

// Copies a rectangle of pixels out of a window.
void FontOAM_CopyWindowRect(const Window *window, int width, int height, int x, int y, char *output);

#endif // POKEPLATINUM_FONT_OAM_H
