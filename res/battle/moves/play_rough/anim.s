#include "macros/btlanimcmd.inc"

L_0:
    LoadParticleResource 0, smoke_screen_spa
    LoadParticleResource 1, struggle_spa
    LoadParticleResource 2, attract_spa
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, 0, 16, 4
    WaitForAnimTasks
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, 0, -16, 4
    WaitForAnimTasks
    PlaySoundEffectR SEQ_SE_DP_W036
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, 24, 0, 4
    WaitForAnimTasks
    PlayLoopedSoundEffectR SEQ_SE_DP_W028B, 3, 6
    CreateEmitter 0, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    CreateEmitter 0, 1, EMITTER_CB_SET_POS_TO_DEFENDER
    BeginLoop 3
    PlaySoundEffectR SEQ_SE_DP_W025B
    CreateEmitter 1, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    Func_Shake 3, 0, 1, 2, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, -6, 4, 2
    WaitForAnimTasks
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, 6, -4, 2
    WaitForAnimTasks
    EndLoop
    PlaySoundEffectR SEQ_SE_DP_W025B
    CreateEmitter 1, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    CreateEmitter 2, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    Func_FadeBattlerSprite BATTLE_ANIM_DEFENDER, 0, 1, BATTLE_COLOR_LIGHT_RED, 14, 0
    Func_Shake 4, 0, 1, 4, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    Func_MoveBattler BATTLE_ANIM_BATTLER_SPRITE_ATTACKER, -24, 0, 4
    WaitForAnimTasks
    WaitForAllEmitters
    UnloadParticleSystem 0
    UnloadParticleSystem 1
    UnloadParticleSystem 2
    End
