#ifndef POKEPLATINUM_STRUCT_02030A80_H
#define POKEPLATINUM_STRUCT_02030A80_H

#include "struct_defs/struct_0202F298_sub1.h"

#include "easy_chat_sentence.h"

// A player's profile as shown on the trainer card: used both for the header of
// a Vs. Recorder battle recording and for the profiles exchanged in the Union
// Room. The struct is transmitted verbatim between players, so every field is
// stored in a fixed, platform-independent layout.
typedef struct PlayerProfile {
    // Trainer name as charcode (7 characters plus a terminator).
    u16 name[8];
    // Trainer ID number.
    u32 id;
    u8 gender;
    // Birthday month (1-12), taken from the DS owner info.
    u8 month;
    // Trainer appearance (trainer class sprite index).
    u8 appearance;
    u8 country;
    u8 region;
    u8 version;
    u8 language;
    // Favorite Pokémon shown on the profile.
    u8 isEgg : 1;
    u8 form : 7;
    u16 species;
    // Nonzero when the intro message is stored as a raw charcode string in
    // `introMessage` rather than as an EasyChat sentence in `introSentence`.
    u8 introMessageIsString;
    u8 unk_1F;
    // The intro message. Both members overlay the same 80-byte buffer: the
    // EasyChat sentence occupies only its first bytes, while the raw string
    // form uses the whole buffer.
    union {
        EasyChatSentence introSentence;
        u16 introMessage[40];
    };
    u8 unk_70[12];
    // Checksum over everything before it, used to validate a received profile.
    BattleRecordingChecksum checksum;
} PlayerProfile;

#endif // POKEPLATINUM_STRUCT_02030A80_H
