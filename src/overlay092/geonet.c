#include "overlay092/geonet.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"
#include "constants/versions.h"

#include "struct_defs/wi_fi_history.h"

#include "overlay092/struct_ov92_021D1530.h"
#include "overlay092/struct_ov92_021D28C0.h"

#include "bg_window.h"
#include "camera.h"
#include "easy3d.h"
#include "font.h"
#include "game_options.h"
#include "graphics.h"
#include "gx_layers.h"
#include "heap.h"
#include "list_menu.h"
#include "math_util.h"
#include "menu.h"
#include "message.h"
#include "narc.h"
#include "overlay_manager.h"
#include "render_text.h"
#include "render_window.h"
#include "save_player.h"
#include "savedata.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "string_list.h"
#include "string_template.h"
#include "system.h"
#include "text.h"
#include "unk_0202419C.h"
#include "wifi_earth_place.h"
#include "wifi_history_save_data.h"

#include "res/text/bank/country_names.h"

// The Geonet app (Wi-Fi Earth): a globe opened from the Global Terminal. It
// lets the player register the country and region they live in, and shows a
// marker on the globe for every place they have connected with over Nintendo
// Wi-Fi Connection or DS Wireless Communications. The player can spin and zoom
// the globe with the +Control Pad or the touch screen, and press X to read the
// name of the place nearest the centre of the view.
//
// Geonet_Main drives the whole app as a state machine. States 0-13 walk through
// registration (intro message, country list, region list, confirmation); states
// 14-18 then show the interactive globe until the player backs out.

// A region entry loaded from a country's wifi_earth_place.narc member. The
// first entry of each member is a header and is skipped. This mirrors
// overlay069's struct_ov69_0225C980.
typedef struct WiFiEarthRegionEntry {
    s16 longitude;
    s16 latitude;
} WiFiEarthRegionEntry;

// A marker drawn on the globe. `rotation` orients the marker so that it stands
// upright on the globe's surface at (longitude, latitude).
typedef struct GeonetPlaceMarker {
    s16 longitude; // Angle around the globe's Y axis.
    s16 latitude; // Angle around the globe's X axis.
    MtxFx33 rotation;
    u16 communicationStatus; // 0 = never, 1 = today, 2 = in the past, 3 = the player's own location.
    u16 country; // Country_Text_* ID.
    u16 region;
} GeonetPlaceMarker;

// Every place marker loaded for the globe. `count` is the number of valid
// entries at the front of `markers`.
typedef struct GeonetPlaceMarkerList {
    u32 count;
    GeonetPlaceMarker markers[1024];
} GeonetPlaceMarkerList;

// State shared by every Geonet screen and by the globe's per-frame draw.
typedef struct GeonetAppData {
    enum HeapID heapID;
    WiFiHistory *wiFiHistory;
    Options *options;
    GeonetPlaceMarkerList placeMarkers;
    BgConfig *bgConfig;
    Window messageWindow; // Bottom-screen message box.
    Window listWindow; // Country/region list menu.
    Window exitWindow; // On-screen EXIT banner (touched or B backs out).
    Window locationWindow; // Registered/previewed location summary.
    ListMenu *listMenu;
    StringList *stringList;
    Menu *menu; // Yes/No confirmation.
    MessageLoader *messageLoader;
    int messageState; // Step in Geonet_ShowMessage's printer state machine.
    int printerID; // Printer for the message currently being shown.
    String *messageString;
    StringTemplate *stringTemplate;
    NNSG3dRenderObj globeRenderObj;
    NNSG3dResMdl *globeModel;
    NNSG3dResFileHeader *globeResource;
    NNSG3dRenderObj markerRenderObjs[5]; // Marker models indexed by communicationStatus; index 4 is drawn once per frame.
    NNSG3dResMdl *markerModels[5];
    NNSG3dResFileHeader *markerResources[5];
    VecFx32 globePos;
    VecFx32 globeScale;
    VecFx32 globeRotation; // x = pitch, y = yaw, z = roll, in angle units.
    VecFx32 markerScale;
    Camera *camera;
    CameraAngle cameraAngle;
    u16 zoomedIn; // FALSE = globe is zoomed out, TRUE = zoomed in.
    VecFx32 lightVector;
    int drawState; // 0 = idle, 1 = draw a frame, 2 = reset the 3D engine.
    BOOL unk_BAEC; // Unused; only ever written as FALSE.
    BOOL isJapanese;
    u16 worldUnlocked; // TRUE if the whole globe may be viewed; FALSE locks the view to Japan.
    u16 unk_BAF6; // Unused.
    int touchKeys; // Key input synthesised from the touch screen.
    int touchDragState; // 0 = waiting out the touch delay, 1 = dragging.
    int lastTouchX;
    int lastTouchY;
    int dragX; // Horizontal touch drag amount (0-63).
    int dragY; // Vertical touch drag amount (0-63).
    int touchDelay; // Frames left before a touch press is treated as a drag.
    int country1; // Registered country (Country_Text_*).
    int region1; // Registered region.
    int country2; // Country being selected.
    int region2; // Region being selected.
    BOOL hasInteractedOutsideJapan;
    int showMarkerInfo; // TRUE while the marker-info overlay (X button) is open.
} GeonetAppData;

// A country menu entry: the message ID of its label and the value reported
// when it is chosen.
typedef struct GeonetMenuChoice {
    u32 messageID;
    u32 value;
} GeonetMenuChoice;

BOOL Geonet_CountryHasRegions(int country);
int Geonet_Init(ApplicationManager *appMan, int *unused);
int Geonet_Main(ApplicationManager *appMan, int *state);
int Geonet_Exit(ApplicationManager *appMan, int *unused);
static void Geonet_InitGXBanks(void);
static void Geonet_InitGraphicsModes(void);
static void Geonet_InitGraphics(GeonetAppData *appData, NARC *narc);
static void Geonet_FreeGraphics(GeonetAppData *appData);
static void Geonet_InitPlaceMarkers(GeonetAppData *appData);
static void Geonet_AddPlaceMarker(GeonetAppData *appData, u32 markerIndex, s16 longitude, s16 latitude, u16 country, u16 region);
static void Geonet_MarkRegisteredPlace(GeonetAppData *appData);
static int Geonet_GetPlaceIndexByCountry(int country);
static void Geonet_ProcessTouchInput(GeonetAppData *appData);
static void Geonet_CalcTouchDrag(int prevX, int prevY, int *keysX, int *dragX, int *keysY, int *dragY);
static BOOL Geonet_ShowMessage(GeonetAppData *appData, u32 messageID, int autoAdvance);
static void Geonet_CreateCountryListMenu(GeonetAppData *appData, Window *window, const WindowTemplate *windowTemplate, const ListMenuTemplate *menuTemplate, const GeonetMenuChoice *choices);
static void Geonet_CreateRegionListMenu(GeonetAppData *appData, Window *window, const WindowTemplate *windowTemplate, const ListMenuTemplate *menuTemplate, u32 messageBank, const u8 *regionList, u32 regionCount);
static void Geonet_DestroyListMenu(GeonetAppData *appData);
static void Geonet_DrawRegisteredLocation(GeonetAppData *appData);
static void Geonet_DrawSelectedLocation(GeonetAppData *appData, int country, int region);
static void Geonet_DestroyLocationWindow(GeonetAppData *appData);
static void Geonet_UpdateInfoMessage(GeonetAppData *appData);
static void Geonet_LoadGlobeModels(GeonetAppData *appData, NARC *narc);
static void Geonet_FreeGlobeModels(GeonetAppData *appData);
static void Geonet_InitGlobeTransforms(GeonetAppData *appData);
static void Geonet_InitCamera(GeonetAppData *appData);
static void Geonet_InitLight(GeonetAppData *appData);
static BOOL Geonet_ProcessGlobeInput(GeonetAppData *appData, int pressedKeys, int heldKeys);
static BOOL Geonet_UpdateCameraZoom(GeonetAppData *appData);
static void Geonet_DrawGlobe(GeonetAppData *appData);
static void Geonet_BuildGlobeRotationMatrix(MtxFx33 *matrix, VecFx32 *rotation);
static void Geonet_BuildMarkerRotationMatrix(MtxFx33 *matrix, VecFx32 *angles);
static void Geonet_NormalizeAngle(GeonetAngle *angle);
static u32 Geonet_CalcAngleDistance(const GeonetAngle *angleA, const GeonetAngle *angleB);
void sub_02000EC4(FSOverlayID param0, const ApplicationManagerTemplate *param1);

