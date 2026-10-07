#include "vs_recorder_ring.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_0208B284.h"
#include "struct_defs/struct_0208B878.h"

#include "heap.h"
#include "math_util.h"
#include "narc.h"
#include "palette.h"
#include "sprite_system.h"
#include "sys_task.h"
#include "sys_task_manager.h"

static BOOL VsRecorderRing_CalcApproachOffset(s16 targetX, s16 targetY, f32 currentX, f32 currentY, f32 *outOffsetX, f32 *outOffsetY, f32 step, s16 minDistance);
static void VsRecorderRing_UpdateSprites(SysTask *task, void *param);
VsRecorderRing *VsRecorderRing_New(VsRecorderRingConfig config, SpriteSystem *spriteSystem, SpriteManager *spriteManager, PaletteData *paletteData);
void VsRecorderRing_LoadGraphics(VsRecorderRing *ring, int color);
void VsRecorderRing_CreateSprites(VsRecorderRing *ring);
void VsRecorderRing_DeleteSprites(VsRecorderRing *ring);
BOOL VsRecorderRing_Init(VsRecorderRing *ring, int color);
BOOL VsRecorderRing_SetActive(VsRecorderRing *ring, BOOL active);
BOOL VsRecorderRing_SetTargetPosition(VsRecorderRing *ring, s16 x, s16 y);
BOOL VsRecorderRing_SetPosition(VsRecorderRing *ring, s16 x, s16 y);
BOOL VsRecorderRing_Shutdown(VsRecorderRing *ring);
BOOL VsRecorderRing_SetVisible(VsRecorderRing *ring, BOOL visible);
static BOOL VsRecorderRing_HasTargetPositionChanged(VsRecorderRing *ring, s16 x, s16 y);

// Palette file index in batt_rec_gra.narc for each Vs. Recorder color.
static const int sVsRecorderColorPalettes[] = {
    97,
    98,
    99,
    100,
    101,
    128,
    133,
};

VsRecorderRing *VsRecorderRing_New(VsRecorderRingConfig config, SpriteSystem *spriteSystem, SpriteManager *spriteManager, PaletteData *paletteData)
{
    VsRecorderRing *ring = Heap_Alloc(config.heapID, sizeof(VsRecorderRing));
    ring->config.count = config.count;
    ring->config.heapID = (enum HeapID)config.heapID; // Needed for matching.
    ring->config.vramType = config.vramType;
    ring->config.mode = config.mode;
    ring->config.subscreenOffset = config.subscreenOffset;
    ring->resources.spriteSystem = spriteSystem;
    ring->resources.spriteManager = spriteManager;
    ring->resources.paletteData = paletteData;
    ring->shutdownState = 0;
    ring->orbitRadiusX = (15 * 1);
    ring->orbitRadiusY = (15 * 1);
    ring->updateTask = NULL;
    ring->active = 0;

    return ring;
}

// Computes the offset that moves a point at (currentX, currentY) `step` pixels
// toward (targetX, targetY). Returns FALSE (leaving the outputs untouched) when
// the point is already within `step` or `minDistance` of the target, or when
// the two points coincide.
static BOOL VsRecorderRing_CalcApproachOffset(s16 targetX, s16 targetY, f32 currentX, f32 currentY, f32 *outOffsetX, f32 *outOffsetY, f32 step, s16 minDistance)
{
    f32 distance;
    f32 distanceSq;
    Vec2F32 offset;
    Vec2F32 delta;
    Vec2F32 baseOffset;

    baseOffset.x = 0;
    baseOffset.y = 0;

    delta.x = (targetX - currentX);
    delta.y = (targetY - currentY);

    offset.x = 0;
    offset.y = 0;

    distanceSq = (delta.x * delta.x) + (delta.y * delta.y);
    distance = FX_Sqrt(FX_F32_TO_FX32(distanceSq));
    distance = FX_FX32_TO_F32(distance);

    if ((distance < step) || (minDistance > distance) || (distance == 0)) {
        return 0;
    }

    offset.x = (delta.x * step) / distance;
    offset.y = (delta.y * step) / distance;

    *outOffsetX = (offset.x + baseOffset.x);
    *outOffsetY = (offset.y + baseOffset.y);

    return 1;
}

