#include "image_clips.h"

#include <nitro.h>
#include <string.h>

#include "constants/accessories.h"
#include "constants/charcode.h"
#include "generated/backdrops.h"
#include "generated/pokemon_contest_types.h"

#include "struct_defs/image_clips.h"
#include "struct_defs/struct_020298D8.h"

#include "overlay022/ov22_02259098.h"
#include "overlay022/struct_ov22_02255040.h"
#include "overlay061/struct_ov61_0222AE80.h"
#include "overlay061/struct_ov61_0222AE80_sub2.h"

#include "easy_chat_sentence.h"
#include "heap.h"
#include "inlines.h"
#include "pokemon.h"
#include "savedata.h"
#include "software_sprite.h"
#include "string_gf.h"

// A photo's integrity field is one of these two magic values. It is set to
// PHOTO_EMPTY_MAGIC on allocation and bumped to PHOTO_FULL_MAGIC once the
// photo has been filled in, so a stale/zeroed slot can be detected.
#define PHOTO_EMPTY_MAGIC (0x1234) // Photo is initialized but without proper data.
#define PHOTO_FULL_MAGIC  (0x2345) // Photo has data written to it

static BOOL IsValidMagic(u32 value)
{
    if (value == PHOTO_EMPTY_MAGIC || value == PHOTO_FULL_MAGIC) {
        return TRUE;
    }

    return FALSE;
}

static inline BOOL DressUpPhoto_IsValid(const DressUpPhoto *photo)
{
    return IsValidMagic(photo->integrity);
}

static inline BOOL ContestPhoto_IsValid(const ContestPhoto *photo)
{
    return IsValidMagic(photo->integrity);
}

static inline void DressUpPhoto_InitInternal(DressUpPhoto *photo)
{
    memset(photo, 0, sizeof(DressUpPhoto));
    photo->integrity = PHOTO_EMPTY_MAGIC;
}

static inline void ContestPhoto_InitInternal(ContestPhoto *photo)
{
    memset(photo, 0, sizeof(ContestPhoto));
    photo->integrity = PHOTO_EMPTY_MAGIC;
}

// Reads the on-screen position and draw priority of a photo sprite.
static void PhotoSprite_GetPositionAndPriority(UnkStruct_020298D8 *sprite, u8 *xPos, u8 *yPos, s8 *priority)
{
    int spriteX, spriteY;
    int spritePriority;

    ov22_02259250(sprite, &spriteX, &spriteY);
    spritePriority = ov22_022591E0(sprite);

    GF_ASSERT(spriteX < 256);
    GF_ASSERT(spriteY < 256);
    GF_ASSERT(spritePriority > -128);

    *xPos = spriteX;
    *yPos = spriteY;
    *priority = spritePriority;
}

static void PhotoPokemon_SetDataFromMon(PhotoPokemon *photoMon, Pokemon *mon, u8 xPos, u8 yPos, s8 priority)
{
    photoMon->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    Pokemon_GetValue(mon, MON_DATA_NICKNAME, photoMon->nickname);

    photoMon->personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);
    photoMon->otID = Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL);
    photoMon->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);

    photoMon->xPos = xPos;
    photoMon->yPos = yPos;
    photoMon->priority = priority;
}

static void PhotoPokemon_SetDataFromMonAndSprite(PhotoPokemon *photoMon, Pokemon *mon, UnkStruct_020298D8 *sprite)
{
    u8 xPos, yPos;
    s8 priority;

    PhotoSprite_GetPositionAndPriority(sprite, &xPos, &yPos, &priority);
    PhotoPokemon_SetDataFromMon(photoMon, mon, xPos, yPos, priority);
}

static void PhotoPokemon_SetTrainerNameAndGender(PhotoPokemon *photoMon, const String *name, int gender)
{
    String_ToChars(name, photoMon->trainerName, TRAINER_NAME_LEN + 1);
    photoMon->trainerGender = gender;
}

static void PhotoPokemon_CopyToPokemonInternal(const PhotoPokemon *photoMon, Pokemon *mon)
{
    Pokemon_InitWith(mon, photoMon->species, 0, 0, TRUE, photoMon->personality, OTID_SET, photoMon->otID);
    Pokemon_SetValue(mon, MON_DATA_NICKNAME, photoMon->nickname);
    Pokemon_SetValue(mon, MON_DATA_FORM, &photoMon->form);
}

