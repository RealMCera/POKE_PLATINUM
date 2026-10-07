#include "item_use_functions.h"

#include "nitro/types.h"
#include <nitro.h>
#include <string.h>

#include "constants/savedata/vars_flags.h"

#include "struct_decls/map_object.h"

#include "applications/mail.h"
#include "applications/party_menu/defs.h"
#include "applications/party_menu/main.h"
#include "applications/poffin_case/main.h"
#include "field/field_system.h"
#include "overlay005/fieldmap.h"
#include "overlay005/fishing.h"
#include "overlay005/ov5_021DFB54.h"
#include "overlay005/ov5_021F007C.h"
#include "overlay005/save_info_window.h"
#include "overlay005/struct_ov5_021F0468_decl.h"
#include "overlay006/field_warp.h"
#include "savedata/save_table.h"

#include "bag.h"
#include "bag_context.h"
#include "berry_patch_manager.h"
#include "bg_window.h"
#include "field_bgm.h"
#include "field_map_change.h"
#include "field_message.h"
#include "field_system.h"
#include "field_task.h"
#include "game_options.h"
#include "heap.h"
#include "item.h"
#include "item_use_functions.h"
#include "items.h"
#include "mail.h"
#include "map_header.h"
#include "map_header_data.h"
#include "map_object.h"
#include "map_object_move.h"
#include "map_tile_behavior.h"
#include "party.h"
#include "player_avatar.h"
#include "player_move.h"
#include "pokedex.h"
#include "pokeradar.h"
#include "render_window.h"
#include "save_player.h"
#include "screen_fade.h"
#include "script_manager.h"
#include "sound.h"
#include "start_menu.h"
#include "string_gf.h"
#include "system.h"
#include "system_flags.h"
#include "system_vars.h"
#include "terrain_collision_manager.h"
#include "unk_0203C954.h"
#include "field_system_apps.h"
#include "battle_salon.h"
#include "vars_flags.h"

#include "res/text/bank/location_names.h"

// Item usage dispatch. Every item that can be used (from the bag menu or from
// the field) is assigned a "use function" index by its data; this module maps
// that index to three callbacks: one to use the item from the bag menu, one to
// use it directly in the field, and one to check whether it can be used at all.
// The field checks all read from an ItemUseContext snapshot so they see a
// consistent view of the map and player state.

// The three callbacks for one item use-function index. Any entry may be NULL
// when that use case does not apply to the item.
typedef struct ItemUseFuncDat {
    ItemMenuUseFunc useItemFromMenuFunc;
    ItemFieldUseFunc useItemInFieldFunc;
    ItemCheckUseFunc canUseItemFunc;
} ItemUseFuncDat;

