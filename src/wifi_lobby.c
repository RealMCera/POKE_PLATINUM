#include "wifi_lobby.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_02017498.h"

#include "overlay061/ov61_0222BF44.h"

#include "gx_layers.h"
#include "heap.h"
#include "network_icon.h"
#include "overlay_manager.h"
#include "sound.h"
#include "system.h"
#include "comm_server_client.h"
#include "vs_recorder.h"
#include "wifi_overlays.h"

// DWC_SetMemFunc only takes function pointers, so the active DWC heap is
// stashed here when the app starts and read by the allocation callbacks.
static NNSFndHeapHandle sDwcHeap;

static void WiFiLobby_InitDwcHeap(WiFiLobbyAppState *appState);
static void WiFiLobby_FreeDwcHeap(WiFiLobbyAppState *appState);
static void *WiFiLobby_DwcAlloc(DWCAllocType allocType, u32 size, int alignment);
static void WiFiLobby_DwcFree(DWCAllocType allocType, void *ptr, u32 size);

// Global Terminal (overlay061) application template.
static const ApplicationManagerTemplate sWiFiLobbyAppTemplate = {
    ov61_0222BF44,
    ov61_0222C0F8,
    ov61_0222C160,
    0xffffffff,
};

int WiFiLobby_Init(ApplicationManager *appMan, int *param1)
{
    WiFiLobbyAppState *appState;

    SetVBlankCallback(NULL, NULL);
    DisableHBlank();
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();

    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);
    GX_SetVisibleWnd(GX_WNDMASK_NONE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);
    G2_BlendNone();
    G2S_BlendNone();

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_116, (0x20000 + 0x8000));

    appState = ApplicationManager_NewData(appMan, sizeof(WiFiLobbyAppState), HEAP_ID_116);
    MI_CpuClear8(appState, sizeof(WiFiLobbyAppState));
    appState->args = ApplicationManager_Args(appMan);

    Sound_SetSceneAndPlayBGM(SOUND_SCENE_11, SEQ_WIFILOBBY_sseq, 1);

    return 1;
}

int WiFiLobby_Main(ApplicationManager *appMan, int *param1)
{
    WiFiLobbyAppState *appState = ApplicationManager_Data(appMan);

    switch (*param1) {
    case 0:
        // Allocate the DWC heap and load the WFC/HTTP overlays.
        WiFiLobby_InitDwcHeap(appState);
        *param1 = 1;
        break;
    case 1:
        // Wait for the wireless driver, then hand the heap to DWC.
        if (WirelessDriver_IsReady()) {
            sDwcHeap = appState->dwcHeap;

            DWC_SetMemFunc(WiFiLobby_DwcAlloc, WiFiLobby_DwcFree);

            appState->dwcInitialized = 1;
            (*param1)++;
        }
        break;
    case 2:
        // Run the Global Terminal (overlay061).
        appState->appMan = ApplicationManager_New(&sWiFiLobbyAppTemplate, appState, HEAP_ID_116);
        (*param1)++;
        break;
    case 3:
        if (ApplicationManager_Exec(appState->appMan) == 1) {
            ApplicationManager_Free(appState->appMan);

            if (appState->vsRecorderRequested == 1) {
                // The Global Terminal asked for the Vs. Recorder.
                appState->vsRecorderLaunched = 1;
                (*param1)++;
            } else {
                *param1 = 8;
            }
        }
        break;
    case 4: {
        const ApplicationManagerTemplate *appTemplate;

        appTemplate = VsRecorder_GetAppTemplate(appState->args->unk_0C);
        appState->appMan = ApplicationManager_New(appTemplate, appState->args->fieldSystem, HEAP_ID_116);
        (*param1)++;
    } break;
    case 5:
        if (ApplicationManager_Exec(appState->appMan) == 1) {
            ApplicationManager_Free(appState->appMan);
            (*param1)++;
        }
        break;
    case 6:
        // Re-enter the Global Terminal after the Vs. Recorder exits.
        appState->appMan = ApplicationManager_New(&sWiFiLobbyAppTemplate, appState, HEAP_ID_116);
        (*param1)++;
        break;
    case 7:
        if (ApplicationManager_Exec(appState->appMan) == 1) {
            ApplicationManager_Free(appState->appMan);
            appState->vsRecorderLaunched = 0;
            (*param1)++;
        }
        break;
    case 8:
        return 1;
    }

    // Keep the DWC connection alive and refresh the signal icon while the
    // Vs. Recorder is running after a successful Global Terminal session.
    if ((appState->dwcInitialized == 1) && (appState->vsRecorderLaunched == 1) && (appState->vsRecorderRequested == 1)) {
        DWC_UpdateConnection();
        NetworkIcon_SetStrength(WM_LINK_LEVEL_3 - DWC_GetLinkLevel());
    }

    return 0;
}

int WiFiLobby_Exit(ApplicationManager *appMan, int *param1)
{
    WiFiLobbyAppState *appState = ApplicationManager_Data(appMan);

    WiFiLobby_FreeDwcHeap(appState);
    Heap_Free(appState->args);
    ApplicationManager_FreeData(appMan);
    Heap_Destroy(HEAP_ID_116);

    return 1;
}

// Allocates the DWC heap and loads the WFC and HTTP overlays. The heap is
// created from a raw allocation aligned up to 32 bytes, as required by
// NNS_FndCreateExpHeap.
static void WiFiLobby_InitDwcHeap(WiFiLobbyAppState *appState)
{
    if (appState->dwcInitialized == 0) {
        appState->dwcHeapBuffer = Heap_Alloc(HEAP_ID_116, 0x20000 + 32);
        appState->dwcHeap = NNS_FndCreateExpHeap((void *)(((u32)appState->dwcHeapBuffer + 31) / 32 * 32), 0x20000);

        Overlay_LoadWFCOverlay();
        Overlay_LoadHttpOverlay();
        WirelessDriver_Init();
    }
}

// Tears down the DWC heap and unloads the WFC and HTTP overlays.
static void WiFiLobby_FreeDwcHeap(WiFiLobbyAppState *appState)
{
    if (appState->dwcInitialized == 1) {
        NNS_FndDestroyExpHeap(appState->dwcHeap);

        Heap_Free(appState->dwcHeapBuffer);
        Overlay_UnloadHttpOverlay();
        Overlay_UnloadWFCOverlay();
        WirelessDriver_Shutdown();

        appState->dwcInitialized = 0;
    }
}

// DWC allocation callback. DWC calls this from interrupt context, so
// interrupts are disabled around the heap access.
static void *WiFiLobby_DwcAlloc(DWCAllocType allocType, u32 size, int alignment)
{
#pragma unused(allocType)
    void *ptr;
    OSIntrMode intrMode;

    intrMode = OS_DisableInterrupts();
    ptr = NNS_FndAllocFromExpHeapEx(sDwcHeap, size, alignment);

    OS_RestoreInterrupts(intrMode);

    if (ptr == NULL) {
        (void)0;
    }

    return ptr;
}

// DWC free callback. DWC calls this from interrupt context, so interrupts are
// disabled around the heap access.
static void WiFiLobby_DwcFree(DWCAllocType allocType, void *ptr, u32 size)
{
#pragma unused(allocType, size)
    OSIntrMode intrMode;

    if (!ptr) {
        return;
    }

    intrMode = OS_DisableInterrupts();
    NNS_FndFreeToExpHeap(sDwcHeap, ptr);
    OS_RestoreInterrupts(intrMode);
}
