#include "wifi_history_geonet.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/wi_fi_history.h"

#include "communication_information.h"
#include "communication_system.h"
#include "trainer_info.h"
#include "wifi_history_save_data.h"

/**
 * @brief Records the country and region of every other player in the current
 * link session into the Wi-Fi history's Geonet communication map.
 *
 * The local player is skipped, and nothing is recorded when there is no link
 * partner (i.e. CommInfo_TrainerInfo(0) is NULL). Each partner's country and
 * region is flagged via WiFiHistory_FlagGeonetCommunicatedWith.
 *
 * @param wiFiHistory Wi-Fi history save block to update
 */
void WiFiHistory_FlagGeonetLinkInfo(WiFiHistory *wiFiHistory)
{
    int connectedCnt = CommSys_ConnectedCount();

    if (CommInfo_TrainerInfo(0) == NULL) {
        return;
    }

    for (int i = 0; i < connectedCnt; i++) {
        if (CommSys_CurNetId() != i) {
            int country = CommInfo_PlayerCountry(i);
            int region = CommInfo_PlayerRegion(i);
            TrainerInfo *trainerInfo = CommInfo_TrainerInfo(i);
            int language = TrainerInfo_Language(trainerInfo);

            WiFiHistory_FlagGeonetCommunicatedWith(wiFiHistory, country, region, language);
        }
    }
}

/**
 * @brief Marks the given country and region as communicated with in the Geonet
 * communication map, unless it has already been recorded.
 *
 * The entry is set to 1 ("communicated today"); WiFiHistory_UpdateGeonetCommunicationMap
 * later ages it to 2 ("communicated in the past"). The language parameter is
 * currently unused.
 *
 * @param wiFiHistory Wi-Fi history save block to update
 * @param country     Country of the communication partner
 * @param region      Region of the communication partner
 * @param language    Language of the communication partner (unused)
 */
void WiFiHistory_FlagGeonetCommunicatedWith(WiFiHistory *wiFiHistory, int country, int region, int language)
{
    if (WiFiHistory_GetGeonetCommunicatedWith(wiFiHistory, country, region) == 0) {
        WiFiHistory_SetGeonetCommunicatedWith(wiFiHistory, country, region, 1);
    }
}
