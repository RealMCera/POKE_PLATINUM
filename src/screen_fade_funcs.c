#include "screen_fade_funcs.h"

#include <nitro.h>
#include <string.h>

#include "constants/graphics.h"

#include "enums.h"
#include "hardware_window.h"
#include "heap.h"
#include "screen_fade.h"
#include "sys_task.h"
#include "sys_task_manager.h"

// Per-fade-type state machines for the screen fade system. screen_fade.c owns
// the ScreenFadeManager and dispatches to one of the ScreenFade_* functions
// below (indexed by enum FadeType) once per frame; each function advances its
// own state machine and returns TRUE once the fade has finished.
//
// Most fades animate a hardware window: the window's inside/outside plane
// masks select which graphics layers are visible, so moving or resizing the
// window reveals or hides the backdrop. A few fades instead drive the master
// brightness register directly (BrightnessOut/BrightnessIn).

// State for a master-brightness fade. Brightness is tracked scaled by 128 so
// it can be interpolated with the same fixed-point helpers as the window fades.
typedef struct {
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    int currentBrightness;
    int targetBrightness;
    int brightnessDelta;
    enum DSScreen screen;
} ScreenFadeBrightness;

// A hardware-window bounding box in 1/128-pixel units. The extra precision
// avoids rounding drift while interpolating between two ScreenFadeRects.
typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} WindowRect;

// A hardware-window bounding box in whole pixels, as authored in the fade
// parameter tables below.
typedef struct {
    u8 left;
    u8 top;
    u8 right;
    u8 bottom;
} ScreenFadeRect;

// Animates a single hardware window from one ScreenFadeRect to another.
typedef struct {
    WindowRect current;
    WindowRect delta;
    WindowRect target;
    enum DSScreen screen;
    int windowID;
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    int flag;
    HardwareWindowSettings *hwSettings;
} WindowFade;

// Parameters for a single-window fade: the start/end rectangles and the
// window's plane masks. `flag` selects immediate vs. VBlank-deferred updates.
typedef struct {
    ScreenFadeRect start;
    ScreenFadeRect end;
    u8 windowID;
    u8 insideMask;
    u8 outsideMask;
    u8 flag;
} WindowFadeParams;

// Two WindowFades animated together (used by the split fades).
typedef struct {
    WindowFade first;
    WindowFade second;
} WindowFadePair;

// Per-scanline window edges for one hardware window. `current` is what the
// HBlank callback reads; `next` is built by the fade and copied over after
// VBlank so the callback never sees a half-updated table.
typedef struct {
    short current[2][192];
    short next[2][192];
    int windowID;
} WindowData;

// Owns one or two WindowData buffers and the screen they apply to.
typedef struct {
    WindowData *data;
    int count;
    enum DSScreen screen;
} WindowDataManager;

// Expanding/contracting circle fade. The window is rebuilt each step as a
// circle of `currentRadius` centred on (centerX, centerY).
typedef struct {
    WindowDataManager windowData;
    int currentRadius;
    int centerX;
    int centerY;
    int radiusDelta;
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    enum HeapID heapID;
    int flag;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
} CircleFade;

typedef struct {
    s16 startRadius;
    s16 endRadius;
    s16 centerX;
    s16 centerY;
    u8 windowID;
    u8 insideMask;
    u8 outsideMask;
    u8 flag;
} CircleFadeParams;

// Wedge (triangle) fade: the window's left and right edges are lines of slope
// tan(currentAngle) meeting at the horizontal centre.
typedef struct {
    WindowDataManager windowData;
    int currentAngle;
    int angleDelta;
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    int flag;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
    enum HeapID heapID;
} WedgeFade;

typedef struct {
    u16 startAngle;
    u16 endAngle;
    u8 windowID;
    u8 insideMask;
    u8 outsideMask;
    u8 flag;
} WedgeFadeParams;

// Curved fade whose window is wide at the top and bottom and narrow in the
// middle; `radius` scales the sine that drives the half-width.
typedef struct {
    WindowDataManager windowData;
    int radius;
    int currentAngle;
    int angleDelta;
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    int flag;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
    enum HeapID heapID;
} HourglassFade;

typedef struct {
    u16 startAngle;
    u16 endAngle;
    u8 windowID;
    u8 insideMask;
    u8 outsideMask;
    u8 flag;
} HourglassFadeParams;

// One horizontal band of an interlace fade.
typedef struct {
    WindowRect current;
    WindowRect delta;
    WindowRect target;
} InterlaceBand;

// Fade that animates several horizontal bands at once, alternating between
// collapsing to the left and to the right.
typedef struct {
    WindowDataManager windowData;
    InterlaceBand *bands;
    int bandCount;
    int stepsRemaining;
    int framesPerStep;
    int frameCounter;
    int flag;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
    enum HeapID heapID;
} InterlaceFade;

typedef struct {
    const ScreenFadeRect *startRects;
    const ScreenFadeRect *endRects;
    u16 count;
    u16 windowID;
    u8 insideMask;
    u8 outsideMask;
    u16 flag;
} InterlaceFadeParams;

// Interpolates an angle from `start` over `range` as a fade progresses.
typedef struct {
    int current;
    int start;
    int range;
} DiagonalAngleAnim;

// Diagonal wipe: two windows tile the screen along a moving diagonal boundary.
typedef struct {
    WindowDataManager windowData;
    DiagonalAngleAnim angle;
    int steps;
    int stepCounter;
    int framesPerStep;
    int frameCounter;
    int flag;
    enum HeapID heapID;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
} DiagonalFade;

typedef struct {
    u16 startAngle;
    u16 endAngle;
    u8 insideMask;
    u8 outsideMask;
    u16 flag;
} DiagonalFadeParams;

// Interpolates an angle from `start` over `range` as a fade progresses.
typedef struct {
    int current;
    int start;
    int range;
} BowtieAngleAnim;

// Bowtie fade: two windows shrink to points at the centre, leaving an
// hourglass-shaped region of content.
typedef struct {
    WindowDataManager windowData;
    BowtieAngleAnim angle;
    int steps;
    int stepCounter;
    int framesPerStep;
    int frameCounter;
    int flag;
    enum HeapID heapID;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
} BowtieFade;

typedef struct {
    u16 startAngle;
    u16 endAngle;
    u8 insideMask;
    u8 outsideMask;
    u16 flag;
} BowtieFadeParams;

// Per-scanline visibility mask for one window. `next` is built by the fade;
// `current` is what the HBlank callback reads.
typedef struct {
    u8 next[192];
    u8 current[192];
    int windowID;
} WipeBuffer;

// One or two WipeBuffers plus the screen they apply to.
typedef struct {
    WipeBuffer buffers[2];
    u8 count;
    u8 screen;
} HBlankWindow;

// A moving boundary: scanlines between `start` and `end` are filled with
// `fill`, toggling at the boundary's current position.
typedef struct {
    u8 start;
    u8 end;
    u16 fill;
} ScreenFadeWipe;

// Fade that fills the screen with backdrop using one or more moving
// boundaries.
typedef struct {
    HBlankWindow hblankWindow;
    const ScreenFadeWipe *wipes;
    int wipeCount;
    int steps;
    int stepCounter;
    int framesPerStep;
    int frameCounter;
    int flag;
    enum HeapID heapID;
    HardwareWindowSettings *hwSettings;
    ScreenFadeHBlanks *hblanks;
} WipeFade;

typedef struct {
    const ScreenFadeWipe *wipes;
    u16 count;
    u16 flag;
} WipeFadeParams;

// Parameters for the two-phase clamp fade: a window fade followed by a wipe.
// `splitRatio` is the fraction of the total steps given to the window phase.
typedef struct {
    WindowFadeParams window;
    WipeFadeParams wipe;
    fx32 splitRatio;
} ClampFadeParams;

// Two-phase clamp fade. `secondPhaseSteps` is the number of steps left for the
// wipe after the window phase; `phase` tracks which phase is running.
typedef struct {
    WindowFade windowFade;
    WipeFade wipeFade;
    ClampFadeParams *params;
    u8 secondPhaseSteps;
    u8 phase;
    u8 flag;
    u8 unk_387;
} ClampFade;

