#include "pokemon_info_display.h"

#include <nitro.h>

#include "constants/versions.h"

#include "struct_defs/struct_02090800.h"

#include "heap.h"
#include "message.h"
#include "pokemon.h"
#include "rtc.h"
#include "string_gf.h"
#include "string_template.h"
#include "trainer_info.h"
#include "special_met_location.h"

static int DeterminePokemonStatus(Pokemon *mon, BOOL monOTMatches, int heapID);
static void InitializeNatureRelatedString(PokemonInfoDisplayStruct *infoDisplay);
static void InitializePokemonMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID);
static void InitializeAlternateMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID);
static void InitializeSpecialMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID, int useEggInfo);
static void InitializeIVsString(PokemonInfoDisplayStruct *infoDisplay);
static void InitializeFlavorAffinityString(PokemonInfoDisplayStruct *infoDisplay);
static void InitializeFriendshipLevelString(PokemonInfoDisplayStruct *infoDisplay);
static void AssignTrainerInfoToBoxPokemon(BoxPokemon *boxMon, TrainerInfo *trainerInfo, enum HeapID heapID);
static void BoxPokemon_SetMetLocationAndDate(BoxPokemon *boxMon, int metLocation, int isHatch);
static void BoxPokemon_ResetMetLocationAndDate(BoxPokemon *boxMon, int isHatch);
static void BoxPokemon_SetMetLevelToCurrentLevel(BoxPokemon *boxMon);
static void BoxPokemon_SetFatefulEncounterFlag(BoxPokemon *boxMon);

// Builds the set of strings shown on the Pokémon summary screen's "Trainer memo"
// page. Each of the five text slots holds a formatted line plus the 1-based row
// on which it should be drawn. Which slots are populated depends on the status
// returned by DeterminePokemonStatus.
PokemonInfoDisplayStruct *PokemonInfoDisplay_New(Pokemon *mon, BOOL monOTMatches, enum HeapID heapID)
{
    PokemonInfoDisplayStruct *infoDisplay = Heap_Alloc(heapID, sizeof(PokemonInfoDisplayStruct));
    infoDisplay->heapID = heapID;
    infoDisplay->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_POKEMON_SUMMARY_SCREEN, infoDisplay->heapID);
    infoDisplay->stringTemplate = StringTemplate_New(9, 32, infoDisplay->heapID);
    infoDisplay->mon = mon;
    infoDisplay->monOTMatches = monOTMatches;

    {
        infoDisplay->natureText.line = 0;
        infoDisplay->natureText.text = NULL;

        infoDisplay->metInfoText.line = 0;
        infoDisplay->metInfoText.text = NULL;

        infoDisplay->ivsText.line = 0;
        infoDisplay->ivsText.text = NULL;

        infoDisplay->flavorText.line = 0;
        infoDisplay->flavorText.text = NULL;

        infoDisplay->friendshipText.line = 0;
        infoDisplay->friendshipText.text = NULL;
    }

    // The status selects the met-info message and the row layout. Cases 0-15
    // describe a regular (non-egg) Pokémon, cases 16-20 an egg or a hatched egg.
    switch (DeterminePokemonStatus(infoDisplay->mon, infoDisplay->monOTMatches, infoDisplay->heapID)) {
    case 0:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 49);

        infoDisplay->ivsText.line = 6;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 7;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 1:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 50);

        infoDisplay->ivsText.line = 6;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 7;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 2:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 51);

        infoDisplay->ivsText.line = 6;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 7;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 3:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 52);

        infoDisplay->ivsText.line = 8;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 9;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 4:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 53);

        infoDisplay->ivsText.line = 8;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 9;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 5:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 54);

        infoDisplay->ivsText.line = 8;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 9;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 6:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 55);

        infoDisplay->ivsText.line = 8;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 9;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 7:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 56);

        infoDisplay->ivsText.line = 7;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 8;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 8:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 57);

        infoDisplay->ivsText.line = 7;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 8;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 9:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 58);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 10:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 59);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 11:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 60);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 12:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 61);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 13:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 62);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 14:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializePokemonMetInfoString(infoDisplay, 63);

        infoDisplay->ivsText.line = 9;
        InitializeIVsString(infoDisplay);
        break;
    case 15:
        infoDisplay->natureText.line = 1;
        InitializeNatureRelatedString(infoDisplay);

        infoDisplay->metInfoText.line = 2;
        InitializeAlternateMetInfoString(infoDisplay, 64);

        infoDisplay->ivsText.line = 6;
        InitializeIVsString(infoDisplay);

        infoDisplay->flavorText.line = 7;
        InitializeFlavorAffinityString(infoDisplay);
        break;
    case 16:
        infoDisplay->metInfoText.line = 1;
        InitializeSpecialMetInfoString(infoDisplay, 101, 0);

        infoDisplay->friendshipText.line = 6;
        InitializeFriendshipLevelString(infoDisplay);
        break;
    case 17:
        infoDisplay->metInfoText.line = 1;
        InitializeSpecialMetInfoString(infoDisplay, 102, 1);

        infoDisplay->friendshipText.line = 6;
        InitializeFriendshipLevelString(infoDisplay);
        break;
    case 18:
        infoDisplay->metInfoText.line = 1;
        InitializeSpecialMetInfoString(infoDisplay, 103, 0);

        infoDisplay->friendshipText.line = 6;
        InitializeFriendshipLevelString(infoDisplay);
        break;
    case 19:
        infoDisplay->metInfoText.line = 1;
        InitializeSpecialMetInfoString(infoDisplay, 103, 1);

        infoDisplay->friendshipText.line = 6;
        InitializeFriendshipLevelString(infoDisplay);
        break;
    case 20:
        infoDisplay->metInfoText.line = 1;
        InitializeSpecialMetInfoString(infoDisplay, 104, 0);

        infoDisplay->friendshipText.line = 6;
        InitializeFriendshipLevelString(infoDisplay);
        break;
    }

    return infoDisplay;
}

