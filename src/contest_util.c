#include "contest_util.h"

#include <nitro.h>
#include <string.h>

#include "constants/contests.h"
#include "generated/move_contest_effects.h"
#include "generated/pokemon_contest_ranks.h"
#include "generated/pokemon_contest_types.h"

#include "struct_defs/struct_020951B0.h"
#include "struct_defs/struct_020954F0.h"
#include "struct_defs/struct_020F568C.h"

#include "battle/pokemon_sprite_data.h"
#include "overlay006/struct_ov6_02248BE8.h"

#include "character_sprite.h"
#include "contest.h"
#include "graphics.h"
#include "heap.h"
#include "math_util.h"
#include "message.h"
#include "narc.h"
#include "pokemon.h"
#include "pokemon_sprite.h"
#include "render_text.h"
#include "string_gf.h"
#include "image_clips.h"

#include "res/text/bank/contest_effects.h"
#include "res/text/bank/contest_text.h"

// Per-effect data for every move contest effect, indexed by MoveContestEffect.
// The two line messages are the short description shown when the move is
// selected; the remaining fields are the acting-competition messages and the
// appeal points awarded.
const ContestEffectData sContestEffectData[CONTEST_EFFECT_MAX] = {
    [CONTEST_EFFECT_NONE] = {
        0x0,
        0x0,
        0x0,
    },
    [CONTEST_EFFECT_FIRST_NEXT_TURN] = {
        ContestEffects_Text_PerformFirst,
        ContestEffects_Text_NextTurn1,
        0x14,
        0x0,
        0x2,
        0x1,
        0x7,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_LAST_NEXT_TURN] = {
        ContestEffects_Text_PerformLast,
        ContestEffects_Text_NextTurn2,
        0x14,
        0x2,
        0x2,
        0x3,
        0x7,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_DOUBLED_JUDGE] = {
        ContestEffects_Text_EachDoubled,
        ContestEffects_Text_JudgePlusTwo,
        0x0,
        0x4,
        0x2,
        0x5,
        0x2,
        0x6,
        0x2,
        0x7,
        0x2,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_2_HEARTS_WHEN_VOLTAGE_UP] = {
        ContestEffects_Text_IfTheVoltage,
        ContestEffects_Text_GoesUpPlusTwo,
        0x14,
        0x8,
        0x8,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_BASIC] = {
        ContestEffects_Text_BasicAct,
        ContestEffects_Text_Unused,
        0x1E,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_UNIQUE_JUDGE] = {
        ContestEffects_Text_IfJudgesAre,
        ContestEffects_Text_NotDoubledPlusThree,
        0xA,
        0x9,
        0x2,
        0xA,
        0x2,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xB,
        0x2,
    },
    [CONTEST_EFFECT_CONSECUTIVE_USE] = {
        ContestEffects_Text_PerformableTwo,
        ContestEffects_Text_TurnsInARow,
        0x14,
        0xC,
        0x2,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_VOLTAGE] = {
        ContestEffects_Text_VoltagePts,
        ContestEffects_Text_AreAdded,
        0x0,
        0xD,
        0x5,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_ALL_SAME_JUDGE] = {
        ContestEffects_Text_IfAllChoose,
        ContestEffects_Text_SameJudgePlusFifteen,
        0x0,
        0xE,
        0x2,
        0xF,
        0x2,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0x10,
        0x2,
    },
    [CONTEST_EFFECT_LOWERS_VOLTAGE] = {
        ContestEffects_Text_LowersVoltage,
        ContestEffects_Text_OfJudgesByOne,
        0x14,
        0x11,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_DOUBLE_NEXT_TURN] = {
        ContestEffects_Text_DoubleScoreIn,
        ContestEffects_Text_NextTurn3,
        0x0,
        0x12,
        0x9,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_STEAL_VOLTAGE] = {
        ContestEffects_Text_GetVoltage,
        ContestEffects_Text_FromOneAhead,
        0x0,
        0x13,
        0x7,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_SUPPRESS_VOLTAGE] = {
        ContestEffects_Text_NoVoltageUp,
        ContestEffects_Text_ThisTurn1,
        0x14,
        0x14,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_RANDOM_ORDER] = {
        ContestEffects_Text_RandomOrder,
        ContestEffects_Text_NextTurn4,
        0x14,
        0x15,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_DOUBLE_FINAL_ACT] = {
        ContestEffects_Text_DoubleScore,
        ContestEffects_Text_ForFinalAct,
        0x14,
        0x16,
        0x9,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_LOW_VOLTAGE_ADVANTAGE] = {
        ContestEffects_Text_HighScoreFor1,
        ContestEffects_Text_LowVoltage,
        0x0,
        0x17,
        0x5,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_FIRST_PERFORMANCE_ADVANTAGE] = {
        ContestEffects_Text_IfFirst,
        ContestEffects_Text_PerformancePlusTwo1,
        0x14,
        0x18,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_FINAL_PERFORMANCE_ADVANTAGE] = {
        ContestEffects_Text_IfFinal,
        ContestEffects_Text_PerformancePlusTwo2,
        0x14,
        0x19,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_NO_VOLTAGE_DOWN] = {
        ContestEffects_Text_NoVoltageDown,
        ContestEffects_Text_ThisTurn2,
        0x14,
        0x1A,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_TWO_VOLTAGE_IN_A_ROW_ADVANTAGE] = {
        ContestEffects_Text_IfVoltageGoes,
        ContestEffects_Text_UpInARowPlusThree,
        0xA,
        0x1B,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_HIGH_SCORE_LATER_TURN] = {
        ContestEffects_Text_HighScoreFor2,
        ContestEffects_Text_ALaterTurn,
        0x0,
        0x1C,
        0x2,
        0x1D,
        0x2,
        0x1E,
        0x2,
        0x1F,
        0x2,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_MAX_VOLTAGE_ADVANTAGE] = {
        ContestEffects_Text_AfterVoltage,
        ContestEffects_Text_HitsMaxPlusThree,
        0x14,
        0x20,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
    [CONTEST_EFFECT_PITY_POINTS] = {
        ContestEffects_Text_IfRatedThe,
        ContestEffects_Text_WorstPlusThree,
        0xA,
        0x21,
        0x2,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
        0xffff,
        0x0,
    },
};

// Returns whether the local player is in control of the contest. Single-player
// contests are always controlled locally; in link contests only the elected
// leader drives the shared sequence.
BOOL Contest_IsPlayerLeader(Contest *contest)
{
    if (contest->isLinkContest == FALSE || (contest->isLinkContest == TRUE && contest->data.leaderContestantID == contest->data.playerContestantID)) {
        return TRUE;
    }

    return FALSE;
}

// Picks the NPC contestants for a contest from the contest NARC and copies them
// into the contest's opponent slots. numRandomOpponents slots are filled with
// randomly chosen opponents; if the player has finished the game and obtained
// the National Dex, one additional "special" opponent is placed in a random
// slot. Real (non-practice) competitions instead take the first four eligible
// opponents in table order.
void Contest_SelectOpponents(Contest *contest, enum HeapID heapID, int numRandomOpponents, enum PokemonContestType contestType, enum PokemonContestRank contestRank, int competitionType, BOOL isGameCompleted, BOOL isNatDexObtained)
{
    int i, j;
    u8 *candidateIndices;
    u8 candidateCount = 0;
    u16 randomIndex;
    int isPostGame = 0;
    int opponentCount;
    int isRealCompetition, isPracticeCompetition;
    UnkStruct_ov6_02248BE8 *opponents;
    int specialCount, specialPick;
    UnkStruct_ov6_02248BE8 specialOpponent;

    isPracticeCompetition = FALSE;
    isRealCompetition = FALSE;

    switch (competitionType) {
    case CONTEST_COMPETITION_PRACTICE_VISUAL:
    case CONTEST_COMPETITION_PRACTICE_DANCE:
    case CONTEST_COMPETITION_PRACTICE_ACTING:
        isPracticeCompetition = TRUE;
        break;
    case CONTEST_COMPETITION_VISUAL:
    case CONTEST_COMPETITION_DANCE:
    case CONTEST_COMPETITION_ACTING:
        isRealCompetition = TRUE;
        break;
    }

    opponents = LoadMemberFromNARC(NARC_INDEX_CONTEST__DATA__CONTEST_DATA, 0, 0, heapID, 1);
    opponentCount = NARC_GetMemberSizeByIndexPair(NARC_INDEX_CONTEST__DATA__CONTEST_DATA, 0) / sizeof(UnkStruct_ov6_02248BE8);
    candidateIndices = Heap_AllocAtEnd(heapID, opponentCount + 1);

    if (isGameCompleted == TRUE && isNatDexObtained == TRUE) {
        isPostGame = 1;
    }

    // Collect the indices of every opponent that matches the rank, story
    // progress, competition type, and contest type.
    for (i = 0; i < opponentCount; i++) {
        if (contestRank != opponents[i].unk_20_0) {
            continue;
        }

        if (isPostGame == 1) {
            if (opponents[i].unk_20_10 == 1) {
                continue;
            }
        } else {
            if ((opponents[i].unk_20_10 == 2) || (opponents[i].unk_20_10 == 3)) {
                continue;
            }
        }

        if (isPracticeCompetition == TRUE) {
            if (opponents[i].unk_20_9 == 0) {
                continue;
            }
        } else if (isRealCompetition == TRUE) {
            if (opponents[i].unk_20_8 == 0) {
                continue;
            }
        } else {
            if ((opponents[i].unk_20_9 == 1) || (opponents[i].unk_20_8 == 1)) {
                continue;
            }
        }

        if (contestType == CONTEST_TYPE_COOL && opponents[i].unk_20_3
            || contestType == CONTEST_TYPE_BEAUTY && opponents[i].unk_20_4
            || contestType == CONTEST_TYPE_CUTE && opponents[i].unk_20_5
            || contestType == CONTEST_TYPE_SMART && opponents[i].unk_20_6
            || contestType == CONTEST_TYPE_TOUGH && opponents[i].unk_20_7) {
            candidateIndices[candidateCount++] = i;
        }
    }

    candidateIndices[candidateCount] = 0xff;

    if (isRealCompetition == FALSE) {
        GF_ASSERT(candidateCount >= numRandomOpponents);

        specialCount = 0;

        for (i = 0; i < candidateCount; i++) {
            if (opponents[candidateIndices[i]].unk_20_10 == 3) {
                specialCount++;
            }
        }

        // Reserve one special opponent to be placed after the random picks.
        if (specialCount > 0) {
            specialPick = Contest_GetRNGNext(contest) % specialCount;

            for (i = 0; i < candidateCount; i++) {
                if (opponents[candidateIndices[i]].unk_20_10 == 3) {
                    if (specialPick == 0) {
                        specialOpponent = opponents[candidateIndices[i]];
                        break;
                    } else {
                        specialPick--;
                    }
                }
            }
        }

        // Fill the last numRandomOpponents slots with distinct random picks,
        // removing each chosen candidate from the pool.
        for (i = 4 - numRandomOpponents; i < 4; i++) {
            randomIndex = Contest_GetRNGNext(contest) % candidateCount;

            if (opponents[candidateIndices[randomIndex]].unk_20_10 == 3) {
                i--;
                continue;
            }

            contest->data.opponentData[i] = opponents[candidateIndices[randomIndex]];

            for (j = randomIndex; candidateIndices[j] != 0xff; j++) {
                candidateIndices[j] = candidateIndices[j + 1];
            }

            candidateCount--;
        }

        if (specialCount > 0) {
            randomIndex = 4 - numRandomOpponents;
            randomIndex += Contest_GetRNGNext(contest) % numRandomOpponents;
            contest->data.opponentData[randomIndex] = specialOpponent;
        }
    } else {
        GF_ASSERT(candidateCount >= 4);

        for (i = 0; i < 4; i++) {
            contest->data.opponentData[i] = opponents[candidateIndices[i]];
        }
    }

    Heap_Free(candidateIndices);
    Heap_Free(opponents);
}

// Applies a preset photo (Pokemon, accessories, and backdrop) to each NPC
// contestant's photo. The preset is chosen by the contest's npcPhotoPreset,
// which indexes one of the twelve per-opponent preset IDs.
void Contest_SetupNPCPhotos(Contest *contest, enum HeapID heapID)
{
    int i, accessorySlot;
    ContestPhotoPreset *presets;
    ContestPhotoPreset *preset;
    int firstNPC;
    int presetID;

    presets = LoadMemberFromNARC(NARC_INDEX_CONTEST__DATA__CONTEST_DATA, 2, FALSE, heapID, TRUE);

    // Link contests place the connected players in the first slots, so only the
    // remaining NPCs get preset photos; in other competitions every contestant
    // is an NPC.
    switch (contest->data.competitionType) {
    case CONTEST_COMPETITION_VISUAL:
    case CONTEST_COMPETITION_DANCE:
    case CONTEST_COMPETITION_ACTING:
        firstNPC = 0;
        break;
    default:
        firstNPC = contest->data.connectionCount;
        break;
    }

    for (i = firstNPC; i < CONTEST_NUM_PARTICIPANTS; i++) {
        switch (contest->data.npcPhotoPreset) {
        case 0:
            presetID = contest->data.opponentData[i].unk_22;
            break;
        case 1:
            presetID = contest->data.opponentData[i].unk_23;
            break;
        case 2:
            presetID = contest->data.opponentData[i].unk_24;
            break;
        case 3:
            presetID = contest->data.opponentData[i].unk_25;
            break;
        case 4:
            presetID = contest->data.opponentData[i].unk_26;
            break;
        case 5:
            presetID = contest->data.opponentData[i].unk_27;
            break;
        case 6:
            presetID = contest->data.opponentData[i].unk_28;
            break;
        case 7:
            presetID = contest->data.opponentData[i].unk_29;
            break;
        case 8:
            presetID = contest->data.opponentData[i].unk_2A;
            break;
        case 9:
            presetID = contest->data.opponentData[i].unk_2B;
            break;
        case 10:
            presetID = contest->data.opponentData[i].unk_2C;
            break;
        case 11:
            presetID = contest->data.opponentData[i].unk_2D;
            break;
        default:
            GF_ASSERT(FALSE);
            presetID = 0;
            break;
        }

        preset = &presets[presetID];

        ContestPhoto_Init(contest->data.photos[i]);
        ContestPhoto_SetPhotoMonFromMon(contest->data.photos[i], contest->data.contestMons[i], preset->monPriority);

        for (accessorySlot = 0; accessorySlot < preset->accessoryCount; accessorySlot++) {
            ContestPhoto_AddAccessoryWithData(contest->data.photos[i], accessorySlot, preset->accessories[accessorySlot].accessoryID, preset->accessories[accessorySlot].xPos, preset->accessories[accessorySlot].yPos, preset->accessories[accessorySlot].priority);
        }

        ContestPhoto_SetBackdrop(contest->data.photos[i], preset->backdrop);
        ContestPhoto_SetContestRank(contest->data.photos[i], contest->data.contestRank);
    }

    Heap_Free(presets);
}

// Resets every contestant's photo to their contest Pokemon with no accessories
// and the default backdrop. Used before the dance competition.
void Contest_InitPhotos(Contest *contest)
{
    int i;

    for (i = 0; i < CONTEST_NUM_PARTICIPANTS; i++) {
        ContestPhoto_Init(contest->data.photos[i]);
        ContestPhoto_SetPhotoMonFromMon(contest->data.photos[i], contest->data.contestMons[i], -1);
        ContestPhoto_SetBackdrop(contest->data.photos[i], 0);
        ContestPhoto_SetContestRank(contest->data.photos[i], contest->data.contestRank);
    }
}

// Builds an NPC contestant's Pokemon from its contest NARC entry: species,
// moves, nickname, OT name, and contest stats.
void Contest_InitOpponentMon(const UnkStruct_ov6_02248BE8 *opponentData, Pokemon *mon, enum HeapID heapID)
{
    int i;
    u16 move;
    u32 personality = Pokemon_GetPersonalityForGenderAndNature(opponentData->unk_14, opponentData->unk_20_12, 0);
    Pokemon_InitWith(mon, opponentData->unk_14, 10, INIT_IVS_RANDOM, TRUE, personality, OTID_NOT_SHINY, 0xf0f0f0f);

    for (i = 0; i < LEARNED_MOVES_MAX; i++) {
        move = opponentData->unk_0C[i];
        Pokemon_SetValue(mon, MON_DATA_MOVE1 + i, &move);
    }

    MessageLoader *contestOpponentNames = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_CONTEST_OPPONENT_NAMES, heapID);
    String *monNickname = MessageLoader_GetNewString(contestOpponentNames, opponentData->unk_16);
    String *monOTName = MessageLoader_GetNewString(contestOpponentNames, opponentData->unk_18);

    Pokemon_SetValue(mon, MON_DATA_NICKNAME_STRING, monNickname);
    Pokemon_SetValue(mon, MON_DATA_OT_NAME_STRING, monOTName);

    String_Free(monNickname);
    String_Free(monOTName);
    MessageLoader_Free(contestOpponentNames);

    u8 cool, beauty, cute, smart, tough, sheen;

    cool = opponentData->cool;
    beauty = opponentData->beauty;
    cute = opponentData->cute;
    smart = opponentData->smart;
    tough = opponentData->tough;
    sheen = opponentData->sheen;

    Pokemon_SetValue(mon, MON_DATA_COOL, &cool);
    Pokemon_SetValue(mon, MON_DATA_BEAUTY, &beauty);
    Pokemon_SetValue(mon, MON_DATA_CUTE, &cute);
    Pokemon_SetValue(mon, MON_DATA_SMART, &smart);
    Pokemon_SetValue(mon, MON_DATA_TOUGH, &tough);
    Pokemon_SetValue(mon, MON_DATA_SHEEN, &sheen);
}

