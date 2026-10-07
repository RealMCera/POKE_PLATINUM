#include "battle_tower_partner.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle_tower.h"
#include "constants/string.h"
#include "generated/battle_tower_modes.h"
#include "generated/frontier_trainers.h"
#include "generated/object_events_gfx.h"
#include "generated/species_data_params.h"
#include "generated/trainer_classes.h"

#include "struct_defs/battle_tower.h"
#include "struct_defs/frontier_pokemon_base.h"
#include "struct_defs/frontier_trainer_base.h"
#include "struct_defs/wifi_battle_tower_data.h"

#include "field/field_system.h"

#include "assert.h"
#include "communication_system.h"
#include "flags.h"
#include "heap.h"
#include "message.h"
#include "narc.h"
#include "party.h"
#include "pokedex.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "string_gf.h"
#include "string_template.h"
#include "trainer_info.h"
#include "battle_tower.h"
#include "wifi_battle_tower_save.h"

static BOOL BattleTower_SelectPartnerPokemon(BattleTower *battleTower, FrontierTrainerBase *trainerData, u16 partnerBattleTowerID, FrontierPokemon *mons, u8 partySize, u16 *partnerSpecies, u16 *partnerItems, BattleTowerPartnerData *partnerData, enum HeapID heapID);
static void *BattleTower_GetTrainerBase(u16 trainerID, int heapID);
static void BattleTower_GetPokemonBase(FrontierPokemonBase *monData, int narcIdx);

