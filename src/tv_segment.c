#include "tv_segment.h"

#include <nitro.h>

#include "constants/flavor.h"
#include "constants/heap.h"
#include "constants/overworld_weather.h"
#include "constants/tv_broadcast.h"
#include "generated/first_arrival_to_zones.h"
#include "generated/map_headers.h"
#include "generated/natures.h"
#include "generated/pokemon_stats.h"

#include "struct_decls/tv_broadcast.h"
#include "struct_defs/dress_up_photo.h"
#include "struct_defs/image_clips.h"
#include "struct_defs/special_encounter.h"
#include "struct_defs/struct_0202E7E4.h"
#include "struct_defs/struct_0202E7F0.h"
#include "struct_defs/struct_0202E7FC.h"
#include "struct_defs/struct_0202E808.h"
#include "struct_defs/struct_0202E810.h"
#include "struct_defs/struct_0202E81C.h"
#include "struct_defs/struct_0202E828.h"
#include "struct_defs/struct_0202E834.h"
#include "struct_defs/tv_segment_contest_hall_showcased_pokemon.h"

#include "applications/poketch/poketch_system.h"
#include "field/field_system.h"
#include "field/field_system_sub2_t.h"
#include "overlay006/swarm.h"
#include "overlay006/tv_episode.h"
#include "savedata/save_table.h"

#include "bag.h"
#include "berry_patches.h"
#include "charcode_util.h"
#include "field_battle_data_transfer.h"
#include "field_overworld_weather.h"
#include "field_system.h"
#include "heap.h"
#include "inlines.h"
#include "map_header.h"
#include "map_header_util.h"
#include "math_util.h"
#include "message.h"
#include "party.h"
#include "poffin_types.h"
#include "pokedex.h"
#include "pokemon.h"
#include "record_mixed_rng.h"
#include "ribbon.h"
#include "roaming_pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "savedata_misc.h"
#include "special_encounter.h"
#include "string_gf.h"
#include "string_template.h"
#include "system_flags.h"
#include "trainer_info.h"
#include "image_clips.h"
#include "tv_broadcast.h"
#include "party_helpers.h"
#include "script_helpers.h"
#include "contest_util.h"
#include "vars_flags.h"

#include "res/text/bank/tv_programs_interviews.h"
#include "res/text/bank/tv_programs_sinnoh_now.h"
#include "res/text/bank/tv_programs_trainer_sightings.h"

// TV broadcast segments. Each TV program (Trainer Sightings, Records,
// Interviews, Sinnoh Now, Variety Hour) is a table of segments; a segment
// pairs a "load message" callback that fills a StringTemplate and returns a
// text-bank message ID with an optional "is eligible" callback that decides
// whether the segment may air. TVSegment_LoadMessage and TVSegment_IsEligible
// dispatch to the segment selected by the current TVEpisode.
//
// The save block stores one 40-byte TVSegmentData union per segment. The
// FieldSystem_SaveTVSegment_* / TVSegment_Save* functions populate that union
// from gameplay events (catching a Pokémon, fishing, winning at the Battle
// Tower, earning a Ribbon, ...), and the TVSegment_LoadMessage_* callbacks
// read it back when the episode is shown.

static void FieldSystem_SaveTVSegment(FieldSystem *fieldSystem, int programTypeID, int segmentID, const void *segment);
static void SaveData_SaveTVSegment(SaveData *saveData, int programTypeID, int segmentID, const void *segment);
static u8 TVSegment_CountRibbons(Pokemon *param0);
static String *TVSegment_GetSpeciesNameString(u16 param0, enum HeapID heapID);

#define TV_EPISODE_SEGMENT_SIZE 40
#define TEMPLATE_NAME_SIZE      MON_NAME_LEN + 1

typedef struct TVSegment_Dummy {
    u8 unused[TV_EPISODE_SEGMENT_SIZE];
} TVSegment_Dummy;

typedef CaptureAttempt TVSegment_CatchThatPokemonShow;

typedef struct TVSegment_WhatsFishing {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u16 fishingRodItemID;
    BOOL caughtFish;
} TVSegment_WhatsFishing;

typedef struct TVSegment_LoveThatGroupCorner {
    u16 groupName[TEMPLATE_NAME_SIZE];
} TVSegment_LoveThatGroupCorner;

typedef struct TVSegment_HiddenItemBreakingNews {
    u16 item;
    u16 location;
} TVSegment_HiddenItemBreakingNews;

typedef struct TVSegment_SinnohShoppingChampCorner {
    u16 item;
    u8 amount;
} TVSegment_SinnohShoppingChampCorner;

typedef struct TVSegment_HappyHappyEggClub {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u16 location;
} TVSegment_HappyHappyEggClub;

typedef struct TVSegment_RateThatNameChange {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u16 nickname[TEMPLATE_NAME_SIZE];
} TVSegment_RateThatNameChange;

typedef struct TVSegment_UndergroundTreasuresCorner {
    u16 item;
    u16 amount;
} TVSegment_UndergroundTreasuresCorner;

typedef struct TVSegment_SafariGameSpecialNewsBulletin {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 numPokemonCaught;
} TVSegment_SafariGameSpecialNewsBulletin;

typedef struct TVSegment_PokemonStorageSpecialNewsBulletin {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
} TVSegment_PokemonStorageSpecialNewsBulletin;

typedef struct TVSegment_HerbalMedicineTrainerSightingDummy {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u16 item;
} TVSegment_HerbalMedicineTrainerSightingDummy;

typedef struct TVSegment_PlantingAndWateringShow {
    u16 berryItemID;
    u8 yieldRating;
    u16 yieldAmount;
} TVSegment_PlantingAndWateringShow;

typedef struct TVSegment_SealClubShow {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 dummy;
    u8 ballSeal;
} TVSegment_SealClubShow;

typedef struct TVSegment_CaptureTheFlagDigest {
    int trainerInfoSize;
    u8 trainerInfo[sizeof(TrainerInfo)];
} TVSegment_CaptureTheFlagDigest;

typedef struct TVSegment_HomeAndManor_NoFurniture {
    u8 dummy;
} TVSegment_HomeAndManor_NoFurniture;

typedef struct TVSegment_HomeAndManor {
    u8 furniture;
} TVSegment_HomeAndManor;

// "Rack 'Em Up Records" segment: a Battle Tower win streak and the lead
// Pokémon of the team that set it. isSingleBattle selects the Single or
// Double Battle variant of the message.
typedef struct {
    u16 streak;
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 isSingleBattle;
} TVSegment_BattleTowerStreakRecord;

// "Rack 'Em Up Records" segment: the largest Pokémon size recorded, in
// millimeters.
typedef struct {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u32 size;
} TVSegment_SizeRecord;

// "Rack 'Em Up Records" segment: a Game Corner slot-machine session. Only
// saved when the player won at least 1000 Coins.
typedef struct {
    u32 coinsBefore;
    u32 coinsAfter;
    u32 minutesPlayed;
} TVSegment_SlotMachineRecord;

// "Rack 'Em Up Records" segment: a Pokémon that earned a Ribbon, along with
// the number of Ribbons it now holds.
typedef struct {
    u16 nickname[TEMPLATE_NAME_SIZE];
    u8 ribbonNameID;
    u8 ribbonCount;
    u8 hasNickname;
    u8 gender;
    u8 language;
    u8 metGame;
    u16 species;
} TVSegment_RibbonRecord;

// "Rack 'Em Up Records" segment: the most Traps disarmed in one session.
typedef struct {
    u16 trapID;
    u16 count;
} TVSegment_TrapRecord;

// "Rack 'Em Up Records" segment: the most Flags captured in one session.
typedef struct {
    u16 count;
} TVSegment_CaptureTheFlagRecord;

// "Rack 'Em Up Records" segment: Battle Points earned in one day.
typedef struct {
    TVSegment_BattlePointsRecordData data;
} TVSegment_BattlePointsRecord;

// "Rack 'Em Up Records" segment: Pokémon traded over the GTS in one day.
typedef struct {
    TVSegment_GTSTradeRecordData data;
} TVSegment_GTSTradeRecord;

typedef struct TVSegment_BattleTowerCorner {
    TVSegment_BattleTowerCornerData outcome;
    u16 customMessageWord;
} TVSegment_BattleTowerCorner;

typedef struct TVSegment_YourPokemonCorner {
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 hasNickname;
    u16 nickname[TEMPLATE_NAME_SIZE];
    u16 customMessageWord;
} TVSegment_YourPokemonCorner;

typedef struct TVSegment_ThePoketchWatch {
    int appID;
    u16 customMessageWord;
} TVSegment_ThePoketchWatch;

typedef struct TVSegment_ContestHall {
    TVSegment_ContestHall_ShowcasedPokemon showcasedPokemon;
    u16 customMessageWord;
} TVSegment_ContestHall;

typedef struct TVSegment_RightOnPhotoCorner {
    u16 species;
    u16 customMessageWord;
} TVSegment_RightOnPhotoCorner;

typedef struct TVSegment_StreetCornerPersonalityCheckup {
    int pokemonType;
} TVSegment_StreetCornerPersonalityCheckup;

typedef struct TVSegment_ThreeCheersForPoffinCorner {
    TVSegment_ThreeCheersForPoffinCornerData data;
    u16 customMessageWord;
} TVSegment_ThreeCheersForPoffinCorner;

typedef struct TVSegment_AmitySquareWatch {
    TVSegment_AmitySquareWatchData data;
    u16 customWordMessage;
} TVSegment_AmitySquareWatch;

typedef struct TVSegment_BattleFrontierFrontlineNews_Single {
    TVSegment_BattleFrontierFrontlineNewsSingleData data;
    u16 customWordMessage;
} TVSegment_BattleFrontierFrontlineNews_Single;

typedef struct TVSegment_InYourFaceInterview_Question {
    u16 customWordMessage;
} TVSegment_InYourFaceInterview_Question;

typedef struct TVSegment_BattleFrontierFrontlineNews_Multi {
    TVSegment_BattleFrontierFrontlineNewsMultiData data;
    u16 customWordMessage;
} TVSegment_BattleFrontierFrontlineNews_Multi;

typedef union TVSegmentData {
    TVSegment_Dummy dummy1;
    TVSegment_CatchThatPokemonShow catchThatPokemonShow;
    TVSegment_WhatsFishing whatsFishing;
    TVSegment_LoveThatGroupCorner loveThatGroupCorner;
    TVSegment_HiddenItemBreakingNews hiddenItemBreakingNews;
    TVSegment_SinnohShoppingChampCorner sinnohShoppingChampCorner;
    TVSegment_HappyHappyEggClub happyHappyEggClub;
    TVSegment_RateThatNameChange rateThatNameChange;
    TVSegment_UndergroundTreasuresCorner undergroundTreasuresCorner;
    TVSegment_SafariGameSpecialNewsBulletin safariGameSpecialNewsBulletin;
    TVSegment_PokemonStorageSpecialNewsBulletin pokemonStorageSpecialNewsBulletin;
    TVSegment_HerbalMedicineTrainerSightingDummy dummy2;
    TVSegment_PlantingAndWateringShow plantingAndWateringShow;
    TVSegment_SealClubShow sealClubShow;
    TVSegment_CaptureTheFlagDigest captureTheFlagDigest;
    TVSegment_HomeAndManor_NoFurniture homeAndManorNoFurniture;
    TVSegment_HomeAndManor homeAndManor;
    TVSegment_BattleTowerStreakRecord battleTowerStreakRecord;
    TVSegment_SizeRecord sizeRecord;
    TVSegment_SlotMachineRecord slotMachineRecord;
    TVSegment_RibbonRecord ribbonRecord;
    TVSegment_TrapRecord trapRecord;
    TVSegment_CaptureTheFlagRecord captureTheFlagRecord;
    TVSegment_BattlePointsRecord battlePointsRecord;
    TVSegment_GTSTradeRecord gtsTradeRecord;
    TVSegment_BattleTowerCorner battleTowerCorner;
    TVSegment_YourPokemonCorner yourPokemonCorner;
    TVSegment_ThePoketchWatch thePoketchWatch;
    TVSegment_ContestHall contestHall;
    TVSegment_RightOnPhotoCorner rightOnPhotoCorner;
    TVSegment_StreetCornerPersonalityCheckup streetCornerPersonalityCheckup;
    TVSegment_ThreeCheersForPoffinCorner threeCheersForPoffinCorner;
    TVSegment_AmitySquareWatch amitySquareWatch;
    TVSegment_BattleFrontierFrontlineNews_Single battleFrontierFrontlineNewsSingle;
    TVSegment_InYourFaceInterview_Question inYourFaceInterviewQuestion;
    TVSegment_BattleFrontierFrontlineNews_Multi battleFrontierFrontlineNewsMulti;
} TVSegmentData;

typedef int (*TVSegment_LoadMessageFunction)(FieldSystem *, StringTemplate *, TVEpisode *);
typedef BOOL (*TVSegment_IsEligibleFunction)(FieldSystem *, TVEpisode *);

typedef struct TVSegment {
    TVSegment_LoadMessageFunction loadMessageFn;
    TVSegment_IsEligibleFunction isEligibleFn;
} TVSegment;

typedef struct TVProgramType {
    int programTypeID;
    u16 bankID;
    u16 numSegments;
    const TVSegment *segments;
} TVProgramType;

#define TV_PROGRAM_SEGMENT_NULL \
    {                           \
        NULL, NULL              \
    }

static const TVSegment sTrainerSightingsSegments[TV_PROGRAM_TYPE_TRAINER_SIGHTINGS_NUM_SEGMENTS];
static const TVSegment sRecordsSegments[TV_PROGRAM_TYPE_RECORDS_NUM_SEGMENTS];
static const TVSegment sInterviewsSegments[TV_PROGRAM_TYPE_INTERVIEWS_NUM_SEGMENTS];
static const TVSegment sSinnohNowSegments[TV_PROGRAM_TYPE_SINNOH_NOW_NUM_SEGMENTS];
static const TVSegment sVarietyHourSegments[TV_PROGRAM_TYPE_VARIETY_HOUR_NUM_SEGMENTS];

static const TVProgramType sProgramTypes[TV_PROGRAM_TYPE_MAX - 1] = {
    [TV_PROGRAM_TYPE_INTERVIEWS - 1] = {
        .programTypeID = TV_PROGRAM_TYPE_INTERVIEWS,
        .bankID = TEXT_BANK_TV_PROGRAMS_INTERVIEWS,
        .numSegments = TV_PROGRAM_TYPE_INTERVIEWS_NUM_SEGMENTS + 1,
        .segments = sInterviewsSegments,
    },
    [TV_PROGRAM_TYPE_TRAINER_SIGHTINGS - 1] = {
        .programTypeID = TV_PROGRAM_TYPE_TRAINER_SIGHTINGS,
        .bankID = TEXT_BANK_TV_PROGRAMS_TRAINER_SIGHTINGS,
        .numSegments = TV_PROGRAM_TYPE_TRAINER_SIGHTINGS_NUM_SEGMENTS + 1,
        .segments = sTrainerSightingsSegments,
    },
    [TV_PROGRAM_TYPE_RECORDS - 1] = {
        .programTypeID = TV_PROGRAM_TYPE_RECORDS,
        .bankID = TEXT_BANK_TV_PROGRAMS_RECORDS,
        .numSegments = TV_PROGRAM_TYPE_RECORDS_NUM_SEGMENTS + 1,
        .segments = sRecordsSegments,
    },
    [TV_PROGRAM_TYPE_SINNOH_NOW - 1] = {
        .programTypeID = TV_PROGRAM_TYPE_SINNOH_NOW,
        .bankID = TEXT_BANK_TV_PROGRAMS_SINNOH_NOW,
        .numSegments = TV_PROGRAM_TYPE_SINNOH_NOW_NUM_SEGMENTS + 1,
        .segments = sSinnohNowSegments,
    },
    [TV_PROGRAM_TYPE_VARIETY_HOUR - 1] = {
        .programTypeID = TV_PROGRAM_TYPE_VARIETY_HOUR,
        .bankID = TEXT_BANK_TV_PROGRAMS_VARIETY_HOUR,
        .numSegments = TV_PROGRAM_TYPE_VARIETY_HOUR_NUM_SEGMENTS + 1,
        .segments = sVarietyHourSegments,
    },
};

