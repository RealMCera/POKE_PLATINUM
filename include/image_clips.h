#ifndef POKEPLATINUM_IMAGE_CLIPS_H
#define POKEPLATINUM_IMAGE_CLIPS_H

#include "generated/pokemon_contest_ranks.h"

#include "struct_defs/dress_up_photo.h"
#include "struct_defs/image_clips.h"
#include "struct_defs/photo_accessory.h"
#include "struct_defs/photo_pokemon.h"
#include "struct_defs/struct_020298D8.h"
#include "struct_defs/struct_02029C88.h"

#include "overlay022/struct_ov22_02255040.h"
#include "overlay061/struct_ov61_0222AE80.h"

#include "pokemon.h"
#include "savedata.h"
#include "string_gf.h"

void ImageClips_Init(ImageClips *imageClips);
int ImageClips_SaveSize(void);
int DressUpPhoto_Size(void);
int ContestPhoto_Size(void);
DressUpPhoto *DressUpPhoto_New(u32 heapID);
ContestPhoto *ContestPhoto_New(u32 heapID);
DressUpPhoto *ImageClips_GetDressUpPhoto(ImageClips *imageClips, int slot);
ContestPhoto *ImageClips_GetContestPhoto(ImageClips *imageClips, int contestType);
FashionCase *ImageClips_GetFashionCase(ImageClips *imageClips);
BOOL ImageClips_DressUpPhotoHasData(const ImageClips *imageClips, int slot);
BOOL ImageClips_ContestPhotoHasData(const ImageClips *imageClips, int contestType);
BOOL FashionCase_CanFitAccessoryCount(const FashionCase *fashionCase, u32 accessoryID, u32 count);
BOOL FashionCase_HasBackdrop(const FashionCase *fashionCase, u32 backdropID);
u32 FashionCase_GetAccessoryCount(const FashionCase *fashionCase, u32 accessoryID);
u32 FashionCase_GetBackdropCount(const FashionCase *fashionCase, u32 backdropID);
u32 FashionCase_GetTotalAccessories(const FashionCase *fashionCase);
u32 FashionCase_GetTotalBackdrops(const FashionCase *fashionCase);
void FashionCase_AddAccessory(FashionCase *fashionCase, u32 accessoryID, u32 amount);
void FashionCase_RemoveAccessory(FashionCase *fashionCase, u32 accessoryID, u32 amount);
void FashionCase_AddBackdrop(FashionCase *fashionCase, u32 backdropID);
BOOL DressUpPhoto_HasData(const DressUpPhoto *photo);
void DressUpPhoto_SetLanguageAndMagic(DressUpPhoto *photo);
void DressUpPhoto_Init(DressUpPhoto *photo);
void DressUpPhoto_SetPhotoMonFromSprite(DressUpPhoto *photo, Pokemon *mon, UnkStruct_020298D8 *sprite);
void DressUpPhoto_AddAccessory(DressUpPhoto *photo, const UnkStruct_ov22_02255040 *sprite, int slot);
void DressUpPhoto_SetBackdrop(DressUpPhoto *photo, u8 backdrop);
void DressUpPhoto_SetTitle(DressUpPhoto *photo, u16 word);
void DressUpPhoto_Copy(DressUpPhoto *dest, const DressUpPhoto *src);
void DressUpPhoto_SetTrainerNameAndGender(DressUpPhoto *photo, const String *name, int gender);
BOOL DressUpPhoto_HasAccessory(const DressUpPhoto *photo, int slot);
const PhotoPokemon *DressUpPhoto_GetPhotoMon(const DressUpPhoto *photo);
const PhotoAccessory *DressUpPhoto_GetAccessory(const DressUpPhoto *photo, int slot);
u16 DressUpPhoto_GetMonSpecies(const DressUpPhoto *photo);
void DressUpPhoto_SetTrainerName(const DressUpPhoto *photo, String *name);
u32 DressUpPhoto_GetTrainerGender(const DressUpPhoto *photo);
u8 DressUpPhoto_GetBackdrop(const DressUpPhoto *photo);
u16 DressUpPhoto_GetTitleWord(const DressUpPhoto *photo);
u8 DressUpPhoto_GetLanguage(const DressUpPhoto *photo);
BOOL ContestPhoto_HasData(const ContestPhoto *photo);
void ContestPhoto_SetFullMagic(ContestPhoto *photo);
void ContestPhoto_Init(ContestPhoto *photo);
void ContestPhoto_SetPhotoMonFromSprite(ContestPhoto *photo, Pokemon *mon, UnkStruct_020298D8 *sprite);
void ContestPhoto_AddAccessory(ContestPhoto *photo, const UnkStruct_ov22_02255040 *sprite, int slot);
void ContestPhoto_SetBackdrop(ContestPhoto *photo, u8 backdrop);
void ContestPhoto_SetContestRank(ContestPhoto *photo, enum PokemonContestRank contestRank);
void ContestPhoto_Copy(ContestPhoto *dest, const ContestPhoto *src);
void ContestPhoto_SetPhotoMonFromMon(ContestPhoto *photo, Pokemon *mon, s8 priority);
void ContestPhoto_AddAccessoryWithData(ContestPhoto *photo, u32 slot, u8 accessoryID, u8 xPos, u8 yPos, s8 priority);
BOOL ContestPhoto_HasAccessory(const ContestPhoto *photo, int slot);
void ContestPhoto_SetTrainerNameAndGender(ContestPhoto *photo, const String *name, int gender);
const PhotoPokemon *ContestPhoto_GetPhotoMon(const ContestPhoto *photo);
const PhotoAccessory *ContestPhoto_GetAccessory(const ContestPhoto *photo, int slot);
void ContestPhoto_GetTrainerName(const ContestPhoto *photo, String *name);
u32 ContestPhoto_GetTrainerGender(const ContestPhoto *photo);
void ContestPhoto_CopyToPokemon(const ContestPhoto *photo, Pokemon *mon);
u8 ContestPhoto_GetAccessoryID(const ContestPhoto *photo, int slot);
u8 ContestPhoto_GetBackdrop(const ContestPhoto *photo);
u32 ContestPhoto_GetContestRank(const ContestPhoto *photo);
u16 PhotoPokemon_GetSpecies(const PhotoPokemon *photoMon);
void PhotoPokemon_SetTrainerName(const PhotoPokemon *photoMon, String *name);
u32 PhotoPokemon_GetTrainerGender(const PhotoPokemon *photoMon);
s8 PhotoPokemon_GetPriority(const PhotoPokemon *photoMon);
u8 PhotoPokemon_GetXPos(const PhotoPokemon *photoMon);
u8 PhotoPokemon_GetYPos(const PhotoPokemon *photoMon);
void PhotoPokemon_CopyToPokemon(const PhotoPokemon *photoMon, Pokemon *mon);
u8 PhotoAccessory_GetID(const PhotoAccessory *accessory);
u8 PhotoAccessory_GetXPos(const PhotoAccessory *accessory);
u8 PhotoAccessory_GetYPos(const PhotoAccessory *accessory);
s8 PhotoAccessory_GetPriority(const PhotoAccessory *accessory);
void ImageClips_AddUniquePhotos(u8 count, int skipIndex, ImageClips *imageClips, const void **photos);
ImageClips *SaveData_GetImageClips(SaveData *saveData);
void DressUpPhoto_Serialize(const DressUpPhoto *photo, UnkStruct_ov61_0222AE80 *photoData);
void DressUpPhoto_Deserialize(const UnkStruct_ov61_0222AE80 *photoData, DressUpPhoto *photo);

#endif // POKEPLATINUM_IMAGE_CLIPS_H