// Maps a trainer class to the overworld sprite used for that class. The first
// entry is the fallback for classes not listed here.
static const u16 sTrainerClassToObjectID[][2] = {
    { TRAINER_CLASS_TRAINER_CHERYL, OBJ_EVENT_GFX_CHERYL },
    { TRAINER_CLASS_TRAINER_RILEY, OBJ_EVENT_GFX_RILEY },
    { TRAINER_CLASS_TRAINER_MARLEY, OBJ_EVENT_GFX_MARLEY },
    { TRAINER_CLASS_TRAINER_BUCK, OBJ_EVENT_GFX_BUCK },
    { TRAINER_CLASS_TRAINER_MIRA, OBJ_EVENT_GFX_MIRA },
    { TRAINER_CLASS_YOUNGSTER, OBJ_EVENT_GFX_YOUNGSTER },
    { TRAINER_CLASS_LASS, OBJ_EVENT_GFX_LASS },
    { TRAINER_CLASS_SCHOOL_KID_MALE, OBJ_EVENT_GFX_SCHOOL_KID_M },
    { TRAINER_CLASS_SCHOOL_KID_FEMALE, OBJ_EVENT_GFX_SCHOOL_KID_F },
    { TRAINER_CLASS_RICH_BOY, OBJ_EVENT_GFX_RICH_BOY },
    { TRAINER_CLASS_LADY, OBJ_EVENT_GFX_LADY },
    { TRAINER_CLASS_CAMPER, OBJ_EVENT_GFX_CAMPER },
    { TRAINER_CLASS_PICNICKER, OBJ_EVENT_GFX_PICNICKER },
    { TRAINER_CLASS_TUBER_MALE, OBJ_EVENT_GFX_NINJA_BOY },
    { TRAINER_CLASS_TUBER_FEMALE, OBJ_EVENT_GFX_TWIN },
    { TRAINER_CLASS_POKEFAN_MALE, OBJ_EVENT_GFX_POKEFAN_M },
    { TRAINER_CLASS_POKEFAN_FEMALE, OBJ_EVENT_GFX_POKEFAN_F },
    { TRAINER_CLASS_WAITER, OBJ_EVENT_GFX_WAITER },
    { TRAINER_CLASS_WAITRESS, OBJ_EVENT_GFX_WAITRESS },
    { TRAINER_CLASS_BREEDER_MALE, OBJ_EVENT_GFX_POKEMON_BREEDER_M },
    { TRAINER_CLASS_BREEDER_FEMALE, OBJ_EVENT_GFX_POKEMON_BREEDER_F },
    { TRAINER_CLASS_CAMERAMAN, OBJ_EVENT_GFX_CAMERAMAN },
    { TRAINER_CLASS_REPORTER, OBJ_EVENT_GFX_REPORTER },
    { TRAINER_CLASS_RANCHER, OBJ_EVENT_GFX_RANCHER },
    { TRAINER_CLASS_COWGIRL, OBJ_EVENT_GFX_COWGIRL },
    { TRAINER_CLASS_CYCLIST_MALE, OBJ_EVENT_GFX_CYCLIST_M },
    { TRAINER_CLASS_CYCLIST_FEMALE, OBJ_EVENT_GFX_CYCLIST_F },
    { TRAINER_CLASS_BLACK_BELT, OBJ_EVENT_GFX_BLACK_BELT },
    { TRAINER_CLASS_BATTLE_GIRL, OBJ_EVENT_GFX_BATTLE_GIRL },
    { TRAINER_CLASS_VETERAN, OBJ_EVENT_GFX_EXPERT_M },
    { TRAINER_CLASS_SOCIALITE, OBJ_EVENT_GFX_SOCIALITE },
    { TRAINER_CLASS_PSYCHIC_MALE, OBJ_EVENT_GFX_PSYCHIC },
    { TRAINER_CLASS_PSYCHIC_FEMALE, OBJ_EVENT_GFX_PSYCHIC },
    { TRAINER_CLASS_RANGER_MALE, OBJ_EVENT_GFX_ACE_TRAINER_M },
    { TRAINER_CLASS_RANGER_FEMALE, OBJ_EVENT_GFX_ACE_TRAINER_F },
    { TRAINER_CLASS_ACE_TRAINER_MALE, OBJ_EVENT_GFX_ACE_TRAINER_M },
    { TRAINER_CLASS_ACE_TRAINER_FEMALE, OBJ_EVENT_GFX_ACE_TRAINER_F },
    { TRAINER_CLASS_ACE_TRAINER_SNOW_MALE, OBJ_EVENT_GFX_ACE_TRAINER_SNOW_M },
    { TRAINER_CLASS_ACE_TRAINER_SNOW_FEMALE, OBJ_EVENT_GFX_ACE_TRAINER_SNOW_F },
    { TRAINER_CLASS_DRAGON_TAMER, OBJ_EVENT_GFX_ACE_TRAINER_M },
    { TRAINER_CLASS_BUG_CATCHER, OBJ_EVENT_GFX_BUG_CATCHER },
    { TRAINER_CLASS_NINJA_BOY, OBJ_EVENT_GFX_NINJA_BOY },
    { TRAINER_CLASS_JOGGER, OBJ_EVENT_GFX_JOGGER },
    { TRAINER_CLASS_FISHERMAN, OBJ_EVENT_GFX_FISHERMAN },
    { TRAINER_CLASS_SAILOR, OBJ_EVENT_GFX_SAILOR },
    { TRAINER_CLASS_HIKER, OBJ_EVENT_GFX_HIKER },
    { TRAINER_CLASS_RUIN_MANIAC, OBJ_EVENT_GFX_RUIN_MANIAC },
    { TRAINER_CLASS_GUITARIST, OBJ_EVENT_GFX_GUITARIST },
    { TRAINER_CLASS_COLLECTOR, OBJ_EVENT_GFX_COLLECTOR },
    { TRAINER_CLASS_ROUGHNECK, OBJ_EVENT_GFX_ROUGHNECK },
    { TRAINER_CLASS_SCIENTIST, OBJ_EVENT_GFX_SCIENTIST_M },
    { TRAINER_CLASS_GENTLEMAN, OBJ_EVENT_GFX_GENTLEMAN },
    { TRAINER_CLASS_WORKER, OBJ_EVENT_GFX_WORKER },
    { TRAINER_CLASS_CLOWN, OBJ_EVENT_GFX_CLOWN },
    { TRAINER_CLASS_POLICEMAN, OBJ_EVENT_GFX_POLICEMAN },
    { TRAINER_CLASS_PI, OBJ_EVENT_GFX_RICH_BOY },
    { TRAINER_CLASS_BIRD_KEEPER, OBJ_EVENT_GFX_ACE_TRAINER_F },
    { TRAINER_CLASS_PARASOL_LADY, OBJ_EVENT_GFX_PARASOL_LADY },
    { TRAINER_CLASS_BEAUTY, OBJ_EVENT_GFX_BEAUTY },
    { TRAINER_CLASS_AROMA_LADY, OBJ_EVENT_GFX_POKEMON_BREEDER_F },
    { TRAINER_CLASS_IDOL, OBJ_EVENT_GFX_IDOL },
    { TRAINER_CLASS_ARTIST, OBJ_EVENT_GFX_ARTIST },
    { TRAINER_CLASS_POKE_KID, OBJ_EVENT_GFX_PIKACHU }
};

