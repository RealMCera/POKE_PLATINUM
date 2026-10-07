#ifndef POKEPLATINUM_CONTEST_COMM_H
#define POKEPLATINUM_CONTEST_COMM_H

#include "struct_defs/struct_02029C88.h"
#include "struct_defs/struct_02095C60.h"

#include "contest.h"

// Contest communication commands: photo exchange and visual scoring sync.
// See contest_comm.c.

void ContestComm_Init(void *commState);
BOOL ContestComm_SendPhoto(Contest *contest, int contestantID, const ContestPhoto *photo);
BOOL ContestComm_SendPhotoSet(Contest *contest, ContestPhoto **photos);
BOOL ContestComm_SendScore(ContestScoringCommState *scoringState, u32 value);
BOOL ContestComm_SendFinished(ContestScoringCommState *scoringState, int finished);

#endif // POKEPLATINUM_CONTEST_COMM_H
