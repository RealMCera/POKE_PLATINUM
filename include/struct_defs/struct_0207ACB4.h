#ifndef POKEPLATINUM_STRUCT_0207ACB4_H
#define POKEPLATINUM_STRUCT_0207ACB4_H

#include "struct_decls/battle_system.h"

// State for the link battle server sender SysTask. The battle system holds a
// pointer to `state` so the end-wait command can set it to 255 and stop the
// task.
typedef struct LinkBattleCommSender {
    BattleSystem *battleSys;
    u8 state;
} LinkBattleCommSender;

#endif // POKEPLATINUM_STRUCT_0207ACB4_H
