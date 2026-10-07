#include <nitro.h>
#include <nnsys.h>
#include <string.h>

#include "constants/narc.h"

#include "struct_defs/struct_0207C894.h"
#include "struct_defs/struct_0207C8C4.h"

#include "camera.h"
#include "g3d_pipeline.h"
#include "gx_layers.h"
#include "heap.h"
#include "particle_system.h"
#include "spl.h"
#include "unk_0202419C.h"

// Graphics and particle-system setup for the evolution cutscene. The scene
// draws a 3D background on engine A's BG0 layer and overlays SPL particle
// effects (the "shinka" evolution demo) on top of it. EvolutionParticleSystem
// wraps the particle system together with the emitter resources loaded from
// the Shinka demo particle NARC.

void EvolutionGraphics_InitPlane(void);
void EvolutionGraphics_SetBlendAlphas(void);
void EvolutionGraphics_G3DPipelineCallback(void);
void EvolutionGraphics_ZeroParticleSystem(void);
void EvolutionGraphics_UpdateParticleSystem(void);
EvolutionParticleSystem *EvolutionParticleSystem_New(EvolutionParticleSystemArgs *args);
void EvolutionParticleSystem_CreateEmitter(EvolutionParticleSystem *particleSystem, int resourceID);
BOOL EvolutionParticleSystem_EmittersActive(EvolutionParticleSystem *particleSystem);
void EvolutionParticleSystem_Free(EvolutionParticleSystem *particleSystem);
static u32 EvolutionGraphics_AllocTexVram(u32 size, BOOL is4x4Comp);
static u32 EvolutionGraphics_AllocPaletteVram(u32 size, BOOL is4Pltt);
static ParticleSystem *EvolutionGraphics_NewParticleSystem(enum HeapID heapID);
static ParticleSystem *EvolutionGraphics_CreateParticleSystem(enum HeapID heapID, int narcID, int narcMemberIdx);
static void EvolutionGraphics_FreeParticleSystem(ParticleSystem *ps);
static void EvolutionGraphics_SetEmitterPos(SPLEmitter *emitter);

void EvolutionGraphics_InitPlane(void)
{
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();
    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);
}

void EvolutionGraphics_SetBlendAlphas(void)
{
    // Blend BG1-BG3 against the backdrop on both engines; the differing
    // coefficients give the evolution background its translucent look.
    G2_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3, 11, 7);
    G2S_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3, 7, 8);
}

G3DPipelineBuffers *EvolutionGraphics_InitG3DPipeline(enum HeapID heapID)
{
    return G3DPipeline_Init(heapID, TEXTURE_VRAM_SIZE_256K, PALETTE_VRAM_SIZE_32K, EvolutionGraphics_G3DPipelineCallback);
}

// Called by the G3D pipeline once its buffers are ready; configures the 3D
// hardware for the evolution scene.
void EvolutionGraphics_G3DPipelineCallback(void)
{
    // BG0 is the 3D layer; give it priority 1 so it sits behind the 2D text.
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG0, 1);
    G2_SetBG0Priority(1);

    G3X_SetShading(GX_SHADING_TOON);
    G3X_AntiAlias(1);
    G3X_AlphaTest(0, 0);
    G3X_AlphaBlend(1);
    G3X_EdgeMarking(0);
    G3X_SetFog(0, GX_FOGBLEND_COLOR_ALPHA, GX_FOGSLOPE_0x8000, 0);

    G3X_SetClearColor(GX_RGB(0, 0, 0), 0, 0x7fff, 63, 0);
    G3_ViewPort(0, 0, 255, 191);
}

void EvolutionGraphics_ZeroParticleSystem(void)
{
    NNSGfdTexKey texKey;
    NNSGfdPlttKey plttKey;
    u32 texAddr, plttAddr;

    // Reserve 32 KiB of texture VRAM and 160 bytes of palette VRAM for the
    // particle system, then clear all particle state.
    texKey = NNS_GfdAllocTexVram(0x2000 * 4, 0, 0);
    plttKey = NNS_GfdAllocPlttVram(0x20 * (4 + 1), 0, 0);

    GF_ASSERT(texKey != NNS_GFD_ALLOC_ERROR_TEXKEY);
    GF_ASSERT(plttKey != NNS_GFD_ALLOC_ERROR_PLTTKEY);

    texAddr = NNS_GfdGetTexKeyAddr(texKey);
    plttAddr = NNS_GfdGetPlttKeyAddr(plttKey);

    ParticleSystem_ZeroAll();
}

