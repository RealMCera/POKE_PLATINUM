#ifndef POKEPLATINUM_STRUCT_02029C88_DECL_H
#define POKEPLATINUM_STRUCT_02029C88_DECL_H

#include "struct_defs/photo_accessory.h"
#include "struct_defs/photo_pokemon.h"

// A photo taken during a Pokemon Contest, saved per contest type in
// ImageClips. Unlike DressUpPhoto it has no title or language; the contest
// rank and backdrop are stored instead.
typedef struct ContestPhoto {
    u32 integrity; // PHOTO_EMPTY_MAGIC or PHOTO_FULL_MAGIC
    u32 contestRank;
    PhotoPokemon photoMon;
    u32 accessoryFlags; // bit i is set when accessories[i] holds an accessory
    PhotoAccessory accessories[20];
    u8 backdrop;
} ContestPhoto;

#endif // POKEPLATINUM_STRUCT_02029C88_DECL_H
