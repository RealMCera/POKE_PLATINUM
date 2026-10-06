#include <nitro.h>
#include <string.h>

#include "applications/mail.h"
#include "applications/party_menu/defs.h"
#include "applications/party_menu/main.h"
#include "field/field_system.h"
#include "overlay005/map_prop_animation.h"
#include "overlay005/ov5_021D431C.h"
#include "overlay006/pc_animation.h"

#include "bag.h"
#include "bg_window.h"
#include "field_system.h"
#include "field_task.h"
#include "font.h"
#include "game_options.h"
#include "heap.h"
#include "item.h"
#include "list_menu.h"
#include "mail.h"
#include "menu.h"
#include "message.h"
#include "party.h"
#include "pokemon.h"
#include "render_text.h"
#include "render_window.h"
#include "save_player.h"
#include "savedata.h"
#include "screen_fade.h"
#include "scroll_prompts.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "string_list.h"
#include "string_template.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "system.h"
#include "text.h"
#include "field_system_apps.h"

// The Mailbox application, opened from the field by script command 0x1B3
// (Mailbox_StartFieldTask). It lists the mail stored in the player's mailbox
// and lets the player read it, erase it (turning it into the item it holds), or
// give it to a party Pokémon. The application is driven by a single SysTask
// whose callback is swapped between the functions below as the player moves
// between the mail list, the action menu, and the confirmation flows.

// One slot of the mailbox as shown in the mail list. The valid entries form a
// circular doubly-linked list threaded through prevSlot/nextSlot, so that
// erasing an entry does not disturb the order of the rest.
typedef struct {
    u8 slot; // index of this entry in the mailbox (0..MAILBOX_SIZE-1)
    u8 isValid; // whether the mailbox slot actually holds mail
    u8 prevSlot; // previous valid entry in the list
    u8 nextSlot; // next valid entry in the list
    u8 trainerGender; // gender of the trainer who sent the mail
    u8 mailType; // mail type, used to derive the held item
    u16 item; // item the mail turns into when erased
    String *trainerName; // name of the trainer who sent the mail
} MailboxEntry;

// An entry of the action menu (READ / ERASE / GIVE / CANCEL) shown when a mail
// is selected. messageID indexes the mailbox text bank; action is the value
// returned by the list menu when the entry is chosen.
typedef struct {
    u32 messageID;
    u32 action;
} MailboxAction;

// Strings loaded from the mailbox text bank. prompts holds the six contextual
// messages (indices 6..11) indexed by the first argument of Mailbox_PrintMessage.
typedef struct {
    StringTemplate *template;
    String *formattedPrompt; // scratch buffer for the templated prompt
    String *cancelString; // "CANCEL", appended to the mail list
    String *titleString; // "MAILBOX", drawn in the title window
    String *prompts[6];
} MailboxStrings;

// State for the mailbox application. The SysTask callback is swapped between
// the functions below as the player moves between the mail list, the action
// menu, and the various confirmation flows.
typedef struct {
    enum HeapID heapID;
    int *result; // set to 1 when the application is closing
    SysTask *task; // the application's main SysTask
    SysTaskFunc returnCallback; // callback to resume after a sub-flow finishes
    u16 state; // state of the current SysTask callback
    u16 subState; // state of the current fade / party-menu / mail-app helper
    u16 messageBoxFrame; // window frame style from the game options
    u8 textSpeed; // text frame delay from the game options
    u8 selectedPartySlot; // party slot chosen for giving mail
    u8 selectedMailSlot; // mailbox slot currently selected in the list
    u8 lastSlot; // tail of the circular list of valid entries
    u8 firstSlot; // head of the circular list of valid entries
    u8 mailCount; // number of valid entries
    MailboxEntry entries[MAILBOX_SIZE];
    MessageLoader *messageLoader;
    MailboxStrings strings;
    u8 textPrinterID; // handle of the currently printing message
    u8 eraseMessageBoxOnFinish; // always passed 0 by this module
    u8 unk_13A;
    u8 menuMode : 4; // 0 while the mail list is open, 1 while the action menu is open
    u8 mailErased : 2; // set when an entry is unlinked, so the cursor is clamped
    u8 canStoreMailItem : 2; // whether the erased mail's item fits in the Bag
    u16 listCursorPos; // saved cursor position of the mail list
    u16 listScrollPos; // saved scroll position of the mail list
    ListMenuTemplate listMenuTemplate;
    ListMenu *listMenu;
    StringList *stringList;
    Menu *yesNoMenu;
    FieldSystem *fieldSystem;
    BgConfig *bgConfig;
    Window listWindow; // window holding the mail list
    Window messageBoxWindow; // window holding the contextual message
    Window titleWindow; // window holding the "MAILBOX" title
    ScrollPrompts *scrollPrompts;
    MailAppArgs *mailAppArgs; // arguments for the read/write mail application
    Mailbox *mailbox;
    Bag *bag;
    PartyMenu *partyMenu;
} MailboxApp;

// Environment for the field task that runs the mailbox application.
typedef struct {
    int done; // set to 1 by Mailbox_Start once the application has closed
    int state; // field task state
} MailboxFieldTaskEnv;

