#include "number_entry_app.h"

#include <dwc.h>
#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "struct_defs/struct_02089438.h"
#include "struct_defs/struct_02089688.h"

#include "bg_window.h"
#include "game_options.h"
#include "gx_layers.h"
#include "heap.h"
#include "narc.h"
#include "network_icon.h"
#include "overlay_manager.h"
#include "palette.h"
#include "sprite_system.h"
#include "string_gf.h"
#include "system.h"
#include "touch_pad.h"
#include "touch_screen_actions.h"
#include "number_entry_screen.h"
#include "number_entry_graphics.h"
#include "vram_transfer.h"

#include "constdata/const_020F2DBC.h"

// Application manager for the number-entry screen. It owns the screen's heap,
// graphics resources and touch pad, and drives the screen's update loop. The
// screen itself (layout, input, rendering) lives in number_entry_screen.c and
// number_entry_graphics.c; this module only wires it into the application
// framework and sets up the shared BG/VRAM configuration.

static void NumberEntryApp_InitGraphics(BgConfig *bgConfig);
static void NumberEntryApp_VBlankCallback(void *data);
static int NumberEntryApp_Init(ApplicationManager *appMan, int *param1);
static int NumberEntryApp_Main(ApplicationManager *appMan, int *param1);
static int NumberEntryApp_Exit(ApplicationManager *appMan, int *param1);

const ApplicationManagerTemplate gNumberEntryAppTemplate = {
    NumberEntryApp_Init,
    NumberEntryApp_Main,
    NumberEntryApp_Exit,
    0xffffffff,
};

// Application init: create the screen's heap and data, set up the graphics
// resources and touch pad, and install the VBlank callback.
static int NumberEntryApp_Init(ApplicationManager *appMan, int *param1)
{
    NumberEntryScreen *v0;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_101, 0x40000);

    v0 = ApplicationManager_NewData(appMan, sizeof(NumberEntryScreen), HEAP_ID_101);
    memset(v0, 0, sizeof(NumberEntryScreen));
    v0->args = *((NumberEntryArgs *)ApplicationManager_Args(appMan));

    SetVBlankCallback(NULL, NULL);
    DisableHBlank();
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();

    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);

    v0->graphics.narc = NARC_ctor(NARC_INDEX_ARC__CODEIN_GRA, HEAP_ID_101);
    v0->graphics.bgConfig = BgConfig_New(HEAP_ID_101);
    v0->graphics.paletteData = PaletteData_New(HEAP_ID_101);

    PaletteData_SetAutoTransparent(v0->graphics.paletteData, TRUE);
    PaletteData_AllocBuffer(v0->graphics.paletteData, PLTTBUF_MAIN_BG, PALETTE_SIZE_BYTES * 16, HEAP_ID_101);
    PaletteData_AllocBuffer(v0->graphics.paletteData, PLTTBUF_SUB_BG, PALETTE_SIZE_BYTES * 16, HEAP_ID_101);
    PaletteData_AllocBuffer(v0->graphics.paletteData, PLTTBUF_MAIN_OBJ, PALETTE_SIZE_BYTES * 16, HEAP_ID_101);
    PaletteData_AllocBuffer(v0->graphics.paletteData, PLTTBUF_SUB_OBJ, PALETTE_SIZE_BYTES * 16, HEAP_ID_101);
    NumberEntryApp_InitGraphics(v0->graphics.bgConfig);
    NumberEntryGraphics_InitSpriteSystem(v0);
    NumberEntry_InitLayout(v0);

    {
        EnableTouchPad();
        InitializeTouchPad(4);
        NumberEntry_InitTouchScreen(v0);
    }

    // Wi-Fi screens show the connection-strength icon on the sub screen.
    if (v0->args.showNetworkIcon != 0) {
        NetworkIcon_Init();
        NetworkIcon_CreateOnSubScreen(1, HEAP_ID_101);
    }

    G2_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2, 15, 7);
    G2S_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3, 7, 8);

    SetVBlankCallback(NumberEntryApp_VBlankCallback, v0);

    return 1;
}

// Application main loop: advance the screen and refresh the Wi-Fi strength
// icon. Returns non-zero once the screen has finished.
static int NumberEntryApp_Main(ApplicationManager *appMan, int *param1)
{
    BOOL v0;
    NumberEntryScreen *v1 = ApplicationManager_Data(appMan);
    v0 = NumberEntry_Update(v1);

    if (v1->args.showNetworkIcon != 0) {
        NetworkIcon_SetStrength(WM_LINK_LEVEL_3 - DWC_GetLinkLevel());
    }

    return v0 ? 1 : 0;
}