static fx32 TanIdx(int param0);
static int TanIdxMul(int param0, int param1);
static void FillTanTable(int param0, int *param1, int param2, int param3);
static int HalfWidthOverTan(int param0, int param1);
static int DeltaPerStep(int param0, int param1, int param2);
static int AddClampedToByte(int param0, int param1);
static void WindowRect_Add(WindowRect *param0, WindowRect *param1);
static void WindowRect_InitAnimation(WindowRect *param0, WindowRect *param1, WindowRect *param2, const ScreenFadeRect *param3, const ScreenFadeRect *param4, int param5);
static void HardwareWindow_Reset(int param0, HardwareWindowSettings *param1, enum DSScreen screen);
static void WindowData_ApplyHBlank(void *param0);
static void WindowData_Free(WindowDataManager *param0);
static void WindowData_FreeInternal(WindowDataManager *param0);
static WindowData *WindowData_Get(WindowDataManager *param0, int param1);
static void WindowData_CopyNextToCurrent(SysTask *param0, void *param1);
static void HBlankWindow_RequestCopy(HBlankWindow *param0);
static void HBlankWindow_Enable(ScreenFadeHBlanks *param0, HBlankWindow *param1, u32 heapID);
static void HBlankWindow_Disable(ScreenFadeHBlanks *param0, HBlankWindow *param1, u32 param2);
static void HBlankWindow_CopyNextToCurrent(SysTask *param0, void *param1);
static void HBlankWindow_ApplyHBlank(void *param0);
static void BrightnessFade_Start(ScreenFade *param0, int param1);
static BOOL BrightnessFade_Update(ScreenFade *param0);
static BOOL BrightnessFade_Step(ScreenFadeBrightness *param0);
static void WindowFade_Start(ScreenFade *param0, const WindowFadeParams *param1);
static BOOL WindowFade_Update(ScreenFade *param0);
static void WindowFadePair_Start(ScreenFade *param0, const WindowFadeParams *param1, const WindowFadeParams *param2);
static BOOL WindowFadePair_Update(ScreenFade *param0);
static void WindowFade_Init(WindowFade *param0, const WindowFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5);
static BOOL WindowFade_Step(WindowFade *param0);
static void CircleFade_Start(ScreenFade *param0, const CircleFadeParams *param1);
static BOOL CircleFade_Update(ScreenFade *param0);
static void CircleFade_Init(CircleFade *param0, const CircleFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL CircleFade_Step(CircleFade *param0);
static void CircleFade_BuildTable(CircleFade *param0);
static void WedgeFade_Start(ScreenFade *param0, const WedgeFadeParams *param1);
static BOOL WedgeFade_Update(ScreenFade *param0);
static void WedgeFade_Init(WedgeFade *param0, const WedgeFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL WedgeFade_Step(WedgeFade *param0);
static void WedgeFade_BuildTable(WedgeFade *param0);
static void HourglassFade_Start(ScreenFade *param0, const HourglassFadeParams *param1);
static BOOL HourglassFade_Update(ScreenFade *param0);
static void HourglassFade_Init(HourglassFade *param0, const HourglassFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL HourglassFade_Step(HourglassFade *param0);
static void HourglassFade_BuildTable(HourglassFade *param0);
static void InterlaceFade_Start(ScreenFade *param0, const InterlaceFadeParams *param1);
static BOOL InterlaceFade_Update(ScreenFade *param0);
static BOOL InterlaceFade_Step(InterlaceFade *param0);
static void InterlaceFade_Init(InterlaceFade *param0, const InterlaceFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static void InterlaceFade_Free(InterlaceFade *param0);
static void InterlaceFade_BuildTable(InterlaceFade *param0);
static void InterlaceFade_AdvanceBands(InterlaceFade *param0);
static void InterlaceFade_DrawBand(WindowDataManager *param0, WindowRect *param1);
static void DiagonalFade_Start(ScreenFade *param0, DiagonalFadeParams *param1);
static BOOL DiagonalFade_Update(ScreenFade *param0);
static void DiagonalFade_Init(DiagonalFade *param0, DiagonalFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL DiagonalFade_Step(DiagonalFade *param0);
static void DiagonalFade_Free(DiagonalFade *param0);
static void DiagonalFade_BuildTable(DiagonalFade *param0);
static void DiagonalAngleAnim_Update(DiagonalAngleAnim *param0, int param1, int param2);
static void BowtieFade_Start(ScreenFade *param0, BowtieFadeParams *param1);
static BOOL BowtieFade_Update(ScreenFade *param0);
static void BowtieFade_Init(BowtieFade *param0, BowtieFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL BowtieFade_Step(BowtieFade *param0);
static void BowtieFade_Free(BowtieFade *param0);
static void BowtieFade_BuildTable(BowtieFade *param0);
static void BowtieAngleAnim_Update(BowtieAngleAnim *param0, int param1, int param2);
static void WipeFade_Start(ScreenFade *param0, WipeFadeParams *param1);
static BOOL WipeFade_Update(ScreenFade *param0);
static void WipeFade_Init(WipeFade *param0, WipeFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL WipeFade_Step(WipeFade *param0);
static void WipeFade_Free(WipeFade *param0);
static void WipeFade_BuildTable(WipeFade *param0);
static void WipeFade_DrawWipe(const ScreenFadeWipe *param0, WipeBuffer *param1, int param2, int param3);
static void ClampFade_Start(ScreenFade *param0, ClampFadeParams *param1);
static BOOL ClampFade_Update(ScreenFade *param0);
static void ClampFade_InitOut(ClampFade *param0, ClampFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, int param7);
static BOOL ClampFade_StepOut(ClampFade *param0, ScreenFade *param1);
static void ClampFade_InitIn(ClampFade *param0, ClampFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID);
static BOOL ClampFade_StepIn(ClampFade *param0, ScreenFade *param1);

// Fades the screen to the fade colour by ramping the master brightness register.
BOOL ScreenFade_BrightnessOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        param0->direction = FADE_OUT;
        param0->method = FADE_BY_BRIGHTNESS;

        BrightnessFade_Start(param0, 1);
        return 0;
    }

    return BrightnessFade_Update(param0);
}

// Fades the screen in from the fade colour by ramping the master brightness register.
BOOL ScreenFade_BrightnessIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        param0->direction = FADE_IN;
        param0->method = FADE_BY_BRIGHTNESS;

        BrightnessFade_Start(param0, 0);
        return 0;
    }

    return BrightnessFade_Update(param0);
}

// Backdrop sweeps down from the top.
BOOL ScreenFade_DownwardOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0 = {
            0, 192, 1
        };
        static WipeFadeParams v1 = {
            NULL, 1, 1
        };

        v1.wipes = &v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content sweeps down from the top.
BOOL ScreenFade_DownwardIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0 = {
            0, 192, 0
        };
        static WipeFadeParams v1 = {
            NULL, 1, 0
        };

        v1.wipes = &v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Backdrop sweeps up from the bottom.
BOOL ScreenFade_UpwardOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0 = {
            192,
            0,
            1
        };
        static WipeFadeParams v1 = {
            NULL,
            1,
            1
        };

        v1.wipes = &v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content sweeps up from the bottom.
BOOL ScreenFade_UpwardIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0 = {
            192,
            0,
            0
        };
        static WipeFadeParams v1 = {
            NULL,
            1,
            0
        };

        v1.wipes = &v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content collapses to the left edge.
BOOL ScreenFade_CloseToLeftOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 255, 192 },
            { 0, 0, 0, 192 },
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return WindowFade_Update(param0);
}

// Content opens from the left edge.
BOOL ScreenFade_OpenFromLeftIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 0, 192 },
            { 0, 0, 255, 192 },
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Camera-shutter close: backdrop closes in from the top and bottom to the middle.
BOOL ScreenFade_ShutterOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 0, 96, 1 },
            { 192, 96, 1 }
        };
        static WipeFadeParams v1 = {
            NULL,
            2,
            1
        };

        v1.wipes = v0;
        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Camera-shutter open: content opens from the middle to the top and bottom.
BOOL ScreenFade_ShutterIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 96, 0, 0 },
            { 96, 192, 0 }
        };
        static WipeFadeParams v1 = {
            NULL,
            2,
            0
        };

        v1.wipes = v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Backdrop opens outward from the middle, so content disappears to the top and bottom.
BOOL ScreenFade_ShutterOpenOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 96, 0, 1 },
            { 96, 192, 1 }
        };
        static WipeFadeParams v1 = {
            NULL,
            2,
            1
        };

        v1.wipes = v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content closes in from the top and bottom toward the middle.
BOOL ScreenFade_ShutterCloseIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 0, 96, 0 },
            { 192, 96, 0 }
        };
        static WipeFadeParams v1 = {
            NULL,
            2,
            0
        };

        v1.wipes = v0;

        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content collapses to the vertical centre line.
BOOL ScreenFade_HorizontalCloseOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 255, 192 },
            { 128, 0, 128, 192 },
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Content opens from the vertical centre line.
BOOL ScreenFade_HorizontalOpenIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 128, 0, 128, 192 },
            { 0, 0, 255, 192 },
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return WindowFade_Update(param0);
}

// Backdrop grows outward from the centre, splitting the content.
BOOL ScreenFade_SplitOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 128, 0, 128, 192 },
            { 0, 0, 128, 192 },
            0,
            GX_BLEND_PLANEMASK_BD,
            GX_BLEND_ALL,
            1
        };
        static const WindowFadeParams v1 = {
            { 128, 0, 128, 192 },
            { 128, 0, 255, 192 },
            1,
            GX_BLEND_PLANEMASK_BD,
            GX_BLEND_ALL,
            1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFadePair_Start(param0, &v0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFadePair_Update(param0);
}

// Content closes in from the edges toward the centre.
BOOL ScreenFade_SplitIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 128, 192 },
            { 128, 0, 128, 192 },
            0,
            GX_BLEND_PLANEMASK_BD,
            GX_BLEND_ALL,
            0
        };
        static const WindowFadeParams v1 = {
            { 128, 0, 255, 192 },
            { 128, 0, 128, 192 },
            1,
            GX_BLEND_PLANEMASK_BD,
            GX_BLEND_ALL,
            0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFadePair_Start(param0, &v0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFadePair_Update(param0);
}

// Content circle shrinks to nothing.
BOOL ScreenFade_CircleOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            256,
            0,
            128,
            96,
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return CircleFade_Update(param0);
}