static void PhotoAccessory_SetData(PhotoAccessory *accessory, u8 accessoryID, u8 xPos, u8 yPos, u8 priority)
{
    accessory->accessoryID = accessoryID;
    accessory->xPos = xPos;
    accessory->yPos = yPos;
    accessory->priority = priority;
}

// Non-unique accessories are stored four bits per accessory, so eight fit in
// each u32 of the flag array.
static void NonUniqueAccessoryFlags_SetCount(u32 *flags, u8 count, u8 accessoryID)
{
    GF_ASSERT(accessoryID < NON_UNIQUE_ACCESSORY_COUNT);

    u8 wordIndex = accessoryID / 8;
    u8 shift = accessoryID % 8;

    shift *= 4;

    flags[wordIndex] &= ~(0xf << shift);
    flags[wordIndex] |= (count << shift);
}

static u8 NonUniqueAccessoryFlags_GetCount(const u32 *flags, u8 accessoryID)
{
    u8 count;
    u8 wordIndex;
    u8 shift;

    GF_ASSERT(accessoryID < NON_UNIQUE_ACCESSORY_COUNT);

    wordIndex = accessoryID / 8;
    shift = accessoryID % 8;
    shift *= 4;
    count = (flags[wordIndex] >> shift) & 0xf;

    if (count > MAX_NON_UNIQUE_ACCESSORIES_PER_TYPE) {
        count = MAX_NON_UNIQUE_ACCESSORIES_PER_TYPE;
    }

    return count;
}

// Unique accessories are stored one bit per accessory (owned or not).
static void UniqueAccessoryFlags_SetCount(u32 *flags, u8 count, u8 accessoryID)
{
    GF_ASSERT(count < MAX_UNIQUE_ACCESSORIES_PER_TYPE + 1);

    u8 wordIndex = accessoryID / 32;
    u8 shift = accessoryID % 32;

    shift *= 1;

    flags[wordIndex] &= ~(0x1 << shift);
    flags[wordIndex] |= (count << shift);
}

static u8 UniqueAccessoryFlags_GetCount(const u32 *flags, u8 accessoryID)
{
    u8 wordIndex;
    u8 shift;

    wordIndex = accessoryID / 32;
    shift = accessoryID % 32;

    shift *= 1;

    return (flags[wordIndex] >> shift) & 0x1;
}

// Backdrops are stored eight bits per backdrop, so four fit in each u32.
static void BackdropFlags_SetCount(u32 *flags, u8 count, u8 backdropID)
{
    u8 wordIndex;
    u8 shift;

    GF_ASSERT(count <= BACKDROP_COUNT);

    wordIndex = backdropID / 4;
    shift = backdropID % 4;

    shift *= 8;

    flags[wordIndex] &= ~(0xff << shift);
    flags[wordIndex] |= (count << shift);
}

static u8 BackdropFlags_GetCount(const u32 *flags, u8 backdropID)
{
    u8 wordIndex;
    u8 shift;

    wordIndex = backdropID / 4;
    shift = backdropID % 4;

    shift *= 8;

    return (flags[wordIndex] >> shift) & 0xff;
}

// A backdrop count of BACKDROP_COUNT means "not owned"; this counts the owned
// ones.
static u8 BackdropFlags_GetTotalCount(const u32 *flags)
{
    int i;
    int count = 0;

    for (i = 0; i < BACKDROP_COUNT; i++) {
        if (BackdropFlags_GetCount(flags, i) != BACKDROP_COUNT) {
            count++;
        }
    }

    return count;
}

// Accessories below NON_UNIQUE_ACCESSORY_COUNT can be owned in multiples;
// the rest are one-of-a-kind.
static BOOL Accessory_CanHaveMultiple(u32 accessoryID)
{
    if (accessoryID < NON_UNIQUE_ACCESSORY_COUNT) {
        return TRUE;
    }

    return FALSE;
}

static inline u8 Accessory_ToUniqueID(u32 accessoryID)
{
    GF_ASSERT(accessoryID >= NON_UNIQUE_ACCESSORY_COUNT);
    return accessoryID - NON_UNIQUE_ACCESSORY_COUNT;
}

