#ifndef POKEPLATINUM_WIFI_PLAYER_PROFILE_H
#define POKEPLATINUM_WIFI_PLAYER_PROFILE_H

#include "struct_defs/wifi_player_profile.h"

#include "savedata.h"

// Wi-Fi player profiles: the trainer identity, appearance, Frontier Easy Chat
// sentences, and Battle Tower team/rating data exchanged with other players.
void WifiPlayerProfile_Build(SaveData *saveData, int teamIdx, WifiPlayerProfile *profile);

#endif // POKEPLATINUM_WIFI_PLAYER_PROFILE_H