// Content circle grows to fill the screen.
BOOL ScreenFade_CircleIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            0,
            256,
            128,
            96,
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return CircleFade_Update(param0);
}

// Circle centred below the screen shrinks, so the content disappears upward.
BOOL ScreenFade_TopHalfCircleOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            512,
            0,
            128,
            288,
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return CircleFade_Update(param0);
}

// Circle centred below the screen grows, so the content appears from the top.
BOOL ScreenFade_TopHalfCircleIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            0,
            512,
            128,
            288,
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return CircleFade_Update(param0);
}

// Wedge-shaped content shrinks to the vertical centre line.
BOOL ScreenFade_WedgeOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WedgeFadeParams v0 = {
            ((0xffff * 90) / 360),
            0,
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        WedgeFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WedgeFade_Update(param0);
}

// Wedge-shaped content opens from the vertical centre line.
BOOL ScreenFade_WedgeIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WedgeFadeParams v0 = {
            0,
            ((0xffff * 90) / 360),
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        WedgeFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WedgeFade_Update(param0);
}

// Content collapses to the centre point.
BOOL ScreenFade_CloseToCenterOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 255, 192 }, { 128, 96, 128, 96 }, 0, GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Content opens from the centre point.
BOOL ScreenFade_OpenFromCenterIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 128, 96, 128, 96 }, { 0, 0, 255, 192 }, 0, GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Backdrop grows outward from the centre point.
BOOL ScreenFade_BackdropFromCenterOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 128, 96, 128, 96 }, { 0, 0, 255, 192 }, 0, GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Backdrop shrinks to the centre point.
BOOL ScreenFade_BackdropToCenterIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 255, 192 }, { 128, 96, 128, 96 }, 0, GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WindowFade_Update(param0);
}

// Hourglass-shaped content shrinks.
BOOL ScreenFade_HourglassOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const HourglassFadeParams v0 = {
            ((0xffff * 90) / 360),
            ((0xffff * 0) / 360),
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            1
        };

        SetScreenBackgroundColor(param0->color);
        HourglassFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return HourglassFade_Update(param0);
}

// Hourglass-shaped content grows.
BOOL ScreenFade_HourglassIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const HourglassFadeParams v0 = {
            ((0xffff * 0) / 360),
            ((0xffff * 90) / 360),
            0,
            GX_BLEND_ALL,
            GX_BLEND_PLANEMASK_BD,
            0
        };

        SetScreenBackgroundColor(param0->color);
        HourglassFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return HourglassFade_Update(param0);
}

// Alternating horizontal bands collapse left and right.
BOOL ScreenFade_InterlaceOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeRect v0[] = {
            { 0, 0, 255, 48 },
            { 0, 47, 255, 96 },
            { 0, 96, 255, 144 },
            { 0, 144, 255, 192 }
        };
        static const ScreenFadeRect v1[] = {
            { 0, 0, 0, 48 },
            { 255, 47, 255, 96 },
            { 0, 96, 0, 144 },
            { 255, 144, 255, 192 }
        };
        InterlaceFadeParams v2;

        v2.startRects = v0;
        v2.endRects = v1;
        v2.count = 4;
        v2.windowID = 0;
        v2.insideMask = GX_BLEND_ALL;
        v2.outsideMask = GX_BLEND_PLANEMASK_BD;
        v2.flag = 1;

        SetScreenBackgroundColor(param0->color);
        InterlaceFade_Start(param0, &v2);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return InterlaceFade_Update(param0);
}

// Alternating horizontal bands open from left and right.
BOOL ScreenFade_InterlaceIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeRect v0[] = {
            { 255, 0, 255, 48 }, { 0, 47, 0, 96 }, { 255, 96, 255, 144 }, { 0, 144, 0, 192 }
        };
        static const ScreenFadeRect v1[] = {
            { 0, 0, 255, 48 }, { 0, 47, 255, 96 }, { 0, 96, 255, 144 }, { 0, 144, 255, 192 }
        };
        InterlaceFadeParams v2;

        v2.startRects = v0;
        v2.endRects = v1;
        v2.count = 4;
        v2.windowID = 0;
        v2.insideMask = GX_BLEND_ALL;
        v2.outsideMask = GX_BLEND_PLANEMASK_BD;
        v2.flag = 0;

        SetScreenBackgroundColor(param0->color);
        InterlaceFade_Start(param0, &v2);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return InterlaceFade_Update(param0);
}

// Three horizontal bands sweep the backdrop downward.
BOOL ScreenFade_DownwardBandsOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[3] = {
            { 0, 64, 1 },
            { 64, 128, 1 },
            { 128, 192, 1 },
        };
        static WipeFadeParams v1 = {
            NULL, 3, 1
        };

        v1.wipes = v0;
        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Three horizontal bands sweep the content upward.
BOOL ScreenFade_UpwardBandsIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[3] = {
            { 64, 0, 0 },
            { 128, 64, 0 },
            { 192, 128, 0 },
        };
        static WipeFadeParams v1 = {
            NULL, 3, 0
        };

        v1.wipes = v0;
        SetScreenBackgroundColor(param0->color);
        WipeFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return WipeFade_Update(param0);
}

// Content collapses along a diagonal boundary.
BOOL ScreenFade_DiagonalOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        DiagonalFadeParams v0 = {
            ((0 * 0xffff) / 360), ((179 * 0xffff) / 360), GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 1
        };

        SetScreenBackgroundColor(param0->color);
        DiagonalFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return DiagonalFade_Update(param0);
}

// Content opens along a diagonal boundary.
BOOL ScreenFade_DiagonalIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        DiagonalFadeParams v0 = {
            ((0 * 0xffff) / 360), ((179 * 0xffff) / 360), GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 0
        };

        SetScreenBackgroundColor(param0->color);
        DiagonalFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return DiagonalFade_Update(param0);
}

// Two windows shrink to points at the centre, leaving a bowtie of content.
BOOL ScreenFade_BowtieOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        BowtieFadeParams v0 = {
            ((0 * 0xffff) / 360), ((45 * 0xffff) / 360), GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 1
        };

        SetScreenBackgroundColor(param0->color);
        BowtieFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return BowtieFade_Update(param0);
}

// Two windows grow from points at the centre.
BOOL ScreenFade_BowtieIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        BowtieFadeParams v0 = {
            ((0 * 0xffff) / 360), ((45 * 0xffff) / 360), GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 0
        };

        SetScreenBackgroundColor(param0->color);
        BowtieFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return BowtieFade_Update(param0);
}

// Circle centred above the screen shrinks, so the content disappears downward.
BOOL ScreenFade_BottomHalfCircleOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            512, 0, 128, -80, 0, GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 1
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return CircleFade_Update(param0);
}

// Circle centred above the screen grows, so the content appears from the bottom.
BOOL ScreenFade_BottomHalfCircleIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const CircleFadeParams v0 = {
            0, 512, 128, -80, 0, GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 0
        };

        SetScreenBackgroundColor(param0->color);
        CircleFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return CircleFade_Update(param0);
}

// Backdrop sweeps in from the left.
BOOL ScreenFade_BackdropFromLeftOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 0, 192 }, { 0, 0, 255, 192 }, 0, GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 1
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return WindowFade_Update(param0);
}

// Backdrop recedes to the left.
BOOL ScreenFade_BackdropToLeftIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const WindowFadeParams v0 = {
            { 0, 0, 255, 192 }, { 0, 0, 0, 192 }, 0, GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 0
        };

        SetScreenBackgroundColor(param0->color);
        WindowFade_Start(param0, &v0);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return WindowFade_Update(param0);
}

// Two-phase fade out: a window collapse followed by a wipe.
BOOL ScreenFade_ClampOut(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 0, 94, 1 },
            { 192, 98, 1 },
        };
        static ClampFadeParams v1 = {
            {
                { 0, 94, 255, 98 },
                { 128, 96, 128, 96 },
                0,
                GX_BLEND_ALL,
                GX_BLEND_PLANEMASK_BD,
                1,
            },
            {
                NULL,
                2,
                1,
            },
            FX32_CONST(0.70f),
        };

        v1.wipe.wipes = v0;

        SetScreenBackgroundColor(param0->color);
        ClampFade_Start(param0, &v1);

        param0->direction = FADE_OUT;
        param0->method = FADE_BY_WINDOW;
        return 0;
    }

    return ClampFade_Update(param0);
}

// Two-phase fade in: a wipe followed by a window opening.
BOOL ScreenFade_ClampIn(ScreenFade *param0)
{
    if (param0->state == 0) {
        static const ScreenFadeWipe v0[2] = {
            { 94, 0, 0 },
            { 98, 192, 0 },
        };
        static ClampFadeParams v1 = {
            {
                { 128, 96, 128, 96 },
                { 0, 94, 255, 98 },
                0,
                GX_BLEND_ALL,
                GX_BLEND_PLANEMASK_BD,
                0,
            },
            {
                NULL,
                2,
                0,
            },
            FX32_CONST(0.70f),
        };

        v1.wipe.wipes = v0;

        SetScreenBackgroundColor(param0->color);
        ClampFade_Start(param0, &v1);

        param0->direction = FADE_IN;
        param0->method = FADE_BY_WINDOW;

        return 0;
    }

    return ClampFade_Update(param0);
}

