#include "easy_chat_args.h"

#include <nitro.h>

#include "applications/easy_chat/defs.h"

#include "easy_chat_sentence.h"
#include "easy_chat_words.h"
#include "game_options.h"
#include "heap.h"
#include "pokedex.h"
#include "save_player.h"
#include "savedata.h"
#include "system_flags.h"
#include "vars_flags.h"

// Allocates and initializes the arguments for an Easy Chat session. `type`
// selects the kind of content being edited (one word, two words, or a
// sentence); `instructionBankEntry` is the message bank entry for the
// instruction displayed at the top of the application. The remaining fields are
// populated from `saveData`.
EasyChatArgs *EasyChatArgs_New(u32 type, u32 instructionBankEntry, SaveData *saveData, enum HeapID heapID)
{
    EasyChatArgs *args = Heap_Alloc(heapID, sizeof(EasyChatArgs));

    args->type = type;
    args->instructionBankEntry = instructionBankEntry;
    args->pokedex = SaveData_GetPokedex(saveData);
    args->unlockedWords = SaveData_GetUnlockedEasyChatWords(saveData);
    args->isGameCompleted = SystemFlag_CheckGameCompleted(SaveData_GetVarsFlags(saveData));
    args->forceConfirm = FALSE;
    args->isUnmodified = TRUE;
    args->wasUpdated = FALSE;
    args->frame = Options_Frame(SaveData_GetOptions(saveData));

    if (type == EASY_CHAT_TYPE_SENTENCE) {
        EasyChatSentence_InitWithType(&args->sentence, EASY_CHAT_SENTENCE_TYPE_GENERAL);
    } else {
        for (int i = 0; i < MAX_EASY_CHAT_WORDS; i++) {
            args->words[i] = WORD_NONE;
        }
    }

    return args;
}

void EasyChatArgs_Free(EasyChatArgs *args)
{
    Heap_Free(args);
}

void EasyChatArgs_SetOneWord(EasyChatArgs *args, u16 word)
{
    args->words[0] = word;
}

void EasyChatArgs_SetTwoWords(EasyChatArgs *args, u16 word1, u16 word2)
{
    args->words[0] = word1;
    args->words[1] = word2;
}

void EasyChatArgs_SetSentence(EasyChatArgs *args, const EasyChatSentence *sentence)
{
    args->sentence = *sentence;
}

// Marks the content as untouched, clearing any previously recorded update.
void EasyChatArgs_FlagAsUnmodified(EasyChatArgs *args)
{
    args->isUnmodified = TRUE;
    args->wasUpdated = FALSE;
}

// Requests that the application always show the confirmation prompt when the
// player tries to leave, even if the content was not changed.
void EasyChatArgs_SetForceConfirm(EasyChatArgs *args)
{
    args->forceConfirm = TRUE;
}

BOOL EasyChatArgs_IsUnmodified(const EasyChatArgs *args)
{
    return args->isUnmodified;
}

BOOL EasyChatArgs_WasUpdated(const EasyChatArgs *args)
{
    return args->wasUpdated;
}

u16 EasyChatArgs_GetOneWord(const EasyChatArgs *args)
{
    return args->words[0];
}

void EasyChatArgs_CopyTwoWordsTo(const EasyChatArgs *args, u16 *dest)
{
    dest[0] = args->words[0];
    dest[1] = args->words[1];
}

void EasyChatArgs_CopySentenceTo(const EasyChatArgs *args, EasyChatSentence *dest)
{
    EasyChatSentence_Copy(dest, &args->sentence);
}

u32 EasyChatArgs_GetType(const EasyChatArgs *args)
{
    return args->type;
}

u32 EasyChatArgs_GetInstructionBankEntry(const EasyChatArgs *args)
{
    return args->instructionBankEntry;
}

int EasyChatArgs_GetFrame(const EasyChatArgs *args)
{
    return args->frame;
}

const Pokedex *EasyChatArgs_GetPokedex(const EasyChatArgs *args)
{
    return args->pokedex;
}

const UnlockedEasyChatWords *EasyChatArgs_GetUnlockedWords(const EasyChatArgs *args)
{
    return args->unlockedWords;
}

BOOL EasyChatArgs_IsGameCompleted(const EasyChatArgs *args)
{
    return args->isGameCompleted;
}

// Returns whether the application should always show the confirmation prompt on
// exit, regardless of whether the content was changed.
BOOL EasyChatArgs_ForceConfirm(const EasyChatArgs *args)
{
    return args->forceConfirm;
}

// Copies the stored content into the caller's buffers, according to the args'
// type. Only the buffer matching the type is written.
void EasyChatArgs_GetContent(const EasyChatArgs *args, u16 *outWords, EasyChatSentence *outSentence)
{
    switch (args->type) {
    case EASY_CHAT_TYPE_ONE_WORD:
        outWords[0] = args->words[0];
        break;
    case EASY_CHAT_TYPE_TWO_WORDS:
        outWords[0] = args->words[0];
        outWords[1] = args->words[1];
        break;
    case EASY_CHAT_TYPE_SENTENCE:
        *outSentence = args->sentence;
        break;
    }
}

// Returns TRUE if the given content matches the stored content. Only the buffer
// matching the args' type is compared.
BOOL EasyChatArgs_Compare(const EasyChatArgs *args, const u16 *words, const EasyChatSentence *sentence)
{
    switch (args->type) {
    case EASY_CHAT_TYPE_ONE_WORD:
        return words[0] == args->words[0];
    case EASY_CHAT_TYPE_TWO_WORDS:
        return words[0] == args->words[0] && words[1] == args->words[1];
    case EASY_CHAT_TYPE_SENTENCE:
    default:
        return EasyChatSentence_Compare(&args->sentence, sentence);
    }
}

// Records the player's confirmed content. `wasUpdated` is set if it differs
// from the previous content, and the args are no longer considered unmodified.
void EasyChatArgs_UpdateContent(EasyChatArgs *args, const u16 *words, const EasyChatSentence *sentence)
{
    args->wasUpdated = !EasyChatArgs_Compare(args, words, sentence);
    args->isUnmodified = FALSE;

    for (int i = 0; i < MAX_EASY_CHAT_WORDS; i++) {
        args->words[i] = words[i];
    }

    args->sentence = *sentence;
}
