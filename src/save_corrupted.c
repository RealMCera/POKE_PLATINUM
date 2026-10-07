#include <nitro.h>
#include <string.h>

#include "main_menu/application_template.h"

#include "bg_window.h"
#include "font.h"
#include "gx_layers.h"
#include "heap.h"
#include "main.h"
#include "menu.h"
#include "message.h"
#include "overlay_manager.h"
#include "render_window.h"
#include "savedata.h"
#include "screen_fade.h"
#include "string_gf.h"
#include "system.h"
#include "text.h"

FS_EXTERN_OVERLAY(main_menu);

// Save-corruption notification app. The title screen launches it when
// SaveData_LoadCheckStatus reports that part of the save file could not be
// loaded. It shows one message per problem (normal save, Battle Video and
// Battle Hall records), waits for the player to acknowledge each one, then
// returns to the main menu.
typedef struct SaveCorruptedAppData {
    enum HeapID heapID;
    int state; // Outer state machine; see SaveCorrupted_ShowCorruptionMessages.
    int messageID; // Message bank entry currently being shown.
    int printState; // Inner state machine driving one message; see SaveCorrupted_PrintMessage.
    int printer; // Text printer for the current message.
    String *string; // String loaded from the message bank for the current message.
    BgConfig *bgConfig;
    MessageLoader *messageLoader;
    Window window;
    Menu *menu; // Unused.
    SaveData *saveData;
    void *unk_38; // Unused.
    u32 loadCheckStatus; // Bitmask from SaveData_LoadCheckStatus; bits are cleared as each message is shown.
} SaveCorruptedAppData;

int SaveCorrupted_Init(ApplicationManager *appMan, int *param1);
int SaveCorrupted_Main(ApplicationManager *appMan, int *param1);
int SaveCorrupted_Exit(ApplicationManager *appMan, int *param1);
static void SaveCorrupted_InitGraphics(SaveCorruptedAppData *param0);
static void SaveCorrupted_FreeGraphics(SaveCorruptedAppData *param0);
static void SaveCorrupted_InitMessage(SaveCorruptedAppData *param0);
static void SaveCorrupted_FreeMessage(SaveCorruptedAppData *param0);
static BOOL SaveCorrupted_ShowCorruptionMessages(SaveCorruptedAppData *param0);
static BOOL SaveCorrupted_PrintMessage(SaveCorruptedAppData *param0, u32 param1, int param2, int param3);

// Message box window: 27x4 tiles at (2, 19) on the main screen.
static const WindowTemplate sSaveCorruptedWindowTemplate = {
    0x0,
    0x2,
    0x13,
    0x1B,
    0x4,
    0x1,
    0x16D
};

// Registered by the title screen when the save file is damaged.
const ApplicationManagerTemplate gSaveCorruptedAppTemplate = {
    SaveCorrupted_Init,
    SaveCorrupted_Main,
    SaveCorrupted_Exit,
    0xFFFFFFFF
};

// Application init: allocate the app data and stash the save file handed down
// by the title screen.
int SaveCorrupted_Init(ApplicationManager *appMan, int *param1)
{
    SaveCorruptedAppData *v0;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_88, 0x20000);

    v0 = ApplicationManager_NewData(appMan, sizeof(SaveCorruptedAppData), HEAP_ID_88);
    memset(v0, 0, sizeof(SaveCorruptedAppData));

    v0->heapID = HEAP_ID_88;
    v0->state = 0;
    v0->saveData = ((ApplicationArgs *)ApplicationManager_Args(appMan))->saveData;

    return 1;
}

// Application main loop. *param1 is the app-level state:
//   0: tear down the previous screen and set up the message box,
//   1: show every corruption message in turn,
//   2: release the graphics and signal completion.
int SaveCorrupted_Main(ApplicationManager *appMan, int *param1)
{
    SaveCorruptedAppData *v0 = ApplicationManager_Data(appMan);
    int v1 = 0;

    switch (*param1) {
    case 0:
        SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
        SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);
        SetVBlankCallback(NULL, NULL);
        SetHBlankCallback(NULL, NULL);
        GXLayers_DisableEngineALayers();
        GXLayers_DisableEngineBLayers();
        GX_SetVisiblePlane(0);
        GXS_SetVisiblePlane(0);
        SetAutorepeat(4, 8);
        SaveCorrupted_InitGraphics(v0);
        SaveCorrupted_InitMessage(v0);
        GXLayers_TurnBothDispOn();
        *param1 = 1;
        break;
    case 1:
        if (SaveCorrupted_ShowCorruptionMessages(v0) == TRUE) {
            *param1 = 2;
        }
        break;
    case 2:
        SaveCorrupted_FreeMessage(v0);
        SaveCorrupted_FreeGraphics(v0);
        SetVBlankCallback(NULL, NULL);
        v1 = 1;
        break;
    }

    return v1;
}

