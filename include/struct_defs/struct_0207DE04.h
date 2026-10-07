#ifndef POKEPLATINUM_STRUCT_0207DE04_H
#define POKEPLATINUM_STRUCT_0207DE04_H

#include "savedata.h"

// Arguments shared between the WiFi menu field task (wifi_menu.c) and the
// overlay065 communication app it launches. The task fills in the request
// (appType, minPlayers, requiredPlayers) before starting the app; the app
// writes back the outcome (success, result) when it exits.
typedef struct {
    // Which activity to run: 0 = Poffin cooking, 1 = Swalot minigame,
    // 2 = Mime Jr. minigame, 3 = Wobbuffet minigame.
    u8 appType;
    // Minimum number of connected players needed to begin.
    u8 minPlayers;
    // Number of players the selected activity requires.
    u8 requiredPlayers;
    // Set to 1 by the app when the activity completed successfully.
    u8 success;
    // Activity-specific result passed on to the follow-up app.
    u8 result;
    u8 padding[3];
    SaveData *saveData;
} WiFiCommAppArgs;

#endif // POKEPLATINUM_STRUCT_0207DE04_H
