#ifndef POKEPLATINUM_STRUCT_02017498_H
#define POKEPLATINUM_STRUCT_02017498_H

#include <dwc.h>
#include <nnsys.h>

#include "struct_defs/struct_0203E6C0.h"

#include "overlay_manager.h"

typedef struct WiFiLobbyAppState {
    UnkStruct_0203E6C0 *args; // Arguments passed in by FieldSystem_OpenGlobalTerminal.
    BOOL vsRecorderLaunched; // Set when the Vs. Recorder is launched; cleared after the return visit to the Global Terminal.
    ApplicationManager *appMan; // Child application manager for the Global Terminal / Vs. Recorder.
    void *dwcHeapBuffer; // Raw allocation backing the DWC heap.
    NNSFndHeapHandle dwcHeap; // Expanded heap handed to DWC via DWC_SetMemFunc.
    DWCInetControl inetControl; // DWC internet connection control block.
    int dwcInitialized; // Non-zero once the DWC heap and overlays are ready.
    BOOL vsRecorderRequested; // Set by the Global Terminal when it wants the Vs. Recorder launched.
} WiFiLobbyAppState;

#endif // POKEPLATINUM_STRUCT_02017498_H
