#ifndef POKEPLATINUM_SCREEN_FADE_FUNCS_H
#define POKEPLATINUM_SCREEN_FADE_FUNCS_H

#include "screen_fade.h"

typedef BOOL (*ScreenFadeFunc)(ScreenFade *);

BOOL ScreenFade_BrightnessOut(ScreenFade *param0);
BOOL ScreenFade_BrightnessIn(ScreenFade *param0);
BOOL ScreenFade_DownwardOut(ScreenFade *param0);
BOOL ScreenFade_DownwardIn(ScreenFade *param0);
BOOL ScreenFade_UpwardOut(ScreenFade *param0);
BOOL ScreenFade_UpwardIn(ScreenFade *param0);
BOOL ScreenFade_CloseToLeftOut(ScreenFade *param0);
BOOL ScreenFade_OpenFromLeftIn(ScreenFade *param0);
BOOL ScreenFade_ShutterOut(ScreenFade *param0);
BOOL ScreenFade_ShutterIn(ScreenFade *param0);
BOOL ScreenFade_ShutterOpenOut(ScreenFade *param0);
BOOL ScreenFade_ShutterCloseIn(ScreenFade *param0);
BOOL ScreenFade_HorizontalCloseOut(ScreenFade *param0);
BOOL ScreenFade_HorizontalOpenIn(ScreenFade *param0);
BOOL ScreenFade_SplitOut(ScreenFade *param0);
BOOL ScreenFade_SplitIn(ScreenFade *param0);
BOOL ScreenFade_CircleOut(ScreenFade *param0);
BOOL ScreenFade_CircleIn(ScreenFade *param0);
BOOL ScreenFade_TopHalfCircleOut(ScreenFade *param0);
BOOL ScreenFade_TopHalfCircleIn(ScreenFade *param0);
BOOL ScreenFade_WedgeOut(ScreenFade *param0);
BOOL ScreenFade_WedgeIn(ScreenFade *param0);
BOOL ScreenFade_CloseToCenterOut(ScreenFade *param0);
BOOL ScreenFade_OpenFromCenterIn(ScreenFade *param0);
BOOL ScreenFade_BackdropFromCenterOut(ScreenFade *param0);
BOOL ScreenFade_BackdropToCenterIn(ScreenFade *param0);
BOOL ScreenFade_HourglassOut(ScreenFade *param0);
BOOL ScreenFade_HourglassIn(ScreenFade *param0);
BOOL ScreenFade_InterlaceOut(ScreenFade *param0);
BOOL ScreenFade_InterlaceIn(ScreenFade *param0);
BOOL ScreenFade_DownwardBandsOut(ScreenFade *param0);
BOOL ScreenFade_UpwardBandsIn(ScreenFade *param0);
BOOL ScreenFade_DiagonalOut(ScreenFade *param0);
BOOL ScreenFade_DiagonalIn(ScreenFade *param0);
BOOL ScreenFade_BowtieOut(ScreenFade *param0);
BOOL ScreenFade_BowtieIn(ScreenFade *param0);
BOOL ScreenFade_BottomHalfCircleOut(ScreenFade *param0);
BOOL ScreenFade_BottomHalfCircleIn(ScreenFade *param0);
BOOL ScreenFade_BackdropFromLeftOut(ScreenFade *param0);
BOOL ScreenFade_BackdropToLeftIn(ScreenFade *param0);
BOOL ScreenFade_ClampOut(ScreenFade *param0);
BOOL ScreenFade_ClampIn(ScreenFade *param0);

#endif // POKEPLATINUM_SCREEN_FADE_FUNCS_H
