#include "field_event.h"

#include <nitro.h>
#include <string.h>

#include "generated/bg_event_dirs.h"
#include "generated/bg_event_types.h"
#include "generated/object_events_gfx.h"

#include "struct_decls/map_object.h"

#include "field/field_system.h"

#include "map_header_data.h"
#include "map_object.h"
#include "map_tile_behavior.h"
#include "player_avatar.h"
#include "script_manager.h"
#include "terrain_collision_manager.h"

static u8 BgEvent_CheckPlayerFacingDirection(FieldSystem *fieldSystem, const BgEvent *bgEvent);
static u8 BgEvent_IsHiddenItemAvailable(FieldSystem *fieldSystem, const BgEvent *bgEvent);

// Returns the player's facing direction and writes the grid coordinates of the
// tile directly in front of the player into *x and *z.
static int BgEvent_GetPlayerFacingPosition(FieldSystem *fieldSystem, int *x, int *z)
{
    int facingDir = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);

    *x = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    *z = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    switch (facingDir) {
    case DIR_NORTH:
        *z -= 1;
        break;
    case DIR_SOUTH:
        *z += 1;
        break;
    case DIR_WEST:
        *x -= 1;
        break;
    case DIR_EAST:
        *x += 1;
        break;
    }

    return facingDir;
}

// TRUE when mapObject shares the player's height (Y coordinate). This stops the
// player from interacting with objects on a different elevation, e.g. across a
// ledge.
static u8 FieldEvent_MapObjectIsAtSameYAsPlayer(PlayerAvatar *playerAvatar, MapObject *mapObject)
{
    MapObject *playerMapObject = PlayerAvatar_GetMapObject(playerAvatar);

    if (MapObject_GetYFromPos(playerMapObject) == MapObject_GetYFromPos(mapObject)) {
        return 1;
    }

    return 0;
}

// Finds the map object on the tile the player faces and writes it to
// *mapObjectOut (NULL when there is none). If the faced tile is a table, the
// player reaches across it, so the lookup is advanced one more tile in the
// facing direction.
void FieldEvent_FindFacingMapObject(FieldSystem *fieldSystem, MapObject **mapObjectOut)
{
    int x, z;
    int facingDir;
    u8 tileBehavior;

    facingDir = BgEvent_GetPlayerFacingPosition(fieldSystem, &x, &z);
    tileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

    if (TileBehavior_IsTable(tileBehavior) == 1) {
        switch (facingDir) {
        case DIR_NORTH:
            z -= 1;
            break;
        case DIR_SOUTH:
            z += 1;
            break;
        case DIR_WEST:
            x -= 1;
            break;
        case DIR_EAST:
            x += 1;
            break;
        }
    }

    *mapObjectOut = MapObjectMan_FindObjectAtCoords(fieldSystem->mapObjMan, x, z, 0);
}

// Finds the map object the player faces and reports whether it can be
// interacted with: it must be active (status 19 clear, i.e. not warping out)
// and at the player's height.
u8 FieldEvent_TryGetFacingInteractableObject(FieldSystem *fieldSystem, MapObject **mapObjectOut)
{
    FieldEvent_FindFacingMapObject(fieldSystem, mapObjectOut);

    if (*mapObjectOut != NULL) {
        if ((MapObject_CheckStatus19(*mapObjectOut) == 1) && (FieldEvent_MapObjectIsAtSameYAsPlayer(fieldSystem->playerAvatar, *mapObjectOut) == 1)) {
            return 1;
        }
    }

    return 0;
}

// Resolves the script of the background event the player is interacting with.
// The event must occupy the tile the player faces; hidden items are only
// returned while their hidden-item flag is still clear, and all other bg events
// must allow the player's current facing direction. Returns 0xffff when no
// event matches.
u16 FieldEvent_GetInteractedBgEventScript(FieldSystem *fieldSystem, const BgEvent *bgEvents, int numBgEvents)
{
    const BgEvent *events = bgEvents;
    int facingX, facingZ;
    int eventIndex;

    BgEvent_GetPlayerFacingPosition(fieldSystem, &facingX, &facingZ);

    for (eventIndex = 0; eventIndex < numBgEvents; eventIndex++) {
        if ((facingX == events[eventIndex].x) && (facingZ == events[eventIndex].z)) {
            if (events[eventIndex].type == BG_EVENT_TYPE_HIDDEN_ITEM) {
                if (BgEvent_IsHiddenItemAvailable(fieldSystem, &events[eventIndex]) == TRUE) {
                    return events[eventIndex].script;
                }
            } else {
                if (BgEvent_CheckPlayerFacingDirection(fieldSystem, &events[eventIndex]) == TRUE) {
                    return events[eventIndex].script;
                }
            }
        }
    }

    return 0xffff;
}

