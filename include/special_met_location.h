#ifndef POKEPLATINUM_SPECIAL_MET_LOCATION_H
#define POKEPLATINUM_SPECIAL_MET_LOCATION_H

int SpecialMetLoc_GetType(u32 location);
int SpecialMetLoc_GetOffset(u32 location);
int SpecialMetLoc_GetId(int baseValue, int modifier);
BOOL SpecialMetLoc_IsValidId(u16 location);

#endif // POKEPLATINUM_SPECIAL_MET_LOCATION_H
