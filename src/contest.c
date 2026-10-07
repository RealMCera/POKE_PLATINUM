#include "contest.h"

#include <nitro.h>
#include <string.h>

#include "constants/contests.h"
#include "constants/heap.h"
#include "generated/accessories.h"
#include "generated/game_records.h"
#include "generated/object_events_gfx.h"
#include "generated/pokemon_contest_ranks.h"
#include "generated/trainer_score_events.h"

#include "struct_decls/tv_broadcast.h"
#include "struct_defs/contest_camera_flash_task.h"
#include "struct_defs/contest_player_mon_dto.h"
#include "struct_defs/image_clips.h"
#include "struct_defs/struct_02029C88.h"
#include "struct_defs/visual_competition_app_args.h"

#include "field/field_system.h"
#include "overlay006/ov6_022489E4.h"
#include "overlay006/struct_ov6_02248DD8.h"
#include "overlay017/ov17_0223B140.h"
#include "overlay017/ov17_0223CB1C.h"
#include "overlay017/ov17_0223DAD0.h"
#include "overlay017/ov17_0224F4D4.h"
#include "overlay022/ov22_02255D44.h"
#include "pch/global_pch.h"
#include "savedata/save_table.h"

#include "assert.h"
#include "brightness_controller.h"
#include "chatot_cry.h"
#include "communication_information.h"
#include "communication_system.h"
#include "field_task.h"
#include "field_transition.h"
#include "game_records.h"
#include "heap.h"
#include "journal.h"
#include "link_contest_records.h"
#include "math_util.h"
#include "party.h"
#include "pokedex.h"
#include "pokemon.h"
#include "ribbon.h"
#include "rtc.h"
#include "savedata.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "string_template.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "system_flags.h"
#include "trainer_info.h"
#include "tv_segment.h"
#include "image_clips.h"
#include "unk_020363E8.h"
#include "unk_02094EDC.h"
#include "unk_02095AF0.h"
#include "vars_flags.h"

FS_EXTERN_OVERLAY(overlay17);
FS_EXTERN_OVERLAY(overlay22);

typedef struct ContestTaskEnv {
    Contest *contest;
    int state;
} ContestTaskEnv;

static Contest *Contest_New(void);
static void Contest_InternalFree(Contest *contest);
static void Contest_SetLCRNGSeed(Contest *contest);
u16 Contest_GetRNGNext(Contest *contest);
BOOL Contest_SetUpLinkContest(Contest *contest);
static BOOL Contest_IsCommTaskDoneInternal(Contest *contest);
static void Contest_LinkContestCommTask(SysTask *param0, void *param1);
static BOOL FieldTask_RunContest(FieldTask *param0);
static BOOL FieldTask_RunPracticeCompetition(FieldTask *param0);
void Contest_GetVisualCompetitionAppArgs(Contest *contest);
void Contest_VisualCompetitionAppArgsFree(Contest *contest);
static void Contest_StartPhotoCommTask(Contest *contest);
static void Contest_PhotoCommTask(SysTask *param0, void *param1);
static void Contest_FillContestantSummary(Contest *contest, int contestantID, UnkStruct_ov6_02248DD8 *param2);
static void SysTask_DoContestCameraFlash(SysTask *sysTask, void *contestParam);
static int CalcMonContestFame(Pokemon *mon, enum PokemonContestType contestType);
static void Contest_InitNPCContestants(Contest *contest, int isGameCompleted, int isNatDexObtained);

const ApplicationManagerTemplate ActingCompetitionAppTemplate = {
    ActingCompetition_Init,
    ActingCompetition_Main,
    ActingCompetition_Exit,
    FS_OVERLAY_ID(overlay17)
};

const ApplicationManagerTemplate DanceCompetitionAppTemplate = {
    DanceCompetition_Init,
    DanceCompetition_Main,
    DanceCompetition_Exit,
    FS_OVERLAY_ID(overlay17)
};

const ApplicationManagerTemplate VisualCompetitionScoringAppTemplate = {
    VisualCompetitionScoring_Init,
    VisualCompetitionScoring_Main,
    VisualCompetitionScoring_Exit,
    FS_OVERLAY_ID(overlay17)
};

const ApplicationManagerTemplate ContestFinalScoringAppTemplate = {
    ContestFinalScoring_Init,
    ContestFinalScoring_Main,
    ContestFinalScoring_Exit,
    FS_OVERLAY_ID(overlay17)
};

const ApplicationManagerTemplate VisualCompetitionAppTemplate = {
    VisualCompetition_Init,
    VisualCompetition_Main,
    VisualCompetition_Exit,
    FS_OVERLAY_ID(overlay22)
};

// Frame delays between camera shutter flashes, indexed by the contestant's
// camera flash variant. Each row is terminated by the terminator value.
#define CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR 0xFF

ALIGN_4 static const u8 sNormalRankCameraFrameDelays[][3] = {
    { 0x14, 0x14, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR },
    { 0xF, 0x19, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR }
};

ALIGN_4 static const u8 sGreatRankCameraFrameDelays[][4] = {
    { 0xA, 0xA, 0x1E, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR },
    { 0xF, 0xF, 0xF, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR }
};

ALIGN_4 static const u8 sUltraRankCameraFrameDelays[][5] = {
    { 0xA, 0x8, 0x14, 0x1C, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR },
    { 0xF, 0xF, 0x8, 0x8, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR }
};

ALIGN_4 static const u8 sLinkMasterRankCameraFrameDelays[][6] = {
    { 0x8, 0x8, 0x8, 0x8, 0x1E, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR },
    { 0xF, 0xF, 0x8, 0x8, 0x14, CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR }
};

// Starts the field task that runs a contest. Link and official contests run
// the full sequence of competition apps; practice competitions run only the
// single competition the player chose.
void FieldTask_InitRunContestTask(FieldTask *fieldTask, Contest *contest)
{
    ContestTaskEnv *taskEnv = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(ContestTaskEnv));

    MI_CpuClear8(taskEnv, sizeof(ContestTaskEnv));
    taskEnv->contest = contest;

    switch (contest->data.competitionType) {
    case CONTEST_COMPETITION_UNK0:
    case CONTEST_COMPETITION_UNK1:
    case CONTEST_COMPETITION_LINK_OR_OFFICIAL:
        FieldTask_InitCall(fieldTask, FieldTask_RunContest, taskEnv);
        break;
    default:
        FieldTask_InitCall(fieldTask, FieldTask_RunPracticeCompetition, taskEnv);
        break;
    }
}

enum ContestManagerState {
    CONTEST_MANAGER_STATE_START,
    CONTEST_MANAGER_STATE_WAIT_FOR_COMM_TASK,
    CONTEST_MANAGER_STATE_START_COMM_SYNC_0,
    CONTEST_MANAGER_STATE_CONFIRM_COMM_SYNC_0,
    CONTEST_MANAGER_STATE_RUN_APP_VISUAL_COMPETITION,
    CONTEST_MANAGER_STATE_5,
    CONTEST_MANAGER_STATE_WAIT_FOR_COMM_TASK_2,
    CONTEST_MANAGER_STATE_RUN_APP_VISUAL_COMPETITION_SCORING,
    CONTEST_MANAGER_STATE_RUN_APP_DANCE_COMPETITION,
    CONTEST_MANAGER_STATE_RUN_APP_ACTING_COMPETITION,
    CONTEST_MANAGER_STATE_RUN_APP_FINAL_SCORING,
    CONTEST_MANAGER_STATE_WAIT_ONE_FRAME,
    CONTEST_MANAGER_STATE_RETURN_TO_MAP,
    CONTEST_MANAGER_STATE_END
};

