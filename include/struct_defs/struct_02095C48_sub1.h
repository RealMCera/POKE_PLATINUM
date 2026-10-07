#ifndef POKEPLATINUM_STRUCT_02095C48_SUB1_H
#define POKEPLATINUM_STRUCT_02095C48_SUB1_H

#include "constants/contests.h"

#include "struct_defs/struct_02029C88.h"
#include "struct_defs/struct_020954F0.h"
#include "struct_defs/struct_02095C48_sub1_sub1.h"

#include "overlay006/struct_ov6_02248BE8.h"

#include "pokemon.h"
#include "string_gf.h"

// Contest state shared between the field task and the competition apps.
typedef struct {
    Pokemon *contestMons[CONTEST_NUM_PARTICIPANTS];
    UnkStruct_ov6_02248BE8 opponentData[CONTEST_NUM_PARTICIPANTS]; // NPC contestant data loaded from the contest NARC
    ContestJudge judges[CONTEST_NUM_JUDGES];
    String *trainerNames[CONTEST_NUM_PARTICIPANTS];
    ContestPhoto *photos[CONTEST_NUM_PARTICIPANTS];
    u8 trainerGenders[CONTEST_NUM_PARTICIPANTS];
    u8 cameraFlashVariant[CONTEST_NUM_PARTICIPANTS]; // selects the camera flash delay pattern
    u8 monContestFame[CONTEST_NUM_PARTICIPANTS];
    u16 contestantObjEventGFX[CONTEST_NUM_PARTICIPANTS];
    u8 leaderContestantID; // contestant that coordinates the link-contest sync
    u8 leaderElectionResult; // leader-election value of the winning contestant
    u8 bonusJudgeIndex; // judge that awards the larger score bonus
    u8 contestType;
    u8 contestRank;
    u8 competitionType;
    u8 npcPhotoPreset; // selects the NPC contestants' preset photos/accessories
    u8 playerContestantID;
    u8 netID; // local player's network ID
    u8 leaderElectionValue; // value sent during leader election; highest wins
    u8 npcCount;
    u8 connectionCount;
    ContestantResult results[CONTEST_NUM_PARTICIPANTS];
} ContestData;

#endif // POKEPLATINUM_STRUCT_02095C48_SUB1_H