// Creates a contestant's Pokemon sprite. If pokemonSpriteData is provided, its
// tile buffer is filled with the sprite's first frame and its palette/NARC are
// recorded so the sprite can be reloaded later.
PokemonSprite *Contest_CreateMonSprite(PokemonSpriteManager *spriteManager, int contestantID, Pokemon *mon, int spriteType, PokemonSpriteData *pokemonSpriteData, enum HeapID heapID, int x, int y, int z)
{
    PokemonSpriteTemplate spriteTemplate;
    PokemonSprite *sprite;
    int yOffset;

    Pokemon_BuildSpriteTemplate(&spriteTemplate, mon, spriteType);

    yOffset = Pokemon_SpriteYOffset(mon, spriteType);

    if (pokemonSpriteData != NULL) {
        GF_ASSERT(pokemonSpriteData->tiles != NULL);
        CharacterSprite_LoadSpriteFrame0(spriteTemplate.narcID, spriteTemplate.character, heapID, pokemonSpriteData->tiles);
        pokemonSpriteData->palette = spriteTemplate.palette;
        pokemonSpriteData->narcID = spriteTemplate.narcID;
    }

    sprite = PokemonSpriteManager_CreateSprite(spriteManager, &spriteTemplate, x, y + yOffset, z, contestantID, NULL, NULL);
    return sprite;
}

