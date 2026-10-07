#include "network_error.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "bg_window.h"
#include "brightness_controller.h"
#include "font.h"
#include "gx_layers.h"
#include "heap.h"
#include "message.h"
#include "render_window.h"
#include "screen_fade.h"
#include "string_gf.h"
#include "string_template.h"
#include "system.h"
#include "text.h"

#include "res/text/bank/network_errors.h"

static const GXBanks sNetworkErrorBanksConfig = {
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

static const GraphicsModes sNetworkErrorBgModeSet = {
    GX_DISPMODE_GRAPHICS,
    GX_BGMODE_0,
    GX_BGMODE_0,
    GX_BG0_AS_2D
};

static const BgTemplate sNetworkErrorBgTemplate = {
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

static const WindowTemplate sNetworkErrorWindowTemplate = {
    0x0,
    0x3,
    0x3,
    0x1A,
    0x12,
    0x1,
    0x23
};

// VBlank interrupt handler for the network error screen. It acknowledges the
// VBlank IRQ and waits for the pending GX DMA transfer to complete, keeping the
// displayed screen stable.
static void NetworkError_VBlankHandler(void)
{
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
    MI_WaitDma(GX_DEFAULT_DMAID);
}

void NetworkError_DisplayNetworkError(enum HeapID heapID, int networkErrorId, int errorCode)
{
    BgConfig *bgConfig;
    Window window;
    MessageLoader *messageLoader;
    String *formattedString;
    String *messageString;
    StringTemplate *stringTemplate;
    int networkErrorMessageId;

    // Select the message to print from TEXT_BANK_NETWORK_ERRORS using the
    // caller-supplied error ID. Unrecognized IDs fall back to the generic
    // communication error message.
    switch (networkErrorId) {
    case 0:
    default:
        networkErrorMessageId = NetworkError_Text_Generic;
        break;
    case 1:
        networkErrorMessageId = pl_msg_00000214_00002;
        break;
    case 2:
        networkErrorMessageId = pl_msg_00000214_00003;
        break;
    case 3:
        networkErrorMessageId = NetworkError_Text_GTSUnreachable;
        break;
    case 4:
        networkErrorMessageId = pl_msg_00000214_00005;
        break;
    case 5:
        networkErrorMessageId = pl_msg_00000214_00006;
        break;
    case 6:
        networkErrorMessageId = pl_msg_00000214_00007;
        break;
    }

    SetScreenColorBrightness(DS_SCREEN_MAIN, COLOR_BLACK);
    SetScreenColorBrightness(DS_SCREEN_SUB, COLOR_BLACK);

    (void)OS_DisableIrqMask(OS_IE_V_BLANK);
    OS_SetIrqFunction(OS_IE_V_BLANK, NetworkError_VBlankHandler);
    (void)OS_EnableIrqMask(OS_IE_V_BLANK);

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

    GXLayers_SetBanks(&sNetworkErrorBanksConfig);
    bgConfig = BgConfig_New(heapID);

    SetAllGraphicsModes(&sNetworkErrorBgModeSet);
    Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_0, &sNetworkErrorBgTemplate, 0);
    Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_0);
    LoadStandardWindowGraphics(bgConfig, BG_LAYER_MAIN_0, 512 - 9, 2, 0, heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);

    messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_NETWORK_ERRORS, heapID);
    formattedString = String_Init(0x180, heapID);
    messageString = String_Init(0x180, heapID);
    Text_ResetAllPrinters();
    stringTemplate = StringTemplate_Default(heapID);

    Window_AddFromTemplate(bgConfig, &window, &sNetworkErrorWindowTemplate);
    Window_FillRectWithColor(&window, 15, 0, 0, 26 * 8, 18 * 8);
    Window_DrawStandardFrame(&window, 0, 512 - 9, 2);

    // Substitute the error code into placeholder 0 of the selected message as a
    // zero-padded, 5-digit number, then format it into the string used below.
    StringTemplate_SetNumber(stringTemplate, 0, errorCode, 5, 2, 1);
    MessageLoader_GetString(messageLoader, networkErrorMessageId, messageString);
    StringTemplate_Format(stringTemplate, formattedString, messageString);

    Text_AddPrinterWithParams(&window, FONT_SYSTEM, formattedString, 0, 0, TEXT_SPEED_INSTANT, NULL);
    String_Free(formattedString);

    GXLayers_TurnBothDispOn();
    ResetScreenMasterBrightness(DS_SCREEN_MAIN);
    ResetScreenMasterBrightness(DS_SCREEN_SUB);
    BrightnessController_SetScreenBrightness(0, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_BOTH_SCREENS);

    Window_Remove(&window);
    MessageLoader_Free(messageLoader);
    StringTemplate_Free(stringTemplate);
    Heap_Free(bgConfig);
}
