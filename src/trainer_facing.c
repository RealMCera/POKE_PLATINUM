#include "trainer_facing.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"

#include "struct_decls/map_object.h"

#include "map_header_data.h"
#include "map_object.h"
#include "map_object_animation.h"

// Per-trainer-type idle "look around" behaviour. Trainer types that face or
// rotate (TRAINER_TYPE_FACE_SIDES, TRAINER_TYPE_FACE_COUNTERCLOCKWISE and
// TRAINER_TYPE_FACE_CLOCKWISE) walk for a number of steps and then pause to
// turn through a set of directions before resuming. The three callback tables
// at the bottom of this file are indexed by trainer type; every other trainer
// type uses the no-op entries.

// State for the "face sides" trainer behaviour (TRAINER_TYPE_FACE_SIDES).
// After every stepTarget movement steps the trainer pauses and turns to face
// the two directions perpendicular to the axis it was originally facing.
typedef struct {
    u8 moveState; // Movement state machine: 0 wait for a step, 1 count steps, 2 wait for movement to end, 3 done
    u8 faceState; // Facing state machine: 0 record facing, 1 start turn, 2 wait for turn, 3 hold and repeat
    s8 stepCount; // Movement steps taken in the current cycle
    s8 stepTarget; // Movement steps to take before facing (object event data[1])
    s8 originalDir; // Facing direction to restore once the facing cycle ends
    s8 axis; // Axis of the original facing: 0 = north/south, 1 = west/east
    s8 sideIndex; // Which of the two perpendicular directions is currently being faced
    s8 faceCount; // Facing turns completed in the current cycle
    s8 frameCount; // Frames spent holding the current facing direction
} TrainerFacingSidesData;

// State for the rotating trainer behaviours (TRAINER_TYPE_FACE_COUNTERCLOCKWISE
// and TRAINER_TYPE_FACE_CLOCKWISE). Layout is identical to
// TrainerFacingSidesData; only the meaning of axis/sideIndex differs.
typedef struct {
    u8 moveState; // Movement state machine: 0 wait for a step, 1 count steps, 2 wait for movement to end, 3 done
    u8 faceState; // Facing state machine: 0 record facing, 1 start turn, 2 wait for turn, 3 hold and repeat
    s8 stepCount; // Movement steps taken in the current cycle
    s8 stepTarget; // Movement steps to take before facing (object event data[1])
    s8 originalDir; // Facing direction to restore once the rotation ends
    s8 rotationDir; // Rotation direction: 0 = counterclockwise, 1 = clockwise
    s8 rotationIndex; // Position within the rotation sequence
    s8 faceCount; // Facing turns completed in the current cycle
    s8 frameCount; // Frames spent holding the current facing direction
} TrainerFacingRotateData;

static void TrainerFacing_CallInit(MapObject *mapObj);
static int TrainerFacing_CallUpdate(MapObject *mapObj);
static int TrainerFacing_CallFace(MapObject *mapObj);
static int TrainerFacing_HasMoved(MapObject *mapObj);
static int TrainerFacing_HasStopped(MapObject *mapObj);
static void TrainerFacing_NoOpInit(MapObject *mapObj);
static int TrainerFacing_NoOpUpdate(MapObject *mapObj);
static int TrainerFacing_NoOpFace(MapObject *mapObj);
static void TrainerFacingSides_Init(MapObject *mapObj);
static int TrainerFacingSides_Update(MapObject *mapObj);
static int TrainerFacingSides_Face(MapObject *mapObj);
static void TrainerFacingRotate_Init(MapObject *mapObj);
static int TrainerFacingRotate_Update(MapObject *mapObj);
static int TrainerFacingRotate_Face(MapObject *mapObj);

void (*const sTrainerFacingInitFuncs[])(MapObject *);
int (*const sTrainerFacingUpdateFuncs[])(MapObject *);
int (*const sTrainerFacingFaceFuncs[])(MapObject *);

// Called when a map object's movement is initialised. Dispatches to the
// per-trainer-type init callback.
void TrainerFacing_Init(MapObject *mapObj)
{
    TrainerFacing_CallInit(mapObj);
}

// Called every frame while the object is not performing a movement action.
// Returns TRUE while the post-movement facing animation is running, which
// suppresses the object's normal movement update.
int TrainerFacing_Update(MapObject *mapObj)
{
    if (TrainerFacing_CallUpdate(mapObj) == 0) {
        return 0;
    }

    if (TrainerFacing_CallFace(mapObj) == 0) {
        return 0;
    }

    return 1;
}

