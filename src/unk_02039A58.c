#include "unk_02039A58.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/comm_cmd_table.h"

#include "wifi_comm.h"

static const CommCmdTable Unk_020E5F24[] = {
    { WiFiComm_RecvPlayerStatus, WiFiComm_PlayerStatusPacketSize, NULL },
    { WiFiComm_RecvSyncRequest, WiFiComm_SyncPacketSize, NULL },
    { WiFiComm_RecvVoiceChat, WiFiComm_VoiceChatPacketSize, NULL }
};

const CommCmdTable *sub_02039A58(void)
{
    return Unk_020E5F24;
}

int sub_02039A60(void)
{
    return sizeof(Unk_020E5F24) / sizeof(CommCmdTable);
}
