#ifndef POKEPLATINUM_STRUCT_020951B0_H
#define POKEPLATINUM_STRUCT_020951B0_H

#include "struct_defs/struct_020951B0_sub1.h"

// A preset contest photo used by NPC contestants. The game picks one of these
// (via the contest's npcPhotoPreset) and applies it to the NPC's photo.
typedef struct {
    ContestPhotoAccessory accessories[20];
    u8 accessoryCount;
    s8 monPriority; // draw priority of the Pokemon in the photo
    s8 backdrop;
    u8 padding_53;
} ContestPhotoPreset;

#endif // POKEPLATINUM_STRUCT_020951B0_H