static const BgTemplate sBgTemplate2 = {
    .x = 0x0,
    .y = 0x0,
    .bufferSize = 0x800,
    .baseTile = 0x0,
    .screenSize = BG_SCREEN_SIZE_256x256,
    .colorMode = GX_BG_COLORMODE_16,
    .screenBase = GX_BG_SCRBASE_0x7000,
    .charBase = GX_BG_CHARBASE_0x00000,
    .bgExtPltt = GX_BG_EXTPLTT_01,
    .priority = 0x0,
    .areaOver = 0x0,
    .mosaic = FALSE,
};

static const BgTemplate sBgTemplate3 = {
    .x = 0x0,
    .y = 0x0,
    .bufferSize = 0x800,
    .baseTile = 0x0,
    .screenSize = BG_SCREEN_SIZE_256x256,
    .colorMode = GX_BG_COLORMODE_16,
    .screenBase = GX_BG_SCRBASE_0x7800,
    .charBase = GX_BG_CHARBASE_0x04000,
    .bgExtPltt = GX_BG_EXTPLTT_01,
    .priority = 0x3,
    .areaOver = 0x0,
    .mosaic = FALSE,
};

// Message box shared by every Geonet prompt.
static const WindowTemplate sMessageWindowTemplate = {
    0x6,
    0x2,
    0x13,
    0x1B,
    0x4,
    0x4,
    0x16D
};

// Yes/No confirmation menu.
static const WindowTemplate sYesNoMenuWindowTemplate = {
    0x6,
    0x19,
    0xD,
    0x6,
    0x4,
    0x4,
    0x155
};

// Country list (the "SEE LIST" screen).
static const WindowTemplate sCountryListWindowTemplate = {
    0x6,
    0x13,
    0xB,
    0xC,
    0x6,
    0x4,
    0x125
};

// Region list (the "REGISTER" screen).
static const WindowTemplate sRegionListWindowTemplate = {
    0x6,
    0x3,
    0x2,
    0x1A,
    0xE,
    0x4,
    0x1
};

// Registered/previewed location summary.
static const WindowTemplate sLocationWindowTemplate = {
    0x6,
    0x2,
    0x1,
    0x1B,
    0x6,
    0x4,
    0xB3
};

// On-screen EXIT banner, positioned so that Geonet_ProcessTouchInput can turn a
// touch inside it into a B press.
static const WindowTemplate sExitWindowTemplate = {
    0x2,
    0x19,
    0x15,
    0x6,
    0x2,
    0x4,
    0x1CD
};

// Top-level country menu choices: SEE LIST (view the globe), REGISTER and EXIT.
static const GeonetMenuChoice sCountryMenuChoices[] = {
    { 0xA, 0x0 },
    { 0xB, 0x1 },
    { 0xC, 0x2 }
};

static const ListMenuTemplate sCountryListMenuTemplate = {
    NULL,
    NULL,
    NULL,
    NULL,
    NELEMS(sCountryMenuChoices),
    NELEMS(sCountryMenuChoices),
    0x0,
    0xC,
    0x0,
    0x0,
    0x1,
    0xF,
    0x2,
    0x0,
    0x10,
    0x0,
    0x0,
    0x0
};

static const ListMenuTemplate sRegionListMenuTemplate = {
    NULL,
    NULL,
    NULL,
    NULL,
    0x0,
    0x7,
    0x0,
    0xC,
    0x0,
    0x0,
    0x1,
    0xF,
    0x2,
    0x0,
    0x10,
    0x1,
    0x0,
    0x0
};

int Geonet_Init(ApplicationManager *appMan, int *unused)
{
    enum HeapID heapID = HEAP_ID_50;

    SetVBlankCallback(NULL, NULL);
    SetHBlankCallback(NULL, NULL);
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();

    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);

    Heap_Create(HEAP_ID_APPLICATION, heapID, 0x80000);

    GeonetAppData *appData = ApplicationManager_NewData(appMan, sizeof(GeonetAppData), heapID);
    memset(appData, 0, sizeof(GeonetAppData));
    appData->heapID = heapID;

    appData->isJapanese = gGameLanguage == LANGUAGE_JAPANESE;

    SaveData *saveData = ApplicationManager_Args(appMan);

    appData->wiFiHistory = SaveData_WiFiHistory(saveData);
    appData->country1 = WiFiHistory_GetCountry(appData->wiFiHistory);
    appData->region1 = WiFiHistory_GetRegion(appData->wiFiHistory);
    appData->hasInteractedOutsideJapan = WiFiHistory_HasInteractedOutsideJapan(appData->wiFiHistory);
    appData->options = SaveData_GetOptions(saveData);

    Geonet_InitGXBanks();
    Geonet_InitGraphicsModes();
    Easy3D_Init(appData->heapID);

    appData->bgConfig = BgConfig_New(appData->heapID);

    GXLayers_TurnBothDispOn();
    Text_ResetAllPrinters();

    appData->stringTemplate = StringTemplate_New(8, 64, appData->heapID);
    appData->camera = Camera_Alloc(appData->heapID);
    appData->drawState = 0;

    // The globe is rendered on the sub screen; swap the displays so that the
    // sub screen's output lands on the main display.
    gSystem.whichScreenIs3D = DS_SCREEN_SUB;

    GXLayers_SwapDisplay();
    SetAutorepeat(4, 8);
    RenderControlFlags_SetCanABSpeedUpPrint(TRUE);
    RenderControlFlags_SetAutoScrollFlags(AUTO_SCROLL_DISABLED);
    RenderControlFlags_SetSpeedUpOnTouch(FALSE);

    Geonet_InitPlaceMarkers(appData);

    return 1;
}

