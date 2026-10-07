#include "frontier_easy_chat.h"

#include <nitro.h>
#include <string.h>

#include "field/field_system.h"
#include "overlay005/fieldmap.h"

#include "bg_window.h"
#include "colored_arrow.h"
#include "easy_chat_args.h"
#include "easy_chat_sentence.h"
#include "field_message.h"
#include "field_task.h"
#include "heap.h"
#include "message.h"
#include "render_window.h"
#include "save_player.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "string_gf.h"
#include "string_template.h"
#include "system.h"
#include "text.h"
#include "field_system_apps.h"
#include "wifi_battle_tower_save.h"

#include "res/text/bank/easy_chat.h"

// Battle Frontier Easy Chat interview. An NPC asks the player to set the four
// phrases stored in the FrontierEasyChatMessages save block (before battle,
// upon winning, upon losing, and becoming No. 1). For each phrase the player
// may either pick one of the four slots to edit or decline ("Not saying").
// Editing a slot hands control to the Easy Chat application; when it returns,
// the edited sentence is written back to the save block and the player is
// asked whether to answer another question.
//
// The flow is driven by a FieldTask state machine (see FrontierEasyChat_Task).

// State of the interview task. The three windows are created lazily and reused
// across the loop: the message box shows the current prompt, the sentence list
// window shows the four phrase slots plus "Not saying", and the confirm window
// shows the yes/no prompt after an edit.
typedef struct FrontierEasyChatWork {
    FieldSystem *fieldSystem;
    String *templateString; // raw message text, may contain a word placeholder
    String *messageString; // formatted text actually drawn to the message box
    StringTemplate *stringTemplate;
    MessageLoader *messageLoader;
    ColoredArrow *coloredArrow;
    Window messageWindow;
    Window sentenceListWindow;
    Window confirmWindow;
    EasyChatSentence sentence; // sentence currently being edited
    EasyChatArgs *easyChatArgs;
    int state;
    int printerID; // handle returned by FieldMessage_Print
    int cursorPos; // highlighted option in the active menu
    int optionCount; // number of options in the active menu
    int menuChoice; // option returned by the active menu, or -1
    Window *activeWindow; // menu window the cursor is currently drawn in
} FrontierEasyChatWork;

static void FrontierEasyChat_Free(FrontierEasyChatWork *param0);
static void FrontierEasyChat_RemoveWindows(FrontierEasyChatWork *param0);
static BOOL FrontierEasyChat_Task(FieldTask *param0);
static void FrontierEasyChat_PrintMessage(FrontierEasyChatWork *param0, int param1, BOOL param2);
static BOOL FrontierEasyChat_IsMessagePrinted(FrontierEasyChatWork *param0);
static void FrontierEasyChat_EraseMessageWindow(FrontierEasyChatWork *param0);
static void FrontierEasyChat_CreateSentenceListWindow(FrontierEasyChatWork *param0);
static void FrontierEasyChat_EraseSentenceListWindow(FrontierEasyChatWork *param0);
static void FrontierEasyChat_CreateConfirmWindow(FrontierEasyChatWork *param0);
static void FrontierEasyChat_EraseConfirmWindow(FrontierEasyChatWork *param0);
static int FrontierEasyChat_HandleMenuInput(FrontierEasyChatWork *param0);

void FrontierEasyChat_Start(FieldTask *param0)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(param0);
    FrontierEasyChatWork *v1 = Heap_Alloc(HEAP_ID_FIELD3, sizeof(FrontierEasyChatWork));

    v1->fieldSystem = fieldSystem;
    v1->templateString = String_Init(400, HEAP_ID_FIELD3);
    v1->messageString = String_Init(400, HEAP_ID_FIELD3);
    v1->stringTemplate = StringTemplate_Default(HEAP_ID_FIELD3);
    v1->messageLoader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_UNK_0420, HEAP_ID_FIELD3);
    v1->coloredArrow = ColoredArrow_New(HEAP_ID_FIELD3);
    v1->easyChatArgs = EasyChatArgs_New(EASY_CHAT_TYPE_SENTENCE, EasyChat_Text_ChooseWordOrPhrase, v1->fieldSystem->saveData, HEAP_ID_FIELD3);

    EasyChatArgs_SetForceConfirm(v1->easyChatArgs);
    Window_Init(&(v1->messageWindow));
    Window_Init(&(v1->sentenceListWindow));
    Window_Init(&(v1->confirmWindow));

    v1->state = 0;

    FieldTask_InitCall(param0, FrontierEasyChat_Task, v1);
}