// Builds the message that lists every banned species the player has already
// seen, used by the Battle Frontier banlist check. `outNumBannedSeen` receives
// the number of species appended to the template.
StringTemplate *BattleFrontier_MakeSeenBanlistSpeciesMsg(SaveData *saveData, u16 numPokemonRequired, u16 unused2, u8 unused3, u8 *outNumBannedSeen)
{
    // Forward declarations required to match
    u8 i;
    u16 bannedSpecies;
    String *speciesName, *unused;
    Pokedex *pokedex;
    StringTemplate *bannedSpeciesList;
    MessageLoader *speciesNameLoader;

    speciesName = String_Init(14, HEAP_ID_FIELD1);
    unused = String_Init(2, HEAP_ID_FIELD1);
    pokedex = SaveData_GetPokedex(saveData);
    speciesNameLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_SPECIES_NAME, HEAP_ID_FIELD1);
    bannedSpeciesList = StringTemplate_New(BATTLE_FRONTIER_BANLIST_SIZE + 1, 14, HEAP_ID_FIELD1);

    StringTemplate_SetNumber(bannedSpeciesList, 0, numPokemonRequired, 1, PADDING_MODE_NONE, CHARSET_MODE_EN);

    for (i = 0; i < BATTLE_FRONTIER_BANLIST_SIZE; i++) {
        bannedSpecies = Pokemon_GetBattleFrontierBanlistEntry(i);

        if (Pokedex_HasSeenSpecies(pokedex, bannedSpecies)) {
            MessageLoader_GetString(speciesNameLoader, bannedSpecies, speciesName);
            StringTemplate_SetString(bannedSpeciesList, (*outNumBannedSeen) + 1, speciesName, unused2, unused3, GAME_LANGUAGE);
            (*outNumBannedSeen)++;
        }
    }

    MessageLoader_Free(speciesNameLoader);
    String_Free(unused);
    String_Free(speciesName);

    return bannedSpeciesList;
}

u16 BattleTower_GetObjectIDFromTrainerClass(u8 trainerClass)
{
    for (int i = 0; i < (NELEMS(sTrainerClassToObjectID)); i++) {
        if (sTrainerClassToObjectID[i][0] == trainerClass) {
            return sTrainerClassToObjectID[i][1];
        }
    }

    return OBJ_EVENT_GFX_SCHOOL_KID_M;
}

// Receives the link partner's Battle Salon data packet, laid out as
// [gender, species0, species1, roomNum]. Returns a bitmask of species
// collisions with the player's own party: bit 0 for party slot 0, bit 1 for
// party slot 1.
u16 BattleTower_ReceivePartnerData(FieldSystem *fieldSystem, const u16 *partnerData)
{
    u16 speciesOverlap = 0;
    BattleTower *battleTower = fieldSystem->battleTower;

    battleTower->partnerGender = (u8)partnerData[0];
    battleTower->unk_16[0] = partnerData[1];
    battleTower->unk_16[1] = partnerData[2];
    battleTower->unk_14 = partnerData[3];
    battleTower->partnerID = BT_PARTNERS_COUNT + battleTower->partnerGender;

    if ((battleTower->unk_2E[0] == battleTower->unk_16[0]) || (battleTower->unk_2E[0] == battleTower->unk_16[1])) {
        speciesOverlap += 1;
    }

    if ((battleTower->unk_2E[1] == battleTower->unk_16[0]) || (battleTower->unk_2E[1] == battleTower->unk_16[1])) {
        speciesOverlap += 2;
    }

    return speciesOverlap;
}

// Receives the link partner's trainer IDs. The host (net ID 0) does not accept
// them, so only the guest copies the packet into its BattleTower state.
u16 BattleTower_ReceivePartnerTrainerIDs(FieldSystem *fieldSystem, const u16 *trainerIDs)
{
    BattleTower *battleTower = fieldSystem->battleTower;

    if (CommSys_CurNetId() == 0) {
        return 0;
    }

    MI_CpuCopy8(trainerIDs, battleTower->trainerIDs, BT_OPPONENTS_COUNT * 2 * sizeof(u16));
    return 1;
}

