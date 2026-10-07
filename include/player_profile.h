#ifndef POKEPLATINUM_PLAYER_PROFILE_H
#define POKEPLATINUM_PLAYER_PROFILE_H

#include "struct_decls/struct_02030A80_decl.h"

#include "easy_chat_sentence.h"
#include "savedata.h"
#include "string_gf.h"

// Player profiles: the trainer card data shown for a Vs. Recorder battle
// recording and exchanged between players in the Union Room. The getters
// sanitize fields that may have been received from another player.
PlayerProfile *PlayerProfile_New(enum HeapID heapID);
void PlayerProfile_Free(PlayerProfile *profile);
void PlayerProfile_Init(PlayerProfile *profile, SaveData *saveData);
String *PlayerProfile_GetName(const PlayerProfile *profile, enum HeapID heapID);
u32 PlayerProfile_GetGender(const PlayerProfile *profile);
int PlayerProfile_GetSpecies(const PlayerProfile *profile);
int PlayerProfile_GetForm(const PlayerProfile *profile);
int PlayerProfile_GetIsEgg(const PlayerProfile *profile);
int PlayerProfile_GetCountry(const PlayerProfile *profile);
int PlayerProfile_GetRegion(const PlayerProfile *profile);
String *PlayerProfile_GetIntroMessage(const PlayerProfile *profile, EasyChatSentence *outSentence, enum HeapID heapID);
int PlayerProfile_GetBirthdayMonth(const PlayerProfile *profile);
int PlayerProfile_GetAppearance(const PlayerProfile *profile);

#endif // POKEPLATINUM_PLAYER_PROFILE_H