void *FieldSystem_OpenTownMapItem(FieldSystem *fieldSystem);
static void ItemUseContext_InitForDistortionWorld(FieldSystem *fieldSystem, ItemUseContext *usageContext);
static void UseHealingItemFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseTownMapFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseExplorerKitFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseBicycleFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseJournalFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseTMHMFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseMailFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseBerryFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UsePoffinCaseFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UsePalPadFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UsePokeRadarFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseSprayDuckFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseMulchFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseHoneyFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseVsSeekerFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseOldRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseGoodRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseSuperRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseEvoStoneFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseEscapeRopeFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseAzureFluteFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseVsRecorderFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static void UseGracideaFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext);
static BOOL UseBicycleInField(ItemFieldUseContext *usageContext);
static BOOL UseJournalInField(ItemFieldUseContext *usageContext);
static BOOL UseOldRodInField(ItemFieldUseContext *usageContext);
static BOOL UseGoodRodInField(ItemFieldUseContext *usageContext);
static BOOL UseSuperRodInField(ItemFieldUseContext *usageContext);
static BOOL UseBagMessageItem(ItemFieldUseContext *usageContext);
static BOOL UseTownMapInField(ItemFieldUseContext *usageContext);
static BOOL UsePoffinCaseInField(ItemFieldUseContext *usageContext);
static BOOL UsePalPadInField(ItemFieldUseContext *usageContext);
static BOOL UseExplorerKitInField(ItemFieldUseContext *usageContext);
static BOOL UsePokeRadarInField(ItemFieldUseContext *usageContext);
static BOOL UseSprayDuckInField(ItemFieldUseContext *usageContext);
static BOOL UseVsSeekerInField(ItemFieldUseContext *usageContext);
static BOOL UseAzureFluteInField(ItemFieldUseContext *usageContext);
static BOOL UseVsRecorderInField(ItemFieldUseContext *usageContext);
static BOOL UseGracideaInField(ItemFieldUseContext *usageContext);
static void *OpenPalPadApp(void *some_param);
static void *ItemUse_OpenPoffinCaseApp(void *some_param);
static void *OpenTownMapApp(void *some_param);
static void *OpenJournalApp(void *some_param);
static void *OpenVsRecorderApp(void *some_param);
static void *OpenPartyMenuForGracidea(void *fieldSystem);
static enum ItemUseCheckResult CanUseBicycle(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseExplorerKit(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseBerry(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUsePokeRadar(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseSprayDuck(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseMulch(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseVsSeeker(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseFishingRod(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseEscapeRope(const ItemUseContext *usageContext);
static enum ItemUseCheckResult CanUseAzureFlute(const ItemUseContext *usageContext);
static BOOL MountOrUnmountBicycle(FieldTask *task);
static BOOL PrintRegisteredKeyItemUseMessage(FieldTask *task);
static void RegisteredItem_CreateGoToAppTask(ItemFieldUseContext *usageContext, void *param1);
static BOOL RegisteredItem_GoToApp(FieldTask *task);
static BOOL WarpWithEscapeRope(FieldTask *task);
static BOOL RunItemScriptTask(FieldTask *task);
static void PrintRegisteredKeyItemError(ItemFieldUseContext *usageContext, u32 param1);

// clang-format off
static const ItemUseFuncDat sItemUseFuncs[] = {
    [ITEM_USE_FUNC_NONE]         = { NULL,                   UseBagMessageItem,     NULL              },
    [ITEM_USE_FUNC_HEALING]      = { UseHealingItemFromMenu, NULL,                  NULL              },
    [ITEM_USE_FUNC_TOWN_MAP]     = { UseTownMapFromMenu,     UseTownMapInField,     NULL              },
    [ITEM_USE_FUNC_EXPLORER_KIT] = { UseExplorerKitFromMenu, UseExplorerKitInField, CanUseExplorerKit },
    [ITEM_USE_FUNC_BICYCLE]      = { UseBicycleFromMenu,     UseBicycleInField,     CanUseBicycle     },
    [ITEM_USE_FUNC_JOURNAL]      = { UseJournalFromMenu,     UseJournalInField,     NULL              },
    [ITEM_USE_FUNC_TM_HM]        = { UseTMHMFromMenu,        NULL,                  NULL              },
    [ITEM_USE_FUNC_MAIL]         = { UseMailFromMenu,        NULL,                  NULL              },
    [ITEM_USE_FUNC_BERRY]        = { UseBerryFromMenu,       NULL,                  CanUseBerry       },
    [ITEM_USE_FUNC_POFFIN_CASE]  = { UsePoffinCaseFromMenu,  UsePoffinCaseInField,  NULL              },
    [ITEM_USE_FUNC_PAL_PAD]      = { UsePalPadFromMenu,      UsePalPadInField,      NULL              },
    [ITEM_USE_FUNC_POKE_RADAR]   = { UsePokeRadarFromMenu,   UsePokeRadarInField,   CanUsePokeRadar   },
    [ITEM_USE_FUNC_SPRAYDUCK]    = { UseSprayDuckFromMenu,   UseSprayDuckInField,   CanUseSprayDuck   },
    [ITEM_USE_FUNC_MULCH]        = { UseMulchFromMenu,       NULL,                  CanUseMulch       },
    [ITEM_USE_FUNC_HONEY]        = { UseHoneyFromMenu,       NULL,                  NULL              },
    [ITEM_USE_FUNC_VS_SEEKER]    = { UseVsSeekerFromMenu,    UseVsSeekerInField,    CanUseVsSeeker    },
    [ITEM_USE_FUNC_OLD_ROD]      = { UseOldRodFromMenu,      UseOldRodInField,      CanUseFishingRod  },
    [ITEM_USE_FUNC_GOOD_ROD]     = { UseGoodRodFromMenu,     UseGoodRodInField,     CanUseFishingRod  },
    [ITEM_USE_FUNC_SUPER_ROD]    = { UseSuperRodFromMenu,    UseSuperRodInField,    CanUseFishingRod  },
    [ITEM_USE_FUNC_BAG_MESSAGE]  = { NULL,                   UseBagMessageItem,     NULL              },
    [ITEM_USE_FUNC_EVO_STONE]    = { UseEvoStoneFromMenu,    NULL,                  NULL              },
    [ITEM_USE_FUNC_ESCAPE_ROPE]  = { UseEscapeRopeFromMenu,  NULL,                  CanUseEscapeRope  },
    [ITEM_USE_FUNC_AZURE_FLUTE]  = { UseAzureFluteFromMenu,  UseAzureFluteInField,  CanUseAzureFlute  },
    [ITEM_USE_FUNC_VS_RECORDER]  = { UseVsRecorderFromMenu,  UseVsRecorderInField,  NULL              },
    [ITEM_USE_FUNC_GRACIDEA]     = { UseGracideaFromMenu,    UseGracideaInField,    NULL              },
};
// clang-format on

// Returns the callback of the requested kind for an item use-function index.
// The return value is cast to the matching function-pointer type by the caller.
u32 ItemUseFunction_Get(u16 funcType, u16 functionIdx)
{
    if (funcType == ITEM_FUNC_USE_FROM_MENU) {
        return (u32)sItemUseFuncs[functionIdx].useItemFromMenuFunc;
    } else if (funcType == ITEM_FUNC_USE_IN_FIELD) {
        return (u32)sItemUseFuncs[functionIdx].useItemInFieldFunc;
    }

    return (u32)sItemUseFuncs[functionIdx].canUseItemFunc;
}

// Fills `ctxOut` with a snapshot of the player's surroundings for the item
// can-use checks. The tile behavior is sampled at the player's current tile and
// at the tile directly in front of them.
void ItemUseContext_Init(FieldSystem *fieldSystem, ItemUseContext *ctxOut)
{
    if (PlayerAvatar_DistortionGravityChanged(fieldSystem->playerAvatar) == TRUE) {
        ItemUseContext_InitForDistortionWorld(fieldSystem, ctxOut);
        return;
    }

    ctxOut->fieldSystem = fieldSystem;
    ctxOut->mapHeaderID = fieldSystem->location->mapHeaderID;
    ctxOut->hasPartner = SystemFlag_CheckHasPartner(SaveData_GetVarsFlags(fieldSystem->saveData));
    ctxOut->playerState = PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar);

    int x = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int z = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    ctxOut->currTileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

    int playerDirection = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);

    switch (playerDirection) {
    case DIR_NORTH:
        z--;
        break;
    case DIR_SOUTH:
        z++;
        break;
    case DIR_EAST:
        x++;
        break;
    case DIR_WEST:
        x--;
        break;
    }

    ctxOut->facingTileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);
    MapObject *mapObj;
    sub_0203C9D4(fieldSystem, &mapObj);

    ctxOut->berryPatchFlags = BerryPatches_GetPatchFlags(fieldSystem, mapObj);
    ctxOut->playerAvatar = fieldSystem->playerAvatar;
}

// Distortion World variant of ItemUseContext_Init. Gravity there is handled by
// the player avatar rather than the terrain collision manager, so the tile
// behaviors are queried from the avatar instead.
static void ItemUseContext_InitForDistortionWorld(FieldSystem *fieldSystem, ItemUseContext *ctxOut)
{
    ctxOut->fieldSystem = fieldSystem;
    ctxOut->mapHeaderID = fieldSystem->location->mapHeaderID;
    ctxOut->hasPartner = SystemFlag_CheckHasPartner(SaveData_GetVarsFlags(fieldSystem->saveData));
    ctxOut->playerState = PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar);
    ctxOut->currTileBehavior = PlayerAvatar_GetDistortionCurrTileBehaviour(fieldSystem->playerAvatar);

    int distortionDir = PlayerAvatar_GetDistortionDir(fieldSystem->playerAvatar);
    ctxOut->facingTileBehavior = PlayerAvatar_GetDistortionFacingTileBehaviour(fieldSystem->playerAvatar, distortionDir);

    ctxOut->berryPatchFlags = BerryPatches_GetPatchFlags(fieldSystem, NULL);
    ctxOut->playerAvatar = fieldSystem->playerAvatar;
}

// Allocates the argument block for a field script started by an item.
static ItemScriptContext *ItemScriptContext_New(u32 scriptID, u16 param0, u16 param1, u16 param2, u16 param3)
{
    ItemScriptContext *ctx = Heap_Alloc(HEAP_ID_FIELD3, sizeof(ItemScriptContext));

    ctx->scriptID = scriptID;
    ctx->param0 = param0;
    ctx->param1 = param1;
    ctx->param2 = param2;
    ctx->param3 = param3;

    return ctx;
}

// Starts a script from the bag menu: hands the script context to the start
// menu, which runs it as a new task.
static void RunItemScriptFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext, u32 scriptID)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = RunItemScriptTask;
    menu->taskData = ItemScriptContext_New(scriptID, usageContext->item, 0, 0, 0);
    menu->state = START_MENU_STATE_NEW_TASK;
}

// Starts a script directly from the field, without going through the menu.
static void RunItemScriptInField(ItemFieldUseContext *usageContext, u32 scriptID)
{
    void *ctx = ItemScriptContext_New(scriptID, usageContext->item, 0, 0, 0);
    FieldSystem_CreateTask(usageContext->fieldSystem, RunItemScriptTask, ctx);
}

// Field task that starts the item's script and copies its parameters into the
// script's SCRIPT_DATA_PARAMETER_* slots, then frees the context.
static BOOL RunItemScriptTask(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    ItemScriptContext *ctx = FieldTask_GetEnv(task);
    int *state = FieldTask_GetState(task);
    MapObject *mapObj;

    switch (*state) {
    case 0:
        sub_0203C9D4(fieldSystem, &mapObj);
        ScriptManager_Start(task, ctx->scriptID, mapObj, NULL);

        *(u16 *)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_DATA_PARAMETER_0) = ctx->param0;
        *(u16 *)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_DATA_PARAMETER_1) = ctx->param1;
        *(u16 *)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_DATA_PARAMETER_2) = ctx->param2;
        *(u16 *)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_DATA_PARAMETER_3) = ctx->param3;

        (*state)++;
        break;
    case 1:
        Heap_Free(ctx);
        return TRUE;
    }

    return FALSE;
}