// TRUE while a hidden item's associated flag has not been set yet, meaning the
// item can still be found.
static u8 BgEvent_IsHiddenItemAvailable(FieldSystem *fieldSystem, const BgEvent *bgEvent)
{
    if (bgEvent->type != BG_EVENT_TYPE_HIDDEN_ITEM) {
        return FALSE;
    }

    if (FieldSystem_CheckFlag(fieldSystem, Script_GetHiddenItemFlag(bgEvent->script)) == TRUE) {
        return FALSE;
    }

    return TRUE;
}

// TRUE when the bg event permits the direction the player is currently facing.
// BG_EVENT_DIR_ALL always matches; the remaining values allow a single
// direction or a single axis (north/south or west/east).
static u8 BgEvent_CheckPlayerFacingDirection(FieldSystem *fieldSystem, const BgEvent *bgEvent)
{
    if (bgEvent->playerFacingDir == BG_EVENT_DIR_ALL) {
        return TRUE;
    }

    switch (PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar)) {
    case DIR_NORTH:
        if ((bgEvent->playerFacingDir == BG_EVENT_DIR_NORTH) || (bgEvent->playerFacingDir == BG_EVENT_DIR_NORTH_SOUTH)) {
            return TRUE;
        }
        break;
    case DIR_SOUTH:
        if ((bgEvent->playerFacingDir == BG_EVENT_DIR_SOUTH) || (bgEvent->playerFacingDir == BG_EVENT_DIR_NORTH_SOUTH)) {
            return TRUE;
        }
        break;
    case DIR_WEST:
        if ((bgEvent->playerFacingDir == BG_EVENT_DIR_WEST) || (bgEvent->playerFacingDir == BG_EVENT_DIR_WEST_EAST)) {
            return TRUE;
        }
        break;
    case DIR_EAST:
        if ((bgEvent->playerFacingDir == BG_EVENT_DIR_EAST) || (bgEvent->playerFacingDir == BG_EVENT_DIR_WEST_EAST)) {
            return TRUE;
        }
        break;
    }

    return FALSE;
}

// Resolves the script of the wall sign the player is reading. The player must
// face north, and the sign bg event must occupy the faced tile. Returns 0xffff
// when no wall sign matches.
u16 FieldEvent_GetInteractedWallSignScript(FieldSystem *fieldSystem, const BgEvent *bgEvents, int numBgEvents)
{
    int facingX, facingZ;
    int eventIndex;

    if (PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar) != DIR_NORTH) {
        return 0xffff;
    }

    const BgEvent *events = bgEvents;
    BgEvent_GetPlayerFacingPosition(fieldSystem, &facingX, &facingZ);

    for (eventIndex = 0; eventIndex < numBgEvents; eventIndex++) {
        if ((facingX == events[eventIndex].x) && (facingZ == events[eventIndex].z) && (events[eventIndex].type == BG_EVENT_TYPE_WALL_SIGN)) {
            return events[eventIndex].script;
        }
    }

    return 0xffff;
}

// TRUE when the player faces north toward a map object that acts as a sign.
// Only the signpost-style object graphics qualify: map signpost, mailbox,
// signboard, arrow signpost, gym signpost and trainer tips signpost.
u8 FieldEvent_IsFacingSignpost(FieldSystem *fieldSystem, MapObject **mapObjectOut)
{
    if (PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar) != DIR_NORTH) {
        return 0;
    }

    if (FieldEvent_TryGetFacingInteractableObject(fieldSystem, mapObjectOut) == 1) {
        u32 graphicsID = MapObject_GetGraphicsID(*mapObjectOut);

        if ((graphicsID == OBJ_EVENT_GFX_MAP_SIGNPOST) || (graphicsID == OBJ_EVENT_GFX_MAILBOX) || (graphicsID == OBJ_EVENT_GFX_SIGNBOARD) || (graphicsID == OBJ_EVENT_GFX_ARROW_SIGNPOST) || (graphicsID == OBJ_EVENT_GFX_GYM_SIGNPOST) || (graphicsID == OBJ_EVENT_GFX_TRAINER_TIPS_SIGNPOST)) {
            return 1;
        }
    }

    return 0;
}

// Resolves the script of the first coordinate event whose rectangle contains
// the player's position and whose variable currently equals its expected value.
// Returns 0xffff when no coordinate event matches.
u16 FieldEvent_GetInteractedCoordEventScript(FieldSystem *fieldSystem, const CoordEvent *coordEvents, int numCoordEvents)
{
    int i;

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    for (i = 0; i < numCoordEvents; i++) {
        if ((playerX >= coordEvents[i].x) && (playerX < (coordEvents[i].x + coordEvents[i].width)) && (playerZ >= coordEvents[i].z) && (playerZ < (coordEvents[i].z + coordEvents[i].length)) && (FieldSystem_TryGetVar(fieldSystem, coordEvents[i].var) == coordEvents[i].value)) {
            return coordEvents[i].script;
        }
    }

    return 0xffff;
}
