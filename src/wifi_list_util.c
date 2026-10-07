#include "wifi_list_util.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "struct_decls/wi_fi_list.h"

#include "heap.h"
#include "savedata.h"
#include "wifi_list.h"
#include "wifi_overlays.h"

// Initializes the Nintendo WFC (DWC) library. The WFC and HTTP overlays must be
// resident while DWC_Init runs, so they are loaded for the call and unloaded
// again afterwards. DWC_Init requires a 32-byte aligned work buffer, so a
// slightly oversized block is allocated and the pointer rounded up to the next
// 32-byte boundary. Returns the DWC_Init result code.
int WiFiList_InitDWC(enum HeapID heapID)
{
    Overlay_LoadWFCOverlay();
    Overlay_LoadHttpOverlay();

    u8 *workBuffer = Heap_Alloc(heapID, DWC_INIT_WORK_SIZE + 32);
    u8 *alignedWork = (u8 *)(((u32)workBuffer + 31) / 32 * 32);
    int result = DWC_Init(alignedWork);

    Heap_Free(workBuffer);
    Overlay_UnloadWFCOverlay();
    Overlay_UnloadHttpOverlay();

    return result;
}

// Ensures the WiFiList holds valid DWC user data. If no profile has been
// created yet, one is created with the Platinum game code ('ADAJ') and its
// dirty flag is cleared so the fresh data is not immediately written back.
void WiFiList_InitUserData(WiFiList *wiFiList)
{
    DWCUserData *userData = WiFiList_GetUserData(wiFiList);

    if (!DWC_CheckUserData(userData)) {
        DWC_CreateUserData(userData, 'ADAJ');
        DWC_ClearDirtyFlag(userData);
    }
}

// Builds an exchange token from the player's DWC user data and uses it to look
// up the player's GameSpy profile ID.
int WiFiList_GetUserGsProfileId(WiFiList *wifiList)
{
    DWCUserData *userData = WiFiList_GetUserData(wifiList);
    DWCFriendData friendData;

    DWC_CreateExchangeToken(userData, &friendData);
    return DWC_GetGsProfileId(userData, &friendData);
}

// A login is valid only when the user data has both a profile and a console
// that DWC considers valid.
BOOL WiFiList_HasValidLogin(SaveData *saveData)
{
    WiFiList *wiFiList = SaveData_GetWiFiList(saveData);
    DWCUserData *userData = WiFiList_GetUserData(wiFiList);

    return DWC_CheckHasProfile(userData) && DWC_CheckValidConsole(userData);
}
