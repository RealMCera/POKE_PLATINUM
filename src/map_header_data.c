#include "map_header_data.h"

#include <nitro.h>
#include <string.h>

#include "constants/heap.h"
#include "constants/versions.h"
#include "generated/map_headers.h"

#include "field/field_system.h"
#include "overlay006/wild_encounters.h"

#include "heap.h"
#include "map_header.h"
#include "map_object.h"
#include "narc.h"

// Map-header event and encounter data.
//
// MapHeaderData_Load reads the map header's event archive and splits it into
// the four event lists (background, object, warp, and coordinate events), then
// loads the map's wild encounters and init scripts. The accessors below expose
// those lists to the field system; the Set* functions mutate individual events
// in place, which scripts use to move warps or NPCs at runtime.

static void MapHeaderData_LoadEvents(MapHeaderData *data, enum MapHeaderID mapHeaderID);
static void MapHeaderData_ParseEvents(MapHeaderData *data);
static void MapHeaderData_LoadInitScripts(MapHeaderData *data, int headerID);

// Allocates the map-header data for the field system.
void MapHeaderData_Init(FieldSystem *fieldSystem, enum HeapID heapID)
{
    GF_ASSERT(fieldSystem->mapHeaderData == NULL);
    fieldSystem->mapHeaderData = Heap_Alloc(heapID, sizeof(MapHeaderData));
}

// Frees the map-header data.
void MapHeaderData_Free(FieldSystem *fieldSystem)
{
    GF_ASSERT(fieldSystem->mapHeaderData != NULL);
    Heap_Free(fieldSystem->mapHeaderData);
}

// Loads all map-header data for the given map: events, wild encounters, and
// init scripts.
void MapHeaderData_Load(FieldSystem *fieldSystem, enum MapHeaderID mapHeaderID)
{
    GF_ASSERT(fieldSystem->mapHeaderData != NULL);

    MapHeaderData_LoadEvents(fieldSystem->mapHeaderData, mapHeaderID);
    MapHeaderData_ParseEvents(fieldSystem->mapHeaderData);
    MapHeaderData_LoadWildEncounters(&fieldSystem->mapHeaderData->wildEncounters, mapHeaderID);
    MapHeaderData_LoadInitScripts(fieldSystem->mapHeaderData, mapHeaderID);
}

// Reads the map's event archive into the temporary buffer.
static void MapHeaderData_LoadEvents(MapHeaderData *data, enum MapHeaderID mapHeaderID)
{
    int eventsID = MapHeader_GetEventsArchiveID(mapHeaderID);
    GF_ASSERT(NARC_GetMemberSizeByIndexPair(NARC_INDEX_FIELDDATA__EVENTDATA__ZONE_EVENT, eventsID) < sizeof(data->tmpEventsBuf));
    NARC_ReadWholeMemberByIndexPair(data->tmpEventsBuf, NARC_INDEX_FIELDDATA__EVENTDATA__ZONE_EVENT, eventsID);
}

// Instantiates the map objects described by the loaded object events.
void MapHeaderData_AddMapObjects(FieldSystem *fieldSystem)
{
    int numObjectEvents = fieldSystem->mapHeaderData->numObjectEvents;

    GF_ASSERT(fieldSystem->mapHeaderData != NULL);

    if (numObjectEvents != 0) {
        MapObjectMan_AddMapObjectsFromEvents(fieldSystem->mapObjMan, fieldSystem->location->mapHeaderID, numObjectEvents, fieldSystem->mapHeaderData->objectEvents);
    }
}

// Returns the map's background events.
const BgEvent *MapHeaderData_GetBgEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->bgEvents;
}

// Returns the number of background events on the map.
int MapHeaderData_GetNumBgEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->numBgEvents;
}

// Returns the warp event at the given index, or NULL if the index is out of
// range.
const WarpEvent *MapHeaderData_GetWarpEventByIndex(const FieldSystem *fieldSystem, int index)
{
    return (index >= fieldSystem->mapHeaderData->numWarpEvents)
        ? NULL
        : &fieldSystem->mapHeaderData->warpEvents[index];
}