static void UseHealingItemFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    PartyMenu *partyMenu = Heap_Alloc(HEAP_ID_FIELD2, sizeof(PartyMenu));

    memset(partyMenu, 0, sizeof(PartyMenu));

    partyMenu->party = SaveData_GetParty(fieldSystem->saveData);
    partyMenu->bag = SaveData_GetBag(fieldSystem->saveData);
    partyMenu->mailbox = SaveData_GetMailbox(fieldSystem->saveData);
    partyMenu->options = SaveData_GetOptions(fieldSystem->saveData);
    partyMenu->broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    partyMenu->fieldMoveContext = &menu->fieldMoveContext;
    partyMenu->type = PARTY_MENU_TYPE_BASIC;
    partyMenu->mode = PARTY_MENU_MODE_USE_ITEM;
    partyMenu->fieldSystem = fieldSystem;
    partyMenu->usedItemID = usageContext->item;
    partyMenu->selectedMonSlot = usageContext->selectedMonSlot;

    FieldSystem_StartChildProcess(fieldSystem, &gPokemonPartyAppTemplate, partyMenu);
    menu->taskData = partyMenu;
    StartMenu_SetCallback(menu, StartMenu_ExitPartyMenu);
}

static void UseTownMapFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    menu->taskData = FieldSystem_OpenTownMapItem(fieldSystem);
    StartMenu_SetCallback(menu, StartMenu_ExitTownMap);
}

static BOOL UseTownMapInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, OpenTownMapApp);
    return TRUE;
}

// Field application work constructors for registered items. Each opens the
// corresponding app and returns its work object, which RegisteredItem_GoToApp
// frees once the app exits.
static void *OpenTownMapApp(void *fieldSystem)
{
    return FieldSystem_OpenTownMapItem(fieldSystem);
}

static void UseExplorerKitFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = FieldTask_MapChangeToUnderground;
    menu->taskData = MapChangeUndergroundContext_New(fieldSystem);
    menu->state = START_MENU_STATE_NEW_TASK;

    fieldSystem->menuCursorPos = 0;
}

static BOOL UseExplorerKitInField(ItemFieldUseContext *usageContext)
{
    MapChangeUndergroundContext *ctx = MapChangeUndergroundContext_New(usageContext->fieldSystem);

    MapObjectMan_PauseAllMovement(usageContext->fieldSystem->mapObjMan);
    FieldSystem_CreateTask(usageContext->fieldSystem, FieldTask_MapChangeToUnderground, ctx);

    usageContext->fieldSystem->menuCursorPos = 0;
    return FALSE;
}