static void FashionCase_Init(FashionCase *fashionCase)
{
    int i;

    memset(fashionCase, 0, sizeof(FashionCase));

    // A count of BACKDROP_COUNT marks a backdrop as not owned.
    for (i = 0; i < BACKDROP_COUNT; i++) {
        BackdropFlags_SetCount(fashionCase->backdropFlags, BACKDROP_COUNT, i);
    }
}

// Default position for a contest photo's Pokemon, derived from its sprite
// offset so that taller Pokemon sit higher.
static void GetMonXYPositions(Pokemon *mon, u8 *xPos, u8 *yPos)
{
    u8 yOffset = Pokemon_DPSpriteYOffset(mon, FACE_FRONT);

    *xPos = 192 - (8 * 8);
    *yPos = (16 + 129) - ((80 / 2) - yOffset) + -4;
    *yPos += (5 * 8);
}

void ImageClips_Init(ImageClips *imageClips)
{
    int i;

    for (i = 0; i < SAVED_PHOTOS_COUNT; i++) {
        DressUpPhoto_InitInternal(&imageClips->savedPhotos[i]);
    }

    for (i = 0; i < CONTEST_TYPE_MAX; i++) {
        ContestPhoto_InitInternal(&imageClips->contestPhotos[i]);
    }

    FashionCase_Init(&imageClips->fashionCase);
}

int ImageClips_SaveSize(void)
{
    return sizeof(ImageClips);
}

int DressUpPhoto_Size(void)
{
    return sizeof(DressUpPhoto);
}

int ContestPhoto_Size(void)
{
    return sizeof(ContestPhoto);
}

DressUpPhoto *DressUpPhoto_New(u32 heapID)
{
    DressUpPhoto *photo = Heap_Alloc(heapID, sizeof(DressUpPhoto));
    DressUpPhoto_InitInternal(photo);

    return photo;
}

ContestPhoto *ContestPhoto_New(u32 heapID)
{
    ContestPhoto *photo = Heap_Alloc(heapID, sizeof(ContestPhoto));
    ContestPhoto_InitInternal(photo);

    return photo;
}

DressUpPhoto *ImageClips_GetDressUpPhoto(ImageClips *imageClips, int slot)
{
    GF_ASSERT(slot < SAVED_PHOTOS_COUNT);
    GF_ASSERT(DressUpPhoto_IsValid(&imageClips->savedPhotos[slot]));

    return &imageClips->savedPhotos[slot];
}

ContestPhoto *ImageClips_GetContestPhoto(ImageClips *imageClips, int contestType)
{
    GF_ASSERT(contestType < CONTEST_TYPE_MAX);
    GF_ASSERT(ContestPhoto_IsValid(&imageClips->contestPhotos[contestType]));

    return &imageClips->contestPhotos[contestType];
}

FashionCase *ImageClips_GetFashionCase(ImageClips *imageClips)
{
    return &imageClips->fashionCase;
}

BOOL ImageClips_DressUpPhotoHasData(const ImageClips *imageClips, int slot)
{
    GF_ASSERT(slot < SAVED_PHOTOS_COUNT);
    return DressUpPhoto_HasData(&imageClips->savedPhotos[slot]);
}

BOOL ImageClips_ContestPhotoHasData(const ImageClips *imageClips, int contestType)
{
    GF_ASSERT(contestType < CONTEST_TYPE_MAX);
    return ContestPhoto_HasData(&imageClips->contestPhotos[contestType]);
}

BOOL FashionCase_CanFitAccessoryCount(const FashionCase *fashionCase, u32 accessoryID, u32 count)
{
    u32 currentCount;
    BOOL canFit = TRUE;

    currentCount = FashionCase_GetAccessoryCount(fashionCase, accessoryID);

    if (Accessory_CanHaveMultiple(accessoryID)) {
        currentCount += count;

        if (currentCount > MAX_NON_UNIQUE_ACCESSORIES_PER_TYPE) {
            canFit = FALSE;
        }
    } else {
        currentCount += count;

        if (currentCount > MAX_UNIQUE_ACCESSORIES_PER_TYPE) {
            canFit = FALSE;
        }
    }

    return canFit;
}

