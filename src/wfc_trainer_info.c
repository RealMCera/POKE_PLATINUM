#include "wfc_trainer_info.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_0207E060.h"

#include "nintendo_wfc/main.h"

#include "heap.h"
#include "trainer_info.h"

// Builds the compact trainer profile that is advertised to friends over
// Nintendo WFC. Only the fields needed to preview a friend's party and to
// decide whether both players are running a compatible activity are copied out
// of the save file's TrainerInfo; the rest of the profile is left zeroed.
WFCTrainerInfo *WFCTrainerInfo_New(const TrainerInfo *info, enum HeapID heapID)
{
    WFCTrainerInfo *wfcTrainerInfo;
    BOOL success;

    wfcTrainerInfo = Heap_Alloc(heapID, (sizeof(WFCTrainerInfo)));
    memset(wfcTrainerInfo, 0, (sizeof(WFCTrainerInfo)));

    wfcTrainerInfo->commState = 28;
    wfcTrainerInfo->gender = TrainerInfo_Gender(info);
    wfcTrainerInfo->appearance = TrainerInfo_Appearance(info);
    wfcTrainerInfo->gameCode = TrainerInfo_GameCode(info);
    wfcTrainerInfo->language = TrainerInfo_Language(info);

    success = NintendoWFC_SetStatusData(wfcTrainerInfo, (sizeof(WFCTrainerInfo)));
    GF_ASSERT(success == 1);

    return wfcTrainerInfo;
}

void WFCTrainerInfo_Free(WFCTrainerInfo *wfcTrainerInfo)
{
    Heap_Free(wfcTrainerInfo);
}