static const TVProgramType *TVBroadcast_GetProgramType(int programTypeID)
{
    const TVProgramType *programType;

    GF_ASSERT(0 < programTypeID && programTypeID < TV_PROGRAM_TYPE_MAX);
    programType = &sProgramTypes[programTypeID - 1];
    GF_ASSERT(programType->programTypeID == programTypeID);

    return programType;
}

static const TVSegment *TVProgramType_GetSegment(const TVProgramType *programType, const TVEpisode *episode)
{
    int segmentID = TVEpisode_GetSegmentID(episode);
    GF_ASSERT(0 < segmentID && segmentID < programType->numSegments);

    return &programType->segments[segmentID - 1];
}

int TVSegment_LoadMessage(int programTypeID, FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode, u16 *bankDestVar)
{
    TVSegment_LoadMessageFunction loadMessageFn;
    const TVProgramType *programType;
    const TVSegment *segment;

    programType = TVBroadcast_GetProgramType(programTypeID);
    *bankDestVar = programType->bankID;
    segment = TVProgramType_GetSegment(programType, episode);
    loadMessageFn = segment->loadMessageFn;

    GF_ASSERT(loadMessageFn != NULL);
    return loadMessageFn(fieldSystem, template, episode);
}

BOOL TVSegment_IsEligible(int programTypeID, FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_IsEligibleFunction isEligibleFn;
    const TVProgramType *programType;
    const TVSegment *segment;

    programType = TVBroadcast_GetProgramType(programTypeID);
    segment = TVProgramType_GetSegment(programType, episode);

    if (segment->loadMessageFn == NULL) {
        return FALSE;
    }

    isEligibleFn = segment->isEligibleFn;

    if (isEligibleFn == NULL) {
        return TRUE;
    }

    return isEligibleFn(fieldSystem, episode);
}

// Identical to SaveData_SaveTVSegment below; the original binary contains both
// copies, so both are kept.
static void SaveData_SaveTVSegmentDuplicate(SaveData *saveData, int param1, int param2, const void *param3)
{
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(saveData);

    GF_ASSERT(sizeof(TVSegmentData) == TV_EPISODE_SEGMENT_SIZE);
    TVBroadcast_SaveSegmentData(broadcast, param1, param2, (const u8 *)param3);
}

static void FieldSystem_SaveTVSegment(FieldSystem *fieldSystem, int programTypeID, int segmentID, const void *segment)
{
    SaveData_SaveTVSegment(fieldSystem->saveData, programTypeID, segmentID, segment);
}

static void SaveData_SaveTVSegment(SaveData *saveData, int programTypeID, int segmentID, const void *segment)
{
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(saveData);

    GF_ASSERT(sizeof(TVSegmentData) == TV_EPISODE_SEGMENT_SIZE);
    TVBroadcast_SaveSegmentData(broadcast, programTypeID, segmentID, (const u8 *)segment);
}

// Sets template variable idx to the string held in a u16 character array.
static void TVSegment_SetTemplateString(StringTemplate *template, int idx, const u16 *param2, int unused3, int language, int unused5)
{
    String *string = String_Init(64, HEAP_ID_FIELD1);

    String_CopyChars(string, param2);
    StringTemplate_SetString(template, idx, string, unused3, unused5, language);
    String_Free(string);
}

static void TVSegment_SetTemplateTrainerName(StringTemplate *template, int idx, const TVEpisode *episode)
{
    TVSegment_SetTemplateString(template, idx, TVEpisode_GetTrainerName(episode), TVEpisode_GetGender(episode), TVEpisode_GetLanguage(episode), 1);
}

static void TVSegment_CopyPokemonNickname(enum HeapID heapID, u16 *param1, Pokemon *mon)
{
    String *string = String_Init(64, heapID);

    Pokemon_GetValue(mon, MON_DATA_NICKNAME_STRING, string);
    String_ToChars(string, param1, TEMPLATE_NAME_SIZE);
    String_Free(string);
}

// Copies the fields shared by most segment structs: species, gender, language
// and the game the Pokémon was met in.
static void TVSegment_CopyPokemonValues(Pokemon *mon, u16 *species, u8 *gender, u8 *language, u8 *metGame)
{
    *species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    *gender = Pokemon_GetValue(mon, MON_DATA_GENDER, NULL);
    *language = Pokemon_GetValue(mon, MON_DATA_LANGUAGE, NULL);
    *metGame = Pokemon_GetValue(mon, MON_DATA_MET_GAME, NULL);
}

// Sets template variable idx to the localized species name.
static void TVSegment_SetTemplatePokemonSpecies(StringTemplate *template, int idx, u16 species, u8 unused3, u8 language, u8 unused5)
{
    u16 speciesName[TEMPLATE_NAME_SIZE];

    MessageLoader_GetSpeciesName(species, HEAP_ID_FIELD1, speciesName);
    TVSegment_SetTemplateString(template, idx, speciesName, unused3, language, 1);
}

static void TVSegment_SetTemplateOwnPokemonSpecies(StringTemplate *template, int idx, u16 species)
{
    u16 speciesName[TEMPLATE_NAME_SIZE];

    MessageLoader_GetSpeciesName(species, HEAP_ID_FIELD1, speciesName);
    TVSegment_SetTemplateString(template, idx, speciesName, 0, GAME_LANGUAGE, 1);
}

static void TVSegment_CopyPokemonNicknameIfSet(enum HeapID heapID, Pokemon *mon, u8 *param2, u16 *param3)
{
    *param2 = Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL);

    if (*param2) {
        String *string = String_Init(64, heapID);

        Pokemon_GetValue(mon, MON_DATA_NICKNAME_STRING, string);
        String_ToChars(string, param3, TEMPLATE_NAME_SIZE);
        String_Free(string);
    }
}