void PokemonInfoDisplay_Free(PokemonInfoDisplayStruct *infoDisplay)
{
    if (infoDisplay->natureText.text != NULL) {
        Heap_Free(infoDisplay->natureText.text);
    }

    if (infoDisplay->metInfoText.text != NULL) {
        Heap_Free(infoDisplay->metInfoText.text);
    }

    if (infoDisplay->ivsText.text != NULL) {
        Heap_Free(infoDisplay->ivsText.text);
    }

    if (infoDisplay->flavorText.text != NULL) {
        Heap_Free(infoDisplay->flavorText.text);
    }

    if (infoDisplay->friendshipText.text != NULL) {
        Heap_Free(infoDisplay->friendshipText.text);
    }

    StringTemplate_Free(infoDisplay->stringTemplate);
    MessageLoader_Free(infoDisplay->messageLoader);
    Heap_Free(infoDisplay);
}

// Loads the nature name (message IDs 24-48) for the Pokémon's nature.
static void InitializeNatureRelatedString(PokemonInfoDisplayStruct *infoDisplay)
{
    int nature = Pokemon_GetNature(infoDisplay->mon);

    if (nature > 24) {
        return;
    }

    infoDisplay->natureText.text = String_Init(((2 * 18) * 2), infoDisplay->heapID);
    MessageLoader_GetString(infoDisplay->messageLoader, (24 + nature), infoDisplay->natureText.text);
}

// Formats the standard met/hatch info line: met date and location, met level,
// and the egg-received date and location.
static void InitializePokemonMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID)
{
    String *template = String_Init((((2 * 18) * 2) * 8), infoDisplay->heapID);

    infoDisplay->metInfoText.text = String_Init((((2 * 18) * 2) * 8), infoDisplay->heapID);

    MessageLoader_GetString(infoDisplay->messageLoader, messageID, template);
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 0, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_YEAR, NULL), 2, 2, 1);
    StringTemplate_SetMonthName(infoDisplay->stringTemplate, 1, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_MONTH, NULL));
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 2, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_DAY, NULL), 2, 0, 1);
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 3, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_LEVEL, NULL), 3, 0, 1);
    StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_LOCATION, NULL));
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 5, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_YEAR, NULL), 2, 2, 1);
    StringTemplate_SetMonthName(infoDisplay->stringTemplate, 6, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_MONTH, NULL));
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 7, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_DAY, NULL), 2, 0, 1);
    StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 8, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_LOCATION, NULL));
    StringTemplate_Format(infoDisplay->stringTemplate, infoDisplay->metInfoText.text, template);
    String_Free(template);
}

