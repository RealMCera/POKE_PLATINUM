#include <nitro.h>
#include <string.h>

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
#include "sound.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "system.h"
#include "text.h"

FS_EXTERN_OVERLAY(game_opening);

// "Delete all saved data" confirmation app. It is launched from the title
// screen when the player holds B + Up + Select (NEXT_APP_CLEAR_SAVE_FILE) and
// asks the player to confirm twice before erasing the save file. Once the data
// has been erased the system is reset, so the game restarts at the title
// screen.
typedef struct ClearSaveDataAppData {
    enum HeapID heapID;
    int confirmState; // Step in the delete-confirmation flow (see ClearSaveData_ConfirmFlow).
    int messageState; // Step in the message printer state machine (see ClearSaveData_ShowMessage).
    int printerID; // Printer created for the message currently being shown.
    String *string; // String loaded for the message currently being shown.
    BgConfig *bgConfig;
    MessageLoader *messageLoader;
    Window window; // Message box window.
    Menu *menu; // Yes/No confirmation menu.
    SaveData *saveData;
    void *waitDial; // Wait dial shown while the save data is erased.
} ClearSaveDataAppData;

void EnqueueApplication(FSOverlayID param0, const ApplicationManagerTemplate *param1);
int ClearSaveData_Init(ApplicationManager *appMan, int *state);
int ClearSaveData_Main(ApplicationManager *appMan, int *state);
int ClearSaveData_Exit(ApplicationManager *appMan, int *state);
static void ClearSaveData_VBlank(void *param);
static void ClearSaveData_InitGraphics(ClearSaveDataAppData *appData);
static void ClearSaveData_FreeGraphics(ClearSaveDataAppData *appData);
static void ClearSaveData_InitMessage(ClearSaveDataAppData *appData);
static void ClearSaveData_FreeMessage(ClearSaveDataAppData *appData);
static BOOL ClearSaveData_ConfirmFlow(ClearSaveDataAppData *appData);
static BOOL ClearSaveData_ShowMessage(ClearSaveDataAppData *appData, u32 messageID, int autoAdvance, int renderDelay);

extern const ApplicationManagerTemplate gTitleScreenAppTemplate;

const ApplicationManagerTemplate gClearSaveDataAppTemplate = {
    ClearSaveData_Init,
    ClearSaveData_Main,
    ClearSaveData_Exit,
    0xFFFFFFFF
};

static const WindowTemplate sClearSaveDataMessageWindowTemplate = {
    0x0,
    0x2,
    0x13,
    0x1B,
    0x4,
    0x1,
    0x16D
};

static const WindowTemplate sYesNoMenuWindowTemplate = {
    0x0,
    0x19,
    0xD,
    0x6,
    0x4,
    0x1,
    0x155
};

int ClearSaveData_Init(ApplicationManager *appMan, int *state)
{
    ClearSaveDataAppData *appData;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_88, 0x20000);

    appData = ApplicationManager_NewData(appMan, sizeof(ClearSaveDataAppData), HEAP_ID_88);
    memset(appData, 0, sizeof(ClearSaveDataAppData));

    appData->heapID = HEAP_ID_88;
    appData->confirmState = 0;
    appData->saveData = ((ApplicationArgs *)ApplicationManager_Args(appMan))->saveData;

    return 1;
}

int ClearSaveData_Main(ApplicationManager *appMan, int *state)
{
    ClearSaveDataAppData *appData = ApplicationManager_Data(appMan);
    int done = 0;

    switch (*state) {
    case 0:
        Sound_StopBGM(SEQ_TITLE01_sseq, 0);
        Sound_ConfigureBGMChannelsAndReverb(SOUND_CHANNEL_CONFIG_DEFAULT);
        Sound_SetScene(SOUND_SCENE_NONE);
        SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
        SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);
        SetVBlankCallback(NULL, NULL);
        SetHBlankCallback(NULL, NULL);
        GXLayers_DisableEngineALayers();
        GXLayers_DisableEngineBLayers();

        GX_SetVisiblePlane(0);
        GXS_SetVisiblePlane(0);

        SetAutorepeat(4, 8);
        ClearSaveData_InitGraphics(appData);
        ClearSaveData_InitMessage(appData);
        SetVBlankCallback(ClearSaveData_VBlank, (void *)appData);
        GXLayers_TurnBothDispOn();
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, COLOR_BLACK, 6, 1, appData->heapID);
        *state = 1;
        break;
    case 1:
        // Wait for the fade from black to finish before showing the prompt.
        if (IsScreenFadeDone() == TRUE) {
            *state = 2;
        }
        break;
    case 2:
        if (ClearSaveData_ConfirmFlow(appData) == TRUE) {
            StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 6, 1, appData->heapID);
            *state = 3;
        }
        break;
    case 3:
        if (IsScreenFadeDone() == TRUE) {
            ClearSaveData_FreeMessage(appData);
            ClearSaveData_FreeGraphics(appData);
            SetVBlankCallback(NULL, NULL);
            done = 1;
        }
        break;
    }

    return done;
}

int ClearSaveData_Exit(ApplicationManager *appMan, int *state)
{
    ClearSaveDataAppData *appData = ApplicationManager_Data(appMan);
    int heapID = appData->heapID;

    ApplicationManager_FreeData(appMan);
    Heap_Destroy(heapID);

    // The save file has been erased, so restart the game from the title screen.
    OS_ResetSystem(0);

    return 1;
}

static void ClearSaveData_VBlank(void *param)
{
    ClearSaveDataAppData *appData = param;
    Bg_RunScheduledUpdates(appData->bgConfig);
}

