#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "struct_defs/struct_0208C06C.h"

#include "overlay062/ov62_0222F2C0.h"
#include "overlay062/ov62_022300D8.h"
#include "overlay062/ov62_02231690.h"

#include "game_overlay.h"
#include "gx_layers.h"
#include "heap.h"
#include "savedata_misc.h"
#include "sound.h"
#include "system.h"
#include "vs_recorder_ring.h"
#include "vs_recorder.h"

#include "constdata/const_020F3050.h"
#include "constdata/const_020F3060.h"

FS_EXTERN_OVERLAY(overlay62);

static int VsRecorderApp_InitNormal(ApplicationManager *appMan, int *param1);
static int VsRecorderApp_InitWiFi(ApplicationManager *appMan, int *param1);
static int VsRecorderApp_Init(ApplicationManager *appMan, int *param1, int param2);
static int VsRecorderApp_Main(ApplicationManager *appMan, int *param1);
static int VsRecorderApp_Exit(ApplicationManager *appMan, int *param1);

// Application manager for the Vs. Recorder viewer (overlay062). The viewer is
// launched by vs_recorder.c as a child app. The normal template is used for the
// Vs. Recorder itself (mode 0); the Wi-Fi template is used for the online modes
// opened from the Global Terminal (modes 2-6).
const ApplicationManagerTemplate gVsRecorderViewerTemplate = {
    VsRecorderApp_InitNormal,
    VsRecorderApp_Main,
    VsRecorderApp_Exit,
    FS_OVERLAY_ID(overlay62)
};

const ApplicationManagerTemplate gVsRecorderViewerWiFiTemplate = {
    VsRecorderApp_InitWiFi,
    VsRecorderApp_Main,
    VsRecorderApp_Exit,
    FS_OVERLAY_ID(overlay62)
};

// Shared init for both viewer variants. param2 selects the Wi-Fi/online variant:
// it plays the Wi-Fi Tower BGM and uses a fixed UI color instead of the color
// saved in the player's MiscSaveBlock.
static int VsRecorderApp_Init(ApplicationManager *appMan, int *param1, int param2)
{
    UnkStruct_0208C06C *v0;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_102, 0x55000);
    v0 = VsRecorder_GetState(appMan);
    ov62_02230060(v0);
    Sound_SetPlayerVolume(1, (127 / 3));

    if (param2 != 0) {
        Sound_SetSceneAndPlayBGM(SOUND_SCENE_FIELD, SEQ_PL_WIFITOWER_sseq, 1);
    }

    if (param2 == 0) {
        {
            MiscSaveBlock *v1 = SaveData_MiscSaveBlock(v0->saveData);

            // Read the player's chosen Vs. Recorder color; clamp out-of-range
            // values to the first entry, then resolve it to a palette color.
            MiscSaveBlock_VsRecorderColor(v1, &v0->unk_14.unk_48);

            if (v0->unk_14.unk_48 >= 7) {
                v0->unk_14.unk_48 = 0;
            }

            v0->unk_14.unk_44 = ov62_022316A0(v0);
        }
    } else {
        // The Wi-Fi/online viewer uses a fixed UI color.
        v0->unk_14.unk_44 = 0x7fdd;
    }

    ov62_0222F2C0(v0);

    return 1;
}

static int VsRecorderApp_InitNormal(ApplicationManager *appMan, int *param1)
{
    return VsRecorderApp_Init(appMan, param1, 0);
}

static int VsRecorderApp_InitWiFi(ApplicationManager *appMan, int *param1)
{
    return VsRecorderApp_Init(appMan, param1, 1);
}

static int VsRecorderApp_Main(ApplicationManager *appMan, int *param1)
{
    BOOL v0 = 0;
    UnkStruct_0208C06C *v1 = VsRecorder_GetState(appMan);

    v1->unk_10 = param1;
    v0 = ov62_0222F910(v1, param1);

    return (v0) ? 1 : 0;
}

// Shutdown sequence, run one step per frame. First the viewer's sprite tasks and
// resources are torn down, then the two Vs. Recorder rings are shut down
// asynchronously, and finally the graphics heap and overlay are released and the
// 3D screen is restored to the main display.
static int VsRecorderApp_Exit(ApplicationManager *appMan, int *param1)
{
    UnkStruct_0208C06C *v0 = VsRecorder_GetState(appMan);

    switch (*param1) {
    case 0:
        ov62_0223069C(v0);
        (*param1)++;
        break;
    case 1:
        ov62_0223066C(v0);
        ov62_02230B74(v0);
        ov62_0223113C(v0);
        (*param1)++;
        break;
    case 2: {
        if (VsRecorderRing_Shutdown(v0->unk_6F0) == 0) {
            (*param1)++;
        }
    } break;
    case 3: {
        if (VsRecorderRing_Shutdown(v0->unk_6F4) == 0) {
            (*param1)++;
        }
    } break;
    default:
        ov62_0222F514(v0);
        Heap_Destroy(HEAP_ID_102);
        Overlay_UnloadByID(FS_OVERLAY_ID(overlay62));
        gSystem.whichScreenIs3D = DS_SCREEN_MAIN;
        GXLayers_SwapDisplay();

        return 1;
    }

    return 0;
}
