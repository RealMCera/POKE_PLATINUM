#include "easy_chat_task.h"

#include <nitro.h>
#include <string.h>

#include "field/field_system.h"
#include "overlay005/fieldmap.h"

#include "easy_chat_args.h"
#include "easy_chat_sentence.h"
#include "field_task.h"
#include "heap.h"
#include "savedata_misc.h"
#include "screen_fade.h"
#include "string_template.h"
#include "field_system_apps.h"

#include "res/text/bank/easy_chat.h"

// State for the Easy Chat field task. The task opens the Easy Chat application
// so the player can edit the Union Room intro message, then writes back whether
// the message was changed.
typedef struct EasyChatTaskState {
    FieldSystem *fieldSystem;
    StringTemplate *stringTemplate;
    EasyChatSentence sentence;
    EasyChatArgs *easyChatArgs;
    MiscSaveBlock *miscSaveBlock;
    int state;
    int unk_1C; // Unused.
    u16 *result;
} EasyChatTaskState;

static void EasyChatTask_Free(EasyChatTaskState *state);
static BOOL EasyChatTask_Update(FieldTask *task);

// Starts the Easy Chat field task. `result` receives 1 if the player changed the
// intro message and 0 if it was left unmodified.
void EasyChatTask_Start(FieldTask *task, u16 *result)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    EasyChatTaskState *state = Heap_Alloc(HEAP_ID_FIELD3, sizeof(EasyChatTaskState));

    state->fieldSystem = fieldSystem;
    state->stringTemplate = StringTemplate_Default(HEAP_ID_FIELD3);
    state->easyChatArgs = EasyChatArgs_New(EASY_CHAT_TYPE_SENTENCE, EasyChat_Text_ChooseWordOrPhrase, state->fieldSystem->saveData, HEAP_ID_FIELD3);
    state->miscSaveBlock = SaveData_MiscSaveBlock(fieldSystem->saveData);
    state->result = result;

    EasyChatSentence_InitWithType(&state->sentence, EASY_CHAT_SENTENCE_TYPE_UNION_ROOM);
    MiscSaveBlock_IntroMsg(state->miscSaveBlock, &state->sentence);
    sub_02097520(state->easyChatArgs);

    state->state = 0;
    FieldTask_InitCall(task, EasyChatTask_Update, state);

    return;
}

// Releases the resources owned by the task.
static void EasyChatTask_Free(EasyChatTaskState *state)
{
    EasyChatArgs_Free(state->easyChatArgs);
    StringTemplate_Free(state->stringTemplate);
    Heap_Free(state);
}

// Field task state machine: open Easy Chat, wait for it to close, return to the
// field map, then commit or discard the edited intro message.
static BOOL EasyChatTask_Update(FieldTask *task)
{
    EasyChatTaskState *state = FieldTask_GetEnv(task);

    switch (state->state) {
    case 0:
        // Hand the current intro message to the Easy Chat application.
        EasyChatArgs_SetSentence(state->easyChatArgs, &(state->sentence));
        EasyChatArgs_FlagAsUnmodified(state->easyChatArgs);
        FieldSystem_OpenEasyChat(state->fieldSystem, state->easyChatArgs);
        state->state = 1;
        break;
    case 1:
        // Wait for the Easy Chat application to exit, then reload the field map.
        if (FieldSystem_IsRunningApplication(state->fieldSystem) == 0) {
            FieldSystem_StartFieldMap(state->fieldSystem);
            state->state = 2;
        }
        break;
    case 2:
        // Wait for the field map to come up, then fade the screen back in.
        if (FieldSystem_IsRunningFieldMap(state->fieldSystem)) {
            FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_IN);
            state->state = 3;
        }
        break;
    case 3:
        if (IsScreenFadeDone()) {
            if (EasyChatArgs_IsUnmodified(state->easyChatArgs)) {
                // The player left the message alone; report no change.
                *state->result = 0;
                state->state = 4;
            } else {
                // The player edited the message; save it and report the change.
                *state->result = 1;
                EasyChatArgs_CopySentenceTo(state->easyChatArgs, &(state->sentence));

                MiscSaveBlock_SetIntroMsg(state->miscSaveBlock, &state->sentence);

                state->state = 4;
            }
        }
        break;
    case 4:
        EasyChatTask_Free(state);
        return 1;
    }

    return 0;
}