static void FrontierEasyChat_Free(FrontierEasyChatWork *param0)
{
    EasyChatArgs_Free(param0->easyChatArgs);
    ColoredArrow_Free(param0->coloredArrow);
    String_Free(param0->templateString);
    String_Free(param0->messageString);
    StringTemplate_Free(param0->stringTemplate);
    MessageLoader_Free(param0->messageLoader);
    FrontierEasyChat_RemoveWindows(param0);
    Heap_Free(param0);
}

static void FrontierEasyChat_RemoveWindows(FrontierEasyChatWork *param0)
{
    if (Window_IsInUse(&(param0->messageWindow))) {
        Window_Remove(&param0->messageWindow);
        Window_Init(&(param0->messageWindow));
    }

    if (Window_IsInUse(&(param0->sentenceListWindow))) {
        Window_Remove(&param0->sentenceListWindow);
        Window_Init(&(param0->sentenceListWindow));
    }

    if (Window_IsInUse(&(param0->confirmWindow))) {
        Window_Remove(&param0->confirmWindow);
        Window_Init(&(param0->confirmWindow));
    }
}

static BOOL FrontierEasyChat_Task(FieldTask *param0)
{
    FrontierEasyChatWork *v0 = FieldTask_GetEnv(param0);

    switch (v0->state) {
    case 0:
        // Print the interview's opening prompt.
        FrontierEasyChat_PrintMessage(v0, 0, 0);
        v0->state = 1;
        break;
    case 1:
        if (FrontierEasyChat_IsMessagePrinted(v0)) {
            FrontierEasyChat_CreateSentenceListWindow(v0);
            v0->state = 2;
        }
        break;
    case 2:
        // Let the player choose a phrase slot (0-3) or "Not saying" (4).
        v0->menuChoice = FrontierEasyChat_HandleMenuInput(v0);

        if (v0->menuChoice != -1) {
            FrontierEasyChat_EraseSentenceListWindow(v0);

            switch (v0->menuChoice) {
            case 0:
                EasyChatSentence_Copy(&(v0->sentence), FrontierEasyChatMessages_GetSentence(v0->fieldSystem->saveData, 0));
                v0->state = 3;
                break;
            case 1:
                EasyChatSentence_Copy(&(v0->sentence), FrontierEasyChatMessages_GetSentence(v0->fieldSystem->saveData, 1));
                v0->state = 3;
                break;
            case 2:
                EasyChatSentence_Copy(&(v0->sentence), FrontierEasyChatMessages_GetSentence(v0->fieldSystem->saveData, 2));
                v0->state = 3;
                break;
            case 3:
                EasyChatSentence_Copy(&(v0->sentence), FrontierEasyChatMessages_GetSentence(v0->fieldSystem->saveData, 3));
                v0->state = 3;
                break;
            case 4:
                // "Not saying": skip straight to the farewell message.
                v0->state = 11;
                break;
            }
        }
        break;
    case 3:
        // Ask the question that corresponds to the chosen slot.
        FrontierEasyChat_PrintMessage(v0, 1 + v0->menuChoice, 0);
        v0->state = 4;
        break;
    case 4:
        if (FrontierEasyChat_IsMessagePrinted(v0)) {
            FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
            v0->state = 5;
        }
        break;
    case 5:
        if (IsScreenFadeDone()) {
            // Hand the current sentence to the Easy Chat application.
            EasyChatArgs_SetSentence(v0->easyChatArgs, &(v0->sentence));
            EasyChatArgs_FlagAsUnmodified(v0->easyChatArgs);
            FrontierEasyChat_RemoveWindows(v0);
            FieldSystem_OpenEasyChat(v0->fieldSystem, v0->easyChatArgs);
            v0->state = 6;
        }
        break;
    case 6:
        if (FieldSystem_IsRunningApplication(v0->fieldSystem) == 0) {
            FieldSystem_StartFieldMap(v0->fieldSystem);
            v0->state = 7;
        }
        break;
    case 7:
        if (FieldSystem_IsRunningFieldMap(v0->fieldSystem)) {
            FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
            v0->state = 8;
        }
        break;
    case 8:
        if (IsScreenFadeDone()) {
            if (EasyChatArgs_IsUnmodified(v0->easyChatArgs)) {
                // The player backed out without changing anything.
                v0->state = 11;
            } else {
                // Store the edited sentence back into the save block.
                EasyChatArgs_CopySentenceTo(v0->easyChatArgs, &(v0->sentence));
                FrontierEasyChatMessages_SetSentence(v0->fieldSystem->saveData, v0->menuChoice, &(v0->sentence));
                FrontierEasyChat_PrintMessage(v0, 6, 0);
                v0->state = 9;
            }
        }
        break;
    case 9:
        if (FrontierEasyChat_IsMessagePrinted(v0)) {
            FrontierEasyChat_CreateConfirmWindow(v0);
            v0->state = 10;
        }
        break;
    case 10:
        // Ask whether the player wants to answer another question.
        v0->menuChoice = FrontierEasyChat_HandleMenuInput(v0);

        if (v0->menuChoice != -1) {
            switch (v0->menuChoice) {
            case 0:
                // Yes: return to the phrase slot list.
                FrontierEasyChat_EraseConfirmWindow(v0);
                FrontierEasyChat_CreateSentenceListWindow(v0);
                v0->state = 2;
                break;
            case 1:
            default: {
                // No: thank the player, naming the phrase they entered if any.
                u16 v1 = EasyChatSentence_GetWord(&v0->sentence, 0);
                FrontierEasyChat_EraseConfirmWindow(v0);

                if (v1 != WORD_NONE) {
                    StringTemplate_SetEasyChatWord(v0->stringTemplate, 0, v1);
                    FrontierEasyChat_PrintMessage(v0, 8, 1);
                } else {
                    FrontierEasyChat_PrintMessage(v0, 7, 0);
                }

                v0->state = 12;
            }
            }
        }
        break;
    case 11:
        // "Not saying" / no change: print the farewell message.
        FrontierEasyChat_PrintMessage(v0, 5, 0);
        v0->state = 12;
        break;
    case 12:
        if (FrontierEasyChat_IsMessagePrinted(v0)) {
            if (gSystem.pressedKeys & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_PLUS_KEY_MASK)) {
                FrontierEasyChat_EraseMessageWindow(v0);
                v0->state = 13;
            }
        }
        break;
    case 13:
        FrontierEasyChat_Free(v0);
        return 1;
    }

    return 0;
}