int Geonet_Main(ApplicationManager *appMan, int *state)
{
    GeonetAppData *appData = ApplicationManager_Data(appMan);
    int done = 0;
    NARC *narc;

    switch (*state) {
    case 0:
        // Load the Geonet text and the globe models, then fade in.
        appData->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0356, appData->heapID);
        narc = NARC_ctor(NARC_INDEX_APPLICATION__WIFI_EARTH__WIFI_EARTH, appData->heapID);

        Geonet_LoadGlobeModels(appData, narc);
        Geonet_InitGraphics(appData, narc);
        NARC_dtor(narc);

        appData->unk_BAEC = 0;

        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, COLOR_BLACK, 6, 1, appData->heapID);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG2, 1);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG2, 1);
        GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG3, 1);
        GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG3, 1);
        *state = 1;
        break;
    case 1:
        if (IsScreenFadeDone() == TRUE) {
            *state = 2;
        }
        break;
    case 2:
        // Intro message. If a location is already registered, skip straight to
        // the globe; otherwise offer to register one.
        if (Geonet_ShowMessage(appData, 0, 1) == TRUE) {
            if (appData->country1 == Country_Text_None) {
                *state = 3;
            } else {
                *state = 14;
            }
        }
        break;
    case 3:
        // "What would you like to do?" with the SEE LIST/REGISTER/EXIT menu.
        if (Geonet_ShowMessage(appData, 1, 1) == TRUE) {
            Geonet_CreateCountryListMenu(appData, &appData->listWindow, &sCountryListWindowTemplate, &sCountryListMenuTemplate, sCountryMenuChoices);
            *state = 4;
        }
        break;
    case 4: {
        int selection = ListMenu_ProcessInput(appData->listMenu);

        if (selection == 0xffffffff) {
            break;
        }

        Geonet_DestroyListMenu(appData);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);

        switch (selection) {
        default:
        case 0: // SEE LIST
            *state = 14;
            break;
        case 1: // REGISTER
            *state = 5;
            break;
        case 0xfffffffe: // cancelled
        case 2: // EXIT
            *state = 17;
            break;
        }
    } break;
    case 5:
        if (Geonet_ShowMessage(appData, 2, 1) == 1) {
            appData->menu = Menu_MakeYesNoChoice(appData->bgConfig, &sYesNoMenuWindowTemplate, (512 - (18 + 12)) - 9, 7, appData->heapID);
            *state = 6;
        }
        break;
    case 6: {
        u32 choice = Menu_ProcessInputAndHandleExit(appData->menu, appData->heapID);

        switch (choice) {
        case 0:
            // A Japanese game defaults to Japan and jumps to the region list;
            // every other language picks a country first.
            if (appData->isJapanese == TRUE) {
                appData->country2 = Country_Text_Japan;
                *state = 9;
            } else {
                *state = 7;
            }
            break;
        case 0xfffffffe:
            *state = 3;
            break;
        }
    } break;
    case 7:
        if (Geonet_ShowMessage(appData, 3, 1) == 1) {
            appData->country2 = 0;

            Geonet_CreateRegionListMenu(appData, &appData->listWindow, &sRegionListWindowTemplate, &sRegionListMenuTemplate, 694, WiFiEarthPlace_GetRegionList(0), WiFiEarthPlace_GetRegionCount(0));
            *state = 8;
        }
        break;
    case 8: {
        // The world entry's "regions" are actually countries, so the selection
        // has to be translated back into a Country_Text_* ID.
        int selection = ListMenu_ProcessInput(appData->listMenu);

        if (selection == 0xffffffff) {
            break;
        }

        Geonet_DestroyListMenu(appData);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);

        if (selection != 0xfffffffe) {
            selection = WiFiEarthPlace_GetRegionList(0)[selection];
        }

        switch (selection) {
        default: {
            appData->country2 = selection;

            if (Geonet_CountryHasRegions(appData->country2) == TRUE) {
                *state = 9;
            } else {
                appData->region2 = 0;
                *state = 11;
            }
        } break;
        case 0xfffffffe:

            *state = 3;
            break;
        }
    } break;
    case 9:
        if (Geonet_ShowMessage(appData, 4, 1) == TRUE) {
            appData->region2 = 0;

            {
                u32 placeIndex = WiFiEarthPlace_GetIndexByCountry(appData->country2);
                Geonet_CreateRegionListMenu(appData, &appData->listWindow, &sRegionListWindowTemplate, &sRegionListMenuTemplate, WiFiEarthPlace_GetMessageBank(placeIndex), WiFiEarthPlace_GetRegionList(placeIndex), WiFiEarthPlace_GetRegionCount(placeIndex));
            }
            *state = 10;
        }
        break;
    case 10: {
        int selection = ListMenu_ProcessInput(appData->listMenu);

        if (selection == 0xffffffff) {
            break;
        }

        Geonet_DestroyListMenu(appData);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);

        if (selection != 0xfffffffe) {
            u32 placeIndex = WiFiEarthPlace_GetIndexByCountry(appData->country2);
            selection = WiFiEarthPlace_GetRegionList(placeIndex)[selection];
        }

        switch (selection) {
        default:
            appData->region2 = selection;
            *state = 11;
            break;
        case 0xfffffffe:
            // Cancelling returns to the country picker, or to the main menu for
            // a Japanese game that skipped it.
            if (appData->isJapanese == TRUE) {
                *state = 3;
            } else {
                *state = 7;
            }
        }
    } break;
    case 11:
        Geonet_DrawSelectedLocation(appData, appData->country2, appData->region2);
        *state = 12;
        break;
    case 12:
        if (Geonet_ShowMessage(appData, 5, 1) == 1) {
            appData->menu = Menu_MakeYesNoChoice(appData->bgConfig, &sYesNoMenuWindowTemplate, (512 - (18 + 12)) - 9, 7, appData->heapID);
            *state = 13;
        }
        break;
    case 13: {
        u32 choice = Menu_ProcessInputAndHandleExit(appData->menu, appData->heapID);

        switch (choice) {
        case 0:
            Geonet_DestroyLocationWindow(appData);
            WiFiHistory_SetCountryAndRegion(appData->wiFiHistory, appData->country2, appData->region2);

            appData->country1 = appData->country2;
            appData->region1 = appData->region2;
            *state = 14;
            break;
        case 0xfffffffe:
            Geonet_DestroyLocationWindow(appData);
            *state = 3;
            break;
        }
    } break;
    case 14:
        // Locks the view to Japan for a Japanese game that has never traded
        // outside Japan.
        if (appData->isJapanese == TRUE && !appData->hasInteractedOutsideJapan) {
            appData->worldUnlocked = FALSE;
        } else {
            appData->worldUnlocked = TRUE;
        }

        Geonet_InitGlobeTransforms(appData);
        Geonet_MarkRegisteredPlace(appData);
        Geonet_InitCamera(appData);
        Geonet_InitLight(appData);

        Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 4 * 8);
        Window_DrawStandardFrame(&appData->exitWindow, 0, (512 - (18 + 12)) - 9, 7);

        if (appData->country1 != Country_Text_None) {
            Geonet_DrawRegisteredLocation(appData);
        }

        Geonet_UpdateInfoMessage(appData);

        appData->showMarkerInfo = 0;
        appData->drawState = 1;
        *state = 15;
        break;
    case 15: {
        u16 prevZoomedIn = appData->zoomedIn;
        Geonet_ProcessTouchInput(appData);

        if ((gSystem.pressedKeys & PAD_BUTTON_B) || (appData->touchKeys & PAD_BUTTON_B)) {
            // Back out: if no location was ever registered, return to the menu;
            // otherwise leave the app.
            Window_EraseStandardFrame(&appData->exitWindow, 0);
            Sound_PlayEffect(SEQ_SE_DP_DECIDE_sseq);
            Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 4 * 8);

            if (appData->country1 == Country_Text_None) {
                appData->drawState = 2;
                *state = 3;
            } else {
                Geonet_DestroyLocationWindow(appData);
                *state = 17;
            }
        } else {
            if ((gSystem.pressedKeys & PAD_BUTTON_X) && (appData->showMarkerInfo == 0)) {
                appData->showMarkerInfo = 1;
                Geonet_UpdateInfoMessage(appData);

                if (appData->showMarkerInfo == 1) {
                    Sound_PlayEffect(SEQ_SE_DP_DECIDE_sseq);
                }
                break;
            }

            if ((gSystem.pressedKeys & (PAD_BUTTON_X | PAD_BUTTON_A | PAD_BUTTON_B)) && (appData->showMarkerInfo == 1)) {
                appData->showMarkerInfo = 0;
                Geonet_UpdateInfoMessage(appData);
                break;
            }

            {
                BOOL inputHandled;

                inputHandled = Geonet_ProcessGlobeInput(appData, gSystem.pressedKeys, gSystem.heldKeys);

                if (inputHandled == TRUE && appData->showMarkerInfo == TRUE) {
                    appData->showMarkerInfo = 0;
                    Geonet_UpdateInfoMessage(appData);
                }
            }

            if (prevZoomedIn != appData->zoomedIn) {
                *state = 16;

                if (appData->zoomedIn == 0) {
                    Sound_PlayEffect(SEQ_SE_PL_TIMER03_sseq_1);
                } else {
                    Sound_PlayEffect(SEQ_SE_PL_TIMER03_sseq_1);
                }
            }
        }
    } break;
    case 16: {
        BOOL zoomDone = Geonet_UpdateCameraZoom(appData);

        if (zoomDone == 1) {
            *state = 15;
        }
    } break;
    case 17:
        appData->unk_BAEC = 0;
        StartScreenFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, COLOR_BLACK, 6, 1, appData->heapID);
        *state = 18;
        break;
    case 18:
        if (IsScreenFadeDone() == TRUE) {
            appData->drawState = 1;

            Geonet_FreeGraphics(appData);
            Geonet_FreeGlobeModels(appData);
            MessageLoader_Free(appData->messageLoader);
            *state = 0;
            done = 1;
        }
        break;
    }

    Geonet_DrawGlobe(appData);

    return done;
}

