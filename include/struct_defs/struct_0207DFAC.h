#ifndef POKEPLATINUM_STRUCT_0207DFAC_H
#define POKEPLATINUM_STRUCT_0207DFAC_H

#include "struct_defs/struct_0207E060.h"

// Buffer of WFC trainer profiles: the local player's own profile followed by
// the profiles received from each friend. CommManager_GetUnk00 returns this
// buffer, and it is also the status data registered with the WFC server.
typedef struct WFCStatusBuffer {
    WFCTrainerInfo ownStatus;
    WFCTrainerInfo friendStatuses[32];
} WFCStatusBuffer;

#endif // POKEPLATINUM_STRUCT_0207DFAC_H