// Formats the met info line for a Pokémon transferred from another game: the
// met location is replaced by the name of the origin game.
static void InitializeAlternateMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID)
{
    String *template = String_Init((((2 * 18) * 2) * 4), infoDisplay->heapID);

    infoDisplay->metInfoText.text = String_Init((((2 * 18) * 2) * 4), infoDisplay->heapID);

    MessageLoader_GetString(infoDisplay->messageLoader, messageID, template);
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 0, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_YEAR, NULL), 2, 2, 1);
    StringTemplate_SetMonthName(infoDisplay->stringTemplate, 1, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_MONTH, NULL));
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 2, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_DAY, NULL), 2, 0, 1);
    StringTemplate_SetNumber(infoDisplay->stringTemplate, 3, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_LEVEL, NULL), 3, 0, 1);

    switch (Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_GAME, NULL)) {
    default:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 7)));
        break;
    case VERSION_FIRERED:
    case VERSION_LEAFGREEN:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 3)));
        break;
    case VERSION_HEARTGOLD:
    case VERSION_SOULSILVER:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 4)));
        break;
    case VERSION_RUBY:
    case VERSION_SAPPHIRE:
    case VERSION_EMERALD:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 5)));
        break;
    case VERSION_GAMECUBE:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 8)));
        break;
    case VERSION_DIAMOND:
    case VERSION_PEARL:
    case VERSION_PLATINUM:
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 4, (SpecialMetLoc_GetId(1, 7)));
        break;
    }

    StringTemplate_Format(infoDisplay->stringTemplate, infoDisplay->metInfoText.text, template);
    String_Free(template);
}

// Formats the met info line for an egg or a hatched egg. When useEggInfo is
// false the egg-received date/location is shown, otherwise the met date/location.
static void InitializeSpecialMetInfoString(PokemonInfoDisplayStruct *infoDisplay, int messageID, int useEggInfo)
{
    String *template = String_Init((((2 * 18) * 2) * 5), infoDisplay->heapID);

    infoDisplay->metInfoText.text = String_Init((((2 * 18) * 2) * 5), infoDisplay->heapID);

    MessageLoader_GetString(infoDisplay->messageLoader, messageID, template);

    if (useEggInfo == 0) {
        StringTemplate_SetNumber(infoDisplay->stringTemplate, 5, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_YEAR, NULL), 2, 2, 1);
        StringTemplate_SetMonthName(infoDisplay->stringTemplate, 6, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_MONTH, NULL));
        StringTemplate_SetNumber(infoDisplay->stringTemplate, 7, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_DAY, NULL), 2, 0, 1);
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 8, Pokemon_GetValue(infoDisplay->mon, MON_DATA_EGG_LOCATION, NULL));
    } else {
        StringTemplate_SetNumber(infoDisplay->stringTemplate, 5, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_YEAR, NULL), 2, 2, 1);
        StringTemplate_SetMonthName(infoDisplay->stringTemplate, 6, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_MONTH, NULL));
        StringTemplate_SetNumber(infoDisplay->stringTemplate, 7, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_DAY, NULL), 2, 0, 1);
        StringTemplate_SetMetLocationName(infoDisplay->stringTemplate, 8, Pokemon_GetValue(infoDisplay->mon, MON_DATA_MET_LOCATION, NULL));
    }

    StringTemplate_Format(infoDisplay->stringTemplate, infoDisplay->metInfoText.text, template);
    String_Free(template);
}

// Message IDs for the IV judge text, indexed by [best stat][rating tier].
static const u16 sIVRatingMessages[6][5] = {
    { 0x47, 0x48, 0x49, 0x4A, 0x4B },
    { 0x4C, 0x4D, 0x4E, 0x4F, 0x50 },
    { 0x51, 0x52, 0x53, 0x54, 0x55 },
    { 0x56, 0x57, 0x58, 0x59, 0x5A },
    { 0x5B, 0x5C, 0x5D, 0x5E, 0x5F },
    { 0x60, 0x61, 0x62, 0x63, 0x64 }
};

