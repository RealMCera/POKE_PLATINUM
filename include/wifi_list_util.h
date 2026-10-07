#ifndef POKEPLATINUM_WIFI_LIST_UTIL_H
#define POKEPLATINUM_WIFI_LIST_UTIL_H

#include "struct_decls/wi_fi_list.h"

#include "savedata.h"

int WiFiList_InitDWC(enum HeapID heapID);
void WiFiList_InitUserData(WiFiList *wiFiList);
int WiFiList_GetUserGsProfileId(WiFiList *wiFiList);
BOOL WiFiList_HasValidLogin(SaveData *saveData);

#endif // POKEPLATINUM_WIFI_LIST_UTIL_H