// Picks the three contest judges from the contest NARC. Two normal judges are
// chosen first, then one special judge at random. The judge at bonusJudgeIndex
// is swapped with the special judge so the bonus judge is always in the last
// slot.
void Contest_SelectJudges(Contest *contest, enum HeapID heapID, int bonusJudgeIndex, enum PokemonContestType contestType, enum PokemonContestRank contestRank)
{
    int i;
    u8 normalCount = 0, specialCount = 0;
    u16 specialPick;
    int judgeCount;
    ContestJudge *judges;
    u8 *normalIndices, *specialIndices;

    judges = LoadMemberFromNARC(NARC_INDEX_CONTEST__DATA__CONTEST_DATA, 1, 0, heapID, 1);
    judgeCount = NARC_GetMemberSizeByIndexPair(NARC_INDEX_CONTEST__DATA__CONTEST_DATA, 1) / sizeof(ContestJudge);
    normalIndices = Heap_AllocAtEnd(heapID, judgeCount + 1);
    specialIndices = Heap_AllocAtEnd(heapID, judgeCount + 1);

    // Split the judges that judge this contest type into normal (value 1) and
    // special (value 2) pools.
    for (i = 0; i < judgeCount; i++) {
        if (contestRank != judges[i].contestRank) {
            continue;
        }

        if (contestType == CONTEST_TYPE_COOL && judges[i].cool) {
            if (judges[i].cool > 1) {
                specialIndices[specialCount++] = i;
            } else {
                normalIndices[normalCount++] = i;
            }
        } else if (contestType == CONTEST_TYPE_BEAUTY && judges[i].beauty) {
            if (judges[i].beauty > 1) {
                specialIndices[specialCount++] = i;
            } else {
                normalIndices[normalCount++] = i;
            }
        } else if (contestType == CONTEST_TYPE_CUTE && judges[i].cute) {
            if (judges[i].cute > 1) {
                specialIndices[specialCount++] = i;
            } else {
                normalIndices[normalCount++] = i;
            }
        } else if (contestType == CONTEST_TYPE_SMART && judges[i].smart) {
            if (judges[i].smart > 1) {
                specialIndices[specialCount++] = i;
            } else {
                normalIndices[normalCount++] = i;
            }
        } else if (contestType == CONTEST_TYPE_TOUGH && judges[i].tough) {
            if (judges[i].tough > 1) {
                specialIndices[specialCount++] = i;
            } else {
                normalIndices[normalCount++] = i;
            }
        }
    }

    normalIndices[normalCount] = 0xff;
    specialIndices[specialCount] = 0xff;

    GF_ASSERT(normalCount >= 2);

    for (i = 0; i < 2; i++) {
        contest->data.judges[i] = judges[normalIndices[i]];
    }

    GF_ASSERT(specialCount >= 1);
    specialPick = Contest_GetRNGNext(contest) % specialCount;
    contest->data.judges[2] = judges[specialIndices[specialPick]];

    {
        ContestJudge swap;

        contest->data.bonusJudgeIndex = bonusJudgeIndex;
        swap = contest->data.judges[bonusJudgeIndex];
        contest->data.judges[bonusJudgeIndex] = contest->data.judges[2];
        contest->data.judges[2] = swap;
    }

    Heap_Free(specialIndices);
    Heap_Free(normalIndices);
    Heap_Free(judges);
}

