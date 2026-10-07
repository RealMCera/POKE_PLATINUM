#ifndef POKEPLATINUM_STRUCT_0207E060_H
#define POKEPLATINUM_STRUCT_0207E060_H

#include "constants/pokemon.h"

// Compact trainer profile exchanged with friends over Nintendo WFC. It is
// registered as the player's own status data via NintendoWFC_SetStatusData and
// received for each friend into the matching slot of UnkStruct_0207DFAC. The
// layout is fixed by the wire format, so fields must not be reordered.
typedef struct WFCTrainerInfo {
    // Species and held item of each party slot, used to preview the friend's
    // party before a trade or battle.
    u16 partySpecies[MAX_PARTY_SIZE];
    u16 partyHeldItems[MAX_PARTY_SIZE];
    // Game code of the cartridge the profile was created on.
    u8 gameCode;
    // Language of the originating save file.
    u8 language;
    // Whether the player has obtained the National Pokédex.
    u8 isNationalDexObtained;
    // Communication app state, shared so that both sides can tell whether they
    // are running a compatible activity (see ov65_0222DD20 and unk_0207DFAC.c).
    u8 commState;
    u8 unk_1C;
    // Trainer appearance and gender, used to draw the friend's sprite.
    u8 appearance;
    u8 gender;
    // Country and region reported by the WiFi history.
    u8 country;
    u8 region;
    // Current voice-chat setting, and the value it is restored from when a
    // connection is (re)established.
    u8 voiceChatEnabled;
    u8 voiceChatEnabledBackup;
} WFCTrainerInfo;

#endif // POKEPLATINUM_STRUCT_0207E060_H
