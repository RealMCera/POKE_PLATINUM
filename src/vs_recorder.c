#include "vs_recorder.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "generated/journal_online_events.h"

#include "struct_defs/struct_0208BA84.h"
#include "struct_defs/struct_0208C06C.h"

#include "field/field_system.h"
#include "overlay062/ov62_02248408.h"

#include "bag.h"
#include "field_battle_data_transfer.h"
#include "field_bgm.h"
#include "game_overlay.h"
#include "game_records.h"
#include "heap.h"
#include "inlines.h"
#include "journal.h"
#include "overlay_manager.h"
#include "savedata.h"
#include "sound.h"
#include "system_flags.h"
#include "battle_recording.h"
#include "unk_0208C010.h"
#include "vars_flags.h"
#include "wifi_overlays.h"

#include "constdata/const_020EA358.h"
#include "constdata/const_020F2FCC.h"
#include "constdata/const_020F3050.h"
#include "constdata/const_020F3060.h"

FS_EXTERN_OVERLAY(overlay61);
FS_EXTERN_OVERLAY(overlay62);

// Private state for the Vs. Recorder application. The app runs a small state
// machine (state) that first shows the Vs. Recorder viewer (overlay062) and,
// if the viewer requests playback, then replays the recorded battle.
typedef struct {
    // Sub-state of the current phase (viewer or battle playback).
    int state;
    int unk_04;
    ApplicationManager *appMan;
    SaveData *saveData;
    // Battle setup used while replaying a recording.
    FieldBattleDTO *battleDTO;
    // Shared Vs. Recorder state, also used by overlay062.
    UnkStruct_0208C06C *vsRecorder;
    // Set by overlay062 when the player chooses to replay a recording.
    VsRecorderPlaybackRequest playbackRequest;
    FieldSystem *fieldSystem;
} VsRecorderAppData;

static void VsRecorder_Init(ApplicationManager *appMan, int param1);
static int VsRecorder_InitVsRecorder(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitMode1(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitBattleVideos(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitBattleVideoRankings(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitTrainerRankings(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitDressUpData(ApplicationManager *appMan, int *param1);
static int VsRecorder_InitBoxData(ApplicationManager *appMan, int *param1);
static int VsRecorder_Main(ApplicationManager *appMan, int *param1);
static int VsRecorder_Exit(ApplicationManager *appMan, int *param1);
static BOOL VsRecorder_RunViewer(VsRecorderAppData *param0, enum HeapID heapID);
static BOOL VsRecorder_RunBattlePlayback(VsRecorderAppData *param0, enum HeapID heapID);
static BOOL VsRecorder_IsFrontierBrain(int param0);

// Returns the Vs. Recorder state shared with the overlay062 viewer app.
UnkStruct_0208C06C *VsRecorder_GetState(ApplicationManager *appMan)
{
    UnkStruct_0208C06C *v0;
    VsRecorderAppData *v1 = ApplicationManager_Args(appMan);

    return v1->vsRecorder;
}

// Called by overlay062 to tell the app whether the player asked to replay a
// recording. `requested` is read by VsRecorder_Main once the viewer exits.
void VsRecorder_SetPlaybackRequest(VsRecorderPlaybackRequest *request, BOOL requested, int param2)
{
    request->requested = requested;
    request->unk_04 = param2;
}

// Records that the player used one of the Global Terminal's online features,
// so the event shows up in the Journal.
static void VsRecorder_RecordOnlineEvent(SaveData *saveData, int heapID, u32 eventType)
{
    JournalEntry *journalEntry = SaveData_GetJournal(saveData);
    void *journalEntryOnlineEvent = JournalEntry_CreateEventMisc(heapID, eventType);

    JournalEntry_SaveData(journalEntry, journalEntryOnlineEvent, JOURNAL_ONLINE_EVENT);
}

// Shared init for every mode. `mode` selects which feature the app shows:
// 0 = Vs. Recorder, 1 = unused, 2 = Battle Videos, 3 = Battle Video Rankings,
// 4 = Trainer Rankings, 5 = Dress-Up Data, 6 = Box Data. Modes 2-6 are opened
// from the Global Terminal and are recorded in the Journal.
static void VsRecorder_Init(ApplicationManager *appMan, int mode)
{
    VsRecorderAppData *v0;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_119, 0x10000);

    v0 = ApplicationManager_NewData(appMan, sizeof(VsRecorderAppData), HEAP_ID_119);
    MI_CpuFill8(v0, 0, sizeof(VsRecorderAppData));

    v0->fieldSystem = ApplicationManager_Args(appMan);
    v0->saveData = v0->fieldSystem->saveData;
    v0->vsRecorder = Heap_Alloc(HEAP_ID_119, sizeof(UnkStruct_0208C06C));

    MI_CpuFill8(v0->vsRecorder, 0, sizeof(UnkStruct_0208C06C));

    v0->vsRecorder->unk_868 = &v0->playbackRequest;
    v0->vsRecorder->saveData = v0->saveData;
    v0->vsRecorder->unk_00 = mode;
    v0->vsRecorder->unk_81C[v0->vsRecorder->unk_534.unk_1A4] = sub_0208C034(v0->vsRecorder, v0->vsRecorder->unk_00);

    int eventType;

    switch (mode) {
    case 2:
        eventType = ONLINE_EVENT_WATCHED_BATTLE_VIDEOS;
        break;
    case 3:
        eventType = ONLINE_EVENT_CHECKED_RANKINGS;
        break;
    case 4:
        eventType = ONLINE_EVENT_CHECKED_RANKINGS;
        break;
    case 5:
        eventType = ONLINE_EVENT_CHECKED_DRESS_UP_DATA;
        break;
    case 6:
        eventType = ONLINE_EVENT_CHECKED_BOX_DATA;
        break;
    default:
        return;
    }

    VsRecorder_RecordOnlineEvent(v0->saveData, 119, eventType);
}

static int VsRecorder_InitVsRecorder(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 0);
    return 1;
}

static int VsRecorder_InitMode1(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 1);
    return 1;
}

static int VsRecorder_InitBattleVideos(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 2);
    return 1;
}

static int VsRecorder_InitBattleVideoRankings(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 3);
    return 1;
}

