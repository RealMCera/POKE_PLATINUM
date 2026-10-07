#include "scrcmd_tv_broadcast.h"

#include <nitro.h>
#include <string.h>

#include "constants/map_object.h"
#include "constants/tv_broadcast.h"
#include "generated/movement_types.h"
#include "generated/trainer_score_events.h"

#include "struct_decls/tv_broadcast.h"
#include "struct_defs/image_clips.h"
#include "struct_defs/struct_0202E7E4.h"
#include "struct_defs/struct_0202E7F0.h"
#include "struct_defs/struct_0202E7FC.h"
#include "struct_defs/struct_0202E808.h"
#include "struct_defs/struct_0202E810.h"
#include "struct_defs/struct_0202E81C.h"
#include "struct_defs/tv_segment_contest_hall_showcased_pokemon.h"

#include "applications/poketch/poketch_system.h"
#include "field/field_system.h"
#include "field/field_system_sub2_t.h"
#include "overlay006/ov6_022465FC.h"
#include "overlay006/tv_commercials.h"
#include "savedata/save_table.h"

#include "field_script_context.h"
#include "game_records.h"
#include "inlines.h"
#include "message.h"
#include "party.h"
#include "pokemon.h"
#include "poketch.h"
#include "script_manager.h"
#include "string_gf.h"
#include "string_template.h"
#include "tv_segment.h"
#include "image_clips.h"
#include "tv_broadcast.h"
#include "unk_02054884.h"

#include "res/text/bank/tv_reporter_interviews.h"

// Script commands for the TV broadcast system. The broadcast state itself (the
// pending program, its segments, and the saved segment data) lives in
// tv_broadcast.c; these handlers are the scripting front end that starts a
// broadcast, loads its framing/segment/commercial messages, and records TV
// segments from field events. The interview segment table below maps each
// interview segment to the callbacks that load its message and decide whether
// it can be recorded.

typedef void (*TVInterview_SaveResponseFunction)(FieldSystem *, u16);
typedef void (*TVInterview_LoadMessageFunction)(FieldSystem *, StringTemplate *);
typedef BOOL (*TVInterview_IsEligibleFunction)(FieldSystem *);

// One entry per interview segment. The callbacks are optional: a segment with
// no loadMessageFn uses its messageID verbatim, and a segment with no
// isEligibleFn is always eligible.
typedef struct TVInterview {
    TVInterview_SaveResponseFunction saveResponseFn;
    TVInterview_LoadMessageFunction loadMessageFn;
    TVInterview_IsEligibleFunction isEligibleFn;
    u32 messageID;
} TVInterview;

static int TVInterview_LoadMessage(int segmentID, FieldSystem *fieldSystem, StringTemplate *template);
static void TVInterview_SaveResponse(FieldSystem *fieldSystem, int segmentID, u16 customMessageWord, u16 unused);
static BOOL TVInterview_IsEligible(FieldSystem *fieldSystem, int segmentID);