void EvolutionGraphics_UpdateParticleSystem(void)
{
    int drawnCount;

    G3_ResetG3X();

    drawnCount = ParticleSystem_DrawAll();

    // Particles are drawn as software sprites, so the sprite camera must be
    // re-established whenever any particle was rendered.
    if (drawnCount > 0) {
        G3_ResetG3X();
        NNS_G2dSetupSoftwareSpriteCamera();
    }

    ParticleSystem_UpdateAll();
    G3_RequestSwapBuffers(GX_SORTMODE_MANUAL, GX_BUFFERMODE_Z);
}

// Allocates texture VRAM for the particle system and registers the key so the
// particle system can release it later. Returns the VRAM address.
static u32 EvolutionGraphics_AllocTexVram(u32 size, BOOL is4x4Comp)
{
    NNSGfdTexKey texKey;
    u32 addr;

    texKey = NNS_GfdAllocTexVram(size, is4x4Comp, 0);
    ParticleSystem_RegisterTextureKey(texKey);

    addr = NNS_GfdGetTexKeyAddr(texKey);
    return addr;
}

// Palette-VRAM counterpart of EvolutionGraphics_AllocTexVram.
static u32 EvolutionGraphics_AllocPaletteVram(u32 size, BOOL is4Pltt)
{
    NNSGfdPlttKey plttKey;
    u32 addr;

    plttKey = NNS_GfdAllocPlttVram(size, is4Pltt, 0);
    ParticleSystem_RegisterPaletteKey(plttKey);

    addr = NNS_GfdGetPlttKeyAddr(plttKey);
    return addr;
}

static ParticleSystem *EvolutionGraphics_NewParticleSystem(enum HeapID heapID)
{
    ParticleSystem *ps;
    void *heap;
    Camera *camera;

    heap = Heap_Alloc(heapID, 0x4800);
    ps = ParticleSystem_New(EvolutionGraphics_AllocTexVram, EvolutionGraphics_AllocPaletteVram, heap, 0x4800, 1, heapID);
    camera = ParticleSystem_GetCamera(ps);

    if (camera != NULL) {
        Camera_SetClipping(FX32_ONE, FX32_ONE * 900, camera);
    }

    return ps;
}

static ParticleSystem *EvolutionGraphics_CreateParticleSystem(enum HeapID heapID, int narcID, int narcMemberIdx)
{
    ParticleSystem *ps = EvolutionGraphics_NewParticleSystem(heapID);
    void *resource = ParticleSystem_LoadResourceFromNARC(narcID, narcMemberIdx, heapID);

    ParticleSystem_SetResource(ps, resource, VRAM_AUTO_RELEASE_TEXTURE_LNK | VRAM_AUTO_RELEASE_PALETTE_LNK, 1);

    return ps;
}

void EvolutionGraphics_FreeParticleSystem(ParticleSystem *ps)
{
    void *heap = ParticleSystem_GetHeapStart(ps);

    ParticleSystem_Free(ps);
    Heap_Free(heap);
}

// Emitter callback that pins the emitter to a fixed height above the origin.
static void EvolutionGraphics_SetEmitterPos(SPLEmitter *emitter)
{
    VecFx32 pos = { 0, 0, 0 };

    VEC_Set(&pos, 0, 8 * 172, 0);
    SPLEmitter_SetPos(emitter, &pos);
}

EvolutionParticleSystem *EvolutionParticleSystem_New(EvolutionParticleSystemArgs *args)
{
    EvolutionParticleSystem *particleSystem = Heap_Alloc(args->heapID, sizeof(EvolutionParticleSystem));

    GF_ASSERT(particleSystem != NULL);

    particleSystem->args = *args;
    particleSystem->ps = EvolutionGraphics_CreateParticleSystem(particleSystem->args.heapID, NARC_INDEX_DEMO__SHINKA__DATA__PARTICLE__SHINKA_DEMO_PARTICLE, particleSystem->args.narcIdx);

    ParticleSystem_SetCameraProjection(particleSystem->ps, CAMERA_PROJECTION_ORTHOGRAPHIC);

    return particleSystem;
}

void EvolutionParticleSystem_CreateEmitter(EvolutionParticleSystem *particleSystem, int resourceID)
{
    ParticleSystem_CreateEmitterWithCallback(particleSystem->ps, resourceID, EvolutionGraphics_SetEmitterPos, particleSystem);
    ParticleSystem_SetCameraProjection(particleSystem->ps, CAMERA_PROJECTION_ORTHOGRAPHIC);
}

BOOL EvolutionParticleSystem_EmittersActive(EvolutionParticleSystem *particleSystem)
{
    if (ParticleSystem_GetActiveEmitterCount(particleSystem->ps) == 0) {
        return 0;
    }

    return 1;
}

void EvolutionParticleSystem_Free(EvolutionParticleSystem *particleSystem)
{
    EvolutionGraphics_FreeParticleSystem(particleSystem->ps);
    Heap_Free(particleSystem);
}