int Geonet_Exit(ApplicationManager *appMan, int *unused)
{
    GeonetAppData *appData = ApplicationManager_Data(appMan);
    enum HeapID heapID = appData->heapID;

    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG2, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG2, 0);
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG3, 0);
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG3, 0);
    Camera_Delete(appData->camera);
    StringTemplate_Free(appData->stringTemplate);
    Easy3D_Shutdown();
    Heap_Free(appData->bgConfig);
    SetVBlankCallback(NULL, NULL);
    ApplicationManager_FreeData(appMan);
    Heap_Destroy(heapID);

    gSystem.whichScreenIs3D = DS_SCREEN_MAIN;

    return 1;
}

static void Geonet_InitGXBanks(void)
{
    GXBanks v0 = {
        GX_VRAM_BG_128_C,
        GX_VRAM_BGEXTPLTT_NONE,
        GX_VRAM_SUB_BG_32_H,
        GX_VRAM_SUB_BGEXTPLTT_NONE,
        GX_VRAM_OBJ_16_F,
        GX_VRAM_OBJEXTPLTT_NONE,
        GX_VRAM_SUB_OBJ_16_I,
        GX_VRAM_SUB_OBJEXTPLTT_NONE,
        GX_VRAM_TEX_01_AB,
        GX_VRAM_TEXPLTT_0123_E
    };

    GXLayers_SetBanks(&v0);
}

static void Geonet_InitGraphicsModes(void)
{
    GraphicsModes v0 = {
        GX_DISPMODE_GRAPHICS,
        GX_BGMODE_0,
        GX_BGMODE_0,
        GX_BG0_AS_3D
    };

    SetAllGraphicsModes(&v0);
}

// Builds the globe's marker list from wifi_earth_place.narc: first one
// country-level marker per entry of member 18, then the finer region markers of
// every country that has its own member.
static void Geonet_InitPlaceMarkers(GeonetAppData *appData)
{
    NARC *narc = NARC_ctor(NARC_INDEX_APPLICATION__WIFI_EARTH__WIFI_EARTH_PLACE, appData->heapID);

    appData->placeMarkers.count = 0;

    {
        WiFiEarthPlaceEntry *placeEntry;
        u32 fileSize;

        void *member = LoadMemberFromOpenNARC_OutFileSize(narc, 18, 0, appData->heapID, 0, &fileSize);
        placeEntry = (WiFiEarthPlaceEntry *)member;
        int entryCount = fileSize / 6;

        placeEntry++; // Skip the header entry.

        for (int i = 1; i < entryCount; i++) {
            // type 2 marks a country whose markers come from its own region
            // list, so it is skipped here.
            if (placeEntry->type != 2) {
                Geonet_AddPlaceMarker(appData, appData->placeMarkers.count, placeEntry->longitude, placeEntry->latitude, i, 0);
                appData->placeMarkers.count++;
            }

            placeEntry++;
        }

        Heap_Free(member);
    }
    {
        void *member;
        WiFiEarthRegionEntry *regionEntry;
        u32 fileSize, narcMemberIndex;
        int placeCount, entryCount;

        int placeIndex = 1;
        placeCount = WiFiEarthPlace_GetCount();

        while (placeIndex < placeCount) {
            narcMemberIndex = WiFiEarthPlace_GetNarcMemberIndex(placeIndex);
            member = LoadMemberFromOpenNARC_OutFileSize(narc, narcMemberIndex, 0, appData->heapID, 0, &fileSize);
            regionEntry = (WiFiEarthRegionEntry *)member;
            entryCount = fileSize / 4;

            regionEntry++; // Skip the header entry.

            for (int i = 1; i < entryCount; i++) {
                Geonet_AddPlaceMarker(appData, appData->placeMarkers.count, regionEntry->longitude, regionEntry->latitude, WiFiEarthPlace_GetCountry(placeIndex), i);
                appData->placeMarkers.count++;
                regionEntry++;
            }

            Heap_Free(member);
            placeIndex++;
        }
    }

    NARC_dtor(narc);
}

// Adds one marker, precomputing the rotation that stands it up on the globe.
static void Geonet_AddPlaceMarker(GeonetAppData *appData, u32 markerIndex, s16 longitude, s16 latitude, u16 country, u16 region)
{
    MtxFx33 rotation = { FX32_ONE, 0, 0, 0, FX32_ONE, 0, 0, 0, FX32_ONE };
    VecFx32 angles;

    appData->placeMarkers.markers[markerIndex].longitude = longitude;
    appData->placeMarkers.markers[markerIndex].latitude = latitude;

    angles.x = longitude;
    angles.y = latitude;
    angles.z = 0;

    Geonet_BuildMarkerRotationMatrix(&rotation, &angles);

    appData->placeMarkers.markers[markerIndex].rotation = rotation;
    appData->placeMarkers.markers[markerIndex].communicationStatus = WiFiHistory_GetGeonetCommunicatedWith(appData->wiFiHistory, country, region);
    appData->placeMarkers.markers[markerIndex].country = country;
    appData->placeMarkers.markers[markerIndex].region = region;
}

// Highlights the player's registered location and points the globe at it.
static void Geonet_MarkRegisteredPlace(GeonetAppData *appData)
{
    for (int i = 0; i < appData->placeMarkers.count; i++) {
        if (appData->placeMarkers.markers[i].country == appData->country1 && appData->placeMarkers.markers[i].region == appData->region1) {
            appData->placeMarkers.markers[i].communicationStatus = 3;
            appData->globeRotation.x = appData->placeMarkers.markers[i].longitude;
            appData->globeRotation.y = appData->placeMarkers.markers[i].latitude;
        }
    }
}

// Returns the wifi_earth_place table index for a country, or 0 (the world
// entry) if the country is not listed.
static int Geonet_GetPlaceIndexByCountry(int country)
{
    return WiFiEarthPlace_GetIndexByCountry(country);
}

// Reads the touch screen into synthesised key input. Touch on the EXIT banner
// becomes a B press; a sustained drag becomes a direction press plus a drag
// amount (0-63) in each axis.
static void Geonet_ProcessTouchInput(GeonetAppData *appData)
{
    int keysX, dragX, keysY, dragY;

    appData->touchKeys = 0;

    if (gSystem.touchPressed) {
        if ((gSystem.touchX >= (25 * 8)) && (gSystem.touchX <= ((25 + 6) * 8)) && (gSystem.touchY >= (21 * 8)) && (gSystem.touchY <= ((21 + 2) * 8))) {
            appData->touchKeys = PAD_BUTTON_B;
            return;
        }

        appData->touchDragState = 0;
        appData->dragX = 0;
        appData->dragY = 0;
        appData->touchDelay = 0;
        appData->touchKeys = 0;
        appData->lastTouchX = gSystem.touchX;
        appData->lastTouchY = gSystem.touchY;
        appData->touchDelay = 4;
    }

    if (gSystem.touchHeld) {
        switch (appData->touchDragState) {
        case 0:
            // Wait out the delay before treating the touch as a drag; then fall
            // through to start dragging.
            if (!appData->touchDelay) {
                appData->touchDragState++;
            } else {
                appData->touchDelay--;
            }
        case 1:
            Geonet_CalcTouchDrag(appData->lastTouchX, appData->lastTouchY, &keysX, &dragX, &keysY, &dragY);
            appData->touchKeys = keysX | keysY;
            appData->dragX = dragX;
            appData->dragY = dragY;
            appData->lastTouchX = gSystem.touchX;
            appData->lastTouchY = gSystem.touchY;
            break;
        }
    } else {
        // A tap that never became a drag counts as an A press.
        if (appData->touchDelay) {
            appData->touchKeys = PAD_BUTTON_A;
        }

        appData->touchDragState = 0;
        appData->dragX = 0;
        appData->dragY = 0;
        appData->touchDelay = 0;
    }
}