// Drives a broadcast from a script. The sub-command selects one of the
// TV_BROADCAST_CALL_* operations: query the pending broadcast, load the
// framing/segment/commercial message, mark the program finished, or fetch the
// next segment ID.
BOOL ScrCmd_CallTVBroadcast(ScriptContext *ctx)
{
    switch (ScriptContext_ReadHalfWord(ctx)) {
    case TV_BROADCAST_CALL_CHECK_STATUS: {
        u16 *statusDestVar = ScriptContext_GetVarPointer(ctx);

        *statusDestVar = TVBroadcast_GetPendingBroadcastType(ctx->fieldSystem);
    } break;
    case TV_BROADCAST_CALL_LOAD_FRAMING_MESSAGE: {
        u16 framingMessageType = ScriptContext_ReadHalfWord(ctx);
        u16 *bankDestVar = ScriptContext_GetVarPointer(ctx);
        u16 *messageDestVar = ScriptContext_GetVarPointer(ctx);

        *bankDestVar = TEXT_BANK_TV_PROGRAMS;
        *messageDestVar = TVBroadcast_GetProgramFramingMessage(ctx->fieldSystem, framingMessageType);
    } break;
    case TV_BROADCAST_CALL_LOAD_SEGMENT: {
        StringTemplate **template = FieldSystem_GetScriptMemberPtr(ctx->fieldSystem, SCRIPT_MANAGER_STR_TEMPLATE);
        u16 segmentID = ScriptContext_GetVar(ctx);
        u16 *bankDestVar = ScriptContext_GetVarPointer(ctx);
        u16 *messageDestVar = ScriptContext_GetVarPointer(ctx);

        TVBroadcast_LoadSegmentMessage(ctx->fieldSystem, *template, segmentID, bankDestVar, messageDestVar);
    } break;
    case TV_BROADCAST_CALL_FINISH_PROGRAM:
        FieldSystem_SetTVProgramFinished(ctx->fieldSystem);
        break;
    case TV_BROADCAST_CALL_LOAD_COMMERCIAL: {
        u16 *bankDestVar = ScriptContext_GetVarPointer(ctx);
        u16 *messageDestVar = ScriptContext_GetVarPointer(ctx);

        *bankDestVar = TEXT_BANK_TV_COMMERCIALS;
        *messageDestVar = TVBroadcast_GetProgramCommercialMessage(ctx->fieldSystem);
    } break;
    case TV_BROADCAST_CALL_UNUSED: {
        u16 filter1 = ScriptContext_GetVar(ctx);
        u16 filter2 = ScriptContext_GetVar(ctx);
        u16 *segmentDestVar = ScriptContext_GetVarPointer(ctx);

        *segmentDestVar = ov6_022468B0(ctx->fieldSystem, filter1, filter2);
    } break;
    case TV_BROADCAST_CALL_GET_NEXT_SEGMENT_ID: {
        u16 *segmentDestVar = ScriptContext_GetVarPointer(ctx);
        *segmentDestVar = TVBroadcast_GetNextSegmentID(ctx->fieldSystem);
    } break;
    }

    return FALSE;
}

BOOL ScrCmd_SaveTVSegmentHiddenItem(ScriptContext *ctx)
{
    FieldSystem_SaveTVSegment_HiddenItemBreakingNews(ctx->fieldSystem, ScriptContext_GetVar(ctx));
    return FALSE;
}

// Records a "Rate That Name Change" TV segment for the party Pokemon in the
// given slot.
BOOL ScrCmd_SaveTVSegmentRateThatNameChange(ScriptContext *ctx)
{
    Party *party = SaveData_GetParty(ctx->fieldSystem->saveData);
    Pokemon *mon = Party_GetPokemonBySlotIndex(party, ScriptContext_GetVar(ctx));

    FieldSystem_SaveTVSegment_RateThatNameChange(ctx->fieldSystem, mon);
    return FALSE;
}

BOOL ScrCmd_SaveTVSegmentPokemonStorageBulletin(ScriptContext *ctx)
{
    FieldSystem_SaveTVSegment_PokemonStorageSpecialNewsBulletin(ctx->fieldSystem);
    return FALSE;
}

BOOL ScrCmd_SaveTVSegmentHomeAndManorNoFurniture(ScriptContext *ctx)
{
    FieldSystem_SaveTVSegment_HomeAndManor_NoFurniture(ctx->fieldSystem);
    return FALSE;
}

BOOL ScrCmd_SaveTVSegmentHomeAndManor(ScriptContext *ctx)
{
    u16 furniture = ScriptContext_GetVar(ctx);

    FieldSystem_SaveTVSegment_HomeAndManor(ctx->fieldSystem, furniture);
    return FALSE;
}

static const TVInterview sInterviews[TV_PROGRAM_TYPE_INTERVIEWS_NUM_SEGMENTS];