void TVBroadcast_SetContestHallShowInfo(TVBroadcast *broadcast, Pokemon *mon, enum PokemonContestType contestType, enum PokemonContestRank contestRank, int contestPlacement)
{
    TVSegment_ContestHall_ShowcasedPokemon *showcasedPokemon = TVBroadcast_GetShowcasedPokemon(broadcast);

    showcasedPokemon->unk_00 = 1;
    TVSegment_CopyPokemonValues(mon, &showcasedPokemon->species, &showcasedPokemon->gender, &showcasedPokemon->language, &showcasedPokemon->metGame);
    showcasedPokemon->contestType = contestType;
    showcasedPokemon->contestRank = contestRank;
    showcasedPokemon->contestPlacement = contestPlacement;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores the Pokémon that accompanied the player in Amity Square, clearing any
// previously found accessory/item.
void TVBroadcast_SetAmitySquareWatchInfo(TVBroadcast *broadcast, Pokemon *param1, enum HeapID heapID)
{
    TVSegment_AmitySquareWatchData *v0 = TVBroadcast_GetAmitySquareWatch(broadcast);

    v0->active = 1;
    v0->foundType = 0;
    v0->nature = Pokemon_GetNature(param1);

    TVSegment_CopyPokemonValues(param1, &v0->species, &v0->gender, &v0->language, &v0->metGame);
    v0->hasNickname = Pokemon_GetValue(param1, MON_DATA_HAS_NICKNAME, NULL);

    TVSegment_CopyPokemonNicknameIfSet(heapID, param1, &v0->hasNickname, v0->nickname);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Records that the Amity Square walk found a Contest accessory (foundType == 2).
void TVBroadcast_SetAmitySquareWatchFoundAccessory(TVBroadcast *broadcast, int param1)
{
    TVSegment_AmitySquareWatchData *v0 = TVBroadcast_GetAmitySquareWatch(broadcast);

    v0->foundType = 2;
    v0->foundAccessory = param1;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Records that the Amity Square walk found an item (foundType == 1).
void TVBroadcast_SetAmitySquareWatchFoundItem(TVBroadcast *broadcast, int param1)
{
    TVSegment_AmitySquareWatchData *v0 = TVBroadcast_GetAmitySquareWatch(broadcast);

    v0->foundType = 1;
    v0->foundItem = param1;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores the poffin type the player last made for the Three Cheers for Poffin
// Corner.
void TVBroadcast_SetPoffinCornerInfo(TVBroadcast *broadcast, int param1)
{
    TVSegment_ThreeCheersForPoffinCornerData *v0 = TVBroadcast_GetPoffinCorner(broadcast);

    v0->active = 1;
    v0->poffinType = param1;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores the outcome of the player's latest Battle Tower run.
void TVBroadcast_SetBattleTowerCornerInfo(TVBroadcast *broadcast, BOOL param1, u16 param2)
{
    TVSegment_BattleTowerCornerData *v0 = TVBroadcast_GetBattleTowerCorner(broadcast);

    v0->active = 1;
    v0->win = param1;
    v0->winStreak = param2;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_ResetSafariGameData(TVBroadcast *broadcast)
{
    TVSegment_SafariGameData *safariGame = TVBroadcast_GetSafariGameData(broadcast);

    safariGame->dummy = 1;
    safariGame->numPokemonCaught = 0;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_UpdateSafariGameData(TVBroadcast *broadcast, Pokemon *mon)
{
    TVSegment_SafariGameData *safariGame = TVBroadcast_GetSafariGameData(broadcast);

    if (safariGame->numPokemonCaught == 0) {
        TVSegment_CopyPokemonValues(mon, &safariGame->species, &safariGame->gender, &safariGame->language, &safariGame->metGame);
    }

    safariGame->numPokemonCaught++;
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores the Pokémon that won a Battle Frontier single-battle event for the
// Frontline News segment.
void TVBroadcast_SetBattleFrontierFrontlineNewsSingleInfo(TVBroadcast *broadcast, Pokemon *mon)
{
    TVSegment_BattleFrontierFrontlineNewsSingleData *v0 = TVBroadcast_GetFrontlineNewsSingle(broadcast);

    v0->active = 1;
    TVSegment_CopyPokemonValues(mon, &v0->species, &v0->gender, &v0->language, &v0->metGame);
    v0->hasNickname = Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL);

    TVSegment_CopyPokemonNicknameIfSet(HEAP_ID_FIELD2, mon, &v0->hasNickname, v0->nickname);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores the facility and partner trainer for the Frontline News multi-battle
// segment.
void TVBroadcast_SetBattleFrontierFrontlineNewsMultiInfo(TVBroadcast *broadcast, u8 param1, const TrainerInfo *param2)
{
    TVSegment_BattleFrontierFrontlineNewsMultiData *v0 = TVBroadcast_GetFrontlineNewsMulti(broadcast);

    v0->active = 1;
    v0->facility = param1;

    CharCode_Copy(v0->trainerName, TrainerInfo_Name(param2));

    v0->language = TrainerInfo_Language(param2);
    v0->gameCode = TrainerInfo_GameCode(param2);
    v0->gender = TrainerInfo_Gender(param2);

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Accumulates Battle Points earned today, capped at 9999.
void TVBroadcast_AddBattlePoints(TVBroadcast *broadcast, u16 param1)
{
    TVSegment_BattlePointsRecordData *v0 = TVBroadcast_GetBattlePointsRecord(broadcast);

    v0->active = 1;
    v0->battlePoints += param1;

    if (v0->battlePoints > 9999) {
        v0->battlePoints = 9999;
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_ResetBattlePoints(TVBroadcast *broadcast)
{
    TVSegment_BattlePointsRecordData *v0 = TVBroadcast_GetBattlePointsRecord(broadcast);

    v0->battlePoints = 0;
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Counts GTS trades made today, capped at 9999.
void TVBroadcast_IncrementGTSTradeCount(TVBroadcast *broadcast)
{
    TVSegment_GTSTradeRecordData *v0 = TVBroadcast_GetGTSTradeRecord(broadcast);

    v0->active = 1;
    v0->tradeCount++;

    if (v0->tradeCount > 9999) {
        v0->tradeCount = 9999;
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_ResetGTSTradeCount(TVBroadcast *broadcast)
{
    TVSegment_GTSTradeRecordData *v0 = TVBroadcast_GetGTSTradeRecord(broadcast);

    v0->tradeCount = 0;
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

CaptureAttempt *CaptureAttempt_New(enum HeapID heapID)
{
    CaptureAttempt *captureAttempt = Heap_Alloc(heapID, sizeof(CaptureAttempt));
    MI_CpuClearFast(captureAttempt, sizeof(CaptureAttempt));

    return captureAttempt;
}

void CaptureAttempt_Free(CaptureAttempt *captureAttempt)
{
    Heap_Free(captureAttempt);
}

void CaptureAttempt_Init(CaptureAttempt *captureAttempt, Pokemon *mon, int resultMask, int ballsThrown, enum HeapID heapID)
{
    MI_CpuClear32(captureAttempt, sizeof(CaptureAttempt));

    captureAttempt->resultMask = resultMask;
    captureAttempt->ballsThrown = ballsThrown;

    TVSegment_CopyPokemonValues(mon, &captureAttempt->species, &captureAttempt->gender, &captureAttempt->language, &captureAttempt->metGame);

    captureAttempt->pokeballItemID = Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL);
    GF_ASSERT(captureAttempt->pokeballItemID);

    TVSegment_CopyPokemonNicknameIfSet(heapID, mon, &captureAttempt->hasNickname, captureAttempt->nickname);
}

void FieldSystem_SaveTVSegment_CatchThatPokemonShow(FieldSystem *fieldSystem, const CaptureAttempt *captureAttempt, int resultMask)
{
    TVSegmentData segments;
    TVSegment_CatchThatPokemonShow *catchThatPokemonShow = &segments.catchThatPokemonShow;

    *catchThatPokemonShow = *captureAttempt;
    catchThatPokemonShow->resultMask = resultMask;

    if (catchThatPokemonShow->ballsThrown == 0) {
        return;
    }

    if (catchThatPokemonShow->ballsThrown > 999) {
        catchThatPokemonShow->ballsThrown = 999;
    }

    switch (resultMask) {
    case BATTLE_RESULT_CAPTURED_MON:
        FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_CATCH_THAT_POKEMON_SHOW_SUCCESS, catchThatPokemonShow);
        break;
    case BATTLE_RESULT_WIN:
    case BATTLE_RESULT_PLAYER_FLED:
    case BATTLE_RESULT_ENEMY_FLED:
        if (catchThatPokemonShow->ballsThrown > 2) {
            FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_CATCH_THAT_POKEMON_SHOW_FAILURE, catchThatPokemonShow);
        }
        break;
    }
}

static int TVSegment_LoadMessage_CatchThatPokemonShow_Success(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_CatchThatPokemonShow *catchThatPokemonShow = TVEpisode_GetSegment(episode);

    if (catchThatPokemonShow->hasNickname) {
        TVSegment_SetTemplateTrainerName(template, 0, episode);
        TVSegment_SetTemplatePokemonSpecies(template, 1, catchThatPokemonShow->species, catchThatPokemonShow->gender, catchThatPokemonShow->language, catchThatPokemonShow->metGame);
        StringTemplate_SetItemName(template, 2, catchThatPokemonShow->pokeballItemID);
        StringTemplate_SetNumber(template, 3, catchThatPokemonShow->ballsThrown, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);
        TVSegment_SetTemplateString(template, 4, catchThatPokemonShow->nickname, catchThatPokemonShow->gender, catchThatPokemonShow->language, 1);
        return TVProgramTrainerSightings_Text_CatchThatPokemonShow_Nicknamed;
    } else {
        TVSegment_SetTemplateTrainerName(template, 0, episode);
        TVSegment_SetTemplatePokemonSpecies(template, 1, catchThatPokemonShow->species, catchThatPokemonShow->gender, catchThatPokemonShow->language, catchThatPokemonShow->metGame);
        StringTemplate_SetItemName(template, 2, catchThatPokemonShow->pokeballItemID);
        StringTemplate_SetNumber(template, 3, catchThatPokemonShow->ballsThrown, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);
        return TVProgramTrainerSightings_Text_CatchThatPokemonShow_NotNicknamed;
    }
}

static int TVSegment_LoadMessage_CatchThatPokemonShow_Failure(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_CatchThatPokemonShow *catchThatPokemonShow = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetNumber(template, 1, catchThatPokemonShow->ballsThrown, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);

    if (catchThatPokemonShow->resultMask == BATTLE_RESULT_WIN) {
        return TVProgramTrainerSightings_Text_CatchThatPokemonShow_Fainted;
    } else {
        return TVProgramTrainerSightings_Text_CatchThatPokemonShow_RanAway;
    }
}

static BOOL TVSegment_IsEligible_CatchThatPokemonShow_Success(FieldSystem *fieldSystem, TVEpisode *episode)
{
    Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);
    TVSegment_CatchThatPokemonShow *catchThatPokemonShow = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(pokedex, catchThatPokemonShow->species);
}

void FieldSystem_SaveTVSegment_WhatsFishing(FieldSystem *fieldSystem, BOOL caughtFish, u16 fishingRodItemID, Pokemon *mon)
{
    TVSegmentData segments;
    TVSegment_WhatsFishing *whatsFishing = &segments.whatsFishing;

    if (caughtFish) {
        TVSegment_CopyPokemonValues(mon, &whatsFishing->species, &whatsFishing->gender, &whatsFishing->language, &whatsFishing->metGame);
    }

    whatsFishing->fishingRodItemID = fishingRodItemID;
    whatsFishing->caughtFish = caughtFish;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_WHATS_FISHING, whatsFishing);
}

static int TVSegment_LoadMessage_WhatsFishing(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_WhatsFishing *whatsFishing = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);

    if (whatsFishing->caughtFish) {
        StringTemplate_SetItemName(template, 1, whatsFishing->fishingRodItemID);
        TVSegment_SetTemplatePokemonSpecies(template, 2, whatsFishing->species, whatsFishing->gender, whatsFishing->language, whatsFishing->metGame);
        return TVProgramTrainerSightings_Text_WhatsFishing_Success;
    } else {
        return TVProgramTrainerSightings_Text_WhatsFishing_Failure;
    }
}

static BOOL TVSegment_IsEligible_WhatsFishing(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_WhatsFishing *whatsFishing = TVEpisode_GetSegment(episode);

    if (whatsFishing->caughtFish == FALSE) {
        return TRUE;
    }

    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), whatsFishing->species);
}

static void FieldSystem_SaveTVSegment_LoveThatGroupCorner(FieldSystem *fieldSystem, int segmentID)
{
    TVSegmentData segments;
    TVSegment_LoveThatGroupCorner *loveThatGroupCorner = &segments.loveThatGroupCorner;
    RecordMixedRNG *rngCollection = SaveData_GetRecordMixedRNG(fieldSystem->saveData);

    GF_ASSERT(sizeof(TVSegmentData) == TV_EPISODE_SEGMENT_SIZE);
    MI_CpuClearFast(&segments, TV_EPISODE_SEGMENT_SIZE);

    CharCode_CopyNumChars(loveThatGroupCorner->groupName, RecordMixedRNG_GetEntryName(rngCollection, RECORD_MIXED_RNG_PLAYER_OVERRIDE, RECORD_MIXED_RNG_GROUP_NAME), TEMPLATE_NAME_SIZE);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, segmentID, loveThatGroupCorner);
}

void FieldSystem_SaveTVSegment_LoveThatGroupCorner_NewGroup(FieldSystem *fieldSystem)
{
    FieldSystem_SaveTVSegment_LoveThatGroupCorner(fieldSystem, TV_PROGRAM_SEGMENT_LOVE_THAT_GROUP_CORNER_NEW_GROUP);
}

void FieldSystem_SaveTVSegment_LoveThatGroupCorner_SwitchGroup(FieldSystem *fieldSystem)
{
    FieldSystem_SaveTVSegment_LoveThatGroupCorner(fieldSystem, TV_PROGRAM_SEGMENT_LOVE_THAT_GROUP_CORNER_SWITCH_GROUP);
}

static int TVSegment_LoadMessage_LoveThatGroupCorner_SwitchGroup(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_LoveThatGroupCorner *loveThatGroupCorner = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateString(template, 1, loveThatGroupCorner->groupName, 0, TVEpisode_GetLanguage(episode), 1);
    TVSegment_SetTemplateTrainerName(template, 0, episode);

    return TVProgramTrainerSightings_Text_LoveThatGroupCorner_SwitchGroup;
}

static int TVSegment_LoadMessage_LoveThatGroupCorner_NewGroup(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_LoveThatGroupCorner *loveThatGroupCorner = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateString(template, 1, loveThatGroupCorner->groupName, 0, TVEpisode_GetLanguage(episode), 1);
    TVSegment_SetTemplateTrainerName(template, 0, episode);

    return TVProgramTrainerSightings_Text_LoveThatGroupCorner_NewGroup;
}

void FieldSystem_SaveTVSegment_HiddenItemBreakingNews(FieldSystem *fieldSystem, u16 item)
{
    TVSegmentData segments;
    TVSegment_HiddenItemBreakingNews *hiddenItemBreakingNews = &segments.hiddenItemBreakingNews;

    hiddenItemBreakingNews->item = item;
    hiddenItemBreakingNews->location = MapHeader_GetMapLabelTextID(fieldSystem->location->mapHeaderID);

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_HIDDEN_ITEM_BREAKING_NEWS, hiddenItemBreakingNews);
}

static int TVSegment_LoadMessage_HiddenItemBreakingNews(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_HiddenItemBreakingNews *hiddenItemBreakingNews = TVEpisode_GetSegment(episode);

    StringTemplate_SetLocationName(template, 0, hiddenItemBreakingNews->location);
    TVSegment_SetTemplateTrainerName(template, 1, episode);
    StringTemplate_SetItemName(template, 2, hiddenItemBreakingNews->item);

    return TVProgramTrainerSightings_Text_HiddenItemBreakingNews;
}

void FieldSystem_SaveTVSegment_SinnohShoppingChampCorner(SaveData *saveData, u16 item, u8 amount)
{
    TVSegmentData segments;
    TVSegment_SinnohShoppingChampCorner *sinnohShoppingChampCorner = &segments.sinnohShoppingChampCorner;

    if (amount >= 10) {
        sinnohShoppingChampCorner->item = item;
        sinnohShoppingChampCorner->amount = amount;

        if (sinnohShoppingChampCorner->amount > 999) {
            sinnohShoppingChampCorner->amount = 999;
        }

        SaveData_SaveTVSegment(saveData, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_SINNOH_SHOPPING_CHAMP_CORNER, sinnohShoppingChampCorner);
    }
}

static int TVSegment_LoadMessage_SinnohShoppingChampCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_SinnohShoppingChampCorner *sinnohShoppingChampCorner = TVEpisode_GetSegment(episode);

    StringTemplate_SetItemName(template, 0, sinnohShoppingChampCorner->item);
    StringTemplate_SetNumber(template, 1, sinnohShoppingChampCorner->amount, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);
    TVSegment_SetTemplateTrainerName(template, 2, episode);
    StringTemplate_SetItemNamePlural(template, 3, sinnohShoppingChampCorner->item);

    return TVProgramTrainerSightings_Text_SinnohShoppingChampCorner;
}

void FieldSystem_SaveTVSegment_HappyHappyEggClub(FieldSystem *fieldSystem, Pokemon *mon)
{
    TVSegmentData segments;
    TVSegment_HappyHappyEggClub *happyHappyEggClub = &segments.happyHappyEggClub;

    TVSegment_CopyPokemonValues(mon, &happyHappyEggClub->species, &happyHappyEggClub->gender, &happyHappyEggClub->language, &happyHappyEggClub->metGame);
    happyHappyEggClub->location = MapHeader_GetMapLabelTextID(fieldSystem->location->mapHeaderID);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_HAPPY_HAPPY_EGG_CLUB, happyHappyEggClub);
}

static int TVSegment_LoadMessage_HappyHappyEggClub(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_HappyHappyEggClub *happyHappyEggClub = TVEpisode_GetSegment(episode);

    StringTemplate_SetLocationName(template, 1, happyHappyEggClub->location);
    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 2, happyHappyEggClub->species, happyHappyEggClub->gender, happyHappyEggClub->language, happyHappyEggClub->metGame);

    return TVProgramTrainerSightings_Text_HappyHappyEggClub;
}

static BOOL TVSegment_IsEligible_HappyHappyEggClub(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_HappyHappyEggClub *happyHappyEggClub = TVEpisode_GetSegment(episode);
    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), happyHappyEggClub->species);
}

void FieldSystem_SaveTVSegment_RateThatNameChange(FieldSystem *fieldSystem, Pokemon *mon)
{
    TVSegmentData segments;
    TVSegment_RateThatNameChange *rateThatNameChange = &segments.rateThatNameChange;

    TVSegment_CopyPokemonValues(mon, &rateThatNameChange->species, &rateThatNameChange->gender, &rateThatNameChange->language, &rateThatNameChange->metGame);
    TVSegment_CopyPokemonNickname(HEAP_ID_FIELD1, rateThatNameChange->nickname, mon);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_RATE_THAT_NAME_CHANGE, rateThatNameChange);
}

static int TVSegment_LoadMessage_RateThatNameChange(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_RateThatNameChange *rateThatNameChange = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 1, rateThatNameChange->species, rateThatNameChange->gender, rateThatNameChange->language, rateThatNameChange->metGame);
    TVSegment_SetTemplateString(template, 2, rateThatNameChange->nickname, rateThatNameChange->gender, rateThatNameChange->language, 1);

    return TVProgramTrainerSightings_Text_RateThatNameChange_MoreAttractive + LCRNG_RandMod(5);
}

static BOOL TVSegment_IsEligible_RateThatNameChange(FieldSystem *fieldSystem, TVEpisode *episode)
{
    Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);
    TVSegment_RateThatNameChange *rateThatNameChange = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(pokedex, rateThatNameChange->species);
}

void FieldSystem_SaveTVSegment_UndergroundTreasuresCorner(FieldSystem *fieldSystem, int item, int amount)
{
    TVSegmentData segments;
    TVSegment_UndergroundTreasuresCorner *undergroundTreasuresCorner = &segments.undergroundTreasuresCorner;

    undergroundTreasuresCorner->item = item;
    undergroundTreasuresCorner->amount = amount;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_UNDERGROUND_TREASURES_CORNER, undergroundTreasuresCorner);
}

static int TVSegment_LoadMessage_UndergroundTreasuresCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_UndergroundTreasuresCorner *undergroundTreasuresCorner = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetUndergroundItemName(template, 1, undergroundTreasuresCorner->item);
    StringTemplate_SetNumber(template, 2, undergroundTreasuresCorner->amount, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return TVProgramTrainerSightings_Text_UndergroundTreasuresCorner;
}

void FieldSystem_SaveTVSegment_SafariGameSpecialNewsBulletin(FieldSystem *fieldSystem)
{
    TVSegmentData segments;
    TVSegment_SafariGameSpecialNewsBulletin *safariGameSpecialNewsBulletin = &segments.safariGameSpecialNewsBulletin;
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    TVSegment_SafariGameData *safariGame = TVBroadcast_GetSafariGameData(broadcast);

    if (safariGame->numPokemonCaught == 0) {
        return;
    }

    safariGameSpecialNewsBulletin->species = safariGame->species;
    safariGameSpecialNewsBulletin->gender = safariGame->gender;
    safariGameSpecialNewsBulletin->language = safariGame->language;
    safariGameSpecialNewsBulletin->metGame = safariGame->metGame;
    safariGameSpecialNewsBulletin->numPokemonCaught = safariGame->numPokemonCaught;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_SAFAR_GAME_SPECIAL_NEWS_BULLETIN, safariGameSpecialNewsBulletin);
}

static int TVSegment_LoadMessage_SafariGameSpecialNewsBulletin(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_SafariGameSpecialNewsBulletin *safariGameSpecialNewsBulletin = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 1, safariGameSpecialNewsBulletin->species, safariGameSpecialNewsBulletin->gender, safariGameSpecialNewsBulletin->language, safariGameSpecialNewsBulletin->metGame);
    StringTemplate_SetNumber(template, 2, safariGameSpecialNewsBulletin->numPokemonCaught, 2, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return TVProgramTrainerSightings_Text_SafariGameSpecialNewsBulletin;
}

static BOOL TVSegment_IsEligible_SafariGameSpecialNewsBulletin(FieldSystem *fieldSystem, TVEpisode *episode)
{
    Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);
    TVSegment_SafariGameSpecialNewsBulletin *safariGameSpecialNewsBulletin = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(pokedex, safariGameSpecialNewsBulletin->species);
}

void FieldSystem_SaveTVSegment_PokemonStorageSpecialNewsBulletin(FieldSystem *fieldSystem)
{
    u32 hasMale, hasFemale, hasGenderless, gender;
    u8 partyCount, partyIndex;
    Pokemon *mon;
    TVSegmentData segments;
    Party *party;
    TVSegment_PokemonStorageSpecialNewsBulletin *pokemonStorageSpecialNewsBulletin = &segments.pokemonStorageSpecialNewsBulletin;

    hasMale = FALSE;
    hasFemale = FALSE;
    hasGenderless = FALSE;
    party = SaveData_GetParty(fieldSystem->saveData);
    partyCount = Party_GetCurrentCount(party);

    for (partyIndex = 0; partyIndex < partyCount; partyIndex++) {
        mon = Party_GetPokemonBySlotIndex(party, partyIndex);

        if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) == FALSE) {
            gender = Pokemon_GetValue(mon, MON_DATA_GENDER, NULL);

            if (gender == GENDER_MALE) {
                hasMale = TRUE;
            } else if (gender == GENDER_FEMALE) {
                hasFemale = TRUE;
            } else if (gender == GENDER_NONE) {
                hasGenderless = TRUE;
            }
        }
    }

    if (hasGenderless == FALSE) {
        if (hasMale == TRUE && hasFemale == FALSE) {
            mon = Party_FindFirstHatchedMon(SaveData_GetParty(fieldSystem->saveData));
            TVSegment_CopyPokemonValues(mon, &pokemonStorageSpecialNewsBulletin->species, &pokemonStorageSpecialNewsBulletin->gender, &pokemonStorageSpecialNewsBulletin->language, &pokemonStorageSpecialNewsBulletin->metGame);
            FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_POKEMON_STORAGE_SPECIAL_NEWS_BULLETIN, pokemonStorageSpecialNewsBulletin);
        } else if (hasMale == FALSE && hasFemale == TRUE) {
            mon = Party_FindFirstHatchedMon(SaveData_GetParty(fieldSystem->saveData));
            TVSegment_CopyPokemonValues(mon, &pokemonStorageSpecialNewsBulletin->species, &pokemonStorageSpecialNewsBulletin->gender, &pokemonStorageSpecialNewsBulletin->language, &pokemonStorageSpecialNewsBulletin->metGame);
            FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_POKEMON_STORAGE_SPECIAL_NEWS_BULLETIN, pokemonStorageSpecialNewsBulletin);
        }
    }

    return;
}