// Returns the appeal points awarded by a move contest effect. Divide by
// POINTS_PER_APPEAL_HEART to get the number of appeal hearts.
s8 Contest_GetAppealPoints(enum MoveContestEffect contestEffect)
{
    GF_ASSERT(contestEffect < (NELEMS(sContestEffectData)));
    return sContestEffectData[contestEffect].appealPoints;
}

void Contest_LoadTwoLineContestEffectMessages(int moveContestEffectID, u32 *lineOneEffectMessageID, u32 *lineTwoEffectMessageID)
{
    GF_ASSERT(moveContestEffectID < (NELEMS(sContestEffectData)));

    *lineOneEffectMessageID = sContestEffectData[moveContestEffectID].lineOneEffectMessageID;
    *lineTwoEffectMessageID = sContestEffectData[moveContestEffectID].lineTwoEffectMessageID;
}

// Maps a move contest effect to the text entry that describes it. Entry 46 is
// the description of contest effect 1, so the entries run consecutively.
u32 Contest_GetContestEffectDescriptionEntryID(int contestEffect)
{
    GF_ASSERT(contestEffect < (NELEMS(sContestEffectData)));
    return 46 + (contestEffect - 1);
}

// Loads the acting-competition message for a contest effect. messageSlot
// selects one of the effect's five message variants; destArgType receives the
// string-template argument type needed to fill in the message's placeholders.
void Contest_LoadContestEffectMessage(int contestMoveEffect, int messageSlot, u32 *destMessageID, u32 *destArgType)
{
    GF_ASSERT(contestMoveEffect < (NELEMS(sContestEffectData)));

    switch (messageSlot) {
    case 0:
    default:
        *destMessageID = sContestEffectData[contestMoveEffect].messageID1;
        *destArgType = sContestEffectData[contestMoveEffect].messageArgType1;
        break;
    case 1:
        *destMessageID = sContestEffectData[contestMoveEffect].messageID2;
        *destArgType = sContestEffectData[contestMoveEffect].messageArgType2;
        break;
    case 2:
        *destMessageID = sContestEffectData[contestMoveEffect].messageID3;
        *destArgType = sContestEffectData[contestMoveEffect].messageArgType3;
        break;
    case 3:
        *destMessageID = sContestEffectData[contestMoveEffect].messageID4;
        *destArgType = sContestEffectData[contestMoveEffect].messageArgType4;
        break;
    case 4:
        *destMessageID = sContestEffectData[contestMoveEffect].messageID5;
        *destArgType = sContestEffectData[contestMoveEffect].messageArgType5;
        break;
    }
}

