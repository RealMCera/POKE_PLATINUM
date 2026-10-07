#ifndef POKEPLATINUM_WIFI_LOBBY_H
#define POKEPLATINUM_WIFI_LOBBY_H

#include "overlay_manager.h"

// Wi-Fi lobby application. Sets up the Nintendo Wi-Fi Connection (DWC) heap,
// loads the WFC/HTTP overlays, and hosts the Global Terminal (overlay061) and
// the Vs. Recorder. See src/wifi_lobby.c for the implementation.
int WiFiLobby_Init(ApplicationManager *appMan, int *param1);
int WiFiLobby_Main(ApplicationManager *appMan, int *param1);
int WiFiLobby_Exit(ApplicationManager *appMan, int *param1);

#endif // POKEPLATINUM_WIFI_LOBBY_H