BOOL FashionCase_HasBackdrop(const FashionCase *fashionCase, u32 backdropID)
{
    u32 count = FashionCase_GetBackdropCount(fashionCase, backdropID);

    if (count != BACKDROP_COUNT) {
        return TRUE;
    }

    return FALSE;
}

u32 FashionCase_GetAccessoryCount(const FashionCase *fashionCase, u32 accessoryID)
{
    u32 count;

    GF_ASSERT(accessoryID < ACCESSORY_COUNT);

    if (Accessory_CanHaveMultiple(accessoryID)) {
        count = NonUniqueAccessoryFlags_GetCount(fashionCase->nonUniqueAccessoryFlags, accessoryID);
    } else {
        accessoryID = Accessory_ToUniqueID(accessoryID);
        count = UniqueAccessoryFlags_GetCount(fashionCase->uniqueAccessoryFlags, accessoryID);
    }

    return count;
}

u32 FashionCase_GetBackdropCount(const FashionCase *fashionCase, u32 backdropID)
{
    GF_ASSERT(backdropID < BACKDROP_COUNT);
    return BackdropFlags_GetCount(fashionCase->backdropFlags, backdropID);
}

u32 FashionCase_GetTotalAccessories(const FashionCase *fashionCase)
{
    int i;
    int count = 0;

    for (i = 0; i < ACCESSORY_COUNT; i++) {
        count += FashionCase_GetAccessoryCount(fashionCase, i);
    }

    return count;
}

u32 FashionCase_GetTotalBackdrops(const FashionCase *fashionCase)
{
    int i;
    int count = 0;

    for (i = 0; i < BACKDROP_COUNT; i++) {
        if (FashionCase_GetBackdropCount(fashionCase, i) != BACKDROP_COUNT) {
            count++;
        }
    }

    return count;
}

void FashionCase_AddAccessory(FashionCase *fashionCase, u32 accessoryID, u32 amount)
{
    u8 count;

    GF_ASSERT(accessoryID < ACCESSORY_COUNT);

    if (Accessory_CanHaveMultiple(accessoryID)) {
        count = NonUniqueAccessoryFlags_GetCount(fashionCase->nonUniqueAccessoryFlags, accessoryID);
        count += amount;

        if (count > MAX_NON_UNIQUE_ACCESSORIES_PER_TYPE) {
            count = MAX_NON_UNIQUE_ACCESSORIES_PER_TYPE;
        }

        NonUniqueAccessoryFlags_SetCount(fashionCase->nonUniqueAccessoryFlags, count, accessoryID);
    } else {
        count = UniqueAccessoryFlags_GetCount(fashionCase->uniqueAccessoryFlags, accessoryID);
        count += amount;

        if (count > MAX_UNIQUE_ACCESSORIES_PER_TYPE) {
            count = MAX_UNIQUE_ACCESSORIES_PER_TYPE;
        }

        accessoryID = Accessory_ToUniqueID(accessoryID);
        UniqueAccessoryFlags_SetCount(fashionCase->uniqueAccessoryFlags, count, accessoryID);
    }
}

void FashionCase_RemoveAccessory(FashionCase *fashionCase, u32 accessoryID, u32 amount)
{
    u8 count;

    GF_ASSERT(accessoryID < ACCESSORY_COUNT);

    if (Accessory_CanHaveMultiple(accessoryID)) {
        count = NonUniqueAccessoryFlags_GetCount(fashionCase->nonUniqueAccessoryFlags, accessoryID);

        if (count > amount) {
            count -= amount;
        } else {
            count = 0;
        }

        NonUniqueAccessoryFlags_SetCount(fashionCase->nonUniqueAccessoryFlags, count, accessoryID);
    } else {
        count = 0;
        accessoryID = Accessory_ToUniqueID(accessoryID);

        UniqueAccessoryFlags_SetCount(fashionCase->uniqueAccessoryFlags, count, accessoryID);
    }
}

void FashionCase_AddBackdrop(FashionCase *fashionCase, u32 backdropID)
{
    u8 count;

    GF_ASSERT(backdropID < BACKDROP_COUNT);

    // Backdrops are not stackable: owning one stores the total number of
    // owned backdrops in its slot, so the count doubles as a "has it" flag.
    if (BackdropFlags_GetCount(fashionCase->backdropFlags, backdropID) == BACKDROP_COUNT) {
        count = BackdropFlags_GetTotalCount(fashionCase->backdropFlags);

        BackdropFlags_SetCount(fashionCase->backdropFlags, count, backdropID);
    }
}