// Compares the current touch position with the previous one and reports the
// drag direction and distance (0-63) for each axis.
static void Geonet_CalcTouchDrag(int prevX, int prevY, int *keysX, int *dragX, int *keysY, int *dragY)
{
    int keyX = 0;
    int keyY = 0;
    int deltaX = 0;
    int deltaY = 0;

    if (gSystem.touchX != 0xffff) {
        deltaX = gSystem.touchX - prevX;

        if (deltaX < 0) {
            deltaX ^= -1;
            keyX = PAD_KEY_RIGHT;
        } else {
            if (deltaX > 0) {
                keyX = PAD_KEY_LEFT;
            }
        }
    }

    deltaX &= 0x3f;
    *keysX = keyX;
    *dragX = deltaX;

    if (gSystem.touchY != 0xffff) {
        deltaY = gSystem.touchY - prevY;

        if (deltaY < 0) {
            deltaY ^= -1;
            keyY = PAD_KEY_DOWN;
        } else {
            if (deltaY > 0) {
                keyY = PAD_KEY_UP;
            }
        }
    }

    deltaY &= 0x3f;
    *keysY = keyY;
    *dragY = deltaY;
}

static void Geonet_InitGraphics(GeonetAppData *appData, NARC *narc)
{
    Bg_InitFromTemplate(appData->bgConfig, BG_LAYER_SUB_2, &sBgTemplate2, 0);
    Bg_ClearTilemap(appData->bgConfig, BG_LAYER_SUB_2);
    Bg_InitFromTemplate(appData->bgConfig, BG_LAYER_SUB_3, &sBgTemplate3, 0);
    Graphics_LoadTilesToBgLayerFromOpenNARC(narc, 5, appData->bgConfig, 7, 0, 0, 0, appData->heapID);
    Graphics_LoadPaletteFromOpenNARC(narc, 6, 4, 0 * (2 * 16), (2 * 16) * 4, appData->heapID);
    Graphics_LoadTilemapToBgLayerFromOpenNARC(narc, 7, appData->bgConfig, 7, 0, 0, 0, appData->heapID);
    LoadMessageBoxGraphics(appData->bgConfig, BG_LAYER_SUB_2, 512 - (18 + 12), 6, Options_Frame(appData->options), appData->heapID);
    LoadStandardWindowGraphics(appData->bgConfig, BG_LAYER_SUB_2, (512 - (18 + 12)) - 9, 7, 0, appData->heapID);
    Font_LoadTextPalette(PAL_LOAD_SUB_BG, PLTT_OFFSET(4), appData->heapID);
    Bg_ClearTilesRange(6, 32, 0, appData->heapID);
    Bg_MaskPalette(BG_LAYER_SUB_2, 0x4753);
    Window_AddFromTemplate(appData->bgConfig, &appData->messageWindow, &sMessageWindowTemplate);
    Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 4 * 8);
    Window_DrawMessageBoxWithScrollCursor(&appData->messageWindow, 0, 512 - (18 + 12), 6);

    appData->messageState = 0;

    Bg_InitFromTemplate(appData->bgConfig, BG_LAYER_MAIN_2, &sBgTemplate2, 0);
    Bg_ClearTilemap(appData->bgConfig, BG_LAYER_MAIN_2);
    Bg_InitFromTemplate(appData->bgConfig, BG_LAYER_MAIN_3, &sBgTemplate3, 0);
    Graphics_LoadTilesToBgLayerFromOpenNARC(narc, 5, appData->bgConfig, 3, 0, 0, 0, appData->heapID);
    Graphics_LoadPaletteFromOpenNARC(narc, 6, 0, 0 * (2 * 16), (2 * 16) * 4, appData->heapID);
    Graphics_LoadTilemapToBgLayerFromOpenNARC(narc, 7, appData->bgConfig, 3, 0, 0, 0, appData->heapID);
    LoadStandardWindowGraphics(appData->bgConfig, BG_LAYER_MAIN_2, (512 - (18 + 12)) - 9, 7, 0, appData->heapID);
    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(4), appData->heapID);
    Bg_ClearTilesRange(BG_LAYER_MAIN_2, 32, 0, appData->heapID);
    Bg_MaskPalette(BG_LAYER_MAIN_2, 0x0);

    {
        String *string = String_Init(16, appData->heapID);
        Font_InitManager(FONT_SUBSCREEN, appData->heapID);

        {
            u16 color1 = 0x4e56;
            u16 color2 = 0x3571;
            u16 color3 = 0x208c;
            u16 color4 = 0x7fff;

            // Recolour a few window-frame palette entries for the Geonet
            // screens.
            Bg_LoadPalette(BG_LAYER_MAIN_2, &color1, 2, 4 * (2 * 16) + 1 * 2);
            Bg_LoadPalette(BG_LAYER_MAIN_2, &color2, 2, 4 * (2 * 16) + 2 * 2);
            Bg_LoadPalette(BG_LAYER_MAIN_2, &color3, 2, 4 * (2 * 16) + 3 * 2);
            Bg_LoadPalette(BG_LAYER_MAIN_2, &color4, 2, 4 * (2 * 16) + 15 * 2);
        }

        Window_AddFromTemplate(appData->bgConfig, &appData->exitWindow, &sExitWindowTemplate);
        Window_FillRectWithColor(&appData->exitWindow, 15, 0, 0, 27 * 8, 4 * 8);
        MessageLoader_GetString(appData->messageLoader, 12, string);

        {
            u32 x;

            x = Font_CalcCenterAlignment(FONT_SUBSCREEN, string, 0, 6 * 8);
            Text_AddPrinterWithParams(&appData->exitWindow, FONT_SUBSCREEN, string, x, 0, TEXT_SPEED_NO_TRANSFER, NULL);
        }

        String_Free(string);
        Font_Free(FONT_SUBSCREEN);
    }
}

static void Geonet_FreeGraphics(GeonetAppData *appData)
{
    Window_Remove(&appData->exitWindow);
    Window_Remove(&appData->messageWindow);
    Bg_FreeTilemapBuffer(appData->bgConfig, BG_LAYER_MAIN_2);
    Bg_FreeTilemapBuffer(appData->bgConfig, BG_LAYER_SUB_2);
    Bg_FreeTilemapBuffer(appData->bgConfig, BG_LAYER_MAIN_3);
    Bg_FreeTilemapBuffer(appData->bgConfig, BG_LAYER_SUB_3);
}

