#ifndef POKEPLATINUM_STRUCT_OV104_0223D8F0_H
#define POKEPLATINUM_STRUCT_OV104_0223D8F0_H

// Movement/animation state for a FrontierObject. type selects the movement
// callback, step is the callback's internal step counter, and params holds the
// callback's scratch values.
typedef struct {
    u8 type;
    u8 step;
    s16 params[7];
} FrontierObjectMovement;

#endif // POKEPLATINUM_STRUCT_OV104_0223D8F0_H
