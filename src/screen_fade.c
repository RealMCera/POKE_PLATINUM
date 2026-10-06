#include "screen_fade.h"

#include <string.h>

#include "constants/graphics.h"
#include "constants/heap.h"

#include "brightness_controller.h"
#include "enums.h"
#include "hardware_window.h"
#include "heap.h"
#include "screen_fade_funcs.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "system.h"

#define LOCAL_TASK_PRIORIITY 1024

enum ScreenFadeOrder {
    ORDER_SIMULTANEOUS,
    ORDER_MAIN_FIRST,
    ORDER_SUB_FIRST
};

typedef struct EnableHBlankTemplate {
    ScreenFadeHBlanks *hblanks;
    void *data;
    Callback callback;
    enum DSScreen screen;
} EnableHBlankTemplate;

typedef struct DisableHBlankTemplate {
    ScreenFadeHBlanks *hblanks;
    enum DSScreen screen;
} DisableHBlankTemplate;

typedef struct ScreenFadeParams {
    enum ScreenFadeOrder order;
    BOOL activeMain;
    BOOL activeSub;
    BOOL existsMain;
    BOOL existsSub;
} ScreenFadeParams;

typedef struct ScreenFadeManager {
    ScreenFadeParams screen;
    ScreenFade mainScreenFade;
    ScreenFade subScreenFade;
    ScreenFadeHBlanks hblanks;
    HardwareWindowSettings hwSettings;
    u16 active;
    u8 dummy_14E;
    u8 dummy_14F;
    u16 savedColor;
} ScreenFadeManager;

static void ResetScreenFadeManager(ScreenFadeManager *manager);
static void ZeroScreenFadeManager(ScreenFadeManager *manager);
static void ClearHBlanks(ScreenFadeHBlanks *hblanks);
static void RunHBlankCallbacks(void *data);
static BOOL TryScreenFade(ScreenFadeParams *screen, ScreenFade *fadeMain, ScreenFade *fadeSub);
static void TryScreenFadeFunc(BOOL *running, ScreenFade *fade);
static BOOL CallScreenFadeFunc(ScreenFade *fade);
static void SetupScreenFadeParams(enum FadeMode mode, ScreenFadeParams *params);
static void InitScreenFade(ScreenFade *fade, enum FadeType type, int steps, int framesPerStep, enum FadeState state, void *data, enum DSScreen screen, HardwareWindowSettings *hwSettings, ScreenFadeHBlanks *hblanks, enum HeapID heapID, u16 color);
static void InitScreenFadeParams(ScreenFadeParams *params, enum ScreenFadeOrder order, BOOL fadeMain, BOOL fadeSub);
static void EnableScreenHBlank(ScreenFadeHBlanks *hblanks, void *data, Callback callback, enum DSScreen screen);
static void DisableScreenHBlank(ScreenFadeHBlanks *hblanks, enum DSScreen screen);
static u16 GetFadeColor(ScreenFadeManager *manager, u16 color);
static u16 GetSavedFadeColor(const ScreenFadeManager *manager);
static void ResetWindowScreenFade(ScreenFade *fade);
static void RequestResetScreenMasterBrightness(ScreenFade *fade);

static void Task_EnableScreenHBlank(SysTask *task, void *data);
static void Task_DisableScreenHBlank(SysTask *task, void *data);
static void Task_ResetScreenMasterBrightness(SysTask *task, void *data);

static void DummyHBlankCallback(void *data);

