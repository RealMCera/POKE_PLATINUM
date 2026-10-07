#include "overlay006/ov6_022489E4.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_020954F0.h"

#include "overlay006/struct_ov6_02248BE8.h"
#include "overlay006/struct_ov6_02248DD8.h"

#include "chatot_cry.h"
#include "communication_system.h"
#include "contest.h"
#include "heap.h"
#include "pokemon.h"
#include "string_gf.h"

typedef struct {
    u8 unk_00;
    u8 unk_01;
    u8 unk_02;
    u8 unk_03[1];
} UnkStruct_ov6_02248A94;

static int ov6_02248A94(UnkStruct_ov6_02248A94 *param0[4], int connectionCount, u8 *param2);

void ov6_022489E4(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;

    MI_CpuCopy8(param2, v0->leaderElectionRecvBuf[param0], param1);
    v0->commRecvCount++;

    if (v0->commRecvCount >= v0->data.connectionCount) {
        UnkStruct_ov6_02248A94 *v1[4];
        int v2;
        u8 v3;

        for (v2 = 0; v2 < v0->data.connectionCount; v2++) {
            v1[v2] = (void *)v0->leaderElectionRecvBuf[v2];
        }

        v0->data.leaderContestantID = ov6_02248A94(v1, v0->data.connectionCount, &v3);
        v0->data.leaderElectionResult = v3;
        v0->data.npcPhotoPreset = v1[v0->data.leaderContestantID]->unk_02;
    }
}

BOOL ov6_02248A64(Contest *param0)
{
    UnkStruct_ov6_02248A94 v0;

    v0.unk_00 = param0->data.leaderElectionValue;
    v0.unk_01 = param0->data.playerContestantID;
    v0.unk_02 = param0->data.npcPhotoPreset;

    if (CommSys_SendData(26, &v0, sizeof(UnkStruct_ov6_02248A94)) == 1) {
        return 1;
    }

    return 0;
}

static int ov6_02248A94(UnkStruct_ov6_02248A94 *param0[4], int connectionCount, u8 *param2)
{
    int v0, v1 = 0;

    for (v0 = 0; v0 < connectionCount; v0++) {
        if (param0[v1]->unk_00 < param0[v0]->unk_00) {
            v1 = v0;
        }
    }

    *param2 = param0[v1]->unk_00;
    return v1;
}

void ov6_02248AC8(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int v1;
    int v2;
    u8 *v3;

    v1 = Pokemon_StructSize();
    v3 = param2;
    v2 = v3[v1];

    MI_CpuCopy8(param2, v0->data.contestMons[v2], v1);

    v0->commRecvCount++;
}

BOOL ov6_02248AF0(Contest *param0, int param1, const Pokemon *param2)
{
    u8 *v0;
    int v1;
    int v2;

    v1 = Pokemon_StructSize();
    v0 = Heap_Alloc(HEAP_ID_20, v1 + 1);
    MI_CpuCopy8(param2, v0, v1);
    v0[v1] = param1;

    if (CommSys_SendData(27, v0, v1 + 1) == 1) {
        v2 = 1;
    } else {
        v2 = 0;
    }

    Heap_Free(v0);
    return v2;
}

void ov6_02248B30(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int v1, v2;
    u8 *v3;
    int v4;

    v1 = Pokemon_StructSize();
    v2 = v1 * 4;
    v3 = param2;

    for (v4 = 0; v4 < 4; v4++) {
        MI_CpuCopy8(&v3[v1 * v4], v0->data.contestMons[v4], v1);
    }

    v0->commRecvCount++;
}

BOOL ov6_02248B70(Contest *param0, Pokemon **param1)
{
    u8 *v0;
    int v1, v2;
    int v3;
    int v4;

    v1 = Pokemon_StructSize();
    v2 = v1 * 4;
    v0 = param0->commSendBuf;

    for (v4 = 0; v4 < 4; v4++) {
        MI_CpuCopy8(param1[v4], &v0[v1 * v4], v1);
    }

    if (CommSys_SendDataHuge(28, v0, v2) == 1) {
        v3 = 1;
    } else {
        v3 = 0;
    }

    return v3;
}

void ov6_02248BC0(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int v1;
    int v2;
    u8 *v3;

    v1 = sizeof(UnkStruct_ov6_02248BE8);
    v3 = param2;
    v2 = v3[v1];

    MI_CpuCopy8(param2, &v0->data.opponentData[v2], v1);

    v0->commRecvCount++;
}

BOOL ov6_02248BE8(Contest *param0, int param1, const UnkStruct_ov6_02248BE8 *param2)
{
    u8 *v0;
    int v1;
    int v2;

    v1 = sizeof(UnkStruct_ov6_02248BE8);
    v0 = Heap_Alloc(HEAP_ID_20, v1 + 1);
    MI_CpuCopy8(param2, v0, v1);
    v0[v1] = param1;

    if (CommSys_SendData(29, v0, v1 + 1) == 1) {
        v2 = 1;
    } else {
        v2 = 0;
    }

    Heap_Free(v0);
    return v2;
}