// Returns whether the partner has signalled readiness, either locally via
// BattleTower_SetPartnerReady or through the received packet's first word.
u16 BattleTower_IsPartnerDataReady(FieldSystem *fieldSystem, const u16 *partnerData)
{
    BattleTower *battleTower = fieldSystem->battleTower;

    if (battleTower->unk_10_3 || partnerData[0]) {
        return 1;
    }

    return 0;
}

// Fills the outgoing Battle Salon packet with the player's gender, the species
// of the first two party members, and the current room number.
void BattleTower_BuildPartnerDataPacket(BattleTower *battleTower, SaveData *saveData)
{
    int i;
    Party *party;
    Pokemon *unusedPokemon;
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);

    battleTower->unk_83E[0] = TrainerInfo_Gender(trainerInfo);
    party = SaveData_GetParty(saveData);

    for (i = 0; i < 2; i++) {
        battleTower->unk_83E[1 + i] = Pokemon_GetValue(Party_GetPokemonBySlotIndex(party, battleTower->unk_2A[i]), MON_DATA_SPECIES, NULL);
    }

    battleTower->unk_83E[3] = WifiBattleTowerRecord_UpdateRoomNum(battleTower->unk_74, 3, 0);
}

// Copies the generated opponent trainer IDs into the outgoing packet.
void BattleTower_BuildTrainerIDPacket(BattleTower *battleTower)
{
    MI_CpuCopy8(battleTower->trainerIDs, battleTower->unk_83E, BT_OPPONENTS_COUNT * 2 * sizeof(u16));
}

// Marks the local partner data as ready and stores the flag in the packet.
void BattleTower_SetPartnerReady(BattleTower *battleTower, u16 ready)
{
    battleTower->unk_10_3 = ready;
    battleTower->unk_83E[0] = ready;
}

// Inclusive trainer-ID ranges used for regular opponents in each Battle Tower
// room (0-6), plus a final range for rooms beyond the seventh.
static const u16 sBattleTowerTrainerRangesPerRoom[][2] = {
    { FRONTIER_TRAINER_YOUNGSTER_JIM, FRONTIER_TRAINER_REPORTER_GINGHAM },
    { FRONTIER_TRAINER_HIKER_RAIDEN, FRONTIER_TRAINER_SOCIALITE_CARMEN },
    { FRONTIER_TRAINER_CYCLIST_GASPAR, FRONTIER_TRAINER_CLOWN_PRESCOT },
    { FRONTIER_TRAINER_PSYCHIC_ALPHA, FRONTIER_TRAINER_ACE_TRAINER_DANIELA },
    { FRONTIER_TRAINER_ACE_TRAINER_YARDLEY, FRONTIER_TRAINER_IDOL_UTAH },
    { FRONTIER_TRAINER_YOUNGSTER_KADEN, FRONTIER_TRAINER_PI_SERGEI },
    { FRONTIER_TRAINER_JOGGER_COLT, FRONTIER_TRAINER_BREEDER_ANTONIA },
    { FRONTIER_TRAINER_CAMPER_FREDDY, FRONTIER_TRAINER_IDOL_NISSA }
};

// Inclusive trainer-ID ranges used for the seventh (boss) opponent of each
// room.
static const u16 sBattleTowerBossTrainerRangesPerRoom[][2] = {
    { FRONTIER_TRAINER_CYCLIST_GASPAR, FRONTIER_TRAINER_SOCIALITE_CARMEN },
    { FRONTIER_TRAINER_PSYCHIC_ALPHA, FRONTIER_TRAINER_CLOWN_PRESCOT },
    { FRONTIER_TRAINER_ACE_TRAINER_YARDLEY, FRONTIER_TRAINER_ACE_TRAINER_DANIELA },
    { FRONTIER_TRAINER_YOUNGSTER_KADEN, FRONTIER_TRAINER_IDOL_UTAH },
    { FRONTIER_TRAINER_JOGGER_COLT, FRONTIER_TRAINER_PI_SERGEI },
    { FRONTIER_TRAINER_CAMPER_FREDDY, FRONTIER_TRAINER_BREEDER_ANTONIA },
    { FRONTIER_TRAINER_ACE_TRAINER_SAWYER, FRONTIER_TRAINER_VETERAN_ALFRED },
    { FRONTIER_TRAINER_CAMPER_FREDDY, FRONTIER_TRAINER_IDOL_NISSA }
};