BOOL DressUpPhoto_HasData(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    if (photo->integrity == PHOTO_FULL_MAGIC) {
        return TRUE;
    }

    return FALSE;
}

void DressUpPhoto_SetLanguageAndMagic(DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    photo->integrity = PHOTO_FULL_MAGIC;
    photo->language = gGameLanguage;
}

void DressUpPhoto_Init(DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    DressUpPhoto_InitInternal(photo);
}

void DressUpPhoto_SetPhotoMonFromSprite(DressUpPhoto *photo, Pokemon *mon, UnkStruct_020298D8 *sprite)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    PhotoPokemon_SetDataFromMonAndSprite(&photo->photoMon, mon, sprite);
}

void DressUpPhoto_AddAccessory(DressUpPhoto *photo, const UnkStruct_ov22_02255040 *sprite, int slot)
{
    NNSG2dSVec2 position = SoftwareSprite_GetPosition(sprite->unk_04);
    int priority = SoftwareSprite_GetPriority(sprite->unk_04);

    GF_ASSERT(slot < PHOTO_ACCESSORY_COUNT);
    GF_ASSERT(position.x < 256);
    GF_ASSERT(position.y < 256);
    GF_ASSERT(priority > -128);
    GF_ASSERT(!(photo->accessoryFlags & (1 << slot)));
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    PhotoAccessory_SetData(&photo->accessories[slot], sprite->unk_00, position.x, position.y, priority);

    photo->accessoryFlags |= 1 << slot;
}

void DressUpPhoto_SetBackdrop(DressUpPhoto *photo, u8 backdrop)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    photo->backdrop = backdrop;
}

void DressUpPhoto_SetTitle(DressUpPhoto *photo, u16 word)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    EasyChatSentence_Init(&photo->title);
    EasyChatSentence_SetWord(&photo->title, 0, word);
}

void DressUpPhoto_Copy(DressUpPhoto *dest, const DressUpPhoto *src)
{
    GF_ASSERT(DressUpPhoto_IsValid(dest));
    memcpy(dest, src, sizeof(DressUpPhoto));
}

void DressUpPhoto_SetTrainerNameAndGender(DressUpPhoto *photo, const String *name, int gender)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    PhotoPokemon_SetTrainerNameAndGender(&photo->photoMon, name, gender);
}

BOOL DressUpPhoto_HasAccessory(const DressUpPhoto *photo, int slot)
{
    GF_ASSERT(slot < PHOTO_ACCESSORY_COUNT);
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    return photo->accessoryFlags & (1 << slot);
}

const PhotoPokemon *DressUpPhoto_GetPhotoMon(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    return &photo->photoMon;
}

const PhotoAccessory *DressUpPhoto_GetAccessory(const DressUpPhoto *photo, int slot)
{
    GF_ASSERT(slot < PHOTO_ACCESSORY_COUNT);
    GF_ASSERT(photo->accessoryFlags & (1 << slot));
    GF_ASSERT(DressUpPhoto_IsValid(photo));

    return &photo->accessories[slot];
}

u16 DressUpPhoto_GetMonSpecies(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    return PhotoPokemon_GetSpecies(&photo->photoMon);
}

void DressUpPhoto_SetTrainerName(const DressUpPhoto *photo, String *name)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    PhotoPokemon_SetTrainerName(&photo->photoMon, name);
}

u32 DressUpPhoto_GetTrainerGender(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    return PhotoPokemon_GetTrainerGender(&photo->photoMon);
}

u8 DressUpPhoto_GetBackdrop(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    return photo->backdrop;
}

u16 DressUpPhoto_GetTitleWord(const DressUpPhoto *photo)
{
    return EasyChatSentence_GetWord(&photo->title, 0);
}

u8 DressUpPhoto_GetLanguage(const DressUpPhoto *photo)
{
    GF_ASSERT(DressUpPhoto_IsValid(photo));
    return photo->language;
}