static int TVSegment_LoadMessage_PokemonStorageSpecialNewsBulletin(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_PokemonStorageSpecialNewsBulletin *pokemonStorageSpecialNewsBulletin = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 1, pokemonStorageSpecialNewsBulletin->species, pokemonStorageSpecialNewsBulletin->gender, pokemonStorageSpecialNewsBulletin->language, pokemonStorageSpecialNewsBulletin->metGame);

    if (pokemonStorageSpecialNewsBulletin->gender == GENDER_MALE) {
        return TVProgramTrainerSightings_Text_PokemonStorageSpecialNewsBulletin_Male;
    }

    return TVProgramTrainerSightings_Text_PokemonStorageSpecialNewsBulletin_Female;
}

static BOOL TVSegment_IsEligible_PokemonStorageSpecialNewsBulletin(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_PokemonStorageSpecialNewsBulletin *pokemonStorageSpecialNewsBulletin = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), pokemonStorageSpecialNewsBulletin->species);
}

void FieldSystem_SaveTVSegment_HerbalMedicineTrainerSightingDummy(TVBroadcast *broadcast, Pokemon *mon, u16 item)
{
    return;
}

static BOOL TVSegment_IsEligible_HerbalMedicineTrainerSightingDummy(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return FALSE;
}

void FieldSystem_SaveTVSegment_PlantingAndWateringShow(FieldSystem *fieldSystem, u16 berryItemID, u8 yieldRating, u16 yieldAmount)
{
    TVSegmentData segments;
    TVSegment_PlantingAndWateringShow *plantingAndWateringShow = &segments.plantingAndWateringShow;

    plantingAndWateringShow->berryItemID = berryItemID;
    plantingAndWateringShow->yieldRating = yieldRating;
    plantingAndWateringShow->yieldAmount = yieldAmount;

    if (yieldRating == 5) {
        (void)0;
    } else if (yieldRating == 4) {
        FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_PLANTING_AND_WATERING_SHOW, plantingAndWateringShow);
    } else if (yieldRating == 0) {
        FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_PLANTING_AND_WATERING_SHOW_NO_BERRIES, plantingAndWateringShow);
    }
}

static int TVSegment_LoadMessage_PlantingAndWateringShow(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_PlantingAndWateringShow *plantingAndWateringShow = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetItemName(template, 1, plantingAndWateringShow->berryItemID);
    StringTemplate_SetNumber(template, 2, plantingAndWateringShow->yieldAmount, 2, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return TVProgramTrainerSightings_Text_PlantingAndWateringShow;
}

static int TVSegment_LoadMessage_PlantingAndWateringShow_NoBerries(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_PlantingAndWateringShow *plantingAndWateringShow = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetItemName(template, 1, plantingAndWateringShow->berryItemID);

    return TVProgramTrainerSightings_Text_PlantingAndWateringShow_NoBerries;
}

void FieldSystem_SaveTVSegment_SealClubShow(TVBroadcast *broadcast, Pokemon *param1, u8 ballSeal)
{
    TVSegmentData segments;
    TVSegment_SealClubShow *sealClubShow = &segments.sealClubShow;

    sealClubShow->ballSeal = ballSeal;
    sealClubShow->dummy = MTRNG_Next() % 3;

    TVSegment_CopyPokemonValues(param1, &sealClubShow->species, &sealClubShow->gender, &sealClubShow->language, &sealClubShow->metGame);
    TVBroadcast_SaveSegmentData(broadcast, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_SEAL_CLUB_SHOW, (const u8 *)sealClubShow);
}

static int TVSegment_LoadMessage_SealClubShow(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_SealClubShow *sealClubShow = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetBallSealName(template, 1, sealClubShow->ballSeal);
    TVSegment_SetTemplatePokemonSpecies(template, 2, sealClubShow->species, sealClubShow->gender, sealClubShow->language, sealClubShow->metGame);

    return TVProgramTrainerSightings_Text_SealClubShow_CleverlyCalculated + LCRNG_RandMod(3);
}

static BOOL TVSegment_IsEligible_SealClubShow(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_SealClubShow *sealClubShow = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), sealClubShow->species);
}

static void TVSegment_CopyTrainerInfo(TVSegment_CaptureTheFlagDigest *captureTheFlagDigest, const TrainerInfo *param1)
{
    captureTheFlagDigest->trainerInfoSize = TrainerInfo_Size();
    TrainerInfo_Copy(param1, (TrainerInfo *)captureTheFlagDigest->trainerInfo);
}

static void TVSegment_LoadMessage_CaptureTheFlagDigest(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_CaptureTheFlagDigest *captureTheFlagDigest = TVEpisode_GetSegment(episode);
    TrainerInfo *trainerInfo = (TrainerInfo *)&captureTheFlagDigest->trainerInfo;

    GF_ASSERT(TrainerInfo_Size() == captureTheFlagDigest->trainerInfoSize);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetPlayerName(template, 1, trainerInfo);
}

void FieldSystem_SaveTVSegment_CaptureTheFlagDigest_TakeFlag(FieldSystem *fieldSystem, const TrainerInfo *trainerInfo)
{
    TVSegmentData segments;
    TVSegment_CaptureTheFlagDigest *captureTheFlagDigest = &segments.captureTheFlagDigest;

    TVSegment_CopyTrainerInfo(captureTheFlagDigest, trainerInfo);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_CAPTURE_THE_FLAG_DIGEST_TAKE_FLAG, captureTheFlagDigest);
}

void FieldSystem_SaveTVSegment_CaptureTheFlagDigest_LoseFlag(FieldSystem *fieldSystem, const TrainerInfo *trainerInfo)
{
    TVSegmentData segments;
    TVSegment_CaptureTheFlagDigest *captureTheFlagDigest = &segments.captureTheFlagDigest;

    TVSegment_CopyTrainerInfo(captureTheFlagDigest, trainerInfo);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_CAPTURE_THE_FLAG_DIGEST_LOSE_FLAG, captureTheFlagDigest);
}

static int TVSegment_LoadMessage_CaptureTheFlagDigest_TakeFlag(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_LoadMessage_CaptureTheFlagDigest(fieldSystem, template, episode);
    return TVProgramTrainerSightings_Text_CaptureTheFlagDigest_TakeFlag;
}

static int TVSegment_LoadMessage_CaptureTheFlagDigest_LoseFlag(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_LoadMessage_CaptureTheFlagDigest(fieldSystem, template, episode);
    return TVProgramTrainerSightings_Text_CaptureTheFlagDigest_LoseFlag;
}

static BOOL TVSegment_IsEligible_HasExplorerKit(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return Bag_CanRemoveItem(SaveData_GetBag(fieldSystem->saveData), ITEM_EXPLORER_KIT, 1, HEAP_ID_FIELD3);
}

void FieldSystem_SaveTVSegment_HomeAndManor_NoFurniture(FieldSystem *fieldSystem)
{
    TVSegmentData segments;
    TVSegment_HomeAndManor_NoFurniture *homeAndManorNoFurniture = &segments.homeAndManorNoFurniture;

    homeAndManorNoFurniture->dummy = 1;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_HOME_AND_MANOR_NO_FURNITURE, homeAndManorNoFurniture);
}

static int TVSegment_LoadMessage_HomeAndManor_NoFurniture(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_SetTemplateTrainerName(template, 0, episode);
    return TVProgramTrainerSightings_Text_HomeAndManor_NoFurniture;
}

static BOOL TVSegment_IsEligible_HomeAndManor_NoFurniture(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_RESORT_AREA);
}

void FieldSystem_SaveTVSegment_HomeAndManor(FieldSystem *fieldSystem, u8 furniture)
{
    TVSegmentData segments;
    TVSegment_HomeAndManor *homeAndManor = &segments.homeAndManor;

    homeAndManor->furniture = furniture;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_TRAINER_SIGHTINGS, TV_PROGRAM_SEGMENT_HOME_AND_MANOR, homeAndManor);
}

static int TVSegment_LoadMessage_HomeAndManor(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_HomeAndManor *homeAndManor = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetFurniture(template, 1, homeAndManor->furniture);

    return TVProgramTrainerSightings_Text_HomeAndManor;
}

static BOOL TVSegment_IsEligible_HomeAndManor(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_RESORT_AREA);
}

void TVSegment_SaveBattleTowerStreakRecord(SaveData *saveData, u32 param1, Pokemon *param2, BOOL param3)
{
    TVSegmentData segments;
    TVSegment_BattleTowerStreakRecord *v1 = &segments.battleTowerStreakRecord;

    TVSegment_CopyPokemonValues(param2, &v1->species, &v1->gender, &v1->language, &v1->metGame);

    v1->streak = param1;
    v1->isSingleBattle = param3;

    SaveData_SaveTVSegmentDuplicate(saveData, 3, 1, v1);
}

static int TVSegment_LoadMessage_BattleTowerStreakRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_BattleTowerStreakRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(param1, 1, v0->species, v0->gender, v0->language, v0->metGame);
    StringTemplate_SetNumber(param1, 2, v0->streak, 4, PADDING_MODE_NONE, CHARSET_MODE_EN);

    if (v0->isSingleBattle) {
        return 0;
    } else {
        return 1;
    }
}

static BOOL TVSegment_IsEligible_BattleTowerStreakRecord(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_BattleTowerStreakRecord *v0 = TVEpisode_GetSegment(episode);

    if (Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), v0->species) == 0) {
        return 0;
    }

    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_FIGHT_AREA);
}

void TVSegment_SaveSizeRecord(FieldSystem *fieldSystem, u32 param1, Pokemon *param2)
{
    TVSegmentData segments;
    TVSegment_SizeRecord *v1 = &segments.sizeRecord;

    TVSegment_CopyPokemonValues(param2, &v1->species, &v1->gender, &v1->language, &v1->metGame);
    v1->size = param1;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_RECORDS, 3, v1);
}

static int TVSegment_LoadMessage_SizeRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_SizeRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(param1, 1, v0->species, v0->gender, v0->language, v0->metGame);

    {
        // Convert the stored millimeter size to inches, rounded to tenths.
        u32 v1 = (((v0->size * 1000) / 254 + 5) / 10);

        StringTemplate_SetNumber(param1, 2, v1 / 10, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);
        StringTemplate_SetNumber(param1, 3, v1 % 10, 1, PADDING_MODE_NONE, CHARSET_MODE_EN);
    }

    return 2;
}

static BOOL TVSegment_IsEligible_SizeRecord(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_SizeRecord *v0 = TVEpisode_GetSegment(episode);
    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), v0->species);
}

void TVSegment_SaveSlotMachineRecord(FieldSystem *fieldSystem, u32 param1, u32 param2, u32 param3)
{
    TVSegmentData segments;
    TVSegment_SlotMachineRecord *v1 = &segments.slotMachineRecord;

    // Only report a session where the player came out at least 1000 Coins ahead.
    if (param2 < 1000 + param1) {
        return;
    }

    v1->coinsBefore = param1;
    v1->coinsAfter = param2;
    v1->minutesPlayed = param3;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_RECORDS, 4, v1);
}

static int TVSegment_LoadMessage_SlotMachineRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_SlotMachineRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    StringTemplate_SetNumber(param1, 1, v0->minutesPlayed, 10, PADDING_MODE_NONE, CHARSET_MODE_EN);
    StringTemplate_SetNumber(param1, 2, v0->coinsBefore, 6, PADDING_MODE_NONE, CHARSET_MODE_EN);
    StringTemplate_SetNumber(param1, 3, v0->coinsAfter, 6, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return 3;
}

void TVSegment_SaveRibbonRecord(SaveData *saveData, Pokemon *mon, u32 monDataParam)
{
    u8 v0, v1;
    TVSegmentData segments;
    TVSegment_RibbonRecord *v3 = &segments.ribbonRecord;

    v1 = TVSegment_CountRibbons(mon);

    // Only report at every fifth Ribbon milestone.
    switch (v1) {
    case 15:
    case 20:
    case 25:
    case 30:
    case 35:
    case 40:
        if (Ribbon_MonDataParamToNameID(monDataParam) > 0xff) {
            GF_ASSERT(FALSE);
            return;
        }

        TVSegment_CopyPokemonValues(mon, &v3->species, &v3->gender, &v3->language, &v3->metGame);
        TVSegment_CopyPokemonNicknameIfSet(HEAP_ID_FIELD3, mon, &v3->hasNickname, v3->nickname);

        v3->ribbonNameID = Ribbon_MonDataParamToNameID(monDataParam);
        v3->ribbonCount = v1;

        SaveData_SaveTVSegment(saveData, 3, 5, v3);
        break;
    }
}

static const u16 sRibbonMonDataParams[] = {
    0x66,
    0x19,
    0x7B,
    0x7C,
    0x7D,
    0x7E,
    0x7F,
    0x80,
    0x81,
    0x82,
    0x83,
    0x84,
    0x85,
    0x86,
    0x87,
    0x88,
    0x89,
    0x8A,
    0x8B,
    0x8C,
    0x8D,
    0x8E,
    0x1A,
    0x1B,
    0x1C,
    0x1D,
    0x1E,
    0x1F,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x2A
};

// Counts how many of the Ribbon mon-data flags in sRibbonMonDataParams are set.
static u8 TVSegment_CountRibbons(Pokemon *param0)
{
    u8 v0 = 0, v1;

    for (v1 = 0; v1 < (NELEMS(sRibbonMonDataParams)); v1++) {
        if (Pokemon_GetValue(param0, sRibbonMonDataParams[v1], NULL) == 1) {
            v0++;
        }
    }

    return v0;
}

static int TVSegment_LoadMessage_RibbonRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_RibbonRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);

    if (v0->hasNickname) {
        TVSegment_SetTemplateString(param1, 1, v0->nickname, v0->gender, v0->language, 1);
    } else {
        TVSegment_SetTemplatePokemonSpecies(param1, 1, v0->species, v0->gender, v0->language, v0->metGame);
    }

    StringTemplate_SetRibbonName(param1, 2, v0->ribbonNameID);
    StringTemplate_SetNumber(param1, 3, v0->ribbonCount, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return 4;
}

void TVSegment_SaveTrapRecord(FieldSystem *fieldSystem, u16 param1, u16 param2)
{
    TVSegmentData segments;
    TVSegment_TrapRecord *v1 = &segments.trapRecord;

    v1->trapID = param1;
    v1->count = param2;

    if (v1->count > 999) {
        v1->count = 999;
    }

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_RECORDS, 8, v1);
}

static int TVSegment_LoadMessage_TrapRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0;
    TVSegment_TrapRecord *v1 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    StringTemplate_SetUndergroundTrapName(param1, 1, v1->trapID);

    v0 = v1->count;

    if (v0 > 999) {
        v0 = 999;
    }

    StringTemplate_SetNumber(param1, 2, v0, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);
    return 7;
}