// Returns the message ID for the contest's title banner: "Link Contest",
// "Practice Contest", or the rank name.
u32 Contest_GetContestRankTitleMessageID(enum PokemonContestRank contestRank, int competitionType, BOOL isLinkContest)
{
    u32 messageID;

    if (isLinkContest == TRUE) {
        return Contest_Text_Link;
    }

    switch (competitionType) {
    case CONTEST_COMPETITION_PRACTICE_VISUAL:
    case CONTEST_COMPETITION_PRACTICE_DANCE:
    case CONTEST_COMPETITION_PRACTICE_ACTING:
        return Contest_Text_Practice;
    }

    switch (contestRank) {
    case CONTEST_RANK_NORMAL:
        messageID = Contest_Text_NormalRank;
        break;
    case CONTEST_RANK_GREAT:
        messageID = Contest_Text_GreatRank;
        break;
    case CONTEST_RANK_ULTRA:
        messageID = Contest_Text_UltraRank;
        break;
    case CONTEST_RANK_MASTER:
    default:
        messageID = Contest_Text_MasterRank;
        break;
    }

    return messageID;
}

// Returns the message ID for a contest rank's name, treating the link rank as
// "Link".
u32 Contest_GetRankMessageID(enum PokemonContestRank contestRank)
{
    u32 messageID;

    switch (contestRank) {
    case CONTEST_RANK_NORMAL:
        messageID = Contest_Text_NormalRank;
        break;
    case CONTEST_RANK_GREAT:
        messageID = Contest_Text_GreatRank;
        break;
    case CONTEST_RANK_ULTRA:
        messageID = Contest_Text_UltraRank;
        break;
    case CONTEST_RANK_MASTER:
        messageID = Contest_Text_MasterRank;
        break;
    case CONTEST_RANK_LINK:
    default:
        messageID = Contest_Text_Link;
        break;
    }

    return messageID;
}