void Mailbox_StartFieldTask(FieldTask *param0);
void Mailbox_Start(void *param0, int *param1);
static void Mailbox_Free(MailboxApp *param0);
static void Mailbox_CloseTask(SysTask *param0, void *param1);
static void Mailbox_EnterList(SysTask *param0, void *param1);
static void Mailbox_HandleListInput(SysTask *param0, void *param1);
static void Mailbox_ShowActionPrompt(SysTask *param0, void *param1);
static void Mailbox_HandleActionInput(SysTask *param0, void *param1);
static void Mailbox_ExitMailList(SysTask *param0, void *param1);
static void Mailbox_ExitActionMenu(SysTask *param0, void *param1);
static void Mailbox_ReadMailFlow(SysTask *param0, void *param1);
static void Mailbox_EraseMailFlow(SysTask *param0, void *param1);
static void Mailbox_EraseMailFinish(SysTask *param0, void *param1);
static void Mailbox_GiveMailToPartyFlow(SysTask *param0, void *param1);
static void Mailbox_CancelGiveMail(SysTask *param0, void *param1);
static void Mailbox_EraseThenGiveFlow(SysTask *param0, void *param1);
static void Mailbox_SetTaskCallback(MailboxApp *param0, SysTaskFunc param1, SysTaskFunc param2);
static void Mailbox_ResetEntry(MailboxEntry *param0, u8 param1);
static void Mailbox_InitEntries(MailboxEntry *param0, u8 param1, enum HeapID heapID);
static void Mailbox_FreeEntries(MailboxEntry *param0, u8 param1);
static void Mailbox_CreateMailList(MailboxApp *param0);
static void Mailbox_PrintListEntry(ListMenu *param0, u32 param1, u8 param2);
static void Mailbox_UpdateScrollArrows(ListMenu *param0, u32 param1, u8 param2);
static void Mailbox_CreateActionMenu(MailboxApp *param0, u8 param1, u8 param2);
static void Mailbox_PlayCursorSound(ListMenu *param0, u32 param1, u8 param2);
static void Mailbox_DestroyMailList(MailboxApp *param0);
static void Mailbox_DestroyActionMenu(MailboxApp *param0);
static void Mailbox_LoadMailbox(MailboxApp *param0, SaveData *saveData, enum HeapID heapID);
static void Mailbox_UnlinkEntry(MailboxApp *param0, u8 param1);
static BOOL Mailbox_EraseMail(MailboxApp *param0);
static void Mailbox_GiveMailToPokemon(MailboxApp *param0, u8 param1, BOOL param2);
static void Mailbox_LoadStrings(MailboxApp *param0);
static void Mailbox_FreeStrings(MailboxApp *param0);
static void Mailbox_LoadWindowGraphics(MailboxApp *param0);
static void Mailbox_UnloadMessageBox(MailboxApp *param0);
static void Mailbox_PrintMessage(MailboxApp *param0, int param1, u8 param2, u8 param3, int param4);
static void Mailbox_EraseMessageBox(MailboxApp *param0);
static BOOL Mailbox_IsTextDone(MailboxApp *param0);
static void Mailbox_CreateYesNoMenu(MailboxApp *param0);
static int Mailbox_ProcessYesNoInput(MailboxApp *param0);
static int Mailbox_RunScreenFade(MailboxApp *param0, int param1);
static int Mailbox_LoadFieldMap(MailboxApp *param0);
static int Mailbox_ExitMenu(MailboxApp *param0, u8 param1);
static int Mailbox_LaunchPartyMenu(MailboxApp *param0, int mode);
static int Mailbox_LaunchMailApp(MailboxApp *param0);

static const ListMenuTemplate sMailboxListTemplate = {
    NULL,
    NULL,
    NULL,
    NULL,
    0x0,
    0x7,
    0x2,
    0xA,
    0x1,
    0x0,
    0x1,
    0xF,
    0x2,
    0x0,
    0x1,
    0x1,
    0x0,
    0x0,
    NULL
};

static const MailboxAction sMailboxActions[] = {
    { 0x1, 0x0 },
    { 0x2, 0x1 },
    { 0x3, 0x2 },
    { 0x4, 0x3 }
};

// Allocates the application state, loads the mailbox and its strings, and
// starts the SysTask that drives the UI. *param1 is set to 1 once the
// application has closed.
void Mailbox_Start(void *param0, int *param1)
{
    MailboxApp *v0 = NULL;
    SaveData *saveData;

    Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_43, 0x5000);

    v0 = Heap_Alloc(HEAP_ID_43, sizeof(MailboxApp));
    MI_CpuClear8(v0, sizeof(MailboxApp));
    v0->result = param1;

    saveData = FieldSystem_GetSaveData(param0);

    v0->fieldSystem = (FieldSystem *)param0;
    v0->heapID = HEAP_ID_43;
    v0->lastSlot = 0;
    v0->firstSlot = 0xFF;
    v0->textSpeed = Options_TextFrameDelay(SaveData_GetOptions(saveData));
    v0->messageBoxFrame = Options_Frame(SaveData_GetOptions(saveData));

    Mailbox_InitEntries(v0->entries, 20, v0->heapID);
    Mailbox_LoadMailbox(v0, saveData, v0->heapID);
    Mailbox_LoadStrings(v0);

    v0->task = SysTask_Start(Mailbox_EnterList, v0, 0);
}

void Mailbox_Free(MailboxApp *param0)
{
    SysTask_Done(param0->task);
    *(param0->result) = 1;
    Mailbox_FreeStrings(param0);
    Mailbox_FreeEntries(param0->entries, 20);
    Heap_Free(param0);
    Heap_Destroy(param0->heapID);
}

static void Mailbox_CloseTask(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;
    Mailbox_Free(v0);
}

static void Mailbox_EnterList(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;

    Mailbox_LoadWindowGraphics(v0);
    Mailbox_CreateMailList(v0);
    SysTask_SetCallback(param0, Mailbox_HandleListInput);
}

// Handles input on the mail list. Selecting a mail (A) opens the action menu;
// B or the trailing CANCEL entry (0xFFFF) closes the application.
static void Mailbox_HandleListInput(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;
    s32 v1 = ListMenu_ProcessInput(v0->listMenu);

    if (v0->scrollPrompts != NULL) {
        ScrollPrompts_UpdateAnim(v0->scrollPrompts);
    }

    if (gSystem.pressedKeys & PAD_BUTTON_B) {
        Mailbox_SetTaskCallback(v0, Mailbox_ExitMailList, Mailbox_CloseTask);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
        return;
    }

    if (gSystem.pressedKeys & PAD_BUTTON_A) {
        Sound_PlayEffect(SE_CONFIRM_sseq_3);

        switch (v1) {
        case 0xffffffff:
        case 0xfffffffe:
        case 0xFFFF:
            Mailbox_SetTaskCallback(v0, Mailbox_ExitMailList, Mailbox_CloseTask);
            break;
        default:
            v0->selectedMailSlot = v1;
            Mailbox_SetTaskCallback(v0, Mailbox_ExitMailList, Mailbox_ShowActionPrompt);
            break;
        }
    }

    return;
}

