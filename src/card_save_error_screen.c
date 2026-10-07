#include "card_save_error_screen.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "bg_window.h"
#include "brightness_controller.h"
#include "font.h"
#include "gx_layers.h"
#include "heap.h"
#include "main.h"
#include "message.h"
#include "render_window.h"
#include "screen_fade.h"
#include "string_gf.h"
#include "system.h"
#include "text.h"

/**
 * @file card_save_error_screen.c
 *
 * Card-save error screen. Shown when saving or loading the game save on the DS
 * Game Card fails, as reported by SaveData_CardSave_Error(). It takes over both
 * DS screens, draws a single full-screen window, prints one message from
 * TEXT_BANK_UNK_0006 chosen by the save error, and then spins forever waiting
 * on VBlank before forcing the console to power off.
 */

// VRAM banks used by the error screen: a single 256-color BG on the main
// engine, with no sub-engine, OBJ, or extended-palette banks.
static const GXBanks sCardSaveErrorScreenBanks = {
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

// Graphics modes for the error screen: plain 2D BG mode 0.
static const GraphicsModes sCardSaveErrorScreenGraphicsModes = {
    GX_DISPMODE_GRAPHICS,
    GX_BGMODE_0,
    GX_BGMODE_0,
    GX_BG0_AS_2D
};

// Background template for the message layer (BG0 of the main engine).
static const BgTemplate sCardSaveErrorScreenBgTemplate = {
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

// Window covering the message area: 26x18 tiles at (3, 3).
static const WindowTemplate sCardSaveErrorScreenWindowTemplate = {
    0x0,
    0x3,
    0x3,
    0x1A,
    0x12,
    0x1,
    0x23
};

/**
 * @brief Show the DS Game Card save-error screen.
 *
 * Displays a message from TEXT_BANK_UNK_0006 selected by @p errorID: a value of
 * SAVE_ERROR_DISABLE_WRITE selects message 1, while any other value (i.e.
 * SAVE_ERROR_DISABLE_READ) selects message 0. The screen is terminal — it idles
 * on VBlank until the player powers the console off, after which
 * PM_ForceToPowerOff() shuts it down.
 */
void CardSaveErrorScreen_Show(enum HeapID heapID, int errorID)
{
    BgConfig *bgConfig;
    Window window;
    MessageLoader *messageLoader;
    String *string;
    int messageIndex;

    // The message index is the inverse of the save error: write errors use
    // TEXT_BANK_UNK_0006 message 1, read errors use message 0.
    if (errorID == 0) {
        messageIndex = 1;
    } else {
        messageIndex = 0;
    }

    // Blank both screens and tear down any existing display configuration.
    SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
    SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);
    SetDummyVBlankIntr();
    SetVBlankCallback(NULL, NULL);
    SetHBlankCallback(NULL, NULL);
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();

    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);

    SetAutorepeat(4, 8);
    gSystem.whichScreenIs3D = DS_SCREEN_MAIN;
    GXLayers_SwapDisplay();

    G2_BlendNone();
    G2S_BlendNone();
    GX_SetVisibleWnd(GX_WNDMASK_NONE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);

    GXLayers_SetBanks(&sCardSaveErrorScreenBanks);

    bgConfig = BgConfig_New(heapID);

    // Set up the single message background and its window graphics.
    SetAllGraphicsModes(&sCardSaveErrorScreenGraphicsModes);
    Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_0, &sCardSaveErrorScreenBgTemplate, 0);
    Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_0);
    LoadStandardWindowGraphics(bgConfig, BG_LAYER_MAIN_0, 512 - 9, 2, 0, heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);

    messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0006, heapID);
    string = String_Init(0x180, heapID);

    // Draw the window frame and print the selected message instantly.
    Text_ResetAllPrinters();
    Window_AddFromTemplate(bgConfig, &window, &sCardSaveErrorScreenWindowTemplate);
    Window_FillRectWithColor(&window, 15, 0, 0, 26 * 8, 18 * 8);
    Window_DrawStandardFrame(&window, 0, 512 - 9, 2);
    MessageLoader_GetString(messageLoader, messageIndex, string);
    Text_AddPrinterWithParams(&window, FONT_SYSTEM, string, 0, 0, TEXT_SPEED_INSTANT, NULL);
    String_Free(string);
    GXLayers_TurnBothDispOn();
    ResetScreenMasterBrightness(DS_SCREEN_MAIN);
    ResetScreenMasterBrightness(DS_SCREEN_SUB);
    BrightnessController_SetScreenBrightness(0, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_BOTH_SCREENS);

    // The message is terminal: idle on VBlank until the player powers off.
    while (TRUE) {
        HandleConsoleFold();
        OS_WaitIrq(1, OS_IE_V_BLANK);
    }

    // Unreachable in practice, but kept for symmetry with the setup above.
    Window_Remove(&window);
    MessageLoader_Free(messageLoader);
    Bg_ToggleLayer(BG_LAYER_MAIN_0, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_1, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_2, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_3, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_0, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_1, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_2, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_3, 0);
    Bg_FreeTilemapBuffer(bgConfig, BG_LAYER_MAIN_0);
    Heap_Free(bgConfig);

    PM_ForceToPowerOff();
}
