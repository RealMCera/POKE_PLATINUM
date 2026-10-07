#include "wifi_lobby_task.h"

#include <nitro.h>
#include <string.h>

#include "field/field_system.h"
#include "overlay066/ov66_0222DCE0.h"
#include "overlay066/struct_ov66_0222DCE0.h"

#include "field_task.h"
#include "heap.h"

FS_EXTERN_OVERLAY(overlay66);

// Field task that opens the Wi-Fi lobby (overlay66, the PPW / Wi-Fi Plaza
// lobby). It is started by the ScrCmd_2F7 script command once the player has a
// valid Nintendo WFC login.
typedef struct {
    // State machine state (see WiFiLobbyTask_Task).
    u16 state;
    // Result reported by the preceding WiFi menu (ScrCmd_0B3), forwarded to the
    // lobby app arguments.
    u16 wifiMenuResult;
    // Arguments handed to the overlay66 lobby app.
    UnkStruct_ov66_0222DCE0 appArgs;
} WiFiLobbyTaskWork;

static BOOL WiFiLobbyTask_Task(FieldTask *task);

// The overlay66 lobby app (ov66_0222DCE0) that the task launches as a child
// process.
static const ApplicationManagerTemplate sWiFiLobbyAppTemplate = {
    ov66_0222DCE0,
    ov66_0222DD6C,
    ov66_0222DD90,
    FS_OVERLAY_ID(overlay66)
};

// Starts the Wi-Fi lobby field task. `wifiMenuResult` is the result reported by
// the preceding WiFi menu and is forwarded to the lobby app.
void WiFiLobbyTask_Start(FieldTask *task, BOOL wifiMenuResult)
{
    WiFiLobbyTaskWork *work = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(WiFiLobbyTaskWork));
    memset(work, 0, sizeof(WiFiLobbyTaskWork));

    work->wifiMenuResult = wifiMenuResult;
    FieldTask_InitCall(task, WiFiLobbyTask_Task, work);
}

static BOOL WiFiLobbyTask_Task(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    WiFiLobbyTaskWork *work = FieldTask_GetEnv(task);

    switch (work->state) {
    case 0: {
        work->appArgs.saveData = fieldSystem->saveData;
        work->appArgs.unk_08 = work->wifiMenuResult;
        work->appArgs.unk_00 = &fieldSystem->unk_C4;
        FieldTask_RunApplication(task, &sWiFiLobbyAppTemplate, &work->appArgs);
        work->state++;
    } break;
    case 1:
        Heap_Free(work);
        return 1;
    }

    return 0;
}