// Runs a link or official contest: visual competition, then visual scoring,
// then the dance and acting competitions, then the final scoring. Each app is
// run as a field application and the state machine waits for the link-contest
// communication task between steps.
static BOOL FieldTask_RunContest(FieldTask *fieldTask)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(fieldTask);
    ContestTaskEnv *taskEnv = FieldTask_GetEnv(fieldTask);

    switch (taskEnv->state) {
    case CONTEST_MANAGER_STATE_START:
        FieldTransition_FinishMap(fieldTask);
        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_WAIT_FOR_COMM_TASK:
        if (taskEnv->contest->isLinkContest == FALSE || Contest_IsCommTaskDoneInternal(taskEnv->contest) == 1) {
            taskEnv->state++;
        }
        break;
    case CONTEST_MANAGER_STATE_START_COMM_SYNC_0:
        if (taskEnv->contest->isLinkContest == TRUE) {
            CommTiming_StartSync(0);
        }

        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_CONFIRM_COMM_SYNC_0:
        if (taskEnv->contest->isLinkContest == TRUE) {
            if (CommTiming_IsSyncState(0) == TRUE) {
                taskEnv->state++;
            }
        } else {
            taskEnv->state++;
        }
        break;
    case CONTEST_MANAGER_STATE_RUN_APP_VISUAL_COMPETITION:
        Contest_GetVisualCompetitionAppArgs(taskEnv->contest);
        FieldTask_RunApplication(fieldTask, &VisualCompetitionAppTemplate, taskEnv->contest->visualCompetitionAppArgs);
        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_5:
        Contest_VisualCompetitionAppArgsFree(taskEnv->contest);

        if (taskEnv->contest->isLinkContest == TRUE) {
            Contest_StartPhotoCommTask(taskEnv->contest);
            taskEnv->state++;
        } else {
            taskEnv->state = CONTEST_MANAGER_STATE_WAIT_FOR_COMM_TASK_2;
        }
        break;
    case CONTEST_MANAGER_STATE_WAIT_FOR_COMM_TASK_2:
        if (taskEnv->contest->isLinkContest == FALSE || Contest_IsCommTaskDoneInternal(taskEnv->contest) == TRUE) {
            taskEnv->state++;
        }
        break;
    case CONTEST_MANAGER_STATE_RUN_APP_VISUAL_COMPETITION_SCORING:
        FieldTask_RunApplication(fieldTask, &VisualCompetitionScoringAppTemplate, taskEnv->contest);
        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_RUN_APP_DANCE_COMPETITION:
        if (taskEnv->contest->data.competitionType == CONTEST_COMPETITION_UNK1 || taskEnv->contest->data.competitionType == CONTEST_COMPETITION_LINK_OR_OFFICIAL) {
            FieldTask_RunApplication(fieldTask, &DanceCompetitionAppTemplate, taskEnv->contest);
        }

        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_RUN_APP_ACTING_COMPETITION:
        if (taskEnv->contest->data.competitionType == CONTEST_COMPETITION_UNK0 || taskEnv->contest->data.competitionType == CONTEST_COMPETITION_LINK_OR_OFFICIAL) {
            FieldTask_RunApplication(fieldTask, &ActingCompetitionAppTemplate, taskEnv->contest);
        }

        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_RUN_APP_FINAL_SCORING:
        FieldTask_RunApplication(fieldTask, &ContestFinalScoringAppTemplate, taskEnv->contest);
        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_WAIT_ONE_FRAME:
        taskEnv->state++;
        break;
    case CONTEST_MANAGER_STATE_RETURN_TO_MAP:
        FieldTransition_StartMap(fieldTask);
        taskEnv->state++;
        break;
    default:
        GF_ASSERT(FALSE);
    case CONTEST_MANAGER_STATE_END:
        Heap_Free(taskEnv);
        return TRUE;
    }

    return FALSE;
}

// Runs a single practice competition (visual, dance, or acting) and then
// computes the player's placement from the relevant score.
static BOOL FieldTask_RunPracticeCompetition(FieldTask *fieldTask)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(fieldTask);
    ContestTaskEnv *taskEnv = FieldTask_GetEnv(fieldTask);

    switch (taskEnv->state) {
    case 0:
        FieldTransition_FinishMap(fieldTask);
        taskEnv->state++;
        break;
    case 1:
        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_VISUAL:
        case CONTEST_COMPETITION_VISUAL:
            Contest_GetVisualCompetitionAppArgs(taskEnv->contest);
            FieldTask_RunApplication(fieldTask, &VisualCompetitionAppTemplate, taskEnv->contest->visualCompetitionAppArgs);
            break;
        }

        taskEnv->state++;
        break;
    case 2:
        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_VISUAL:
        case CONTEST_COMPETITION_VISUAL:
            Contest_VisualCompetitionAppArgsFree(taskEnv->contest);
            break;
        }

        taskEnv->state++;
        break;
    case 3:
        taskEnv->state++;
        break;
    case 4:
        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_VISUAL:
        case CONTEST_COMPETITION_VISUAL:
            FieldTask_RunApplication(fieldTask, &VisualCompetitionScoringAppTemplate, taskEnv->contest);
            break;
        }

        taskEnv->state++;
        break;
    case 5:
        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_DANCE:
        case CONTEST_COMPETITION_DANCE:
            sub_02095338(taskEnv->contest);
            FieldTask_RunApplication(fieldTask, &DanceCompetitionAppTemplate, taskEnv->contest);
            break;
        }

        taskEnv->state++;
        break;
    case 6:
        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_ACTING:
        case CONTEST_COMPETITION_ACTING:
            FieldTask_RunApplication(fieldTask, &ActingCompetitionAppTemplate, taskEnv->contest);
            break;
        }

        taskEnv->state++;
        break;
    case 7: {
        s32 v2[4];
        int v3, v4;

        switch (taskEnv->contest->data.competitionType) {
        case CONTEST_COMPETITION_PRACTICE_VISUAL:
        case CONTEST_COMPETITION_VISUAL:
            for (v3 = 0; v3 < 4; v3++) {
                v2[v3] = sub_02095928(taskEnv->contest, v3)
                    + sub_0209598C(taskEnv->contest, v3);
            }
            break;
        case CONTEST_COMPETITION_PRACTICE_DANCE:
        case CONTEST_COMPETITION_DANCE:
            for (v3 = 0; v3 < 4; v3++) {
                v2[v3] = taskEnv->contest->data.results[v3].danceCompetitionScore;
            }
            break;
        case CONTEST_COMPETITION_PRACTICE_ACTING:
        case CONTEST_COMPETITION_ACTING:
            for (v3 = 0; v3 < 4; v3++) {
                v2[v3] = taskEnv->contest->data.results[v3].actingScore;
            }
            break;
        }

        v4 = 0;

        for (v3 = 1; v3 < 4; v3++) {
            if (v2[0] < v2[v3]) {
                v4++;
            }
        }

        taskEnv->contest->data.results[0].contestPlacement = v4;
    }
        taskEnv->state++;
        break;
    case 8:
        FieldTransition_StartMap(fieldTask);
        taskEnv->state++;
        break;
    default:
        GF_ASSERT(FALSE);
    case 9:
        Heap_Free(taskEnv);
        return 1;
    }

    return 0;
}