// Tangent of an angle expressed as an FX index (0x10000 == 360 degrees).
static fx32 TanIdx(int param0)
{
    return FX_Div(FX_SinIdx(param0), FX_CosIdx(param0));
}

// tan(angle) * value, returned as an integer.
static int TanIdxMul(int param0, int param1)
{
    fx32 v0;
    fx32 v1;

    v0 = TanIdx(param0);

    v1 = FX_Mul(v0, param1 << FX32_SHIFT);
    v1 >>= FX32_SHIFT;

    return v1;
}

// Fills param1[param3..param2) with tan(angle) * i. Used to build the sloped
// window edges of the wedge and hourglass fades.
static void FillTanTable(int param0, int *param1, int param2, int param3)
{
    int v0;
    fx32 v1;
    fx32 v2, v3;
    int v4, v5;

    v1 = TanIdx(param0);

    for (v0 = param3; v0 < param2; v0++) {
        v3 = v0 << FX32_SHIFT;
        v2 = FX_Mul(v1, v3);
        v2 >>= FX32_SHIFT;
        *(param1 + v0) = v2;
    }
}

// The distance from the centre at which a line of slope tan(angle) reaches
// half of param1. Used to find how far the hourglass's curved edge extends.
static int HalfWidthOverTan(int param0, int param1)
{
    fx32 v0;
    fx32 v1;
    int v2;

    v0 = TanIdx(param0);
    v1 = (param1 / 2) << FX32_SHIFT;
    v2 = FX_Div(v1, v0);

    return v2;
}

// Per-step change from param0 to param1 over param2 steps, scaled by 128 to
// match the WindowRect's sub-pixel precision.
static int DeltaPerStep(int param0, int param1, int param2)
{
    int v0 = param1 - param0;
    v0 *= 128;
    v0 /= param2;

    return v0;
}

// param0 + param1, clamped to the 0..255 range of a window coordinate.
static int AddClampedToByte(int param0, int param1)
{
    int v0 = param0 + param1;

    if (v0 < 0) {
        v0 = 0;
    }

    if (v0 > 255) {
        v0 = 255;
    }

    return v0;
}

static void WindowRect_Add(WindowRect *param0, WindowRect *param1)
{
    param0->left += param1->left;
    param0->top += param1->top;
    param0->right += param1->right;
    param0->bottom += param1->bottom;
}

// Sets up a window-rectangle animation: param0 is the current rectangle
// (start, scaled by 128), param1 the target (end) and param2 the per-step
// delta over param5 steps.
static void WindowRect_InitAnimation(WindowRect *param0, WindowRect *param1, WindowRect *param2, const ScreenFadeRect *param3, const ScreenFadeRect *param4, int param5)
{
    param0->left = param3->left * 128;
    param0->top = param3->top * 128;
    param0->right = param3->right * 128;
    param0->bottom = param3->bottom * 128;

    param1->left = param4->left;
    param1->top = param4->top;
    param1->right = param4->right;
    param1->bottom = param4->bottom;

    param2->left = DeltaPerStep(param3->left, param4->left, param5);
    param2->top = DeltaPerStep(param3->top, param4->top, param5);
    param2->right = DeltaPerStep(param3->right, param4->right, param5);
    param2->bottom = DeltaPerStep(param3->bottom, param4->bottom, param5);
}

// param1 is 0 for a fade in and 1 for a fade out. The master brightness
// register ranges over -16..16; white (0x7fff) fades toward +16 and black
// (0x0) toward -16, so a fade in starts at the extreme and a fade out ends
// there.
static void BrightnessFade_Start(ScreenFade *param0, int param1)
{
    int v0, v1;
    ScreenFadeBrightness *v2;

    param0->data = Heap_Alloc(param0->heapID, sizeof(ScreenFadeBrightness));
    memset(param0->data, 0, sizeof(ScreenFadeBrightness));
    v2 = param0->data;

    if (param1 == 0) {
        if (param0->color == 0x7fff) {
            v0 = 16;
            v1 = 0;
        } else if (param0->color == 0x0) {
            v0 = -16;
            v1 = 0;
        } else {
            v0 = -16;
            v1 = 0;

            GF_ASSERT(FALSE);
        }
    } else {
        if (param0->color == 0x7fff) {
            v0 = 0;
            v1 = 16;
        } else if (param0->color == 0x0) {
            v0 = 0;
            v1 = -16;
        } else {
            v0 = 0;
            v1 = -16;

            GF_ASSERT(FALSE);
        }
    }

    SetScreenMasterBrightness(param0->screen, v0);

    v2->stepsRemaining = param0->steps;
    v2->framesPerStep = param0->framesPerStep;
    v2->frameCounter = 0;
    v2->currentBrightness = v0 * 128;
    v2->targetBrightness = v1 * 128;
    v2->brightnessDelta = DeltaPerStep(v0, v1, param0->steps);
    v2->screen = param0->screen;

    param0->state++;
}

static BOOL BrightnessFade_Update(ScreenFade *param0)
{
    ScreenFadeBrightness *v0 = param0->data;
    BOOL v1;
    BOOL v2 = 0;

    switch (param0->state) {
    case 1:
        v1 = BrightnessFade_Step(v0);

        if (v1 == 1) {
            param0->state++;
        }
        break;
    case 2:
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        break;
    }

    return v2;
}

static BOOL BrightnessFade_Step(ScreenFadeBrightness *param0)
{
    BOOL v0 = 0;

    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;

            param0->currentBrightness += param0->brightnessDelta;
        } else {
            param0->currentBrightness = param0->targetBrightness;
            v0 = 1;
        }

        SetScreenMasterBrightness(param0->screen, param0->currentBrightness / 128);
    }

    return v0;
}

static inline void WindowData_SetWindowPosition(int param0, int param1, int param2, int param3, int param4, enum DSScreen screen)
{
    if (param4 == 0) {
        if (screen == DS_SCREEN_MAIN) {
            if (GX_IsHBlank()) {
                G2_SetWnd0Position(param0, param1, param2, param3);
            }
        } else {
            if (GX_IsHBlank()) {
                G2S_SetWnd0Position(param0, param1, param2, param3);
            }
        }
    } else {
        if (screen == DS_SCREEN_MAIN) {
            if (GX_IsHBlank()) {
                G2_SetWnd1Position(param0, param1, param2, param3);
            }
        } else {
            if (GX_IsHBlank()) {
                G2S_SetWnd1Position(param0, param1, param2, param3);
            }
        }
    }
}

static inline void WindowData_ApplyScanline(WindowDataManager *param0, int param1, int param2)
{
    WindowData *v0 = WindowData_Get(param0, param2);
    WindowData_SetWindowPosition(v0->current[0][param1], 0, v0->current[1][param1], 192, v0->windowID, param0->screen);
}

// HBlank callback: applies the per-scanline window edges for the next
// scanline. The window spans the full height, so only its left/right edges
// change per line.
static void WindowData_ApplyHBlank(void *param0)
{
    WindowDataManager *v0 = (WindowDataManager *)param0;
    int v1;
    int v2;

    GF_ASSERT(param0);

    v1 = GX_GetVCount();

    if (v1 < 192) {
        v1++;

        if (v1 > 191) {
            v1 -= 192;
        }

        if (v0->count == 1) {
            WindowData_ApplyScanline(v0, v1, 0);
        } else {
            WindowData_ApplyScanline(v0, v1, 0);
            WindowData_ApplyScanline(v0, v1, 1);
        }
    }
}

// Allocates one WindowData for a single window (param1 0/1 selects WND0/WND1)
// or two for a pair.
static void WindowData_Init(WindowDataManager *param0, int param1, enum DSScreen screen, enum HeapID heapID)
{
    switch (param1) {
    case 0:
    case 1:
        param0->data = Heap_Alloc(heapID, sizeof(WindowData));
        param0->count = 1;
        param0->screen = screen;
        param0->data->windowID = param1;
        break;
    case 2: {
        int v0;

        param0->data = Heap_Alloc(heapID, sizeof(WindowData) * 2);
        param0->count = 2;
        param0->screen = screen;

        for (v0 = 0; v0 < 2; v0++) {
            param0->data[v0].windowID = v0;
        }
    } break;
    default:
        break;
    }
}

static void WindowData_Free(WindowDataManager *param0)
{
    WindowData_FreeInternal(param0);
}

static void WindowData_FreeInternal(WindowDataManager *param0)
{
    Heap_Free(param0->data);
    param0->data = NULL;
}

static WindowData *WindowData_Get(WindowDataManager *param0, int param1)
{
    GF_ASSERT(param0->count > param1);
    return param0->data + param1;
}

// SysTask run after VBlank: publishes the freshly built per-scanline edges so
// the HBlank callback never reads a half-updated table.
static void WindowData_CopyNextToCurrent(SysTask *param0, void *param1)
{
    WindowDataManager *v0 = (WindowDataManager *)param1;
    WindowData *v1;
    int v2;

    for (v2 = 0; v2 < v0->count; v2++) {
        v1 = WindowData_Get(v0, v2);
        memcpy(v1->current, v1->next, sizeof(short) * 2 * 192);
    }

    SysTask_Done(param0);
}