// Draws the action menu over the mail list and prints the "What would you like
// to do with <name>'s Mail?" prompt.
static void Mailbox_ShowActionPrompt(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;

    switch (v0->state) {
    case 0:
        Mailbox_LoadWindowGraphics(v0);
        Mailbox_CreateActionMenu(v0, 0, 0);
        Mailbox_PrintMessage(v0, 0, v0->textSpeed, 0, 1);
        ++v0->state;
        break;
    case 1:
        if (!Mailbox_IsTextDone(v0)) {
            break;
        }

        SysTask_SetCallback(param0, Mailbox_HandleActionInput);
        v0->state = 0;
        break;
    }

    return;
}

// Dispatches the action chosen in the action menu: 0 = READ, 1 = ERASE,
// 2 = GIVE, 3 = CANCEL.
static void Mailbox_HandleActionInput(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;
    s32 v1 = ListMenu_ProcessInput(v0->listMenu);

    if (gSystem.pressedKeys & PAD_BUTTON_B) {
        Mailbox_SetTaskCallback(v0, Mailbox_ExitActionMenu, Mailbox_EnterList);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
        return;
    }

    if (gSystem.pressedKeys & PAD_BUTTON_A) {
        Sound_PlayEffect(SE_CONFIRM_sseq_3);

        switch (v1) {
        case 0xffffffff:
        case 0xfffffffe:
        case 3:
            Mailbox_SetTaskCallback(v0, Mailbox_ExitActionMenu, Mailbox_EnterList);
            break;
        case 1:
            Mailbox_SetTaskCallback(v0, Mailbox_EraseMailFlow, NULL);
            break;
        case 2:
            Mailbox_SetTaskCallback(v0, Mailbox_GiveMailToPartyFlow, Mailbox_CancelGiveMail);
            break;
        case 0:
        default:
            SysTask_SetCallback(param0, Mailbox_ReadMailFlow);
            break;
        }
    }

    return;
}

// Tears down the mail list and message box, then hands control to the
// callback stored in returnCallback.
static void Mailbox_ExitMailList(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;

    Mailbox_DestroyMailList(v0);
    Mailbox_UnloadMessageBox(v0);
    SysTask_SetCallback(param0, v0->returnCallback);
}

// Tears down the action menu and message box, then hands control to the
// callback stored in returnCallback.
static void Mailbox_ExitActionMenu(SysTask *param0, void *param1)
{
    MailboxApp *v0 = (MailboxApp *)param1;

    Mailbox_DestroyActionMenu(v0);
    Mailbox_UnloadMessageBox(v0);
    SysTask_SetCallback(param0, v0->returnCallback);
}

// READ: fades out, opens the mail viewer application, then restores the field
// and rebuilds the mail list.
static void Mailbox_ReadMailFlow(SysTask *param0, void *param1)
{
    int v0;
    MailboxApp *v1 = (MailboxApp *)param1;

    switch (v1->state) {
    case 0:
        if (!Mailbox_RunScreenFade(v1, 0)) {
            return;
        }

        Mailbox_ExitMenu(v1, 1);
        break;
    case 1:
        if (!Mailbox_LaunchMailApp(v1)) {
            return;
        }

        break;
    case 2:
        if (!Mailbox_LoadFieldMap(v1)) {
            return;
        }

        break;
    case 3:
        if (!Mailbox_RunScreenFade(v1, 1)) {
            return;
        }

        Mailbox_LoadWindowGraphics(v1);
        Mailbox_CreateMailList(v1);
        v1->state = 0;
        SysTask_SetCallback(param0, Mailbox_HandleListInput);
        return;
    }

    v1->state++;
    return;
}

// ERASE: asks "The message will be lost. Is that OK?". If the player declines,
// it returns to the action menu. If they accept, it asks whether a Pokémon
// should hold the erased Mail: yes goes to Mailbox_EraseThenGiveFlow, no goes
// to Mailbox_EraseMailFinish (which puts the resulting item in the Bag).
static void Mailbox_EraseMailFlow(SysTask *param0, void *param1)
{
    int v0;
    MailboxApp *v1 = (MailboxApp *)param1;

    switch (v1->state) {
    case 0:
        Mailbox_PrintMessage(v1, 1, v1->textSpeed, 0, 1);
        break;
    case 1:
        if (!Mailbox_IsTextDone(v1)) {
            return;
        }

        Mailbox_CreateYesNoMenu(v1);
        break;
    case 2:
        v0 = Mailbox_ProcessYesNoInput(v1);

        if (v0 < 0) {
            return;
        }

        if (v0) {
            Mailbox_PrintMessage(v1, 2, v1->textSpeed, 0, 0);
            v1->state = 4;
        } else {
            v1->state = 3;
        }
        return;
    case 3:
        Mailbox_EraseMessageBox(v1);
        Mailbox_SetTaskCallback(v1, Mailbox_ExitActionMenu, Mailbox_EnterList);
        v1->state = 0;
        return;
    case 4:
        if (!Mailbox_IsTextDone(v1)) {
            return;
        }

        Mailbox_CreateYesNoMenu(v1);
        break;
    case 5:
        v0 = Mailbox_ProcessYesNoInput(v1);

        if (v0 < 0) {
            return;
        }

        if (v0) {
            Mailbox_SetTaskCallback(v1, Mailbox_EraseThenGiveFlow, NULL);
        } else {
            Mailbox_SetTaskCallback(v1, Mailbox_EraseMailFinish, Mailbox_ExitActionMenu);
        }

        Mailbox_EraseMessageBox(v1);
        v1->state = 0;
        return;
    }

    v1->state++;
    return;
}

