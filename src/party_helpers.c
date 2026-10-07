#include "party_helpers.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle/condition.h"
#include "constants/field_poison.h"
#include "constants/heap.h"
#include "constants/items.h"

#include "overlay005/daycare.h"

#include "heap.h"
#include "party.h"
#include "pokemon.h"
#include "save_catchrecords.h"
#include "save_player.h"
#include "savedata.h"
#include "trainer_info.h"
#include "special_met_location.h"

// A Pokémon can battle if it has any HP left and is not still an egg.
BOOL Pokemon_CanBattle(Pokemon *mon)
{
    // this can be simplified further, but it won't match
    if (Pokemon_GetValue(mon, MON_DATA_HP, NULL) == 0) {
        return FALSE;
    }

    return !Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
}

// Creates a Pokémon of the given species and level, gives it the requested
// held item and met data, and adds it to the party. Returns whether the mon
// was successfully added, recording the catch only in that case.
BOOL Pokemon_GiveMonFromScript(enum HeapID heapID, SaveData *saveData, u16 species, u8 level, u16 heldItem, int metLocation, int metTerrain)
{
    BOOL result;
    Pokemon *mon;
    u32 item;
    Party *party;
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);

    party = SaveData_GetParty(saveData);
    mon = Pokemon_New(heapID);

    Pokemon_Init(mon);
    Pokemon_InitWith(mon, species, level, INIT_IVS_RANDOM, FALSE, 0, OTID_NOT_SET, 0);
    Pokemon_SetCatchData(mon, trainerInfo, ITEM_POKE_BALL, metLocation, metTerrain, heapID);

    item = heldItem;
    Pokemon_SetValue(mon, MON_DATA_HELD_ITEM, &item);
    result = Party_AddPokemon(party, mon);

    if (result) {
        SaveData_UpdateCatchRecords(saveData, mon);
    }

    Heap_Free(mon);

    return result;
}

// Creates an egg of the given species and adds it to the party. Used to hand
// out eggs from scripts, such as the Manaphy egg granted by a Mystery Gift.
// The met/egg location is built from a base type and an offset into that
// type's table. Returns whether the egg was added.
BOOL Pokemon_GiveEggFromScript(int unused, SaveData *saveData, u16 species, u8 eggLocation, int metLocationBase, int metLocationOffset)
{
    int specialMetLocation;
    BOOL result;
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);
    Party *party = SaveData_GetParty(saveData);
    Pokemon *mon = Pokemon_New(HEAP_ID_FIELD3);

    Pokemon_Init(mon);

    specialMetLocation = SpecialMetLoc_GetId(metLocationBase, metLocationOffset);
    Egg_CreateEgg(mon, species, eggLocation, trainerInfo, 4, specialMetLocation);

    result = Party_AddPokemon(party, mon);
    Heap_Free(mon);

    return result;
}

// Overwrites the move in the given move slot of the given party member.
void Party_ResetMonMoveSlot(Party *party, int partySlot, int moveSlot, u16 moveID)
{
    Pokemon_ResetMoveSlot(Party_GetPokemonBySlotIndex(party, partySlot), moveID, moveSlot);
}

// In many of the functions below, C99-style iterator declaration doesn't match

// Returns the slot index of the first non-egg party member that knows the
// given move, or PARTY_SLOT_NONE if none does.
int Party_HasMonWithMove(Party *party, u16 moveID)
{
    int i;
    int partyCount = Party_GetCurrentCount(party);

    for (i = 0; i < partyCount; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) != FALSE) {
            continue;
        }

        if (Pokemon_GetValue(mon, MON_DATA_MOVE1, NULL) == moveID
            || Pokemon_GetValue(mon, MON_DATA_MOVE2, NULL) == moveID
            || Pokemon_GetValue(mon, MON_DATA_MOVE3, NULL) == moveID
            || Pokemon_GetValue(mon, MON_DATA_MOVE4, NULL) == moveID) {
            return i;
        }
    }

    return PARTY_SLOT_NONE;
}