// Allocates a contest with the player as the only connected contestant; the
// remaining slots are filled with NPCs.
static Contest *Contest_New(void)
{
    Contest *contest = Heap_Alloc(HEAP_ID_20, sizeof(Contest));
    MI_CpuClear8(contest, sizeof(Contest));

    contest->data.playerContestantID = PLAYER_CONTESTANT_ID;
    contest->data.connectionCount = 1;
    contest->data.npcCount = CONTEST_NUM_PARTICIPANTS - 1;

    return contest;
}

static void Contest_InternalFree(Contest *contest)
{
    Heap_Free(contest);
}

// Sets up a contest from the player's chosen Pokemon and competition. The
// player's contestant data is filled in here; the NPC contestants are set up
// by Contest_InitNPCContestants.
Contest *Contest_Init(const PlayerMonContestDTO *playerMonContestDTO)
{
    Heap_Create(HEAP_ID_FIELD2, HEAP_ID_20, 0x3000 + 0x1000);

    Contest *contest = Contest_New();
    contest->rngSeed = LCRNG_GetSeed();

    Contest_SetLCRNGSeed(contest);

    contest->data.contestType = playerMonContestDTO->contestType;
    contest->data.contestRank = playerMonContestDTO->contestRank;
    contest->data.competitionType = playerMonContestDTO->competitionType;
    contest->data.npcPhotoPreset = sub_02095A74(playerMonContestDTO->contestRank, FALSE);
    contest->data.leaderContestantID = contest->data.playerContestantID;
    contest->data.leaderElectionValue = 110;
    contest->data.leaderElectionResult = contest->data.leaderElectionValue;
    contest->data.bonusJudgeIndex = 1;
    contest->imageClips = playerMonContestDTO->imageClips;
    contest->options = playerMonContestDTO->options;
    contest->saveData = playerMonContestDTO->saveData;
    contest->playerMon = playerMonContestDTO->mon;
    contest->monPartySlot = playerMonContestDTO->monPartySlot;
    contest->trainerInfo = playerMonContestDTO->trainerInfo;
    contest->isGameCompleted = playerMonContestDTO->isGameCompleted;
    contest->isNatDexObtained = playerMonContestDTO->isNatDexObtained;

    int i;
    for (i = 0; i < 4; i++) {
        contest->data.photos[i] = ContestPhoto_New(HEAP_ID_20);
    }

    sub_020954F0(contest, HEAP_ID_FIELD2, contest->data.bonusJudgeIndex, contest->data.contestType, contest->data.contestRank);
    contest->party = Party_New(HEAP_ID_20);

    for (i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->data.contestMons[i] = Pokemon_New(HEAP_ID_20);
    }

    for (i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->chatotCry[i] = ChatotCry_New(HEAP_ID_20);
    }

    ChatotCry_Copy(contest->chatotCry[PLAYER_CONTESTANT_ID], playerMonContestDTO->chatotCry);

    {
        Pokemon_Copy(playerMonContestDTO->mon, contest->data.contestMons[PLAYER_CONTESTANT_ID]);
        contest->data.trainerNames[PLAYER_CONTESTANT_ID] = String_Init(8, HEAP_ID_20);
        String_Copy(contest->data.trainerNames[PLAYER_CONTESTANT_ID], playerMonContestDTO->trainerName);

        contest->data.trainerGenders[PLAYER_CONTESTANT_ID] = TrainerInfo_Gender(playerMonContestDTO->trainerInfo);
        contest->data.cameraFlashVariant[PLAYER_CONTESTANT_ID] = 0;
        contest->data.monContestFame[PLAYER_CONTESTANT_ID] = CalcMonContestFame(contest->data.contestMons[PLAYER_CONTESTANT_ID], contest->data.contestType);

        if (Contest_IsPracticeCompetition(contest) == FALSE) {
            if (TrainerInfo_Gender(playerMonContestDTO->trainerInfo) == GENDER_MALE) {
                contest->data.contestantObjEventGFX[PLAYER_CONTESTANT_ID] = OBJ_EVENT_GFX_PLAYER_M_CONTEST;
            } else {
                contest->data.contestantObjEventGFX[PLAYER_CONTESTANT_ID] = OBJ_EVENT_GFX_PLAYER_F_CONTEST;
            }
        } else {
            if (TrainerInfo_Gender(playerMonContestDTO->trainerInfo) == GENDER_MALE) {
                contest->data.contestantObjEventGFX[PLAYER_CONTESTANT_ID] = OBJ_EVENT_GFX_PLAYER_M;
            } else {
                contest->data.contestantObjEventGFX[PLAYER_CONTESTANT_ID] = OBJ_EVENT_GFX_PLAYER_F;
            }
        }
    }

    Contest_InitNPCContestants(contest, playerMonContestDTO->isGameCompleted, playerMonContestDTO->isNatDexObtained);

    if (Contest_IsPracticeCompetition(contest) == TRUE) {
        for (i = 0; i < 4; i++) {
            contest->contestantOrder[i] = 4 - i - 1;
        }
    } else {
        for (i = 0; i < 4; i++) {
            contest->contestantOrder[i] = i;
        }
    }

    return contest;
}

// Picks the NPC contestants from the contest NARC and copies their data into
// the contest. Slot 0 is the player, so the loops start at 1.
static void Contest_InitNPCContestants(Contest *contest, int isGameCompleted, int isNatDexObtained)
{
    int v0 = 4 - 1;
    int i;

    sub_02094F04(contest, HEAP_ID_FIELD2, v0, contest->data.contestType, contest->data.contestRank, contest->data.competitionType, isGameCompleted, isNatDexObtained);

    // starts at 1, because 0 was already initialized for player
    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        sub_02095380(&contest->data.opponentData[i], contest->data.contestMons[i], HEAP_ID_20);
    }

    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        if (contest->data.trainerNames[i] == NULL) {
            contest->data.trainerNames[i] = String_Init(8, HEAP_ID_20);
        }

        Pokemon_GetValue(contest->data.contestMons[i], MON_DATA_OT_NAME_STRING, contest->data.trainerNames[i]);
    }

    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->data.trainerGenders[i] = contest->data.opponentData[i].unk_20_12;
    }

    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->data.cameraFlashVariant[i] = contest->data.opponentData[i].unk_20_14;
    }

    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->data.monContestFame[i] = contest->data.opponentData[i].unk_2E;
    }

    for (i = 1; i < CONTEST_NUM_PARTICIPANTS; i++) {
        contest->data.contestantObjEventGFX[i] = contest->data.opponentData[i].unk_08;
    }

    sub_020951B0(contest, HEAP_ID_FIELD2);
}