// Restores the default window state: no window at all, or WND0 covering the
// whole screen with all planes visible inside and only the backdrop outside.
static void HardwareWindow_Reset(int param0, HardwareWindowSettings *param1, enum DSScreen screen)
{
    if (param0 == 0) {
        RequestVisibleHardwareWindows(param1, GX_WNDMASK_NONE, screen);
    } else {
        RequestVisibleHardwareWindows(param1, GX_WNDMASK_W0, screen);
        RequestHardwareWindowMaskInsidePlane(param1, GX_BLEND_ALL, 0, 0, screen);
        RequestHardwareWindowDimensions(param1, 0, 0, 0, 0, 0, screen);
        RequestHardwareWindowMaskOutsidePlane(param1, GX_BLEND_PLANEMASK_BD, 0, screen);
    }
}

// Applies (param9 == 0) or defers to VBlank (param9 != 0) a window's inside/
// outside plane masks and its bounding box.
static void HardwareWindow_Setup(HardwareWindowSettings *param0, int param1, int param2, int param3, enum DSScreen screen, int param5, int param6, int param7, int param8, int param9)
{
    if (param9 == 0) {
        SetHardwareWindowMaskInsidePlane(param1, 0, param3, screen);
        SetHardwareWindowMaskOutsidePlane(param2, 0, screen);
        SetHardwareWindowDimensions(param5, param6, param7, param8, param3, screen);
    } else {
        RequestHardwareWindowMaskInsidePlane(param0, param1, 0, param3, screen);
        RequestHardwareWindowMaskOutsidePlane(param0, param2, 0, screen);
        RequestHardwareWindowDimensions(param0, param5, param6, param7, param8, param3, screen);
    }
}

// Applies or defers the mask selecting which hardware windows are visible.
static void HardwareWindow_SetVisible(HardwareWindowSettings *param0, int param1, enum DSScreen screen, int param3)
{
    if (param3 == 0) {
        SetVisibleHardwareWindows(param1, screen);
    } else {
        RequestVisibleHardwareWindows(param0, param1, screen);
    }
}

// Sets up one or two per-scanline visibility buffers, each bound to a window.
static void HBlankWindow_Init(HBlankWindow *param0, enum DSScreen screen, int param2, int param3, int param4)
{
    memset(param0, 0, sizeof(HBlankWindow));

    if (param2 == 1) {
        param0->buffers[0].windowID = param3;
        param0->count = param2;
        param0->screen = screen;
    } else {
        param0->buffers[0].windowID = param3;
        param0->buffers[1].windowID = param4;
        param0->count = param2;
        param0->screen = screen;
    }
}

static void HBlankWindow_RequestCopy(HBlankWindow *param0)
{
    SysTask_ExecuteAfterVBlank(HBlankWindow_CopyNextToCurrent, param0, 1023);
}

static void HBlankWindow_Enable(ScreenFadeHBlanks *param0, HBlankWindow *param1, u32 heapID)
{
    RequestEnableScreenHBlank(param0, param1, HBlankWindow_ApplyHBlank, param1->screen, heapID);
}

static void HBlankWindow_Disable(ScreenFadeHBlanks *param0, HBlankWindow *param1, u32 heapID)
{
    RequestDisableScreenHBlank(param0, param1->screen, heapID);
}

// SysTask run after VBlank: publishes the freshly built visibility masks.
static void HBlankWindow_CopyNextToCurrent(SysTask *param0, void *param1)
{
    HBlankWindow *v0 = param1;
    int v1;

    for (v1 = 0; v1 < 2; v1++) {
        memcpy(v0->buffers[v1].current, v0->buffers[v1].next, sizeof(u8) * 192);
    }

    SysTask_Done(param0);
}

static inline void HBlankWindow_SetOutsidePlane(int param0, BOOL param1, enum DSScreen screen)
{
    if (screen == DS_SCREEN_MAIN) {
        if (GX_IsHBlank()) {
            G2_SetWndOutsidePlane(param0, param1);
        }
    } else {
        if (GX_IsHBlank()) {
            G2S_SetWndOutsidePlane(param0, param1);
        }
    }
}

static inline void HBlankWindow_SetInsidePlane(int param0, BOOL param1, int param4, enum DSScreen screen)
{
    if (param4 == 0) {
        if (screen == DS_SCREEN_MAIN) {
            if (GX_IsHBlank()) {
                G2_SetWnd0InsidePlane(param0, param1);
            }
        } else {
            if (GX_IsHBlank()) {
                G2S_SetWnd0InsidePlane(param0, param1);
            }
        }
    } else {
        if (screen == DS_SCREEN_MAIN) {
            if (GX_IsHBlank()) {
                G2_SetWnd1InsidePlane(param0, param1);
            }
        } else {
            if (GX_IsHBlank()) {
                G2S_SetWnd1InsidePlane(param0, param1);
            }
        }
    }
}

// A mask byte of 0 shows content outside the (zero-size) window; 1 shows the
// backdrop instead.
static inline void HBlankWindow_ApplyScanline(HBlankWindow *param0, int param1, int param2)
{
    WipeBuffer *v0 = &param0->buffers[param2];

    if (v0->current[param1] == 0) {
        HBlankWindow_SetOutsidePlane(GX_BLEND_ALL, 1, param0->screen);
        HBlankWindow_SetInsidePlane(GX_BLEND_PLANEMASK_BD, 1, v0->windowID, param0->screen);
    } else {
        HBlankWindow_SetOutsidePlane(GX_BLEND_PLANEMASK_BD, 1, param0->screen);
        HBlankWindow_SetInsidePlane(GX_BLEND_ALL, 1, v0->windowID, param0->screen);
    }
}

// HBlank callback: applies the per-scanline visibility masks for the next
// scanline.
static void HBlankWindow_ApplyHBlank(void *param0)
{
    HBlankWindow *v0 = (HBlankWindow *)param0;
    int v1;
    int v2;

    GF_ASSERT(param0);

    v1 = GX_GetVCount();

    if (v1 < 192) {
        v1++;

        if (v1 > 191) {
            v1 -= 192;
        }

        if (v0->count == 1) {
            HBlankWindow_ApplyScanline(v0, v1, 0);
        } else {
            HBlankWindow_ApplyScanline(v0, v1, 0);
            HBlankWindow_ApplyScanline(v0, v1, 1);
        }
    }
}

static void WindowFade_Start(ScreenFade *param0, const WindowFadeParams *param1)
{
    WindowFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(WindowFade));

    v0 = (WindowFade *)param0->data;

    WindowFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings);

    if (param1->windowID == 0) {
        HardwareWindow_SetVisible(param0->hwSettings, GX_WNDMASK_W0, v0->screen, v0->flag);
    } else {
        HardwareWindow_SetVisible(param0->hwSettings, GX_WNDMASK_W1, v0->screen, v0->flag);
    }

    param0->state++;
}

static BOOL WindowFade_Update(ScreenFade *param0)
{
    WindowFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (WindowFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = WindowFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, param0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;

    case 3:
        v2 = 1;
        break;

    default:
        break;
    }

    return v2;
}

static void WindowFadePair_Start(ScreenFade *param0, const WindowFadeParams *param1, const WindowFadeParams *param2)
{
    WindowFadePair *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(WindowFadePair));
    v0 = (WindowFadePair *)param0->data;

    WindowFade_Init(&v0->first, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings);
    WindowFade_Init(&v0->second, param2, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings);
    HardwareWindow_SetVisible(param0->hwSettings, GX_WNDMASK_W0 | GX_WNDMASK_W1, param0->screen, v0->first.flag);

    param0->state++;
}

static BOOL WindowFadePair_Update(ScreenFade *param0)
{
    WindowFadePair *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (WindowFadePair *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = WindowFade_Step(&v0->first);
        v1 += WindowFade_Step(&v0->second);

        if (v1 == 2) {
            HardwareWindow_Reset(v0->first.flag, param0->hwSettings, param0->screen);

            param0->state++;
        }
        break;
    case 2:
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    }

    return v2;
}

static void WindowFade_Init(WindowFade *param0, const WindowFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5)
{
    WindowRect_InitAnimation(&param0->current, &param0->target, &param0->delta, &param1->start, &param1->end, param2);

    param0->screen = screen;
    param0->windowID = param1->windowID;
    param0->stepsRemaining = param2;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->flag = param1->flag;

    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, param1->windowID, screen, param1->start.left, param1->start.top, param1->start.right, param1->start.bottom, param0->flag);
}

static BOOL WindowFade_Step(WindowFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;
            WindowRect_Add(&param0->current, &param0->delta);
        } else {
            RequestHardwareWindowDimensions(param0->hwSettings, param0->target.left, param0->target.top, param0->target.right, param0->target.bottom, param0->windowID, param0->screen);
            return 1;
        }

        RequestHardwareWindowDimensions(param0->hwSettings, param0->current.left / 128, param0->current.top / 128, param0->current.right / 128, param0->current.bottom / 128, param0->windowID, param0->screen);
    }

    return 0;
}

static void CircleFade_Start(ScreenFade *param0, const CircleFadeParams *param1)
{
    CircleFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(CircleFade));
    v0 = (CircleFade *)param0->data;

    CircleFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);

    param0->state++;
}

