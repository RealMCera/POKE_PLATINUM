#ifndef POKEPLATINUM_WIFI_COMM_H
#define POKEPLATINUM_WIFI_COMM_H

#include "overlay065/struct_ov65_02236744_decl.h"

// Communication command interface for the overlay065 WiFi apps. See
// wifi_comm.c for the command layout.

void WiFiComm_Init(UnkStruct_ov65_02236744 *app);
void WiFiComm_InitNoContext(void);
int WiFiComm_SyncPacketSize(void);
int WiFiComm_PlayerStatusPacketSize(void);
int WiFiComm_VoiceChatPacketSize(void);
void WiFiComm_RecvPlayerStatus(int netId, int size, void *data, void *context);
void WiFiComm_RecvSyncRequest(int netId, int size, void *data, void *context);
void WiFiComm_RecvVoiceChat(int netId, int size, void *data, void *context);

#endif // POKEPLATINUM_WIFI_COMM_H
