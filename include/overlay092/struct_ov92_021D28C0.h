#ifndef POKEPLATINUM_STRUCT_OV92_021D28C0_H
#define POKEPLATINUM_STRUCT_OV92_021D28C0_H

// A pair of x/y coordinates. The Geonet app stores angles in this form
// (0xFFFF units per full turn).
typedef struct GeonetAngle {
    s32 x;
    s32 y;
} GeonetAngle;

#endif // POKEPLATINUM_STRUCT_OV92_021D28C0_H