static enum ItemUseCheckResult CanUseExplorerKit(const ItemUseContext *usageContext)
{
    if (MapHeader_GetMapLabelTextID(usageContext->mapHeaderID) == LocationNames_Text_MysteryZone) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (!MapHeader_IsOnMainMatrix(usageContext->mapHeaderID)) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (PlayerAvatar_IsOnCyclingRoad(usageContext->playerAvatar) == TRUE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (SystemFlag_CheckSafariGameActive(SaveData_GetVarsFlags(usageContext->fieldSystem->saveData)) == TRUE
        || SystemFlag_CheckInPalPark(SaveData_GetVarsFlags(usageContext->fieldSystem->saveData)) == TRUE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (PlayerAvatar_GetPlayerState(usageContext->playerAvatar) == PLAYER_AVATAR_SURFING) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (TileBehavior_IsBridge(usageContext->currTileBehavior) == TRUE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (TileBehavior_ForbidsExplorationKit(usageContext->currTileBehavior) == TRUE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    u16 x = PlayerAvatar_GetXPos(usageContext->fieldSystem->playerAvatar);
    u16 z = PlayerAvatar_GetZPos(usageContext->fieldSystem->playerAvatar);

    // doesn't match as !MapHeaderData_IsPosFreeOfObjectEvents
    if (MapHeaderData_IsPosFreeOfObjectEvents(usageContext->fieldSystem, x, z) == FALSE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    return ITEM_USE_CAN_USE;
}

static void UseBicycleFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = MountOrUnmountBicycle;
    menu->taskData = NULL;
    menu->state = START_MENU_STATE_NEW_TASK;
}

static BOOL UseBicycleInField(ItemFieldUseContext *usageContext)
{
    FieldSystem_CreateTask(usageContext->fieldSystem, MountOrUnmountBicycle, NULL);
    return FALSE;
}

// Toggles the bicycle: if the player is already cycling (state 0x1) they
// dismount and the map BGM is restored; otherwise they mount and the bicycle
// theme overrides the map BGM. The radar chain is cleared when mounting.
static BOOL MountOrUnmountBicycle(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    int *state = FieldTask_GetState(task);

    switch (*state) {
    case 0:
        (*state)++;
        break;
    case 1:
        if (PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar) == 0x1) {
            MapObject_SetPauseMovementOff(PlayerAvatar_GetMapObject(fieldSystem->playerAvatar));
            PlayerAvatar_SetTransitionState(fieldSystem->playerAvatar, PLAYER_TRANSITION_WALKING);
            PlayerAvatar_RequestChangeState(fieldSystem->playerAvatar);

            FieldBGM_SetOverride(fieldSystem, SEQ_NONE);
            FieldBGM_TryFadeOut(fieldSystem, FieldBGM_GetEffective(fieldSystem, fieldSystem->location->mapHeaderID), 1);
        } else {
            FieldBGM_SetOverride(fieldSystem, SEQ_BICYCLE_sseq);
            FieldBGM_TryFadeOut(fieldSystem, SEQ_BICYCLE_sseq, 1);
            MapObject_SetPauseMovementOff(PlayerAvatar_GetMapObject(fieldSystem->playerAvatar));

            PlayerAvatar_SetTransitionState(fieldSystem->playerAvatar, PLAYER_TRANSITION_CYCLING);
            PlayerAvatar_RequestChangeState(fieldSystem->playerAvatar);

            RadarChain_Clear(fieldSystem->chain);
        }

        (*state)++;
        break;
    case 2:
        (*state)++;
        break;
    case 3:
        MapObjectMan_UnpauseAllMovement(fieldSystem->mapObjMan);
        return TRUE;
    }

    return FALSE;
}

// The bicycle can be mounted almost anywhere, but it cannot be dismounted
// while the game forces biking (gates, Cycling Road, bike bridges), and it
// cannot be used in very tall grass, mud, or while surfing.
static enum ItemUseCheckResult CanUseBicycle(const ItemUseContext *usageContext)
{
    VarsFlags *v0 = SaveData_GetVarsFlags(usageContext->fieldSystem->saveData);

    if (usageContext->hasPartner == TRUE) {
        return ITEM_USE_CANNOT_USE_WITH_PARTNER;
    }

    if (SystemFlag_HandleForceBikingInGate(v0, HANDLE_FLAG_CHECK) == TRUE) {
        return ITEM_USE_CANNOT_DISMOUNT;
    }

    if (PlayerAvatar_IsOnCyclingRoad(usageContext->playerAvatar) == TRUE) {
        return ITEM_USE_CANNOT_DISMOUNT;
    }

    {
        MapObject *v1 = PlayerAvatar_GetMapObject(usageContext->playerAvatar);

        if (MapObject_IsOnBikeBridgeNorthSouth(v1, usageContext->currTileBehavior) == TRUE || MapObject_IsOnBikeBridgeEastWest(v1, usageContext->currTileBehavior) == TRUE) {
            return ITEM_USE_CANNOT_DISMOUNT;
        }
    }

    if (TileBehavior_IsVeryTallGrass(usageContext->currTileBehavior) == TRUE || TileBehavior_IsMud(usageContext->currTileBehavior) == TRUE || TileBehavior_IsMudWithGrass(usageContext->currTileBehavior) == TRUE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (MapHeader_IsBikeAllowed(usageContext->mapHeaderID) == FALSE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (usageContext->playerState == PLAYER_AVATAR_SURFING) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    return ITEM_USE_CAN_USE;
}

static void UseJournalFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *v1 = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_OpenJournalApp(fieldSystem, NULL);
    StartMenu_SetCallback(v1, StartMenu_ExitJournal);
}

static BOOL UseJournalInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, OpenJournalApp);
    return TRUE;
}

static void *OpenJournalApp(void *fieldSystem)
{
    FieldSystem_OpenJournalApp(fieldSystem, NULL);
    return NULL;
}

static void UseTMHMFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    PartyMenu *partyMenu = Heap_Alloc(HEAP_ID_FIELD2, sizeof(PartyMenu));

    memset(partyMenu, 0, sizeof(PartyMenu));

    partyMenu->party = SaveData_GetParty(fieldSystem->saveData);
    partyMenu->bag = SaveData_GetBag(fieldSystem->saveData);
    partyMenu->mailbox = SaveData_GetMailbox(fieldSystem->saveData);
    partyMenu->options = SaveData_GetOptions(fieldSystem->saveData);
    partyMenu->fieldMoveContext = &menu->fieldMoveContext;
    partyMenu->type = PARTY_MENU_TYPE_BASIC;
    partyMenu->mode = PARTY_MENU_MODE_TEACH_MOVE;
    partyMenu->fieldSystem = fieldSystem;
    partyMenu->usedItemID = usageContext->item;
    partyMenu->selectedMonSlot = usageContext->selectedMonSlot;
    partyMenu->learnedMove = Item_MoveForTMHM(usageContext->item);

    FieldSystem_StartChildProcess(fieldSystem, &gPokemonPartyAppTemplate, partyMenu);
    menu->taskData = partyMenu;
    StartMenu_SetCallback(menu, StartMenu_ExitPartyMenu);
}

static void UseMailFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    MailAppArgs *args = FieldSystem_LaunchMailApp_Read(fieldSystem, MAIL_CONTEXT_CHECK, Item_GetMailType(usageContext->item), HEAP_ID_FIELD2);