// Per-VBlank task. Each sprite chases the target position of the sprite ahead
// of it, so the ring trails in a chain. Once a sprite gets close enough to its
// target it switches to state 1 and is placed on a shared orbit around sprite
// 0's target, spaced evenly by angle.
static void VsRecorderRing_UpdateSprites(SysTask *task, void *param)
{
    VsRecorderRing *ring = param;

    if (ring->active == 0) {
        return;
    }

    {
        int i;
        int orbitingCount = 0;
        f32 currentX;
        f32 currentY;
        fx32 posFxX;
        fx32 posFxY;
        f32 offsetX;
        f32 offsetY;
        BOOL canApproach;

        for (i = 0; i < ring->config.count; i++) {
            // Each sprite inherits the target of the sprite ahead of it.
            if (i != 0) {
                ring->sprites[i].targetX = ring->sprites[i - 1].targetX;
                ring->sprites[i].targetY = ring->sprites[i - 1].targetY;
            }

            ManagedSprite_GetPositionFxXYWithSubscreenOffset(ring->sprites[i].sprite, &posFxX, &posFxY, ring->config.subscreenOffset);

            currentX = FX_FX32_TO_F32(posFxX);
            currentY = FX_FX32_TO_F32(posFxY);

            canApproach = VsRecorderRing_CalcApproachOffset(ring->sprites[i].targetX, ring->sprites[i].targetY, currentX, currentY, &offsetX, &offsetY, (8.0f - ((i - orbitingCount) * 0.5f)) / 2, 16);

            if (canApproach && (ring->sprites[i].state == 0)) {
                ManagedSprite_OffsetPositionFxXY(ring->sprites[i].sprite, FX_F32_TO_FX32(offsetX), FX_F32_TO_FX32(offsetY));
            } else {
                {
                    int unused;
                    int angle;
                    s16 spriteX, spriteY;
                    fx32 orbitX, orbitY;

                    switch (ring->sprites[i].state) {
                    case 0:
                        Sprite_GetPositionXYWithSubscreenOffset2(ring->sprites[i].sprite, &spriteX, &spriteY, ring->config.subscreenOffset);

                        // The first sprite to arrive seeds the orbit angle from
                        // its current position; the rest are spaced evenly
                        // behind it.
                        if (ring->firstOrbitIndex == 0xFF) {
                            ring->firstOrbitIndex = i;
                            angle = FX_Atan2Idx(FX_F32_TO_FX32(ring->sprites[0].targetY - spriteY), FX_F32_TO_FX32(ring->sprites[0].targetX - spriteX));
                            ring->sprites[i].angle = angle;
                        } else {
                            ring->sprites[i].angle = ring->sprites[ring->firstOrbitIndex].angle - ((720 / ring->config.count) * ring->orbitCounter);
                        }

                        ring->sprites[i].angle %= 720;
                        ring->orbitCounter++;
                        ring->sprites[i].state++;
                        break;
                    case 1:
                        ring->sprites[i].angle += (8 / 2);
                        ring->sprites[i].angle %= 720;
                        orbitX = (ring->sprites[0].targetX << FX32_SHIFT) + (CalcSineDegrees_Wraparound(ring->sprites[i].angle) * ring->orbitRadiusX);
                        orbitY = (ring->sprites[0].targetY << FX32_SHIFT) + (CalcCosineDegrees_Wraparound(ring->sprites[i].angle) * ring->orbitRadiusY);
                        ManagedSprite_SetPositionFxXYWithSubscreenOffset(ring->sprites[i].sprite, orbitX, orbitY, ring->config.subscreenOffset);
                        break;
                    default:
                        break;
                    }
                }
                orbitingCount++;
            }
        }
    }
}

// Loads the color palette for both screens. `color` indexes
// sVsRecorderColorPalettes.
void VsRecorderRing_LoadColorPalette(VsRecorderRing *ring, int color)
{
    NARC *narc;
    SpriteSystem *spriteSystem = ring->resources.spriteSystem;
    SpriteManager *spriteManager = ring->resources.spriteManager;
    PaletteData *paletteData = ring->resources.paletteData;
    narc = ring->resources.narc;

    SpriteManager_UnloadPlttObjById(spriteManager, 22222 + 1);
    SpriteManager_UnloadPlttObjById(spriteManager, 22222 + 2);
    SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, 2, spriteSystem, spriteManager, narc, sVsRecorderColorPalettes[color], 0, 1, NNS_G2D_VRAM_TYPE_2DMAIN, 22222 + 1);
    SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, 3, spriteSystem, spriteManager, narc, sVsRecorderColorPalettes[color], 0, 1, NNS_G2D_VRAM_TYPE_2DSUB, 22222 + 2);
}