// Picks a random trainer ID for the given room and opponent slot. The final
// opponent of a room is drawn from the boss range; single battles additionally
// force Palmer at the 21st and 49th battles.
u16 BattleTower_GetTrainerIDForRoomAndOpponentNum(BattleTower *battleTower, u8 roomNum, u8 opponentNum, int challengeMode)
{
    u16 trainerID;

    if (challengeMode == BATTLE_TOWER_MODE_SINGLE) {
        if (roomNum == 2 && opponentNum == BT_OPPONENTS_COUNT - 1) { // 21st battle
            return FRONTIER_TRAINER_TOWER_TYCOON_PALMER_SILVER;
        }

        if (roomNum == 6 && opponentNum == BT_OPPONENTS_COUNT - 1) { // 49th battle
            return FRONTIER_TRAINER_TOWER_TYCOON_PALMER_GOLD;
        }
    }

    if (roomNum < 7) {
        if (opponentNum == BT_OPPONENTS_COUNT - 1) {
            trainerID = sBattleTowerBossTrainerRangesPerRoom[roomNum][1] - sBattleTowerBossTrainerRangesPerRoom[roomNum][0] + 1;
            trainerID = sBattleTowerBossTrainerRangesPerRoom[roomNum][0] + (BattleTower_GetRandom(battleTower) % trainerID);
        } else {
            trainerID = sBattleTowerTrainerRangesPerRoom[roomNum][1] - sBattleTowerTrainerRangesPerRoom[roomNum][0] + 1;
            trainerID = sBattleTowerTrainerRangesPerRoom[roomNum][0] + (BattleTower_GetRandom(battleTower) % trainerID);
        }
    } else {
        trainerID = sBattleTowerTrainerRangesPerRoom[7][1] - sBattleTowerTrainerRangesPerRoom[7][0] + 1;
        trainerID = sBattleTowerTrainerRangesPerRoom[7][0] + (BattleTower_GetRandom(battleTower) % trainerID);
    }

    return trainerID;
}

// Loads a partner trainer's base data and name into `opponent`, returning the
// heap-allocated base data for the caller to free.
static FrontierTrainerBase *BattleTower_GetPartnerTrainer(FrontierOpponent *opponent, u16 trainerID, enum HeapID heapID)
{
    MessageLoader *msgLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_FRONTIER_TRAINER_NAMES, heapID);

    MI_CpuClear8(opponent, sizeof(FrontierOpponent));

    FrontierTrainerBase *trainerBase = BattleTower_GetTrainerBase(trainerID, heapID);

    opponent->trainer.trainerID = trainerID;
    opponent->trainer.introMsg[0] = 0xFFFF;
    opponent->trainer.introMsg[1] = trainerID * 3;
    opponent->trainer.trainerType = trainerBase->trainerType;

    String *trainerName = MessageLoader_GetNewString(msgLoader, trainerID);

    String_ToChars(trainerName, &opponent->trainer.trainerName[0], TRAINER_NAME_LEN + 1);
    String_Free(trainerName);
    MessageLoader_Free(msgLoader);

    return trainerBase;
}

// Held items substituted for a partner's Pokémon when the normal item would
// clash with one already chosen.
static const u16 sPartnerPokemonDefaultItems[] = {
    ITEM_BRIGHTPOWDER,
    ITEM_LUM_BERRY,
    ITEM_LEFTOVERS,
    ITEM_QUICK_CLAW
};