static BOOL CircleFade_Update(ScreenFade *param0)
{
    CircleFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (CircleFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = CircleFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void CircleFade_Init(CircleFade *param0, const CircleFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    int v0;
    WindowData *v1;

    v0 = DeltaPerStep(param1->startRadius, param1->endRadius, param2);
    WindowData_Init(&param0->windowData, param1->windowID, screen, heapID);

    param0->currentRadius = param1->startRadius * 128;
    param0->centerX = param1->centerX;
    param0->centerY = param1->centerY;
    param0->radiusDelta = v0;
    param0->stepsRemaining = param2;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    CircleFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v1 = WindowData_Get(&param0->windowData, 0);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, param1->windowID, screen, v1->next[0][0], 0, v1->next[1][0], 192, param0->flag);

    if (param1->windowID == 0) {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W0, screen, param0->flag);
    } else {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W1, screen, param0->flag);
    }

    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL CircleFade_Step(CircleFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;
            param0->currentRadius += param0->radiusDelta;
            CircleFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

// Computes the horizontal span of a circle of radius param0 centred at
// (param1, param2) on scanline param3, writing the left/right edges to
// param4/param5. Scanlines outside the circle produce an empty span.
static void CircleFade_ComputeSpan(int param0, int param1, int param2, int param3, int *param4, int *param5)
{
    fx32 v0;
    fx32 v1;
    fx32 v2;

    v0 = param0 / 128;
    v1 = param3 - param2;

    if (v1 < 0) {
        v1 = -v1;
    }

    if (v1 >= v0) {
        *param4 = 0;
        *param5 = 0;
    } else {
        v1 <<= FX32_SHIFT;
        v0 <<= FX32_SHIFT;
        v2 = FX_Sqrt(FX_Mul(v0, v0) - FX_Mul(v1, v1));
        v2 >>= FX32_SHIFT;

        *param4 = param1 - v2;

        if (*param4 < 0) {
            *param4 = 0;
        }

        *param5 = *param4 + (v2 * 2);

        if (*param5 > 255) {
            *param5 = 255;
        }
    }
}

// Builds the per-scanline circle edges. Scanlines below the centre mirror the
// ones above it, so only the upper half is computed directly.
static void CircleFade_BuildTable(CircleFade *param0)
{
    WindowDataManager *v0 = &param0->windowData;
    int v1;
    int v2;
    int v3;
    int v4;
    WindowData *v5 = WindowData_Get(v0, 0);

    for (v1 = 0; v1 < 192; v1++) {
        if (v1 <= param0->centerY) {
            CircleFade_ComputeSpan(param0->currentRadius, param0->centerX, param0->centerY, v1, &v2, &v3);
        } else {
            if (v1 <= (param0->centerY * 2)) {
                v2 = v5->next[0][(param0->centerY * 2) - v1];
                v3 = v5->next[1][(param0->centerY * 2) - v1];
            } else {
                CircleFade_ComputeSpan(param0->currentRadius, param0->centerX, param0->centerY, v1, &v2, &v3);
            }
        }

        v5->next[0][v1] = v2;
        v5->next[1][v1] = v3;
    }
}

static void WedgeFade_Start(ScreenFade *param0, const WedgeFadeParams *param1)
{
    WedgeFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(WedgeFade));
    v0 = (WedgeFade *)param0->data;

    WedgeFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);
    param0->state++;
}

static BOOL WedgeFade_Update(ScreenFade *param0)
{
    WedgeFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (WedgeFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = WedgeFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void WedgeFade_Init(WedgeFade *param0, const WedgeFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    WindowData *v0;

    param0->angleDelta = DeltaPerStep(param1->startAngle, param1->endAngle, param2);
    WindowData_Init(&param0->windowData, param1->windowID, screen, heapID);

    param0->currentAngle = param1->startAngle * 128;
    param0->stepsRemaining = param2;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    WedgeFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v0 = WindowData_Get(&param0->windowData, 0);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, param1->windowID, screen, v0->next[0][0], 0, v0->next[1][0], 192, param0->flag);

    if (param1->windowID == 0) {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W0, screen, param0->flag);
    } else {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W1, screen, param0->flag);
    }

    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL WedgeFade_Step(WedgeFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;
            param0->currentAngle += param0->angleDelta;
            WedgeFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

// Builds a wedge whose left and right edges are lines of slope tan(angle)
// meeting at the horizontal centre (x = 128).
static void WedgeFade_BuildTable(WedgeFade *param0)
{
    int v0;
    int v1, v2;
    int v3[192];
    WindowData *v4 = WindowData_Get(&param0->windowData, 0);
    FillTanTable(param0->currentAngle / 128, v3, 192, 0);

    for (v0 = 0; v0 < 192; v0++) {
        v4->next[0][v0] = AddClampedToByte(128, -v3[v0]);
        v4->next[1][v0] = AddClampedToByte(128, v3[v0]);
    }
}

static void HourglassFade_Start(ScreenFade *param0, const HourglassFadeParams *param1)
{
    HourglassFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(HourglassFade));
    v0 = (HourglassFade *)param0->data;

    HourglassFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);
    param0->state++;
}

static BOOL HourglassFade_Update(ScreenFade *param0)
{
    HourglassFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (HourglassFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = HourglassFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    }

    return v2;
}

static void HourglassFade_Init(HourglassFade *param0, const HourglassFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    int v0;
    WindowData *v1;

    v0 = (param1->endAngle - param1->startAngle);
    v0 /= param2;

    WindowData_Init(&param0->windowData, param1->windowID, screen, heapID);

    param0->radius = 128 * FX32_ONE;
    param0->currentAngle = param1->startAngle;
    param0->angleDelta = v0;
    param0->stepsRemaining = param2;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    HourglassFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v1 = WindowData_Get(&param0->windowData, 0);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, param1->windowID, screen, v1->next[0][96], 0, v1->next[1][96], 192, param0->flag);

    if (param1->windowID == 0) {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W0, screen, param0->flag);
    } else {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W1, screen, param0->flag);
    }

    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL HourglassFade_Step(HourglassFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;
            param0->currentAngle += param0->angleDelta;
            HourglassFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

// Builds the hourglass edges. The half-width at the centre is sin(angle) * 128;
// the curved caps are lines of slope tan(v2) that meet the centre width at
// scanline v1. The top half is mirrored onto the bottom half.
static void HourglassFade_BuildTable(HourglassFade *param0)
{
    int v0;
    int v1;
    int v2;
    int v3[192];
    int v4;
    int v5;
    int v6;
    int v7, v8;
    WindowData *v9 = WindowData_Get(&param0->windowData, 0);
    v5 = FX_Mul(FX_SinIdx(param0->currentAngle), param0->radius);

    v5 >>= FX32_SHIFT;

    v2 = v5 * 2;
    v2 = v2 / 21;
    v2 += 1;
    v2 = 180 - (v2 * 2);
    v2 = ((0xffff * (v2)) / 360);
    v2 /= 2;
    v1 = HalfWidthOverTan(v2, 256);
    v1 >>= FX32_SHIFT;

    GF_ASSERT(v1 < 192);

    FillTanTable(v2, v3, v1, 0);

    for (v0 = 0; v0 < 96; v0++) {
        v4 = v1 - (v0 + 1);
        v6 = v5;

        if (v4 > 0) {
            if (v3[v4] > v6) {
                v6 = v3[v4];
            }
        }

        v7 = AddClampedToByte(128, -v6);
        v8 = AddClampedToByte(128, v6);

        v9->next[0][v0] = v7;
        v9->next[1][v0] = v8;
        v9->next[0][191 - v0] = v7;
        v9->next[1][191 - v0] = v8;
    }
}

static void InterlaceFade_Start(ScreenFade *param0, const InterlaceFadeParams *param1)
{
    InterlaceFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(InterlaceFade));
    v0 = (InterlaceFade *)param0->data;

    InterlaceFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);
    param0->state++;
}

