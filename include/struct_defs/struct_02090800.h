#ifndef POKEPLATINUM_STRUCT_02090800_H
#define POKEPLATINUM_STRUCT_02090800_H

#include "struct_defs/pokemon_info_display_sub1.h"

#include "message.h"
#include "pokemon.h"
#include "string_template.h"

typedef struct {
    enum HeapID heapID;
    MessageLoader *messageLoader;
    StringTemplate *stringTemplate;
    Pokemon *mon;
    BOOL monOTMatches;
    PokemonInfoDisplayLine natureText;
    PokemonInfoDisplayLine metInfoText;
    PokemonInfoDisplayLine ivsText;
    PokemonInfoDisplayLine flavorText;
    PokemonInfoDisplayLine friendshipText;
} PokemonInfoDisplayStruct;

#endif // POKEPLATINUM_STRUCT_02090800_H
