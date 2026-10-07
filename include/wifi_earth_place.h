#ifndef POKEPLATINUM_WIFI_EARTH_PLACE_H
#define POKEPLATINUM_WIFI_EARTH_PLACE_H

u32 WiFiEarthPlace_GetCount(void);
u32 WiFiEarthPlace_GetIndexByCountry(u32 country);
u32 WiFiEarthPlace_GetRegionLimit(u32 country);
u32 WiFiEarthPlace_GetMessageBankByCountry(u32 country);
u32 WiFiEarthPlace_GetMessageBank(u32 index);
u32 WiFiEarthPlace_GetCountry(u32 index);
u32 WiFiEarthPlace_GetNarcMemberIndex(u32 index);
const u8 *WiFiEarthPlace_GetRegionList(u32 index);
u32 WiFiEarthPlace_GetRegionCount(u32 index);

#endif // POKEPLATINUM_WIFI_EARTH_PLACE_H
