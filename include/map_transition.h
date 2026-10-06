#ifndef POKEPLATINUM_MAP_TRANSITION_H
#define POKEPLATINUM_MAP_TRANSITION_H

#include "field/field_system_decl.h"

#include "field_task.h"

// Map transitions. See src/map_transition.c.
void MapTransition_StartScreenFade(FieldTask *taskMan, int mode, int typeMain, int typeSub, u16 color, int steps, int framesPerStep, enum HeapID heapID);
void MapTransition_Start(FieldSystem *fieldSystem, const int mapHeaderID, const int warpId, const int x, const int z, const int faceDirection, const int transitionType);
void MapTransition_StartAuto(FieldSystem *fieldSystem, const int mapHeaderID, const int warpId, const int x, const int z, const int faceDirection);

#endif // POKEPLATINUM_MAP_TRANSITION_H