BOOL ContestPhoto_HasData(const ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));

    if (photo->integrity == PHOTO_FULL_MAGIC) {
        return TRUE;
    }

    return FALSE;
}

void ContestPhoto_SetFullMagic(ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    photo->integrity = PHOTO_FULL_MAGIC;
}

void ContestPhoto_Init(ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    ContestPhoto_InitInternal(photo);
}

void ContestPhoto_SetPhotoMonFromSprite(ContestPhoto *photo, Pokemon *mon, UnkStruct_020298D8 *sprite)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    PhotoPokemon_SetDataFromMonAndSprite(&photo->photoMon, mon, sprite);
}

void ContestPhoto_AddAccessory(ContestPhoto *photo, const UnkStruct_ov22_02255040 *sprite, int slot)
{
    NNSG2dSVec2 position = SoftwareSprite_GetPosition(sprite->unk_04);
    int priority = SoftwareSprite_GetPriority(sprite->unk_04);

    GF_ASSERT(slot < 20);
    GF_ASSERT(position.x < 256);
    GF_ASSERT(position.y < 256);
    GF_ASSERT(priority > -128);
    GF_ASSERT(!(photo->accessoryFlags & (1 << slot)));
    GF_ASSERT(ContestPhoto_IsValid(photo));

    PhotoAccessory_SetData(&photo->accessories[slot], sprite->unk_00, position.x, position.y, priority);

    photo->accessoryFlags |= 1 << slot;
}

void ContestPhoto_SetBackdrop(ContestPhoto *photo, u8 backdrop)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    photo->backdrop = backdrop;
}

void ContestPhoto_SetContestRank(ContestPhoto *photo, enum PokemonContestRank contestRank)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    photo->contestRank = contestRank;
}

void ContestPhoto_Copy(ContestPhoto *dest, const ContestPhoto *src)
{
    GF_ASSERT(ContestPhoto_IsValid(dest));
    memcpy(dest, src, sizeof(ContestPhoto));
}

void ContestPhoto_SetPhotoMonFromMon(ContestPhoto *photo, Pokemon *mon, s8 priority)
{
    u8 xPos;
    u8 yPos;

    GF_ASSERT(ContestPhoto_IsValid(photo));

    GetMonXYPositions(mon, &xPos, &yPos);
    PhotoPokemon_SetDataFromMon(&photo->photoMon, mon, xPos, yPos, priority);
}

void ContestPhoto_AddAccessoryWithData(ContestPhoto *photo, u32 slot, u8 accessoryID, u8 xPos, u8 yPos, s8 priority)
{
    GF_ASSERT(slot < 20);
    GF_ASSERT(accessoryID < ACCESSORY_COUNT);
    GF_ASSERT(xPos < 256);
    GF_ASSERT(yPos < 256);
    GF_ASSERT(priority > -128);
    GF_ASSERT(!(photo->accessoryFlags & (1 << slot)));
    GF_ASSERT(ContestPhoto_IsValid(photo));

    // Accessories must draw in front of the Pokemon.
    if (photo->photoMon.priority >= priority) {
        priority = photo->photoMon.priority + 1;
    }

    PhotoAccessory_SetData(&photo->accessories[slot], accessoryID, xPos, yPos, priority);
    photo->accessoryFlags |= 1 << slot;
}

BOOL ContestPhoto_HasAccessory(const ContestPhoto *photo, int slot)
{
    GF_ASSERT(slot < 20);
    GF_ASSERT(ContestPhoto_IsValid(photo));

    if ((photo->accessoryFlags & (1 << slot)) != 0) {
        return 1;
    }

    return 0;
}

void ContestPhoto_SetTrainerNameAndGender(ContestPhoto *photo, const String *name, int gender)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    PhotoPokemon_SetTrainerNameAndGender(&photo->photoMon, name, gender);
}

const PhotoPokemon *ContestPhoto_GetPhotoMon(const ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    return &photo->photoMon;
}

const PhotoAccessory *ContestPhoto_GetAccessory(const ContestPhoto *photo, int slot)
{
    GF_ASSERT(slot < 20);
    GF_ASSERT(photo->accessoryFlags & (1 << slot));
    GF_ASSERT(ContestPhoto_IsValid(photo));

    return &photo->accessories[slot];
}