// Application exit: tear down the screen's graphics, touch pad and heap.
static int NumberEntryApp_Exit(ApplicationManager *appMan, int *param1)
{
    NumberEntryScreen *v0 = ApplicationManager_Data(appMan);

    if (v0->args.showNetworkIcon != 0) {
        NetworkIcon_Destroy();
    }

    NumberEntryGraphics_Free(v0);
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG0, 0);
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG1, 0);
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG2, 0);
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG3, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG0, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG1, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG2, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG3, 0);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 1);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 2);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 3);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 4);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 5);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 6);
    Bg_FreeTilemapBuffer(v0->graphics.bgConfig, 7);
    Heap_Free(v0->graphics.bgConfig);
    PaletteData_FreeBuffer(v0->graphics.paletteData, PLTTBUF_MAIN_BG);
    PaletteData_FreeBuffer(v0->graphics.paletteData, PLTTBUF_SUB_BG);
    PaletteData_FreeBuffer(v0->graphics.paletteData, PLTTBUF_MAIN_OBJ);
    PaletteData_FreeBuffer(v0->graphics.paletteData, PLTTBUF_SUB_OBJ);
    PaletteData_Free(v0->graphics.paletteData);
    NARC_dtor(v0->graphics.narc);

    {
        u32 v1;
        v1 = DisableTouchPad();
    }

    SpriteSystem_FreeResourcesAndManager(v0->graphics.spriteSystem, v0->graphics.spriteManager);
    SpriteSystem_Free(v0->graphics.spriteSystem);
    TouchScreenActions_Free(v0->graphics.touchScreenActions);
    ApplicationManager_FreeData(appMan);
    Heap_Destroy(HEAP_ID_101);

    return 1;
}

// Allocates and fills the arguments shared with the number-entry screen. The
// caller supplies the per-group digit counts in digitsPerGroup; index 4 is
// filled with a copy of the last group's count so the screen can use it as a
// sentinel when deriving the group slot ranges.
static NumberEntryArgs *NumberEntryArgs_Alloc(enum HeapID heapID, int digitCount, int digitsPerGroup[], Options *options, u32 messageEntry, u32 showNetworkIcon)
{
    int i;
    NumberEntryArgs *v1 = NULL;

    v1 = Heap_Alloc(heapID, sizeof(NumberEntryArgs));

    v1->digitCount = digitCount;
    v1->numberString = String_Init(digitCount + 1, heapID);
    v1->options = options;

    for (i = 0; i < 4; i++) {
        v1->digitsPerGroup[i] = digitsPerGroup[i];
    }

    v1->digitsPerGroup[i] = digitsPerGroup[i - 1];
    v1->messageEntry = messageEntry;
    v1->showNetworkIcon = showNetworkIcon;

    return v1;
}

// Creates arguments for a screen with no pre-filled groups.
NumberEntryArgs *NumberEntryArgs_New(enum HeapID heapID, int digitCount, int digitsPerGroup[], Options *options, u32 messageEntry, u32 showNetworkIcon)
{
    NumberEntryArgs *v0 = NumberEntryArgs_Alloc(heapID, digitCount, digitsPerGroup, options, messageEntry, showNetworkIcon);

    v0->prefilledGroupCount = 0;
    v0->prefilledDigits = 0;
    return v0;
}

// Creates arguments for a screen whose first prefilledGroupCount groups are
// pre-filled with prefilledDigits and cannot be edited.
NumberEntryArgs *NumberEntryArgs_NewWithPrefilled(enum HeapID heapID, int digitCount, int digitsPerGroup[], Options *options, u32 messageEntry, u32 showNetworkIcon, u32 prefilledGroupCount, u32 prefilledDigits)
{
    NumberEntryArgs *v0 = NumberEntryArgs_Alloc(heapID, digitCount, digitsPerGroup, options, messageEntry, showNetworkIcon);

    v0->prefilledGroupCount = prefilledGroupCount;
    v0->prefilledDigits = prefilledDigits;
    return v0;
}

void NumberEntryArgs_Free(NumberEntryArgs *args)
{
    GF_ASSERT(args->numberString != NULL);
    GF_ASSERT(args != NULL);

    String_Free(args->numberString);
    Heap_Free(args);
}