static int VsRecorder_InitTrainerRankings(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 4);
    return 1;
}

static int VsRecorder_InitDressUpData(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 5);
    return 1;
}

static int VsRecorder_InitBoxData(ApplicationManager *appMan, int *param1)
{
    VsRecorder_Init(appMan, 6);
    return 1;
}

// Main loop. Phase 0 runs the Vs. Recorder viewer; if the viewer requested
// playback, phase 1 replays the recording and then returns to phase 0.
static int VsRecorder_Main(ApplicationManager *appMan, int *param1)
{
    BOOL v0;
    VsRecorderAppData *v1 = ApplicationManager_Data(appMan);

    switch (*param1) {
    case 0:
        v0 = VsRecorder_RunViewer(v1, HEAP_ID_119);

        if (v0) {
            if (v1->playbackRequest.requested == 1) {
                *param1 = 1;
                v1->state = 0;
            } else {
                return 1;
            }
        }
        break;
    case 1:
        v0 = VsRecorder_RunBattlePlayback(v1, HEAP_ID_119);

        if (v0) {
            *param1 = 0;
            v1->state = 0;
        }
        break;
    }

    return 0;
}

static int VsRecorder_Exit(ApplicationManager *appMan, int *param1)
{
    VsRecorderAppData *v0 = ApplicationManager_Data(appMan);

    if (BattleRecording_Exists() == 1) {
        BattleRecording_Free();
    }

    Heap_Free(v0->vsRecorder);
    ApplicationManager_FreeData(appMan);
    Sound_SetPlayerVolume(1, 127);
    Heap_Destroy(HEAP_ID_119);

    return 1;
}

// Runs the overlay062 viewer app and returns TRUE once it exits. The viewer
// variant differs for the Vs. Recorder (mode 0) and the online features.
static BOOL VsRecorder_RunViewer(VsRecorderAppData *param0, enum HeapID heapID)
{
    switch (param0->state) {
    case 0:
        if (param0->vsRecorder->unk_00 == 0) {
            param0->appMan = ApplicationManager_New(&Unk_020F3050, param0, heapID);
        } else {
            param0->appMan = ApplicationManager_New(&Unk_020F3060, param0, heapID);
        }

        param0->state++;
        break;
    default:
        if (ApplicationManager_Exec(param0->appMan) == 0) {
            break;
        }

        ApplicationManager_Free(param0->appMan);
        return 1;
    }

    return 0;
}