// Returns the message ID for a contest type's name (e.g. "Cool Contest").
u32 Contest_GetContestTypeMessageID(enum PokemonContestType contestType)
{
    return Contest_GetFullContestTypeMessageID(contestType, 2);
}

// Returns the message ID for a contest type's name. Practice dance contests
// show the generic "Contest" name instead of the type-specific one.
u32 Contest_GetFullContestTypeMessageID(enum PokemonContestType contestType, int competitionType)
{
    u32 messageID;

    if (competitionType == CONTEST_COMPETITION_PRACTICE_DANCE) {
        return Contest_Text_Contest;
    }

    switch (contestType) {
    case CONTEST_TYPE_COOL:
        messageID = Contest_Text_CoolContest;
        break;
    case CONTEST_TYPE_BEAUTY:
        messageID = Contest_Text_BeautyContest;
        break;
    case CONTEST_TYPE_CUTE:
        messageID = Contest_Text_CuteContest;
        break;
    case CONTEST_TYPE_SMART:
        messageID = Contest_Text_SmartContest;
        break;
    case CONTEST_TYPE_TOUGH:
    default:
        messageID = Contest_Text_ToughContest;
        break;
    }

    return messageID;
}

// Contestant IDs and entry numbers are reversed: entry number 0 is the last
// contestant (ID 3), entry number 3 is the first (ID 0).
int Contest_ContestantIDToContestantEntryNum(int contestantID)
{
    return CONTEST_NUM_PARTICIPANTS - contestantID - 1;
}