void Contest_Free(Contest *contest)
{
    Heap_Free(contest->party);

    for (int i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        Heap_Free(contest->data.contestMons[i]);
        String_Free(contest->data.trainerNames[i]);
        Heap_Free(contest->data.photos[i]);
        Heap_Free(contest->chatotCry[i]);
    }

    LCRNG_SetSeed(contest->rngSeed);
    Contest_InternalFree(contest);
    Heap_Destroy(HEAP_ID_20);
}

// Contest fame is one plus the number of consecutive super contest ribbons
// the Pokemon has won in this contest type, starting from the normal rank.
static int CalcMonContestFame(Pokemon *mon, enum PokemonContestType contestType)
{
    int ribbon;
    int fame = 1;

    for (int i = 0; i <= CONTEST_NUM_RANKS; i++) {
        switch (contestType) {
        case CONTEST_TYPE_COOL:
            ribbon = Pokemon_GetValue(mon, MON_DATA_SUPER_COOL_RIBBON + i, NULL);
            break;
        case CONTEST_TYPE_BEAUTY:
            ribbon = Pokemon_GetValue(mon, MON_DATA_SUPER_BEAUTY_RIBBON + i, NULL);
            break;
        case CONTEST_TYPE_CUTE:
            ribbon = Pokemon_GetValue(mon, MON_DATA_SUPER_CUTE_RIBBON + i, NULL);
            break;
        case CONTEST_TYPE_SMART:
            ribbon = Pokemon_GetValue(mon, MON_DATA_SUPER_SMART_RIBBON + i, NULL);
            break;
        case CONTEST_TYPE_TOUGH:
            ribbon = Pokemon_GetValue(mon, MON_DATA_SUPER_TOUGH_RIBBON + i, NULL);
            break;
        default:
            GF_ASSERT(FALSE);
            break;
        }

        if (ribbon == 0) {
            break;
        }

        fame++;
    }

    return fame;
}

// Builds the argument block for the visual competition app. In a link contest
// the app uses the link rank so that all players see the same rank title.
void Contest_GetVisualCompetitionAppArgs(Contest *contest)
{
    VisualCompetitionAppArgs *appArgs;

    ContestPhoto_Init(contest->data.photos[contest->data.playerContestantID]);

    appArgs = Heap_Alloc(HEAP_ID_20, sizeof(VisualCompetitionAppArgs));
    MI_CpuClear8(appArgs, sizeof(VisualCompetitionAppArgs));

    appArgs->mon = contest->data.contestMons[contest->data.playerContestantID];
    appArgs->photo = contest->data.photos[contest->data.playerContestantID];
    appArgs->npcPhotoPreset = contest->data.npcPhotoPreset;

    if (contest->isLinkContest == TRUE) {
        appArgs->contestRank = CONTEST_RANK_LINK;
    } else {
        appArgs->contestRank = contest->data.contestRank;
    }

    appArgs->competitionType = contest->data.competitionType;
    appArgs->contestType = contest->data.contestType;
    appArgs->fashionCase = ImageClips_GetFashionCase(contest->imageClips);
    appArgs->scoringCommState = &contest->scoringCommState;
    appArgs->options = contest->options;
    appArgs->trainerInfo = contest->trainerInfo;

    contest->visualCompetitionAppArgs = appArgs;
}

void Contest_VisualCompetitionAppArgsFree(Contest *contest)
{
    Heap_Free(contest->visualCompetitionAppArgs);
    contest->visualCompetitionAppArgs = NULL;
}

