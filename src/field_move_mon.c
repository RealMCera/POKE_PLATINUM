#include "field_move_mon.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_020711C8.h"

#include "heap.h"
#include "party.h"
#include "savedata.h"

// Allocates a FieldMoveMon holding the party Pokemon in slot fieldMonId. The
// taskData slot is left NULL for the field-move task to fill in.
FieldMoveMon *FieldMoveMon_New(const enum HeapID heapID, const u8 fieldMonId, SaveData *saveData)
{
    FieldMoveMon *fieldMoveMon = Heap_AllocAtEnd(heapID, sizeof(FieldMoveMon));
    fieldMoveMon->mon = Party_GetPokemonBySlotIndex(SaveData_GetParty(saveData), fieldMonId);
    fieldMoveMon->taskData = NULL;

    return fieldMoveMon;
}