// Returns the index of the warp event at the given position, or -1 if there is
// none.
int MapHeaderData_GetIndexOfWarpEventAtPos(const FieldSystem *fieldSystem, int x, int z)
{
    for (int i = 0; i < fieldSystem->mapHeaderData->numWarpEvents; i++) {
        if (fieldSystem->mapHeaderData->warpEvents[i].x == x
            && fieldSystem->mapHeaderData->warpEvents[i].z == z) {
            return i;
        }
    }

    return -1;
}

// Returns the number of coordinate events on the map.
int MapHeaderData_GetNumCoordEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->numCoordEvents;
}

// Returns the map's coordinate events.
const CoordEvent *MapHeaderData_GetCoordEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->coordEvents;
}

// Returns the number of object events on the map.
u32 MapHeaderData_GetNumObjectEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->numObjectEvents;
}

// Returns the map's object events.
const ObjectEvent *MapHeaderData_GetObjectEvents(const FieldSystem *fieldSystem)
{
    return fieldSystem->mapHeaderData->objectEvents;
}

// Moves the object event with the given local ID to the given position.
BOOL MapHeaderData_SetObjectEventPos(FieldSystem *fieldSystem, int localID, u16 x, u16 z)
{
    int i;
    ObjectEvent *objectEvent = fieldSystem->mapHeaderData->objectEvents;
    u32 numObjectEvents = fieldSystem->mapHeaderData->numObjectEvents;

    for (i = 0; i < numObjectEvents; i++) {
        if (objectEvent[i].localID == localID) {
            objectEvent[i].x = x;
            objectEvent[i].z = z;
            return TRUE;
        }
    }

    GF_ASSERT(FALSE);
    return FALSE;
}

// Sets the facing direction of the object event with the given local ID.
BOOL MapHeaderData_SetObjectEventDir(FieldSystem *fieldSystem, int localID, int dir)
{
    int i;
    ObjectEvent *objectEvent = fieldSystem->mapHeaderData->objectEvents;
    u32 numObjectEvents = fieldSystem->mapHeaderData->numObjectEvents;

    for (i = 0; i < numObjectEvents; i++) {
        if (objectEvent[i].localID == localID) {
            objectEvent[i].dir = dir;
            return TRUE;
        }
    }

    GF_ASSERT(FALSE);
    return FALSE;
}

// Sets the movement type of the object event with the given local ID.
BOOL MapHeaderData_SetObjectEventMovementType(FieldSystem *fieldSystem, int localID, int movementType)
{
    int i;
    ObjectEvent *objectEvent = fieldSystem->mapHeaderData->objectEvents;
    u32 numObjectEvents = fieldSystem->mapHeaderData->numObjectEvents;

    for (i = 0; i < numObjectEvents; i++) {
        if (objectEvent[i].localID == localID) {
            objectEvent[i].movementType = movementType;
            return TRUE;
        }
    }

    GF_ASSERT(FALSE);
    return FALSE;
}

// Moves the warp event at the given index to the given position.
BOOL MapHeaderData_SetWarpEventPos(FieldSystem *fieldSystem, u16 index, u16 x, u16 z)
{
    WarpEvent *warpEvent = fieldSystem->mapHeaderData->warpEvents;
    warpEvent[index].x = x;
    warpEvent[index].z = z;
    return TRUE;
}

// Sets the destination map header of the warp event at the given index.
BOOL MapHeaderData_SetWarpEventDestHeaderID(FieldSystem *fieldSystem, u16 index, u16 destHeaderID)
{
    WarpEvent *warpEvents = fieldSystem->mapHeaderData->warpEvents;
    warpEvents[index].destHeaderID = destHeaderID;
    return TRUE;
}

// Sets the destination warp point of the warp event at the given index.
BOOL MapHeaderData_SetWarpEventDestWarpID(FieldSystem *fieldSystem, u16 index, u16 destWarpID)
{
    WarpEvent *warpEvents = fieldSystem->mapHeaderData->warpEvents;
    warpEvents[index].destWarpID = destWarpID;
    return TRUE;
}

