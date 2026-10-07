#ifndef POKEPLATINUM_FIELD_EVENT_H
#define POKEPLATINUM_FIELD_EVENT_H

#include "struct_decls/map_object.h"

#include "field/field_system_decl.h"

#include "map_header_data.h"

void FieldEvent_FindFacingMapObject(FieldSystem *fieldSystem, MapObject **mapObjectOut);
u8 FieldEvent_TryGetFacingInteractableObject(FieldSystem *fieldSystem, MapObject **mapObjectOut);
u16 FieldEvent_GetInteractedBgEventScript(FieldSystem *fieldSystem, const BgEvent *bgEvents, int numBgEvents);
u16 FieldEvent_GetInteractedWallSignScript(FieldSystem *fieldSystem, const BgEvent *bgEvents, int numBgEvents);
u8 FieldEvent_IsFacingSignpost(FieldSystem *fieldSystem, MapObject **mapObjectOut);
u16 FieldEvent_GetInteractedCoordEventScript(FieldSystem *fieldSystem, const CoordEvent *coordEvents, int numCoordEvents);

#endif // POKEPLATINUM_FIELD_EVENT_H
