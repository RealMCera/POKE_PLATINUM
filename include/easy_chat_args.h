#ifndef POKEPLATINUM_EASY_CHAT_ARGS_H
#define POKEPLATINUM_EASY_CHAT_ARGS_H

#include "applications/easy_chat/defs.h"

#include "easy_chat_sentence.h"
#include "easy_chat_words.h"
#include "pokedex.h"
#include "savedata.h"

// Arguments handed to the Easy Chat application when it is launched. They carry
// the content being edited (a set of words or a sentence, selected by `type`),
// the save data the application needs to build its word lists, and the flags
// used to report the result back to the caller.
typedef struct EasyChatArgs {
    u8 type; // one of the EASY_CHAT_TYPE_* values
    u8 instructionBankEntry; // message bank entry for the instruction shown at the top
    u8 isUnmodified; // TRUE while the content has not been confirmed by the player
    u8 wasUpdated; // TRUE if the confirmed content differs from the original
    u8 isGameCompleted; // whether the save has completed the game (unlocks extra words)
    u8 forceConfirm; // if TRUE, always show the confirmation prompt on exit
    int frame; // text frame style taken from the save's options
    const Pokedex *pokedex;
    const UnlockedEasyChatWords *unlockedWords;
    EasyChatSentence sentence; // content when type is EASY_CHAT_TYPE_SENTENCE
    u16 words[MAX_EASY_CHAT_WORDS]; // content when type is EASY_CHAT_TYPE_ONE_WORD or TWO_WORDS
    u8 padding[4];
} EasyChatArgs;

EasyChatArgs *EasyChatArgs_New(u32 type, u32 instructionBankEntry, SaveData *saveData, enum HeapID heapID);
void EasyChatArgs_Free(EasyChatArgs *args);
void EasyChatArgs_SetOneWord(EasyChatArgs *args, u16 word);
void EasyChatArgs_SetTwoWords(EasyChatArgs *args, u16 word1, u16 word2);
void EasyChatArgs_SetSentence(EasyChatArgs *args, const EasyChatSentence *sentence);
void EasyChatArgs_FlagAsUnmodified(EasyChatArgs *args);
void EasyChatArgs_SetForceConfirm(EasyChatArgs *args);
BOOL EasyChatArgs_IsUnmodified(const EasyChatArgs *args);
BOOL EasyChatArgs_WasUpdated(const EasyChatArgs *args);
u16 EasyChatArgs_GetOneWord(const EasyChatArgs *args);
void EasyChatArgs_CopyTwoWordsTo(const EasyChatArgs *args, u16 *dest);
void EasyChatArgs_CopySentenceTo(const EasyChatArgs *args, EasyChatSentence *dest);
u32 EasyChatArgs_GetType(const EasyChatArgs *args);
u32 EasyChatArgs_GetInstructionBankEntry(const EasyChatArgs *args);
int EasyChatArgs_GetFrame(const EasyChatArgs *args);
const Pokedex *EasyChatArgs_GetPokedex(const EasyChatArgs *args);
const UnlockedEasyChatWords *EasyChatArgs_GetUnlockedWords(const EasyChatArgs *args);
BOOL EasyChatArgs_IsGameCompleted(const EasyChatArgs *args);
BOOL EasyChatArgs_ForceConfirm(const EasyChatArgs *args);
void EasyChatArgs_GetContent(const EasyChatArgs *args, u16 *outWords, EasyChatSentence *outSentence);
BOOL EasyChatArgs_Compare(const EasyChatArgs *args, const u16 *words, const EasyChatSentence *sentence);
void EasyChatArgs_UpdateContent(EasyChatArgs *args, const u16 *words, const EasyChatSentence *sentence);

#endif // POKEPLATINUM_EASY_CHAT_ARGS_H