// Finds the Pokémon's highest IV and loads the matching IV judge message. The
// starting stat is derived from the personality value so that ties are broken
// consistently.
static void InitializeIVsString(PokemonInfoDisplayStruct *infoDisplay)
{
    int ivs[6], bestStat, bestIV;
    int messageID, rating;

    infoDisplay->ivsText.text = String_Init(((2 * 18) * 2), infoDisplay->heapID);

    ivs[0] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_HP_IV, NULL));
    ivs[1] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_ATK_IV, NULL));
    ivs[2] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_DEF_IV, NULL));
    ivs[3] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_SPEED_IV, NULL));
    ivs[4] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_SPATK_IV, NULL));
    ivs[5] = (Pokemon_GetValue(infoDisplay->mon, MON_DATA_SPDEF_IV, NULL));

    switch (Pokemon_GetValue(infoDisplay->mon, MON_DATA_PERSONALITY, NULL) % 6) {
    default:
    case 0:
        bestStat = 0;
        bestIV = ivs[0];

        if (bestIV < ivs[1]) {
            bestStat = 1;
            bestIV = ivs[1];
        }

        if (bestIV < ivs[2]) {
            bestStat = 2;
            bestIV = ivs[2];
        }

        if (bestIV < ivs[3]) {
            bestStat = 3;
            bestIV = ivs[3];
        }

        if (bestIV < ivs[4]) {
            bestStat = 4;
            bestIV = ivs[4];
        }

        if (bestIV < ivs[5]) {
            bestStat = 5;
            bestIV = ivs[5];
        }
        break;
    case 1:
        bestStat = 1;
        bestIV = ivs[1];

        if (bestIV < ivs[2]) {
            bestStat = 2;
            bestIV = ivs[2];
        }

        if (bestIV < ivs[3]) {
            bestStat = 3;
            bestIV = ivs[3];
        }

        if (bestIV < ivs[4]) {
            bestStat = 4;
            bestIV = ivs[4];
        }

        if (bestIV < ivs[5]) {
            bestStat = 5;
            bestIV = ivs[5];
        }

        if (bestIV < ivs[0]) {
            bestStat = 0;
            bestIV = ivs[0];
        }
        break;
    case 2:
        bestStat = 2;
        bestIV = ivs[2];

        if (bestIV < ivs[3]) {
            bestStat = 3;
            bestIV = ivs[3];
        }

        if (bestIV < ivs[4]) {
            bestStat = 4;
            bestIV = ivs[4];
        }

        if (bestIV < ivs[5]) {
            bestStat = 5;
            bestIV = ivs[5];
        }

        if (bestIV < ivs[0]) {
            bestStat = 0;
            bestIV = ivs[0];
        }

        if (bestIV < ivs[1]) {
            bestStat = 1;
            bestIV = ivs[1];
        }
        break;
    case 3:
        bestStat = 3;
        bestIV = ivs[3];

        if (bestIV < ivs[4]) {
            bestStat = 4;
            bestIV = ivs[4];
        }

        if (bestIV < ivs[5]) {
            bestStat = 5;
            bestIV = ivs[5];
        }

        if (bestIV < ivs[0]) {
            bestStat = 0;
            bestIV = ivs[0];
        }

        if (bestIV < ivs[1]) {
            bestStat = 1;
            bestIV = ivs[1];
        }

        if (bestIV < ivs[2]) {
            bestStat = 2;
            bestIV = ivs[2];
        }
        break;
    case 4:
        bestStat = 4;
        bestIV = ivs[4];

        if (bestIV < ivs[5]) {
            bestStat = 5;
            bestIV = ivs[5];
        }

        if (bestIV < ivs[0]) {
            bestStat = 0;
            bestIV = ivs[0];
        }

        if (bestIV < ivs[1]) {
            bestStat = 1;
            bestIV = ivs[1];
        }

        if (bestIV < ivs[2]) {
            bestStat = 2;
            bestIV = ivs[2];
        }

        if (bestIV < ivs[3]) {
            bestStat = 3;
            bestIV = ivs[3];
        }
        break;
    case 5:
        bestStat = 5;
        bestIV = ivs[5];

        if (bestIV < ivs[0]) {
            bestStat = 0;
            bestIV = ivs[0];
        }

        if (bestIV < ivs[1]) {
            bestStat = 1;
            bestIV = ivs[1];
        }

        if (bestIV < ivs[2]) {
            bestStat = 2;
            bestIV = ivs[2];
        }

        if (bestIV < ivs[3]) {
            bestStat = 3;
            bestIV = ivs[3];
        }

        if (bestIV < ivs[4]) {
            bestStat = 4;
            bestIV = ivs[4];
        }
        break;
    }

    rating = sIVRatingMessages[bestStat][(bestIV % 5)];
    MessageLoader_GetString(infoDisplay->messageLoader, rating, infoDisplay->ivsText.text);
}

