#ifndef POKEPLATINUM_WIFI_HISTORY_GEONET_H
#define POKEPLATINUM_WIFI_HISTORY_GEONET_H

#include "struct_defs/wi_fi_history.h"

void WiFiHistory_FlagGeonetLinkInfo(WiFiHistory *wiFiHistory);
void WiFiHistory_FlagGeonetCommunicatedWith(WiFiHistory *wiFiHistory, int country, int region, int language);

#endif // POKEPLATINUM_WIFI_HISTORY_GEONET_H
