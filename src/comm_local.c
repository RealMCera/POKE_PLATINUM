#include "comm_local.h"

#include <nitro.h>
#include <string.h>

// Maximum number of remote machines (players other than the local one) that
// may take part in each comm type, indexed by comm type. Callers add one to
// obtain the maximum player count; see CommType_MaxPlayers.
u16 CommLocal_MaxMachines(u16 commType)
{
    u8 maxMachines[] = {
        0x1,
        0x1,
        0x1,
        0x1,
        0x3,
        0x3,
        0x3,
        0x4,
        0x3,
        0x4,
        0x7,
        0x3,
        0x7,
        0x4,
        0x1,
        0x4,
        0x1,
        0x1,
        0x4,
        0x1,
        0x1,
        0x1,
        0x1,
        0x3,
        0x0,
        0x0,
        0x4,
        0x1,
        0x1,
        0x2,
        0x1,
        0x1,
        0x1,
        0x3,
        0x1,
        0x3,
        0x0
    };

    GF_ASSERT(commType < sizeof(maxMachines));
    return maxMachines[commType];
}

// Minimum number of remote machines required for each comm type, indexed by
// comm type. Callers add one to obtain the minimum player count; see
// CommType_MinPlayers.
u16 CommLocal_MinMachines(u16 commType)
{
    u8 minMachines[] = {
        0x1,
        0x1,
        0x1,
        0x1,
        0x3,
        0x3,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0
    };

    GF_ASSERT(commType < sizeof(minMachines));
    return minMachines[commType];
}

// Union-room activities share one compatible group: beacons from any of these
// comm types are accepted by any other (see CommServerClient_ScanCallback), and
// players use their Union appearance while in battle (see
// FieldBattleDTO_InitWithPartyOrderFromSave).
BOOL CommLocal_IsUnionGroup(int commType)
{
    switch (commType) {
    case 18: // COMM_TYPE_UNION_APP
    case 9:  // COMM_TYPE_UNION
    case 13: // COMM_TYPE_DRAW
    case 7:  // COMM_TYPE_MIX_RECORDS
    case 26: // COMM_TYPE_SPIN_TRADE
        return 1;
    }

    return 0;
}

// Comm types that connect over Nintendo Wi-Fi Connection.
BOOL CommLocal_IsWifiGroup(int commType)
{
    switch (commType) {
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 29:
    case 33:
    case 34:
    case 35:
    case 36:
        return 1;
    }

    return 0;
}

// Wi-Fi comm types that gather all connected players into a shared lobby and
// use conference-mode voice chat (see NintendoWFC_StartVoiceChat and
// VoiceChat_Init).
BOOL CommLocal_IsWifiConferenceGroup(int commType)
{
    switch (commType) {
    case 29: // COMM_TYPE_POFFIN_WIFI
    case 33: // COMM_TYPE_WIFI_PLAZA
    case 35: // COMM_TYPE_CLUB_WIFI
        return 1;
    }

    return 0;
}

// Wi-Fi comm types that only accept friends: a non-friend host causes new
// connections to be refused (see MatchmakingHostMatchedCallback).
BOOL CommLocal_IsFriendOnlyWifiGroup(int commType)
{
    switch (commType) {
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 34:
        return 1;
    }

    return 0;
}

// Link battle and contest comm types, which use a short WirelessManager
// connection lifetime (see WirelessManager_ConnectServer and
// WirelessManager_ConnectClient).
BOOL CommLocal_IsBattleOrContestGroup(int commType)
{
    switch (commType) {
    case 1: // COMM_TYPE_SINGLE_BATTLE
    case 2: // COMM_TYPE_DOUBLE_BATTLE
    case 3: // COMM_TYPE_MIX_BATTLE
    case 4: // COMM_TYPE_MULTI_BATTLE_1
    case 5: // COMM_TYPE_MULTI_BATTLE_2
    case 8: // COMM_TYPE_CONTEST
        return 1;
    }

    return 0;
}