void TVSegment_SaveCaptureTheFlagRecord(FieldSystem *fieldSystem, u16 param1)
{
    TVSegmentData segments;
    TVSegment_CaptureTheFlagRecord *v1 = &segments.captureTheFlagRecord;

    v1->count = param1;

    if (v1->count > 999) {
        v1->count = 999;
    }

    if (param1 > 1) {
        FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_RECORDS, 9, v1);
    }
}

static int TVSegment_LoadMessage_CaptureTheFlagRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0;
    TVSegment_CaptureTheFlagRecord *v1 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);

    v0 = v1->count;

    if (v0 > 999) {
        v0 = 999;
    }

    StringTemplate_SetNumber(param1, 1, v0, 3, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return 8;
}

static BOOL TVSegment_IsEligible_HasExplorerKitForRecords(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return Bag_CanRemoveItem(SaveData_GetBag(fieldSystem->saveData), ITEM_EXPLORER_KIT, 1, HEAP_ID_FIELD3);
}

void TVSegment_SaveBattlePointsRecord(SaveData *saveData)
{
    TVSegmentData segments;
    TVSegment_BattlePointsRecord *v1 = &segments.battlePointsRecord;
    TVSegment_BattlePointsRecordData *v2 = TVBroadcast_GetBattlePointsRecord(SaveData_GetTVBroadcast(saveData));

    if (v2->battlePoints >= 30) {
        v1->data = *v2;
        v2->active = 0;

        SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
        SaveData_SaveTVSegment(saveData, 3, 10, v1);
    }
}

static int TVSegment_LoadMessage_BattlePointsRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_BattlePointsRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    StringTemplate_SetNumber(param1, 1, v0->data.battlePoints, 4, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return 9;
}

static BOOL TVSegment_IsEligible_BattlePointsRecord(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_FIGHT_AREA);
}

void TVSegment_SaveGTSTradeRecord(SaveData *saveData)
{
    TVSegmentData segments;
    TVSegment_GTSTradeRecord *v1 = &segments.gtsTradeRecord;
    TVSegment_GTSTradeRecordData *v2 = TVBroadcast_GetGTSTradeRecord(SaveData_GetTVBroadcast(saveData));

    if (v2->tradeCount >= 10) {
        v1->data = *v2;
        v2->active = 0;

        SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
        SaveData_SaveTVSegment(saveData, 3, 11, v1);
    }
}

static int TVSegment_LoadMessage_GTSTradeRecord(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    TVSegment_GTSTradeRecord *v0 = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(param1, 0, episode);
    StringTemplate_SetNumber(param1, 1, v0->data.tradeCount, 4, PADDING_MODE_NONE, CHARSET_MODE_EN);

    return 10;
}

static BOOL TVSegment_IsEligible_GTSTradeRecord(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_OREBURGH_CITY);
}

void FieldSystem_SaveTVSegment_BattleTowerCorner(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_BattleTowerCorner *battleTowerCorner = &segments.battleTowerCorner;
    TVSegment_BattleTowerCornerData *outcome = TVBroadcast_GetBattleTowerCorner(SaveData_GetTVBroadcast(fieldSystem->saveData));

    battleTowerCorner->outcome = *outcome;
    outcome->active = 0;
    battleTowerCorner->customMessageWord = customMessageWord;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_BATTLE_TOWER_CORNER, battleTowerCorner);
}

static int TVSegment_LoadMessage_BattleTowerCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_BattleTowerCorner *battleTowerCorner = TVEpisode_GetSegment(episode);

    StringTemplate_SetNumber(template, 0, battleTowerCorner->outcome.winStreak, 4, PADDING_MODE_NONE, CHARSET_MODE_EN);
    TVSegment_SetTemplateTrainerName(template, 1, episode);
    StringTemplate_SetEasyChatWord(template, 2, battleTowerCorner->customMessageWord);

    if (battleTowerCorner->outcome.win == TRUE) {
        return TVProgramInterviews_Text_BattleTowerCorner_Win;
    } else {
        return TVProgramInterviews_Text_BattleTowerCorner_Lose;
    }
}

static BOOL TVSegment_IsEligible_BattleTowerCorner(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_FIGHT_AREA);
}

void FieldSystem_SaveTVSegment_YourPokemonCorner(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_YourPokemonCorner *yourPokemonCorner = &segments.yourPokemonCorner;
    Pokemon *mon = Party_FindFirstHatchedMon(SaveData_GetParty(fieldSystem->saveData));

    TVSegment_CopyPokemonValues(mon, &yourPokemonCorner->species, &yourPokemonCorner->gender, &yourPokemonCorner->language, &yourPokemonCorner->metGame);
    TVSegment_CopyPokemonNicknameIfSet(HEAP_ID_FIELD3, mon, &yourPokemonCorner->hasNickname, yourPokemonCorner->nickname);

    yourPokemonCorner->customMessageWord = customMessageWord;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_YOUR_POKEMON_CORNER, yourPokemonCorner);
}

static int TVSegment_LoadMessage_YourPokemonCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_YourPokemonCorner *yourPokemonCorner = TVEpisode_GetSegment(episode);

    if (yourPokemonCorner->hasNickname) {
        TVSegment_SetTemplateTrainerName(template, 0, episode);
        TVSegment_SetTemplatePokemonSpecies(template, 1, yourPokemonCorner->species, yourPokemonCorner->gender, yourPokemonCorner->language, yourPokemonCorner->metGame);
        TVSegment_SetTemplateString(template, 2, yourPokemonCorner->nickname, yourPokemonCorner->gender, yourPokemonCorner->language, 1);
        StringTemplate_SetEasyChatWord(template, 3, yourPokemonCorner->customMessageWord);
        return TVProgramInterviews_Text_YourPokemonCorner_Nickname;
    } else {
        TVSegment_SetTemplateTrainerName(template, 0, episode);
        TVSegment_SetTemplatePokemonSpecies(template, 1, yourPokemonCorner->species, yourPokemonCorner->gender, yourPokemonCorner->language, yourPokemonCorner->metGame);

        StringTemplate_SetEasyChatWord(template, 3, yourPokemonCorner->customMessageWord);
        return TVProgramInterviews_Text_YourPokemonCorner_NoNickname;
    }
}

static BOOL TVSegment_IsEligible_YourPokemonCorner(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_YourPokemonCorner *yourPokemonCorner = TVEpisode_GetSegment(episode);

    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), yourPokemonCorner->species);
}

void FieldSystem_SaveTVSegment_ThePoketchWatch(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_ThePoketchWatch *thePoketchWatch = &segments.thePoketchWatch;

    thePoketchWatch->appID = PoketchSystem_CurrentAppID(fieldSystem->unk_04->poketchSys);
    thePoketchWatch->customMessageWord = customMessageWord;

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_THE_POKETCH_WATCH, thePoketchWatch);
}

static int TVSegment_LoadMessage_ThePoketchWatch(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_ThePoketchWatch *thePoketchWatch = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetPoketchAppName(template, 1, thePoketchWatch->appID);
    StringTemplate_SetEasyChatWord(template, 2, thePoketchWatch->customMessageWord);

    return TVProgramInterviews_Text_ThePoketchWatch;
}

void FieldSystem_SaveTVSegment_ContestHall(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_ContestHall *contestHall = &segments.contestHall;
    TVSegment_ContestHall_ShowcasedPokemon *showcasedPokemon = TVBroadcast_GetShowcasedPokemon(SaveData_GetTVBroadcast(fieldSystem->saveData));

    contestHall->showcasedPokemon = *showcasedPokemon;
    showcasedPokemon->unk_00 = 0;
    contestHall->customMessageWord = customMessageWord;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_CONTEST_HALL, contestHall);
}

static int TVSegment_LoadMessage_ContestHall(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_ContestHall *contestHall = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplatePokemonSpecies(template, 0, contestHall->showcasedPokemon.species, contestHall->showcasedPokemon.gender, contestHall->showcasedPokemon.language, contestHall->showcasedPokemon.metGame);
    StringTemplate_SetContestTypeName(template, 1, Contest_GetContestTypeMessageID(contestHall->showcasedPokemon.contestType));
    StringTemplate_SetContestRankName(template, 2, Contest_GetRankMessageID(contestHall->showcasedPokemon.contestRank));
    StringTemplate_SetNumber(template, 3, contestHall->showcasedPokemon.contestPlacement, 1, PADDING_MODE_NONE, CHARSET_MODE_EN);
    TVSegment_SetTemplateTrainerName(template, 4, episode);
    StringTemplate_SetEasyChatWord(template, 5, contestHall->customMessageWord);

    if (contestHall->showcasedPokemon.contestPlacement == 1) {
        return TVProgramInterviews_Text_ContestHall_Win;
    } else {
        return TVProgramInterviews_Text_ContestHall_Lose;
    }
}

static BOOL TVSegment_IsEligible_ContestHall(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_ContestHall *contestHall = TVEpisode_GetSegment(episode);
    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), contestHall->showcasedPokemon.species);
}

void FieldSystem_SaveTVSegment_RightOnPhotoCorner(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_RightOnPhotoCorner *rightOnPhotoCorner = &segments.rightOnPhotoCorner;

    rightOnPhotoCorner->customMessageWord = customMessageWord;

    ImageClips *imageClips = SaveData_GetImageClips(fieldSystem->saveData);
    DressUpPhoto *photo = ImageClips_GetDressUpPhoto(imageClips, 0);

    rightOnPhotoCorner->species = DressUpPhoto_GetMonSpecies(photo);

    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_RIGHT_ON_PHOTO_CORNER, rightOnPhotoCorner);
}

static int TVSegment_LoadMessage_RightOnPhotoCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_RightOnPhotoCorner *rightOnPhotoCorner = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplateOwnPokemonSpecies(template, 1, rightOnPhotoCorner->species);
    StringTemplate_SetEasyChatWord(template, 2, rightOnPhotoCorner->customMessageWord);

    return TVProgramInterviews_Text_RightOnPhotoCorner;
}

static BOOL TVSegment_IsEligible_RightOnPhotoCorner(FieldSystem *fieldSystem, TVEpisode *episode)
{
    TVSegment_RightOnPhotoCorner *rightOnPhotoCorner = TVEpisode_GetSegment(episode);
    return Pokedex_HasSeenSpecies(SaveData_GetPokedex(fieldSystem->saveData), rightOnPhotoCorner->species);
}

void FieldSystem_SaveTVSegment_StreetCornerPersonalityCheckup(FieldSystem *fieldSystem, u16 pokemonType)
{
    TVSegmentData segments;
    TVSegment_StreetCornerPersonalityCheckup *streetCornerPersonalityCheckup = &segments.streetCornerPersonalityCheckup;

    streetCornerPersonalityCheckup->pokemonType = pokemonType;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_STREET_CORNER_PERSONALITY_CHECKUP, streetCornerPersonalityCheckup);
}

static int TVSegment_LoadMessage_StreetCornerPersonalityCheckup(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_StreetCornerPersonalityCheckup *streetCornerPersonalityCheckup = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    return TVProgramInterviews_Text_StreetCornerPersonalityCheckup_NormalType + streetCornerPersonalityCheckup->pokemonType - 1;
}

void FieldSystem_SaveTVSegment_ThreeCheersForPoffinCorner(FieldSystem *fieldSystem, u16 customMessageWord)
{
    TVSegmentData segments;
    TVSegment_ThreeCheersForPoffinCorner *threeCheersForPoffinCorner = &segments.threeCheersForPoffinCorner;
    TVSegment_ThreeCheersForPoffinCornerData *v2 = TVBroadcast_GetPoffinCorner(SaveData_GetTVBroadcast(fieldSystem->saveData));

    threeCheersForPoffinCorner->data = *v2;
    threeCheersForPoffinCorner->customMessageWord = customMessageWord;
    v2->active = 0;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_THREE_CHEERS_FOR_POFFIN_CORNER, threeCheersForPoffinCorner);
}

static int TVSegment_LoadMessage_ThreeCheersForPoffinCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_ThreeCheersForPoffinCorner *threeCheersForPoffinCorner = TVEpisode_GetSegment(episode);
    int poffin = threeCheersForPoffinCorner->data.poffinType;

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetPoffinName(template, 1, poffin);
    StringTemplate_SetEasyChatWord(template, 2, threeCheersForPoffinCorner->customMessageWord);

    switch (poffin) {
    case POFFIN_TYPE_RICH:
        return TVProgramInterviews_Text_ThreeCheersForPoffinCorner_RichPoffin;
    case POFFIN_TYPE_OVERRIPE:
        return TVProgramInterviews_Text_ThreeCheersForPoffinCorner_OverripePoffin;
    case POFFIN_TYPE_FOUL:
        return TVProgramInterviews_Text_ThreeCheersForPoffinCorner_FoulPoffin;
    case POFFIN_TYPE_MILD:
        return TVProgramInterviews_Text_ThreeCheersForPoffinCorner_MildPoffin;
    default:
        return TVProgramInterviews_Text_ThreeCheersForPoffinCorner_RegularPoffin;
    }
}

void FieldSystem_SaveTVSegment_AmitySquareWatch(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_AmitySquareWatch *amitySquareWatch = &segments.amitySquareWatch;
    TVSegment_AmitySquareWatchData *v2 = TVBroadcast_GetAmitySquareWatch(SaveData_GetTVBroadcast(fieldSystem->saveData));

    amitySquareWatch->data = *v2;
    amitySquareWatch->customWordMessage = customWordMessage;
    v2->active = 0;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_AMITY_SQUARE_WATCH, amitySquareWatch);
}

static int TVSegment_LoadMessage_AmitySquareWatch(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_AmitySquareWatch *amitySquareWatch = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 1, amitySquareWatch->data.species, amitySquareWatch->data.gender, amitySquareWatch->data.language, amitySquareWatch->data.metGame);
    StringTemplate_SetNatureName(template, 2, amitySquareWatch->data.nature);
    StringTemplate_SetEasyChatWord(template, 5, amitySquareWatch->customWordMessage);

    switch (amitySquareWatch->data.foundType) {
    case 0:
        return TVProgramInterviews_Text_AmitySquareWatch;
    case 2:
        StringTemplate_SetContestAccessoryName(template, 3, amitySquareWatch->data.foundAccessory);
        return TVProgramInterviews_Text_AmitySquareWatch_FoundAccessory;
    case 1:
        StringTemplate_SetItemName(template, 3, amitySquareWatch->data.foundItem);
        return TVProgramInterviews_Text_AmitySquareWatch_FoundItem;
    }

    return TVProgramInterviews_Text_AmitySquareWatch;
}

void FieldSystem_SaveTVSegment_BattleFrontierFrontlineNews_Single(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_BattleFrontierFrontlineNews_Single *battleFrontierFrontlineNewsSingle = &segments.battleFrontierFrontlineNewsSingle;
    TVSegment_BattleFrontierFrontlineNewsSingleData *v2 = TVBroadcast_GetFrontlineNewsSingle(SaveData_GetTVBroadcast(fieldSystem->saveData));

    battleFrontierFrontlineNewsSingle->data = *v2;
    battleFrontierFrontlineNewsSingle->customWordMessage = customWordMessage;
    v2->active = 0;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_BATTLE_FRONTIER_FRONTLINE_NEWS_SINGLE, battleFrontierFrontlineNewsSingle);
}