void ov6_02248C28(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int v1;
    u8 *v2;
    int v3;

    v1 = sizeof(ContestJudge) * (1 + 2) + 1;
    v2 = param2;

    for (v3 = 0; v3 < (1 + 2); v3++) {
        MI_CpuCopy8(&v2[sizeof(ContestJudge) * v3], &v0->data.judges[v3], sizeof(ContestJudge));
    }

    v0->data.bonusJudgeIndex = v2[v1 - 1];
    v0->commRecvCount++;
}

BOOL ov6_02248C68(Contest *param0, int param1, const ContestJudge *param2)
{
    u8 *v0;
    int v1;
    int v2;
    int v3;
    const ContestJudge *v4 = param2;
    v1 = sizeof(ContestJudge) * (1 + 2) + 1;
    v0 = Heap_Alloc(HEAP_ID_20, v1);

    for (v3 = 0; v3 < (1 + 2); v3++) {
        MI_CpuCopy8(v4, &v0[sizeof(ContestJudge) * v3], sizeof(ContestJudge));
        v4++;
    }

    v0[v1 - 1] = param0->data.bonusJudgeIndex;

    if (CommSys_SendData(30, v0, v1) == 1) {
        v2 = 1;
    } else {
        v2 = 0;
    }

    Heap_Free(v0);
    return v2;
}

void ov6_02248CBC(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int contestantID, v2, v3;
    u8 *v4;
    u16 *v5;

    v3 = 4;
    v4 = param2;
    contestantID = v4[0];
    v2 = v4[1];
    v5 = (u16 *)(&v4[v3]);

    String_Clear(v0->data.trainerNames[contestantID]);
    String_CopyChars(v0->data.trainerNames[contestantID], v5);

    v0->commRecvCount++;
}

BOOL ov6_02248CE8(Contest *contest, int param1, const String *trainerNames)
{
    int v0, v1;
    u8 *v2;
    BOOL v3;
    u16 v4[TRAINER_NAME_LEN + 1];

    v0 = 8 * sizeof(u16);
    v1 = 4;

    String_ToChars(trainerNames, v4, TRAINER_NAME_LEN + 1);

    v2 = Heap_Alloc(HEAP_ID_20, v0 + v1);
    MI_CpuCopy8(v4, &v2[v1], v0);
    v2[0] = param1;
    v2[1] = v0;
    v2[2] = 0;
    v2[3] = 0;

    if (CommSys_SendData(31, v2, v0 + v1) == 1) {
        v3 = 1;
    } else {
        v3 = 0;
    }

    Heap_Free(v2);
    return v3;
}

void ov6_02248D38(int param0, int param1, void *param2, void *param3)
{
    Contest *v0 = param3;
    int v1;
    int v2;
    u8 *v3;

    v1 = ChatotCry_SaveSize();
    v3 = param2;
    v2 = v3[v1];

    MI_CpuCopy8(param2, v0->chatotCry[v2], v1);

    v0->commRecvCount++;
}

BOOL ov6_02248D64(Contest *param0, int param1, void *param2)
{
    u8 *v0;
    int v1;
    int v2;

    v1 = ChatotCry_SaveSize();
    v0 = param0->commSendBuf;

    if (param2 != NULL) {
        MI_CpuCopy8(param2, v0, v1);
    }

    v0[v1] = param1;

    if (CommSys_SendDataHuge(36, v0, v1 + 1) == 1) {
        v2 = 1;
    } else {
        v2 = 0;
    }

    return v2;
}

void ov6_02248DA0(int param0, int param1, void *param2, void *param3)
{
    Contest *contest = param3;
    int v1;
    int contestantID;
    u8 *v3;
    UnkStruct_ov6_02248DD8 *v4;

    v1 = sizeof(UnkStruct_ov6_02248DD8);
    v4 = param2;
    v3 = param2;
    contestantID = v3[v1];

    contest->data.trainerGenders[contestantID] = v4->trainerGender;
    contest->data.cameraFlashVariant[contestantID] = v4->unk_01;
    contest->data.monContestFame[contestantID] = v4->monContestFame;
    contest->data.contestantObjEventGFX[contestantID] = v4->contestantObjEventGFX;
    contest->commRecvCount++;
}

BOOL ov6_02248DD8(Contest *param0, int param1, const UnkStruct_ov6_02248DD8 *param2)
{
    u8 *v0;
    int v1;
    int v2;

    v1 = sizeof(UnkStruct_ov6_02248DD8);
    v0 = Heap_Alloc(HEAP_ID_20, v1 + 1);
    MI_CpuCopy8(param2, v0, v1);
    v0[v1] = param1;

    if (CommSys_SendData(37, v0, v1 + 1) == 1) {
        v2 = 1;
    } else {
        v2 = 0;
    }

    Heap_Free(v0);
    return v2;
}