// Erases the selected mail and reports whether its item fit in the Bag, then
// returns to the action menu.
static void Mailbox_EraseMailFinish(SysTask *param0, void *param1)
{
    int v0;
    MailboxApp *v1 = (MailboxApp *)param1;

    switch (v1->state) {
    case 0:
        if (v1->returnCallback == NULL) {
            Mailbox_LoadWindowGraphics(v1);
            Mailbox_CreateActionMenu(v1, 0, 1);
        }

        if (Mailbox_EraseMail(v1)) {
            Mailbox_PrintMessage(v1, 3, v1->textSpeed, 0, 1);
        } else {
            Mailbox_PrintMessage(v1, 4, v1->textSpeed, 0, 1);
        }
        break;
    case 1:
        if (!Mailbox_IsTextDone(v1)) {
            return;
        }
        break;
    case 2:
        if (!(gSystem.pressedKeys & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            return;
        }

        Mailbox_EraseMessageBox(v1);
        Mailbox_SetTaskCallback(v1, Mailbox_ExitActionMenu, Mailbox_EnterList);

        v1->state = 0;
        return;
    }

    v1->state++;
    return;
}

// GIVE: opens the party menu in mailbox mode and transfers the selected mail to
// the chosen Pokémon. Slot 7 means the player cancelled.
static void Mailbox_GiveMailToPartyFlow(SysTask *param0, void *param1)
{
    u8 v0;
    PartyMenu *partyMenu;
    MailboxApp *v2 = (MailboxApp *)param1;

    switch (v2->state) {
    case 0:
        if (!Mailbox_RunScreenFade(v2, 0)) {
            return;
        }

        Mailbox_ExitMenu(v2, 1);
        break;
    case 1:
        if (!Mailbox_LaunchPartyMenu(v2, PARTY_MENU_MODE_MAILBOX)) {
            return;
        }
        break;
    case 2:
        if (!Mailbox_LoadFieldMap(v2)) {
            return;
        }
        break;
    case 3:
        if (!Mailbox_RunScreenFade(v2, 1)) {
            return;
        }

        v0 = v2->partyMenu->selectedMonSlot;
        Heap_Free(v2->partyMenu);

        if (v0 == 7) {
            Mailbox_SetTaskCallback(v2, v2->returnCallback, NULL);
            v2->state = 0;
            return;
        }

        Mailbox_GiveMailToPokemon(v2, v0, 0);
        Mailbox_SetTaskCallback(v2, Mailbox_EnterList, NULL);
        v2->state = 0;
        return;
    }

    v2->state++;
    return;
}

// Shown when the player backs out of giving a mail to a Pokémon: prints
// "Stopped making the Pokémon hold Mail." and returns to the action menu.
static void Mailbox_CancelGiveMail(SysTask *param0, void *param1)
{
    int v0;
    MailboxApp *v1 = (MailboxApp *)param1;

    switch (v1->state) {
    case 0:
        Mailbox_LoadWindowGraphics(v1);
        Mailbox_CreateActionMenu(v1, 0, 2);
        Mailbox_PrintMessage(v1, 5, v1->textSpeed, 0, 1);
        break;
    case 1:
        if (!Mailbox_IsTextDone(v1)) {
            return;
        }
        break;
    case 2:
        if (!(gSystem.pressedKeys & (PAD_BUTTON_A | PAD_BUTTON_B))) {
            return;
        }

        Mailbox_EraseMessageBox(v1);
        Mailbox_SetTaskCallback(v1, Mailbox_ExitActionMenu, Mailbox_EnterList);
        v1->state = 0;
        return;
    }

    v1->state++;
    return;
}

// ERASE -> "let a Pokémon hold this Mail?" -> yes. Fades out and opens the
// party menu to pick a Pokémon (GIVE_ITEM). If the player cancels, the mail is
// erased to the Bag via Mailbox_EraseMailFinish. Otherwise the mail app is
// opened to write a new message; if none is written the mail is erased to the
// Bag, and if one is written the mail is given to the chosen Pokémon
// (GIVE_MAIL) before the field is restored.
static void Mailbox_EraseThenGiveFlow(SysTask *param0, void *param1)
{
    int v0;
    u8 v1, v2;
    PartyMenu *partyMenu;
    MailboxApp *v4 = (MailboxApp *)param1;

    switch (v4->state) {
    case 0:
        if (!Mailbox_RunScreenFade(v4, 0)) {
            return;
        }

        Mailbox_ExitMenu(v4, 1);

        if (Bag_GetItemQuantity(v4->bag, v4->entries[v4->selectedMailSlot].item, v4->heapID) > 0) {
            v4->canStoreMailItem = 1;
        } else {
            v4->canStoreMailItem = 0;
        }
        break;
    case 1:

        if (!Mailbox_LaunchPartyMenu(v4, PARTY_MENU_MODE_GIVE_ITEM)) {
            return;
        }

        v1 = v4->partyMenu->selectedMonSlot;
        v2 = v4->partyMenu->menuSelectionResult;

        Heap_Free(v4->partyMenu);

        if ((v2 != 6) || (v1 == 7)) {
            v4->returnCallback = Mailbox_EraseMailFinish;
            v4->state = 4;
            return;
        }

        v4->selectedPartySlot = v1;
        v4->entries[v4->selectedMailSlot].isValid = 0;
        break;
    case 2:
        v0 = Mailbox_LaunchMailApp(v4);
        if (!v0) {
            return;
        }

        if (v0 == 1) {
            v4->returnCallback = Mailbox_EraseMailFinish;
            v4->state = 4;
            return;
        }

        v4->returnCallback = Mailbox_EnterList;
        break;
    case 3:
        if (!Mailbox_LaunchPartyMenu(v4, PARTY_MENU_MODE_GIVE_MAIL)) {
            return;
        }

        Heap_Free(v4->partyMenu);
        Mailbox_GiveMailToPokemon(v4, v4->selectedPartySlot, v4->canStoreMailItem);
        v4->canStoreMailItem = 0;
        break;
    case 4:
        if (!Mailbox_LoadFieldMap(v4)) {
            return;
        }
        break;
    case 5:
        if (!Mailbox_RunScreenFade(v4, 1)) {
            return;
        }

        Mailbox_SetTaskCallback(v4, v4->returnCallback, NULL);
        v4->state = 0;
        return;
    }

    ++v4->state;
}

// Builds the mail list: one entry per valid mailbox slot (labelled with the
// sender's name) plus a trailing CANCEL entry. The list menu's parent pointer
// is the application state, which the callbacks recover via
// ListMenu_GetAttribute(..., LIST_MENU_PARENT).
static void Mailbox_CreateMailList(MailboxApp *param0)
{
    u8 v0 = 0, v1 = 0;
    MailboxEntry *v2;
    static const WindowTemplate v3[2] = {
        { 0x3, 0x13, 0x1, 0xC, 0x10, 0xD, 0x295 },
        { 0x3, 0x1, 0x1, 0x9, 0x2, 0xD, 0x283 }
    };

    param0->scrollPrompts = ScrollPrompts_New(param0->heapID);

    ScrollPrompts_SetPosition(param0->scrollPrompts, 200, 10, 138);
    ScrollPrompts_SetDrawFlag(param0->scrollPrompts, SCROLL_PROMPT_TOP_ARROW, TRUE);
    ScrollPrompts_SetDrawFlag(param0->scrollPrompts, SCROLL_PROMPT_BOTTOM_ARROW, TRUE);
    Window_AddFromTemplate(param0->bgConfig, &(param0->listWindow), &v3[0]);
    Window_AddFromTemplate(param0->bgConfig, &(param0->titleWindow), &v3[1]);
    Window_FillTilemap(&param0->listWindow, (15 << 4) | 15);
    Window_FillTilemap(&param0->titleWindow, (15 << 4) | 15);

    param0->stringList = StringList_New(param0->mailCount + 1, param0->heapID);

    for (v0 = 0; v0 < 20; v0++) {
        v2 = &(param0->entries[v0]);

        if (!v2->isValid) {
            continue;
        }

        StringList_AddFromString(param0->stringList, v2->trainerName, v2->slot);
        v1++;
    }

    StringList_AddFromString(param0->stringList, param0->strings.cancelString, 0xFFFF);
    v1++;

    MI_CpuCopy8((void *)&sMailboxListTemplate, (void *)&(param0->listMenuTemplate), sizeof(ListMenuTemplate));

    param0->listMenuTemplate.window = &(param0->listWindow);
    param0->listMenuTemplate.choices = param0->stringList;
    param0->listMenuTemplate.parent = (void *)param0;
    param0->listMenuTemplate.count = v1;
    param0->listMenuTemplate.yOffset = 6;
    param0->listMenuTemplate.cursorCallback = Mailbox_UpdateScrollArrows;
    param0->listMenuTemplate.printCallback = Mailbox_PrintListEntry;

    if (param0->mailErased) {
        if (param0->listCursorPos == 0) {
            if ((param0->listScrollPos != 0) && (param0->listScrollPos >= (v1 - 1))) {
                --param0->listScrollPos;
            }
        } else {
            if (param0->listCursorPos + 7 >= v1) {
                --param0->listCursorPos;
            }
        }

        param0->mailErased = 0;
    }

    param0->listMenu = ListMenu_New(&(param0->listMenuTemplate), param0->listCursorPos, param0->listScrollPos, param0->heapID);

    Window_DrawStandardFrame(&param0->listWindow, 0, 1024 - (18 + 12) - 9, 11);
    Text_AddPrinterWithParamsAndColor(&param0->titleWindow, FONT_SYSTEM, param0->strings.titleString, 2, 0, TEXT_SPEED_INSTANT, TEXT_COLOR(1, 2, 15), NULL);
    Window_DrawStandardFrame(&param0->titleWindow, 0, 1024 - (18 + 12) - 9, 11);
    Bg_ScheduleTilemapTransfer(param0->bgConfig, 3);

    param0->menuMode = 0;
}

// List-menu print callback: tints each sender's name by their gender, and the
// CANCEL entry with its own colour.
static void Mailbox_PrintListEntry(ListMenu *param0, u32 param1, u8 param2)
{
    MailboxApp *v0 = (MailboxApp *)ListMenu_GetAttribute(param0, 19);

    if (param1 == 0xFFFF) {
        ListMenu_SetAltTextColors(param0, 1, 15, 2);
    } else {
        if (v0->entries[param1].trainerGender != GENDER_MALE) {
            ListMenu_SetAltTextColors(param0, 3, 15, 4);
        } else {
            ListMenu_SetAltTextColors(param0, 7, 15, 8);
        }
    }
}

// List-menu cursor callback: shows or hides the scroll arrows depending on
// whether the list is scrolled to the top or bottom.
static void Mailbox_UpdateScrollArrows(ListMenu *param0, u32 param1, u8 param2)
{
    u16 v0, v1, v2;
    MailboxApp *v3 = (MailboxApp *)ListMenu_GetAttribute(param0, 19);

    ListMenu_GetListAndCursorPos(param0, &v0, &v1);
    v2 = ListMenu_GetAttribute(param0, 2);

    if (!param2) {
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
    }

    if (v0 == 0) {
        ScrollPrompts_SetDrawFlag(v3->scrollPrompts, SCROLL_PROMPT_TOP_ARROW, FALSE);
    } else {
        ScrollPrompts_SetDrawFlag(v3->scrollPrompts, SCROLL_PROMPT_TOP_ARROW, TRUE);
    }

    if (v0 < (v2 - 7)) {
        ScrollPrompts_SetDrawFlag(v3->scrollPrompts, SCROLL_PROMPT_BOTTOM_ARROW, TRUE);
    } else {
        ScrollPrompts_SetDrawFlag(v3->scrollPrompts, SCROLL_PROMPT_BOTTOM_ARROW, FALSE);
    }
}

// Builds the action menu (READ / ERASE / GIVE / CANCEL) from sMailboxActions.
static void Mailbox_CreateActionMenu(MailboxApp *param0, u8 param1, u8 param2)
{
    u8 v0 = 0, v1 = 0;
    static const WindowTemplate v2 = {
        0x3,
        0x1,
        0x1,
        0xE,
        0x8,
        0xD,
        0x295
    };

    v1 = NELEMS(sMailboxActions);
    param0->stringList = StringList_New(v1, param0->heapID);

    Window_AddFromTemplate(param0->bgConfig, &(param0->listWindow), &v2);
    Window_FillTilemap(&param0->listWindow, (15 << 4) | 15);

    for (v0 = 0; v0 < v1; v0++) {
        StringList_AddFromMessageBank(param0->stringList, param0->messageLoader, sMailboxActions[v0].messageID, sMailboxActions[v0].action);
    }

    MI_CpuCopy8((void *)&sMailboxListTemplate, (void *)&(param0->listMenuTemplate), sizeof(ListMenuTemplate));

    param0->listMenuTemplate.window = &(param0->listWindow);
    param0->listMenuTemplate.choices = param0->stringList;
    param0->listMenuTemplate.parent = (void *)param0;
    param0->listMenuTemplate.count = v1;
    param0->listMenuTemplate.maxDisplay = 4;
    param0->listMenuTemplate.pagerMode = PAGER_MODE_NONE;
    param0->listMenuTemplate.cursorCallback = Mailbox_PlayCursorSound;
    param0->listMenu = ListMenu_New(&(param0->listMenuTemplate), param1, param2, param0->heapID);

    Window_DrawStandardFrame(&param0->listWindow, 0, 1024 - (18 + 12) - 9, 11);
    Bg_ScheduleTilemapTransfer(param0->bgConfig, 3);

    param0->menuMode = 1;
}

static void Mailbox_PlayCursorSound(ListMenu *param0, u32 param1, u8 param2)
{
    if (!param2) {
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
    }
}

static void Mailbox_DestroyMailList(MailboxApp *param0)
{
    u16 v0, v1;

    ListMenu_Free(param0->listMenu, &v0, &v1);
    StringList_Free(param0->stringList);

    Window_ClearAndCopyToVRAM(&(param0->titleWindow));
    Window_EraseStandardFrame(&(param0->titleWindow), 0);
    Window_Remove(&(param0->titleWindow));

    Window_ClearAndCopyToVRAM(&(param0->listWindow));
    Window_EraseStandardFrame(&(param0->listWindow), 0);
    Window_Remove(&(param0->listWindow));

    param0->listCursorPos = v0;
    param0->listScrollPos = v1;

    if (param0->scrollPrompts != NULL) {
        ScrollPrompts_Free(param0->scrollPrompts);
        param0->scrollPrompts = NULL;
    }

    Bg_ScheduleTilemapTransfer(param0->bgConfig, 3);
}

static void Mailbox_DestroyActionMenu(MailboxApp *param0)
{
    u16 v0, v1;

    Window_ClearAndCopyToVRAM(&(param0->listWindow));
    Window_EraseStandardFrame(&(param0->listWindow), 0);
    ListMenu_Free(param0->listMenu, &v0, &v1);
    StringList_Free(param0->stringList);
    Window_Remove(&(param0->listWindow));
    Mailbox_EraseMessageBox(param0);
    Bg_ScheduleTilemapTransfer(param0->bgConfig, 3);
}

// Switches the main SysTask to param1 and remembers param2 as the callback to
// resume once the new flow finishes.
static void Mailbox_SetTaskCallback(MailboxApp *param0, SysTaskFunc param1, SysTaskFunc param2)
{
    SysTask_SetCallback(param0->task, param1);

    param0->state = 0;
    param0->returnCallback = param2;
}

static void Mailbox_ResetEntry(MailboxEntry *param0, u8 param1)
{
    param0->slot = param1;
    param0->isValid = 0;
    param0->prevSlot = 0;
    param0->nextSlot = 0;

    if (param0->trainerName != NULL) {
        String_Clear(param0->trainerName);
    }
}

static void Mailbox_InitEntries(MailboxEntry *param0, u8 param1, enum HeapID heapID)
{
    u8 v0 = 0;

    for (v0 = 0; v0 < param1; v0++) {
        param0[v0].trainerName = String_Init(8, heapID);
        Mailbox_ResetEntry(param0, v0);
    }
}

static void Mailbox_FreeEntries(MailboxEntry *param0, u8 param1)
{
    u8 v0 = 0;

    for (v0 = 0; v0 < param1; v0++) {
        if (param0[v0].trainerName != NULL) {
            String_Free(param0[v0].trainerName);
        }
    }
}

// Reads every mailbox slot into entries[] and threads the valid ones into a
// circular doubly-linked list (firstSlot..lastSlot) so erasing an entry does
// not disturb the order of the rest.
static void Mailbox_LoadMailbox(MailboxApp *param0, SaveData *saveData, enum HeapID heapID)
{
    u8 i = 0, v1 = 0, v2 = 0xFF, v3 = 0;
    int v4;
    Mailbox *mailbox;
    Mail *mail;
    MailboxEntry *v7, *v8;

    mailbox = SaveData_GetMailbox(saveData);

    param0->mailbox = mailbox;
    param0->bag = SaveData_GetBag(saveData);

    mail = Mail_New(heapID);

    for (i = 0; i < MAILBOX_SIZE; i++) {
        Mailbox_GetMailAtSlot(mailbox, MAIL_CONTEXT_MAILBOX, i, mail);

        v7 = &(param0->entries[i]);
        v8 = &(param0->entries[param0->lastSlot]);

        Mailbox_ResetEntry(v7, i);

        v7->slot = i;

        if (!Mail_IsValid(mail)) {
            continue;
        }

        v7->isValid = 1;
        v7->trainerGender = Mail_GetTrainerGender(mail);
        v7->mailType = Mail_GetMailType(mail);
        v7->item = Item_ForMailType(v7->mailType);

        String_CopyChars(v7->trainerName, Mail_GetTrainerName(mail));

        v7->prevSlot = param0->lastSlot;
        v8->nextSlot = v7->slot;

        param0->lastSlot = v7->slot;
        param0->mailCount++;

        if (param0->firstSlot == 0xFF) {
            param0->firstSlot = i;
        }
    }

    param0->entries[param0->lastSlot].nextSlot = param0->firstSlot;
    param0->entries[param0->firstSlot].prevSlot = param0->lastSlot;

    Heap_Free(mail);
}

// Removes an entry from the circular list and marks it invalid. The cursor is
// clamped the next time the list is rebuilt.
static void Mailbox_UnlinkEntry(MailboxApp *param0, u8 param1)
{
    MailboxEntry *v0 = &(param0->entries[param1]);

    param0->entries[v0->prevSlot].nextSlot = v0->nextSlot;
    param0->entries[v0->nextSlot].prevSlot = v0->prevSlot;
    param0->entries[param1].isValid = 0;
    param0->mailErased = 1;
}

// Erases the selected mail, adding the item it turns into to the Bag if there
// is room. Returns whether the item fit.
static BOOL Mailbox_EraseMail(MailboxApp *param0)
{
    MailboxEntry *v0;
    BOOL canFitItem;

    v0 = &(param0->entries[param0->selectedMailSlot]);
    canFitItem = Bag_CanFitItem(param0->bag, v0->item, 1, param0->heapID);

    if (canFitItem) {
        Bag_TryAddItem(param0->bag, v0->item, 1, param0->heapID);
    }

    Mailbox_ClearMailAtSlot(param0->mailbox, MAIL_CONTEXT_MAILBOX, param0->selectedMailSlot);
    Mailbox_UnlinkEntry(param0, param0->selectedMailSlot);
    Mailbox_ResetEntry(v0, param0->selectedMailSlot);

    return canFitItem;
}

// Transfers the selected mail from the mailbox to the given party slot. If
// param2 is set, the item the mail turns into is also added to the Bag.
static void Mailbox_GiveMailToPokemon(MailboxApp *param0, u8 param1, BOOL param2)
{
    MailboxEntry *v0;
    Party *v1;
    Pokemon *v2;

    v0 = &(param0->entries[param0->selectedMailSlot]);

    if (!v0->isValid) {
        return;
    }

    v1 = SaveData_GetParty(FieldSystem_GetSaveData(param0->fieldSystem));
    v2 = Party_GetPokemonBySlotIndex(v1, param1);

    Mail_TransferFromMailboxToMon(param0->mailbox, param0->selectedMailSlot, v2, param0->heapID);

    if (param2) {
        if (Bag_CanFitItem(param0->bag, v0->item, 1, param0->heapID)) {
            Bag_TryAddItem(param0->bag, v0->item, 1, param0->heapID);
        }
    }

    Mailbox_UnlinkEntry(param0, param0->selectedMailSlot);
    Mailbox_ResetEntry(v0, param0->selectedMailSlot);
}

static void Mailbox_LoadStrings(MailboxApp *param0)
{
    MessageLoader *v0;
    int v1;
    String *v2;

    param0->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_MAILBOX, param0->heapID);
    param0->strings.template = StringTemplate_New(1, 128, param0->heapID);
    param0->strings.formattedPrompt = String_Init(128, param0->heapID);
    param0->strings.cancelString = MessageLoader_GetNewString(param0->messageLoader, 4);
    param0->strings.titleString = MessageLoader_GetNewString(param0->messageLoader, 0);

    for (v1 = 0; v1 < 6; v1++) {
        param0->strings.prompts[v1] = MessageLoader_GetNewString(param0->messageLoader, 6 + v1);
    }
}