static const ScreenFadeFunc sScreenFadeFuncs[FADE_TYPE_MAX] = {
    [FADE_TYPE_BRIGHTNESS_OUT] = ScreenFade_BrightnessOut,
    [FADE_TYPE_BRIGHTNESS_IN] = ScreenFade_BrightnessIn,
    [FADE_TYPE_DOWNWARD_OUT] = ScreenFade_DownwardOut,
    [FADE_TYPE_DOWNWARD_IN] = ScreenFade_DownwardIn,
    [FADE_TYPE_UPWARD_OUT] = ScreenFade_UpwardOut,
    [FADE_TYPE_UNK_5] = ScreenFade_UpwardIn,
    [FADE_TYPE_UNK_6] = ScreenFade_CloseToLeftOut,
    [FADE_TYPE_UNK_7] = ScreenFade_OpenFromLeftIn,
    [FADE_TYPE_UNK_8] = ScreenFade_ShutterOut,
    [FADE_TYPE_UNK_9] = ScreenFade_ShutterIn,
    [FADE_TYPE_UNK_10] = ScreenFade_ShutterOpenOut,
    [FADE_TYPE_UNK_11] = ScreenFade_ShutterCloseIn,
    [FADE_TYPE_UNK_12] = ScreenFade_HorizontalCloseOut,
    [FADE_TYPE_UNK_13] = ScreenFade_HorizontalOpenIn,
    [FADE_TYPE_UNK_14] = ScreenFade_SplitOut,
    [FADE_TYPE_UNK_15] = ScreenFade_SplitIn,
    [FADE_TYPE_CIRCLE_OUT] = ScreenFade_CircleOut,
    [FADE_TYPE_CIRCLE_IN] = ScreenFade_CircleIn,
    [FADE_TYPE_TOP_HALF_CIRCLE_OUT] = ScreenFade_TopHalfCircleOut,
    [FADE_TYPE_TOP_HALF_CIRCLE_IN] = ScreenFade_TopHalfCircleIn,
    [FADE_TYPE_UNK_20] = ScreenFade_WedgeOut,
    [FADE_TYPE_UNK_21] = ScreenFade_WedgeIn,
    [FADE_TYPE_UNK_22] = ScreenFade_CloseToCenterOut,
    [FADE_TYPE_UNK_23] = ScreenFade_OpenFromCenterIn,
    [FADE_TYPE_UNK_24] = ScreenFade_BackdropFromCenterOut,
    [FADE_TYPE_UNK_25] = ScreenFade_BackdropToCenterIn,
    [FADE_TYPE_UNK_26] = ScreenFade_HourglassOut,
    [FADE_TYPE_UNK_27] = ScreenFade_HourglassIn,
    [FADE_TYPE_UNK_28] = ScreenFade_InterlaceOut,
    [FADE_TYPE_UNK_29] = ScreenFade_InterlaceIn,
    [FADE_TYPE_UNK_30] = ScreenFade_DownwardBandsOut,
    [FADE_TYPE_UNK_31] = ScreenFade_UpwardBandsIn,
    [FADE_TYPE_UNK_32] = ScreenFade_DiagonalOut,
    [FADE_TYPE_UNK_33] = ScreenFade_DiagonalIn,
    [FADE_TYPE_UNK_34] = ScreenFade_BowtieOut,
    [FADE_TYPE_UNK_35] = ScreenFade_BowtieIn,
    [FADE_TYPE_UNK_36] = ScreenFade_BottomHalfCircleOut,
    [FADE_TYPE_UNK_37] = ScreenFade_BottomHalfCircleIn,
    [FADE_TYPE_UNK_38] = ScreenFade_BackdropFromLeftOut,
    [FADE_TYPE_UNK_39] = ScreenFade_BackdropToLeftIn,
    [FADE_TYPE_CLAMP_OUT] = ScreenFade_ClampOut,
    [FADE_TYPE_CLAMP_IN] = ScreenFade_ClampIn,
};

static ScreenFadeManager sScreenFadeManager;

void StartScreenFade(enum FadeMode mode, enum FadeType typeMain, enum FadeType typeSub, u16 color, int steps, int framesPerStep, enum HeapID heapID)
{
    GF_ASSERT(steps);
    GF_ASSERT(framesPerStep);
    GF_ASSERT(sScreenFadeManager.active == FALSE);

    ScreenFadeManager *manager = &sScreenFadeManager;
    ZeroScreenFadeManager(manager);
    SetupScreenFadeParams(mode, &manager->screen);
    ClearHBlanks(&manager->hblanks);

    u16 fadeColor = GetFadeColor(manager, color);
    InitScreenFade(&manager->mainScreenFade, typeMain, steps, framesPerStep, FADE_IDLE, NULL, DS_SCREEN_MAIN, &manager->hwSettings, &manager->hblanks, heapID, fadeColor);
    InitScreenFade(&manager->subScreenFade, typeSub, steps, framesPerStep, FADE_IDLE, NULL, DS_SCREEN_SUB, &manager->hwSettings, &manager->hblanks, heapID, fadeColor);

    manager->active = TRUE;
    TryScreenFadeFunc(&manager->screen.activeMain, &manager->mainScreenFade);
    TryScreenFadeFunc(&manager->screen.activeSub, &manager->subScreenFade);

    if (manager->screen.existsMain) {
        RequestResetScreenMasterBrightness(&manager->mainScreenFade);
        manager->dummy_14E = 1;
    }

    if (manager->screen.existsSub) {
        RequestResetScreenMasterBrightness(&manager->subScreenFade);
        manager->dummy_14F = 1;
    }
}