int Contest_ContestantEntryNumToContestantID(int contestantEntryNum)
{
    return CONTEST_NUM_PARTICIPANTS - contestantEntryNum - 1;
}

// Returns whether the contest is a practice competition (visual, dance, or
// acting practice).
BOOL Contest_IsPracticeCompetition(Contest *contest)
{
    switch (contest->data.competitionType) {
    case CONTEST_COMPETITION_PRACTICE_VISUAL:
    case CONTEST_COMPETITION_PRACTICE_DANCE:
    case CONTEST_COMPETITION_PRACTICE_ACTING:
        return TRUE;
    }

    return FALSE;
}

// Converts a contestant's visual score into a 0-7 rank by counting how many of
// the rank's score thresholds it reaches. The thresholds depend on the contest
// rank (or the link-contest row when playing over link).
int Contest_GetVisualScoreRank(Contest *contest, int contestantID)
{
    int rank, visualScore, i;
    const u16 *thresholds;
    const u16 thresholdTable[][8] = {
        { 10, 20, 30, 40, 50, 60, 70, 80 },
        { 90, 110, 130, 150, 170, 190, 210, 230 },
        { 170, 200, 230, 260, 290, 320, 350, 380 },
        { 320, 360, 400, 440, 480, 520, 560, 600 },
        { 100, 200, 300, 400, 450, 500, 550, 600 },
    };

    rank = 0;
    visualScore = contest->data.results[contestantID].visualScore;

    if (contest->isLinkContest == TRUE) {
        thresholds = thresholdTable[CONTEST_RANK_LINK];
    } else {
        thresholds = thresholdTable[contest->data.contestRank];
    }

    for (i = 0; i < 8; i++) {
        if (visualScore < thresholds[i]) {
            return rank;
        }

        rank++;
    }

    return rank;
}