static void Mailbox_FreeStrings(MailboxApp *param0)
{
    int v0;

    for (v0 = 0; v0 < 6; v0++) {
        String_Free(param0->strings.prompts[v0]);
    }

    String_Free(param0->strings.titleString);
    String_Free(param0->strings.cancelString);
    String_Free(param0->strings.formattedPrompt);
    StringTemplate_Free(param0->strings.template);
    MessageLoader_Free(param0->messageLoader);
}

static void Mailbox_LoadWindowGraphics(MailboxApp *param0)
{
    param0->bgConfig = FieldSystem_GetBgConfig(param0->fieldSystem);

    LoadMessageBoxGraphics(param0->bgConfig, BG_LAYER_MAIN_3, 1024 - (18 + 12), 10, param0->messageBoxFrame, param0->heapID);
    LoadStandardWindowGraphics(param0->bgConfig, BG_LAYER_MAIN_3, 1024 - (18 + 12) - 9, 11, 0, param0->heapID);

    Font_LoadTextPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(13), param0->heapID);
    Font_LoadScreenIndicatorsPalette(PAL_LOAD_MAIN_BG, PLTT_OFFSET(12), param0->heapID);

    Window_Add(param0->bgConfig, &param0->messageBoxWindow, 3, 2, 19, 27, 4, 12, (1024 - (18 + 12) - 9) - 27 * 4);
    Window_FillTilemap(&param0->messageBoxWindow, 0);
}

