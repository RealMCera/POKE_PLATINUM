#ifndef POKEPLATINUM_STRUCT_0207C8C4_H
#define POKEPLATINUM_STRUCT_0207C8C4_H

#include "struct_defs/struct_0207C894.h"

#include "particle_system.h"
#include "spl.h"

// Wraps the particle system used by the evolution scene together with the
// parameters it was created from.
typedef struct {
    EvolutionParticleSystemArgs args;
    SPLEmitter *emitter;
    ParticleSystem *ps;
} EvolutionParticleSystem;

#endif // POKEPLATINUM_STRUCT_0207C8C4_H
