#ifndef POKEPLATINUM_STRUCT_02095C60_H
#define POKEPLATINUM_STRUCT_02095C60_H

#include "constants/contests.h"

#include "struct_defs/struct_02095B28.h"

// Communication state for the visual competition scoring: the local score,
// the packet being sent, and the scores received from the other contestants.
typedef struct {
    ContestCommValue localValue; // used directly when not in a link contest
    ContestCommValue sendValue; // packet sent by the leader
    u8 receivedValues[CONTEST_NUM_PARTICIPANTS];
    u8 leaderContestantID;
    u8 netID;
    u8 isLinkContest;
    u8 connectionCount;
} ContestScoringCommState;

#endif // POKEPLATINUM_STRUCT_02095C60_H