// Prints message bank entry param1 into the message box. When param2 is set the
// entry is run through the string template first, substituting the Easy Chat
// word set by the caller.
static void FrontierEasyChat_PrintMessage(FrontierEasyChatWork *param0, int param1, BOOL param2)
{
    Window *v0 = &(param0->messageWindow);

    if (param2) {
        MessageLoader_GetString(param0->messageLoader, param1, param0->templateString);
        StringTemplate_Format(param0->stringTemplate, param0->messageString, param0->templateString);
    } else {
        MessageLoader_GetString(param0->messageLoader, param1, param0->messageString);
    }

    if (Window_IsInUse(v0) == 0) {
        FieldMessage_AddWindow(param0->fieldSystem->bgConfig, v0, 3);
        FieldMessage_DrawWindow(v0, SaveData_GetOptions(param0->fieldSystem->saveData));
    } else {
        FieldMessage_ClearWindow(v0);
        Window_DrawMessageBoxWithScrollCursor(v0, 0, 1024 - (18 + 12), 10);
    }

    param0->printerID = FieldMessage_Print(v0, param0->messageString, SaveData_GetOptions(param0->fieldSystem->saveData), 1);
}

static BOOL FrontierEasyChat_IsMessagePrinted(FrontierEasyChatWork *param0)
{
    return FieldMessage_FinishedPrinting(param0->printerID);
}

static void FrontierEasyChat_EraseMessageWindow(FrontierEasyChatWork *param0)
{
    Window *v0 = &(param0->messageWindow);

    if (Window_IsInUse(v0)) {
        Window_EraseMessageBox(v0, 0);
    }
}

