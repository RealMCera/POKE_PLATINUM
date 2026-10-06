#ifndef POKEPLATINUM_STRUCT_IMAGE_CLIPS_H
#define POKEPLATINUM_STRUCT_IMAGE_CLIPS_H

#include "struct_defs/dress_up_photo.h"
#include "struct_defs/fashion_case.h"
#include "struct_defs/struct_02029C88.h"

#define SAVED_PHOTOS_COUNT 11

// The player's saved Image Clips: dress-up photos, one contest photo per
// contest type, and the fashion case holding collected accessories/backdrops.
typedef struct ImageClips {
    DressUpPhoto savedPhotos[SAVED_PHOTOS_COUNT];
    ContestPhoto contestPhotos[5]; // indexed by contest type
    FashionCase fashionCase;
} ImageClips;

#endif // POKEPLATINUM_STRUCT_IMAGE_CLIPS_H
