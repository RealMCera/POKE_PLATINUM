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

// VRAM banks used by the DWC init screen: only main BG VRAM is allocated.
static const GXBanks sDwcInitScreenBanks = {
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

// Display mode for the screen: 2D graphics with BG mode 0 on both engines.
static const GraphicsModes sDwcInitScreenGraphicsModes = {
    GX_DISPMODE_GRAPHICS,
    GX_BGMODE_0,
    GX_BGMODE_0,
    GX_BG0_AS_2D
};

// Full-screen background template for the message text.
static const BgTemplate sDwcInitScreenBgTemplate = {
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

// Window covering the message area, using the standard window frame.
static const WindowTemplate sDwcInitScreenWindowTemplate = {
    0x0,
    0x3,
    0x3,
    0x1A,
    0x12,
    0x1,
    0x23
};

// Shows the DWC init message screen and blocks until the player presses A.
// Called from NitroMain when WiFiList_InitDWC reports that the Wi-Fi user
// information was erased (DWC_INIT_RESULT_DESTROY_OTHER_SETTING).
void DwcInitScreen_Show(enum HeapID heapID, int unused)
{
    BgConfig *v0;
    Window v1;
    MessageLoader *v2;
    String *v3;
    int v4 = 16;

    // Reset both screens and disable all layers before setting up the message.
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

    GXLayers_SetBanks(&sDwcInitScreenBanks);
    v0 = BgConfig_New(heapID);

    // Set up the full-screen BG and the standard window frame.
    SetAllGraphicsModes(&sDwcInitScreenGraphicsModes);
    Bg_InitFromTemplate(v0, BG_LAYER_MAIN_0, &sDwcInitScreenBgTemplate, 0);
    Bg_ClearTilemap(v0, BG_LAYER_MAIN_0);
    LoadStandardWindowGraphics(v0, BG_LAYER_MAIN_0, 512 - 9, 2, 0, heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(1), heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_0, 32, 0, heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_0, 0x6c21);
    Bg_MaskPalette(BG_LAYER_SUB_0, 0x6c21);

    // Message 16 of TEXT_BANK_UNK_0695: the Wi-Fi user information was erased.
    v2 = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0695, heapID);
    v3 = String_Init(0x180, heapID);

    Text_ResetAllPrinters();
    Window_AddFromTemplate(v0, &v1, &sDwcInitScreenWindowTemplate);
    Window_FillRectWithColor(&v1, 15, 0, 0, 26 * 8, 18 * 8);
    Window_DrawStandardFrame(&v1, 0, 512 - 9, 2);
    MessageLoader_GetString(v2, v4, v3);
    Text_AddPrinterWithParams(&v1, FONT_SYSTEM, v3, 0, 0, TEXT_SPEED_INSTANT, NULL);
    String_Free(v3);
    GXLayers_TurnBothDispOn();
    ResetScreenMasterBrightness(DS_SCREEN_MAIN);
    ResetScreenMasterBrightness(DS_SCREEN_SUB);
    BrightnessController_SetScreenBrightness(0, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_BOTH_SCREENS);

    // Wait for the A button, one frame at a time.
    while (TRUE) {
        int v5 = PAD_Read();

        HandleConsoleFold();

        if (v5 & PAD_BUTTON_A) {
            break;
        }

        OS_WaitIrq(1, OS_IE_V_BLANK);
    }

    // Tear down the window, message loader, and BG layers.
    Window_Remove(&v1);
    MessageLoader_Free(v2);

    Bg_ToggleLayer(BG_LAYER_MAIN_0, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_1, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_2, 0);
    Bg_ToggleLayer(BG_LAYER_MAIN_3, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_0, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_1, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_2, 0);
    Bg_ToggleLayer(BG_LAYER_SUB_3, 0);
    Bg_FreeTilemapBuffer(v0, BG_LAYER_MAIN_0);
    Heap_Free(v0);
}
