#ifndef POKEPLATINUM_STRUCT_DRESS_UP_PHOTO_H
#define POKEPLATINUM_STRUCT_DRESS_UP_PHOTO_H

#include "struct_defs/photo_accessory.h"
#include "struct_defs/photo_pokemon.h"

#include "easy_chat_sentence.h"

#define PHOTO_ACCESSORY_COUNT 10

// A dress-up photo taken in the photo studio: the Pokemon, its accessories,
// a title and the backdrop it was taken against.
typedef struct DressUpPhoto {
    u32 integrity; // PHOTO_EMPTY_MAGIC or PHOTO_FULL_MAGIC
    PhotoPokemon photoMon;
    u32 accessoryFlags; // bit i is set when accessories[i] holds an accessory
    EasyChatSentence title;
    PhotoAccessory accessories[PHOTO_ACCESSORY_COUNT];
    u8 backdrop;
    u8 language;
} DressUpPhoto;

#endif // POKEPLATINUM_STRUCT_DRESS_UP_PHOTO_H