// Application exit: free the app data and hand control back to the main menu.
int SaveCorrupted_Exit(ApplicationManager *appMan, int *param1)
{
    SaveCorruptedAppData *v0 = ApplicationManager_Data(appMan);
    int heapID = v0->heapID;

    ApplicationManager_FreeData(appMan);
    Heap_Destroy(heapID);
    EnqueueApplication(FS_OVERLAY_ID(main_menu), &gMainMenuAppTemplate);

    return 1;
}

// Sets up the VRAM banks, background and text graphics used by the message box.
static void SaveCorrupted_InitGraphics(SaveCorruptedAppData *param0)
{
    {
        GXBanks v0 = {
            GX_VRAM_BG_256_AB,
            GX_VRAM_BGEXTPLTT_NONE,
            GX_VRAM_SUB_BG_NONE,
            GX_VRAM_SUB_BGEXTPLTT_NONE,
            GX_VRAM_OBJ_NONE,
            GX_VRAM_OBJEXTPLTT_NONE,
            GX_VRAM_SUB_OBJ_NONE,
            GX_VRAM_SUB_OBJEXTPLTT_NONE,
            GX_VRAM_TEX_NONE,
            GX_VRAM_TEXPLTT_NONE
        };
        GXLayers_SetBanks(&v0);
    }
    {
        param0->bgConfig = BgConfig_New(param0->heapID);
    }
    {
        GraphicsModes v1 = {
            GX_DISPMODE_GRAPHICS,
            GX_BGMODE_0,
            GX_BGMODE_0,
            GX_BG0_AS_2D
        };
        SetAllGraphicsModes(&v1);
    }
    {
        BgTemplate v2 = {
            .x = 0x0,
            .y = 0x0,
            .bufferSize = 0x800,
            .baseTile = 0x0,
            .screenSize = BG_SCREEN_SIZE_256x256,
            .colorMode = GX_BG_COLORMODE_16,
            .screenBase = GX_BG_SCRBASE_0x0000,
            .charBase = GX_BG_CHARBASE_0x18000,
            .bgExtPltt = GX_BG_EXTPLTT_01,
            .priority = 0x1,
            .areaOver = 0x0,
            .mosaic = FALSE,
        };
        Bg_InitFromTemplate(param0->bgConfig, BG_LAYER_MAIN_0, &v2, 0);
        Bg_ClearTilemap(param0->bgConfig, BG_LAYER_MAIN_0);
    }
    LoadMessageBoxGraphics(param0->bgConfig, BG_LAYER_MAIN_0, 512 - (18 + 12), 2, 0, param0->heapID);
    LoadStandardWindowGraphics(param0->bgConfig, BG_LAYER_MAIN_0, (512 - (18 + 12)) - 9, 3, 0, param0->heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), param0->heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, param0->heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0);
}

// Hides every background layer and frees the background config.
static void SaveCorrupted_FreeGraphics(SaveCorruptedAppData *param0)
{
    Bg_ToggleLayer(BG_LAYER_MAIN_0, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_1, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_2, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_3, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_0, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_1, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_2, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_3, 0);
    Bg_FreeTilemapBuffer(param0->bgConfig, BG_LAYER_MAIN_0);
    Heap_Free(param0->bgConfig);
}

// Loads the save_corrupted message bank and creates the message box window.
static void SaveCorrupted_InitMessage(SaveCorruptedAppData *param0)
{
    param0->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_SAVE_CORRUPTED, param0->heapID);
    Text_ResetAllPrinters();
    param0->printState = 0;

    Window_AddFromTemplate(param0->bgConfig, &param0->window, &sSaveCorruptedWindowTemplate);
    Window_FillRectWithColor(&param0->window, 15, 0, 0, 27 * 8, 4 * 8);
}

// Removes the message box window and frees the message loader.
static void SaveCorrupted_FreeMessage(SaveCorruptedAppData *param0)
{
    Window_Remove(&param0->window);
    MessageLoader_Free(param0->messageLoader);
}

