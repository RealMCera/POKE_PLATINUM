#ifndef POKEPLATINUM_WIFI_COMM_CMD_H
#define POKEPLATINUM_WIFI_COMM_CMD_H

#include "struct_defs/comm_cmd_table.h"

// Command table interface for the overlay065 WiFi apps. See wifi_comm_cmd.c
// for the command layout.
const CommCmdTable *WiFiCommCmd_GetTable(void);
int WiFiCommCmd_GetTableCount(void);

#endif // POKEPLATINUM_WIFI_COMM_CMD_H