static void ClearSaveData_InitGraphics(ClearSaveDataAppData *appData)
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
        appData->bgConfig = BgConfig_New(appData->heapID);
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
        Bg_InitFromTemplate(appData->bgConfig, BG_LAYER_MAIN_0, &v2, 0);
        Bg_ClearTilemap(appData->bgConfig, BG_LAYER_MAIN_0);
    }

    LoadMessageBoxGraphics(appData->bgConfig, BG_LAYER_MAIN_0, 512 - (18 + 12), 2, 0, appData->heapID);
    LoadStandardWindowGraphics(appData->bgConfig, BG_LAYER_MAIN_0, 512 - (18 + 12) - 9, 3, 0, appData->heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), appData->heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, appData->heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);
}

static void ClearSaveData_FreeGraphics(ClearSaveDataAppData *appData)
{
    Bg_ToggleLayer(BG_LAYER_MAIN_0, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_1, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_2, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_3, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_0, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_1, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_2, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_3, 0);
    Bg_FreeTilemapBuffer(appData->bgConfig, BG_LAYER_MAIN_0);
    Heap_Free(appData->bgConfig);
}

static void ClearSaveData_InitMessage(ClearSaveDataAppData *appData)
{
    appData->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0004, appData->heapID);
    Text_ResetAllPrinters();
    appData->messageState = 0;
    Window_AddFromTemplate(appData->bgConfig, &appData->window, &sClearSaveDataMessageWindowTemplate);
    Window_FillRectWithColor(&appData->window, 15, 0, 0, 27 * 8, 4 * 8);
}

static void ClearSaveData_FreeMessage(ClearSaveDataAppData *appData)
{
    Window_Remove(&appData->window);
    MessageLoader_Free(appData->messageLoader);
}

// Drives the two-step delete confirmation:
//   0-1: show "Delete all saved data?" and its Yes/No menu,
//   2-3: show the "cannot be recovered" warning and its Yes/No menu,
//   4-5: show "Deleting all data" and erase the save file,
//   6:   clear the message box and report completion.
// Choosing "No" (menu result 0xfffffffe) skips straight to state 6.
static BOOL ClearSaveData_ConfirmFlow(ClearSaveDataAppData *appData)
{
    BOOL done = 0;

    switch (appData->confirmState) {
    case 0:
        if (ClearSaveData_ShowMessage(appData, 0, 1, 4) == TRUE) {
            appData->menu = Menu_MakeYesNoChoiceWithCursorAt(appData->bgConfig, &sYesNoMenuWindowTemplate, 512 - (18 + 12) - 9, 3, 1, appData->heapID);
            appData->confirmState = 1;
        }
        break;
    case 1: {
        u32 menuInput = Menu_ProcessInputAndHandleExit(appData->menu, appData->heapID);

        switch (menuInput) {
        case 0:
            appData->confirmState = 2;
            break;
        case 0xfffffffe:
            appData->confirmState = 6;
            break;
        }
    } break;
    case 2:
        if (ClearSaveData_ShowMessage(appData, 1, 1, 4) == TRUE) {
            appData->menu = Menu_MakeYesNoChoiceWithCursorAt(appData->bgConfig, &sYesNoMenuWindowTemplate, (512 - (18 + 12)) - 9, 3, 1, appData->heapID);
            appData->confirmState = 3;
        }
        break;
    case 3: {
        u32 menuInput = Menu_ProcessInputAndHandleExit(appData->menu, appData->heapID);

        switch (menuInput) {
        case 0:
            appData->confirmState = 4;
            break;
        case 0xfffffffe:
            appData->confirmState = 6;
            break;
        }
    } break;
    case 4:
        if (ClearSaveData_ShowMessage(appData, 2, 1, 0) == TRUE) {
            appData->waitDial = Window_AddWaitDial(&appData->window, 512 - (18 + 12));
            appData->confirmState = 5;
        }
        break;
    case 5:
        SaveData_Erase(appData->saveData);
        DestroyWaitDial(appData->waitDial);
        appData->confirmState = 6;
        break;
    case 6:
        Bg_ClearTilemap(appData->bgConfig, BG_LAYER_MAIN_0);
        done = 1;
        break;
    }

    return done;
}

// Prints message messageID in the message box and returns TRUE once it has
// finished. renderDelay is the text speed passed to the printer; 0 prints the
// whole string at once, in which case the string is freed immediately. When
// autoAdvance is 0 the message also waits for the A button before returning.
static BOOL ClearSaveData_ShowMessage(ClearSaveDataAppData *appData, u32 messageID, int autoAdvance, int renderDelay)
{
    BOOL done = 0;

    switch (appData->messageState) {
    case 0:
        Window_FillRectWithColor(&appData->window, 15, 0, 0, 27 * 8, 4 * 8);
        Window_DrawMessageBoxWithScrollCursor(&appData->window, 0, 512 - (18 + 12), 2);

        appData->string = String_Init(0x400, appData->heapID);
        MessageLoader_GetString(appData->messageLoader, messageID, appData->string);
        appData->printerID = Text_AddPrinterWithParams(&appData->window, FONT_MESSAGE, appData->string, 0, 0, renderDelay, NULL);

        if (renderDelay == 0) {
            // Instant text: nothing left to wait for, so skip the printer state.
            String_Free(appData->string);
            appData->messageState++;
        }

        appData->messageState++;
        break;
    case 1:
        if (!(Text_IsPrinterActive(appData->printerID))) {
            String_Free(appData->string);
            appData->messageState++;
        }
        break;
    case 2:
        if ((autoAdvance != 0) || (gSystem.pressedKeys & 1)) {
            appData->messageState = 0;
            done = 1;
        }
    }

    return done;
}
