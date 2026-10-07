#include <nitro.h>
#include <string.h>

#include "struct_defs/wi_fi_history.h"
#include "struct_defs/wifi_battle_tower_data.h"
#include "struct_defs/wifi_player_profile.h"

#include "appearance.h"
#include "save_player.h"
#include "savedata.h"
#include "trainer_info.h"
#include "wifi_battle_tower_save.h"
#include "wifi_history_save_data.h"

// Builds the Wi-Fi player profile exchanged with other players. The profile
// combines the trainer's identity and appearance, their country/region from the
// Wi-Fi history, their Frontier Easy Chat sentences, and a Battle Tower team
// with its rating. `teamIdx` selects which of the record's two stored teams is
// included.
void WifiPlayerProfile_Build(SaveData *saveData, int teamIdx, WifiPlayerProfile *profile)
{
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);
    WiFiHistory *wiFiHistory = SaveData_WiFiHistory(saveData);
    WifiBattleTowerRecord *record = SaveData_GetWifiBattleTowerRecord(saveData);

    MI_CpuClear8(profile, sizeof(WifiPlayerProfile));

    // Trainer identity.
    MI_CpuCopy8(TrainerInfo_Name(trainerInfo), profile->name, sizeof(profile->name));
    *(u32 *)profile->trainerInfoId = TrainerInfo_ID(trainerInfo);

    // Game and home-region metadata.
    profile->version = gGameVersion;
    profile->language = gGameLanguage;
    profile->country = (u8)WiFiHistory_GetCountry(wiFiHistory);
    profile->region = (u8)WiFiHistory_GetRegion(wiFiHistory);
    profile->gender = TrainerInfo_Gender(trainerInfo);
    profile->appearance = Appearance_GetData(profile->gender, TrainerInfo_Appearance(trainerInfo), APPEARANCE_DATA_TRAINER_CLASS_2);

    // The first three Frontier Easy Chat sentences are the player's battle
    // sentences; the fourth is their "No. 1" sentence.
    for (int i = 0; i < 3; i++) {
        MI_CpuCopy8(FrontierEasyChatMessages_GetSentence(saveData, 0 + i), &profile->battleSentences[8 * i], 8);
    }

    MI_CpuCopy8(FrontierEasyChatMessages_GetSentence(saveData, 3), profile->no1Sentence, 8);

    // Team 0 is only included when the record's bit-7 flag is set; all other
    // team indices always report the record's rating score and copy the team.
    if (teamIdx == 0) {
        profile->ratingScore = WifiBattleTowerRecord_UpdateBitFlag(record, 7, 0);

        if (profile->ratingScore) {
            WifiBattleTowerRecord_GetTeam(record, teamIdx, profile->mons);
        }
    } else {
        profile->ratingScore = WifiBattleTowerRecord_GetRatingScore(record);
        WifiBattleTowerRecord_GetTeam(record, teamIdx, profile->mons);
    }
}
