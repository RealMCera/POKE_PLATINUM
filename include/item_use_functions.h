#ifndef POKEPLATINUM_ITEM_USE_FUNCTIONS_H
#define POKEPLATINUM_ITEM_USE_FUNCTIONS_H

#include "field/field_system_decl.h"

#include "bg_window.h"
#include "field_task.h"
#include "player_avatar.h"
#include "string_gf.h"

// Selects which of an item's three use callbacks ItemUseFunction_Get returns.
#define ITEM_FUNC_USE_FROM_MENU 0
#define ITEM_FUNC_USE_IN_FIELD  1
#define ITEM_FUNC_CHECK_CAN_USE 2

// Result of an item's can-use check. ITEM_USE_CAN_USE (0) means the item may be
// used; the other values are distinct failure reasons used to pick the error
// message shown to the player.
enum ItemUseCheckResult {
    ITEM_USE_CANNOT_USE_GENERIC = -1,
    ITEM_USE_CAN_USE,
    ITEM_USE_CANNOT_DISMOUNT,
    ITEM_USE_CANNOT_USE_WITH_PARTNER,
    ITEM_USE_CANNOT_FISH_HERE,
};

// Allocates and returns the work object for a field application launched by a
// registered item (town map, journal, poffin case, ...).
typedef void *(*FieldApplicationWorkCtor)(void *fieldSystem);

// Snapshot of the player's surroundings taken when an item is used. The
// can-use checks read this instead of querying the field directly, so that
// every check sees a consistent view of the map and player state.
typedef struct ItemUseContext {
    int mapHeaderID;
    BOOL hasPartner;
    int playerState;
    u16 facingTileBehavior; // behavior of the tile the player is facing
    u16 currTileBehavior;
    u16 berryPatchFlags;
    u8 padding_12[2];
    PlayerAvatar *playerAvatar;
    FieldSystem *fieldSystem;
} ItemUseContext;

// State for using a registered item directly in the field (without opening the
// bag). `appCtor`/`appWork` track the field application the item launches, if
// any, so the task can free its work object once the app exits.
typedef struct ItemFieldUseContext {
    FieldSystem *fieldSystem;
    ItemUseContext useContext;
    FieldApplicationWorkCtor appCtor;
    void *appWork;
    u16 item;
    u8 state;
} ItemFieldUseContext;

// State for using an item from the bag's start menu. The menu owns the task
// and the selected party slot, if the item targets a Pokémon.
typedef struct ItemMenuUseContext {
    FieldTask *fieldTask;
    u16 item;
    u8 selectedMonSlot;
    u8 padding_07;
} ItemMenuUseContext;

typedef BOOL (*ItemFieldUseFunc)(ItemFieldUseContext *);
typedef void (*ItemMenuUseFunc)(ItemMenuUseContext *, const ItemUseContext *);
typedef enum ItemUseCheckResult (*ItemCheckUseFunc)(const ItemUseContext *);

// Arguments passed to a field script started by an item (berry patch
// interactions, Vs. Seeker, Azure Flute, ...). `scriptID` selects the script;
// the four params are written to the script's SCRIPT_DATA_PARAMETER_* slots.
typedef struct ItemScriptContext {
    u32 scriptID;
    u16 param0;
    u16 param1;
    u16 param2;
    u16 param3;
} ItemScriptContext;

// Message box used to show a registered item's usage or error message. The
// window and string are owned by this struct and freed when the task ends.
typedef struct ItemUseMessageContext {
    Window window;
    String *string;
    u16 printState; // handle returned by FieldMessage_Print
    u16 state;
} ItemUseMessageContext;

u32 ItemUseFunction_Get(u16 funcType, u16 functionIdx);
void ItemUseContext_Init(FieldSystem *fieldSystem, ItemUseContext *param1);
BOOL BerryPatch_IsEmpty(const ItemUseContext *usageContext);
BOOL ItemUseFunction_UseRegisteredItem(FieldSystem *fieldSystem);

#endif // POKEPLATINUM_ITEM_USE_FUNCTIONS_H
