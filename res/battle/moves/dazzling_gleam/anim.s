#include "macros/btlanimcmd.inc"

L_0:
    LoadParticleResource 0, luster_purge_spa
    LoadParticleResource 1, lunar_dance_spa
    LoadParticleResource 2, charm_spa
    PlaySoundEffectL SEQ_SE_DP_W236
    CreateEmitter 2, 0, EMITTER_CB_SET_POS_TO_ATTACKER
    CreateEmitter 0, 1, EMITTER_CB_SET_POS_TO_ATTACKER
    CreateEmitter 0, 2, EMITTER_CB_SET_POS_TO_ATTACKER
    CreateEmitter 0, 3, EMITTER_CB_SET_POS_TO_ATTACKER
    CreateEmitter 0, 4, EMITTER_CB_SET_POS_TO_ATTACKER
    Func_FadeBattlerSprite BATTLE_ANIM_ATTACKER, 0, 1, BATTLE_COLOR_LIGHT_RED, 12, 20
    Delay 50
    PlaySoundEffectC SEQ_SE_DP_W076
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 0, 14, BATTLE_COLOR_LIGHT_RED
    Func_FadeBattlerSprite BATTLE_ANIM_BATTLER_PLAYER_1, 0, 1, BATTLE_COLOR_WHITE, 16, 20
    Func_FadeBattlerSprite BATTLE_ANIM_BATTLER_ENEMY_1, 0, 1, BATTLE_COLOR_WHITE, 16, 20
    Func_FadeBattlerSprite BATTLE_ANIM_BATTLER_PLAYER_2, 0, 1, BATTLE_COLOR_WHITE, 16, 20
    Func_FadeBattlerSprite BATTLE_ANIM_BATTLER_ENEMY_2, 0, 1, BATTLE_COLOR_WHITE, 16, 20
    Delay 16
    PlayLoopedSoundEffectR SEQ_SE_DP_W030, 4, 8
    CreateEmitter 0, 0, EMITTER_CB_SET_POS_TO_DEFENDER
    CreateEmitter 1, 1, EMITTER_CB_SET_POS_TO_DEFENDER_SIDE
    CreateEmitter 1, 2, EMITTER_CB_SET_POS_TO_DEFENDER_SIDE
    Delay 16
    Func_FadeBg FADE_BG_TYPE_BASE, 1, 14, 0, BATTLE_COLOR_LIGHT_RED
    Func_Shake 4, 0, 1, 6, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER
    Func_Shake 4, 0, 1, 6, BATTLE_ANIM_BATTLER_SPRITE_DEFENDER_PARTNER
    WaitForAnimTasks
    WaitForAllEmitters
    UnloadParticleSystem 0
    UnloadParticleSystem 1
    UnloadParticleSystem 2
    End