// Prints a message and waits for it to finish. `autoAdvance` makes it advance
// as soon as the printer is done rather than waiting for an A press. Returns
// TRUE once the player has advanced past the message.
static BOOL Geonet_ShowMessage(GeonetAppData *appData, u32 messageID, int autoAdvance)
{
    BOOL ready = 0;

    switch (appData->messageState) {
    case 0:
        Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 4 * 8);
        appData->messageString = String_Init(0x400, appData->heapID);
        MessageLoader_GetString(appData->messageLoader, messageID, appData->messageString);
        appData->printerID = Text_AddPrinterWithParams(&appData->messageWindow, FONT_MESSAGE, appData->messageString, 0, 0, Options_TextFrameDelay(appData->options), NULL);
        appData->messageState = 1;
        break;
    case 1:
        if (!(Text_IsPrinterActive(appData->printerID))) {
            String_Free(appData->messageString);
            appData->messageState = 2;
        }
        break;
    case 2:
        if ((autoAdvance != 0) || (gSystem.pressedKeys & PAD_BUTTON_A)) {
            appData->messageState = 0;
            ready = 1;
        }
    }

    return ready;
}

static void Geonet_ListMenuCursorCallback(ListMenu *listMenu, u32 unused, u8 cursorMovement)
{
    if (cursorMovement == 0) {
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
    }
}

static void Geonet_CreateCountryListMenu(GeonetAppData *appData, Window *window, const WindowTemplate *windowTemplate, const ListMenuTemplate *menuTemplate, const GeonetMenuChoice *choices)
{
    ListMenuTemplate template;
    int i;

    Window_AddFromTemplate(appData->bgConfig, window, windowTemplate);
    appData->stringList = StringList_New(menuTemplate->count, appData->heapID);

    for (i = 0; i < menuTemplate->count; i++) {
        StringList_AddFromMessageBank(appData->stringList, appData->messageLoader, choices[i].messageID, choices[i].value);
    }

    template = *menuTemplate;
    template.choices = appData->stringList;
    template.window = window;
    template.cursorCallback = Geonet_ListMenuCursorCallback;
    appData->listMenu = ListMenu_New(&template, 0, 0, appData->heapID);

    Window_DrawStandardFrame(template.window, 1, (512 - (18 + 12)) - 9, 7);
    Window_CopyToVRAM(window);
}

static void Geonet_CreateRegionListMenu(GeonetAppData *appData, Window *window, const WindowTemplate *windowTemplate, const ListMenuTemplate *menuTemplate, u32 messageBank, const u8 *regionList, u32 regionCount)
{
    ListMenuTemplate template;
    MessageLoader *regionMessageLoader;
    int i;

    Window_AddFromTemplate(appData->bgConfig, window, windowTemplate);
    regionMessageLoader = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, messageBank, appData->heapID);
    appData->stringList = StringList_New(regionCount, appData->heapID);

    for (i = 0; i < regionCount; i++) {
        StringList_AddFromMessageBank(appData->stringList, regionMessageLoader, regionList[i], i);
    }

    MessageLoader_Free(regionMessageLoader);

    template = *menuTemplate;
    template.choices = appData->stringList;
    template.count = regionCount;
    template.window = window;
    template.cursorCallback = Geonet_ListMenuCursorCallback;

    appData->listMenu = ListMenu_New(&template, 0, 0, appData->heapID);

    Window_DrawStandardFrame(template.window, 1, (512 - (18 + 12)) - 9, 7);
    Window_CopyToVRAM(window);
}

static void Geonet_DestroyListMenu(GeonetAppData *appData)
{
    Window_EraseStandardFrame(&appData->listWindow, 0);
    Window_Remove(&appData->listWindow);
    ListMenu_Free(appData->listMenu, NULL, NULL);
    StringList_Free(appData->stringList);
}

// Shows the player's already-registered country and region.
static void Geonet_DrawRegisteredLocation(GeonetAppData *appData)
{
    String *formatted = String_Init(0x400, appData->heapID);
    String *format = String_Init(0x400, appData->heapID);

    Window_AddFromTemplate(appData->bgConfig, &appData->locationWindow, &sLocationWindowTemplate);
    Window_FillRectWithColor(&appData->locationWindow, 15, 0, 0, 27 * 8, 6 * 8);
    Window_DrawStandardFrame(&appData->locationWindow, 0, (512 - (18 + 12)) - 9, 7);

    StringTemplate_SetCountryName(appData->stringTemplate, 0, appData->country1);
    StringTemplate_SetCityName(appData->stringTemplate, 1, appData->country1, appData->region1);

    MessageLoader_GetString(appData->messageLoader, 13, format);
    StringTemplate_Format(appData->stringTemplate, formatted, format);

    Text_AddPrinterWithParams(&appData->locationWindow, FONT_SYSTEM, formatted, 0, 0, TEXT_SPEED_INSTANT, NULL);

    String_Free(format);
    String_Free(formatted);

    Window_CopyToVRAM(&appData->locationWindow);
}

// Shows a country/region pair that has been picked but not yet saved.
static void Geonet_DrawSelectedLocation(GeonetAppData *appData, int country, int region)
{
    String *countryName = String_Init(64, appData->heapID);
    String *regionName = String_Init(64, appData->heapID);

    Window_AddFromTemplate(appData->bgConfig, &appData->locationWindow, &sLocationWindowTemplate);
    Window_FillRectWithColor(&appData->locationWindow, 15, 0, 0, 27 * 8, 6 * 8);
    Window_DrawStandardFrame(&appData->locationWindow, 0, (512 - (18 + 12)) - 9, 7);

    Geonet_GetCountryAndRegionNames(country, region, countryName, regionName, appData->heapID);

    if (region != 0) {
        Text_AddPrinterWithParams(&appData->locationWindow, FONT_SYSTEM, regionName, 0, 16, TEXT_SPEED_NO_TRANSFER, NULL);
    }

    Text_AddPrinterWithParams(&appData->locationWindow, FONT_SYSTEM, countryName, 0, 0, TEXT_SPEED_INSTANT, NULL);
    String_Free(regionName);
    String_Free(countryName);
    Window_CopyToVRAM(&appData->locationWindow);
}

static void Geonet_DestroyLocationWindow(GeonetAppData *appData)
{
    Window_EraseStandardFrame(&appData->locationWindow, 0);
    Window_Remove(&appData->locationWindow);
}

// Refreshes the bottom-screen hint. With showMarkerInfo off it shows the
// "press X" hint; with it on it finds the communicated-with marker nearest the
// centre of the view and shows its name, pointing the globe at it.
static void Geonet_UpdateInfoMessage(GeonetAppData *appData)
{
    if (appData->showMarkerInfo == 0) {
        {
            String *string = String_Init(0x400, appData->heapID);

            Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 6 * 8);
            MessageLoader_GetString(appData->messageLoader, 14, string);
            Text_AddPrinterWithParams(&appData->messageWindow, FONT_MESSAGE, string, 0, 0, TEXT_SPEED_INSTANT, NULL);
            String_Free(string);
        }
    } else {
        {
            int i;
            BOOL found = 0;
            s16 minLongitude = (s16)(appData->globeRotation.x - 0x80);
            s16 maxLongitude = (s16)(appData->globeRotation.x + 0x80);
            s16 minLatitude = (s16)(appData->globeRotation.y - 0x80);
            s16 maxLatitude = (s16)(appData->globeRotation.y + 0x80);
            u32 bestDistance = 0x80 * 2;
            u32 bestIndex = appData->placeMarkers.count;
            u32 distance;
            GeonetAngle globeAngle, markerAngle;

            globeAngle.x = appData->globeRotation.x;
            globeAngle.y = appData->globeRotation.y;

            Geonet_NormalizeAngle(&globeAngle);

            for (i = 0; i < appData->placeMarkers.count; i++) {
                if ((appData->placeMarkers.markers[i].longitude > minLongitude) && (appData->placeMarkers.markers[i].longitude < maxLongitude) && (appData->placeMarkers.markers[i].latitude > minLatitude) && (appData->placeMarkers.markers[i].latitude < maxLatitude) && (appData->placeMarkers.markers[i].communicationStatus != 0)) {
                    markerAngle.x = appData->placeMarkers.markers[i].longitude;
                    markerAngle.y = appData->placeMarkers.markers[i].latitude;

                    Geonet_NormalizeAngle(&markerAngle);

                    distance = Geonet_CalcAngleDistance(&globeAngle, &markerAngle);

                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestIndex = i;
                    }
                }
            }

            if (bestIndex != appData->placeMarkers.count) {
                String *countryName = String_Init(64, appData->heapID);
                String *regionName = String_Init(64, appData->heapID);

                Window_FillRectWithColor(&appData->messageWindow, 15, 0, 0, 27 * 8, 6 * 8);
                Geonet_GetCountryAndRegionNames(appData->placeMarkers.markers[bestIndex].country, appData->placeMarkers.markers[bestIndex].region, countryName, regionName, appData->heapID);

                if (appData->placeMarkers.markers[bestIndex].region != 0) {
                    Text_AddPrinterWithParams(&appData->messageWindow, FONT_MESSAGE, regionName, 0, 16, TEXT_SPEED_NO_TRANSFER, NULL);
                }

                Text_AddPrinterWithParams(&appData->messageWindow, FONT_MESSAGE, countryName, 0, 0, TEXT_SPEED_INSTANT, NULL);
                String_Free(regionName);
                String_Free(countryName);

                appData->globeRotation.x = appData->placeMarkers.markers[bestIndex].longitude;
                appData->globeRotation.y = appData->placeMarkers.markers[bestIndex].latitude;

                found = 1;
            }

            if (found == 0) {
                appData->showMarkerInfo = 0;
            }
        }
    }
}

