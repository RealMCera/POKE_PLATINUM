#ifndef POKEPLATINUM_STRUCT_OV104_0223C634_H
#define POKEPLATINUM_STRUCT_OV104_0223C634_H

#include "overlay063/struct_ov63_0222BEC0_decl.h"
#include "overlay063/struct_ov63_0222CE44.h"
#include "overlay104/struct_ov104_0223D570.h"
#include "overlay104/struct_ov104_0223D8F0.h"

#include "sys_task_manager.h"

// A Frontier map object: a 3D object plus its managed sprite, display/spawn
// parameters, movement state, and the task driving its movement.
typedef struct {
    UnkStruct_ov63_0222BEC0 *object;
    UnkStruct_ov63_0222CE44 *sprite;
    FrontierObjectParams params;
    FrontierObjectMovement movement;
    SysTask *movementTask;
} FrontierObject;

#endif // POKEPLATINUM_STRUCT_OV104_0223C634_H
