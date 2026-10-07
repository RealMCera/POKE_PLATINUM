#ifndef POKEPLATINUM_STRUCT_0207C894_H
#define POKEPLATINUM_STRUCT_0207C894_H

#include "constants/heap.h"

// Construction parameters for the evolution scene's particle system.
typedef struct {
    enum HeapID heapID;
    // Member index into the Shinka (evolution) demo particle NARC.
    int narcIdx;
} EvolutionParticleSystemArgs;

#endif // POKEPLATINUM_STRUCT_0207C894_H