    menu->additionalTaskContext = StartMenu_BuildMailData(usageContext->item, MAIL_READ_FROM_BAG, 0);
    menu->taskData = args;

    StartMenu_SetCallback(menu, StartMenu_ExitMail);
}

static enum ItemUseCheckResult CanUseBerry(const ItemUseContext *usageContext)
{
    return ITEM_USE_CAN_USE;
}

// Using a berry on an empty patch plants it (berry tree interaction script);
// otherwise it is treated as a healing item to feed to a party Pokémon.
static void UseBerryFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem;
    StartMenu *v1;
    MapObject *v2;

    fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    v1 = FieldTask_GetEnv(usageContext->fieldTask);

    if (additionalContext->berryPatchFlags & BERRY_PATCH_FLAG_EMPTY) {
        RunItemScriptFromMenu(usageContext, additionalContext, SCRIPT_ID(BERRY_TREE_INTERACTIONS, 1));
    } else {
        UseHealingItemFromMenu(usageContext, additionalContext);
    }
}

BOOL BerryPatch_IsEmpty(const ItemUseContext *usageContext)
{
    if (usageContext->berryPatchFlags & BERRY_PATCH_FLAG_EMPTY) {
        return TRUE;
    }

    return FALSE;
}

static void UsePoffinCaseFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    PoffinCaseAppData *poffinCase = FieldSystem_LaunchPoffinCaseApp(fieldSystem, HEAP_ID_FIELD2);

    menu->taskData = poffinCase;
    StartMenu_SetCallback(menu, StartMenu_ExitPoffinCase);
}

static BOOL UsePoffinCaseInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, ItemUse_OpenPoffinCaseApp);
    return TRUE;
}

static void *ItemUse_OpenPoffinCaseApp(void *fieldSystem)
{
    return FieldSystem_LaunchPoffinCaseApp(fieldSystem, HEAP_ID_FIELD2);
}

static void UsePalPadFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_OpenPalPad(fieldSystem, fieldSystem->saveData);
    menu->taskData = NULL;
    StartMenu_SetCallback(menu, StartMenu_ExitPalPad);
}

static BOOL UsePalPadInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, OpenPalPadApp);
    return TRUE;
}

static void *OpenPalPadApp(void *fieldSystem)
{
    FieldSystem_OpenPalPad(fieldSystem, ((FieldSystem *)fieldSystem)->saveData);
    return NULL;
}

static void UsePokeRadarFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    int *v2 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(int));

    (*v2) = 0;
    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = RefreshRadarChain;
    menu->taskData = v2;
    menu->state = START_MENU_STATE_NEW_TASK;
}

static BOOL UsePokeRadarInField(ItemFieldUseContext *usageContext)
{
    int *v0 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(int));

    *v0 = 0;
    FieldSystem_CreateTask(usageContext->fieldSystem, RefreshRadarChain, v0);

    return FALSE;
}

// The Poké Radar can only be used while standing in tall grass, and not while
// cycling (player state 0x1) or with a partner.
static enum ItemUseCheckResult CanUsePokeRadar(const ItemUseContext *usageContext)
{
    if (usageContext->hasPartner == TRUE) {
        return ITEM_USE_CANNOT_USE_WITH_PARTNER;
    }

    if (PlayerAvatar_GetPlayerState(usageContext->fieldSystem->playerAvatar) == 0x1) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (!TileBehavior_IsTallGrass(usageContext->currTileBehavior)) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    return ITEM_USE_CAN_USE;
}

static void UseSprayDuckFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    RunItemScriptFromMenu(usageContext, additionalContext, SCRIPT_ID(BERRY_TREE_INTERACTIONS, 2));
}

static BOOL UseSprayDuckInField(ItemFieldUseContext *usageContext)
{
    RunItemScriptInField(usageContext, SCRIPT_ID(BERRY_TREE_INTERACTIONS, 2));
    return FALSE;
}

// The Sprayduck can only water a patch that currently holds a berry.
static enum ItemUseCheckResult CanUseSprayDuck(const ItemUseContext *usageContext)
{
    if (usageContext->hasPartner == TRUE) {
        return ITEM_USE_CANNOT_USE_WITH_PARTNER;
    }

    if (usageContext->berryPatchFlags & BERRY_PATCH_FLAG_HAS_BERRY) {
        return ITEM_USE_CAN_USE;
    } else {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }
}