// Loads the palette, character, cell and animation resources for the ring's
// sprites. `color` indexes sVsRecorderColorPalettes; when the ring's mode is
// non-zero a fixed palette (file 96) is used instead.
void VsRecorderRing_LoadGraphics(VsRecorderRing *ring, int color)
{
    NARC *narc;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    PaletteData *paletteData;
    int resourceID = 22222 + ring->config.vramType;

    spriteSystem = ring->resources.spriteSystem;
    spriteManager = ring->resources.spriteManager;
    paletteData = ring->resources.paletteData;
    narc = ring->resources.narc;

    if (ring->config.vramType == 1) {
        if (ring->config.mode == 0) {
            SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, 2, spriteSystem, spriteManager, narc, sVsRecorderColorPalettes[color], 0, 1, NNS_G2D_VRAM_TYPE_2DMAIN, resourceID);
        } else {
            SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, 2, spriteSystem, spriteManager, narc, 96, 0, 1, NNS_G2D_VRAM_TYPE_2DMAIN, resourceID);
        }

        SpriteSystem_LoadCharResObjFromOpenNarc(spriteSystem, spriteManager, narc, 95, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, resourceID);
    } else {
        if (ring->config.mode == 0) {
            SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, PLTTBUF_SUB_OBJ, spriteSystem, spriteManager, narc, sVsRecorderColorPalettes[color], FALSE, 1, NNS_G2D_VRAM_TYPE_2DSUB, resourceID);
        } else {
            SpriteSystem_LoadPaletteBufferFromOpenNarc(paletteData, PLTTBUF_SUB_OBJ, spriteSystem, spriteManager, narc, 96, FALSE, 1, NNS_G2D_VRAM_TYPE_2DSUB, resourceID);
        }

        SpriteSystem_LoadCharResObjFromOpenNarc(spriteSystem, spriteManager, narc, 95, FALSE, NNS_G2D_VRAM_TYPE_2DSUB, resourceID);
    }

    SpriteSystem_LoadCellResObjFromOpenNarc(spriteSystem, spriteManager, narc, 93, FALSE, resourceID);
    SpriteSystem_LoadAnimResObjFromOpenNarc(spriteSystem, spriteManager, narc, 94, FALSE, resourceID);
}

// Creates the ring's sprites, all sharing the same graphics, and parks them at
// the center of the screen until the update task moves them.
void VsRecorderRing_CreateSprites(VsRecorderRing *ring)
{
    int i;
    SpriteTemplate template;
    SpriteSystem *spriteSystem = ring->resources.spriteSystem;
    SpriteManager *spriteManager = ring->resources.spriteManager;
    PaletteData *paletteData = ring->resources.paletteData;

    template.x = 128;
    template.y = 96;
    template.z = 0;
    template.animIdx = 0;
    template.priority = 0;
    template.vramType = ring->config.vramType;
    template.bgPriority = 0;
    template.vramTransfer = FALSE;
    template.plttIdx = 0;
    template.resources[0] = 22222 + ring->config.vramType;
    template.resources[1] = 22222 + ring->config.vramType;
    template.resources[2] = 22222 + ring->config.vramType;
    template.resources[3] = 22222 + ring->config.vramType;
    template.resources[4] = SPRITE_RESOURCE_NONE;
    template.resources[5] = SPRITE_RESOURCE_NONE;

    for (i = 0; i < ring->config.count; i++) {
        ring->sprites[i].sprite = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);

        ManagedSprite_TickFrame(ring->sprites[i].sprite);
        ManagedSprite_SetPositionXY(ring->sprites[i].sprite, 256 / 2, 192 / 2);
    }
}

// Unloads the shared graphics and deletes every sprite in the ring.
void VsRecorderRing_DeleteSprites(VsRecorderRing *ring)
{
    int i;

    for (i = 0; i < ring->config.count; i++) {
        SpriteManager_UnloadCharObjById(ring->resources.spriteManager, 22222 + ring->config.vramType);
        SpriteManager_UnloadCellObjById(ring->resources.spriteManager, 22222 + ring->config.vramType);
        SpriteManager_UnloadAnimObjById(ring->resources.spriteManager, 22222 + ring->config.vramType);
        Sprite_DeleteAndFreeResources(ring->sprites[i].sprite);
    }
}

// Opens the graphics NARC, loads the resources, creates the sprites and starts
// the per-VBlank update task. `color` indexes sVsRecorderColorPalettes.
BOOL VsRecorderRing_Init(VsRecorderRing *ring, int color)
{
    ring->resources.narc = NARC_ctor(NARC_INDEX_RESOURCE__ENG__BATT_REC__BATT_REC_GRA, ring->config.heapID);

    VsRecorderRing_LoadGraphics(ring, color);
    VsRecorderRing_CreateSprites(ring);

    ring->updateTask = SysTask_ExecuteOnVBlank(VsRecorderRing_UpdateSprites, ring, 0x1000);

    return 1;
}