// Drives the message sequence. Each iteration picks one outstanding problem
// from loadCheckStatus, fades in, shows the matching message until the player
// presses A, fades out and loops. Returns TRUE once no problems remain.
static BOOL SaveCorrupted_ShowCorruptionMessages(SaveCorruptedAppData *param0)
{
    BOOL v0 = 0;

    switch (param0->state) {
    case 0: {
        param0->loadCheckStatus = SaveData_LoadCheckStatus(param0->saveData);

        if (param0->loadCheckStatus == 0) {
            param0->state = 6;
        } else {
            param0->state = 1;
        }
    } break;
    case 1:
        param0->state = 2;

        // Pick the next problem, highest priority first, and clear its bits so
        // the loop advances. messageID indexes the save_corrupted message bank:
        //   0: normal save corrupted, previous loaded
        //   1: normal save erased
        //   2: Battle Video corrupted, previous loaded
        //   3: Battle Video erased
        //   4: Battle Hall record corrupted, previous loaded
        //   5: Battle Hall record erased
        if (param0->loadCheckStatus & (1 << 1)) { // NORMAL_LOAD_ERROR
            param0->loadCheckStatus &= 0xffffffff ^ ((1 << 1) | (1 << 0));
            param0->messageID = 1;
        } else if (param0->loadCheckStatus & (1 << 0)) { // NORMAL_LOAD_CORRUPT
            param0->loadCheckStatus ^= (1 << 0);
            param0->messageID = 0;
        } else if (param0->loadCheckStatus & (1 << 3)) { // FRONTIER_LOAD_ERROR
            param0->loadCheckStatus &= 0xffffffff ^ ((1 << 3) | (1 << 2));
            param0->messageID = 5;
        } else if (param0->loadCheckStatus & (1 << 2)) { // FRONTIER_LOAD_CORRUPT
            param0->loadCheckStatus ^= (1 << 2);
            param0->messageID = 4;
        } else if (param0->loadCheckStatus & (1 << 5)) { // VIDEO_LOAD_ERROR
            param0->loadCheckStatus &= 0xffffffff ^ ((1 << 5) | (1 << 4));
            param0->messageID = 3;
        } else if (param0->loadCheckStatus & (1 << 4)) { // VIDEO_LOAD_CORRUPT
            param0->loadCheckStatus ^= (1 << 4);
            param0->messageID = 2;
        } else {
            param0->state = 6;
        }
        break;
    case 2:
        Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
        Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, COLOR_BLACK, 6, 1, param0->heapID);
        param0->state = 3;
        break;
    case 3:
        if (IsScreenFadeDone() == TRUE) {
            param0->state = 4;
        }
        break;
    case 4:
        if (SaveCorrupted_PrintMessage(param0, param0->messageID, 0, 4) == 1) {
            StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 6, 1, param0->heapID);
            param0->state = 5;
        }
        break;
    case 5:
        if (IsScreenFadeDone() == TRUE) {
            Bg_MaskPalette(BG_LAYER_MAIN_0, 0);
            Bg_MaskPalette(BG_LAYER_SUB_0, 0);
            param0->state = 1; // Show the next outstanding message.
        }
        break;
    case 6:
        v0 = 1;
        break;
    }

    return v0;
}

// Shows one message and waits for the player to acknowledge it. param1 is the
// message bank entry, param2 forces the wait (non-zero skips the A check) and
// param3 is the printer speed. Returns TRUE once the message is dismissed.
static BOOL SaveCorrupted_PrintMessage(SaveCorruptedAppData *param0, u32 param1, int param2, int param3)
{
    BOOL v0 = 0;

    switch (param0->printState) {
    case 0:
        Window_FillRectWithColor(&param0->window, 15, 0, 0, 27 * 8, 4 * 8);
        Window_DrawMessageBoxWithScrollCursor(&param0->window, 0, 512 - (18 + 12), 2);

        param0->string = String_Init(0x400, param0->heapID);
        MessageLoader_GetString(param0->messageLoader, param1, param0->string);
        param0->printer = Text_AddPrinterWithParams(&param0->window, FONT_MESSAGE, param0->string, 0, 0, param3, NULL);

        if (param3 == 0) {
            String_Free(param0->string);
            param0->printState++;
        }

        param0->printState++;
        break;
    case 1:
        if (!(Text_IsPrinterActive(param0->printer))) {
            String_Free(param0->string);
            param0->printState++;
        }
        break;
    case 2:
        if ((param2 != 0) || (gSystem.pressedKeys & PAD_BUTTON_A)) {
            param0->printState = 0;
            v0 = 1;
        }
    }

    return v0;
}
