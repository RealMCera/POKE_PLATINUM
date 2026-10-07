#include "comm_sync_save.h"

#include <nitro.h>
#include <string.h>

#include "savedata.h"
#include "comm_tool.h"

// Resets the save-sync state machine to its initial state.
void CommSyncSave_Reset(int *syncState)
{
    *syncState = 0;
}

// Advances the save-sync state machine by one step. Returns TRUE once the save
// has been written and every player has confirmed it.
//
// The machine writes the save in two phases, each gated by a comm sync so that
// all players write at the same time:
//   0-2: start sync 111, wait for it, then init the save state and run it until
//        the final block has been written (SAVE_RESULT_PROCEED_FINAL).
//   3-5: start sync 112, wait for it, then run the save state until it finishes
//        (SAVE_RESULT_OK).
//   6:   done.
int CommSyncSave_Update(SaveData *saveData, int blockID, int *syncState)
{
    int saveResult;

    switch (*syncState) {
    case 0:
        // Announce that this player is ready to write its save.
        CommTiming_StartSync(111);
        *syncState = 1;
        break;
    case 1:
        // Wait until every player has reached sync 111, then begin writing.
        if (CommTiming_IsSyncState(111)) {
            SaveData_SaveStateInit(saveData, blockID);
            *syncState = 2;
        }
        break;
    case 2:
        // Write the save blocks; the final block reports PROCEED_FINAL.
        saveResult = SaveData_SaveStateMain(saveData);

        GF_ASSERT(saveResult != SAVE_RESULT_OK);
        GF_ASSERT(saveResult != SAVE_RESULT_CORRUPT);

        if (saveResult == SAVE_RESULT_PROCEED_FINAL) {
            *syncState = 3;
            CommTiming_StartSync(112);
        }
        break;
    case 3:
        // Wait until every player has finished writing.
        if (CommTiming_IsSyncState(112)) {
            *syncState = 4;
        }
        break;
    case 4:
        // Flush the save state; OK means the save is fully committed.
        saveResult = SaveData_SaveStateMain(saveData);

        GF_ASSERT(saveResult != SAVE_RESULT_CORRUPT);
        GF_ASSERT(saveResult != SAVE_RESULT_PROCEED_FINAL);

        if (saveResult == SAVE_RESULT_OK) {
            *syncState = 5;
        }
        break;
    case 5:
        *syncState = 6;
        break;
    case 6:
        return TRUE;
        break;
    }

    return FALSE;
}