static int TVSegment_LoadMessage_BattleFrontierFrontlineNews_Single(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_BattleFrontierFrontlineNews_Single *battleFrontierFrontlineNewsSingle = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    TVSegment_SetTemplatePokemonSpecies(template, 1, battleFrontierFrontlineNewsSingle->data.species, battleFrontierFrontlineNewsSingle->data.gender, battleFrontierFrontlineNewsSingle->data.language, battleFrontierFrontlineNewsSingle->data.metGame);

    if (battleFrontierFrontlineNewsSingle->data.hasNickname) {
        TVSegment_SetTemplateString(template, 2, battleFrontierFrontlineNewsSingle->data.nickname, battleFrontierFrontlineNewsSingle->data.gender, battleFrontierFrontlineNewsSingle->data.language, 1);
    } else {
        TVSegment_SetTemplatePokemonSpecies(template, 2, battleFrontierFrontlineNewsSingle->data.species, battleFrontierFrontlineNewsSingle->data.gender, battleFrontierFrontlineNewsSingle->data.language, battleFrontierFrontlineNewsSingle->data.metGame);
    }

    StringTemplate_SetEasyChatWord(template, 3, battleFrontierFrontlineNewsSingle->customWordMessage);
    return TVProgramInterviews_Text_BattleFrontierFrontlineNews_Single;
}

static BOOL TVSegment_IsEligible_BattleFrontierFrontlineNews_Single(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_FIGHT_AREA);
}

void FieldSystem_SaveTVSegment_InYourFaceInterview_Question1(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = &segments.inYourFaceInterviewQuestion;

    inYourFaceInterviewQuestion->customWordMessage = customWordMessage;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_1, inYourFaceInterviewQuestion);
}

static int TVSegment_LoadMessage_InYourFaceInterview_Question1(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetEasyChatWord(template, 1, inYourFaceInterviewQuestion->customWordMessage);

    return TVProgramInterviews_Text_InYourFaceInterview_Question1;
}

void FieldSystem_SaveTVSegment_InYourFaceInterview_Question2(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = &segments.inYourFaceInterviewQuestion;

    inYourFaceInterviewQuestion->customWordMessage = customWordMessage;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_2, inYourFaceInterviewQuestion);
}

static int TVSegment_LoadMessage_InYourFaceInterview_Question2(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetEasyChatWord(template, 1, inYourFaceInterviewQuestion->customWordMessage);

    return TVProgramInterviews_Text_InYourFaceInterview_Question2;
}

void FieldSystem_SaveTVSegment_InYourFaceInterview_Question3(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = &segments.inYourFaceInterviewQuestion;

    inYourFaceInterviewQuestion->customWordMessage = customWordMessage;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_3, inYourFaceInterviewQuestion);
}

static int TVSegment_LoadMessage_InYourFaceInterview_Question3(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetEasyChatWord(template, 1, inYourFaceInterviewQuestion->customWordMessage);

    return TVProgramInterviews_Text_InYourFaceInterview_Question3;
}

void FieldSystem_SaveTVSegment_InYourFaceInterview_Question4(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = &segments.inYourFaceInterviewQuestion;

    inYourFaceInterviewQuestion->customWordMessage = customWordMessage;
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_4, inYourFaceInterviewQuestion);
}

static int TVSegment_LoadMessage_InYourFaceInterview_Question4(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TVSegment_InYourFaceInterview_Question *inYourFaceInterviewQuestion = TVEpisode_GetSegment(episode);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    StringTemplate_SetEasyChatWord(template, 1, inYourFaceInterviewQuestion->customWordMessage);

    return TVProgramInterviews_Text_InYourFaceInterview_Question4;
}

void FieldSystem_SaveTVSegment_BattleFrontierFrontlineNews_Multi(FieldSystem *fieldSystem, u16 customWordMessage)
{
    TVSegmentData segments;
    TVSegment_BattleFrontierFrontlineNews_Multi *battleFrontierFrontlineNewsMulti = &segments.battleFrontierFrontlineNewsMulti;
    TVSegment_BattleFrontierFrontlineNewsMultiData *v2 = TVBroadcast_GetFrontlineNewsMulti(SaveData_GetTVBroadcast(fieldSystem->saveData));

    battleFrontierFrontlineNewsMulti->data = *v2;
    battleFrontierFrontlineNewsMulti->customWordMessage = customWordMessage;
    v2->active = 0;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
    FieldSystem_SaveTVSegment(fieldSystem, TV_PROGRAM_TYPE_INTERVIEWS, TV_PROGRAM_SEGMENT_BATTLE_FRONTIER_FRONTLINE_NEWS_MULTI, battleFrontierFrontlineNewsMulti);
}

static int TVSegment_LoadMessage_BattleFrontierFrontlineNews_Multi(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    int messageID;
    TVSegment_BattleFrontierFrontlineNews_Multi *battleFrontierFrontlineNewsMulti = TVEpisode_GetSegment(episode);
    String *v2 = String_Init(64, HEAP_ID_FIELD1);

    TVSegment_SetTemplateTrainerName(template, 0, episode);
    String_CopyChars(v2, battleFrontierFrontlineNewsMulti->data.trainerName);
    StringTemplate_SetString(template, 1, v2, battleFrontierFrontlineNewsMulti->data.gender, 0, battleFrontierFrontlineNewsMulti->data.language);
    String_Free(v2);
    StringTemplate_SetEasyChatWord(template, 2, battleFrontierFrontlineNewsMulti->customWordMessage);

    switch (battleFrontierFrontlineNewsMulti->data.facility) {
    case 1:
        messageID = TVProgramInterviews_Text_BattleFrontierFrontlineNews_Multi_BattleTower;
        break;
    case 4:
        messageID = TVProgramInterviews_Text_BattleFrontierFrontlineNews_Multi_BattleCastle;
        break;
    case 5:
        messageID = TVProgramInterviews_Text_BattleFrontierFrontlineNews_Multi_BattleHall;
        break;
    case 2:
    case 3:
        messageID = TVProgramInterviews_Text_BattleFrontierFrontlineNews_Multi_BattleFactory;
        break;
    case 6:
        messageID = TVProgramInterviews_Text_BattleFrontierFrontlineNews_Multi_BattleArcade;
        break;
    }

    return messageID;
}

static BOOL TVSegment_IsEligible_BattleFrontierFrontlineNews_Multi(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_FIGHT_AREA);
}

static const u8 sGroupRNGEntries[] = {
    RECORD_MIXED_RNG_PLAYER_OVERRIDE,
    RECORD_MIXED_RNG_QUEUE_0,
    RECORD_MIXED_RNG_QUEUE_1,
    RECORD_MIXED_RNG_QUEUE_2,
    RECORD_MIXED_RNG_QUEUE_3,
};

static int RecordMixedRNG_CountValidEntries(RecordMixedRNG *rngCollection)
{
    int i, count;

    for (i = 0, count = 0; i < NELEMS(sGroupRNGEntries); i++) {
        if (RecordMixedRNG_IsEntryValid(rngCollection, sGroupRNGEntries[i])) {
            count++;
        }
    }

    return count;
}

static int TVSegment_LoadMessage_DiscoveringGroups(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    int i, validEntries, entry;
    enum PokemonType type;
    RecordMixedRNG *rngCollection = SaveData_GetRecordMixedRNG(fieldSystem->saveData);

    validEntries = RecordMixedRNG_CountValidEntries(rngCollection);
    GF_ASSERT(validEntries > 0);

    if (validEntries > 1) {
        validEntries = MTRNG_Next() % validEntries;
    } else {
        validEntries = 0;
    }

    for (i = 0; i < NELEMS(sGroupRNGEntries); i++) {
        if (RecordMixedRNG_IsEntryValid(rngCollection, sGroupRNGEntries[i])) {
            if (validEntries == 0) {
                entry = sGroupRNGEntries[i];
                break;
            } else {
                validEntries--;
            }
        }
    }

    GF_ASSERT(validEntries == 0);

    type = LCRNG_RandMod(NUM_POKEMON_TYPES - 1);

    if (type >= TYPE_MYSTERY) {
        type++;
    }

    StringTemplate_SetUnionGroupName(template, fieldSystem->saveData, entry, 0, 1);
    StringTemplate_SetUnionGroupName(template, fieldSystem->saveData, entry, 1, 0);
    StringTemplate_SetPokemonTypeName(template, 2, type);

    return TVProgramSinnohNow_Text_DiscoveringGroups;
}