// Moves the background event at the given index to the given position.
BOOL MapHeaderData_SetBgEventPos(FieldSystem *fieldSystem, u16 index, u16 x, u16 z)
{
    BgEvent *bgEvent = MapHeaderData_GetBgEvents(fieldSystem);

    bgEvent += index;
    bgEvent->x = x;
    bgEvent->z = z;

    return TRUE;
}

#define CONSUME_EVENTS(T, dataTEvents, dataNumTEvents) \
    do {                                               \
        (dataNumTEvents) = *(u32 *)events;             \
        events += sizeof(u32);                         \
        if ((dataNumTEvents) != 0) {                   \
            (dataTEvents) = (const T *)events;         \
        } else {                                       \
            (dataTEvents) = NULL;                      \
        }                                              \
        events += sizeof(T) * (dataNumTEvents);        \
    } while (0)

// Splits the raw event buffer into the four event lists. Each list is preceded
// by a u32 count; the list pointers point into the buffer.
static void MapHeaderData_ParseEvents(MapHeaderData *data)
{
    const u8 *events = (const u8 *)data->tmpEventsBuf;

    CONSUME_EVENTS(BgEvent, data->bgEvents, data->numBgEvents);
    CONSUME_EVENTS(ObjectEvent, data->objectEvents, data->numObjectEvents);
    CONSUME_EVENTS(WarpEvent, data->warpEvents, data->numWarpEvents);
    CONSUME_EVENTS(CoordEvent, data->coordEvents, data->numCoordEvents);
}

// Loads the wild encounters for the given map, or clears them if the map has
// none.
void MapHeaderData_LoadWildEncounters(WildEncounters *data, enum MapHeaderID mapHeaderID)
{
    memset(data, 0, sizeof(WildEncounters));
    if (MapHeader_HasWildEncounters(mapHeaderID)) {
        enum NarcID narcID = (GAME_VERSION == VERSION_DIAMOND || GAME_VERSION == VERSION_PLATINUM)
            ? NARC_INDEX_FIELDDATA__ENCOUNTDATA__PL_ENC_DATA
            : NARC_INDEX_FIELDDATA__ENCOUNTDATA__P_ENC_DATA;
        NARC_ReadWholeMemberByIndexPair(data, narcID, MapHeader_GetWildEncountersArchiveID(mapHeaderID));
    }
}

// Returns the map's wild encounters.
const WildEncounters *MapHeaderData_GetWildEncounters(const FieldSystem *fieldSystem)
{
    return &fieldSystem->mapHeaderData->wildEncounters;
}

// Reads the map's init scripts into the data buffer.
static void MapHeaderData_LoadInitScripts(MapHeaderData *data, int headerID)
{
    int initScriptsID = MapHeader_GetInitScriptsArchiveID(headerID);

    MI_CpuClearFast(data->initScripts, sizeof(data->initScripts));
    GF_ASSERT(NARC_GetMemberSizeByIndexPair(NARC_INDEX_FIELDDATA__SCRIPT__SCR_SEQ, initScriptsID) < sizeof(data->initScripts));

    NARC_ReadWholeMemberByIndexPair(data->initScripts, NARC_INDEX_FIELDDATA__SCRIPT__SCR_SEQ, initScriptsID);
}

// Returns the raw bytes of the map's init scripts.
const u8 *MapHeaderData_GetInitScriptBytes(const FieldSystem *fieldSystem)
{
    GF_ASSERT(fieldSystem->mapHeaderData != NULL);
    return (const u8 *)&fieldSystem->mapHeaderData->initScripts;
}

// Returns TRUE if no object event is placed at the given position.
BOOL MapHeaderData_IsPosFreeOfObjectEvents(const FieldSystem *fieldSystem, u16 x, u16 z)
{
    const MapHeaderData *data = fieldSystem->mapHeaderData;

    for (u32 i = 0; i < data->numObjectEvents; i++) {
        if (data->objectEvents[i].x == x && data->objectEvents[i].z == z) {
            return FALSE;
        }
    }

    return TRUE;
}
