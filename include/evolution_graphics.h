#ifndef POKEPLATINUM_EVOLUTION_GRAPHICS_H
#define POKEPLATINUM_EVOLUTION_GRAPHICS_H

#include "struct_defs/struct_0207C894.h"
#include "struct_defs/struct_0207C8C4.h"

#include "g3d_pipeline.h"

void EvolutionGraphics_InitPlane(void);
void EvolutionGraphics_SetBlendAlphas(void);
G3DPipelineBuffers *EvolutionGraphics_InitG3DPipeline(enum HeapID heapID);
void EvolutionGraphics_G3DPipelineCallback(void);
void EvolutionGraphics_ZeroParticleSystem(void);
void EvolutionGraphics_UpdateParticleSystem(void);
EvolutionParticleSystem *EvolutionParticleSystem_New(EvolutionParticleSystemArgs *args);
void EvolutionParticleSystem_CreateEmitter(EvolutionParticleSystem *particleSystem, int resourceID);
BOOL EvolutionParticleSystem_EmittersActive(EvolutionParticleSystem *particleSystem);
void EvolutionParticleSystem_Free(EvolutionParticleSystem *particleSystem);

#endif // POKEPLATINUM_EVOLUTION_GRAPHICS_H