// Handles the TV_INTERVIEW_CALL_* sub-commands: load the message for an
// interview segment (running its loadMessageFn first to fill the template), or
// save the player's response to a segment and award trainer score.
BOOL ScrCmd_CallTVInterview(ScriptContext *ctx)
{
    StringTemplate **template = FieldSystem_GetScriptMemberPtr(ctx->fieldSystem, SCRIPT_MANAGER_STR_TEMPLATE);

    switch (ScriptContext_ReadHalfWord(ctx)) {
    case TV_INTERVIEW_CALL_LOAD_MESSAGE: {
        int segmentID = ScriptContext_GetVar(ctx);
        u16 *bankIDVar = ScriptContext_GetVarPointer(ctx);
        u16 *messageIDVar = ScriptContext_GetVarPointer(ctx);
        *bankIDVar = TEXT_BANK_TV_REPORTER_INTERVIEWS;
        *messageIDVar = TVInterview_LoadMessage(segmentID, ctx->fieldSystem, *template);
        break;
    }
    case TV_INTERVIEW_CALL_SAVE_RESPONSE: {
        GameRecords *records = SaveData_GetGameRecords(ctx->fieldSystem->saveData);

        u16 segmentID = ScriptContext_GetVar(ctx);
        u16 customMessageWord = ScriptContext_GetVar(ctx);
        u16 unused = ScriptContext_GetVar(ctx);

        TVInterview_SaveResponse(ctx->fieldSystem, segmentID, customMessageWord, unused);
        GameRecords_IncrementTrainerScore(records, TRAINER_SCORE_EVENT_TV_INTERVIEW_GIVEN);
        break;
    }
    }

    return FALSE;
}

// Writes whether the given interview segment can currently be recorded into a
// script variable.
BOOL ScrCmd_CheckTVInterviewEligible(ScriptContext *ctx)
{
    u16 segmentID;
    u16 *destVar;

    segmentID = ScriptContext_GetVar(ctx);
    destVar = ScriptContext_GetVarPointer(ctx);
    *destVar = TVInterview_IsEligible(ctx->fieldSystem, segmentID);

    return FALSE;
}

// Populates the Amity Square Watch TV segment. The sub-command selects what to
// record: the watched Pokemon (0), a found item (1), or a found accessory (2).
BOOL ScrCmd_SaveAmitySquareWatchData(ScriptContext *ctx)
{
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(ctx->fieldSystem->saveData);

    switch (ScriptContext_ReadHalfWord(ctx)) {
    case 0: {
        Party *party = SaveData_GetParty(ctx->fieldSystem->saveData);
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, ScriptContext_GetVar(ctx));

        TVBroadcast_SetAmitySquareWatchInfo(broadcast, mon, HEAP_ID_FIELD1);
    } break;
    case 1:
        TVBroadcast_SetAmitySquareWatchFoundItem(broadcast, ScriptContext_GetVar(ctx));
        break;
    case 2:
        TVBroadcast_SetAmitySquareWatchFoundAccessory(broadcast, ScriptContext_GetVar(ctx));
        break;
    }

    return FALSE;
}

// Runs the segment's saveResponseFn, if it has one. The custom message word is
// the player's answer to the interview question.
static void TVInterview_SaveResponse(FieldSystem *fieldSystem, int segmentID, u16 customMessageWord, u16 unused)
{
    TVInterview_SaveResponseFunction saveResponseFn = sInterviews[segmentID - 1].saveResponseFn;

    if (saveResponseFn != NULL) {
        saveResponseFn(fieldSystem, customMessageWord);
    }
}

// Runs the segment's loadMessageFn, if it has one, to fill the string template,
// then returns the segment's message ID.
static int TVInterview_LoadMessage(int segmentID, FieldSystem *fieldSystem, StringTemplate *template)
{
    TVInterview_LoadMessageFunction loadMessageFn = sInterviews[segmentID - 1].loadMessageFn;

    if (loadMessageFn != NULL) {
        loadMessageFn(fieldSystem, template);
    }

    return sInterviews[segmentID - 1].messageID;
}

// A segment is eligible if the broadcast can still save it and, when the
// segment defines an isEligibleFn, that callback agrees.
static BOOL TVInterview_IsEligible(FieldSystem *fieldSystem, int segmentID)
{
    TVInterview_IsEligibleFunction isEligibleFn;
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);

    if (TVBroadcast_CanSaveSegment(broadcast, 1, segmentID) == 0) {
        return FALSE;
    }

    isEligibleFn = sInterviews[segmentID - 1].isEligibleFn;

    if (isEligibleFn == NULL) {
        return TRUE;
    }

    return isEligibleFn(fieldSystem);
}

// Copies a raw character buffer into a String and installs it as a template
// argument. Used to insert species names that were loaded as character arrays.
static void TVInterview_SetTemplateString(StringTemplate *template, int idx, const u16 *chars, int gender, int language, int unused)
{
    String *string = String_Init(64, HEAP_ID_FIELD1);

    String_CopyChars(string, chars);
    StringTemplate_SetString(template, idx, string, gender, unused, language);
    String_Free(string);
}