// Communication task that exchanges the data needed to start a link contest:
// the leader is elected, then the contestants' Pokemon, opponent data, trainer
// names, Chatot cries, contestant summaries, and judges are broadcast. Each
// step starts a sync, waits for it, sends/receives, and waits until every
// connected player has replied (tracked by commRecvCount).
static void Contest_LinkContestCommTask(SysTask *sysTask, void *param1)
{
    Contest *contest = param1;

    switch (contest->linkState) {
    case 0:
        CommTiming_StartSync(5);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 1:
        if (CommTiming_IsSyncState(5) == 1) {
            contest->linkState++;
        }
        break;
    case 2:
        if (ov6_02248A64(contest) == 1) {
            contest->linkState++;
        }
        break;
    case 3:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 4:
        CommTiming_StartSync(6);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 5:
        if (CommTiming_IsSyncState(6) == 1) {
            contest->linkState++;
        }
        break;
    case 6:
        if (ov6_02248AF0(contest, contest->data.playerContestantID, contest->data.contestMons[PLAYER_CONTESTANT_ID]) == 1) {
            contest->linkState++;
        }
        break;
    case 7:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 8:
        CommTiming_StartSync(7);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 9:
        if (CommTiming_IsSyncState(7) == 1) {
            contest->linkState++;
        }
        break;
    case 10:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            if (ov6_02248B70(contest, contest->data.contestMons) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 11:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 12:
        CommTiming_StartSync(8);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 13:
        if (CommTiming_IsSyncState(8) == 1) {
            contest->linkState++;
        }
        break;
    case 14:
        if (ov6_02248BE8(contest, contest->data.playerContestantID, &contest->data.opponentData[0]) == 1) {
            contest->linkState++;
        }
        break;
    case 15:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 16:
        CommTiming_StartSync(9);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 17:
        if (CommTiming_IsSyncState(9) == 1) {
            contest->linkState++;
        }
        break;
    case 18:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            if (ov6_02248BE8(contest, contest->commContestantIndex, &contest->data.opponentData[contest->commContestantIndex]) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 19:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->commContestantIndex++;

            if (contest->commContestantIndex < 4) {
                contest->linkState--;
            } else {
                contest->commContestantIndex = 0;
                contest->linkState++;
            }
        }
        break;
    case 20:
        CommTiming_StartSync(10);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 21:
        if (CommTiming_IsSyncState(10) == 1) {
            contest->linkState++;
        }
        break;
    case 22:
        if (ov6_02248CE8(contest, contest->data.playerContestantID, contest->data.trainerNames[PLAYER_CONTESTANT_ID]) == 1) {
            contest->linkState++;
        }
        break;
    case 23:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            {
                int netID;
                const TrainerInfo *v2;

                for (netID = 0; netID < contest->data.connectionCount; netID++) {
                    v2 = CommInfo_TrainerInfo(netID);
                    String_Clear(contest->data.trainerNames[netID]);
                    TrainerInfo_NameString(v2, contest->data.trainerNames[netID]);
                }
            }

            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 24:
        CommTiming_StartSync(11);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 25:
        if (CommTiming_IsSyncState(11) == 1) {
            contest->linkState++;
        }
        break;
    case 26:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            if (ov6_02248CE8(contest, contest->commContestantIndex, contest->data.trainerNames[contest->commContestantIndex]) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 27:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->commContestantIndex++;

            if (contest->commContestantIndex < 4) {
                contest->linkState--;
            } else {
                contest->commContestantIndex = 0;
                contest->linkState++;
            }
        }
        break;
    case 28:
        MI_CpuCopy8(contest->chatotCry[PLAYER_CONTESTANT_ID], contest->commSendBuf, ChatotCry_SaveSize());
        CommTiming_StartSync(12);

        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 29:
        if (CommTiming_IsSyncState(12) == 1) {
            contest->linkState++;
        }
        break;
    case 30:
        if (ov6_02248D64(contest, contest->data.playerContestantID, NULL) == 1) {
            contest->linkState++;
        }
        break;
    case 31:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 32:
        CommTiming_StartSync(13);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 33:
        if (CommTiming_IsSyncState(13) == 1) {
            contest->linkState++;
        }
        break;
    case 34:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            if (ov6_02248D64(contest, contest->commContestantIndex, contest->chatotCry[contest->commContestantIndex]) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 35:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->commContestantIndex++;

            if (contest->commContestantIndex < 4) {
                contest->linkState--;
            } else {
                contest->commContestantIndex = 0;
                contest->linkState++;
            }
        }
        break;
    case 36:
        CommTiming_StartSync(14);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 37:
        if (CommTiming_IsSyncState(14) == 1) {
            contest->linkState++;
        }
        break;
    case 38: {
        UnkStruct_ov6_02248DD8 v3;

        Contest_FillContestantSummary(contest, 0, &v3);

        if (ov6_02248DD8(contest, contest->data.playerContestantID, &v3) == 1) {
            contest->linkState++;
        }
    } break;
    case 39:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 40:
        CommTiming_StartSync(15);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 41:
        if (CommTiming_IsSyncState(15) == 1) {
            contest->linkState++;
        }
        break;
    case 42:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            UnkStruct_ov6_02248DD8 v4;

            Contest_FillContestantSummary(contest, contest->commContestantIndex, &v4);

            if (ov6_02248DD8(contest, contest->commContestantIndex, &v4) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 43:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->commContestantIndex++;

            if (contest->commContestantIndex < 4) {
                contest->linkState--;
            } else {
                contest->commContestantIndex = 0;
                contest->linkState++;
            }
        }
        break;
    case 44:
        CommTiming_StartSync(16);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 45:
        if (CommTiming_IsSyncState(16) == 1) {
            contest->linkState++;
        }
        break;
    case 46:
        if (ov6_02248C68(contest, contest->data.playerContestantID, contest->data.judges) == 1) {
            contest->linkState++;
        }
        break;
    case 47:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    default: {
        int contestantID;
        const TrainerInfo *trainerInfo;

        for (contestantID = 0; contestantID < contest->data.connectionCount; contestantID++) {
            trainerInfo = CommInfo_TrainerInfo(contestantID);
            contest->data.trainerGenders[contestantID] = TrainerInfo_Gender(trainerInfo);
        }

        for (; contestantID < CONTEST_NUM_PARTICIPANTS; contestantID++) {
            contest->data.trainerGenders[contestantID] = contest->data.opponentData[contestantID].unk_20_12;
        }
    }

        contest->scoringCommState.leaderContestantID = contest->data.leaderContestantID;
        contest->scoringCommState.netID = contest->data.netID;
        contest->scoringCommState.isLinkContest = 1;
        contest->scoringCommState.connectionCount = contest->data.connectionCount;

        contest->commTask = NULL;
        contest->linkState = 0;

        SysTask_Done(sysTask);
        return;
    }
}

// Turns the contest into a link contest: the game-completion and National Dex
// flags are only set if every connected player has them, and the NPC count is
// reduced by the number of connected players. Starts the setup comm task.
BOOL Contest_SetUpLinkContest(Contest *contest)
{
    if (CommSys_IsInitialized() == FALSE) {
        return FALSE;
    }

    contest->isLinkContest = TRUE;

    int connectionCount = CommSys_ConnectedCount();
    int netID = CommSys_CurNetId();

    {
        int i;
        TrainerInfo *connectedTrainerInfo;

        for (i = 0; i < connectionCount; i++) {
            connectedTrainerInfo = CommInfo_TrainerInfo(i);
            GF_ASSERT(connectedTrainerInfo != NULL);

            if (TrainerInfo_IsMainStoryCleared(connectedTrainerInfo) == FALSE) {
                break;
            }
        }

        if (i != connectionCount) {
            contest->isGameCompleted = FALSE;
        } else {
            contest->isGameCompleted = TRUE;
        }

        for (i = 0; i < connectionCount; i++) {
            connectedTrainerInfo = CommInfo_TrainerInfo(i);
            GF_ASSERT(connectedTrainerInfo != NULL);

            if (TrainerInfo_HasNationalDex(connectedTrainerInfo) == FALSE) {
                break;
            }
        }

        if (i != connectionCount) {
            contest->isNatDexObtained = FALSE;
        } else {
            contest->isNatDexObtained = TRUE;
        }

        Contest_InitNPCContestants(contest, contest->isGameCompleted, contest->isNatDexObtained);

        for (i = 0; i < connectionCount; i++) {
            connectedTrainerInfo = CommInfo_TrainerInfo(i);

            if (TrainerInfo_GameCode(connectedTrainerInfo) == 0) {
                contest->gameCodeMismatchCount++;
            }
        }
    }

    contest->data.netID = netID;
    contest->data.npcCount = CONTEST_NUM_PARTICIPANTS - connectionCount;
    contest->data.connectionCount = connectionCount;
    contest->data.playerContestantID = netID;
    contest->data.leaderElectionValue = 110;
    contest->data.npcPhotoPreset = sub_02095A74(contest->data.contestRank, TRUE);

    sub_02095AF0(contest);

    contest->commTask = SysTask_Start(Contest_LinkContestCommTask, contest, 10);

    return TRUE;
}

BOOL Contest_IsCommTaskDone(Contest *contest)
{
    return Contest_IsCommTaskDoneInternal(contest);
}

BOOL Contest_IsCommTaskDoneInternal(Contest *contest)
{
    if (contest->commTask == NULL) {
        return TRUE;
    }

    return FALSE;
}

static void Contest_StartPhotoCommTask(Contest *contest)
{
    GF_ASSERT(contest->commTask == NULL);

    contest->linkState = 0;
    contest->commTask = SysTask_Start(Contest_PhotoCommTask, contest, 10);
}

// Communication task that exchanges the contestants' contest photos after the
// visual competition. The leader broadcasts all four photos at once.
static void Contest_PhotoCommTask(SysTask *sysTask, void *param1)
{
    Contest *contest = param1;

    switch (contest->linkState) {
    case 0:
        CommTiming_StartSync(17);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 1:
        if (CommTiming_IsSyncState(17) == 1) {
            contest->linkState++;
        }
        break;
    case 2:
        if (sub_02095B5C(contest, contest->data.playerContestantID, contest->data.photos[contest->data.playerContestantID]) == 1) {
            contest->linkState++;
        }
        break;
    case 3:
        if (contest->commRecvCount >= contest->data.connectionCount) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    case 4:
        CommTiming_StartSync(18);
        contest->commRecvCount = 0;
        contest->linkState++;
        break;
    case 5:
        if (CommTiming_IsSyncState(18) == 1) {
            contest->linkState++;
        }
        break;
    case 6:
        if (contest->data.playerContestantID == contest->data.leaderContestantID) {
            if (sub_02095BEC(contest, contest->data.photos) == 1) {
                contest->linkState++;
            }
        } else {
            contest->linkState++;
        }
        break;
    case 7:
        if (contest->commRecvCount > 0) {
            contest->commRecvCount = 0;
            contest->linkState++;
        }
        break;
    default:
        contest->commTask = NULL;
        contest->linkState = 0;
        SysTask_Done(sysTask);
        return;
    }
}

void Contest_BufferJudgeName(Contest *contest, int judgeID, StringTemplate *strTemplate, u32 idx)
{
    StringTemplate_SetContestJudgeName(strTemplate, idx, contest->data.judges[judgeID].judgeNameMessageID);
}

void Contest_BufferContestantTrainerName(Contest *contest, int contestantEntryNum, StringTemplate *strTemplate, u32 idx)
{
    int contestantID = Contest_ContestantEntryNumToContestantID(contestantEntryNum);
    StringTemplate_SetString(strTemplate, idx, contest->data.trainerNames[contestantID], contest->data.trainerGenders[contestantID], 1, GAME_LANGUAGE);
}

void Contest_BufferMonNickname(Contest *contest, int contestantEntryNum, StringTemplate *strTemplate, u32 idx)
{
    BoxPokemon *boxMon;
    int contestantID = Contest_ContestantEntryNumToContestantID(contestantEntryNum);
    boxMon = Pokemon_GetBoxPokemon(contest->data.contestMons[contestantID]);

    StringTemplate_SetNickname(strTemplate, idx, boxMon);
}

void Contest_BufferContestRank(Contest *contest, StringTemplate *strTemplate, u32 idx)
{
    u32 contestRankMessageID = Contest_GetContestRankTitleMessageID(contest->data.contestRank, contest->data.competitionType, contest->isLinkContest);
    StringTemplate_SetContestRankName(strTemplate, idx, contestRankMessageID);
}

void Contest_BufferContestType(Contest *contest, StringTemplate *strTemplate, u32 idx)
{
    u32 contestTypeMessageID = Contest_GetFullContestTypeMessageID(contest->data.contestType, contest->data.competitionType);
    StringTemplate_SetContestTypeName(strTemplate, idx, contestTypeMessageID);
}

void Contest_BufferWinningContestantTrainerName(Contest *contest, StringTemplate *strTemplate, u32 param2)
{
    int i;

    for (i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        if (contest->data.results[i].contestPlacement == 0) {
            break;
        }
    }

    int contestantID = i;
    int contestantEntryNum = Contest_ContestantIDToContestantEntryNum(contestantID);

    Contest_BufferContestantTrainerName(contest, contestantEntryNum, strTemplate, param2);
}

void Contest_BufferWinningContestantMonName(Contest *contest, StringTemplate *strTemplate, u32 idx)
{
    int contestantID, contestantEntryNum;
    int i;

    for (i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        if (contest->data.results[i].contestPlacement == 0) {
            break;
        }
    }

    contestantID = i;
    contestantEntryNum = Contest_ContestantIDToContestantEntryNum(contestantID);

    Contest_BufferMonNickname(contest, contestantEntryNum, strTemplate, idx);
}

BOOL Contest_True(Contest *contest)
{
    return TRUE;
}

void Contest_StartCommSync(Contest *contest, u8 syncNo)
{
    if (contest->isLinkContest == FALSE) {
        return;
    }

    CommTiming_StartSync(syncNo);
}

BOOL Contest_IsSyncState(Contest *contest, u8 syncState)
{
    if (contest->isLinkContest == FALSE) {
        return TRUE;
    }

    if (CommTiming_IsSyncState(syncState) == TRUE) {
        return TRUE;
    }

    return FALSE;
}

int Contest_GetPlayerContestPlacement(Contest *contest)
{
    return contest->data.results[contest->data.playerContestantID].contestPlacement;
}

int Contest_GetWinningContestantEntryNum(Contest *contest)
{
    int contestantID;

    for (contestantID = 0; contestantID < CONTEST_NUM_PARTICIPANTS; contestantID++) {
        if (contest->data.results[contestantID].contestPlacement == 0) {
            return Contest_ContestantIDToContestantEntryNum(contestantID);
        }
    }

    return 0;
}

int Contest_GetPlayerContestantEntryNum(Contest *contest)
{
    return Contest_ContestantIDToContestantEntryNum(contest->data.playerContestantID);
}

int Contest_GetContestantObjEventGFX(Contest *contest, int contestantEntryNum)
{
    int contestantID = Contest_ContestantEntryNumToContestantID(contestantEntryNum);
    int contestantObjEventGFX = contest->data.contestantObjEventGFX[contestantID];

    return contestantObjEventGFX;
}

int Contest_GetContestantMonContestFame(Contest *contest, int contestantEntryNum)
{
    int ContestantID = Contest_ContestantEntryNumToContestantID(contestantEntryNum);
    return contest->data.monContestFame[ContestantID];
}

int Contest_GetContestMode(Contest *contest)
{
    if (contest->isLinkContest == TRUE) {
        return CONTEST_MODE_LINK;
    }

    if (Contest_IsPracticeCompetition(contest) == TRUE) {
        return CONTEST_MODE_PRACTICE;
    }

    return CONTEST_MODE_OFFICIAL;
}

void Contest_GetContestInfo(Contest *contest, u16 *contestRank, u16 *contestType, u16 *competitionType, u16 *monPartySlot)
{
    *contestRank = contest->data.contestRank;
    *contestType = contest->data.contestType;
    *competitionType = contest->data.competitionType;
    *monPartySlot = contest->monPartySlot;
}

void Contest_LockAutoScrollForLinkContests(Contest *contest)
{
    SetLockTextWithAutoScroll(contest->isLinkContest);
}

void Contest_LockTextSpeed(Contest *contest)
{
    LockTextSpeed();
}

// Returns whether the player's Pokemon already has the ribbon for this
// contest type and rank.
BOOL Contest_CheckPlayerMonHasRibbon(Contest *contest)
{
    u32 monDataRibbon = CalcMonDataRibbon(contest->data.contestRank, contest->data.contestType);

    if (Pokemon_GetValue(contest->playerMon, monDataRibbon, NULL) == FALSE) {
        return FALSE;
    }

    return TRUE;
}

void Contest_SetRibbonName(Contest *contest, StringTemplate *string, u32 idx, int unused)
{
    u32 ribbonID, ribbon;

    switch (contest->data.contestType) {
    case CONTEST_TYPE_COOL:
        ribbonID = RIBBON_COOL + contest->data.contestRank;
        break;
    case CONTEST_TYPE_BEAUTY:
        ribbonID = RIBBON_BEAUTY + contest->data.contestRank;
        break;
    case CONTEST_TYPE_CUTE:
        ribbonID = RIBBON_CUTE + contest->data.contestRank;
        break;
    case CONTEST_TYPE_SMART:
        ribbonID = RIBBON_SMART + contest->data.contestRank;
        break;
    case CONTEST_TYPE_TOUGH:
        ribbonID = RIBBON_TOUGH + contest->data.contestRank;
        break;
    default:
        GF_ASSERT(FALSE);
        return;
    }

    ribbon = Ribbon_GetData(ribbonID, RIBBON_DATA_NAME_ID);
    StringTemplate_SetRibbonName(string, idx, ribbon);
}

// Returns the accessory awarded for winning a contest for the first time, or
// 0xffff if the player did not win or the fashion case cannot hold it.
u32 Contest_CalcFirstTimeVictoryAccessoryReward(Contest *contest)
{
    u32 accessoryID = ACCESSORY_COUNT;

    if (contest->data.results[contest->data.playerContestantID].contestPlacement > 0) {
        return 0xffff;
    }

    switch (contest->data.contestType) {
    case CONTEST_TYPE_COOL:
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            accessoryID = ACCESSORY_RED_BARRETTE;
            break;
        case CONTEST_RANK_GREAT:
            accessoryID = ACCESSORY_RED_BALLOONS;
            break;
        case CONTEST_RANK_ULTRA:
            accessoryID = ACCESSORY_TOP_HAT;
            break;
        case CONTEST_RANK_MASTER:
            accessoryID = ACCESSORY_GOLD_PEDESTAL;
            break;
        }
        break;
    case CONTEST_TYPE_BEAUTY:
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            accessoryID = ACCESSORY_BLUE_BARRETTE;
            break;
        case CONTEST_RANK_GREAT:
            accessoryID = ACCESSORY_BLUE_BALLOONS;
            break;
        case CONTEST_RANK_ULTRA:
            accessoryID = ACCESSORY_SILK_VEIL;
            break;
        case CONTEST_RANK_MASTER:
            accessoryID = ACCESSORY_GLASS_STAGE;
            break;
        }
        break;
    case CONTEST_TYPE_CUTE:
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            accessoryID = ACCESSORY_PINK_BARRETTE;
            break;
        case CONTEST_RANK_GREAT:
            accessoryID = ACCESSORY_PINK_BALLOON;
            break;
        case CONTEST_RANK_ULTRA:
            accessoryID = ACCESSORY_LACE_HEADDRESS;
            break;
        case CONTEST_RANK_MASTER:
            accessoryID = ACCESSORY_FLOWER_STAGE;
            break;
        }
        break;
    case CONTEST_TYPE_SMART:
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            accessoryID = ACCESSORY_GREEN_BARRETTE;
            break;
        case CONTEST_RANK_GREAT:
            accessoryID = ACCESSORY_GREEN_BALLOONS;
            break;
        case CONTEST_RANK_ULTRA:
            accessoryID = ACCESSORY_PROFESSOR_HAT;
            break;
        case CONTEST_RANK_MASTER:
            accessoryID = ACCESSORY_CUBE_STAGE;
            break;
        }
        break;
    case CONTEST_TYPE_TOUGH:
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            accessoryID = ACCESSORY_YELLOW_BARRETTE;
            break;
        case CONTEST_RANK_GREAT:
            accessoryID = ACCESSORY_YELLOW_BALLOONS;
            break;
        case CONTEST_RANK_ULTRA:
            accessoryID = ACCESSORY_HEROIC_HEADBAND;
            break;
        case CONTEST_RANK_MASTER:
            accessoryID = ACCESSORY_AWARD_PODIUM;
            break;
        }
        break;
    }

    GF_ASSERT(accessoryID != ACCESSORY_COUNT);

    ImageClips *imageClips = SaveData_GetImageClips(contest->saveData);
    FashionCase *fashionCase = ImageClips_GetFashionCase(imageClips);

    if (!FashionCase_CanFitAccessoryCount(fashionCase, accessoryID, 1)) {
        return 0xffff;
    }

    return accessoryID;
}

