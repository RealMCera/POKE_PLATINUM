#ifndef POKEPLATINUM_CONTEST_UTIL_H
#define POKEPLATINUM_CONTEST_UTIL_H

#include "generated/move_contest_effects.h"
#include "generated/pokemon_contest_ranks.h"
#include "generated/pokemon_contest_types.h"

#include "battle/pokemon_sprite_data.h"
#include "overlay006/struct_ov6_02248BE8.h"

#include "contest.h"
#include "pokemon.h"
#include "pokemon_sprite.h"

BOOL Contest_IsPlayerLeader(Contest *contest);
void Contest_SelectOpponents(Contest *contest, enum HeapID heapID, int numRandomOpponents, enum PokemonContestType contestType, enum PokemonContestRank contestRank, int competitionType, BOOL isGameCompleted, BOOL isNatDexObtained);
void Contest_SetupNPCPhotos(Contest *contest, enum HeapID heapID);
void Contest_InitPhotos(Contest *contest);
void Contest_InitOpponentMon(const UnkStruct_ov6_02248BE8 *opponentData, Pokemon *mon, enum HeapID heapID);
PokemonSprite *Contest_CreateMonSprite(PokemonSpriteManager *spriteManager, int contestantID, Pokemon *mon, int spriteType, PokemonSpriteData *pokemonSpriteData, enum HeapID heapID, int x, int y, int z);
void Contest_SelectJudges(Contest *contest, enum HeapID heapID, int bonusJudgeIndex, enum PokemonContestType contestType, enum PokemonContestRank contestRank);
s8 Contest_GetAppealPoints(enum MoveContestEffect contestEffect);
void Contest_LoadTwoLineContestEffectMessages(int moveContestEffectID, u32 *lineOneEffectMessageID, u32 *lineTwoEffectMessageID);
u32 Contest_GetContestEffectDescriptionEntryID(int contestEffect);
void Contest_LoadContestEffectMessage(int contestMoveEffect, int messageSlot, u32 *destMessageID, u32 *destArgType);
u32 Contest_GetContestRankTitleMessageID(enum PokemonContestRank contestRank, int competitionType, BOOL isLinkContest);
u32 Contest_GetRankMessageID(enum PokemonContestRank contestRank);
u32 Contest_GetContestTypeMessageID(enum PokemonContestType contestType);
u32 Contest_GetFullContestTypeMessageID(enum PokemonContestType contestType, int competitionType);
int Contest_ContestantIDToContestantEntryNum(int contestantID);
int Contest_ContestantEntryNumToContestantID(int contestantEntryNum);
BOOL Contest_IsPracticeCompetition(Contest *contest);
int Contest_GetVisualScoreRank(Contest *contest, int contestantID);
int Contest_GetDanceScoreRank(Contest *contest, int contestantID);
void SetLockTextWithAutoScroll(BOOL lockTextWithAutoScroll);
void LockTextSpeed();
u32 CalcMonDataRibbon(enum PokemonContestRank contestRank, enum PokemonContestType contestType);
u32 Contest_GetRandomNPCPhotoPreset(enum PokemonContestRank contestRank, BOOL isLinkContest);

#endif // POKEPLATINUM_CONTEST_UTIL_H