// Message IDs for the flavor preference text; index 0 means no preference.
static const u16 sFlavorAffinityMessages[6] = {
    0x46,
    0x41,
    0x42,
    0x43,
    0x44,
    0x45
};

// Loads the flavor preference text for the Pokémon's highest flavor affinity.
static void InitializeFlavorAffinityString(PokemonInfoDisplayStruct *infoDisplay)
{
    int flavor, affinity, messageID;

    infoDisplay->flavorText.text = String_Init(((2 * 18) * 2), infoDisplay->heapID);
    affinity = 0;

    for (flavor = 0; flavor < 5; flavor++) {
        if (Pokemon_GetFlavorAffinity(infoDisplay->mon, flavor) == 1) {
            affinity = flavor + 1;
        }
    }

    messageID = sFlavorAffinityMessages[affinity];
    MessageLoader_GetString(infoDisplay->messageLoader, messageID, infoDisplay->flavorText.text);
}

// Loads the friendship text for the Pokémon's current friendship level.
static void InitializeFriendshipLevelString(PokemonInfoDisplayStruct *infoDisplay)
{
    int friendship = Pokemon_GetValue(infoDisplay->mon, MON_DATA_FRIENDSHIP, NULL);
    int messageID;

    infoDisplay->friendshipText.text = String_Init((((2 * 18) * 2) * 4), infoDisplay->heapID);

    if (friendship <= 5) {
        messageID = 105;
    } else if (friendship <= 10) {
        messageID = 106;
    } else if (friendship <= 40) {
        messageID = 107;
    } else {
        messageID = 108;
    }

    MessageLoader_GetString(infoDisplay->messageLoader, messageID, infoDisplay->friendshipText.text);
}