// Dispatch helpers: look up the callback for the object's trainer type.
static void TrainerFacing_CallInit(MapObject *mapObj)
{
    int trainerType = MapObject_GetTrainerType(mapObj);

    sTrainerFacingInitFuncs[trainerType](mapObj);
}

static int TrainerFacing_CallUpdate(MapObject *mapObj)
{
    int trainerType = MapObject_GetTrainerType(mapObj);
    return sTrainerFacingUpdateFuncs[trainerType](mapObj);
}

static int TrainerFacing_CallFace(MapObject *mapObj)
{
    int trainerType = MapObject_GetTrainerType(mapObj);
    return sTrainerFacingFaceFuncs[trainerType](mapObj);
}

// TRUE if the object's tile position changed since the previous frame.
static int TrainerFacing_HasMoved(MapObject *mapObj)
{
    int coord = MapObject_GetX(mapObj);
    int prevCoord = MapObject_GetXPrev(mapObj);

    if (coord != prevCoord) {
        return 1;
    }

    coord = MapObject_GetZ(mapObj);
    prevCoord = MapObject_GetZPrev(mapObj);

    if (coord != prevCoord) {
        return 1;
    }

    return 0;
}

// TRUE if the object's tile position is unchanged since the previous frame.
static int TrainerFacing_HasStopped(MapObject *mapObj)
{
    int coord = MapObject_GetX(mapObj);
    int prevCoord = MapObject_GetXPrev(mapObj);

    if (coord != prevCoord) {
        return 0;
    }

    coord = MapObject_GetZ(mapObj);
    prevCoord = MapObject_GetZPrev(mapObj);

    if (coord != prevCoord) {
        return 0;
    }

    return 1;
}

// No-op callbacks used by trainer types without facing behaviour.
static void TrainerFacing_NoOpInit(MapObject *mapObj)
{
    return;
}

static int TrainerFacing_NoOpUpdate(MapObject *mapObj)
{
    return 0;
}

static int TrainerFacing_NoOpFace(MapObject *mapObj)
{
    return 0;
}

// Allocates the facing state and reads the step count from object event data[1].
static void TrainerFacingSides_Init(MapObject *mapObj)
{
    TrainerFacingSidesData *state = MapObject_InitUnkE8(mapObj, (sizeof(TrainerFacingSidesData)));
    state->stepTarget = MapObject_GetDataAt(mapObj, 1);
}

// Counts movement steps. Once stepTarget steps have been taken and the object
// has stopped, returns TRUE so the facing animation can begin.
static int TrainerFacingSides_Update(MapObject *mapObj)
{
    TrainerFacingSidesData *state = MapObject_GetUnkE8(mapObj);

    switch (state->moveState) {
    case 0:
        if (TrainerFacing_HasMoved(mapObj) == 1) {
            state->moveState++;
        }
        break;
    case 1:
        if (TrainerFacing_HasStopped(mapObj) == 0) {
            break;
        }

        state->stepCount++;

        if (state->stepCount < state->stepTarget) {
            state->moveState = 0;
            break;
        }

        state->moveState++;
    case 2:
        if (MapObject_IsMoving(mapObj) == 1) {
            break;
        }

        state->moveState++;
        state->stepCount = 0;
        state->faceState = 0;
    case 3:
        return 1;
    }

    return 0;
}

// Turns the trainer to face the two directions perpendicular to its original
// axis (north/south -> west/east and vice versa), holding each for 8 frames,
// then restores the original facing and restarts the movement cycle.
static int TrainerFacingSides_Face(MapObject *mapObj)
{
    TrainerFacingSidesData *state = MapObject_GetUnkE8(mapObj);

    switch (state->faceState) {
    case 0: {
        int axisByDir[4] = { 0, 0, 1, 1 };
        int facingDir = MapObject_GetFacingDir(mapObj);

        state->originalDir = facingDir;
        state->axis = axisByDir[facingDir];
        state->faceState++;
    }
    case 1: {
        int dirsByAxis[2][2] = {
            { 2, 3 },
            { 0, 1 },
        };
        int targetDir = dirsByAxis[state->axis][state->sideIndex];
        int movementAction = MovementAction_TurnActionTowardsDir(targetDir, MOVEMENT_ACTION_FACE_NORTH);

        LocalMapObj_SetMovementAction(mapObj, movementAction);
        state->faceState++;
    }
    case 2: {
        if (LocalMapObj_RunMovementAction(mapObj) == 0) {
            return 1;
        }

        state->faceState++;
    }
    case 3: {
        state->frameCount++;

        if (state->frameCount < 8) {
            return 1;
        }

        state->frameCount = 0;
        state->faceCount++;

        if (state->faceCount < 4) {
            state->sideIndex = (state->sideIndex + 1) & 0x1;
            state->faceState = 1;
            return 1;
        }

        MapObject_TryFace(mapObj, state->originalDir);

        state->faceState++;
        state->faceCount = 0;
        state->moveState = 0;
    }
    }

    return 0;
}

