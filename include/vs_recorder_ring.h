#ifndef POKEPLATINUM_VS_RECORDER_RING_H
#define POKEPLATINUM_VS_RECORDER_RING_H

#include "struct_defs/struct_0208B284.h"
#include "struct_defs/struct_0208B878.h"

#include "palette.h"
#include "sprite_system.h"

// A ring of sprites that fly in and orbit a common center, used by the
// Vs. Recorder viewer (overlay062). The ring is driven by a per-VBlank task
// started by VsRecorderRing_Init and stopped by VsRecorderRing_Shutdown.
VsRecorderRing *VsRecorderRing_New(VsRecorderRingConfig config, SpriteSystem *spriteSystem, SpriteManager *spriteManager, PaletteData *paletteData);
void VsRecorderRing_LoadColorPalette(VsRecorderRing *ring, int color);
BOOL VsRecorderRing_Init(VsRecorderRing *ring, int color);
BOOL VsRecorderRing_SetActive(VsRecorderRing *ring, BOOL active);
BOOL VsRecorderRing_SetTargetPosition(VsRecorderRing *ring, s16 x, s16 y);
BOOL VsRecorderRing_TrySetTargetPosition(VsRecorderRing *ring, s16 x, s16 y);
BOOL VsRecorderRing_SetPosition(VsRecorderRing *ring, s16 x, s16 y);
BOOL VsRecorderRing_Shutdown(VsRecorderRing *ring);
BOOL VsRecorderRing_SetVisible(VsRecorderRing *ring, BOOL visible);
void VsRecorderRing_SetOrbitRadii(VsRecorderRing *ring, int radiusX, int radiusY);
void VsRecorderRing_StartAnim(VsRecorderRing *ring);
void VsRecorderRing_StopAnim(VsRecorderRing *ring);

#endif // POKEPLATINUM_VS_RECORDER_RING_H
