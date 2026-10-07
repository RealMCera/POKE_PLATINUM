#ifndef POKEPLATINUM_BATTLE_SUBSCREEN_CURSOR_H
#define POKEPLATINUM_BATTLE_SUBSCREEN_CURSOR_H

#include "constants/heap.h"

/**
 * @brief Allocate and zero the battle subscreen cursor holder.
 *
 * Allocates a pointer-sized buffer on @p heapID and clears it to zero. The
 * buffer is stored in FieldSystem::battleSubscreenCursorOn and shared with the
 * battle system as the "subscreen cursor on" state.
 *
 * @param heapID Heap to allocate from.
 * @return Pointer to the zeroed cursor holder.
 */
u8 *BattleSubscreenCursor_New(enum HeapID heapID);

/**
 * @brief Free the battle subscreen cursor holder.
 *
 * @param cursor Cursor holder returned by BattleSubscreenCursor_New.
 */
void BattleSubscreenCursor_Free(u8 *cursor);

#endif // POKEPLATINUM_BATTLE_SUBSCREEN_CURSOR_H
