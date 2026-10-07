#include "special_met_location.h"

#include <nitro.h>
#include <string.h>

// Base value of each met-location type. A location is assigned to the first
// type whose base value is greater than it, so the ranges are:
//   type 0: 0..1999    (regular location names)
//   type 1: 2000..2999 (special met locations)
//   type 2: 3000..3999 (mystery gift event locations)
static const u16 sMetLocationTypeBaseValues[] = {
    0x0,
    0x7D0,
    0xBB8
};

// Returns the met-location type (message bank) that the given location belongs to.
int SpecialMetLoc_GetType(u32 location)
{
    int type;

    for (type = 0; type < (3 - 1); type++) {
        if (location < sMetLocationTypeBaseValues[type + 1]) {
            return type;
        }
    }

    return type;
}

// Returns the location's offset within its type's message bank.
int SpecialMetLoc_GetOffset(u32 location)
{
    int type = SpecialMetLoc_GetType(location);
    return location - sMetLocationTypeBaseValues[type];
}

// Builds a met-location value from a type and an offset within that type.
// baseValue 1 = Transfer mons and eggs
int SpecialMetLoc_GetId(int baseValue, int modifier)
{
    GF_ASSERT(baseValue < 3);
    return sMetLocationTypeBaseValues[baseValue] + modifier;
}

// Returns TRUE if the given value is a valid met/egg location.
BOOL SpecialMetLoc_IsValidId(u16 location)
{
    if (((location >= 1) && (location <= 111)) || ((location >= 2000) && (location <= 2010)) || ((location >= 3000) && (location <= 3076))) {
        return 1;
    }

    return 0;
}