// Initialises one partner Pokémon from its base set data. When `personality`
// is 0 a non-shiny personality matching the base nature is generated; the
// generated (or supplied) personality is returned.
static u32 BattleTower_InitPartnerPokemon(BattleTower *battleTower, FrontierPokemon *mon, u16 narcIdx, u32 otID, u32 personality, u8 ivs, u8 itemIdx, BOOL useDefaultItem, enum HeapID heapID)
{
    int i;
    u32 generatedPersonality;
    FrontierPokemonBase baseMon;

    MI_CpuClear8(mon, sizeof(FrontierPokemon));
    BattleTower_GetPokemonBase(&baseMon, narcIdx);

    mon->species = baseMon.species;
    mon->form = baseMon.form;

    if (useDefaultItem) {
        mon->item = sPartnerPokemonDefaultItems[itemIdx];
    } else {
        mon->item = baseMon.item;
    }

    u8 friendship = MAX_FRIENDSHIP_VALUE;

    for (i = 0; i < LEARNED_MOVES_MAX; i++) {
        mon->moves[i] = baseMon.moves[i];

        if (baseMon.moves[i] == MOVE_FRUSTRATION) {
            friendship = 0;
        }
    }

    mon->otID = otID;

    if (personality == 0) {
        do {
            generatedPersonality = (BattleTower_GetRandom(battleTower) | BattleTower_GetRandom(battleTower) << 16);
        } while ((baseMon.nature != Pokemon_GetNatureOf(generatedPersonality)) || (Pokemon_IsPersonalityShiny(otID, generatedPersonality) == 1));

        mon->personality = generatedPersonality;
    } else {
        mon->personality = personality;
        generatedPersonality = personality;
    }

    mon->hpIV = ivs;
    mon->atkIV = ivs;
    mon->defIV = ivs;
    mon->speedIV = ivs;
    mon->spAtkIV = ivs;
    mon->spDefIV = ivs;

    int evCount = 0;

    for (i = 0; i < 6; i++) {
        if (baseMon.evFlags & FlagIndex(i)) {
            evCount++;
        }
    }

    // Spread the 510 total EVs evenly across the stats the set invests in.
    if ((510 / evCount) > 255) {
        evCount = 255;
    } else {
        evCount = 510 / evCount;
    }

    for (i = 0; i < 6; i++) {
        if (baseMon.evFlags & FlagIndex(i)) {
            mon->evList[i] = evCount;
        }
    }

    mon->combinedPPUps = 0;
    mon->language = gGameLanguage;

    i = SpeciesData_GetSpeciesValue(mon->species, SPECIES_DATA_ABILITY_2);

    if (i) {
        if (mon->personality & 1) {
            mon->ability = i;
        } else {
            mon->ability = SpeciesData_GetSpeciesValue(mon->species, SPECIES_DATA_ABILITY_1);
        }
    } else {
        mon->ability = SpeciesData_GetSpeciesValue(mon->species, SPECIES_DATA_ABILITY_1);
    }

    mon->friendship = friendship;
    MessageLoader_GetSpeciesName(mon->species, heapID, &(mon->nickname[0]));

    return generatedPersonality;
}

// Builds a partner's FrontierOpponent (trainer plus party) for the given
// partner ID. Returns whether the item-conflict fallback was used.
BOOL BattleTower_BuildPartnerOpponent(BattleTower *battleTower, FrontierOpponent *opponent, u16 partnerBattleTowerID, int partySize, u16 *partnerSpecies, u16 *partnerItems, BattleTowerPartnerData *partnerData, enum HeapID heapID)
{
    BOOL usedDefaultItems = FALSE;
    FrontierTrainerBase *trainerData = BattleTower_GetPartnerTrainer(opponent, partnerBattleTowerID, heapID);
    usedDefaultItems = BattleTower_SelectPartnerPokemon(battleTower, trainerData, partnerBattleTowerID, &opponent->pokemon[0], partySize, partnerSpecies, partnerItems, partnerData, heapID);

    Heap_Free(trainerData);

    return usedDefaultItems;
}

// Rebuilds a partner's FrontierOpponent from previously saved partner data,
// reusing the stored mon set IDs and personalities.
void BattleTower_LoadPartnerOpponent(BattleTower *battleTower, FrontierOpponent *opponent, u16 partnerBattleTowerID, BOOL useDefaultItems, const BattleTowerPartnerData *partnerData, enum HeapID heapID)
{
    u8 ivs = 0;
    FrontierTrainerBase *trainerData = BattleTower_GetPartnerTrainer(opponent, partnerBattleTowerID, heapID);
    ivs = BattleTower_GetIVsFromTrainerID(partnerBattleTowerID);

    for (int i = 0; i < 2; i++) {
        BattleTower_InitPartnerPokemon(battleTower, &(opponent->pokemon[i]), partnerData->monSetIDs[i], partnerData->otID, partnerData->personalities[i], ivs, i, useDefaultItems, heapID);
    }

    Heap_Free(trainerData);
}

