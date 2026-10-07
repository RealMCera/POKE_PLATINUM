#ifndef POKEPLATINUM_NETWORK_ERROR_H
#define POKEPLATINUM_NETWORK_ERROR_H

#include "constants/heap.h"

void NetworkError_DisplayNetworkError(enum HeapID heapID, int networkErrorId, int errorCode);

#endif // POKEPLATINUM_NETWORK_ERROR_H
