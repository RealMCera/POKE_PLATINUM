#include "spear_pillar_task.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_020985E4.h"
#include "struct_defs/struct_0209862C.h"

#include "field/field_system.h"
#include "overlay005/fieldmap.h"
#include "overlay100/ov100_021D0D80.h"

#include "field_system.h"
#include "field_task.h"
#include "heap.h"
#include "save_player.h"
#include "savedata.h"
#include "screen_fade.h"
#include "trainer_info.h"

FS_EXTERN_OVERLAY(overlay100);

// Entry point for the Spear Pillar cutscene field task, invoked by script
// command ScrCmd_2FB. Collects the player's options and trainer info, then
// hands control to SpearPillarTask_Main through the field task system.
void SpearPillarTask_Start(FieldTask *task, SaveData *saveData)
{
    SpearPillarTaskEnv *env = Heap_Alloc(HEAP_ID_FIELD2, sizeof(SpearPillarTaskEnv));
    SpearPillarTaskArgs *args = Heap_Alloc(HEAP_ID_FIELD2, sizeof(SpearPillarTaskArgs));

    args->options = SaveData_GetOptions(saveData);
    args->trainerInfo = SaveData_GetTrainerInfo(saveData);
    args->gender = TrainerInfo_Gender(args->trainerInfo);

    env->state = 0;
    env->args = args;

    FieldTask_InitCall(task, SpearPillarTask_Main, env);
}

// Field task state machine that fades the field out, runs the Spear Pillar
// cutscene in overlay100, then restores the field map.
BOOL SpearPillarTask_Main(FieldTask *task)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(task);
    SpearPillarTaskEnv *env = FieldTask_GetEnv(task);

    switch (env->state) {
    case 0:
        // Fade the field to black before launching the cutscene.
        FieldMap_FadeScreen(FADE_TYPE_BRIGHTNESS_OUT);
        env->state++;
    case 1:
        if (IsScreenFadeDone() == FALSE) {
            break;
        }

        {
            // Launch the Spear Pillar cutscene overlay, passing the collected
            // options and trainer info as its application arguments.
            static const ApplicationManagerTemplate v2 = {
                ov100_021D0D80,
                ov100_021D0EA8,
                ov100_021D0F44,
                FS_OVERLAY_ID(overlay100)
            };

            FieldSystem_StartChildProcess(fieldSystem, &v2, env->args);
            env->state++;
        }
        break;
    case 2:
        // Wait for the cutscene overlay to exit, then reload the field map.
        if (FieldSystem_IsRunningApplication(fieldSystem) == 0) {
            FieldSystem_StartFieldMap(fieldSystem);
            env->state++;
        }
        break;
    case 3:
        // Wait for the field map to finish loading.
        if (FieldSystem_IsRunningFieldMap(fieldSystem) == 0) {
            env->state++;
        }
        break;
    case 4:
        // One-frame delay before cleaning up.
        env->state++;
        break;
    case 5:
        Heap_Free(env->args);
        Heap_Free(env);
        return 1;
        break;
    }

    return 0;
}
