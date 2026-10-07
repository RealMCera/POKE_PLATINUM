#include "field_blackout.h"

#include <nitro.h>
#include <string.h>

#include "generated/text_banks.h"

#include "field/field_system.h"

#include "bg_window.h"
#include "brightness_controller.h"
#include "field_bgm.h"
#include "field_map_change.h"
#include "field_map_change_flags.h"
#include "field_overworld_state.h"
#include "field_system.h"
#include "field_task.h"
#include "field_transition.h"
#include "font.h"
#include "graphics.h"
#include "gx_layers.h"
#include "heap.h"
#include "location.h"
#include "message.h"
#include "party.h"
#include "pokemon.h"
#include "render_window.h"
#include "save_player.h"
#include "screen_fade.h"
#include "script_manager.h"
#include "sound_playback.h"
#include "spawn_locations.h"
#include "string_gf.h"
#include "string_template.h"
#include "system.h"
#include "text.h"

// State for the black-out message scene shown while the map is changing.
typedef struct FieldBlackoutScene {
    int state; // Scene task state.
    FieldSystem *fieldSystem;
    BgConfig *bgConfig;
    Window window;
    MessageLoader *messageLoader;
    StringTemplate *stringTemplate;
} FieldBlackoutScene;

static void FieldBlackout_InitGraphics(BgConfig *bgConfig);
static void FieldBlackout_InitScene(FieldSystem *fieldSystem, FieldTask *task);
static BOOL FieldBlackout_SceneTask(FieldTask *task);
static void FieldBlackout_PrintMessage(FieldBlackoutScene *scene, u16 messageID, u8 x, u8 y);

// Window used to display the black-out message on the main screen.
static const WindowTemplate sBlackOutWindowTemplate = {
    0x3,
    0x4,
    0x5,
    0x19,
    0xF,
    0xD,
    0x1
};

// Configures the graphics banks, display modes and font palette used by the
// black-out message scene.
static void FieldBlackout_InitGraphics(BgConfig *bgConfig)
{
    static const GXBanks v0 = {
        GX_VRAM_BG_128_B,
        GX_VRAM_BGEXTPLTT_NONE,
        GX_VRAM_SUB_BG_128_C,
        GX_VRAM_SUB_BGEXTPLTT_NONE,
        GX_VRAM_OBJ_64_E,
        GX_VRAM_OBJEXTPLTT_NONE,
        GX_VRAM_SUB_OBJ_16_I,
        GX_VRAM_SUB_OBJEXTPLTT_NONE,
        GX_VRAM_TEX_0_A,
        GX_VRAM_TEXPLTT_01_FG
    };
    static const GraphicsModes v1 = {
        GX_DISPMODE_GRAPHICS,
        GX_BGMODE_0,
        GX_BGMODE_0,
        GX_BG0_AS_2D
    };
    static const BgTemplate v2 = {
        .x = 0,
        .y = 0,
        .bufferSize = 0x800,
        .baseTile = 0,
        .screenSize = BG_SCREEN_SIZE_256x256,
        .colorMode = GX_BG_COLORMODE_16,
        .screenBase = GX_BG_SCRBASE_0xf800,
        .charBase = GX_BG_CHARBASE_0x00000,
        .bgExtPltt = GX_BG_EXTPLTT_01,
        .priority = 1,
        .areaOver = 0,
        .mosaic = FALSE,
    };

    GXLayers_SetBanks(&v0);
    SetAllGraphicsModes(&v1);
    Bg_InitFromTemplate(bgConfig, BG_LAYER_MAIN_3, &v2, 0);
    Graphics_LoadPalette(NARC_INDEX_GRAPHIC__PL_FONT, 6, 0, 13 * 0x20, 0x20, HEAP_ID_FIELD2);
}

// Allocates and initializes the black-out message scene, then hands control to
// FieldBlackout_SceneTask. The message shown depends on where the player blacks
// out: the Twinleaf player house uses the "scurried back home" text, everywhere
// else uses the "scurried to a Pokémon Center" text.
static void FieldBlackout_InitScene(FieldSystem *fieldSystem, FieldTask *task)
{
    FieldBlackoutScene *scene = Heap_Alloc(HEAP_ID_FIELD2, sizeof(FieldBlackoutScene));

    if (scene == NULL) {
        GF_ASSERT(FALSE);
    }

    memset(scene, 0, sizeof(FieldBlackoutScene));

    scene->state = 0;
    scene->fieldSystem = fieldSystem;
    scene->bgConfig = BgConfig_New(HEAP_ID_FIELD2);

    FieldBlackout_InitGraphics(scene->bgConfig);

    scene->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_BLACK_OUT_SCENE, HEAP_ID_FIELD2);
    scene->stringTemplate = StringTemplate_Default(HEAP_ID_FIELD2);

    Window_AddFromTemplate(scene->bgConfig, &scene->window, &sBlackOutWindowTemplate);
    StringTemplate_SetPlayerName(scene->stringTemplate, 0, SaveData_GetTrainerInfo(FieldSystem_GetSaveData(fieldSystem)));

    if (fieldSystem->location->mapHeaderID == MAP_HEADER_TWINLEAF_TOWN_PLAYER_HOUSE_1F) {
        FieldBlackout_PrintMessage(scene, 4, 0, 0);
    } else {
        FieldBlackout_PrintMessage(scene, 3, 0, 0);
    }

    Window_CopyToVRAM(&scene->window);
    FieldTask_InitCall(task, FieldBlackout_SceneTask, scene);

    return;
}

