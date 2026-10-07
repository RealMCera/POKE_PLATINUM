#ifndef POKEPLATINUM_WFC_TRAINER_INFO_H
#define POKEPLATINUM_WFC_TRAINER_INFO_H

#include "struct_decls/struct_0207E060_decl.h"

#include "trainer_info.h"

WFCTrainerInfo *WFCTrainerInfo_New(const TrainerInfo *info, enum HeapID heapID);
void WFCTrainerInfo_Free(WFCTrainerInfo *wfcTrainerInfo);

#endif // POKEPLATINUM_WFC_TRAINER_INFO_H
