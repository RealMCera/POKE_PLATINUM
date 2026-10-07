#include "wifi_comm.h"

#include <nitro.h>
#include <string.h>

#include "overlay065/ov65_0223648C.h"
#include "overlay065/struct_ov65_022366E4.h"
#include "overlay065/struct_ov65_02236744_decl.h"
#include "overlay065/struct_ov65_02236760.h"

#include "unk_02032798.h"
#include "unk_02039A58.h"

// Communication commands shared by the overlay065 WiFi apps (the Nintendo WFC
// menu, WiFi battles and the Poffin/WiFi Plaza activity app). The command
// manager indexes the table returned by unk_02039A58() with (cmd - 22), so its
// three entries are commands 22, 23 and 24:
//   22: player join/leave status (UnkStruct_ov65_022366E4, fixed size)
//   23: sync request, no payload
//   24: voice chat enable flags (UnkStruct_ov65_02236760, fixed size)
// Each handler forwards the packet to overlay065 along with the app state that
// was registered by WiFiComm_Init.

// Registers the WiFi command table with the given app state as the context that
// the command manager passes back to every handler.
void WiFiComm_Init(UnkStruct_ov65_02236744 *app)
{
    CommCmd_Init(sub_02039A58(), sub_02039A60(), app);
}

// Registers the same table with no context, so any command that arrives while
// no WiFi app is running is dropped by the NULL checks below. Called before a
// WiFi battle starts, when no WiFi app state is registered yet.
void WiFiComm_InitNoContext(void)
{
    CommCmd_Init(sub_02039A58(), sub_02039A60(), NULL);
}

// Packet-size callbacks for the command table. Commands 22 and 24 carry a
// fixed struct; command 23 has no payload.

int WiFiComm_SyncPacketSize(void)
{
    return 0;
}

int WiFiComm_PlayerStatusPacketSize(void)
{
    return sizeof(UnkStruct_ov65_022366E4);
}

int WiFiComm_VoiceChatPacketSize(void)
{
    return sizeof(UnkStruct_ov65_02236760);
}

// Command 22: a player joined, left or changed connection state.
void WiFiComm_RecvPlayerStatus(int netId, int size, void *data, void *context)
{
    UnkStruct_ov65_02236744 *app = context;
    UnkStruct_ov65_022366E4 *packet = data;

    if (app == NULL) {
        return;
    }

    ov65_022366E4(app, packet);
}

// Command 23: asks all players to begin the WiFi sync.
void WiFiComm_RecvSyncRequest(int netId, int size, void *data, void *context)
{
    UnkStruct_ov65_02236744 *app = context;

    if (app == NULL) {
        return;
    }

    ov65_02236744(app);
}

// Command 24: carries the four players' voice chat enable flags.
void WiFiComm_RecvVoiceChat(int netId, int size, void *data, void *context)
{
    UnkStruct_ov65_02236744 *app = context;

    if (app == NULL) {
        return;
    }

    ov65_02236760(app, data);
}
