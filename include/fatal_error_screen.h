#ifndef POKEPLATINUM_FATAL_ERROR_SCREEN_H
#define POKEPLATINUM_FATAL_ERROR_SCREEN_H

#include "constants/heap.h"

void FatalErrorScreen_ShowSaveDataError(enum HeapID heapID);
void FatalErrorScreen_ShowGbaPakError(enum HeapID heapID);

#endif // POKEPLATINUM_FATAL_ERROR_SCREEN_H