static void UseMulchFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    RunItemScriptFromMenu(usageContext, additionalContext, SCRIPT_ID(BERRY_TREE_INTERACTIONS, 3));
}

// Mulch can only be applied to a patch that accepts it (e.g. one that is not
// already mulched).
static enum ItemUseCheckResult CanUseMulch(const ItemUseContext *usageContext)
{
    if (usageContext->berryPatchFlags & BERRY_PATCH_FLAG_CAN_MULCH) {
        return ITEM_USE_CAN_USE;
    } else {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }
}

static void UseHoneyFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem;
    StartMenu *menu;
    UnkStruct_ov5_021F0468 *v2;
    int v3;

    fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    v3 = ov5_021F0484();
    v2 = Heap_AllocAtEnd(HEAP_ID_FIELD2, v3);

    memset(v2, 0, v3);

    menu->callback = ov5_021F0488;
    menu->taskData = v2;
    menu->state = START_MENU_STATE_NEW_TASK;

    Bag_TryRemoveItem(SaveData_GetBag(fieldSystem->saveData), usageContext->item, 1, HEAP_ID_FIELD2);
}

static void UseVsSeekerFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    RunItemScriptFromMenu(usageContext, additionalContext, SCRIPT_ID(VS_SEEKER, 0));
}

static BOOL UseVsSeekerInField(ItemFieldUseContext *usageContext)
{
    RunItemScriptInField(usageContext, SCRIPT_ID(VS_SEEKER, 0));
    return FALSE;
}

static enum ItemUseCheckResult CanUseVsSeeker(const ItemUseContext *usageContext)
{
    if (MapHeader_IsOnMainMatrix(usageContext->mapHeaderID)) {
        return ITEM_USE_CAN_USE;
    }

    return ITEM_USE_CANNOT_USE_GENERIC;
}

static void UseOldRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = FieldTask_Fishing;
    menu->taskData = FishingContext_Init(fieldSystem, HEAP_ID_FIELD2, FISHING_TYPE_OLD_ROD);
    menu->state = START_MENU_STATE_NEW_TASK;
}

static BOOL UseOldRodInField(ItemFieldUseContext *usageContext)
{
    void *fishingContext = FishingContext_Init(usageContext->fieldSystem, HEAP_ID_FIELD1, FISHING_TYPE_OLD_ROD);

    FieldSystem_CreateTask(usageContext->fieldSystem, FieldTask_Fishing, fishingContext);
    return FALSE;
}

static void UseGoodRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = FieldTask_Fishing;
    menu->taskData = FishingContext_Init(fieldSystem, HEAP_ID_FIELD2, FISHING_TYPE_GOOD_ROD);
    menu->state = START_MENU_STATE_NEW_TASK;
}

static BOOL UseGoodRodInField(ItemFieldUseContext *usageContext)
{
    void *fishingContext = FishingContext_Init(usageContext->fieldSystem, HEAP_ID_FIELD1, FISHING_TYPE_GOOD_ROD);

    FieldSystem_CreateTask(usageContext->fieldSystem, FieldTask_Fishing, fishingContext);
    return FALSE;
}

static void UseSuperRodFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = FieldTask_Fishing;
    menu->taskData = FishingContext_Init(fieldSystem, HEAP_ID_FIELD2, FISHING_TYPE_SUPER_ROD);
    menu->state = START_MENU_STATE_NEW_TASK;
}

static BOOL UseSuperRodInField(ItemFieldUseContext *usageContext)
{
    void *fishingContext = FishingContext_Init(usageContext->fieldSystem, HEAP_ID_FIELD1, FISHING_TYPE_SUPER_ROD);

    FieldSystem_CreateTask(usageContext->fieldSystem, FieldTask_Fishing, fishingContext);
    return FALSE;
}

// Fishing requires surfable water on the tile the player faces. It is blocked
// throughout the Distortion World, and on an elevated bridge the player must
// step down before casting.
static enum ItemUseCheckResult CanUseFishingRod(const ItemUseContext *usageContext)
{
    if (usageContext->hasPartner == TRUE) {
        return ITEM_USE_CANNOT_USE_WITH_PARTNER;
    }

    if (usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_1F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B1F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B2F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B3F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B4F
        || usageContext->mapHeaderID == MAP_HEADER_UNKNOWN_578
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B5F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B6F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_B7F
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_GIRATINA_ROOM
        || usageContext->mapHeaderID == MAP_HEADER_DISTORTION_WORLD_TURNBACK_CAVE_ROOM) {
        return ITEM_USE_CANNOT_FISH_HERE;
    }

    if (TileBehavior_IsSurfable(usageContext->facingTileBehavior) == TRUE) {
        if ((TileBehavior_IsBridge(usageContext->currTileBehavior) == TRUE) || (TileBehavior_IsBridgeStart(usageContext->currTileBehavior) == TRUE)) {
            MapObject *playerObj = PlayerAvatar_GetMapObject(usageContext->playerAvatar);

            if (MapObject_IsStatusOnElevatedBridge(playerObj) == TRUE) {
                return ITEM_USE_CANNOT_USE_GENERIC;
            }
        }

        return ITEM_USE_CAN_USE;
    }

    return ITEM_USE_CANNOT_USE_GENERIC;
}

// Shows the registered item's usage message (e.g. "The Bicycle can be used
// here.") and waits for the player to dismiss it.
static BOOL UseBagMessageItem(ItemFieldUseContext *usageContext)
{
    ItemUseMessageContext *msgCtx = Heap_Alloc(HEAP_ID_FIELD2, sizeof(ItemUseMessageContext));

    msgCtx->state = 0;
    msgCtx->string = String_Init(128, HEAP_ID_FIELD2);

    BagContext_FormatUsageMessage(usageContext->fieldSystem->saveData, msgCtx->string, Bag_GetRegisteredItem(SaveData_GetBag(usageContext->fieldSystem->saveData)), HEAP_ID_FIELD2);
    FieldSystem_CreateTask(usageContext->fieldSystem, PrintRegisteredKeyItemUseMessage, msgCtx);

    return FALSE;
}