// Selects `partySize` distinct Pokémon sets for a partner, avoiding duplicate
// species and items both within the team and against the player's party. If
// item conflicts persist after 50 attempts, the default item list is used
// instead (returned as TRUE). The chosen set IDs and personalities are written
// back to `partnerData` when it is non-NULL.
static BOOL BattleTower_SelectPartnerPokemon(BattleTower *battleTower, FrontierTrainerBase *trainerData, u16 partnerBattleTowerID, FrontierPokemon *mons, u8 partySize, u16 *partnerSpecies, u16 *partnerItems, BattleTowerPartnerData *partnerData, enum HeapID heapID)
{
    int i, unused;
    u8 ivs;
    u8 setIndex;
    u32 otID;
    int monSetID;
    int selectedMonSetIDs[4];
    u32 personalities[4];
    int numSelected;
    int itemConflictCount;
    BOOL usedDefaultItems = FALSE;
    FrontierPokemonBase existingMon;
    FrontierPokemonBase candidateMon;

    GF_ASSERT(partySize <= 4);

    numSelected = 0;
    itemConflictCount = 0;

    while (numSelected != partySize) {
        setIndex = BattleTower_GetRandom(battleTower) % trainerData->numSets;
        monSetID = trainerData->setIDs[setIndex];

        BattleTower_GetPokemonBase(&candidateMon, monSetID);

        // Reject a set whose species is already on the team.
        for (i = 0; i < numSelected; i++) {
            BattleTower_GetPokemonBase(&existingMon, selectedMonSetIDs[i]);

            if (existingMon.species == candidateMon.species) {
                break;
            }
        }

        if (i != numSelected) {
            continue;
        }

        // Reject a set whose species is in the player's party.
        if (partnerSpecies != NULL) {
            for (i = 0; i < partySize; i++) {
                if (partnerSpecies[i] == candidateMon.species) {
                    break;
                }
            }

            if (i != partySize) {
                continue;
            }
        }

        if (itemConflictCount < 50) {
            // Reject a set whose held item is already on the team.
            for (i = 0; i < numSelected; i++) {
                BattleTower_GetPokemonBase(&existingMon, selectedMonSetIDs[i]);

                if ((existingMon.item) && (existingMon.item == candidateMon.item)) {
                    break;
                }
            }

            if (i != numSelected) {
                itemConflictCount++;
                continue;
            }

            // Reject a set whose held item is in the player's party.
            if (partnerItems != NULL) {
                for (i = 0; i < partySize; i++) {
                    if ((partnerItems[i] == candidateMon.item) && (partnerItems[i] != 0)) {
                        break;
                    }
                }

                if (i != partySize) {
                    itemConflictCount++;
                    continue;
                }
            }
        }

        selectedMonSetIDs[numSelected] = monSetID;
        numSelected++;
    }

    ivs = BattleTower_GetIVsFromTrainerID(partnerBattleTowerID);
    otID = (BattleTower_GetRandom(battleTower) | (BattleTower_GetRandom(battleTower) << 16));

    if (itemConflictCount >= 50) {
        usedDefaultItems = TRUE;
    }

    for (i = 0; i < numSelected; i++) {
        personalities[i] = BattleTower_InitPartnerPokemon(battleTower, &(mons[i]), selectedMonSetIDs[i], otID, 0, ivs, i, usedDefaultItems, heapID);
    }

    if (partnerData == NULL) {
        return usedDefaultItems;
    }

    partnerData->otID = otID;

    for (i = 0; i < 2; i++) {
        partnerData->monSetIDs[i] = selectedMonSetIDs[i];
        partnerData->personalities[i] = personalities[i];
    }

    return usedDefaultItems;
}

// Reads a trainer's base data (BTDTR) from the Battle Tower NARC.
static void *BattleTower_GetTrainerBase(u16 trainerID, int heapID)
{
    return NARC_AllocAndReadWholeMemberByIndexPair(NARC_INDEX_BATTLE__B_PL_TOWER__PL_BTDTR, trainerID, heapID);
}

// Reads a Pokémon set's base data (BTDPM) from the Battle Tower NARC.
static void BattleTower_GetPokemonBase(FrontierPokemonBase *monData, int narcIdx)
{
    NARC_ReadWholeMemberByIndexPair(monData, NARC_INDEX_BATTLE__B_PL_TOWER__PL_BTDPM, narcIdx);
}