// Enables or disables the per-VBlank update task.
BOOL VsRecorderRing_SetActive(VsRecorderRing *ring, BOOL active)
{
    ring->active = active;
    return 1;
}

// Sets the target position of the ring's center and restarts the fly-in.
BOOL VsRecorderRing_SetTargetPosition(VsRecorderRing *ring, s16 x, s16 y)
{
    ring->sprites[0].targetX = x;
    ring->sprites[0].targetY = y;
    ring->firstOrbitIndex = 0xFF;
    ring->orbitCounter = 0;

    {
        int i;

        for (i = 0; i < ring->config.count; i++) {
            ring->sprites[i].state = 0;
        }
    }

    return 1;
}

// Like VsRecorderRing_SetTargetPosition, but does nothing (returning FALSE) if
// the center is already at (x, y).
BOOL VsRecorderRing_TrySetTargetPosition(VsRecorderRing *ring, s16 x, s16 y)
{
    if (VsRecorderRing_HasTargetPositionChanged(ring, x, y) == 0) {
        return 0;
    }

    ring->sprites[0].targetX = x;
    ring->sprites[0].targetY = y;
    ring->firstOrbitIndex = 0xFF;
    ring->orbitCounter = 0;

    {
        int i;

        for (i = 0; i < ring->config.count; i++) {
            ring->sprites[i].state = 0;
        }
    }

    return 1;
}

static BOOL VsRecorderRing_HasTargetPositionChanged(VsRecorderRing *ring, s16 x, s16 y)
{
    if ((ring->sprites[0].targetX == x) && (ring->sprites[0].targetY == y)) {
        return 0;
    }

    return 1;
}

// Moves the ring's center and every sprite to (x, y) immediately, skipping the
// fly-in animation.
BOOL VsRecorderRing_SetPosition(VsRecorderRing *ring, s16 x, s16 y)
{
    {
        int i;

        ring->firstOrbitIndex = 0xFF;
        ring->orbitCounter = 0;

        for (i = 0; i < ring->config.count; i++) {
            ring->sprites[i].targetX = x;
            ring->sprites[i].targetY = y;
            ManagedSprite_SetPositionXYWithSubscreenOffset(ring->sprites[i].sprite, x, y, ring->config.subscreenOffset);
        }
    }

    return 1;
}

// Tears the ring down one step per call: deactivate, stop the update task, then
// free the sprites and the ring itself. Returns FALSE once the ring is freed.
BOOL VsRecorderRing_Shutdown(VsRecorderRing *ring)
{
    switch (ring->shutdownState) {
    case 0:
        VsRecorderRing_SetActive(ring, 0);
        ring->shutdownState++;
        break;
    case 1:
        SysTask_Done(ring->updateTask);
        ring->shutdownState++;
        break;
    default:
        VsRecorderRing_DeleteSprites(ring);
        NARC_dtor(ring->resources.narc);
        Heap_Free(ring);

        return 0;
    }

    return 1;
}

// Shows or hides every sprite in the ring.
BOOL VsRecorderRing_SetVisible(VsRecorderRing *ring, BOOL visible)
{
    {
        int i;

        for (i = 0; i < ring->config.count; i++) {
            ManagedSprite_SetDrawFlag(ring->sprites[i].sprite, visible);
        }
    }

    return 1;
}

// Sets the orbit radii in pixels. A zero radius on either axis falls back to
// the default of 15.
void VsRecorderRing_SetOrbitRadii(VsRecorderRing *ring, int radiusX, int radiusY)
{
    if ((radiusX != 0) && (radiusY != 0)) {
        ring->orbitRadiusX = radiusX;
        ring->orbitRadiusY = radiusY;
    } else {
        ring->orbitRadiusX = (15 * 1);
        ring->orbitRadiusY = (15 * 1);
    }
}

// Switches every sprite to animation 1 (the active/spinning pose).
void VsRecorderRing_StartAnim(VsRecorderRing *ring)
{
    int i;

    for (i = 0; i < ring->config.count; i++) {
        ManagedSprite_SetAnim(ring->sprites[i].sprite, 1);
    }
}

// Switches every sprite back to animation 0 (the idle pose).
void VsRecorderRing_StopAnim(VsRecorderRing *ring)
{
    int i;

    for (i = 0; i < ring->config.count; i++) {
        ManagedSprite_SetAnim(ring->sprites[i].sprite, 0);
    }
}