// Field task that prints a registered item message and waits for a key press
// before tearing the window down. Shared by the usage and error messages.
static BOOL PrintRegisteredKeyItemUseMessage(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    ItemUseMessageContext *msgCtx = FieldTask_GetEnv(task);

    switch (msgCtx->state) {
    case 0:
        MapObjectMan_PauseAllMovement(fieldSystem->mapObjMan);
        FieldMessage_AddWindow(fieldSystem->bgConfig, &msgCtx->window, 3);

        const Options *options = SaveData_GetOptions(fieldSystem->saveData);

        FieldMessage_DrawWindow(&msgCtx->window, options);
        msgCtx->printState = FieldMessage_Print(&msgCtx->window, msgCtx->string, options, 1);
        msgCtx->state++;
        break;
    case 1:
        if (FieldMessage_FinishedPrinting(msgCtx->printState) == TRUE) {
            if (gSystem.pressedKeys & (PAD_KEY | PAD_BUTTON_A | PAD_BUTTON_B)) {
                Window_EraseMessageBox(&msgCtx->window, 0);
                msgCtx->state++;
            }
        }
        break;
    case 2:
        MapObjectMan_UnpauseAllMovement(fieldSystem->mapObjMan);
        Window_Remove(&msgCtx->window);
        String_Free(msgCtx->string);
        Heap_Free(msgCtx);

        return TRUE;
    }

    return FALSE;
}

static void UseEvoStoneFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    PartyMenu *partyMenu = Heap_Alloc(HEAP_ID_FIELD2, sizeof(PartyMenu));

    memset(partyMenu, 0, sizeof(PartyMenu));

    partyMenu->party = SaveData_GetParty(fieldSystem->saveData);
    partyMenu->bag = SaveData_GetBag(fieldSystem->saveData);
    partyMenu->mailbox = SaveData_GetMailbox(fieldSystem->saveData);
    partyMenu->options = SaveData_GetOptions(fieldSystem->saveData);
    partyMenu->broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    partyMenu->fieldMoveContext = &menu->fieldMoveContext;
    partyMenu->type = PARTY_MENU_TYPE_BASIC;
    partyMenu->mode = PARTY_MENU_MODE_USE_EVO_ITEM;
    partyMenu->usedItemID = usageContext->item;
    partyMenu->selectedMonSlot = usageContext->selectedMonSlot;

    FieldSystem_StartChildProcess(fieldSystem, &gPokemonPartyAppTemplate, partyMenu);
    menu->taskData = partyMenu;
    StartMenu_SetCallback(menu, StartMenu_ExitPartyMenu);
}

static void UseEscapeRopeFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_StartFieldMap(fieldSystem);

    menu->callback = WarpWithEscapeRope;
    menu->taskData = NULL;
    menu->state = START_MENU_STATE_NEW_TASK;

    Bag_TryRemoveItem(SaveData_GetBag(fieldSystem->saveData), usageContext->item, 1, HEAP_ID_FIELD2);
}

static enum ItemUseCheckResult CanUseEscapeRope(const ItemUseContext *usageContext)
{
    if (usageContext->hasPartner == TRUE) {
        return ITEM_USE_CANNOT_USE_WITH_PARTNER;
    }

    if ((MapHeader_IsCave(usageContext->mapHeaderID) == TRUE) && (MapHeader_IsEscapeRopeAllowed(usageContext->mapHeaderID) == TRUE)) {
        return ITEM_USE_CAN_USE;
    }

    return ITEM_USE_CANNOT_USE_GENERIC;
}

static BOOL WarpWithEscapeRope(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    FieldWarp *fieldWarp = FieldWarp_InitEscapeRope(fieldSystem, HEAP_ID_FIELD2);

    FieldTask_InitJump(task, FieldWarp_EscapeRopeFadeOut, fieldWarp);
    return FALSE;
}

static void UseAzureFluteFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    RunItemScriptFromMenu(usageContext, additionalContext, SCRIPT_ID(COMMON_SCRIPTS, 39));
}

static BOOL UseAzureFluteInField(ItemFieldUseContext *usageContext)
{
    RunItemScriptInField(usageContext, SCRIPT_ID(COMMON_SCRIPTS, 39));
    return FALSE;
}

static enum ItemUseCheckResult CanUseAzureFlute(const ItemUseContext *usageContext)
{
    VarsFlags *v0 = SaveData_GetVarsFlags(usageContext->fieldSystem->saveData);

    if (SystemFlag_CheckGameCompleted(v0) == FALSE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (SystemVars_CheckDistributionEvent(v0, DISTRIBUTION_EVENT_ARCEUS) == FALSE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (Pokedex_IsNationalDexObtained(SaveData_GetPokedex(usageContext->fieldSystem->saveData)) == FALSE) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    if (!MapHeader_IsAzureFluteAllowed(usageContext->mapHeaderID)) {
        return ITEM_USE_CANNOT_USE_GENERIC;
    }

    return ITEM_USE_CAN_USE;
}

static void UseVsRecorderFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);

    FieldSystem_OpenVsRecorder(fieldSystem, fieldSystem->saveData);
    menu->taskData = NULL;
    StartMenu_SetCallback(menu, StartMenu_ExitVsRecorder);
}

static BOOL UseVsRecorderInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, OpenVsRecorderApp);
    return TRUE;
}

static void *OpenVsRecorderApp(void *fieldSystem)
{
    FieldSystem_SaveStateIfCommunicationOff(fieldSystem);
    FieldSystem_OpenVsRecorder(fieldSystem, ((FieldSystem *)fieldSystem)->saveData);

    return NULL;
}