void ExecScreenFade(void)
{
    ScreenFadeManager *manager = &sScreenFadeManager;
    if (manager->active && TryScreenFade(&manager->screen, &manager->mainScreenFade, &manager->subScreenFade) == TRUE) {
        ResetScreenFadeManager(manager);
    }
}

BOOL IsScreenFadeDone(void)
{
    return !sScreenFadeManager.active;
}

void FinishScreenFade(void)
{
    DisableScreenHBlank(&sScreenFadeManager.hblanks, DS_SCREEN_MAIN);
    DisableScreenHBlank(&sScreenFadeManager.hblanks, DS_SCREEN_SUB);

    if (sScreenFadeManager.screen.activeMain) {
        sScreenFadeManager.mainScreenFade.state = FADE_CLEANUP;
    }

    if (sScreenFadeManager.screen.activeSub) {
        sScreenFadeManager.subScreenFade.state = FADE_CLEANUP;
    }

    TryScreenFadeFunc(&sScreenFadeManager.screen.activeMain, &sScreenFadeManager.mainScreenFade);
    TryScreenFadeFunc(&sScreenFadeManager.screen.activeSub, &sScreenFadeManager.subScreenFade);

    sScreenFadeManager.active = 0;
    sScreenFadeManager.dummy_14E = 0;
    sScreenFadeManager.dummy_14F = 0;

    ZeroScreenFadeManager(&sScreenFadeManager);
}

void ResetVisibleHardwareWindows(enum DSScreen screen)
{
    SetVisibleHardwareWindows(GX_WNDMASK_NONE, screen);
}

void ResetScreenMasterBrightness(enum DSScreen screen)
{
    SetScreenMasterBrightness(screen, 0);
}

void SetScreenColorBrightness(enum DSScreen screen, u16 color)
{
    if (color == COLOR_SAVED) {
        color = sScreenFadeManager.savedColor;
    }

    int brightness;
    if (color == COLOR_WHITE) {
        brightness = BRIGHTNESS_WHITE;
    } else {
        brightness = BRIGHTNESS_BLACK;
    }

    SetScreenMasterBrightness(screen, brightness);
}

void SetColorBrightness(u16 color)
{
    if (color == COLOR_SAVED) {
        color = sScreenFadeManager.savedColor;
    }

    int brightness;
    if (color == COLOR_WHITE) {
        brightness = BRIGHTNESS_WHITE;
    } else {
        brightness = BRIGHTNESS_BLACK;
    }

    SetScreenMasterBrightness(DS_SCREEN_MAIN, brightness);
    SetScreenMasterBrightness(DS_SCREEN_SUB, brightness);
    sScreenFadeManager.savedColor = color;
}

void SetupScreenFadeRegisters(enum DSScreen screen, u16 color)
{
    if (color == COLOR_SAVED) {
        color = sScreenFadeManager.savedColor;
    }

    if (screen == DS_SCREEN_MAIN) {
        GX_LoadBGPltt((void *)&color, 0, sizeof(u16));
    } else {
        GXS_LoadBGPltt((void *)&color, 0, sizeof(u16));
    }

    RequestVisibleHardwareWindows(&sScreenFadeManager.hwSettings, GX_WNDMASK_W0, screen);
    RequestHardwareWindowMaskInsidePlane(&sScreenFadeManager.hwSettings, GX_BLEND_ALL, 0, 0, screen);
    RequestHardwareWindowDimensions(&sScreenFadeManager.hwSettings, 0, 0, 0, 0, 0, screen);
    RequestHardwareWindowMaskOutsidePlane(&sScreenFadeManager.hwSettings, GX_BLEND_PLANEMASK_BD, 0, screen);
}

void SetScreenBackgroundColor(u16 color)
{
    GX_LoadBGPltt((void *)&color, 0, sizeof(u16));
    GXS_LoadBGPltt((void *)&color, 0, sizeof(u16));
}

void SetScreenMasterBrightness(enum DSScreen screen, int brightness)
{
    if (screen == DS_SCREEN_MAIN) {
        GX_SetMasterBrightness(brightness);
    } else {
        GXS_SetMasterBrightness(brightness);
    }
}

static void ResetScreenFadeManager(ScreenFadeManager *manager)
{
    manager->active = FALSE;
    manager->savedColor = GetSavedFadeColor(manager);

    if (manager->screen.existsMain) {
        ResetWindowScreenFade(&manager->mainScreenFade);
        if (manager->mainScreenFade.direction == FADE_IN) {
            sScreenFadeManager.dummy_14E = 0;
        }
    }

    if (manager->screen.existsSub) {
        ResetWindowScreenFade(&manager->subScreenFade);
        if (manager->mainScreenFade.direction == FADE_IN) {
            sScreenFadeManager.dummy_14F = 0;
        }
    }

    ZeroScreenFadeManager(manager);
}

