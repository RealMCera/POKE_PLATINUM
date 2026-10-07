#include "overlay104/ov104_0223D860.h"

#include <nitro.h>
#include <string.h>

#include "struct_decls/battle_frontier_decl.h"

#include "overlay063/ov63_0222D77C.h"
#include "overlay063/struct_ov63_0222CCB8.h"
#include "overlay104/frontier_graphics.h"
#include "overlay104/ov104_02231F74.h"
#include "overlay104/struct_ov104_0223C634.h"
#include "overlay104/struct_ov104_0223D8F0.h"

#include "battle_frontier.h"

typedef BOOL (*UnkFuncPtr_ov104_022418A8)(BattleFrontier *, FrontierObject *);

static void ov104_0223D898(BattleFrontier *param0, FrontierObject *param1);
void ov104_0223D860(BattleFrontier *param0, u16 param1, u8 param2, s16 param3[], int param4);
void ov104_0223D8C4(BattleFrontier *param0);
static BOOL ov104_0223D8F0(BattleFrontier *param0, FrontierObject *param1);

static const UnkFuncPtr_ov104_022418A8 Unk_ov104_022418A8[] = {
    NULL,
    ov104_0223D8F0
};

void ov104_0223D860(BattleFrontier *param0, u16 param1, u8 param2, s16 param3[], int param4)
{
    FrontierObject *v0;
    int v1;

    v0 = ov104_0223D5A8(param0, param1);
    MI_CpuClear8(&v0->movement, sizeof(FrontierObjectMovement));

    v0->movement.type = param2;

    for (v1 = 0; v1 < param4; v1++) {
        v0->movement.params[v1] = param3[v1];
    }
}

static void ov104_0223D898(BattleFrontier *param0, FrontierObject *param1)
{
    int v0;

    if (Unk_ov104_022418A8[param1->movement.type] == NULL) {
        return;
    }

    v0 = Unk_ov104_022418A8[param1->movement.type](param0, param1);

    if (v0 == 1) {
        MI_CpuClear8(&param1->movement, sizeof(FrontierObjectMovement));
    }
}

void ov104_0223D8C4(BattleFrontier *param0)
{
    FrontierObject *v0;
    int v1;

    v0 = BattleFrontier_GetObjects(param0);

    for (v1 = 0; v1 < 32; v1++) {
        if ((v0->object != NULL) && (v0->movementTask == NULL)) {
            ov104_0223D898(param0, v0);
        }

        v0++;
    }
}

static BOOL ov104_0223D8F0(BattleFrontier *param0, FrontierObject *param1)
{
    FrontierObjectMovement *v0 = &param1->movement;
    UnkStruct_ov63_0222CCB8 v1;
    FrontierGraphics *v2 = BattleFrontier_GetGraphics(param0);

    if (v0->params[2] > 0) {
        v0->params[2]--;
        return 0;
    }

    switch (v0->step) {
    case 0:
        switch (v0->params[0]) {
        case 0:
        case 1:
            if (v0->params[1] == 0) {
                v0->params[3] = 2;
                v0->params[4] = 3;
            } else {
                v0->params[3] = 3;
                v0->params[4] = 2;
            }
            break;
        case 2:
        case 3:
            if (v0->params[1] == 0) {
                v0->params[3] = 0;
                v0->params[4] = 1;
            } else {
                v0->params[3] = 1;
                v0->params[4] = 0;
            }
            break;
        default:
            GF_ASSERT(FALSE);
            return 1;
        }

        v0->params[5] = v0->params[0];
        v0->step++;
    case 1:
    case 2:
    case 3:
        ov104_02232C80(&v1, param1->object, param1->params.unk_04, v0->params[3 + v0->step - 1]);
        ov63_0222D7C8(v2->unk_30, &v1);

        if (v0->params[0] == v0->params[3 + v0->step - 1]) {
            v0->params[2] = 45;
            v0->step = 1;
        } else {
            v0->params[2] = 30;
            v0->step++;
        }
        break;
    default:
        return 1;
    }

    return 0;
}