static BOOL InterlaceFade_Update(ScreenFade *param0)
{
    InterlaceFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (InterlaceFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = InterlaceFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        InterlaceFade_Free(v0);
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void InterlaceFade_Init(InterlaceFade *param0, const InterlaceFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    int v0;
    WindowData *v1;

    param0->bands = Heap_Alloc(heapID, sizeof(InterlaceBand) * param1->count);
    GF_ASSERT(param0->bands != NULL);
    param0->bandCount = param1->count;

    for (v0 = 0; v0 < param1->count; v0++) {
        WindowRect_InitAnimation(&param0->bands[v0].current, &param0->bands[v0].target, &param0->bands[v0].delta, (param1->startRects + v0), (param1->endRects + v0), param2);
    }

    WindowData_Init(&param0->windowData, param1->windowID, screen, heapID);

    param0->stepsRemaining = param2;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    InterlaceFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v1 = WindowData_Get(&param0->windowData, 0);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, param1->windowID, screen, v1->next[0][0], 0, v1->next[1][0], 192, param0->flag);

    if (param1->windowID == 0) {
        HardwareWindow_SetVisible(param0->hwSettings, GX_WNDMASK_W0, screen, param0->flag);
    } else {
        HardwareWindow_SetVisible(param0->hwSettings, GX_WNDMASK_W1, screen, param0->flag);
    }

    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL InterlaceFade_Step(InterlaceFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepsRemaining - 1) > 0) {
            param0->stepsRemaining--;
            InterlaceFade_AdvanceBands(param0);
            InterlaceFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

static void InterlaceFade_Free(InterlaceFade *param0)
{
    Heap_Free(param0->bands);
    param0->bands = NULL;
}

// Clears the table, then draws each band back-to-front so earlier bands win
// where they overlap.
static void InterlaceFade_BuildTable(InterlaceFade *param0)
{
    int v0;
    WindowData *v1 = WindowData_Get(&param0->windowData, 0);
    memset(v1->next, 0, 768);

    for (v0 = (param0->bandCount - 1); v0 >= 0; v0--) {
        InterlaceFade_DrawBand(&param0->windowData, &param0->bands[v0].current);
    }
}

// Writes one band's left/right edges into the scanlines it covers.
static void InterlaceFade_DrawBand(WindowDataManager *param0, WindowRect *param1)
{
    int v0;
    WindowData *v1;
    WindowRect v2;

    v1 = WindowData_Get(param0, 0);

    v2.left = param1->left / 128;
    v2.top = param1->top / 128;
    v2.right = param1->right / 128;
    v2.bottom = param1->bottom / 128;

    for (v0 = v2.top; v0 < v2.bottom; v0++) {
        v1->next[0][v0] = v2.left;
        v1->next[1][v0] = v2.right;
    }
}

// Advances every band's current rectangle by its per-step delta.
static void InterlaceFade_AdvanceBands(InterlaceFade *param0)
{
    int v0;

    for (v0 = 0; v0 < param0->bandCount; v0++) {
        WindowRect_Add(&param0->bands[v0].current, &param0->bands[v0].delta);
    }
}

static void DiagonalFade_Start(ScreenFade *param0, DiagonalFadeParams *param1)
{
    DiagonalFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(DiagonalFade));
    memset(param0->data, 0, sizeof(DiagonalFade));

    v0 = param0->data;
    DiagonalFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);

    param0->state++;
}

static BOOL DiagonalFade_Update(ScreenFade *param0)
{
    DiagonalFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (DiagonalFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = DiagonalFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        DiagonalFade_Free(v0);
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void DiagonalFade_Init(DiagonalFade *param0, DiagonalFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    WindowData *v0;
    WindowData *v1;

    param0->angle.current = 0;
    param0->angle.start = param1->startAngle;
    param0->angle.range = param1->endAngle - param1->startAngle;

    WindowData_Init(&param0->windowData, 2, screen, heapID);

    param0->steps = param2;
    param0->stepCounter = 0;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    DiagonalAngleAnim_Update(&param0->angle, param0->stepCounter, param0->steps);
    DiagonalFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v0 = WindowData_Get(&param0->windowData, 0);
    v1 = WindowData_Get(&param0->windowData, 1);

    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, 0, screen, v0->next[0][0], 0, v0->next[1][0], 192, param0->flag);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, 1, screen, v1->next[0][0], 0, v1->next[1][0], 192, param0->flag);
    HardwareWindow_SetVisible(param5, GX_WNDMASK_W0 | GX_WNDMASK_W1, screen, param0->flag);
    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL DiagonalFade_Step(DiagonalFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepCounter + 1) <= param0->steps) {
            param0->stepCounter++;

            DiagonalAngleAnim_Update(&param0->angle, param0->stepCounter, param0->steps);
            DiagonalFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

static void DiagonalFade_Free(DiagonalFade *param0)
{
    return;
}

// Builds the two windows that tile the screen along a diagonal boundary of
// slope tan(angle). The boundary is mirrored across the screen centre, so the
// two halves use complementary angles.
static void DiagonalFade_BuildTable(DiagonalFade *param0)
{
    WindowData *v0;
    WindowData *v1;
    u16 v2;
    int v3, v4;
    int v5;

    v2 = param0->angle.current % ((90 * 0xffff) / 360);
    v0 = WindowData_Get(&param0->windowData, 0);
    v1 = WindowData_Get(&param0->windowData, 1);

    for (v5 = 0; v5 < 96; v5++) {
        if (param0->angle.current < ((90 * 0xffff) / 360)) {
            v3 = 128;
            v4 = TanIdxMul(v2, (96 - v5));

            if (v4 > 127) {
                v4 = 127;
            }

            v0->next[0][191 - v5] = v3 - v4;
            v0->next[1][191 - v5] = v3;

            v1->next[0][v5] = v3;
            v1->next[1][v5] = v3 + v4;
        } else {
            v0->next[0][191 - v5] = 0;
            v0->next[1][191 - v5] = 128;

            v1->next[0][v5] = 128;
            v1->next[1][v5] = 255;
        }
    }

    for (v5 = 96; v5 < 192; v5++) {
        if (param0->angle.current < ((90 * 0xffff) / 360)) {
            v0->next[0][191 - v5] = 128;
            v0->next[1][191 - v5] = 128;

            v1->next[0][v5] = 128;
            v1->next[1][v5] = 128;
        } else {
            v3 = TanIdxMul(((90 * 0xffff) / 360) - v2, (v5 - 96));

            if (v3 > 127) {
                v3 = 127;
            }

            v0->next[0][191 - v5] = 0;
            v0->next[1][191 - v5] = 128 - v3;

            v1->next[0][v5] = 128 + v3;
            v1->next[1][v5] = 255;
        }
    }
}

// Interpolates the angle from `start` over `range` for step param1 of param2.
static void DiagonalAngleAnim_Update(DiagonalAngleAnim *param0, int param1, int param2)
{
    int v0 = param0->range * param1;
    v0 = v0 / param2;

    param0->current = v0 + param0->start;
}

static void BowtieFade_Start(ScreenFade *param0, BowtieFadeParams *param1)
{
    BowtieFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(BowtieFade));
    memset(param0->data, 0, sizeof(BowtieFade));

    v0 = param0->data;
    BowtieFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);

    param0->state++;
}

static BOOL BowtieFade_Update(ScreenFade *param0)
{
    BowtieFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (BowtieFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = BowtieFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        BowtieFade_Free(v0);
        WindowData_Free(&v0->windowData);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void BowtieFade_Init(BowtieFade *param0, BowtieFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    WindowData *v0;
    WindowData *v1;

    param0->angle.current = param1->startAngle;
    param0->angle.start = param1->startAngle;
    param0->angle.range = param1->endAngle - param1->startAngle;

    WindowData_Init(&param0->windowData, 2, screen, heapID);

    param0->steps = param2;
    param0->stepCounter = 0;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;
    param0->heapID = heapID;
    param0->flag = param1->flag;

    BowtieFade_BuildTable(param0);
    SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);

    v0 = WindowData_Get(&param0->windowData, 0);
    v1 = WindowData_Get(&param0->windowData, 1);

    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, 0, screen, 0, 0, 255, 192, param0->flag);
    HardwareWindow_Setup(param5, param1->insideMask, param1->outsideMask, 1, screen, 0, 0, 255, 192, param0->flag);
    HardwareWindow_SetVisible(param5, GX_WNDMASK_W0 | GX_WNDMASK_W1, screen, param0->flag);
    RequestEnableScreenHBlank(param0->hblanks, &param0->windowData, WindowData_ApplyHBlank, screen, heapID);
}

static BOOL BowtieFade_Step(BowtieFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepCounter + 1) <= param0->steps) {
            param0->stepCounter++;

            BowtieAngleAnim_Update(&param0->angle, param0->stepCounter, param0->steps);
            BowtieFade_BuildTable(param0);
            SysTask_ExecuteAfterVBlank(WindowData_CopyNextToCurrent, &param0->windowData, 1023);
        } else {
            RequestDisableScreenHBlank(param0->hblanks, param0->windowData.screen, param0->heapID);
            return 1;
        }
    }

    return 0;
}

static void BowtieFade_Free(BowtieFade *param0)
{
    return;
}

// Builds two windows symmetric about the screen centre: window 0 covers the
// left region [128 - v4, 128 - v3] and window 1 the right [128 + v3, 128 + v4].
// The shape is mirrored top-to-bottom, so as the angle grows the two windows
// shrink to points at the centre.
static void BowtieFade_BuildTable(BowtieFade *param0)
{
    WindowData *v0;
    WindowData *v1;
    u16 v2;
    int v3, v4;
    int v5;

    v2 = param0->angle.current;
    v0 = WindowData_Get(&param0->windowData, 0);
    v1 = WindowData_Get(&param0->windowData, 1);

    for (v5 = 0; v5 < 96; v5++) {
        v3 = TanIdxMul(v2, (96 - v5));
        v4 = TanIdxMul(((90 * 0xffff) / 360) - v2, (96 - v5));

        if (v3 > 127) {
            v3 = 127;
        }

        if (v4 > 127) {
            v4 = 127;
        }

        v0->next[0][v5] = 128 - v4;
        v0->next[1][v5] = 128 - v3;

        v0->next[0][191 - v5] = 128 - v4;
        v0->next[1][191 - v5] = 128 - v3;

        v1->next[0][v5] = 128 + v3;
        v1->next[1][v5] = 128 + v4;

        v1->next[0][191 - v5] = 128 + v3;
        v1->next[1][191 - v5] = 128 + v4;
    }
}

