#ifndef POKEPLATINUM_G3_BUFFER_H
#define POKEPLATINUM_G3_BUFFER_H

#include <nitro/gx.h>

void G3_InitBufferSwap(void);
void G3_ResetG3X(void);
void G3_RequestSwapBuffers(GXSortMode sortMode, GXBufferMode bufferMode);
void G3_ProcessSwapBuffers(void);

#endif // POKEPLATINUM_G3_BUFFER_H