// Loads the globe model (member 0) and its four marker models. Member 2 goes
// to index 4, which is drawn once per frame; members 1, 3 and 4 become the
// models for communication statuses 3, 1 and 2 respectively.
static void Geonet_LoadGlobeModels(GeonetAppData *appData, NARC *narc)
{
    appData->globeResource = NARC_AllocAndReadWholeMember(narc, 0, appData->heapID);
    Easy3D_InitRenderObjFromResource(&appData->globeRenderObj, &appData->globeModel, &appData->globeResource);

    appData->markerResources[3] = NARC_AllocAndReadWholeMember(narc, 1, appData->heapID);
    Easy3D_InitRenderObjFromResource(&appData->markerRenderObjs[3], &appData->markerModels[3], &appData->markerResources[3]);

    appData->markerResources[4] = NARC_AllocAndReadWholeMember(narc, 2, appData->heapID);
    Easy3D_InitRenderObjFromResource(&appData->markerRenderObjs[4], &appData->markerModels[4], &appData->markerResources[4]);

    appData->markerResources[1] = NARC_AllocAndReadWholeMember(narc, 3, appData->heapID);
    Easy3D_InitRenderObjFromResource(&appData->markerRenderObjs[1], &appData->markerModels[1], &appData->markerResources[1]);

    appData->markerResources[2] = NARC_AllocAndReadWholeMember(narc, 4, appData->heapID);
    Easy3D_InitRenderObjFromResource(&appData->markerRenderObjs[2], &appData->markerModels[2], &appData->markerResources[2]);
}

static void Geonet_FreeGlobeModels(GeonetAppData *appData)
{
    Heap_Free(appData->markerResources[2]);
    Heap_Free(appData->markerResources[1]);
    Heap_Free(appData->markerResources[4]);
    Heap_Free(appData->markerResources[3]);
    Heap_Free(appData->globeResource);
}

static void Geonet_InitGlobeTransforms(GeonetAppData *appData)
{
    {
        appData->globePos.x = 0;
        appData->globePos.y = 0;
        appData->globePos.z = 0;
    }
    {
        appData->globeScale.x = (FX32_ONE);
        appData->globeScale.y = (FX32_ONE);
        appData->globeScale.z = (FX32_ONE);
    }
    {
        appData->globeRotation.x = 0x1A40;
        appData->globeRotation.y = 0x7C00;
        appData->globeRotation.z = 0;
    }
    {
        appData->markerScale.x = (FX32_ONE);
        appData->markerScale.y = (FX32_ONE);
        appData->markerScale.z = (FX32_ONE);
    }
}

static void Geonet_InitCamera(GeonetAppData *appData)
{
    VecFx32 target = { 0, 0, 0 };
    VecFx32 position = { 0, 0, 0x128000 };

    Camera_InitWithTargetAndPosition(&target, &position, 0x5c1, 0, 0, appData->camera);
    Camera_SetClipping(0, FX32_ONE * 100, appData->camera);
    Camera_ComputeProjectionMatrix(0, appData->camera);
    Camera_SetAsActive(appData->camera);

    if (appData->worldUnlocked == 0) {
        appData->zoomedIn = 1;
    } else {
        appData->zoomedIn = 0;
    }

    // Snap the camera straight to its resting distance for the initial zoom
    // state.
    while (TRUE) {
        if (Geonet_UpdateCameraZoom(appData) == 1) {
            break;
        }
    }
}

static void Geonet_InitLight(GeonetAppData *appData)
{
    appData->lightVector.x = 0;
    appData->lightVector.y = 0;
    appData->lightVector.z = (-(FX32_ONE - 1));

    NNS_G3dGlbLightVector(0, appData->lightVector.x, appData->lightVector.y, appData->lightVector.z);
}

// Builds the globe's spin matrix: yaw (x) around Y, pitch (y) around X, then
// roll (z) around Z.
static void Geonet_BuildGlobeRotationMatrix(MtxFx33 *matrix, VecFx32 *rotation)
{
    MtxFx33 temp;

    MTX_RotY33(matrix, FX_SinIdx((u16)rotation->y), FX_CosIdx((u16)rotation->y));
    MTX_RotX33(&temp, FX_SinIdx((u16)rotation->x), FX_CosIdx((u16)rotation->x));
    MTX_Concat33(matrix, &temp, matrix);
    MTX_RotZ33(&temp, FX_SinIdx((u16)rotation->z), FX_CosIdx((u16)rotation->z));
    MTX_Concat33(matrix, &temp, matrix);
}

// Builds a marker's orientation so that it sits upright on the globe at
// (x = longitude, y = latitude).
static void Geonet_BuildMarkerRotationMatrix(MtxFx33 *matrix, VecFx32 *angles)
{
    MtxFx33 temp;

    MTX_RotY33(matrix, FX_SinIdx((u16)angles->x), FX_CosIdx((u16)angles->x));
    MTX_RotX33(&temp, FX_SinIdx((u16)-angles->y), FX_CosIdx((u16)-angles->y));
    MTX_Concat33(matrix, &temp, matrix);
    MTX_RotZ33(&temp, FX_CosIdx((u16)angles->z), FX_SinIdx((u16)angles->z));
    MTX_Concat33(matrix, &temp, matrix);
}