// Starts the camera flash effect for a contestant. The delay pattern depends
// on the contest rank and on the contestant's camera flash variant.
void Contest_StartCameraFlashTask(Contest *contest, int contestantEntryNum)
{
    ContestCameraFlashTask *cameraFlashTask;

    GF_ASSERT(contest->cameraFlashTask == NULL);

    cameraFlashTask = Heap_AllocAtEnd(HEAP_ID_FIELD1, sizeof(ContestCameraFlashTask));
    MI_CpuClear8(cameraFlashTask, sizeof(ContestCameraFlashTask));
    cameraFlashTask->contestantID = Contest_ContestantEntryNumToContestantID(contestantEntryNum);

    int varianceIndex = contest->data.cameraFlashVariant[cameraFlashTask->contestantID] & 1;

    if (contest->isLinkContest == TRUE) {
        cameraFlashTask->cameraFlashFrameDelays = sLinkMasterRankCameraFrameDelays[varianceIndex];
    } else {
        switch (contest->data.contestRank) {
        case CONTEST_RANK_NORMAL:
            cameraFlashTask->cameraFlashFrameDelays = sNormalRankCameraFrameDelays[varianceIndex];
            break;
        case CONTEST_RANK_GREAT:
            cameraFlashTask->cameraFlashFrameDelays = sGreatRankCameraFrameDelays[varianceIndex];
            break;
        case CONTEST_RANK_ULTRA:
            cameraFlashTask->cameraFlashFrameDelays = sUltraRankCameraFrameDelays[varianceIndex];
            break;
        case CONTEST_RANK_MASTER:
        default:
            cameraFlashTask->cameraFlashFrameDelays = sLinkMasterRankCameraFrameDelays[varianceIndex];
            break;
        }
    }

    cameraFlashTask->sysTask = SysTask_Start(SysTask_DoContestCameraFlash, contest, 100);
    contest->cameraFlashTask = cameraFlashTask;
}