static void Mailbox_UnloadMessageBox(MailboxApp *param0)
{
    Window_ClearAndCopyToVRAM(&param0->messageBoxWindow);
    Window_Remove(&param0->messageBoxWindow);

    MI_CpuClear8(&param0->messageBoxWindow, sizeof(Window));
    param0->bgConfig = NULL;
}

// Prints strings.prompts[param1] in the message box. Prompt 0 embeds the
// selected mail's sender name via the string template; param2 is the text
// speed, param3 whether to erase the box once printing finishes, and param4
// whether to draw the message box frame first.
static void Mailbox_PrintMessage(MailboxApp *param0, int param1, u8 param2, u8 param3, int param4)
{
    String *v0;

    if (param4) {
        Window_DrawMessageBoxWithScrollCursor(&param0->messageBoxWindow, 1, 1024 - (18 + 12), 10);
    }

    Window_FillRectWithColor(&param0->messageBoxWindow, (15 << 4) | 15, 0, 0, 27 * 8, 4 * 8);
    RenderControlFlags_SetCanABSpeedUpPrint(TRUE);
    RenderControlFlags_SetAutoScrollFlags(AUTO_SCROLL_DISABLED);

    if (param1 == 0) {
        String_Clear(param0->strings.formattedPrompt);
        StringTemplate_SetString(param0->strings.template, 0, param0->entries[param0->selectedMailSlot].trainerName, 2, 1, GAME_LANGUAGE);
        StringTemplate_Format(param0->strings.template, param0->strings.formattedPrompt, param0->strings.prompts[param1]);

        v0 = param0->strings.formattedPrompt;
    } else {
        v0 = param0->strings.prompts[param1];
    }

    param0->textPrinterID = Text_AddPrinterWithParamsAndColor(&param0->messageBoxWindow, FONT_MESSAGE, v0, 0, 0, param2, TEXT_COLOR(1, 2, 15), NULL);
    Window_CopyToVRAM(&param0->messageBoxWindow);
    param0->eraseMessageBoxOnFinish = param3;
}