// Classifies the Pokémon to decide which met-info message and layout to use.
// The result distinguishes regular met Pokémon, hatched eggs, eggs, fateful
// encounters, and Pokémon obtained from special locations, with separate codes
// depending on whether the OT matches the player.
static int DeterminePokemonStatus(Pokemon *mon, BOOL monOTMatches, int heapID)
{
    int status = 0;

    if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) == 0) {
        if (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == 0) {
            if (Pokemon_GetValue(mon, MON_DATA_MET_LOCATION, NULL) == (SpecialMetLoc_GetId(0, 55))) {
                status = 15;
            } else if (Pokemon_GetValue(mon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1) {
                if (monOTMatches == 1) {
                    status = 7;
                } else {
                    status = 8;
                }
            } else if (Pokemon_GetValue(mon, MON_DATA_MET_LOCATION, NULL) == (SpecialMetLoc_GetId(1, 1))) {
                status = 2;
            } else {
                if (monOTMatches == 1) {
                    status = 0;
                } else {
                    status = 1;
                }
            }
        } else {
            if (Pokemon_GetValue(mon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1) {
                if (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 2)) {
                    if (monOTMatches == 1) {
                        status = 13;
                    } else {
                        status = 14;
                    }
                } else if (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(2, 1)) {
                    if (monOTMatches == 1) {
                        status = 11;
                    } else {
                        status = 12;
                    }
                } else {
                    if (monOTMatches == 1) {
                        status = 9;
                    } else {
                        status = 10;
                    }
                }
            } else {
                if ((Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 1)) || (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 0)) || (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 9)) || (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 10)) || (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == SpecialMetLoc_GetId(1, 11))) {
                    if (monOTMatches == 1) {
                        status = 5;
                    } else {
                        status = 6;
                    }
                } else {
                    if (monOTMatches == 1) {
                        status = 3;
                    } else {
                        status = 4;
                    }
                }
            }
        }
    } else {
        if (monOTMatches == 1) {
            if (Pokemon_GetValue(mon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1) {
                if (Pokemon_GetValue(mon, MON_DATA_EGG_LOCATION, NULL) == (SpecialMetLoc_GetId(2, 1))) {
                    status = 20;
                } else {
                    status = 18;
                }
            } else {
                status = 16;
            }
        } else {
            if (Pokemon_GetValue(mon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1) {
                status = 19;
            } else {
                status = 17;
            }
        }
    }

    return status;
}

void UpdateMonStatusAndTrainerInfo(Pokemon *mon, TrainerInfo *trainerInfo, int sel, int metLocation, enum HeapID heapID)
{
    UpdateBoxMonStatusAndTrainerInfo(&mon->box, trainerInfo, sel, metLocation, heapID);
}

// Updates a BoxPokemon's met/egg location, date, level, and OT info according to
// the acquisition context selected by sel (e.g. wild encounter, trade, egg).
void UpdateBoxMonStatusAndTrainerInfo(BoxPokemon *boxMon, TrainerInfo *trainerInfo, int sel, int metLocation, enum HeapID heapID)
{
    switch (sel) {
    case 0:
        if (metLocation > (SpecialMetLoc_GetId(1, 0))) {
            metLocation = (SpecialMetLoc_GetId(2, 2));
        }

        if (BoxPokemon_GetValue(boxMon, MON_DATA_IS_EGG, NULL) == FALSE) {
            BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
            BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, TRUE);
            BoxPokemon_SetMetLevelToCurrentLevel(boxMon);
        } else {
            BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, FALSE);
            BoxPokemon_ResetMetLocationAndDate(boxMon, TRUE);
        }

        AssignTrainerInfoToBoxPokemon(boxMon, trainerInfo, heapID);
        break;
    case 1:
        if (BoxPokemon_GetValue(boxMon, MON_DATA_IS_EGG, NULL) == FALSE) {
            BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
            BoxPokemon_SetMetLocationAndDate(boxMon, (SpecialMetLoc_GetId(1, 1)), TRUE);
            BoxPokemon_SetMetLevelToCurrentLevel(boxMon);
        } else {
            BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
            BoxPokemon_SetMetLocationAndDate(boxMon, (SpecialMetLoc_GetId(1, 1)), TRUE);
        }
        break;
    case 2:
        BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
        BoxPokemon_SetMetLocationAndDate(boxMon, (SpecialMetLoc_GetId(0, 55)), TRUE);
        BoxPokemon_SetMetLevelToCurrentLevel(boxMon);
        break;
    case 3:
        BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, FALSE);
        BoxPokemon_ResetMetLocationAndDate(boxMon, TRUE);
        AssignTrainerInfoToBoxPokemon(boxMon, trainerInfo, heapID);
        break;
    case 4:
        if (BoxPokemon_BelongsToPlayer(boxMon, trainerInfo, heapID) == 1) {
            if (BoxPokemon_GetValue(boxMon, MON_DATA_IS_EGG, NULL) == FALSE) {
                BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
                BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, TRUE);
                BoxPokemon_SetMetLevelToCurrentLevel(boxMon);
            } else {
                BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, FALSE);
                BoxPokemon_ResetMetLocationAndDate(boxMon, TRUE);
            }
        } else {
            if (BoxPokemon_GetValue(boxMon, MON_DATA_IS_EGG, NULL) == FALSE) {
                BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
                BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, TRUE);
                BoxPokemon_SetMetLevelToCurrentLevel(boxMon);
            } else {
                BoxPokemon_ResetMetLocationAndDate(boxMon, FALSE);
                BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, TRUE);
            }
        }

        BoxPokemon_SetFatefulEncounterFlag(boxMon);
        break;
    case 5:
        if (BoxPokemon_GetValue(boxMon, MON_DATA_IS_EGG, NULL) == 0) {
            (void)0;
        } else {
            BoxPokemon_SetMetLocationAndDate(boxMon, (SpecialMetLoc_GetId(1, 2)), TRUE);
        }
        break;
    case 6:
        if (metLocation > (SpecialMetLoc_GetId(1, 0))) {
            metLocation = (SpecialMetLoc_GetId(2, 2));
        }

        if (BoxPokemon_BelongsToPlayer(boxMon, trainerInfo, heapID) == 0) {
            {
                int value;

                value = BoxPokemon_GetValue(boxMon, MON_DATA_MET_LOCATION, NULL);
                BoxPokemon_SetValue(boxMon, MON_DATA_EGG_LOCATION, &value);

                value = BoxPokemon_GetValue(boxMon, MON_DATA_MET_YEAR, NULL);
                BoxPokemon_SetValue(boxMon, MON_DATA_EGG_YEAR, &value);

                value = BoxPokemon_GetValue(boxMon, MON_DATA_MET_MONTH, NULL);
                BoxPokemon_SetValue(boxMon, MON_DATA_EGG_MONTH, &value);

                value = BoxPokemon_GetValue(boxMon, MON_DATA_MET_DAY, NULL);
                BoxPokemon_SetValue(boxMon, MON_DATA_EGG_DAY, &value);
            }
        }

        BoxPokemon_SetMetLocationAndDate(boxMon, metLocation, TRUE);
        AssignTrainerInfoToBoxPokemon(boxMon, trainerInfo, heapID);
        break;
    }
}

