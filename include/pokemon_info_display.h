#ifndef POKEPLATINUM_POKEMON_INFO_DISPLAY_H
#define POKEPLATINUM_POKEMON_INFO_DISPLAY_H

#include "struct_defs/struct_02090800.h"

#include "pokemon.h"
#include "trainer_info.h"

PokemonInfoDisplayStruct *PokemonInfoDisplay_New(Pokemon *mon, BOOL monOTMatches, enum HeapID heapID);
void PokemonInfoDisplay_Free(PokemonInfoDisplayStruct *infoDisplay);
void UpdateMonStatusAndTrainerInfo(Pokemon *mon, TrainerInfo *trainerInfo, int sel, int metLocation, enum HeapID heapID);
void UpdateBoxMonStatusAndTrainerInfo(BoxPokemon *boxMon, TrainerInfo *trainerInfo, int sel, int metLocation, enum HeapID heapID);

#endif // POKEPLATINUM_POKEMON_INFO_DISPLAY_H