static void Mailbox_EraseMessageBox(MailboxApp *param0)
{
    Window_EraseMessageBox(&param0->messageBoxWindow, 1);
    Window_ClearAndCopyToVRAM(&param0->messageBoxWindow);
}

static BOOL Mailbox_IsTextDone(MailboxApp *param0)
{
    if (Text_IsPrinterActive(param0->textPrinterID)) {
        return 0;
    }

    if (param0->eraseMessageBoxOnFinish) {
        Mailbox_EraseMessageBox(param0);
    }

    return 1;
}

static void Mailbox_CreateYesNoMenu(MailboxApp *param0)
{
    static const WindowTemplate v0 = {
        0x3,
        0x19,
        0xD,
        0x6,
        0x4,
        0xD,
        0x355
    };

    param0->yesNoMenu = Menu_MakeYesNoChoice(param0->bgConfig, &v0, 1024 - (18 + 12) - 9, 11, param0->heapID);
}

static int Mailbox_ProcessYesNoInput(MailboxApp *param0)
{
    switch (Menu_ProcessInputAndHandleExit(param0->yesNoMenu, param0->heapID)) {
    case 0:
        return 1;
    case 0xfffffffe:
        return 0;
    }

    return -1;
}

static int Mailbox_RunScreenFade(MailboxApp *param0, int param1)
{
    switch (param0->subState) {
    case 0:
        StartScreenFade(FADE_BOTH_SCREENS, param1, param1, 0x0, 6, 1, param0->heapID);
        param0->subState++;
        break;
    case 1:
        if (!IsScreenFadeDone()) {
            break;
        }

        param0->subState = 0;
        return 1;
    }

    return 0;
}