void ContestPhoto_GetTrainerName(const ContestPhoto *photo, String *name)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    PhotoPokemon_SetTrainerName(&photo->photoMon, name);
}

u32 ContestPhoto_GetTrainerGender(const ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    return PhotoPokemon_GetTrainerGender(&photo->photoMon);
}

void ContestPhoto_CopyToPokemon(const ContestPhoto *photo, Pokemon *mon)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    PhotoPokemon_CopyToPokemonInternal(&photo->photoMon, mon);
}

u8 ContestPhoto_GetAccessoryID(const ContestPhoto *photo, int slot)
{
    GF_ASSERT(slot < 20);
    GF_ASSERT(photo->accessoryFlags & (1 << slot));
    GF_ASSERT(ContestPhoto_IsValid(photo));

    return PhotoAccessory_GetID(&photo->accessories[slot]);
}

u8 ContestPhoto_GetBackdrop(const ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    return photo->backdrop;
}

u32 ContestPhoto_GetContestRank(const ContestPhoto *photo)
{
    GF_ASSERT(ContestPhoto_IsValid(photo));
    return photo->contestRank;
}

u16 PhotoPokemon_GetSpecies(const PhotoPokemon *photoMon)
{
    return photoMon->species;
}

void PhotoPokemon_SetTrainerName(const PhotoPokemon *photoMon, String *name)
{
    String_CopyChars(name, photoMon->trainerName);
}

u32 PhotoPokemon_GetTrainerGender(const PhotoPokemon *photoMon)
{
    return photoMon->trainerGender;
}

s8 PhotoPokemon_GetPriority(const PhotoPokemon *photoMon)
{
    return photoMon->priority;
}

u8 PhotoPokemon_GetXPos(const PhotoPokemon *photoMon)
{
    return photoMon->xPos;
}

u8 PhotoPokemon_GetYPos(const PhotoPokemon *photoMon)
{
    return photoMon->yPos;
}

void PhotoPokemon_CopyToPokemon(const PhotoPokemon *photoMon, Pokemon *mon)
{
    PhotoPokemon_CopyToPokemonInternal(photoMon, mon);
}

u8 PhotoAccessory_GetID(const PhotoAccessory *accessory)
{
    return accessory->accessoryID;
}

u8 PhotoAccessory_GetXPos(const PhotoAccessory *accessory)
{
    return accessory->xPos;
}

u8 PhotoAccessory_GetYPos(const PhotoAccessory *accessory)
{
    return accessory->yPos;
}

s8 PhotoAccessory_GetPriority(const PhotoAccessory *accessory)
{
    return accessory->priority;
}

// A photo is "unique" if it has data and its bytes differ from every photo
// already saved in the image clips.
static BOOL ImageClips_IsPhotoUnique(ImageClips *imageClips, const DressUpPhoto *photo)
{
    int i;
    const void *savedPhoto;
    u32 photoCRC, savedCRC;
    MATHCRC32Table crcTable;
    BOOL isUnique = 1;

    if (DressUpPhoto_HasData(photo) == TRUE) {
        MATH_CRC32InitTable(&crcTable);
        photoCRC = MATH_CalcCRC32(&crcTable, photo, sizeof(DressUpPhoto));

        for (i = 0; i < SAVED_PHOTOS_COUNT; i++) {
            savedPhoto = ImageClips_GetDressUpPhoto(imageClips, i);
            MATH_CRC32InitTable(&crcTable);
            savedCRC = MATH_CalcCRC32(&crcTable, savedPhoto, sizeof(DressUpPhoto));

            if (savedCRC == photoCRC) {
                isUnique = 0;
                break;
            }
        }
    } else {
        isUnique = 0;
    }

    return isUnique;
}