// Allocates the rotation state, reads the step count from object event data[1]
// and picks the rotation direction from the trainer type.
static void TrainerFacingRotate_Init(MapObject *mapObj)
{
    int trainerType;
    TrainerFacingRotateData *state = MapObject_InitUnkE8(mapObj, (sizeof(TrainerFacingRotateData)));
    state->stepTarget = MapObject_GetDataAt(mapObj, 1);

    trainerType = MapObject_GetTrainerType(mapObj);

    if (trainerType == 0x5) {
        trainerType = 0;
    } else {
        trainerType = 1;
    }

    state->rotationDir = trainerType;
}

// Counts movement steps. Once stepTarget steps have been taken and the object
// has stopped, returns TRUE so the rotation animation can begin.
static int TrainerFacingRotate_Update(MapObject *mapObj)
{
    TrainerFacingRotateData *state = MapObject_GetUnkE8(mapObj);

    switch (state->moveState) {
    case 0:
        if (TrainerFacing_HasMoved(mapObj) == 1) {
            state->moveState++;
        }

        break;
    case 1:
        if (TrainerFacing_HasStopped(mapObj) == 0) {
            break;
        }

        state->stepCount++;

        if (state->stepCount < state->stepTarget) {
            state->moveState = 0;
            break;
        }

        state->moveState++;
    case 2:
        if (MapObject_IsMoving(mapObj) == 1) {
            break;
        }

        state->moveState++;
        state->stepCount = 0;
        state->faceState = 0;
    case 3:
        return 1;
    }

    return 0;
}

// Rotates the trainer through all four directions (counterclockwise for
// TRAINER_TYPE_FACE_COUNTERCLOCKWISE, clockwise otherwise), holding each for
// 8 frames, then restores the original facing and restarts the movement cycle.
static int TrainerFacingRotate_Face(MapObject *mapObj)
{
    TrainerFacingRotateData *state;
    int rotationOrder[2][4] = {
        { 0, 2, 1, 3 },
        { 0, 3, 1, 2 },
    };

    state = MapObject_GetUnkE8(mapObj);

    switch (state->faceState) {
    case 0: {
        int index, facingDir = MapObject_GetFacingDir(mapObj);

        for (index = 0; (index < 4 && facingDir != rotationOrder[state->rotationDir][index]); index++) {
            (void)0;
        }

        GF_ASSERT(index < 4);

        state->originalDir = facingDir;
        state->rotationIndex = (index + 1) % 4;
        state->faceState++;
    }
    case 1: {
        int targetDir = rotationOrder[state->rotationDir][state->rotationIndex];
        int movementAction = MovementAction_TurnActionTowardsDir(targetDir, MOVEMENT_ACTION_FACE_NORTH);

        LocalMapObj_SetMovementAction(mapObj, movementAction);
        state->faceState++;
    }
    case 2: {
        if (LocalMapObj_RunMovementAction(mapObj) == 0) {
            return 1;
        }

        state->faceState++;
    }
    case 3: {
        state->frameCount++;

        if (state->frameCount < 8) {
            return 1;
        }

        state->frameCount = 0;
        state->faceCount++;

        if (state->faceCount < 4) {
            state->rotationIndex = (state->rotationIndex + 1) % 4;
            state->faceState = 1;
            return 1;
        }

        MapObject_TryFace(mapObj, state->originalDir);
        state->faceState++;
        state->faceCount = 0;
        state->moveState = 0;
    }
    }

    return 0;
}

// Callbacks indexed by trainer type. Only FACE_SIDES (4) and the two rotating
// types (5, 6) have non-trivial entries.
static void (*const sTrainerFacingInitFuncs[])(MapObject *) = {
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacingSides_Init,
    TrainerFacingRotate_Init,
    TrainerFacingRotate_Init,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit,
    TrainerFacing_NoOpInit
};

static int (*const sTrainerFacingUpdateFuncs[])(MapObject *) = {
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacingSides_Update,
    TrainerFacingRotate_Update,
    TrainerFacingRotate_Update,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate,
    TrainerFacing_NoOpUpdate
};

static int (*const sTrainerFacingFaceFuncs[])(MapObject *) = {
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacingSides_Face,
    TrainerFacingRotate_Face,
    TrainerFacingRotate_Face,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace,
    TrainerFacing_NoOpFace
};
