#include "macros/btlanimcmd.inc"

#define FAIRY_DUSK 0x3892 // RGB(18, 4, 14)

L_0:
    JumpIfBattlerSide BATTLER_ROLE_ATTACKER, L_1, L_2
    End

L_1:
    LoadParticleResource 0, silver_wind_spa
    LoadParticleResource 1, sweet_scent_spa
    PlaySoundEffectC SEQ_SE_DP_W016
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 0, 10, FAIRY_DUSK
    WaitForAnimTasks
    CreateEmitter 0, 0, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 0, 0, 0, 0
    PlaySoundEffectC SEQ_SE_DP_W230
    CreateEmitter 1, 0, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 3, 0, 0, 0
    SetExtraParams 1, -2000, 8000, 0
    CreateEmitter 1, 2, EMITTER_CB_NONE
    Delay 12
    PlaySoundEffectR SEQ_SE_DP_W213
    CreateEmitter 1, 4, EMITTER_CB_SET_POS_TO_DEFENDER
    Func_FadeBattlerSprite BATTLE_ANIM_DEFENDER, 0, 1, BATTLE_COLOR_LIGHT_RED, 12, 0
    Func_Shake 2, 0, 1, 4, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    WaitForAllEmitters
    UnloadParticleSystem 0
    UnloadParticleSystem 1
    WaitForAnimTasks
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 10, 0, FAIRY_DUSK
    WaitForAnimTasks
    StopSoundEffect SEQ_SE_DP_W016
    End

L_2:
    LoadParticleResource 0, silver_wind_spa
    LoadParticleResource 1, sweet_scent_spa
    PlaySoundEffectC SEQ_SE_DP_W016
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 0, 10, FAIRY_DUSK
    WaitForAnimTasks
    CreateEmitter 0, 0, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 0, 0, 0, 0
    PlaySoundEffectC SEQ_SE_DP_W230
    CreateEmitter 1, 1, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 3, 0, 0, 0
    SetExtraParams 1, -2000, 8000, 0
    CreateEmitter 1, 3, EMITTER_CB_NONE
    Delay 12
    PlaySoundEffectR SEQ_SE_DP_W213
    CreateEmitter 1, 4, EMITTER_CB_SET_POS_TO_DEFENDER
    Func_FadeBattlerSprite BATTLE_ANIM_DEFENDER, 0, 1, BATTLE_COLOR_LIGHT_RED, 12, 0
    Func_Shake 2, 0, 1, 4, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    WaitForAllEmitters
    UnloadParticleSystem 0
    UnloadParticleSystem 1
    WaitForAnimTasks
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 10, 0, FAIRY_DUSK
    WaitForAnimTasks
    StopSoundEffect SEQ_SE_DP_W016
    End
