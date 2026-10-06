#ifndef POKEPLATINUM_STRUCT_0207AD40_H
#define POKEPLATINUM_STRUCT_0207AD40_H

#include "struct_decls/battle_system.h"

// State for the link battle client receiver SysTask. The battle system holds a
// pointer to `state` so the end-wait command can set it to 255 and stop the
// task.
typedef struct LinkBattleCommReceiver {
    BattleSystem *battleSys;
    u8 state;
} LinkBattleCommReceiver;

#endif // POKEPLATINUM_STRUCT_0207AD40_H
