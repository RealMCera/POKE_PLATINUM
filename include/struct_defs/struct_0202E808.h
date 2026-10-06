#ifndef POKEPLATINUM_STRUCT_0202E808_H
#define POKEPLATINUM_STRUCT_0202E808_H

typedef struct {
    u8 dummy; // Written on reset but never read; purpose unknown.
    u8 padding_01;
    u16 species;
    u8 gender;
    u8 language;
    u8 metGame;
    u8 numPokemonCaught;
} TVSegment_SafariGameData;

#endif // POKEPLATINUM_STRUCT_0202E808_H