// Builds the list of the four phrase slots plus "Not saying" and arms the
// cursor for it.
static void FrontierEasyChat_CreateSentenceListWindow(FrontierEasyChatWork *param0)
{
    Window *v0 = &(param0->sentenceListWindow);

    if (Window_IsInUse(v0) == 0) {
        int v1;

        LoadStandardWindowGraphics(param0->fieldSystem->bgConfig, 3, 155, 11, 0, HEAP_ID_FIELD3);
        Window_Add(param0->fieldSystem->bgConfig, v0, 3, 1, 1, 13, 10, 13, 1);
        Window_FillTilemap(v0, 15);

        for (v1 = 0; v1 < 5; v1++) {
            MessageLoader_GetString(param0->messageLoader, 9 + v1, param0->messageString);
            Text_AddPrinterWithParams(v0, FONT_SYSTEM, param0->messageString, 12, v1 * 16, TEXT_SPEED_NO_TRANSFER, NULL);
        }

        ColoredArrow_Print(param0->coloredArrow, v0, 0, 0);
    }

    param0->activeWindow = v0;
    param0->cursorPos = 0;
    param0->optionCount = 5;

    Window_DrawStandardFrame(v0, 0, 155, 11);
}

static void FrontierEasyChat_EraseSentenceListWindow(FrontierEasyChatWork *param0)
{
    Window *v0 = &(param0->sentenceListWindow);
    Window_EraseStandardFrame(v0, 1);
}

// Builds the yes/no confirmation window and arms the cursor for it.
static void FrontierEasyChat_CreateConfirmWindow(FrontierEasyChatWork *param0)
{
    Window *v0 = &(param0->confirmWindow);

    if (Window_IsInUse(v0) == 0) {
        int v1;

        LoadStandardWindowGraphics(param0->fieldSystem->bgConfig, 3, 155, 11, 0, HEAP_ID_FIELD3);
        Window_Add(param0->fieldSystem->bgConfig, v0, 3, 25, 13, 6, 4, 13, 131);
        Window_FillTilemap(v0, 15);

        for (v1 = 0; v1 < 2; v1++) {
            MessageLoader_GetString(param0->messageLoader, v1 + 14, param0->messageString);
            Text_AddPrinterWithParams(v0, FONT_SYSTEM, param0->messageString, 12, v1 * 16, TEXT_SPEED_NO_TRANSFER, NULL);
        }

        ColoredArrow_Print(param0->coloredArrow, v0, 0, 0);
    }

    param0->activeWindow = v0;
    param0->cursorPos = 0;
    param0->optionCount = 2;

    Window_DrawStandardFrame(v0, 0, 155, 11);
}

static void FrontierEasyChat_EraseConfirmWindow(FrontierEasyChatWork *param0)
{
    Window *v0 = &(param0->confirmWindow);
    Window_EraseStandardFrame(v0, 1);
}

// Advances the cursor of the active menu in response to the d-pad and returns
// the confirmed option, or -1 while the player is still choosing. Up/down wrap
// around; B confirms the last option (used as "No" in the confirm window).
static int FrontierEasyChat_HandleMenuInput(FrontierEasyChatWork *param0)
{
    do {
        if (gSystem.pressedKeys & PAD_KEY_UP) {
            param0->cursorPos--;

            if (param0->cursorPos < 0) {
                if (param0->optionCount > 2) {
                    param0->cursorPos = param0->optionCount - 1;
                } else {
                    param0->cursorPos = 0;
                }
            }
            break;
        }

        if (gSystem.pressedKeys & PAD_KEY_DOWN) {
            param0->cursorPos++;

            if (param0->cursorPos >= param0->optionCount) {
                if (param0->optionCount > 2) {
                    param0->cursorPos = 0;
                } else {
                    param0->cursorPos = param0->optionCount - 1;
                }
            }
            break;
        }

        if (gSystem.pressedKeys & PAD_BUTTON_A) {
            Sound_PlayEffect(SE_CONFIRM_sseq_3);
            return param0->cursorPos;
        }

        if (gSystem.pressedKeys & PAD_BUTTON_B) {
            Sound_PlayEffect(SE_CONFIRM_sseq_3);
            return param0->optionCount - 1;
        }

        return -1;
    } while (0);

    {
        // Redraw the cursor at its new position.
        Window_FillRectWithColor(param0->activeWindow, 15, 0, 0, 12, param0->activeWindow->height * 8);
        ColoredArrow_Print(param0->coloredArrow, param0->activeWindow, 0, param0->cursorPos * 16);
        Window_LoadTiles(param0->activeWindow);
        Sound_PlayEffect(SE_CONFIRM_sseq_3);
    }

    return -1;
}