static BOOL TVSegment_IsEligible_DiscoveringGroups(FieldSystem *fieldSystem, TVEpisode *episode)
{
    RecordMixedRNG *rngCollection = SaveData_GetRecordMixedRNG(fieldSystem->saveData);

    if (RecordMixedRNG_CountValidEntries(rngCollection) != 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static u16 sOnTheSpotWeatherLocations[] = {
    MAP_HEADER_ROUTE_212_SOUTH,
    MAP_HEADER_ROUTE_213,
    MAP_HEADER_ROUTE_216,
    MAP_HEADER_ACUITY_LAKEFRONT,
    MAP_HEADER_SNOWPOINT_CITY
};

static int TVSegment_LoadMessage_OnTheSpotWeather(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    int mapHeaderID = sOnTheSpotWeatherLocations[LCRNG_RandMod(NELEMS(sOnTheSpotWeatherLocations))];
    int weather = FieldSystem_GetWeather(fieldSystem, mapHeaderID);
    StringTemplate_SetLocationName(template, 0, MapHeader_GetMapLabelTextID(mapHeaderID));

    switch (weather) {
    case OVERWORLD_WEATHER_CLEAR:
        switch (LCRNG_RandMod(4)) {
        case 0:
            return TVProgramSinnohNow_Text_OnTheSpotWeather_Clear1;
        case 1:
            return TVProgramSinnohNow_Text_OnTheSpotWeather_Clear2;
        case 2:
            return TVProgramSinnohNow_Text_OnTheSpotWeather_Clear3;
        case 3:
            return TVProgramSinnohNow_Text_OnTheSpotWeather_Clear4;
        }
    case OVERWORLD_WEATHER_CLOUDY:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Cloudy;
    case OVERWORLD_WEATHER_RAINING:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Raining;
    case OVERWORLD_WEATHER_HEAVY_RAIN:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_HeavyRain;
    case OVERWORLD_WEATHER_SNOWING:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Snowing;
    case OVERWORLD_WEATHER_HEAVY_SNOW:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_HeavySnow;
    case OVERWORLD_WEATHER_BLIZZARD:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Blizzard;
    case OVERWORLD_WEATHER_THUNDERSTORM:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Thunderstorm;
    case OVERWORLD_WEATHER_HAILING:
        return TVProgramSinnohNow_Text_OnTheSpotWeather_Hailing;
    default:
        GF_ASSERT(FALSE);
    }

    return TVProgramSinnohNow_Text_OnTheSpotWeather_Clear1;
}

static BOOL FieldSystem_AlwaysTrue(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return TRUE;
}

// Skips Marts, Gyms and Pokémon Centers
static int TVSegment_LoadMessage_YourTownsBestThree(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(FieldSystem_GetSaveData(fieldSystem));
    enum MapHeaderID mapHeaderID = fieldSystem->location->mapHeaderID;

    if (mapHeaderID == MAP_HEADER_TWINLEAF_TOWN || (mapHeaderID >= MAP_HEADER_TWINLEAF_TOWN_RIVAL_HOUSE_1F && mapHeaderID <= MAP_HEADER_TWINLEAF_TOWN_SOUTHWEST_HOUSE)) {
        StringTemplate_SetPlayerName(template, 0, trainerInfo);
        StringTemplate_SetRivalName(template, 1, fieldSystem->saveData);
        return TVProgramSinnohNow_Text_YourTownsBestThree_TwinleafTown;
    }

    if (mapHeaderID == MAP_HEADER_SANDGEM_TOWN || (mapHeaderID >= MAP_HEADER_SANDGEM_TOWN_POKEMON_RESEARCH_LAB && mapHeaderID <= MAP_HEADER_SANDGEM_TOWN_HOUSE)) {
        StringTemplate_SetCounterpartName(template, 1, fieldSystem->saveData);
        return TVProgramSinnohNow_Text_YourTownsBestThree_SandgemTown;
    }

    if (mapHeaderID == MAP_HEADER_FLOAROMA_TOWN || (mapHeaderID >= MAP_HEADER_FLOWER_SHOP && mapHeaderID <= MAP_HEADER_FLOAROMA_TOWN_MIDDLE_HOUSE) || mapHeaderID == MAP_HEADER_FLOAROMA_MEADOW_HOUSE) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_FloaromaTown;
    }

    if (mapHeaderID == MAP_HEADER_SOLACEON_TOWN || (mapHeaderID >= MAP_HEADER_POKEMON_DAY_CARE && mapHeaderID <= MAP_HEADER_SOLACEON_TOWN_EAST_HOUSE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_SolaceonTown;
    }

    if (mapHeaderID == MAP_HEADER_CELESTIC_TOWN || (mapHeaderID >= MAP_HEADER_CELESTIC_TOWN_NORTH_HOUSE && mapHeaderID <= MAP_HEADER_CELESTIC_TOWN_CAVE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_CelesticTown;
    }

    if (mapHeaderID == MAP_HEADER_JUBILIFE_CITY || (mapHeaderID >= MAP_HEADER_POKETCH_CO_1F && mapHeaderID <= MAP_HEADER_UNUSED_JUBILIFE_CITY_HOUSE_4)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_JubilifeCity;
    }

    if (mapHeaderID == MAP_HEADER_CANALAVE_CITY || (mapHeaderID >= MAP_HEADER_CANALAVE_LIBRARY_1F && mapHeaderID <= MAP_HEADER_CANALAVE_CITY_SAILOR_ELDRITCH_HOUSE) || mapHeaderID == MAP_HEADER_CANALAVE_CITY_WEST_HOUSE) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_CanalaveCity;
    }

    if (mapHeaderID == MAP_HEADER_OREBURGH_CITY || (mapHeaderID >= MAP_HEADER_OREBURGH_CITY_NORTHWEST_HOUSE_1F && mapHeaderID <= MAP_HEADER_OREBURGH_CITY_SOUTH_HOUSE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_OreburghCity;
    }

    if (mapHeaderID == MAP_HEADER_ETERNA_CITY || (mapHeaderID >= MAP_HEADER_CYCLE_SHOP && mapHeaderID <= MAP_HEADER_UNUSED_ETERNA_CITY_HOUSE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_EternaCity;
    }

    if (mapHeaderID == MAP_HEADER_HEARTHOME_CITY || (mapHeaderID >= MAP_HEADER_HEARTHOME_CITY_SOUTHEAST_HOUSE_1F && mapHeaderID <= MAP_HEADER_FOREIGN_BUILDING)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_HearthomeCity;
    }

    if (mapHeaderID == MAP_HEADER_PASTORIA_CITY || (mapHeaderID >= MAP_HEADER_PASTORIA_CITY_OBSERVATORY_GATE_1F && mapHeaderID <= MAP_HEADER_PASTORIA_CITY_NORTHEAST_HOUSE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_PastoriaCity;
    }

    if (mapHeaderID == MAP_HEADER_VEILSTONE_CITY || (mapHeaderID >= MAP_HEADER_GAME_CORNER && mapHeaderID <= MAP_HEADER_ROUTE_215_GATE_TO_VEILSTONE_CITY) || (mapHeaderID >= MAP_HEADER_GALACTIC_HQ_1F && mapHeaderID <= MAP_HEADER_GALACTIC_HQ_B2F)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_VeilstoneCity;
    }

    if (mapHeaderID == MAP_HEADER_SUNYSHORE_CITY || (mapHeaderID >= MAP_HEADER_SUNYSHORE_MARKET && mapHeaderID <= MAP_HEADER_VISTA_LIGHTHOUSE) || mapHeaderID == MAP_HEADER_VISTA_LIGHTHOUSE_ELEVATOR) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_SunyshoreCity;
    }

    if (mapHeaderID == MAP_HEADER_SNOWPOINT_CITY || (mapHeaderID >= MAP_HEADER_SNOWPOINT_CITY_WEST_HOUSE && mapHeaderID <= MAP_HEADER_SNOWPOINT_CITY_EAST_HOUSE)) {
        return TVProgramSinnohNow_Text_YourTownsBestThree_SnowpointCity;
    }

    StringTemplate_SetPlayerName(template, 0, trainerInfo);
    StringTemplate_SetRivalName(template, 1, fieldSystem->saveData);

    return TVProgramSinnohNow_Text_YourTownsBestThree_WhereWillWeGo;
}

static int TVSegment_LoadMessage_SwarmNewsFlash(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    u16 mapID, species;
    SpecialEncounter *speEnc = SaveData_GetSpecialEncounters(fieldSystem->saveData);

    Swarm_GetMapIdAndSpecies(SpecialEncounter_GetDailyMon(speEnc, DAILY_SWARM), &mapID, &species);
    StringTemplate_SetLocationName(template, 0, MapHeader_GetMapLabelTextID(mapID));
    TVSegment_SetTemplateOwnPokemonSpecies(template, 1, species);

    return TVProgramSinnohNow_Text_SwarmNewsFlash;
}

static BOOL TVSegment_IsEligible_SwarmNewsFlash(FieldSystem *fieldSystem, TVEpisode *episode)
{
    SpecialEncounter *speEnc = SaveData_GetSpecialEncounters(fieldSystem->saveData);
    return SpecialEncounter_IsSwarmEnabled(speEnc);
}

// Leftover from DP
static BOOL MatchupChannelDummy(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return FALSE;
}

enum BerryLookoutArrival {
    BERRY_LOOKOUT_ARRIVAL_TWINLEAF_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SANDGEM_TOWN,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,
    BERRY_LOOKOUT_ARRIVAL_RESORT_AREA,
    BERRY_LOOKOUT_ARRIVAL_JUBILIFE_CITY,
    BERRY_LOOKOUT_ARRIVAL_CANALAVE_CITY,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,
    BERRY_LOOKOUT_ARRIVAL_ETERNA_CITY,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,
    BERRY_LOOKOUT_ARRIVAL_SUNYSHORE_CITY,
    BERRY_LOOKOUT_ARRIVAL_SNOWPOINT_CITY,
    BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,
    BERRY_LOOKOUT_ARRIVAL_COUNT
};

// clang-format off
static const u16 sBerryLookoutArrivalFlags[BERRY_LOOKOUT_ARRIVAL_COUNT] = {
    [BERRY_LOOKOUT_ARRIVAL_TWINLEAF_TOWN]        = FIRST_ARRIVAL_TWINLEAF_TOWN,
    [BERRY_LOOKOUT_ARRIVAL_SANDGEM_TOWN]         = FIRST_ARRIVAL_SANDGEM_TOWN,
    [BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN]        = FIRST_ARRIVAL_FLOAROMA_TOWN,
    [BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN]        = FIRST_ARRIVAL_SOLACEON_TOWN,
    [BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN]        = FIRST_ARRIVAL_CELESTIC_TOWN,
    [BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA]        = FIRST_ARRIVAL_SURVIVAL_AREA,
    [BERRY_LOOKOUT_ARRIVAL_RESORT_AREA]          = FIRST_ARRIVAL_RESORT_AREA,
    [BERRY_LOOKOUT_ARRIVAL_JUBILIFE_CITY]        = FIRST_ARRIVAL_JUBILIFE_CITY,
    [BERRY_LOOKOUT_ARRIVAL_CANALAVE_CITY]        = FIRST_ARRIVAL_CANALAVE_CITY,
    [BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY]        = FIRST_ARRIVAL_OREBURGH_CITY,
    [BERRY_LOOKOUT_ARRIVAL_ETERNA_CITY]          = FIRST_ARRIVAL_ETERNA_CITY,
    [BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY]       = FIRST_ARRIVAL_HEARTHOME_CITY,
    [BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY]        = FIRST_ARRIVAL_PASTORIA_CITY,
    [BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY]       = FIRST_ARRIVAL_VEILSTONE_CITY,
    [BERRY_LOOKOUT_ARRIVAL_SUNYSHORE_CITY]       = FIRST_ARRIVAL_SUNYSHORE_CITY,
    [BERRY_LOOKOUT_ARRIVAL_SNOWPOINT_CITY]       = FIRST_ARRIVAL_SNOWPOINT_CITY,
    [BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD] = FIRST_ARRIVAL_OUTSIDE_VICTORY_ROAD,
    [BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA]           = FIRST_ARRIVAL_FIGHT_AREA,
};

static const u16 sBerryLookoutPatchInfo[] = {
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_FLOAROMA_TOWN,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_FLOAROMA_TOWN,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ETERNA_FOREST_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ETERNA_FOREST_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ETERNA_FOREST_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ETERNA_FOREST_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_FUEGO_IRONWORKS_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_FUEGO_IRONWORKS_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_FUEGO_IRONWORKS_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_FUEGO_IRONWORKS_OUTSIDE,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_NORTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_NORTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_NORTH,
    BERRY_LOOKOUT_ARRIVAL_FLOAROMA_TOWN,        MAP_HEADER_ROUTE_205_NORTH,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_206,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_206,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_206,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_206,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_207,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_207,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_207,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_207,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_208,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_208,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_208,
    BERRY_LOOKOUT_ARRIVAL_OREBURGH_CITY,        MAP_HEADER_ROUTE_208,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_209,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_209,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_209,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_209,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_SOLACEON_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_SOLACEON_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_SOLACEON_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_SOLACEON_TOWN,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_ROUTE_210_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_ROUTE_210_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_ROUTE_210_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_SOLACEON_TOWN,        MAP_HEADER_ROUTE_210_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_210_NORTH,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_210_NORTH,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_210_NORTH,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_210_NORTH,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_211_EAST,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_211_EAST,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_211_EAST,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_211_EAST,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_212_NORTH,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_212_NORTH,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_212_NORTH,
    BERRY_LOOKOUT_ARRIVAL_HEARTHOME_CITY,       MAP_HEADER_ROUTE_212_NORTH,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_212_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_212_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_212_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_212_SOUTH,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_PASTORIA_CITY,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_PASTORIA_CITY,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_PASTORIA_CITY,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_PASTORIA_CITY,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_213,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_213,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_213,
    BERRY_LOOKOUT_ARRIVAL_PASTORIA_CITY,        MAP_HEADER_ROUTE_213,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_214,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_214,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_214,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_214,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_215,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_215,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_215,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_215,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_218,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_218,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_218,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_218,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_221,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_221,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_221,
    BERRY_LOOKOUT_ARRIVAL_CELESTIC_TOWN,        MAP_HEADER_ROUTE_221,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_222,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_222,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_222,
    BERRY_LOOKOUT_ARRIVAL_VEILSTONE_CITY,       MAP_HEADER_ROUTE_222,
    BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD, MAP_HEADER_ROUTE_224,
    BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD, MAP_HEADER_ROUTE_224,
    BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD, MAP_HEADER_ROUTE_224,
    BERRY_LOOKOUT_ARRIVAL_OUTSIDE_VICTORY_ROAD, MAP_HEADER_ROUTE_224,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_FIGHT_AREA,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_FIGHT_AREA,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_FIGHT_AREA,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_FIGHT_AREA,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_ROUTE_225,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_ROUTE_225,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_ROUTE_225,
    BERRY_LOOKOUT_ARRIVAL_FIGHT_AREA,           MAP_HEADER_ROUTE_225,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_226,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_226,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_226,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_226,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_228,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_228,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_228,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_228,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_229,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_229,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_229,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_229,
    BERRY_LOOKOUT_ARRIVAL_RESORT_AREA,          MAP_HEADER_RESORT_AREA,
    BERRY_LOOKOUT_ARRIVAL_RESORT_AREA,          MAP_HEADER_RESORT_AREA,
    BERRY_LOOKOUT_ARRIVAL_RESORT_AREA,          MAP_HEADER_RESORT_AREA,
    BERRY_LOOKOUT_ARRIVAL_RESORT_AREA,          MAP_HEADER_RESORT_AREA,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_230,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_230,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_230,
    BERRY_LOOKOUT_ARRIVAL_SURVIVAL_AREA,        MAP_HEADER_ROUTE_230,
};
// clang-format on

static int BerryLookout_GetVisitedPatch(FieldSystem *fieldSystem)
{
    u8 arrivals[NELEMS(sBerryLookoutArrivalFlags)];
    u8 patches[NELEMS(sBerryLookoutPatchInfo) / 2];
    int i, count;
    VarsFlags *varsFlags = SaveData_GetVarsFlags(fieldSystem->saveData);

    for (i = 0; i < NELEMS(sBerryLookoutArrivalFlags); i++) {
        arrivals[i] = SystemFlag_HandleFirstArrivalToZone(varsFlags, HANDLE_FLAG_CHECK, sBerryLookoutArrivalFlags[i]);
    }

    for (i = 0, count = 0; i < NELEMS(sBerryLookoutPatchInfo) / 2; i++) {
        if (arrivals[sBerryLookoutPatchInfo[i * 2]]) {
            patches[count] = i;
            count++;
        }
    }

    return patches[LCRNG_RandMod(count)];
}

static int TVSegment_LoadMessage_BerryLookout(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    BerryPatch *berryPatches = MiscSaveBlock_GetBerryPatches(fieldSystem->saveData);
    int patchID = BerryLookout_GetVisitedPatch(fieldSystem);
    enum MapHeaderID headerID = sBerryLookoutPatchInfo[patchID * 2 + 1];
    StringTemplate_SetLocationName(template, 0, MapHeader_GetMapLabelTextID(headerID));

    switch (BerryPatches_GetPatchGrowthStage(berryPatches, patchID)) {
    case BERRY_GROWTH_STAGE_FRUIT:
        return TVProgramSinnohNow_Text_BerryLookout_Fruit;
    case BERRY_GROWTH_STAGE_BLOOMING:
        return TVProgramSinnohNow_Text_BerryLookout_Blooming;
    case BERRY_GROWTH_STAGE_GROWING:
        return TVProgramSinnohNow_Text_BerryLookout_Growing;
    case BERRY_GROWTH_STAGE_SPROUTED:
        return TVProgramSinnohNow_Text_BerryLookout_Sprouted;
    case BERRY_GROWTH_STAGE_NONE:
    case BERRY_GROWTH_STAGE_PLANTED:
    default:
        return TVProgramSinnohNow_Text_BerryLookout_None;
    }
}

static BOOL TVSegment_IsEligible_BerryLookout(FieldSystem *fieldSystem, TVEpisode *episode)
{
    VarsFlags *varsFlags = SaveData_GetVarsFlags(fieldSystem->saveData);
    return SystemFlag_HandleFirstArrivalToZone(varsFlags, HANDLE_FLAG_CHECK, FIRST_ARRIVAL_OREBURGH_CITY);
}

// Leftover from DP
static BOOL PokemonResearchCornerDummy(FieldSystem *fieldSystem, TVEpisode *episode)
{
    return FALSE;
}

static int TVSegment_LoadMessage_RichBoyNatureCorner(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    u32 personality, i;
    u8 nature;
    int flavor = 0xff, stat = 0xff;
    personality = (LCRNG_Next() % 0xffff);
    nature = Pokemon_GetNatureOf(personality);

    StringTemplate_SetNatureName(template, 0, nature);

    if (nature == NATURE_HARDY
        || nature == NATURE_DOCILE
        || nature == NATURE_SERIOUS
        || nature == NATURE_BASHFUL
        || nature == NATURE_QUIRKY) {
        return TVProgramSinnohNow_Text_RichBoyNatureCorner_NeutralNature;
    }

    if ((personality % 2) == 0) {
        for (i = 0; i < FLAVOR_MAX; i++) {
            if (Pokemon_GetFlavorAffinityOf(personality, i) == 1) {
                flavor = i;
                break;
            }
        }

        StringTemplate_SetFlavorName(template, 2, flavor);
        return TVProgramSinnohNow_Text_RichBoyNatureCorner_FlavorAffinity;
    }

    for (i = 0; i < STAT_MAX - 1; i++) {
        if (Pokemon_GetStatAffinityOf(nature, 1 + i) > 0) {
            stat = i;
            break;
        }
    }

    StringTemplate_SetPokemonStatName(template, 1, 1 + stat);
    return TVProgramSinnohNow_Text_RichBoyNatureCorner_StatAffinity;
}

static int TVSegment_LoadMessage_RoamerNewsFlash(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    Roamer *roamer;
    SpecialEncounter *speEnc;
    u16 roamingRouteIndex, i;
    u32 species, personality;
    String *string = String_Init(22, HEAP_ID_FIELD1);
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(FieldSystem_GetSaveData(fieldSystem));

    speEnc = SaveData_GetSpecialEncounters(fieldSystem->saveData);
    roamingRouteIndex = (LCRNG_Next() % RI_MAX);

    MapHeader_LoadName(RoamingPokemon_GetRouteFromId(roamingRouteIndex), HEAP_ID_FIELD1, string);
    StringTemplate_SetString(template, 0, string, 0, 1, GAME_LANGUAGE);
    String_Free(string);

    for (i = 0; i < ROAMING_SLOT_MAX; i++) {
        if (SpecialEncounter_IsRoamerActive(speEnc, i)) {
            roamer = SpecialEncounter_GetRoamer(speEnc, i);

            species = Roamer_GetData(roamer, ROAMER_DATA_SPECIES);
            personality = Roamer_GetData(roamer, ROAMER_DATA_PERSONALITY);

            TVSegment_SetTemplatePokemonSpecies(template, 1, species, Pokemon_GetGenderOf(species, personality), TrainerInfo_Language(trainerInfo), TrainerInfo_GameCode(trainerInfo));
            break;
        }
    }

    return TVProgramSinnohNow_Text_RoamerNewsFlash;
}

static BOOL TVSegment_IsEligible_RoamerNewsFlash(FieldSystem *fieldSystem, TVEpisode *episode)
{
    int i;
    SpecialEncounter *speEnc = SaveData_GetSpecialEncounters(fieldSystem->saveData);

    for (i = 0; i < ROAMING_SLOT_MAX; i++) {
        if (SpecialEncounter_IsRoamerActive(speEnc, i)) {
            return TRUE;
        }
    }

    return FALSE;
}

static int ImageClips_CountDressUpPhotosWithData(ImageClips *imageClips)
{
    int i, count;

    for (i = 0, count = 0; i < SAVED_PHOTOS_COUNT; i++) {
        if (ImageClips_DressUpPhotoHasData(imageClips, i) == TRUE) {
            count++;
        }
    }

    return count;
}

static int TVSegment_LoadMessage_PokemonPhotoRating(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    DressUpPhoto *photo;
    int i, count, rand, slot;
    ImageClips *imageClips = SaveData_GetImageClips(fieldSystem->saveData);

    count = ImageClips_CountDressUpPhotosWithData(imageClips);

    if (count > 1) {
        rand = MTRNG_Next() % count;
    } else {
        rand = 0;
    }

    for (i = 0; i < SAVED_PHOTOS_COUNT; i++) {
        if (ImageClips_DressUpPhotoHasData(imageClips, i) == TRUE) {
            if (rand == 0) {
                slot = i;
                break;
            } else {
                rand--;
            }
        }
    }

    GF_ASSERT(i < SAVED_PHOTOS_COUNT);
    photo = ImageClips_GetDressUpPhoto(imageClips, slot);

    u16 word;
    String *trainerName = String_Init(TRAINER_NAME_LEN + 1, HEAP_ID_FIELD1);
    int gender = DressUpPhoto_GetTrainerGender(photo);

    DressUpPhoto_SetTrainerName(photo, trainerName);
    StringTemplate_SetString(template, 0, trainerName, gender, 1, DressUpPhoto_GetLanguage(photo));
    String_Free(trainerName);

    word = DressUpPhoto_GetTitleWord(photo);
    StringTemplate_SetEasyChatWord(template, 1, word);

    return TVProgramSinnohNow_Text_PokemonPhotoRating;
}

static BOOL TVSegment_IsEligible_PokemonPhotoRating(FieldSystem *fieldSystem, TVEpisode *episode)
{
    ImageClips *imageClips = SaveData_GetImageClips(fieldSystem->saveData);

    if (ImageClips_CountDressUpPhotosWithData(imageClips) != 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// Variety Hour segments. These callbacks return raw message IDs into
// tv_programs_variety_hour: 0-7 are the exploration-team "special report"
// episodes, 8-10 Sinnoh Sports, 11-13 the "At the Surf's Edge" drama, 14-16
// Pokétch Detective, 17 Sinnoh Hot Hit Tunes, 18 Dramatic Cinema Hour, 19 We
// Love the GTS, and 20-28 "The Diary of a Poké Romantic".

// Picks a special-report episode from the pool unlocked by story progress.
static int TVSegment_LoadMessage_SpecialReport(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0 = 0;

    if (SystemFlag_CheckGameCompleted(SaveData_GetVarsFlags(fieldSystem->saveData)) == 1) {
        v0 = (LCRNG_Next() % 8);
    } else if (SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_HEARTHOME_CITY) == TRUE) {
        v0 = (LCRNG_Next() % 5);
    } else if (SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_ETERNA_CITY) == TRUE) {
        v0 = (LCRNG_Next() % 4);
    } else if (SystemFlag_HandleFirstArrivalToZone(SaveData_GetVarsFlags(fieldSystem->saveData), HANDLE_FLAG_CHECK, FIRST_ARRIVAL_OREBURGH_MINE) == TRUE) {
        v0 = (LCRNG_Next() % 2);
    }

    return 0 + v0;
}

// Picks a random seen species and one of the three Sinnoh Sports variants.
static int TVSegment_LoadMessage_SinnohSports(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    String *v0;
    u16 v1, v2, v3;
    const Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);

    v1 = (LCRNG_Next() % (NATIONAL_DEX_COUNT - 1)) + 1;

    for (v2 = 1; v2 <= NATIONAL_DEX_COUNT; v2++) {
        if (Pokedex_HasSeenSpecies(pokedex, v1) == TRUE) {
            v3 = v1;
            break;
        }

        v1++;

        if (v1 == NATIONAL_DEX_COUNT) {
            v1 = 1;
        }
    }

    v0 = TVSegment_GetSpeciesNameString(v3, HEAP_ID_FIELD1);

    StringTemplate_SetString(param1, 0, v0, 0, 1, GAME_LANGUAGE);
    String_Free(v0);

    v1 = (LCRNG_Next() % 3);

    if (v1 == 0) {
        return 8;
    } else if (v1 == 1) {
        v1 = (LCRNG_Next() % 34) + 25;
        StringTemplate_SetNumber(param1, 1, v1, 2, PADDING_MODE_NONE, CHARSET_MODE_EN);
        return 9;
    } else {
        return 10;
    }
}

// Loads a species name from the species-name text bank.
static String *TVSegment_GetSpeciesNameString(u16 param0, enum HeapID heapID)
{
    MessageLoader *v0 = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_SPECIES_NAME, heapID);
    String *v1 = MessageLoader_GetNewString(v0, param0);

    MessageLoader_Free(v0);
    return v1;
}

static BOOL TVSegment_IsEligible_SinnohSports(FieldSystem *fieldSystem, TVEpisode *episode)
{
    const Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);

    if (Pokedex_IsObtained(pokedex) == TRUE) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// Picks one of the three "At the Surf's Edge" episodes.
static int TVSegment_LoadMessage_AtTheSurfsEdge(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0 = (LCRNG_Next() % 3);

    if (v0 == 0) {
        return 11;
    } else if (v0 == 1) {
        return 12;
    } else {
        return 13;
    }
}

// Picks one of the three Pokétch Detective cases.
static int TVSegment_LoadMessage_PoketchDetective(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0 = (LCRNG_Next() % 3);

    if (v0 == 0) {
        return 14;
    } else if (v0 == 1) {
        return 15;
    } else {
        return 16;
    }
}

// Builds the Sinnoh Hot Hit Tunes chart from the player's party and Pokédex.
static int TVSegment_LoadMessage_SinnohHotHitTunes(FieldSystem *fieldSystem, StringTemplate *template, TVEpisode *episode)
{
    String *v0;
    u16 v1, v2;
    Pokemon *mon;
    Party *party;
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(fieldSystem->saveData);
    Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);

    party = SaveData_GetParty(fieldSystem->saveData);
    mon = Party_GetPokemonBySlotIndex(party, SaveData_GetFirstNonEggInParty(fieldSystem->saveData));

    TVSegment_SetTemplatePokemonSpecies(template, 0, Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), Pokemon_GetValue(mon, MON_DATA_GENDER, NULL), TrainerInfo_Language(trainerInfo), TrainerInfo_GameCode(trainerInfo));
    StringTemplate_SetContestAccessoryName(template, 1, LCRNG_Next() % 100);

    v1 = (LCRNG_Next() % (NATIONAL_DEX_COUNT - 2) + 1);

    for (v2 = 1; v2 <= NATIONAL_DEX_COUNT; v2++) {
        if (Pokedex_HasSeenSpecies(pokedex, v1) == TRUE) {
            v0 = TVSegment_GetSpeciesNameString(v1, HEAP_ID_FIELD1);
            StringTemplate_SetString(template, 2, v0, 0, 1, GAME_LANGUAGE);
            String_Free(v0);
            break;
        }

        v1++;

        if (v1 >= NATIONAL_DEX_COUNT) {
            v1 = 1;
        }
    }

    StringTemplate_SetMoveName(template, 3, (LCRNG_Next() % 467 - 2) + 1);

    return 17;
}