// Merges the candidate photos into the saved list, dropping any that are
// already saved (or empty). Existing photos are shifted toward the end to
// make room, and slot 0 (the current photo) is preserved.
void ImageClips_AddUniquePhotos(u8 count, int skipIndex, ImageClips *imageClips, const void **photos)
{
    int uniqueCount;
    DressUpPhoto *dest;
    const DressUpPhoto *src;
    int i;
    int destIndex;

    uniqueCount = 0;

    for (i = 0; i < count; i++) {
        if (i == skipIndex) {
            continue;
        }

        if (photos[i] != NULL) {
            src = photos[i];

            if (ImageClips_IsPhotoUnique(imageClips, src) == 1) {
                uniqueCount++;
            }
        }
    }

    for (i = SAVED_PHOTOS_COUNT - 1; i >= 1; i--) {
        if (i + uniqueCount < SAVED_PHOTOS_COUNT) {
            dest = ImageClips_GetDressUpPhoto(imageClips, i + uniqueCount);
            src = ImageClips_GetDressUpPhoto(imageClips, i);

            DressUpPhoto_Copy(dest, src);
        }
    }

    destIndex = 1;

    for (i = 0; i < count; i++) {
        if (i == skipIndex) {
            continue;
        }

        if (photos[i] != NULL) {
            src = photos[i];

            if (ImageClips_IsPhotoUnique(imageClips, src) == 1) {
                dest = ImageClips_GetDressUpPhoto(imageClips, destIndex);
                destIndex++;
                DressUpPhoto_Copy(dest, src);
            }
        }
    }
}

ImageClips *SaveData_GetImageClips(SaveData *saveData)
{
    return SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_IMAGE_CLIPS);
}

// Copies a DressUpPhoto into the fixed-layout photo record used by the PC
// storage system. The record omits the nickname and trainer gender.
void DressUpPhoto_Serialize(const DressUpPhoto *photo, UnkStruct_ov61_0222AE80 *photoData)
{
    int i;

    MI_CpuClear8(photoData, sizeof(UnkStruct_ov61_0222AE80));

    photoData->integrity = photo->integrity;

    photoData->unk_04.personality = photo->photoMon.personality;
    photoData->unk_04.otID = photo->photoMon.otID;
    photoData->unk_04.species = photo->photoMon.species;

    for (i = 0; i < TRAINER_NAME_LEN + 1; i++) {
        photoData->unk_04.trainerName[i] = photo->photoMon.trainerName[i];
    }

    photoData->unk_04.priority = photo->photoMon.priority;
    photoData->unk_04.xPos = photo->photoMon.xPos;
    photoData->unk_04.yPos = photo->photoMon.yPos;
    photoData->unk_04.form = photo->photoMon.form;

    photoData->unk_24 = photo->accessoryFlags;
    photoData->title = photo->title;

    for (i = 0; i < PHOTO_ACCESSORY_COUNT; i++) {
        photoData->unk_30[i] = *((UnkStruct_ov61_0222AE80_sub2 *)(&photo->accessories[i]));
    }

    photoData->unk_58 = photo->backdrop;
    photoData->language = photo->language;
}

// Restores a DressUpPhoto from the PC storage record. The nickname and
// trainer gender are not stored, so they are reset here.
void DressUpPhoto_Deserialize(const UnkStruct_ov61_0222AE80 *photoData, DressUpPhoto *photo)
{
    int size;
    int i;

    size = DressUpPhoto_Size();
    MI_CpuClear8(photo, size);

    photo->integrity = photoData->integrity;

    photo->photoMon.personality = photoData->unk_04.personality;
    photo->photoMon.otID = photoData->unk_04.otID;
    photo->photoMon.species = photoData->unk_04.species;

    for (i = 0; i < TRAINER_NAME_LEN + 1; i++) {
        photo->photoMon.trainerName[i] = photoData->unk_04.trainerName[i];
    }

    photo->photoMon.priority = photoData->unk_04.priority;
    photo->photoMon.xPos = photoData->unk_04.xPos;
    photo->photoMon.yPos = photoData->unk_04.yPos;
    photo->photoMon.form = photoData->unk_04.form;

    photo->accessoryFlags = photoData->unk_24;
    photo->title = *((EasyChatSentence *)(&photoData->title));

    for (i = 0; i < PHOTO_ACCESSORY_COUNT; i++) {
        photo->accessories[i] = *((PhotoAccessory *)(&photoData->unk_30[i]));
    }

    photo->backdrop = photoData->unk_58;
    photo->language = photoData->language;

    for (i = 0; i < MON_NAME_LEN + 1; i++) {
        photo->photoMon.nickname[i] = CHAR_EOS;
    }

    photo->photoMon.trainerGender = 0;
}