BOOL Contest_CameraFlashTaskDone(Contest *contest)
{
    return contest->cameraFlashTask == NULL;
}

// Plays the camera flash: waits for the current brightness transition to
// finish, then fires the shutter once per delay in the pattern. The pattern is
// terminated by CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR.
static void SysTask_DoContestCameraFlash(SysTask *sysTask, void *contestParam)
{
    Contest *contest = contestParam;
    ContestCameraFlashTask *cameraFlashTask = contest->cameraFlashTask;

    if (BrightnessController_IsTransitionComplete(BRIGHTNESS_MAIN_SCREEN) == FALSE) {
        return;
    }

    switch (cameraFlashTask->state) {
    case 0:
        cameraFlashTask->frameCounter++;

        if (cameraFlashTask->frameCounter > cameraFlashTask->cameraFlashFrameDelays[cameraFlashTask->flashCounter]) {
            BrightnessController_StartTransition(6, 0, 4, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_MAIN_SCREEN);
            Sound_PlayEffect(SEQ_SE_DP_KATI_sseq_1); // camera shutter sound
            cameraFlashTask->flashCounter++;
            cameraFlashTask->frameCounter = 0;

            if (cameraFlashTask->cameraFlashFrameDelays[cameraFlashTask->flashCounter] == CAMERA_FLASH_FRAME_DELAY_ARRAY_TERMINATOR) {
                cameraFlashTask->state++;
            }
        }
        break;
    default:
        Heap_Free(contest->cameraFlashTask);
        contest->cameraFlashTask = NULL;
        SysTask_Done(sysTask);
        return;
    }
}

