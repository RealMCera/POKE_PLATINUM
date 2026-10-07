#ifndef POKEPLATINUM_STRUCT_DEF_POKEMON_INFO_DISPLAY_SUB1_H
#define POKEPLATINUM_STRUCT_DEF_POKEMON_INFO_DISPLAY_SUB1_H

#include "string_gf.h"

// One line of the trainer memo: the 1-based row it is drawn on and its text.
typedef struct PokemonInfoDisplayLine {
    int line;
    String *text;
} PokemonInfoDisplayLine;

#endif // POKEPLATINUM_STRUCT_DEF_POKEMON_INFO_DISPLAY_SUB1_H