static BOOL TryScreenFade(ScreenFadeParams *screen, ScreenFade *fadeMain, ScreenFade *fadeSub)
{
    switch (screen->order) {
    case ORDER_SIMULTANEOUS:
        TryScreenFadeFunc(&screen->activeMain, fadeMain);
        TryScreenFadeFunc(&screen->activeSub, fadeSub);
        break;

    case ORDER_MAIN_FIRST:
        if (screen->activeMain) {
            TryScreenFadeFunc(&screen->activeMain, fadeMain);
        } else {
            TryScreenFadeFunc(&screen->activeSub, fadeSub);
        }
        break;

    case ORDER_SUB_FIRST:
        if (screen->activeSub) {
            TryScreenFadeFunc(&screen->activeSub, fadeSub);
        } else {
            TryScreenFadeFunc(&screen->activeMain, fadeMain);
        }
        break;
    }

    if (screen->activeMain == FALSE && screen->activeSub == FALSE) {
        return TRUE;
    }

    return FALSE;
}

static void TryScreenFadeFunc(BOOL *running, ScreenFade *fade)
{
    if (*running && CallScreenFadeFunc(fade) == TRUE) {
        *running = FALSE;
    }
}

static BOOL CallScreenFadeFunc(ScreenFade *fade)
{
    return sScreenFadeFuncs[fade->type](fade);
}

static void SetupScreenFadeParams(enum FadeMode mode, ScreenFadeParams *params)
{
    switch (mode) {
    case FADE_BOTH_SCREENS:
        InitScreenFadeParams(params, ORDER_SIMULTANEOUS, TRUE, TRUE);
        break;

    case FADE_MAIN_THEN_SUB:
        InitScreenFadeParams(params, ORDER_MAIN_FIRST, TRUE, TRUE);
        break;

    case FADE_SUB_THEN_MAIN:
        InitScreenFadeParams(params, ORDER_SUB_FIRST, TRUE, TRUE);
        break;

    case FADE_MAIN_ONLY:
        InitScreenFadeParams(params, ORDER_MAIN_FIRST, TRUE, FALSE);
        break;

    case FADE_SUB_ONLY:
        InitScreenFadeParams(params, ORDER_SUB_FIRST, FALSE, TRUE);
        break;
    }
}

static void InitScreenFadeParams(ScreenFadeParams *params, enum ScreenFadeOrder order, BOOL fadeMain, BOOL fadeSub)
{
    params->order = order;
    params->activeMain = fadeMain;
    params->activeSub = fadeSub;
    params->existsMain = fadeMain;
    params->existsSub = fadeSub;
}

static void InitScreenFade(ScreenFade *fade, enum FadeType type, int steps, int framesPerStep, enum FadeState state, void *data, enum DSScreen screen, HardwareWindowSettings *hwSettings, ScreenFadeHBlanks *hblanks, enum HeapID heapID, u16 color)
{
    fade->type = type;
    fade->steps = steps;
    fade->framesPerStep = framesPerStep;
    fade->state = state;
    fade->data = data;
    fade->screen = screen;
    fade->hwSettings = hwSettings;
    fade->hblanks = hblanks;
    fade->heapID = heapID;
    fade->color = color;
}

static void ClearHBlanks(ScreenFadeHBlanks *hblanks)
{
    for (int screen = 0; screen < DS_SCREEN_MAX; screen++) {
        hblanks->data[screen] = NULL;
        hblanks->callback[screen] = DummyHBlankCallback;
        hblanks->running[screen] = FALSE;
    }
}

static void RunHBlankCallbacks(void *data)
{
    ScreenFadeHBlanks *hblanks = data;
    for (int screen = 0; screen < DS_SCREEN_MAX; screen++) {
        hblanks->callback[screen](hblanks->data[screen]);
    }
}