// Converts a contestant's dance score into a 0-3 rank. A score of zero ranks
// 0; otherwise the rank is one plus the number of thresholds reached.
int Contest_GetDanceScoreRank(Contest *contest, int contestantID)
{
    int rank, danceScore, i;
    const u8 *thresholds;
    const u8 thresholdTable[][3] = {
        { 3, 5, 8 },
        { 5, 10, 15 },
        { 7, 15, 23 },
        { 10, 20, 30 },
        { 10, 20, 30 },
    };

    rank = 0;
    danceScore = contest->data.results[contestantID].danceScore;

    if (danceScore == 0) {
        return 0;
    }

    if (contest->isLinkContest == TRUE) {
        thresholds = thresholdTable[CONTEST_RANK_LINK];
    } else {
        thresholds = thresholdTable[contest->data.contestRank];
    }

    rank = 1;

    for (i = 0; i < 3; i++) {
        if (danceScore <= thresholds[i]) {
            return rank;
        }

        rank++;
    }

    return rank;
}

// Configures the text renderer for contest messages. When lockTextWithAutoScroll
// is FALSE the player can speed up printing and auto-scroll; when TRUE the text
// auto-scrolls at a fixed rate and cannot be sped up.
void SetLockTextWithAutoScroll(BOOL lockTextWithAutoScroll)
{
    if (lockTextWithAutoScroll == FALSE) {
        RenderControlFlags_SetCanABSpeedUpPrint(TRUE);
        RenderControlFlags_SetAutoScrollFlags(AUTO_SCROLL_NO_WAIT);
        RenderControlFlags_SetSpeedUpOnTouch(TRUE);
    } else {
        RenderControlFlags_SetAutoScrollFlags(AUTO_SCROLL_ENABLED);
        RenderControlFlags_SetCanABSpeedUpPrint(FALSE);
        RenderControlFlags_SetSpeedUpOnTouch(FALSE);
    }
}

// Disables text speed-up and auto-scroll entirely.
void LockTextSpeed()
{
    RenderControlFlags_SetCanABSpeedUpPrint(FALSE);
    RenderControlFlags_SetAutoScrollFlags(AUTO_SCROLL_DISABLED);
    RenderControlFlags_SetSpeedUpOnTouch(FALSE);
}

// Returns the MON_DATA_* index of the super contest ribbon for the given
// contest type and rank.
u32 CalcMonDataRibbon(enum PokemonContestRank contestRank, enum PokemonContestType contestType)
{
    u32 monDataRibbon;

    switch (contestType) {
    case CONTEST_TYPE_COOL:
        monDataRibbon = MON_DATA_SUPER_COOL_RIBBON + contestRank;
        break;
    case CONTEST_TYPE_BEAUTY:
        monDataRibbon = MON_DATA_SUPER_BEAUTY_RIBBON + contestRank;
        break;
    case CONTEST_TYPE_CUTE:
        monDataRibbon = MON_DATA_SUPER_CUTE_RIBBON + contestRank;
        break;
    case CONTEST_TYPE_SMART:
        monDataRibbon = MON_DATA_SUPER_SMART_RIBBON + contestRank;
        break;
    case CONTEST_TYPE_TOUGH:
        monDataRibbon = MON_DATA_SUPER_TOUGH_RIBBON + contestRank;
        break;
    default:
        GF_ASSERT(FALSE);
        return MON_DATA_SUPER_COOL_RIBBON;
    }

    return monDataRibbon;
}

// Picks a random NPC photo preset (0-11). Master-rank and link contests can use
// any preset; lower ranks are restricted to a subset that grows with the rank.
u32 Contest_GetRandomNPCPhotoPreset(enum PokemonContestRank contestRank, BOOL isLinkContest)
{
    u8 presets[12];
    int presetCount = 0;

    if (contestRank == CONTEST_RANK_MASTER || isLinkContest == TRUE) {
        return LCRNG_Next() % 12;
    }

    MI_CpuClear8(presets, 12);

    presets[presetCount++] = 2;
    presets[presetCount++] = 3;
    presets[presetCount++] = 4;

    if (contestRank >= CONTEST_RANK_GREAT) {
        presets[presetCount++] = 0;
        presets[presetCount++] = 1;
        presets[presetCount++] = 5;
    }

    if (contestRank >= CONTEST_RANK_ULTRA) {
        presets[presetCount++] = 6;
        presets[presetCount++] = 7;
        presets[presetCount++] = 8;
    }

    return presets[LCRNG_Next() % presetCount];
}
