#ifndef POKEPLATINUM_STRUCT_0208B878_H
#define POKEPLATINUM_STRUCT_0208B878_H

#include "struct_defs/struct_0208B284.h"
#include "struct_defs/struct_0208B878_sub1.h"
#include "struct_defs/struct_0208B878_sub2.h"

#include "sys_task_manager.h"

// A ring of sprites that fly in from off-screen and then orbit a common
// center. Used by the Vs. Recorder viewer (overlay062) for its intro and
// menu animations.
typedef struct {
    VsRecorderRingResources resources;
    VsRecorderRingConfig config;
    VsRecorderRingSprite sprites[12];
    // Shutdown state machine step; see VsRecorderRing_Shutdown.
    int shutdownState;
    // Index of the first sprite that reached the orbit, or 0xFF if none has.
    // Its angle seeds the angles of the sprites that follow it.
    int firstOrbitIndex;
    // Number of sprites that have entered the orbit so far.
    int orbitCounter;
    // Non-zero while the per-VBlank update task is running.
    BOOL active;
    SysTask *updateTask;
    int unk_F8;
    // Horizontal and vertical orbit radii in pixels.
    int orbitRadiusX;
    int orbitRadiusY;
    int unk_104;
} VsRecorderRing;

#endif // POKEPLATINUM_STRUCT_0208B878_H