// Configures the VRAM banks and BG layers shared by the number-entry screen.
// The main engine shows the digit row on BG1; the sub engine shows the message
// box on BG0/BG1. The screens are swapped so the sub engine is displayed on the
// top screen.
static void NumberEntryApp_InitGraphics(BgConfig *bgConfig)
{
    GXLayers_DisableEngineALayers();

    {
        GXBanks v0 = {
            GX_VRAM_BG_128_A,
            GX_VRAM_BGEXTPLTT_NONE,
            GX_VRAM_SUB_BG_32_H,
            GX_VRAM_SUB_BGEXTPLTT_NONE,
            GX_VRAM_OBJ_64_E,
            GX_VRAM_OBJEXTPLTT_NONE,
            GX_VRAM_SUB_OBJ_16_I,
            GX_VRAM_SUB_OBJEXTPLTT_NONE,
            GX_VRAM_TEX_01_BC,
            GX_VRAM_TEXPLTT_01_FG
        };

        GXLayers_SetBanks(&v0);

        MI_CpuClear32((void *)HW_BG_VRAM, HW_BG_VRAM_SIZE);
        MI_CpuClear32((void *)HW_DB_BG_VRAM, HW_DB_BG_VRAM_SIZE);
        MI_CpuClear32((void *)HW_OBJ_VRAM, HW_OBJ_VRAM_SIZE);
        MI_CpuClear32((void *)HW_DB_OBJ_VRAM, HW_DB_OBJ_VRAM_SIZE);
    }

    {
        BgTemplate v1[] = {
            {
                .x = 0,
                .y = 0,
                .bufferSize = 0x800,
                .baseTile = 0,
                .screenSize = BG_SCREEN_SIZE_256x256,
                .colorMode = GX_BG_COLORMODE_16,
                .screenBase = GX_BG_SCRBASE_0x0000,
                .charBase = GX_BG_CHARBASE_0x04000,
                .bgExtPltt = GX_BG_EXTPLTT_01,
                .priority = 0x1,
                .areaOver = 0,
                .mosaic = FALSE,
            },
            {
                .x = 0,
                .y = 0,
                .bufferSize = 0x1000,
                .baseTile = 0,
                .screenSize = BG_SCREEN_SIZE_256x256,
                .colorMode = GX_BG_COLORMODE_16,
                .screenBase = GX_BG_SCRBASE_0x1000,
                .charBase = GX_BG_CHARBASE_0x0c000,
                .bgExtPltt = GX_BG_EXTPLTT_01,
                .priority = 0x2,
                .areaOver = 0,
                .mosaic = FALSE,
            },
            {
                .x = 0,
                .y = 0,
                .bufferSize = 0x1800,
                .baseTile = 0,
                .screenSize = BG_SCREEN_SIZE_256x256,
                .colorMode = GX_BG_COLORMODE_16,
                .screenBase = GX_BG_SCRBASE_0x3000,
                .charBase = GX_BG_CHARBASE_0x10000,
                .bgExtPltt = GX_BG_EXTPLTT_01,
                .priority = 0x3,
                .areaOver = 0,
                .mosaic = FALSE,
            },
        };

        Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_1, &v1[0], 0);
        Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_2, &v1[1], 0);
        Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_3, &v1[2], 0);
        Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_1);
        Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_2);
        Bg_ClearTilemap(bgConfig, BG_LAYER_MAIN_3);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG0, 0);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG1, 1);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG2, 0);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG3, 0);
    }

    {
        BgTemplate v2[] = {
            {
                .x = 0,
                .y = 0,
                .bufferSize = 0x800,
                .baseTile = 0,
                .screenSize = BG_SCREEN_SIZE_256x256,
                .colorMode = GX_BG_COLORMODE_16,
                .screenBase = GX_BG_SCRBASE_0x6800,
                .charBase = GX_BG_CHARBASE_0x00000,
                .bgExtPltt = GX_BG_EXTPLTT_01,
                .priority = 0x0,
                .areaOver = 0,
                .mosaic = FALSE,
            },
            {
                .x = 0,
                .y = 0,
                .bufferSize = 0x800,
                .baseTile = 0,
                .screenSize = BG_SCREEN_SIZE_256x256,
                .colorMode = GX_BG_COLORMODE_16,
                .screenBase = GX_BG_SCRBASE_0x7000,
                .charBase = GX_BG_CHARBASE_0x04000,
                .bgExtPltt = GX_BG_EXTPLTT_01,
                .priority = 0x1,
                .areaOver = 0,
                .mosaic = FALSE,
            },
            { 0 },
            { 0 },
        };

        Bg_InitFromTemplate(bgConfig, BG_LAYER_SUB_0, &v2[0], 0);
        Bg_InitFromTemplate(bgConfig, BG_LAYER_SUB_1, &v2[1], 0);
        Bg_InitFromTemplate(bgConfig, BG_LAYER_SUB_2, &v2[2], 0);
        Bg_InitFromTemplate(bgConfig, BG_LAYER_SUB_3, &v2[3], 0);
        Bg_ClearTilemap(bgConfig, BG_LAYER_SUB_0);
        Bg_ClearTilemap(bgConfig, BG_LAYER_SUB_1);
        Bg_ClearTilemap(bgConfig, BG_LAYER_SUB_2);
        Bg_ClearTilemap(bgConfig, BG_LAYER_SUB_3);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG0, 1);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG1, 1);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG2, 0);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG3, 0);
    }

    gSystem.whichScreenIs3D = DS_SCREEN_SUB;

    GXLayers_SwapDisplay();
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_OBJ, 1);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_OBJ, 1);
}

// VBlank callback: flush the pending VRAM transfers, OAM and palette updates.
static void NumberEntryApp_VBlankCallback(void *data)
{
    NumberEntryScreen *v0 = data;

    VramTransfer_Process();
    SpriteSystem_TransferOam();
    PaletteData_CommitFadedBuffers(v0->graphics.paletteData);
    Bg_RunScheduledUpdates(v0->graphics.bgConfig);

    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
}