static void EnableScreenHBlank(ScreenFadeHBlanks *hblanks, void *data, Callback callback, enum DSScreen screen)
{
    u8 validCallback = TRUE;
    GF_ASSERT(hblanks->running[screen] == FALSE);
    GF_ASSERT(hblanks->callback[screen] != NULL);

    if (hblanks->running[DS_SCREEN_MAIN] == FALSE && hblanks->running[DS_SCREEN_SUB] == FALSE) {
        validCallback = SetHBlankCallback(RunHBlankCallbacks, hblanks);
    }

    GF_ASSERT(validCallback == TRUE);

    hblanks->data[screen] = data;

    if (callback) {
        hblanks->callback[screen] = callback;
    } else {
        hblanks->callback[screen] = DummyHBlankCallback;
    }

    hblanks->running[screen] = TRUE;
}

static void DisableScreenHBlank(ScreenFadeHBlanks *hblanks, enum DSScreen screen)
{
    hblanks->running[screen] = FALSE;

    if (hblanks->running[DS_SCREEN_MAIN] == FALSE && hblanks->running[DS_SCREEN_SUB] == FALSE) {
        DisableHBlank();
    }

    hblanks->callback[screen] = DummyHBlankCallback;
    hblanks->data[screen] = NULL;
}

void RequestEnableScreenHBlank(ScreenFadeHBlanks *hblanks, void *data, Callback callback, enum DSScreen screen, enum HeapID heapID)
{
    EnableHBlankTemplate *template = Heap_AllocAtEnd(heapID, sizeof(EnableHBlankTemplate));
    template->hblanks = hblanks;
    template->data = data;
    template->callback = callback;
    template->screen = screen;

    SysTask_ExecuteAfterVBlank(Task_EnableScreenHBlank, template, LOCAL_TASK_PRIORIITY);
}

void RequestDisableScreenHBlank(ScreenFadeHBlanks *hblanks, enum DSScreen screen, enum HeapID heapID)
{
    DisableHBlankTemplate *template = Heap_AllocAtEnd(heapID, sizeof(DisableHBlankTemplate));
    template->hblanks = hblanks;
    template->screen = screen;

    SysTask_ExecuteAfterVBlank(Task_DisableScreenHBlank, template, LOCAL_TASK_PRIORIITY);
}

static void Task_EnableScreenHBlank(SysTask *task, void *data)
{
    EnableHBlankTemplate *template = data;
    EnableScreenHBlank(template->hblanks, template->data, template->callback, template->screen);
    SysTask_Done(task);
    Heap_Free(data);
}

static void Task_DisableScreenHBlank(SysTask *task, void *data)
{
    DisableHBlankTemplate *template = data;
    DisableScreenHBlank(template->hblanks, template->screen);
    SysTask_Done(task);
    Heap_Free(data);
}

static void DummyHBlankCallback(void *data)
{
    return;
}

static u16 GetFadeColor(ScreenFadeManager *manager, u16 color)
{
    if (color == COLOR_SAVED) {
        return manager->savedColor;
    }

    return color;
}

static u16 GetSavedFadeColor(const ScreenFadeManager *manager)
{
    const ScreenFade *fade;
    if (manager->screen.existsMain == TRUE) {
        fade = &manager->mainScreenFade;
    } else {
        fade = &manager->subScreenFade;
    }

    if (fade->direction == FADE_OUT) {
        return fade->color;
    }

    return manager->savedColor;
}

static void Task_ResetScreenMasterBrightness(SysTask *task, void *data)
{
    ScreenFade *fade = data;
    SetScreenMasterBrightness(fade->screen, 0);
    SysTask_Done(task);
}

static void RequestResetScreenMasterBrightness(ScreenFade *fade)
{
    if (fade->direction == FADE_IN
        && (fade->color == COLOR_WHITE || fade->color == COLOR_BLACK)
        && fade->method == FADE_BY_WINDOW) {
        SysTask_ExecuteAfterVBlank(Task_ResetScreenMasterBrightness, fade, LOCAL_TASK_PRIORIITY);
    }
}

static void ResetWindowScreenFade(ScreenFade *fade)
{
    if (fade->direction == FADE_OUT
        && (fade->color == COLOR_WHITE || fade->color == COLOR_BLACK)
        && fade->method == FADE_BY_WINDOW) {
        SetScreenColorBrightness(fade->screen, fade->color);
        ResetVisibleHardwareWindows(fade->screen);
    }
}

static void ZeroScreenFadeManager(ScreenFadeManager *manager)
{
    memset(&manager->screen, 0, sizeof(ScreenFadeParams));
    memset(&manager->mainScreenFade, 0, sizeof(ScreenFade));
    memset(&manager->subScreenFade, 0, sizeof(ScreenFade));
    memset(&manager->hblanks, 0, sizeof(ScreenFadeHBlanks));
    memset(&manager->hwSettings, 0, sizeof(HardwareWindowSettings));
}
