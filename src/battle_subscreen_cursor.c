#include "battle_subscreen_cursor.h"

#include "heap.h"

u8 *BattleSubscreenCursor_New(enum HeapID heapID)
{
    u8 *cursor = Heap_Alloc(heapID, sizeof(void *));
    MI_CpuClear8(cursor, sizeof(void *));
    return cursor;
}

void BattleSubscreenCursor_Free(u8 *cursor)
{
    Heap_Free(cursor);
}