// Interpolates the angle from `start` over `range` for step param1 of param2.
static void BowtieAngleAnim_Update(BowtieAngleAnim *param0, int param1, int param2)
{
    int v0 = param0->range * param1;
    v0 = v0 / param2;

    param0->current = v0 + param0->start;
}

static void WipeFade_Start(ScreenFade *param0, WipeFadeParams *param1)
{
    WipeFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(WipeFade));
    memset(param0->data, 0, sizeof(WipeFade));

    v0 = param0->data;
    WipeFade_Init(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);

    param0->state++;
}

static BOOL WipeFade_Update(ScreenFade *param0)
{
    WipeFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (WipeFade *)param0->data;

    switch (param0->state) {
    case 1:
        v1 = WipeFade_Step(v0);

        if (v1 == 1) {
            HardwareWindow_Reset(v0->flag, v0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        WipeFade_Free(v0);
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

static void WipeFade_Init(WipeFade *param0, WipeFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    HBlankWindow_Init(&param0->hblankWindow, screen, 1, 0, 0);

    if (param1->flag == 0) {
        memset(param0->hblankWindow.buffers[0].next, 1, sizeof(u8) * 192);
        memset(param0->hblankWindow.buffers[0].current, 1, sizeof(u8) * 192);
    } else {
        memset(param0->hblankWindow.buffers[0].next, 0, sizeof(u8) * 192);
        memset(param0->hblankWindow.buffers[0].current, 0, sizeof(u8) * 192);
    }

    param0->wipes = param1->wipes;
    param0->wipeCount = param1->count;
    param0->flag = param1->flag;
    param0->heapID = heapID;
    param0->steps = param2;
    param0->stepCounter = 0;
    param0->framesPerStep = param3;
    param0->frameCounter = 0;
    param0->hwSettings = param5;
    param0->hblanks = param6;

    HBlankWindow_Enable(param6, &param0->hblankWindow, heapID);

    if (param1->flag == 1) {
        HardwareWindow_Setup(param5, GX_BLEND_PLANEMASK_BD, GX_BLEND_ALL, 0, screen, 0, 0, 0, 0, param1->flag);
    } else {
        HardwareWindow_Setup(param5, GX_BLEND_ALL, GX_BLEND_PLANEMASK_BD, 0, screen, 0, 0, 0, 0, param1->flag);
    }

    HardwareWindow_SetVisible(param5, GX_WNDMASK_W0, screen, param0->flag);
}

static BOOL WipeFade_Step(WipeFade *param0)
{
    param0->frameCounter++;

    if (param0->frameCounter >= param0->framesPerStep) {
        param0->frameCounter = 0;

        if ((param0->stepCounter + 1) <= param0->steps) {
            param0->stepCounter++;

            WipeFade_BuildTable(param0);
            HBlankWindow_RequestCopy(&param0->hblankWindow);
        } else {
            HBlankWindow_Disable(param0->hblanks, &param0->hblankWindow, param0->heapID);
            return 1;
        }
    }

    return 0;
}

static void WipeFade_Free(WipeFade *param0)
{
    return;
}

// Draws every wipe of the fade into the buffer for the current step.
static void WipeFade_BuildTable(WipeFade *param0)
{
    int v0;
    WipeBuffer *v1;
    const ScreenFadeWipe *v2;

    v1 = &param0->hblankWindow.buffers[0];

    for (v0 = 0; v0 < param0->wipeCount; v0++) {
        v2 = &param0->wipes[v0];
        WipeFade_DrawWipe(v2, v1, param0->stepCounter, param0->steps);
    }
}

// Fills scanlines between `start` and `end` with `fill`, toggling the value at
// the boundary's current position (start + (end - start) * param2 / param3).
// When start > end the fill is inverted so the region past the boundary gets
// the opposite value.
static void WipeFade_DrawWipe(const ScreenFadeWipe *param0, WipeBuffer *param1, int param2, int param3)
{
    int v0;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;

    v1 = (param0->end - param0->start) * param2;
    v2 = v1 / param3;

    v2 += param0->start;

    if (param0->start <= param0->end) {
        v3 = param0->start;
        v4 = param0->end;
        v5 = param0->fill;
    } else {
        v3 = param0->end;
        v4 = param0->start;

        if (param0->fill == 0) {
            v5 = 1;
        } else {
            v5 = 0;
        }
    }

    for (v0 = v3; v0 < v4; v0++) {
        if (v0 == v2) {
            if (v5 == 0) {
                v5 = 1;
            } else {
                v5 = 0;
            }
        }

        param1->next[v0] = v5;
    }
}

static void ClampFade_Start(ScreenFade *param0, ClampFadeParams *param1)
{
    ClampFade *v0;

    param0->data = Heap_Alloc(param0->heapID, sizeof(ClampFade));
    memset(param0->data, 0, sizeof(ClampFade));

    v0 = param0->data;

    if (param1->window.flag == 0) {
        ClampFade_InitOut(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);
    } else {
        ClampFade_InitIn(v0, param1, param0->steps, param0->framesPerStep, param0->screen, param0->hwSettings, param0->hblanks, param0->heapID);
    }

    param0->state++;
}

static BOOL ClampFade_Update(ScreenFade *param0)
{
    ClampFade *v0;
    BOOL v1;
    BOOL v2 = 0;

    v0 = (ClampFade *)param0->data;

    switch (param0->state) {
    case 1:
        if (v0->flag == 0) {
            v1 = ClampFade_StepOut(v0, param0);
        } else {
            v1 = ClampFade_StepIn(v0, param0);
        }

        if (v1 == 1) {
            HardwareWindow_Reset(param0->direction, param0->hwSettings, param0->screen);
            param0->state++;
        }
        break;
    case 2:
        Heap_Free(param0->data);
        param0->data = NULL;
        param0->state++;
        v2 = 1;
        break;
    case 3:
        v2 = 1;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }

    return v2;
}

// Fade-out variant: the window phase runs first (collapsing the content to a
// thin band), then the wipe phase finishes the fade. `splitRatio` gives the
// window phase its share of the steps.
static void ClampFade_InitOut(ClampFade *param0, ClampFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, int param7)
{
    int v0 = FX_Mul(param2 * FX32_ONE, param1->splitRatio) >> FX32_SHIFT;

    param0->secondPhaseSteps = param2 - v0;
    param0->params = param1;
    param0->flag = param1->window.flag;

    WindowFade_Init(&param0->windowFade, &param1->window, v0, param3, screen, param5);

    if (param1->window.windowID == 0) {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W0, screen, param1->window.flag);
    } else {
        HardwareWindow_SetVisible(param5, GX_WNDMASK_W1, screen, param1->window.flag);
    }

    param0->phase = 0;
}

static BOOL ClampFade_StepOut(ClampFade *param0, ScreenFade *param1)
{
    BOOL v0;
    BOOL v1 = 0;

    switch (param0->phase) {
    case 0:
        v0 = WindowFade_Step(&param0->windowFade);

        if (v0 == 1) {
            param0->phase++;

            WipeFade_Init(&param0->wipeFade, &param0->params->wipe, param0->secondPhaseSteps, param1->framesPerStep, param1->screen, param1->hwSettings, param1->hblanks, param1->heapID);
        }
        break;
    case 1:
        v0 = WipeFade_Step(&param0->wipeFade);

        if (v0 == 1) {
            v1 = 1;
            param0->phase++;
        }
        break;
    case 2:
        v1 = 1;
        break;
    }

    return v1;
}

// Fade-in variant: the wipe phase runs first, then the window phase opens the
// content back up. `splitRatio` gives the wipe phase its share of the steps.
static void ClampFade_InitIn(ClampFade *param0, ClampFadeParams *param1, int param2, int param3, enum DSScreen screen, HardwareWindowSettings *param5, ScreenFadeHBlanks *param6, enum HeapID heapID)
{
    int v0;

    param0->secondPhaseSteps = FX_Mul(param2 * FX32_ONE, param1->splitRatio) >> FX32_SHIFT;

    v0 = param2 - param0->secondPhaseSteps;

    param0->params = param1;
    param0->flag = param1->window.flag;

    WipeFade_Init(&param0->wipeFade, &param0->params->wipe, v0, param3, screen, param5, param6, heapID);

    param0->phase = 0;
}

static BOOL ClampFade_StepIn(ClampFade *param0, ScreenFade *param1)
{
    BOOL v0;
    BOOL v1 = 0;

    switch (param0->phase) {
    case 0:
        v0 = WipeFade_Step(&param0->wipeFade);

        if (v0 == 1) {
            param0->phase++;
            WindowFade_Init(&param0->windowFade, &param0->params->window, param0->secondPhaseSteps, param1->framesPerStep, param1->screen, param1->hwSettings);

            if (param0->params->window.windowID == 0) {
                HardwareWindow_SetVisible(param1->hwSettings, GX_WNDMASK_W0, param1->screen, param0->params->window.flag);
            } else {
                HardwareWindow_SetVisible(param1->hwSettings, GX_WNDMASK_W1, param1->screen, param0->params->window.flag);
            }
        }
        break;
    case 1:
        v0 = WindowFade_Step(&param0->windowFade);

        if (v0 == 1) {
            v1 = 1;
            param0->phase++;
        }
        break;
    case 2:
        v1 = 1;
        break;
    }

    return v1;
}
