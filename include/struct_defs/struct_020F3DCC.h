#ifndef POKEPLATINUM_STRUCT_020F3DCC_H
#define POKEPLATINUM_STRUCT_020F3DCC_H

#include "struct_decls/struct_020F3DCC_decl.h"

// One entry in a Vs. Recorder menu. Entries are grouped into five-element
// tables; an entry with a non-NULL `children` pointer opens a submenu when
// selected.
typedef struct VsRecorderMenuEntry_t {
    BOOL enabled; // Whether the entry is present in the menu.
    BOOL unk_04;
    int labelMessageID; // Message ID of the entry's label.
    int spriteAnimID; // Animation played by the entry's sprite.
    int action; // Action performed when the entry is selected.
    int nextState; // Screen state entered after the action runs.
    int dataIndex; // Index into the recording/profile data shown by the entry.
    int unk_1C;
    const VsRecorderMenuEntry *children; // Submenu opened by this entry, or NULL.
} VsRecorderMenuEntry;

#endif // POKEPLATINUM_STRUCT_020F3DCC_H
