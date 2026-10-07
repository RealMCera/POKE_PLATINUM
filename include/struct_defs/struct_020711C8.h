#ifndef POKEPLATINUM_STRUCT_020711C8_H
#define POKEPLATINUM_STRUCT_020711C8_H

#include "pokemon.h"

// Holder allocated for a field-move task. It keeps the party Pokemon that used
// the field move (mon) together with a scratch pointer (taskData) that the task
// itself may use for its own state. taskData starts NULL and is owned by the
// task; see FieldMoveMon_New.
typedef struct FieldMoveMon {
    Pokemon *mon;
    void *taskData;
} FieldMoveMon;

#endif // POKEPLATINUM_STRUCT_020711C8_H
