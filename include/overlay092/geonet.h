#ifndef POKEPLATINUM_GEONET_H
#define POKEPLATINUM_GEONET_H

#include "overlay_manager.h"
#include "string_gf.h"

int Geonet_Init(ApplicationManager *appMan, int *state);
int Geonet_Main(ApplicationManager *appMan, int *state);
int Geonet_Exit(ApplicationManager *appMan, int *state);
BOOL Geonet_GetCountryAndRegionNames(int country, int region, String *countryName, String *regionName, enum HeapID heapID);
BOOL Geonet_CountryHasRegions(int country);

#endif // POKEPLATINUM_GEONET_H
