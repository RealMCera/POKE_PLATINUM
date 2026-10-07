#include "wfc_host_match.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_0207DFAC.h"

#include "comm_manager.h"

// Host-match callback registered via NintendoWFC_SetHostMatchCallback. When a
// player tries to join the host's WFC group, the host compares its own
// commState against the joining player's commState (stored in the friend slot
// indexed by hostFriendIdx). The two are compatible only when they belong to
// the same activity, so the callback returns TRUE for the known host/friend
// state pairs and FALSE otherwise.
BOOL WFCHostMatch_IsCompatible(int hostFriendIdx)
{
    int v0;
    int v1;
    WFCStatusBuffer *v2 = CommManager_GetUnk00();

    v0 = v2->ownStatus.commState;
    v1 = v2->friendStatuses[hostFriendIdx].commState;

    // Each pair below is (host commState, joining player commState) for one
    // activity. States 1-8 are the friend-side states and 9-16 the matching
    // host-side states of the same activities; 18-27 pair adjacent host/friend
    // states.
    if ((v0 == 12) && (v1 == 5)) {
        return 1;
    } else if ((v0 == 13) && (v1 == 6)) {
        return 1;
    } else if ((v0 == 14) && (v1 == 7)) {
        return 1;
    } else if ((v0 == 9) && (v1 == 2)) {
        return 1;
    } else if ((v0 == 10) && (v1 == 3)) {
        return 1;
    } else if ((v0 == 11) && (v1 == 4)) {
        return 1;
    } else if ((v0 == 15) && (v1 == 8)) {
        return 1;
    } else if ((v0 == 19) && (v1 == 18)) {
        return 1;
    } else if ((v0 == 21) && (v1 == 20)) {
        return 1;
    } else if ((v0 == 23) && (v1 == 22)) {
        return 1;
    } else if ((v0 == 25) && (v1 == 24)) {
        return 1;
    } else if ((v0 == 27) && (v1 == 26)) {
        return 1;
    } else if ((v0 == 16) && (v1 == 1)) {
        return 1;
    }

    return 0;
}
