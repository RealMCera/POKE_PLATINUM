#ifndef POKEPLATINUM_STRUCT_0205C680_H
#define POKEPLATINUM_STRUCT_0205C680_H

#include "overworld_anim_manager.h"

// Per-slot state for one Union Room map object. Slots 0-49 are the other
// players; slot 50 is the player's own map object.
typedef struct UnionRoomTrainer {
    u8 desiredState; // requested action: 0 none, 1 appear (new), 2 appear (present), 3 leave
    u8 phase; // animation phase: 0 idle, 1 warping in, 2 present, 3 warping out, 4 hidden
    u8 friendRank; // PalPad_TrainerIsFriend result: 0 none, 1 friend, >=2 friend-of-friend
    u8 startWarpEffect; // set to 1 to request the warp-in visual effect
    u8 effectActive; // warp effect timer is running
    u8 padding_05;
    u16 effectTimer; // frames left before the warp effect is finished
    u8 appearance; // graphics ID passed to MapObject_ChangeGraphics
    u8 movementSet; // wander movement has already been configured
    u8 padding_0A[2];
    u32 trainerId; // trainer ID of the group leader this slot belongs to
    OverworldAnimManager *warpEffect;
    OverworldAnimManager *friendEffect;
} UnionRoomTrainer;

#endif // POKEPLATINUM_STRUCT_0205C680_H
