#ifndef POKEPLATINUM_COMM_SYNC_SAVE_H
#define POKEPLATINUM_COMM_SYNC_SAVE_H

#include "savedata.h"

void CommSyncSave_Reset(int *syncState);
int CommSyncSave_Update(SaveData *saveData, int blockID, int *syncState);

#endif // POKEPLATINUM_COMM_SYNC_SAVE_H
