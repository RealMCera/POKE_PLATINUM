#ifndef POKEPLATINUM_PARTY_MENU_MAIN_H
#define POKEPLATINUM_PARTY_MENU_MAIN_H

#include "constants/heap.h"

#include "applications/party_menu/defs.h"

#include "overlay_manager.h"

// Public interface of the party menu application (see
// src/applications/party_menu/main.c). The selection result constants below are
// the values returned by PartyMenu_CheckEligibility and friends.

#define PARTY_MENU_SELECTION_INELIGIBLE 0
#define PARTY_MENU_SELECTION_ELIGIBLE   1
#define PARTY_MENU_SELECTION_ENTERED    2

enum PartyMenuInput {
    PARTY_MENU_INPUT_CONFIRM,
    PARTY_MENU_INPUT_DIRECTION_PAD,
    PARTY_MENU_INPUT_TOUCH_SCREEN,
    PARTY_MENU_INPUT_CANCEL,
    PARTY_MENU_INPUT_4,
    PARTY_MENU_INPUT_NONE
};

extern const ApplicationManagerTemplate gPokemonPartyAppTemplate;

void PartyMenu_UpdateFormChangeGraphicsMode(PartyMenuApplication *application, BOOL isTeardown);
u8 PartyMenu_IsMemberPresent(PartyMenuApplication *application, u8 param1);
u8 PartyMenu_LoadMember(PartyMenuApplication *application, u8 slot);
const u16 *PartyMenu_GetHealthbarTilemap(PartyMenuApplication *application);
void PartyMenu_UpdateSlotPalette(PartyMenuApplication *application, u8 slot);
void PartyMenu_SetCursorToSlot(PartyMenuApplication *application, u8 partySlot);
u8 PartyMenu_CheckEligibility(PartyMenuApplication *application, u8 slot);
u8 PartyMenu_CheckBattleHallEligibility(PartyMenuApplication *application, u8 slot);
u8 PartyMenu_CheckBattleCastleEligibility(PartyMenuApplication *application, u8 slot);
u8 PartyMenu_GetMemberPanelAnim(u8 menuType, u8 slot);
u8 GetFieldMoveIndex(u16 moveID);
u32 PartyMenu_GetIconCharResourceID(void);
u32 PartyMenu_GetIconPaletteResourceID(void);
u32 PartyMenu_GetIconCellResourceID(void);
u32 PartyMenu_GetIconAnimResourceID(void);
void PartyMenu_LoadMemberPanelTilemaps(enum HeapID heapID, u16 *leadMember, u16 *backMembers, u16 *noMember);
void PartyMenu_SetupFormChangeAnim(PartyMenuApplication *application);
void PartyMenu_TeardownFormChangeAnim(PartyMenuApplication *application);

#endif // POKEPLATINUM_PARTY_MENU_MAIN_H