// Fills the template with the species name of the first hatched party Pokemon.
static void TVInterview_LoadYourPokemonCornerMessage(FieldSystem *fieldSystem, StringTemplate *template)
{
    Party *party = SaveData_GetParty(fieldSystem->saveData);
    Pokemon *mon = Party_FindFirstHatchedMon(party);

    StringTemplate_SetSpeciesName(template, 0, Pokemon_GetBoxPokemon(mon));
}

// Fills the template with the name of the Poketch app currently on screen.
static void TVInterview_LoadPoketchWatchMessage(FieldSystem *fieldSystem, StringTemplate *template)
{
    int appID = PoketchSystem_CurrentAppID(fieldSystem->unk_04->poketchSys);
    StringTemplate_SetPoketchAppName(template, 0, appID);
}

// Fills the template with the species name of the Pokemon watched in Amity
// Square.
static void TVInterview_LoadAmitySquareWatchMessage(FieldSystem *fieldSystem, StringTemplate *template)
{
    u16 speciesName[10 + 1];
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    TVSegment_AmitySquareWatchData *watch = TVBroadcast_GetAmitySquareWatch(broadcast);

    MessageLoader_GetSpeciesName(watch->species, HEAP_ID_FIELD1, speciesName);
    TVInterview_SetTemplateString(template, 0, speciesName, 0, GAME_LANGUAGE, 1);
}

// Fills the template with the species name from the single Battle Frontier
// Frontline News segment.
static void TVInterview_LoadFrontlineNewsSingleMessage(FieldSystem *fieldSystem, StringTemplate *template)
{
    u16 speciesName[10 + 1];
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    TVSegment_BattleFrontierFrontlineNewsSingleData *news = TVBroadcast_GetFrontlineNewsSingle(broadcast);

    MessageLoader_GetSpeciesName(news->species, HEAP_ID_FIELD1, speciesName);
    TVInterview_SetTemplateString(template, 0, speciesName, 0, GAME_LANGUAGE, 1);
}

// Fills the template with the trainer name from the multi Battle Frontier
// Frontline News segment.
static void TVInterview_LoadFrontlineNewsMultiMessage(FieldSystem *fieldSystem, StringTemplate *template)
{
    String *trainerName;
    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    TVSegment_BattleFrontierFrontlineNewsMultiData *news = TVBroadcast_GetFrontlineNewsMulti(broadcast);

    trainerName = String_Init(64, HEAP_ID_FIELD1);

    String_CopyChars(trainerName, news->trainerName);
    StringTemplate_SetString(template, 0, trainerName, news->gender, 1, GAME_LANGUAGE);
    String_Free(trainerName);
}

