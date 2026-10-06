#ifndef POKEPLATINUM_STRUCT_0205C924_H
#define POKEPLATINUM_STRUCT_0205C924_H

#include "easy_chat_sentence.h"
#include "string_gf.h"

// One entry in the Union Room chat log: a trainer's name, their easy chat
// sentence rendered to a string, and an optional Pal Pad message.
typedef struct UnionRoomChatLogEntry {
    String *trainerName;
    String *sentenceString;
    String *palPadString;
    u32 trainerId;
    int gender;
    EasyChatSentence sentence;
} UnionRoomChatLogEntry;

#endif // POKEPLATINUM_STRUCT_0205C924_H
