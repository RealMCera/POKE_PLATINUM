#include "wifi_comm_cmd.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"

#include "wifi_comm.h"

// Command table for the overlay065 WiFi apps. The command manager indexes this
// table with (cmd - 22), so the entries are commands 22, 23 and 24:
//   22: player join/leave status (WiFiComm_RecvPlayerStatus)
//   23: sync request (WiFiComm_RecvSyncRequest)
//   24: voice chat enable flags (WiFiComm_RecvVoiceChat)
// Each entry pairs a handler with the callback that reports its payload size;
// no command provides a receive buffer, so the third field is NULL.
static const CommCmdTable sWiFiCommCmdTable[] = {
    { WiFiComm_RecvPlayerStatus, WiFiComm_PlayerStatusPacketSize, NULL },
    { WiFiComm_RecvSyncRequest, WiFiComm_SyncPacketSize, NULL },
    { WiFiComm_RecvVoiceChat, WiFiComm_VoiceChatPacketSize, NULL }
};

// Returns the WiFi communication command table.
const CommCmdTable *WiFiCommCmd_GetTable(void)
{
    return sWiFiCommCmdTable;
}

// Returns the number of entries in the WiFi communication command table.
int WiFiCommCmd_GetTableCount(void)
{
    return sizeof(sWiFiCommCmdTable) / sizeof(CommCmdTable);
}