// Counts the party members that can battle (see Pokemon_CanBattle).
int Party_AliveMonsCount(const Party *party)
{
    int i;
    int partyCount = Party_GetCurrentCount(party);
    int count = 0;

    for (i = 0; i < partyCount; i++) {
        if (Pokemon_CanBattle(Party_GetPokemonBySlotIndex(party, i))) {
            count++;
        }
    }

    return count;
}

// Returns the first party member that can battle, asserting if the party has
// no eligible battler at all.
Pokemon *Party_FindFirstEligibleBattler(const Party *party)
{
    int i;
    int partyCount = Party_GetCurrentCount(party);

    for (i = 0; i < partyCount; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_CanBattle(mon)) {
            return mon;
        }
    }

    GF_ASSERT(FALSE);
    return NULL;
}

// Returns the first hatched (non-egg) party member, or NULL if every slot
// still holds an egg.
Pokemon *Party_FindFirstHatchedMon(const Party *party)
{
    u16 i;
    u16 partyCount = Party_GetCurrentCount(party);

    for (i = 0; i < partyCount; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) == FALSE) {
            return mon;
        }
    }

    return NULL;
}

// Returns whether at least two party members can battle.
BOOL Party_HasTwoAliveMons(const Party *party)
{
    return Party_AliveMonsCount(party) >= 2;
}

// Awards the Sinnoh Champion Ribbon to every non-egg party member.
void Party_GiveChampionRibbons(Party *party)
{
    int i;
    u8 championRibbon = TRUE;
    int partyCount = Party_GetCurrentCount(party);

    for (i = 0; i < partyCount; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) == FALSE) {
            Pokemon_SetValue(mon, MON_DATA_SINNOH_CHAMP_RIBBON, &championRibbon);
        }
    }
}

// Applies overworld poison damage: each poisoned, battle-able party member
// loses 1 HP, never dropping below 1. A member left at exactly 1 HP is
// reported as fainted and gains friendship for surviving the poison.
// Returns FLDPSN_FAINTED if any member reached 1 HP, otherwise FLDPSN_POISONED
// if any were poisoned, otherwise FLDPSN_NONE.
int Pokemon_DoPoisonDamage(Party *party, u16 mapLabelTextID)
{
    int numPoisoned = 0;
    int numFainted = 0;
    int i, partyCount = Party_GetCurrentCount(party);
    Pokemon *mon;

    for (i = 0; i < partyCount; i++) {
        mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_CanBattle(mon)
            && (Pokemon_GetValue(mon, MON_DATA_STATUS, NULL) & (MON_CONDITION_TOXIC | MON_CONDITION_POISON))) {
            u32 hp = Pokemon_GetValue(mon, MON_DATA_HP, NULL);

            if (hp > 1) {
                hp--;
            }

            Pokemon_SetValue(mon, MON_DATA_HP, &hp);

            if (hp == 1) {
                numFainted++;
                Pokemon_UpdateFriendship(mon, FRIENDSHIP_EVENT_POISON_SURVIVE, mapLabelTextID);
            }

            numPoisoned++;
        }
    }

    if (numFainted) {
        return FLDPSN_FAINTED;
    } else if (numPoisoned) {
        return FLDPSN_POISONED;
    } else {
        return FLDPSN_NONE;
    }
}

// If the Pokémon is poisoned and down to exactly 1 HP, it survives the poison:
// its status condition is cleared and TRUE is returned. Otherwise returns
// FALSE.
BOOL Pokemon_TrySurvivePoison(Pokemon *mon)
{
    if (Pokemon_GetValue(mon, MON_DATA_STATUS, NULL) & (MON_CONDITION_TOXIC | MON_CONDITION_POISON)
        && Pokemon_GetValue(mon, MON_DATA_HP, NULL) == 1) {
        u32 condition = MON_CONDITION_NONE;

        Pokemon_SetValue(mon, MON_DATA_STATUS, &condition);
        return TRUE;
    }

    return FALSE;
}
