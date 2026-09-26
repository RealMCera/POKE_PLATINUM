#include "macros/btlanimcmd.inc"

L_0:
    LoadParticleResource 0, attract_spa
    LoadParticleResource 1, absorb_spa
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 0, 10, BATTLE_COLOR_LIGHT_RED
    WaitForAnimTasks
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, 16, 0, 4
    WaitForAnimTasks
    PlaySoundEffectR SEQ_SE_DP_W204
    CreateEmitter 0, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    Func_FadeBattlerSprite BATTLE_ANIM_DEFENDER, 0, 1, BATTLE_COLOR_LIGHT_RED, 14, 0
    Func_Shake 2, 0, 1, 2, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, -16, 0, 4
    WaitForAnimTasks
    Delay 10
    CreateEmitter 1, 0, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 2, 1, 16, 0
    SetExtraParams 2, 0, 0, 0, 0
    PlayLoopedSoundEffectC SEQ_SE_DP_W152, 2, 16
    WaitForAllEmitters
    CreateEmitter 1, 1, EMITTER_CB_GENERIC
    SetExtraParams 0, 2, 1, 0, 0, 0
    Func_FadeBattlerSprite BATTLE_ANIM_ATTACKER, 0, 1, BATTLE_COLOR_LIGHT_RED, 12, 0
    PlaySoundEffectL SEQ_SE_DP_W071B
    WaitForAllEmitters
    UnloadParticleSystem 0
    UnloadParticleSystem 1
    WaitForAnimTasks
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 10, 0, BATTLE_COLOR_LIGHT_RED
    WaitForAnimTasks
    End