// Copies the trainer's ID, gender, and name into the Pokémon's OT fields.
static void AssignTrainerInfoToBoxPokemon(BoxPokemon *boxMon, TrainerInfo *trainerInfo, enum HeapID heapID)
{
    int trainerID = TrainerInfo_ID(trainerInfo);
    int gender = TrainerInfo_Gender(trainerInfo);
    String *name = TrainerInfo_NameNewString(trainerInfo, heapID);

    BoxPokemon_SetValue(boxMon, MON_DATA_OT_ID, &trainerID);
    BoxPokemon_SetValue(boxMon, MON_DATA_OT_GENDER, &gender);
    BoxPokemon_SetValue(boxMon, MON_DATA_OT_NAME_STRING, name);
    String_Free(name);
}

// Writes metLocation and the current date into either the egg fields (isHatch
// false) or the met fields (isHatch true).
static void BoxPokemon_SetMetLocationAndDate(BoxPokemon *boxMon, int metLocation, int isHatch)
{
    RTCDate date;

    GetCurrentDate(&date);

    if (isHatch == FALSE) {
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_LOCATION, &metLocation);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_YEAR, &date.year);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_MONTH, &date.month);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_DAY, &date.day);
    } else {
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_LOCATION, &metLocation);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_YEAR, &date.year);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_MONTH, &date.month);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_DAY, &date.day);
    }
}

// Clears either the egg fields (isHatch false) or the met fields (isHatch true).
static void BoxPokemon_ResetMetLocationAndDate(BoxPokemon *boxMon, int isHatch)
{
    int value = 0;

    if (isHatch == FALSE) {
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_LOCATION, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_YEAR, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_MONTH, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_EGG_DAY, &value);
    } else {
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_LOCATION, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_YEAR, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_MONTH, &value);
        BoxPokemon_SetValue(boxMon, MON_DATA_MET_DAY, &value);
    }
}

// Records the Pokémon's current level as the level it was met at.
static void BoxPokemon_SetMetLevelToCurrentLevel(BoxPokemon *boxMon)
{
    int level = BoxPokemon_GetValue(boxMon, MON_DATA_LEVEL, NULL);
    BoxPokemon_SetValue(boxMon, MON_DATA_MET_LEVEL, &level);
}

// Marks the Pokémon as a fateful encounter.
static void BoxPokemon_SetFatefulEncounterFlag(BoxPokemon *boxMon)
{
    int fatefulEncounter = TRUE;
    BoxPokemon_SetValue(boxMon, MON_DATA_FATEFUL_ENCOUNTER, &fatefulEncounter);
}