// Reports the outcome of a finished contest: the winning contestant's entry
// number, whether it was a link contest, whether the winner was an NPC, and
// whether it was a practice or a single official competition.
void Contest_GetResults(Contest *contest, int *destWinningContestantEntryNum, BOOL *destIsLinkContest, int *destWinnerIsNPC, BOOL *destIsPracticeCompetition, int *destIsSingleCompetition)
{
    int winningContestantID;
    int contestantID;

    for (contestantID = 0; contestantID < CONTEST_NUM_PARTICIPANTS; contestantID++) {
        if (contest->data.results[contestantID].contestPlacement == 0) {
            break;
        }
    }

    winningContestantID = contestantID;

    *destWinningContestantEntryNum = Contest_ContestantIDToContestantEntryNum(winningContestantID);
    *destIsLinkContest = contest->isLinkContest;

    if (winningContestantID >= contest->data.connectionCount) {
        *destWinnerIsNPC = 1;
    } else {
        *destWinnerIsNPC = 0;
    }

    *destIsPracticeCompetition = FALSE;
    *destIsSingleCompetition = 0;

    switch (contest->data.competitionType) {
    case CONTEST_COMPETITION_PRACTICE_VISUAL:
    case CONTEST_COMPETITION_PRACTICE_DANCE:
    case CONTEST_COMPETITION_PRACTICE_ACTING:
        *destIsPracticeCompetition = TRUE;
        break;
    case CONTEST_COMPETITION_VISUAL:
    case CONTEST_COMPETITION_DANCE:
    case CONTEST_COMPETITION_ACTING:
        *destIsSingleCompetition = 1;
        break;
    }
}

// Records the results of a finished official or link contest: the master-rank
// system flag, the winner's ribbon, the TV broadcast, game records, Pokedex
// encounters for the NPC contestants, and the saved contest photo. Practice
// and single competitions do not reach this point.
void Contest_EndContest(Contest *contest, SaveData *saveData, u32 mapID, JournalEntry *journalEntry)
{
    BOOL ribbonWon = FALSE;

    switch (contest->data.competitionType) {
    case CONTEST_COMPETITION_PRACTICE_VISUAL:
    case CONTEST_COMPETITION_VISUAL:
    case CONTEST_COMPETITION_PRACTICE_DANCE:
    case CONTEST_COMPETITION_DANCE:
    case CONTEST_COMPETITION_PRACTICE_ACTING:
    case CONTEST_COMPETITION_ACTING:
        return;
    }

    if (contest->isLinkContest == FALSE) {
        VarsFlags *varsFlags = SaveData_GetVarsFlags(contest->saveData);

        if (contest->data.competitionType == CONTEST_COMPETITION_LINK_OR_OFFICIAL && contest->data.contestRank >= CONTEST_RANK_MASTER && Contest_GetPlayerContestPlacement(contest) == 0
            && SystemFlag_CheckContestMaster(varsFlags, contest->data.contestType) == FALSE) {
            SystemFlag_SetContestMaster(varsFlags, contest->data.contestType);
        }

        if (Contest_GetPlayerContestPlacement(contest) == 0) {
            u8 v3 = 1;
            int monDataRibbon = CalcMonDataRibbon(contest->data.contestRank, contest->data.contestType);

            if (Pokemon_GetValue(contest->playerMon, monDataRibbon, NULL) == 0) {
                ribbonWon = TRUE;
            }

            Pokemon_SetValue(contest->playerMon, monDataRibbon, &v3);
            TVSegment_SaveRibbonRecord(contest->saveData, contest->playerMon, monDataRibbon);
        }

        TVBroadcast *broadcast = SaveData_GetTVBroadcast(contest->saveData);
        TVBroadcast_SetContestHallShowInfo(broadcast, contest->playerMon, contest->data.contestType, contest->data.contestRank, contest->data.results[contest->data.playerContestantID].contestPlacement + 1);

        GameRecords *gameRecords = SaveData_GetGameRecords(contest->saveData);
        GameRecords_IncrementRecordValue(gameRecords, RECORD_SUPER_CONTEST_PARTICIPATIONS);

        if (Contest_GetPlayerContestPlacement(contest) == 0) {
            GameRecords_IncrementRecordValue(gameRecords, RECORD_OFFICIAL_CONTEST_WINS);
            GameRecords_IncrementTrainerScore(gameRecords, TRAINER_SCORE_EVENT_WIN_OFFICIAL_CONTEST);
        }

        if (ribbonWon == TRUE) {
            GameRecords_IncrementRecordValue(gameRecords, RECORD_RIBBONS_WON);
        }

        Pokedex *pokedex = SaveData_GetPokedex(contest->saveData);

        for (int i = contest->data.connectionCount; i < CONTEST_NUM_PARTICIPANTS; i++) {
            Pokedex_Encounter(pokedex, contest->data.contestMons[i]);
        }
    } else {
        LinkContestRecords_IncrementSavaData(contest->saveData, contest->data.contestType, contest->data.results[contest->data.playerContestantID].contestPlacement);

        GameRecords *records = SaveData_GetGameRecords(contest->saveData);
        GameRecords_IncrementRecordValue(records, RECORD_LINK_CONTEST_PARTICIPATIONS);

        if (Contest_GetPlayerContestPlacement(contest) == 0) {
            GameRecords_IncrementRecordValue(records, RECORD_LINK_CONTEST_WINS);
            GameRecords_IncrementTrainerScore(records, TRAINER_SCORE_EVENT_WIN_LINK_CONTEST);
        }

        void *journalEntryOnlineEvent = JournalEntry_CreateEventPlacedInContest(contest->data.results[contest->data.playerContestantID].contestPlacement + 1, HEAP_ID_FIELD2);
        JournalEntry *unused = SaveData_GetJournal(contest->saveData);

        JournalEntry_SaveData(journalEntry, journalEntryOnlineEvent, JOURNAL_ONLINE_EVENT);
    }

    if (Contest_GetPlayerContestPlacement(contest) == 0) {
        ImageClips *imageClips = SaveData_GetImageClips(contest->saveData);
        ContestPhoto *v12 = ImageClips_GetContestPhoto(imageClips, contest->data.contestType);

        ContestPhoto_Init(v12);
        ContestPhoto_Copy(v12, contest->data.photos[contest->data.playerContestantID]);
        ContestPhoto_SetFullMagic(v12);
    }

    if (Contest_GetPlayerContestPlacement(contest) == 0) {
        Pokemon_UpdateFriendship(contest->playerMon, FRIENDSHIP_EVENT_CONTEST_WIN, mapID);
    }
}

static void Contest_SetLCRNGSeed(Contest *contest)
{
    LCRNG_SetSeed((GetSecondsSinceMidnight() * (LCRNG_GetSeed() + 10)) & 0xffff);
}

// Advances the contest's LCRNG and returns the next 16-bit value.
u16 Contest_GetRNGNext(Contest *contest)
{
    return LCRNG_Next();
}

// Advances the given ARNG seed and returns the next value scaled down to the
// LCRNG range, so callers can use it interchangeably with Contest_GetRNGNext.
u16 Contest_GetSeededRNGNext(u32 seed, u32 *destRNGVal)
{
    u32 rngVal = ARNG_Next(seed);
    *destRNGVal = rngVal;

    return rngVal / LCRNG_DIVISOR;
}

// Fills the summary sent to the other players with the contestant's gender,
// camera flash variant, contest fame, and overworld sprite.
static void Contest_FillContestantSummary(Contest *contest, int contestantID, UnkStruct_ov6_02248DD8 *param2)
{
    param2->trainerGender = contest->data.trainerGenders[contestantID];
    param2->unk_01 = contest->data.cameraFlashVariant[contestantID];
    param2->monContestFame = contest->data.monContestFame[contestantID];
    param2->contestantObjEventGFX = contest->data.contestantObjEventGFX[contestantID];
}