// Applies rotation and zoom input to the globe. A, and a tap on the touch
// screen, toggle the zoom level when the world view is unlocked. The rotation
// limits are wider when the world view is unlocked; otherwise the view stays
// centred on Japan.
static BOOL Geonet_ProcessGlobeInput(GeonetAppData *appData, int pressedKeys, int heldKeys)
{
    u16 yawStep;
    u16 pitchStep;
    s16 longitude;
    s16 latitude;
    BOOL handled = 0;

    longitude = appData->globeRotation.x;
    latitude = appData->globeRotation.y;

    if ((pressedKeys & PAD_BUTTON_A) || (appData->touchKeys & PAD_BUTTON_A)) {
        if (appData->worldUnlocked == 1) {
            if (appData->zoomedIn == 0) {
                appData->zoomedIn = 1;
            } else {
                appData->zoomedIn = 0;
            }
        }

        handled = 1;
        return handled;
    }

    if (appData->zoomedIn == 0) {
        if ((appData->dragX) || (appData->dragY)) {
            yawStep = 0x200 / 6 * appData->dragX;
            pitchStep = 0x200 / 6 * appData->dragY;
        } else {
            yawStep = 0x200;
            pitchStep = 0x200;
        }
    } else {
        if ((appData->dragX) || (appData->dragY)) {
            yawStep = 0x20 / 3 * appData->dragX;
            pitchStep = 0x20 / 3 * appData->dragY;
        } else {
            yawStep = 0x20;
            pitchStep = 0x20;
        }
    }

    if ((heldKeys & PAD_KEY_LEFT) || (appData->touchKeys & PAD_KEY_LEFT)) {
        if (appData->worldUnlocked == 1) {
            appData->globeRotation.y += yawStep;
        } else {
            if (latitude < (s16)0xd820) {
                appData->globeRotation.y += yawStep;
            }
        }

        handled = 1;
    }

    if ((heldKeys & PAD_KEY_RIGHT) || (appData->touchKeys & PAD_KEY_RIGHT)) {
        if (appData->worldUnlocked == 1) {
            appData->globeRotation.y -= yawStep;
        } else {
            if (latitude > (s16)0xcc80) {
                appData->globeRotation.y -= yawStep;
            }
        }

        handled = 1;
    }

    if ((heldKeys & PAD_KEY_UP) || (appData->touchKeys & PAD_KEY_UP)) {
        if (appData->worldUnlocked == 1) {
            if ((longitude + pitchStep) < (0x4000 - 0x200)) {
                appData->globeRotation.x += pitchStep;
            } else {
                appData->globeRotation.x = (0x4000 - 0x200);
            }
        } else {
            if (longitude < (s16)0x2020) {
                appData->globeRotation.x += pitchStep;
            }
        }

        handled = 1;
    }

    if ((heldKeys & PAD_KEY_DOWN) || (appData->touchKeys & PAD_KEY_DOWN)) {
        if (appData->worldUnlocked == 1) {
            if ((longitude - pitchStep) > (-0x4000 + 0x200)) {
                appData->globeRotation.x -= pitchStep;
            } else {
                appData->globeRotation.x = (-0x4000 + 0x200);
            }
        } else {
            if (longitude > (s16)0x1300) {
                appData->globeRotation.x -= pitchStep;
            }
        }

        handled = 1;
    }

    return handled;
}

// Steps the camera between its near (0x50000) and far (0x128000) distances.
// The marker scale follows so that markers keep a constant on-screen size.
// Returns TRUE once the target distance is reached.
static BOOL Geonet_UpdateCameraZoom(GeonetAppData *appData)
{
    fx32 distance = Camera_GetDistance(appData->camera);
    BOOL atTarget = 0;

    switch (appData->zoomedIn) {
    case 1:
        if (distance > (0x50000 + 0x8000)) {
            distance -= 0x8000;
            appData->markerScale.x -= 0x80;
            appData->markerScale.y = appData->markerScale.x;
        } else {
            distance = 0x50000;
            atTarget = 1;
        }
        break;
    case 0:
        if (distance < (0x128000 - 0x8000)) {
            distance += 0x8000;
            appData->markerScale.x += 0x80;
            appData->markerScale.y = appData->markerScale.x;
        } else {
            distance = 0x128000;
            atTarget = 1;
        }
        break;
    }

    Camera_SetDistance(distance, appData->camera);

    return atTarget;
}

// Draws one frame of the globe: the sphere, then the extra model at index 4,
// then every marker whose communication status is non-zero.
static void Geonet_DrawGlobe(GeonetAppData *appData)
{
    MtxFx33 globeRotation = { FX32_ONE, 0, 0, 0, FX32_ONE, 0, 0, 0, FX32_ONE };

    switch (appData->drawState) {
    case 0:
        break;
    case 2:
        G3_ResetG3X();
        G3_RequestSwapBuffers(GX_SORTMODE_AUTO, GX_BUFFERMODE_W);
        appData->drawState = 0;
        break;
    case 1:
        G3_ResetG3X();
        Camera_ComputeViewMatrix();

        {
            Geonet_BuildGlobeRotationMatrix(&globeRotation, &appData->globeRotation);
            Easy3D_DrawRenderObj(&appData->globeRenderObj, &appData->globePos, &globeRotation, &appData->globeScale);

            {
                MtxFx33 identity = { FX32_ONE, 0, 0, 0, FX32_ONE, 0, 0, 0, FX32_ONE };
                Easy3D_DrawRenderObj(&appData->markerRenderObjs[4], &appData->globePos, &identity, &appData->markerScale);
            }

            {
                MtxFx33 markerRotation = { FX32_ONE, 0, 0, 0, FX32_ONE, 0, 0, 0, FX32_ONE };
                int i;

                for (i = 0; i < appData->placeMarkers.count; i++) {
                    MTX_Concat33(&appData->placeMarkers.markers[i].rotation, &globeRotation, &markerRotation);

                    if (appData->placeMarkers.markers[i].communicationStatus != 0) {
                        Easy3D_DrawRenderObj(&appData->markerRenderObjs[appData->placeMarkers.markers[i].communicationStatus], &appData->globePos, &markerRotation, &appData->markerScale);
                    }
                }
            }
        }

        G3_RequestSwapBuffers(GX_SORTMODE_AUTO, GX_BUFFERMODE_W);
        break;
    }
}

// Fills countryName and regionName for a country/region pair. Returns FALSE if
// the country has no region list, in which case regionName is set to "None".
BOOL Geonet_GetCountryAndRegionNames(int country, int region, String *countryName, String *regionName, enum HeapID heapID)
{
    MessageLoader *messageLoader;
    int placeIndex = Geonet_GetPlaceIndexByCountry(country);
    BOOL hasRegion;

    messageLoader = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_COUNTRY_NAMES, heapID);

    MessageLoader_GetString(messageLoader, country, countryName);
    MessageLoader_Free(messageLoader);

    if (placeIndex == 0) {
        placeIndex = 1;
        region = 0;
        hasRegion = 0;
    } else {
        hasRegion = 1;
    }

    messageLoader = MessageLoader_Init(MSG_LOADER_PRELOAD_ENTIRE_BANK, NARC_INDEX_MSGDATA__PL_MSG, WiFiEarthPlace_GetMessageBank(placeIndex), heapID);

    MessageLoader_GetString(messageLoader, region, regionName);
    MessageLoader_Free(messageLoader);

    return hasRegion;
}

BOOL Geonet_CountryHasRegions(int country)
{
    if (Geonet_GetPlaceIndexByCountry(country)) {
        return 1;
    }

    return 0;
}

// Wraps a pair of angles into [0, 0xFFFF).
static void Geonet_NormalizeAngle(GeonetAngle *angle)
{
    if (angle->x >= 0) {
        angle->x = angle->x % 0xffff;
    } else {
        angle->x = angle->x + (0xffff * ((MATH_ABS(angle->x) / 0xffff) + 1));
    }

    if (angle->y >= 0) {
        angle->y = angle->y % 0xffff;
    } else {
        angle->y = angle->y + (0xffff * ((MATH_ABS(angle->y) / 0xffff) + 1));
    }
}

// Returns the angular distance between two angle pairs, taking the shorter way
// around the circle on each axis.
static u32 Geonet_CalcAngleDistance(const GeonetAngle *angleA, const GeonetAngle *angleB)
{
    s32 dx, dy;
    u32 distance;

    dx = MATH_ABS(angleA->x - angleB->x);
    dy = MATH_ABS(angleA->y - angleB->y);

    if (dx > CalcAngleRotationIdx(180)) {
        dx = 0xffff - dx;
    }

    if (dy > CalcAngleRotationIdx(180)) {
        dy = 0xffff - dy;
    }

    distance = FX_Sqrt(((dx * dx) + (dy * dy)) << FX32_SHIFT) >> FX32_SHIFT;

    return distance;
}
