#include "g3_buffer.h"

#include <nitro.h>
#include <string.h>

// A pending request to swap the G3 (3D graphics) buffers. A caller records the
// desired sort and buffer modes with G3_RequestSwapBuffers; the request is
// consumed once per frame by G3_ProcessSwapBuffers.
typedef struct G3SwapRequest {
    BOOL pending;
    GXSortMode sortMode;
    GXBufferMode bufferMode;
} G3SwapRequest;

static G3SwapRequest sSwapRequest;

void G3_InitBufferSwap(void)
{
    memset(&sSwapRequest, 0, sizeof(G3SwapRequest));
    sSwapRequest.pending = FALSE;
}

void G3_ResetG3X(void)
{
    G3X_Reset();
}

void G3_RequestSwapBuffers(GXSortMode sortMode, GXBufferMode bufferMode)
{
    sSwapRequest.sortMode = sortMode;
    sSwapRequest.bufferMode = bufferMode;
    sSwapRequest.pending = TRUE;
}

void G3_ProcessSwapBuffers(void)
{
    if (sSwapRequest.pending) {
        G3_SwapBuffers(sSwapRequest.sortMode, sSwapRequest.bufferMode);
        sSwapRequest.pending = FALSE;
    }
}