// Replays the selected battle recording. For online features the Global
// Terminal overlay is unloaded first and restored once playback finishes.
static BOOL VsRecorder_RunBattlePlayback(VsRecorderAppData *param0, enum HeapID heapID)
{
    switch (param0->state) {
    case 0:
        if (param0->vsRecorder->unk_00 != 0) {
            Overlay_UnloadByID(FS_OVERLAY_ID(overlay61));
            Overlay_UnloadHttpOverlay();
        }

        param0->state++;
        break;
    case 1: {
        int v0;

        param0->battleDTO = FieldBattleDTO_New(heapID, 0x0);

        if (BattleRecording_Exists() == 0) {
            BattleRecording_Load(param0->saveData, heapID, &v0, param0->battleDTO, param0->vsRecorder->unk_86C);
        } else {
            BattleRecording_RestoreBattleInfo(param0->battleDTO, param0->saveData);
            v0 = 1;
        }

        param0->battleDTO->bagCursor = BagCursor_New(heapID);
        param0->battleDTO->records = SaveData_GetGameRecords(param0->saveData);

        if (Overlay_LoadByID(FS_OVERLAY_ID(overlay62), 2) == 1) {
            ov62_02248408(BattleRecording_Get(), param0->battleDTO, heapID);
            Overlay_UnloadByID(FS_OVERLAY_ID(overlay62));
        }

        param0->vsRecorder->unk_874 = 1;

        if (v0 != 1) {
            Heap_Free(param0->battleDTO->bagCursor);
            FieldBattleDTO_Free(param0->battleDTO);
            param0->state = 0;
            return 1;
        } else {
            param0->state++;
        }
    } break;
    case 2: {
        Sound_SetPlayerVolume(1, 127);
        sub_02005464(1);

        if (VsRecorder_IsFrontierBrain(param0->battleDTO->trainer[1].header.trainerType) == 1) {
            Sound_SetSceneAndPlayBGM(SOUND_SCENE_BATTLE, BATTLE_FRONTIER_BRAIN_sseq, 1);
        } else {
            Sound_SetSceneAndPlayBGM(SOUND_SCENE_BATTLE, BATTLE_TRAINER_sseq, 1);
        }
    }
        param0->appMan = ApplicationManager_New(&gBattleApplicationTemplate, param0->battleDTO, heapID);
        param0->state++;
        break;
    default:
        if (ApplicationManager_Exec(param0->appMan) == 0) {
            break;
        }

        param0->vsRecorder->unk_874 = param0->battleDTO->recordingStopped;

        if (param0->vsRecorder->unk_00 != 0) {
            if (param0->vsRecorder->unk_874 == 0) {
                *param0->vsRecorder->unk_878 = 1;
            }
        }

        Heap_Free(param0->battleDTO->bagCursor);
        FieldBattleDTO_Free(param0->battleDTO);
        ApplicationManager_Free(param0->appMan);

        {
            u16 bgmID;

            sub_02005464(0);
            Sound_SetScene(SOUND_SCENE_NONE);

            bgmID = FieldBGM_GetEffective(param0->fieldSystem, param0->fieldSystem->location->mapHeaderID);

            Sound_SetFieldBGM(FieldBGM_GetForMapHeader(param0->fieldSystem, param0->fieldSystem->location->mapHeaderID));
            Sound_SetSceneAndPlayBGM(SOUND_SCENE_FIELD, bgmID, 1);
        }

        param0->state = 0;

        if (param0->vsRecorder->unk_00 != 0) {
            Overlay_LoadHttpOverlay();
            Overlay_LoadByID(FS_OVERLAY_ID(overlay61), 2);
        }

        return 1;
    }

    return 0;
}

// One app template per mode. All modes share the same main loop and exit
// callback; only the init differs. Mode 0 is the Vs. Recorder opened from the
// field, the rest are the Global Terminal's online features.
const ApplicationManagerTemplate gVsRecorderAppTemplate = {
    VsRecorder_InitVsRecorder,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sOnlineEventAppTemplate1 = {
    VsRecorder_InitMode1,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sBattleVideosAppTemplate = {
    VsRecorder_InitBattleVideos,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sBattleVideoRankingsAppTemplate = {
    VsRecorder_InitBattleVideoRankings,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sTrainerRankingsAppTemplate = {
    VsRecorder_InitTrainerRankings,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sDressUpDataAppTemplate = {
    VsRecorder_InitDressUpData,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

const ApplicationManagerTemplate sBoxDataAppTemplate = {
    VsRecorder_InitBoxData,
    VsRecorder_Main,
    VsRecorder_Exit,
    0xffffffff,
};

// Indexed by the mode passed to VsRecorder_GetAppTemplate.
static const ApplicationManagerTemplate *sAppTemplates[] = {
    &gVsRecorderAppTemplate,
    &sOnlineEventAppTemplate1,
    &sBattleVideosAppTemplate,
    &sBattleVideoRankingsAppTemplate,
    &sTrainerRankingsAppTemplate,
    &sDressUpDataAppTemplate,
    &sBoxDataAppTemplate,
};

// Returns the app template for the given mode. Called by the Global Terminal
// (overlay61) to launch the selected online feature.
const ApplicationManagerTemplate *VsRecorder_GetAppTemplate(int mode)
{
    const ApplicationManagerTemplate *v0 = sAppTemplates[mode];
    return v0;
}

// TRUE if the player has not yet arrived at the Battle Park for the first
// time. Used by overlay062 to gate the Battle Park introduction.
BOOL VsRecorder_CheckFirstArrivalBattlePark(UnkStruct_0208C06C *vsRecorder)
{
    VarsFlags *v0 = SaveData_GetVarsFlags(vsRecorder->saveData);
    return SystemFlag_HandleFirstArrivalToZone(v0, HANDLE_FLAG_CHECK, FIRST_ARRIVAL_BATTLE_PARK);
}

// Frontier Brains use a distinct battle BGM. Their trainer types are 97 and
// 99-102 (the Battle Frontier facility heads).
static BOOL VsRecorder_IsFrontierBrain(int trainerType)
{
    int v0;
    const int v1[] = {
        97,
        99,
        100,
        101,
        102,
    };

    for (v0 = 0; v0 < NELEMS(v1); v0++) {
        if (trainerType == v1[v0]) {
            return 1;
        }
    }

    return 0;
}
