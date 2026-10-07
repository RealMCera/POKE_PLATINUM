#ifndef POKEPLATINUM_WIFI_MENU_H
#define POKEPLATINUM_WIFI_MENU_H

#include "field_task.h"

// Starts the Nintendo WFC menu from the WiFi Club script.
void WiFiMenu_Start(FieldTask *param0);
// Starts the Nintendo WFC menu from the GTS / WiFi Plaza / Battle Tower
// scripts and writes the WiFi app result back to `param1`.
void WiFiMenu_StartWithResult(FieldTask *param0, u16 *param1);

#endif // POKEPLATINUM_WIFI_MENU_H