static BOOL TVInterview_IsBattleTowerCornerEligible(FieldSystem *fieldSystem)
{
    TVSegment_BattleTowerCornerData *data = TVBroadcast_GetBattleTowerCorner(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->active;
}

static BOOL TVInterview_IsPoketchWatchEligible(FieldSystem *fieldSystem)
{
    Poketch *poketch = SaveData_GetPoketch(fieldSystem->saveData);
    return Poketch_IsEnabled(poketch);
}

static BOOL TVInterview_IsContestHallEligible(FieldSystem *fieldSystem)
{
    TVSegment_ContestHall_ShowcasedPokemon *data = TVBroadcast_GetShowcasedPokemon(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->unk_00;
}

static BOOL TVInterview_IsRightOnPhotoCornerEligible(FieldSystem *fieldSystem)
{
    ImageClips *imageClips = SaveData_GetImageClips(fieldSystem->saveData);
    return ImageClips_DressUpPhotoHasData(imageClips, 0);
}

static BOOL TVInterview_IsThreeCheersForPoffinCornerEligible(FieldSystem *fieldSystem)
{
    TVSegment_ThreeCheersForPoffinCornerData *data = TVBroadcast_GetPoffinCorner(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->active;
}

static BOOL TVInterview_IsAmitySquareWatchEligible(FieldSystem *fieldSystem)
{
    TVSegment_AmitySquareWatchData *data = TVBroadcast_GetAmitySquareWatch(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->active;
}

static BOOL TVInterview_IsFrontlineNewsSingleEligible(FieldSystem *fieldSystem)
{
    TVSegment_BattleFrontierFrontlineNewsSingleData *data = TVBroadcast_GetFrontlineNewsSingle(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->active;
}

static BOOL TVInterview_IsFrontlineNewsMultiEligible(FieldSystem *fieldSystem)
{
    TVSegment_BattleFrontierFrontlineNewsMultiData *data = TVBroadcast_GetFrontlineNewsMulti(SaveData_GetTVBroadcast(fieldSystem->saveData));
    return data->active;
}

// Interview segment table, indexed by segment ID minus one. Unused segments
// have no callbacks and point at a dummy message.
static const TVInterview sInterviews[TV_PROGRAM_TYPE_INTERVIEWS_NUM_SEGMENTS] = {
    [TV_PROGRAM_SEGMENT_INTERVIEW_UNUSED_01 - 1] = {
        .saveResponseFn = NULL,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_Dummy3,
    },
    [TV_PROGRAM_SEGMENT_BATTLE_TOWER_CORNER - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_BattleTowerCorner,
        .loadMessageFn = NULL,
        .isEligibleFn = TVInterview_IsBattleTowerCornerEligible,
        .messageID = TVReporterInterviews_Text_BattleTowerCorner,
    },
    [TV_PROGRAM_SEGMENT_INTERVIEW_UNUSED_03 - 1] = {
        .saveResponseFn = NULL,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_Dummy5,
    },
    [TV_PROGRAM_SEGMENT_YOUR_POKEMON_CORNER - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_YourPokemonCorner,
        .loadMessageFn = TVInterview_LoadYourPokemonCornerMessage,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_YourPokemonCorner,
    },
    [TV_PROGRAM_SEGMENT_INTERVIEW_UNUSED_05 - 1] = {
        .saveResponseFn = NULL,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_Dummy7,
    },
    [TV_PROGRAM_SEGMENT_THE_POKETCH_WATCH - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_ThePoketchWatch,
        .loadMessageFn = TVInterview_LoadPoketchWatchMessage,
        .isEligibleFn = TVInterview_IsPoketchWatchEligible,
        .messageID = TVReporterInterviews_Text_ThePoketchWatch,
    },
    [TV_PROGRAM_SEGMENT_CONTEST_HALL - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_ContestHall,
        .loadMessageFn = NULL,
        .isEligibleFn = TVInterview_IsContestHallEligible,
        .messageID = TVReporterInterviews_Text_ContestHall,
    },
    [TV_PROGRAM_SEGMENT_INTERVIEW_UNUSED_08 - 1] = {
        .saveResponseFn = NULL,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_Dummy10,
    },
    [TV_PROGRAM_SEGMENT_RIGHT_ON_PHOTO_CORNER - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_RightOnPhotoCorner,
        .loadMessageFn = NULL,
        .isEligibleFn = TVInterview_IsRightOnPhotoCornerEligible,
        .messageID = TVReporterInterviews_Text_RightOnPhotoCorner,
    },
    [TV_PROGRAM_SEGMENT_STREET_CORNER_PERSONALITY_CHECKUP - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_StreetCornerPersonalityCheckup,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_StreetCornerPersonalityCheckup,
    },
    [TV_PROGRAM_SEGMENT_THREE_CHEERS_FOR_POFFIN_CORNER - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_ThreeCheersForPoffinCorner,
        .loadMessageFn = NULL,
        .isEligibleFn = TVInterview_IsThreeCheersForPoffinCornerEligible,
        .messageID = TVReporterInterviews_Text_ThreeCheersForPoffinCorner,
    },
    [TV_PROGRAM_SEGMENT_INTERVIEW_UNUSED_12 - 1] = {
        .saveResponseFn = NULL,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_Dummy14,
    },
    [TV_PROGRAM_SEGMENT_AMITY_SQUARE_WATCH - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_AmitySquareWatch,
        .loadMessageFn = TVInterview_LoadAmitySquareWatchMessage,
        .isEligibleFn = TVInterview_IsAmitySquareWatchEligible,
        .messageID = TVReporterInterviews_Text_AmitySquareWatch,
    },
    [TV_PROGRAM_SEGMENT_BATTLE_FRONTIER_FRONTLINE_NEWS_SINGLE - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_BattleFrontierFrontlineNews_Single,
        .loadMessageFn = TVInterview_LoadFrontlineNewsSingleMessage,
        .isEligibleFn = TVInterview_IsFrontlineNewsSingleEligible,
        .messageID = TVReporterInterviews_Text_BattleFrontierFrontlineNews_Single,
    },
    [TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_1 - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_InYourFaceInterview_Question1,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_InYourFaceInterview_Question1,
    },
    [TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_2 - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_InYourFaceInterview_Question2,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_InYourFaceInterview_Question2,
    },
    [TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_3 - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_InYourFaceInterview_Question3,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_InYourFaceInterview_Question3,
    },
    [TV_PROGRAM_SEGMENT_IN_YOUR_FACE_INTERVIEW_QUESTION_4 - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_InYourFaceInterview_Question4,
        .loadMessageFn = NULL,
        .isEligibleFn = NULL,
        .messageID = TVReporterInterviews_Text_InYourFaceInterview_Question4,
    },
    [TV_PROGRAM_SEGMENT_BATTLE_FRONTIER_FRONTLINE_NEWS_MULTI - 1] = {
        .saveResponseFn = FieldSystem_SaveTVSegment_BattleFrontierFrontlineNews_Multi,
        .loadMessageFn = TVInterview_LoadFrontlineNewsMultiMessage,
        .isEligibleFn = TVInterview_IsFrontlineNewsMultiEligible,
        .messageID = TVReporterInterviews_Text_BattleFrontierFrontlineNews_Multi,
    },
};

// Writes the number of Pokemon caught in the current Safari Game into a script
// variable.
BOOL ScrCmd_GetCurrentSafariGameCaughtNum(ScriptContext *ctx)
{
    TVBroadcast *broadcast;
    TVSegment_SafariGameData *safariGame;
    u16 *destVar = ScriptContext_GetVarPointer(ctx);

    broadcast = SaveData_GetTVBroadcast(ctx->fieldSystem->saveData);
    safariGame = TVBroadcast_GetSafariGameData(broadcast);
    *destVar = safariGame->numPokemonCaught;

    return FALSE;
}

// Writes the position, facing direction, and movement type of the Battle
// Frontier reporter into script variables. The position depends on which
// facility the multi Battle Frontier Frontline News segment was recorded at
// (1 = Tower, 2/3 = Factory, 4 = Castle, 5 = Hall, 6 = Arcade).
BOOL ScrCmd_GetBattleFrontierReporterPosition(ScriptContext *ctx)
{
    TVBroadcast *broadcast;
    TVSegment_BattleFrontierFrontlineNewsMultiData *news;
    u16 *x = ScriptContext_GetVarPointer(ctx);
    u16 *z = ScriptContext_GetVarPointer(ctx);
    u16 *dir = ScriptContext_GetVarPointer(ctx);
    u16 *movementType = ScriptContext_GetVarPointer(ctx);

    broadcast = SaveData_GetTVBroadcast(ctx->fieldSystem->saveData);
    news = TVBroadcast_GetFrontlineNewsMulti(broadcast);

    switch (news->facility) {
    case 5:
        *x = 25;
        *z = 36;
        *dir = DIR_NORTH;
        *movementType = MOVEMENT_TYPE_LOOK_NORTH;
        break;
    case 4:
        *x = 37;
        *z = 61;
        *dir = DIR_SOUTH;
        *movementType = MOVEMENT_TYPE_LOOK_SOUTH;
        break;
    case 6:
        *x = 59;
        *z = 61;
        *dir = DIR_SOUTH;
        *movementType = MOVEMENT_TYPE_LOOK_SOUTH;
        break;
    case 2:
    case 3:
        *x = 72;
        *z = 36;
        *dir = DIR_NORTH;
        *movementType = MOVEMENT_TYPE_LOOK_NORTH;
        break;
    case 1:
    default:
        *x = 47;
        *z = 21;
        *dir = DIR_NORTH;
        *movementType = MOVEMENT_TYPE_LOOK_NORTH;
        break;
    }

    return FALSE;
}
