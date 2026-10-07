#include "fatal_error_screen.h"

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
 * @file fatal_error_screen.c
 *
 * Fatal-error message screens. Each entry point takes over both DS screens,
 * draws a single full-screen window, prints one message from
 * TEXT_BANK_UNK_0005, and then spins forever waiting on VBlank before forcing
 * the console to power off. The two entry points differ only in which message
 * they display.
 */

// VRAM banks used by the error screen: a single 256-color BG on the main
// engine, with no sub-engine, OBJ, or extended-palette banks.
static const GXBanks sFatalErrorScreenBanks = {
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
static const GraphicsModes sFatalErrorScreenGraphicsModes = {
    GX_DISPMODE_GRAPHICS,
    GX_BGMODE_0,
    GX_BGMODE_0,
    GX_BG0_AS_2D
};

// Background template for the message layer (BG0 of the main engine).
static const BgTemplate sFatalErrorScreenBgTemplate = {
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
static const WindowTemplate sFatalErrorScreenWindowTemplate = {
    0x0,
    0x3,
    0x3,
    0x1A,
    0x12,
    0x1,
    0x23
};

/**
 * @brief Show the "save data could not be accessed" fatal-error screen.
 *
 * Displays TEXT_BANK_UNK_0005 message 0, which asks the player to power off
 * and reinsert the DS Game Card. Used when the save backup is missing or a
 * card read fails.
 */
void FatalErrorScreen_ShowSaveDataError(enum HeapID heapID)
{
    BgConfig *bgConfig;
    Window window;
    MessageLoader *messageLoader;
    String *string;
    int messageIndex = 0;

    // Blank both screens and tear down any existing display configuration.
    SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
    SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);
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
    GXLayers_SetBanks(&sFatalErrorScreenBanks);

    bgConfig = BgConfig_New(heapID);

    // Set up the single message background and its window graphics.
    SetAllGraphicsModes(&sFatalErrorScreenGraphicsModes);
    Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_0, &sFatalErrorScreenBgTemplate, 0);
    Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_0);
    LoadStandardWindowGraphics(bgConfig, BG_LAYER_MAIN_0, 512 - 9, 2, 0, heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 27681);
    Bg_MaskPalette(BG_LAYER_SUB_0, 27681);

    messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0005, heapID);
    string = String_Init(384, heapID);

    // Draw the window frame and print the message instantly.
    Text_ResetAllPrinters();
    Window_AddFromTemplate(bgConfig, &window, &sFatalErrorScreenWindowTemplate);
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

/**
 * @brief Show the "data could not be read" fatal-error screen.
 *
 * Displays TEXT_BANK_UNK_0005 message 1, which asks the player to power off
 * and reinsert the Game Boy Advance Game Pak. Used when the GBA cartridge is
 * pulled out during migration.
 */
void FatalErrorScreen_ShowGbaPakError(enum HeapID heapID)
{
    BgConfig *bgConfig;
    Window window;
    MessageLoader *messageLoader;
    String *string;
    int messageIndex = 1;

    // Blank both screens and tear down any existing display configuration.
    SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
    SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);
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

    GXLayers_SetBanks(&sFatalErrorScreenBanks);

    bgConfig = BgConfig_New(heapID);

    // Set up the single message background and its window graphics.
    SetAllGraphicsModes(&sFatalErrorScreenGraphicsModes);
    Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_0, &sFatalErrorScreenBgTemplate, 0);
    Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_0);
    LoadStandardWindowGraphics(bgConfig, BG_LAYER_MAIN_0, 512 - 9, 2, 0, heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);

    messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0005, heapID);
    string = String_Init(0x180, heapID);

    // Draw the window frame and print the message instantly.
    Text_ResetAllPrinters();
    Window_AddFromTemplate(bgConfig, &window, &sFatalErrorScreenWindowTemplate);
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
