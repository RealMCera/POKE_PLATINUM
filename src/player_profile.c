#include "player_profile.h"

#include <nitro.h>
#include <string.h>

#include "generated/text_banks.h"

#include "struct_defs/struct_0202F298_sub1.h"
#include "struct_defs/struct_02030A80.h"
#include "struct_defs/wi_fi_history.h"

#include "appearance.h"
#include "charcode_util.h"
#include "easy_chat_sentence.h"
#include "easy_chat_words.h"
#include "heap.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "savedata_misc.h"
#include "species.h"
#include "string_gf.h"
#include "system_data.h"
#include "trainer_info.h"
#include "wifi_earth_place.h"
#include "wifi_history_save_data.h"

#include "res/text/bank/country_names.h"
#include "res/text/bank/greetings.h"
#include "res/text/bank/union_room_sentences.h"

PlayerProfile *PlayerProfile_New(enum HeapID heapID)
{
    PlayerProfile *profile = Heap_Alloc(heapID, sizeof(PlayerProfile));
    MI_CpuClear8(profile, sizeof(PlayerProfile));

    return profile;
}

void PlayerProfile_Free(PlayerProfile *profile)
{
    Heap_Free(profile);
}

// Fills a profile with the local player's trainer card data. The same routine
// is used for the header of a Vs. Recorder recording and for the profile sent
// to other players in the Union Room.
void PlayerProfile_Init(PlayerProfile *profile, SaveData *saveData)
{
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);
    WiFiHistory *wiFiHistory = SaveData_WiFiHistory(saveData);
    SystemData *systemData = SaveData_GetSystemData(saveData);
    const MiscSaveBlock *miscSaveBlock = SaveData_MiscSaveBlockConst(saveData);
    int species, form, isEgg;
    int i;
    OSOwnerInfo ownerInfo;

    OS_GetOwnerInfo(&ownerInfo);

    MiscSaveBlock_GetFavoriteMon(miscSaveBlock, &species, &form, &isEgg);
    MI_CpuClear8(profile, sizeof(PlayerProfile));
    CharCode_Copy(profile->name, TrainerInfo_Name(trainerInfo));

    profile->id = TrainerInfo_ID(trainerInfo);
    profile->gender = TrainerInfo_Gender(trainerInfo);
    profile->species = species;
    profile->form = form;
    profile->isEgg = isEgg;
    profile->country = WiFiHistory_GetCountry(wiFiHistory);
    profile->region = WiFiHistory_GetRegion(wiFiHistory);

    // Pre-fill the intro-message buffer with WORD_NONE so the unused word
    // slots of the EasyChat sentence are blank.
    for (i = 0; i < 40; i++) {
        profile->introMessage[i] = 0xffff;
    }

    MiscSaveBlock_IntroMsg(miscSaveBlock, &profile->introSentence);

    profile->month = ownerInfo.birthday.month;
    profile->appearance = Appearance_GetData(TrainerInfo_Gender(trainerInfo), TrainerInfo_Appearance(trainerInfo), APPEARANCE_DATA_INDEX);
    profile->version = GAME_VERSION;
    profile->language = GAME_LANGUAGE;
    profile->checksum.checksum = SaveData_CalculateChecksum(saveData, profile, sizeof(PlayerProfile) - (sizeof(BattleRecordingChecksum)));
}

String *PlayerProfile_GetName(const PlayerProfile *profile, enum HeapID heapID)
{
    String *name = String_Init((7 * 2) + 1, heapID);

    String_CopyNumChars(name, profile->name, (7 * 2) + 1);
    return name;
}

u32 PlayerProfile_GetGender(const PlayerProfile *profile)
{
    // Fall back to male if the stored gender is not one of the valid values.
    if ((profile->gender != GENDER_MALE) && (profile->gender != GENDER_FEMALE)) {
        return GENDER_MALE;
    }

    return profile->gender;
}

int PlayerProfile_GetSpecies(const PlayerProfile *profile)
{
    // Treat an out-of-range species as no Pokémon.
    if (profile->species >= MAX_SPECIES) {
        return SPECIES_NONE;
    }

    return profile->species;
}

int PlayerProfile_GetForm(const PlayerProfile *profile)
{
    if (profile->species >= MAX_SPECIES) {
        return SPECIES_NONE;
    }

    return Pokemon_SanitizeFormId(profile->species, profile->form);
}

int PlayerProfile_GetIsEgg(const PlayerProfile *profile)
{
    // Clamp the one-bit flag to 0/1 in case a received profile has garbage.
    if (profile->isEgg > 1) {
        return 1;
    }

    return profile->isEgg;
}

int PlayerProfile_GetCountry(const PlayerProfile *profile)
{
    // Note: 234 is the count of entries in the `country_names` text bank.
    if (profile->country >= 234) {
        return Country_Text_None;
    }

    return profile->country;
}

int PlayerProfile_GetRegion(const PlayerProfile *profile)
{
    if (profile->country >= 234) {
        return 0;
    }

    // Reject a region that does not exist for the given country.
    if (WiFiEarthPlace_GetRegionLimit(profile->country) < profile->region) {
        return 0;
    }

    return profile->region;
}

// Returns the profile's intro message. When the profile stores an EasyChat
// sentence (introMessageIsString == 0) the sentence is copied into
// `outSentence` and NULL is returned; otherwise a String holding the raw
// charcode message is returned. An invalid sentence is replaced with a default
// Union Room greeting.
String *PlayerProfile_GetIntroMessage(const PlayerProfile *profile, EasyChatSentence *outSentence, enum HeapID heapID)
{
    int invalid = 0;

    if (profile->introMessageIsString == 0) {
        *outSentence = profile->introSentence;

        if (outSentence->type >= 5) {
            invalid++;
        } else if (outSentence->id > 19) {
            invalid++;
        } else {
            u32 loaderIndex, entry;

            if (((outSentence->words[0] != WORD_NONE) && (EasyChatWord_GetLoaderIndexAndEntry(outSentence->words[0], &loaderIndex, &entry) == 0)) || ((outSentence->words[1] != WORD_NONE) && (EasyChatWord_GetLoaderIndexAndEntry(outSentence->words[1], &loaderIndex, &entry) == 0))) {
                invalid++;
            }
        }

        if (invalid > 0) {
            EasyChatSentence_InitWithType(outSentence, EASY_CHAT_SENTENCE_TYPE_UNION_ROOM);
            outSentence->id = UnionRoomSentences_Text_BlankHello;
            outSentence->words[0] = EasyChatWord_FromBankAndEntry(TEXT_BANK_GREETINGS, Greetings_Text_Regards);
            outSentence->words[1] = WORD_NONE;
        }

        return NULL;
    } else {
        String *message = String_Init(40, heapID);

        String_CopyNumChars(message, profile->introMessage, 40);
        return message;
    }
}

int PlayerProfile_GetBirthdayMonth(const PlayerProfile *profile)
{
    // Clamp to a valid month, defaulting to January.
    if ((profile->month >= 1) && (profile->month <= 12)) {
        return profile->month;
    }

    return 1;
}

int PlayerProfile_GetAppearance(const PlayerProfile *profile)
{
    // Fall back to the school kid appearance if out of range.
    if (profile->appearance > TRAINER_APPEARANCE_LADY) {
        return TRAINER_APPEARANCE_SCHOOL_KID_M;
    }

    return profile->appearance;
}