// Restarts the field map and plays the PC boot-up animation, waiting for it to
// finish before returning 1.
static int Mailbox_LoadFieldMap(MailboxApp *param0)
{
    switch (param0->subState) {
    case 0:
        FieldSystem_StartFieldMap(param0->fieldSystem);
        param0->subState++;
        break;
    case 1:
        if (!FieldSystem_IsRunningFieldMap(param0->fieldSystem)) {
            break;
        }

        FieldSystem_LoadPCAnimation(param0->fieldSystem, 90);
        FieldSystem_PlayPCBootUpAnimation(param0->fieldSystem, 90);
        param0->subState++;
        break;
    case 2:
        if (!MapPropOneShotAnimationManager_IsAnimationLoopFinished(param0->fieldSystem->mapPropOneShotAnimMan, 90)) {
            break;
        }

        param0->subState = 0;
        return 1;
    }

    return 0;
}

static int Mailbox_ExitMenu(MailboxApp *param0, u8 param1)
{
    FieldSystem_UnloadAnimation(param0->fieldSystem, 90);

    if (param1 == 0) {
        Mailbox_DestroyMailList(param0);
    } else {
        Mailbox_DestroyActionMenu(param0);
    }

    Mailbox_UnloadMessageBox(param0);
    return 1;
}

static int Mailbox_LaunchPartyMenu(MailboxApp *param0, int mode)
{
    PartyMenu *partyMenu;

    switch (param0->subState) {
    case 0:
        partyMenu = Heap_Alloc(param0->heapID, sizeof(PartyMenu));
        MI_CpuClear8(partyMenu, sizeof(PartyMenu));

        partyMenu->party = SaveData_GetParty(FieldSystem_GetSaveData(param0->fieldSystem));
        partyMenu->bag = SaveData_GetBag(FieldSystem_GetSaveData(param0->fieldSystem));
        partyMenu->options = SaveData_GetOptions(FieldSystem_GetSaveData(param0->fieldSystem));
        partyMenu->mailbox = SaveData_GetMailbox(param0->fieldSystem->saveData);
        partyMenu->type = PARTY_MENU_TYPE_BASIC;
        partyMenu->mode = mode;
        partyMenu->usedItemID = param0->entries[param0->selectedMailSlot].item;

        if (mode == PARTY_MENU_MODE_GIVE_MAIL) {
            partyMenu->selectedMonSlot = param0->selectedPartySlot;
        }

        FieldSystem_StartChildProcess(param0->fieldSystem, &gPokemonPartyAppTemplate, partyMenu);
        param0->partyMenu = partyMenu;
        param0->subState++;
        break;
    case 1:
        if (FieldSystem_IsRunningApplication(param0->fieldSystem)) {
            break;
        }

        param0->subState = 0;
        return 1;
    }

    return 0;
}

// Opens the mail application. A valid entry is read; an invalid one (erased
// above) is written. Returns 2 if the player wrote a message, 1 if they did
// not, and 0 while the application is still running.
static int Mailbox_LaunchMailApp(MailboxApp *param0)
{
    int v0 = 0;

    switch (param0->subState) {
    case 0:
        if (param0->entries[param0->selectedMailSlot].isValid) {
            param0->mailAppArgs = FieldSystem_LaunchMailApp_Read(param0->fieldSystem, MAIL_CONTEXT_MAILBOX, param0->selectedMailSlot, param0->heapID);
        } else {
            param0->mailAppArgs = FieldSystem_LaunchMailApp_Write(param0->fieldSystem, MAIL_CONTEXT_MAILBOX, param0->selectedPartySlot, param0->entries[param0->selectedMailSlot].mailType, param0->heapID);
        }

        param0->subState++;
        break;
    case 1:
        if (FieldSystem_IsRunningApplication(param0->fieldSystem)) {
            break;
        }

        if (MailApp_WasMailWritten(param0->mailAppArgs)) {
            param0->entries[param0->selectedMailSlot].isValid = 1;
            MailApp_CopyWrittenMailToMailboxSlot(param0->mailAppArgs, MAIL_CONTEXT_MAILBOX, param0->entries[param0->selectedMailSlot].slot);

            v0 = 2;
        } else {
            v0 = 1;
        }

        MailAppArgs_Free(param0->mailAppArgs);
        param0->subState = 0;
        return v0;
    }

    return 0;
}

// Field task that runs the mailbox application and frees its environment once
// the application reports that it has closed.
static BOOL Mailbox_FieldTask(FieldTask *param0)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    MailboxFieldTaskEnv *v1 = FieldTask_GetEnv(param0);

    switch (v1->state) {
    case 0:
        Mailbox_Start(fieldSystem, &(v1->done));
        v1->state++;
        break;
    case 1:
        if (!v1->done) {
            return 0;
        }

        Heap_Free(v1);
        return 1;
    }

    return 0;
}

// Entry point used by script command 0x1B3: starts the field task that runs the
// mailbox application.
void Mailbox_StartFieldTask(FieldTask *param0)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    MailboxFieldTaskEnv *v1 = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(MailboxFieldTaskEnv));

    v1->done = 0;
    v1->state = 0;

    FieldTask_InitCall(fieldSystem->task, Mailbox_FieldTask, v1);
}