static void UseGracideaFromMenu(ItemMenuUseContext *usageContext, const ItemUseContext *additionalContext)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(usageContext->fieldTask);
    StartMenu *menu = FieldTask_GetEnv(usageContext->fieldTask);
    menu->taskData = FieldSystem_OpenPartyMenu_SelectForItemUsage(fieldSystem, HEAP_ID_FIELD2, ITEM_GRACIDEA);

    StartMenu_SetCallback(menu, StartMenu_ExitPartyMenu);
}

static BOOL UseGracideaInField(ItemFieldUseContext *usageContext)
{
    RegisteredItem_CreateGoToAppTask(usageContext, OpenPartyMenuForGracidea);
    return TRUE;
}

static void *OpenPartyMenuForGracidea(void *fieldSystem)
{
    return FieldSystem_OpenPartyMenu_SelectForItemUsage(fieldSystem, HEAP_ID_FIELD2, ITEM_GRACIDEA);
}

// Uses the item registered to the Select button directly in the field. Returns
// TRUE if the press was handled (even if the item could not be used), FALSE if
// there is nothing to do. The item's can-use check runs first; on failure an
// error message is shown instead of using the item.
BOOL ItemUseFunction_UseRegisteredItem(FieldSystem *fieldSystem)
{
    ItemFieldUseContext *usageContext;
    ItemFieldUseFunc useInField;
    ItemCheckUseFunc checkUse;
    u16 item;
    u16 itemUseFuncIdx;
    BOOL usageResult;

    if (FieldSystem_IsInBattleTowerSalon(fieldSystem) == TRUE) {
        return FALSE;
    }

    if (SystemFlag_CheckInPalPark(SaveData_GetVarsFlags(fieldSystem->saveData)) == TRUE) {
        return FALSE;
    }

    item = (u16)Bag_GetRegisteredItem(SaveData_GetBag(fieldSystem->saveData));
    itemUseFuncIdx = (u16)Item_LoadParam(item, ITEM_PARAM_FIELD_USE_FUNC, HEAP_ID_FIELD2);
    checkUse = (ItemCheckUseFunc)ItemUseFunction_Get(ITEM_FUNC_CHECK_CAN_USE, itemUseFuncIdx);
    useInField = (ItemFieldUseFunc)ItemUseFunction_Get(ITEM_FUNC_USE_IN_FIELD, itemUseFuncIdx);

    if (useInField == NULL) {
        return FALSE;
    }

    usageContext = Heap_Alloc(HEAP_ID_FIELD2, sizeof(ItemFieldUseContext));
    memset(usageContext, 0, sizeof(ItemFieldUseContext));

    usageContext->fieldSystem = fieldSystem;
    usageContext->item = item;

    ItemUseContext_Init(fieldSystem, &usageContext->useContext);

    usageResult = 0;

    if (checkUse == NULL) {
        usageResult = useInField(usageContext);
    } else {
        u32 usageCheckResult = checkUse(&usageContext->useContext);

        if (usageCheckResult == 0) {
            usageResult = useInField(usageContext);
        } else {
            PrintRegisteredKeyItemError(usageContext, usageCheckResult);
        }
    }

    // The use function takes ownership of the context when it starts a task;
    // otherwise nothing was started and the context can be freed here.
    if (usageResult == 0) {
        Heap_Free(usageContext);
    }

    return TRUE;
}

// Shows the message explaining why the registered item cannot be used.
static void PrintRegisteredKeyItemError(ItemFieldUseContext *usageContext, u32 error)
{
    ItemUseMessageContext *msgCtx = Heap_Alloc(HEAP_ID_FIELD2, sizeof(ItemUseMessageContext));

    msgCtx->state = 0;
    msgCtx->string = String_Init(128, HEAP_ID_FIELD2);

    BagContext_FormatErrorMessage(SaveData_GetTrainerInfo(usageContext->fieldSystem->saveData), msgCtx->string, usageContext->item, error, HEAP_ID_FIELD2);
    FieldSystem_CreateTask(usageContext->fieldSystem, PrintRegisteredKeyItemUseMessage, msgCtx);
}

// Field task that fades out, launches the registered item's field application,
// waits for it to exit, then fades the field map back in. The app's work object
// is freed once the app is done; the poffin case owns its work object and frees
// it through PoffinCaseAppData_Free instead.
static BOOL RegisteredItem_GoToApp(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    ItemFieldUseContext *usageContext = FieldTask_GetEnv(task);

    switch (usageContext->state) {
    case 0:
        MapObjectMan_PauseAllMovement(fieldSystem->mapObjMan);
        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
        usageContext->state = 1;
        break;
    case 1:
        if (IsScreenFadeDone()) {
            usageContext->appWork = usageContext->appCtor(fieldSystem);
            usageContext->state = 2;
        }
        break;
    case 2:
        if (FieldSystem_IsRunningApplication(fieldSystem)) {
            break;
        }

        if (usageContext->appWork != NULL) {
            if (usageContext->appCtor == ItemUse_OpenPoffinCaseApp) {
                PoffinCaseAppData_Free(usageContext->appWork);
            } else {
                Heap_Free(usageContext->appWork);
            }
        }

        FieldSystem_StartFieldMap(fieldSystem);
        usageContext->state = 3;
        break;
    case 3:
        if (FieldSystem_IsRunningFieldMap(fieldSystem)) {
            MapObjectMan_PauseAllMovement(fieldSystem->mapObjMan);
            FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
            usageContext->state = 4;
        }
        break;
    case 4:
        if (IsScreenFadeDone()) {
            MapObjectMan_UnpauseAllMovement(fieldSystem->mapObjMan);
            Heap_Free(usageContext);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

// Records the app constructor and starts the task that will run it.
static void RegisteredItem_CreateGoToAppTask(ItemFieldUseContext *usageContext, void *appCtor)
{
    usageContext->appCtor = appCtor;
    FieldSystem_CreateTask(usageContext->fieldSystem, RegisteredItem_GoToApp, usageContext);
}