static BOOL TVSegment_IsEligible_SinnohHotHitTunes(FieldSystem *fieldSystem, TVEpisode *episode)
{
    const Pokedex *pokedex = SaveData_GetPokedex(fieldSystem->saveData);

    if (Pokedex_IsObtained(pokedex) == TRUE) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static int TVSegment_LoadMessage_DramaticCinemaHour(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    return 18;
}

static int TVSegment_LoadMessage_WeLoveTheGTS(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    return 19;
}

// We Love the GTS only airs once the player has earned the first Gym Badge.
static BOOL TVSegment_IsEligible_WeLoveTheGTS(FieldSystem *fieldSystem, TVEpisode *episode)
{
    if (TrainerInfo_HasBadge(SaveData_GetTrainerInfo(fieldSystem->saveData), 0) == 1) {
        return 1;
    } else {
        return 0;
    }
}

// Picks one of the nine "Diary of a Poké Romantic" episodes.
static int TVSegment_LoadMessage_DiaryOfAPokeRomantic(FieldSystem *fieldSystem, StringTemplate *param1, TVEpisode *episode)
{
    u16 v0 = (LCRNG_Next() % 9);

    if (v0 == 0) {
        return 20;
    } else if (v0 == 1) {
        return 21;
    } else if (v0 == 2) {
        return 22;
    } else if (v0 == 3) {
        return 23;
    } else if (v0 == 4) {
        return 24;
    } else if (v0 == 5) {
        return 25;
    } else if (v0 == 6) {
        return 26;
    } else if (v0 == 7) {
        return 27;
    } else {
        return 28;
    }
}

static const TVSegment sTrainerSightingsSegments[TV_PROGRAM_TYPE_TRAINER_SIGHTINGS_NUM_SEGMENTS] = {
    {
        TVSegment_LoadMessage_CatchThatPokemonShow_Success,
        TVSegment_IsEligible_CatchThatPokemonShow_Success,
    },
    {
        TVSegment_LoadMessage_CatchThatPokemonShow_Failure,
        NULL,
    },
    {
        TVSegment_LoadMessage_WhatsFishing,
        TVSegment_IsEligible_WhatsFishing,
    },
    {
        TVSegment_LoadMessage_LoveThatGroupCorner_SwitchGroup,
        NULL,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_HiddenItemBreakingNews,
        NULL,
    },
    {
        TVSegment_LoadMessage_SinnohShoppingChampCorner,
        NULL,
    },
    {
        TVSegment_LoadMessage_HappyHappyEggClub,
        TVSegment_IsEligible_HappyHappyEggClub,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_RateThatNameChange,
        TVSegment_IsEligible_RateThatNameChange,
    },
    TV_PROGRAM_SEGMENT_NULL,
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_UndergroundTreasuresCorner,
        TVSegment_IsEligible_HasExplorerKit,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_SafariGameSpecialNewsBulletin,
        TVSegment_IsEligible_SafariGameSpecialNewsBulletin,
    },
    {
        TVSegment_LoadMessage_PokemonStorageSpecialNewsBulletin,
        TVSegment_IsEligible_PokemonStorageSpecialNewsBulletin,
    },
    {
        NULL,
        TVSegment_IsEligible_HerbalMedicineTrainerSightingDummy,
    },
    {
        TVSegment_LoadMessage_PlantingAndWateringShow,
        NULL,
    },
    {
        TVSegment_LoadMessage_PlantingAndWateringShow_NoBerries,
        NULL,
    },
    {
        TVSegment_LoadMessage_LoveThatGroupCorner_NewGroup,
        NULL,
    },
    {
        TVSegment_LoadMessage_SealClubShow,
        TVSegment_IsEligible_SealClubShow,
    },
    {
        TVSegment_LoadMessage_CaptureTheFlagDigest_TakeFlag,
        TVSegment_IsEligible_HasExplorerKit,
    },
    {
        TVSegment_LoadMessage_CaptureTheFlagDigest_LoseFlag,
        TVSegment_IsEligible_HasExplorerKit,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_HomeAndManor_NoFurniture,
        TVSegment_IsEligible_HomeAndManor_NoFurniture,
    },
    {
        TVSegment_LoadMessage_HomeAndManor,
        TVSegment_IsEligible_HomeAndManor,
    }
};

static const TVSegment sRecordsSegments[TV_PROGRAM_TYPE_RECORDS_NUM_SEGMENTS] = {
    { TVSegment_LoadMessage_BattleTowerStreakRecord, TVSegment_IsEligible_BattleTowerStreakRecord },
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_SizeRecord, TVSegment_IsEligible_SizeRecord },
    { TVSegment_LoadMessage_SlotMachineRecord, NULL },
    { TVSegment_LoadMessage_RibbonRecord, NULL },
    TV_PROGRAM_SEGMENT_NULL,
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_TrapRecord, TVSegment_IsEligible_HasExplorerKitForRecords },
    { TVSegment_LoadMessage_CaptureTheFlagRecord, TVSegment_IsEligible_HasExplorerKitForRecords },
    { TVSegment_LoadMessage_BattlePointsRecord, TVSegment_IsEligible_BattlePointsRecord },
    { TVSegment_LoadMessage_GTSTradeRecord, TVSegment_IsEligible_GTSTradeRecord }
};

static const TVSegment sInterviewsSegments[TV_PROGRAM_TYPE_INTERVIEWS_NUM_SEGMENTS] = {
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_BattleTowerCorner,
        TVSegment_IsEligible_BattleTowerCorner,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_YourPokemonCorner,
        TVSegment_IsEligible_YourPokemonCorner,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_ThePoketchWatch,
        NULL,
    },
    {
        TVSegment_LoadMessage_ContestHall,
        TVSegment_IsEligible_ContestHall,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_RightOnPhotoCorner,
        TVSegment_IsEligible_RightOnPhotoCorner,
    },
    {
        TVSegment_LoadMessage_StreetCornerPersonalityCheckup,
        NULL,
    },
    {
        TVSegment_LoadMessage_ThreeCheersForPoffinCorner,
        NULL,
    },
    TV_PROGRAM_SEGMENT_NULL,
    {
        TVSegment_LoadMessage_AmitySquareWatch,
        NULL,
    },
    {
        TVSegment_LoadMessage_BattleFrontierFrontlineNews_Single,
        TVSegment_IsEligible_BattleFrontierFrontlineNews_Single,
    },
    {
        TVSegment_LoadMessage_InYourFaceInterview_Question1,
        NULL,
    },
    {
        TVSegment_LoadMessage_InYourFaceInterview_Question2,
        NULL,
    },
    {
        TVSegment_LoadMessage_InYourFaceInterview_Question3,
        NULL,
    },
    {
        TVSegment_LoadMessage_InYourFaceInterview_Question4,
        NULL,
    },
    {
        TVSegment_LoadMessage_BattleFrontierFrontlineNews_Multi,
        TVSegment_IsEligible_BattleFrontierFrontlineNews_Multi,
    }
};

static const TVSegment sSinnohNowSegments[TV_PROGRAM_TYPE_SINNOH_NOW_NUM_SEGMENTS] = {
    { TVSegment_LoadMessage_DiscoveringGroups, TVSegment_IsEligible_DiscoveringGroups },
    { TVSegment_LoadMessage_OnTheSpotWeather, FieldSystem_AlwaysTrue },
    { TVSegment_LoadMessage_YourTownsBestThree, NULL },
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_SwarmNewsFlash, TVSegment_IsEligible_SwarmNewsFlash },
    TV_PROGRAM_SEGMENT_NULL,
    { NULL, MatchupChannelDummy },
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_BerryLookout, TVSegment_IsEligible_BerryLookout },
    TV_PROGRAM_SEGMENT_NULL,
    { NULL, PokemonResearchCornerDummy },
    { TVSegment_LoadMessage_RichBoyNatureCorner, NULL },
    TV_PROGRAM_SEGMENT_NULL,
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_RoamerNewsFlash, TVSegment_IsEligible_RoamerNewsFlash },
    TV_PROGRAM_SEGMENT_NULL,
    { TVSegment_LoadMessage_PokemonPhotoRating, TVSegment_IsEligible_PokemonPhotoRating }
};

static const TVSegment sVarietyHourSegments[TV_PROGRAM_TYPE_VARIETY_HOUR_NUM_SEGMENTS] = {
    { TVSegment_LoadMessage_SpecialReport, NULL },
    { TVSegment_LoadMessage_SinnohSports, TVSegment_IsEligible_SinnohSports },
    { TVSegment_LoadMessage_AtTheSurfsEdge, NULL },
    { TVSegment_LoadMessage_PoketchDetective, NULL },
    { TVSegment_LoadMessage_SinnohHotHitTunes, TVSegment_IsEligible_SinnohHotHitTunes },
    { TVSegment_LoadMessage_DramaticCinemaHour, NULL },
    { TVSegment_LoadMessage_WeLoveTheGTS, TVSegment_IsEligible_WeLoveTheGTS },
    { TVSegment_LoadMessage_DiaryOfAPokeRomantic, NULL }
};

// Called on the daily rollover: commits the day's Battle Point and GTS trade
// totals to the Records program, then clears the running counters.
void TVSegment_ResetDailyRecords(SaveData *saveData)
{
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(saveData);

    TVSegment_SaveBattlePointsRecord(saveData);
    TVSegment_SaveGTSTradeRecord(saveData);

    TVBroadcast_ResetBattlePoints(broadcast);
    TVBroadcast_ResetGTSTradeCount(broadcast);

    return;
}