// Runs the black-out message scene: fade in, wait for the player to dismiss the
// message, fade out, then tear down the scene.
static BOOL FieldBlackout_SceneTask(FieldTask *task)
{
    FieldBlackoutScene *scene = FieldTask_GetEnv(task);

    switch (scene->state) {
    case 0:
        StartScreenFade(FADE_MAIN_ONLY, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_MAX, COLOR_BLACK, 8, 1, HEAP_ID_FIELD3);
        scene->state++;
        break;
    case 1:
        if (IsScreenFadeDone()) {
            scene->state++;
        }
        break;
    case 2:
        if ((gSystem.pressedKeys & PAD_BUTTON_A) || (gSystem.pressedKeys & PAD_BUTTON_B)) {
            StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 8, 1, HEAP_ID_FIELD3);
            scene->state++;
        }
        break;
    case 3:
        if (IsScreenFadeDone()) {
            Window_FillTilemap(&scene->window, 0);
            scene->state++;
        }
        break;
    case 4:
        Window_EraseMessageBox(&scene->window, 0);
        Window_Remove(&scene->window);
        StringTemplate_Free(scene->stringTemplate);
        MessageLoader_Free(scene->messageLoader);
        Bg_FreeTilemapBuffer(scene->bgConfig, BG_LAYER_MAIN_3);
        Heap_Free(scene->bgConfig);
        Heap_Free(scene);

        return 1;
    }

    return 0;
}

// Formats messageID with the scene's string template and prints it centered
// horizontally in the scene window.
static void FieldBlackout_PrintMessage(FieldBlackoutScene *scene, u16 messageID, u8 x, u8 y)
{
    String *v0 = String_Init(1024, HEAP_ID_FIELD2);
    String *v1 = String_Init(1024, HEAP_ID_FIELD2);

    Window_FillTilemap(&scene->window, 0);
    MessageLoader_GetString(scene->messageLoader, messageID, v0);
    StringTemplate_Format(scene->stringTemplate, v1, v0);

    {
        u32 v2 = Font_CalcMaxLineWidth(FONT_SYSTEM, v1, 0);
        x = (u8)(scene->window.width * 8 - v2) / 2 - 4;
    }

    Text_AddPrinterWithParamsAndColor(&scene->window, FONT_SYSTEM, v1, x, y, TEXT_SPEED_NO_TRANSFER, TEXT_COLOR(15, 2, 0), NULL);
    String_Free(v0);
    String_Free(v1);

    return;
}

// Field task that handles blacking out from a lost battle. It restores Giratina
// to its Altered Form, warps the player to the black-out spawn point, fades out
// the BGM, dims the screens while the black-out message scene plays, performs
// the map transition, then runs the recovery script that heals the party.
BOOL FieldTask_BlackOutFromBattle(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    int *state = FieldTask_GetState(task);

    switch (*state) {
    case 0: {
        if ((fieldSystem != NULL) && (fieldSystem->saveData != NULL)) {
            Party_SetGiratinaForm(SaveData_GetParty(fieldSystem->saveData), GIRATINA_FORM_ALTERED);
        }

        Location location;
        FieldOverworldState *fieldState = SaveData_GetFieldOverworldState(fieldSystem->saveData);
        u16 warpId = FieldOverworldState_GetBlackOutWarpId(fieldState);

        Location_InitBlackOut(warpId, &location);
        Location_InitFly(warpId, FieldOverworldState_GetExitLocation(fieldState));
        FieldTask_ChangeMapByLocation(task, &location);
        FieldSystem_ClearPartnerTrainer(fieldSystem);
        (*state)++;
        break;
    }
    case 1:
        Sound_FadeOutBGM(0, 20);
        (*state)++;
        break;
    case 2:
        if (Sound_IsFadeActive() == FALSE) {
            FieldBGM_Stop();
            (*state)++;
        }
        break;
    case 3:
        BrightnessController_SetScreenBrightness(-16, (GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD) ^ GX_BLEND_PLANEMASK_BG3, BRIGHTNESS_MAIN_SCREEN);
        BrightnessController_SetScreenBrightness(-16, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_SUB_SCREEN);
        FieldBlackout_InitScene(fieldSystem, task);
        (*state)++;
        break;
    case 4:
        FieldTransition_StartMap(task);
        (*state)++;
        break;
    case 5:
        BrightnessController_SetScreenBrightness(0, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ | GX_BLEND_PLANEMASK_BD, BRIGHTNESS_BOTH_SCREENS);

        // The default warp means the player blacked out at home; any other warp
        // means they were sent to a Pokémon Center.
        if (FieldOverworldState_GetDefaultWarpID() == FieldOverworldState_GetBlackOutWarpId(SaveData_GetFieldOverworldState(fieldSystem->saveData))) {
            ScriptManager_Start(task, SCRIPT_ID(COMMON_SCRIPTS, 20), NULL, NULL);
        } else {
            ScriptManager_Start(task, SCRIPT_ID(COMMON_SCRIPTS, 21), NULL, NULL);
        }

        (*state)++;
        break;
    case 6:
        return 1;
    }

    return 0;
}

void FieldTask_StartBlackOutFromBattle(FieldTask *task)
{
    FieldTask_InitCall(task, FieldTask_BlackOutFromBattle, NULL);
}
