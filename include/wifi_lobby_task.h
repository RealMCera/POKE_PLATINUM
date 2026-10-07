#ifndef POKEPLATINUM_WIFI_LOBBY_TASK_H
#define POKEPLATINUM_WIFI_LOBBY_TASK_H

#include "field_task.h"

// Starts the Wi-Fi lobby field task. `wifiMenuResult` is the result reported by
// the preceding WiFi menu and is forwarded to the lobby app.
void WiFiLobbyTask_Start(FieldTask *task, BOOL wifiMenuResult);

#endif // POKEPLATINUM_WIFI_LOBBY_TASK_H
