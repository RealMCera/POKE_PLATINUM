#include "email.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_02030CEC.h"
#include "struct_defs/wi_fi_history.h"
#include "struct_defs/world_exchange.h"

#include "charcode_util.h"
#include "math_util.h"
#include "save_player.h"
#include "savedata.h"
#include "trainer_info.h"
#include "wifi_history_save_data.h"

static void WorldExchange_InitTrainerFields(SaveData *saveData, WorldExchangeTrainer *trainer);

int Email_SaveSize(void)
{
    return sizeof(EmailSaveData);
}

void Email_Init(EmailSaveData *emailSaveData)
{
    MI_CpuClear8(emailSaveData, sizeof(EmailSaveData));

    memset(emailSaveData->email, '\0', (50 + 1));
    emailSaveData->wiiMessageReception = 1;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);
}

// Initializes the email save block for the given save data.
void Email_InitSaveData(SaveData *saveData)
{
    Email_Init(SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL));
}

// Returns TRUE once a Wii number has been registered.
BOOL Email_HasEmailString(SaveData *saveData)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);

    if (emailSaveData->email[0] == '\0') {
        return 0;
    }

    return 1;
}

void Email_SetEmailString(SaveData *saveData, const char *emailString)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);

    strcpy(emailSaveData->email, emailString);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);
}

char *Email_GetEmailString(SaveData *saveData)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);
    return emailSaveData->email;
}

// Sets one of the World Exchange fields by index:
// 0 = wiiMessageReception, 1 = rngValue, 2 = registrationCode, 3 = password.
void Email_SetValue(SaveData *saveData, int index, u32 value)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);

    switch (index) {
    case 0:
        emailSaveData->wiiMessageReception = value;
        break;
    case 1:
        emailSaveData->rngValue = value;
        break;
    case 2:
        emailSaveData->registrationCode = value;
        break;
    case 3:
        emailSaveData->password = value;
        break;
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);
}

u32 Email_GetValue(SaveData *saveData, int index)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);

    switch (index) {
    case 0:
        return emailSaveData->wiiMessageReception;
    case 1:
        return emailSaveData->rngValue;
    case 2:
        return emailSaveData->registrationCode;
    case 3:
        return emailSaveData->password;
    }

    return 0;
}

// Fills the World Exchange trainer fields that are common to both a freshly
// generated profile and one restored from the save: game version, language,
// country, region, trainer ID and name, and the registered Wii number.
static void WorldExchange_InitTrainerFields(SaveData *saveData, WorldExchangeTrainer *trainer)
{
    WiFiHistory *wiFiHistory = SaveData_WiFiHistory(saveData);
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(saveData);
    char *emailString = Email_GetEmailString(saveData);

    MI_CpuClear8(trainer, sizeof(WorldExchangeTrainer));

    trainer->gameCode = GAME_VERSION;
    trainer->language = GAME_LANGUAGE;
    trainer->country = WiFiHistory_GetCountry(wiFiHistory);
    trainer->region = WiFiHistory_GetRegion(wiFiHistory);
    trainer->trainerId = TrainerInfo_ID(trainerInfo);

    CharCode_Copy(trainer->trainerName, TrainerInfo_Name(trainerInfo));
    trainer->unk_10 = 0;

    strcpy(trainer->email, emailString);
    trainer->emailInitialised = Email_GetValue(saveData, 0);

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);
}

// Initializes a World Exchange trainer profile with a fresh random value and
// returns that value. The registration code is left unset (0xffff) until the
// player enters it.
u32 WorldExchange_InitTrainer(SaveData *saveData, WorldExchangeTrainer *trainer)
{
    u32 rngValue;

    WorldExchange_InitTrainerFields(saveData, trainer);

    rngValue = LCRNG_Next() % 1000;

    trainer->rngValue = rngValue;
    trainer->registrationCode = 0xffff;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);

    return rngValue;
}

// Fills a World Exchange trainer profile from the saved data, restoring the
// random value and registration code stored when the Wii number was registered.
void WorldExchange_GetTrainerObject(SaveData *saveData, WorldExchangeTrainer *trainer)
{
    EmailSaveData *emailSaveData = SaveData_SaveTable(saveData, SAVE_TABLE_ENTRY_EMAIL);

    WorldExchange_InitTrainerFields(saveData, trainer);

    trainer->rngValue = emailSaveData->rngValue;
    trainer->registrationCode = emailSaveData->registrationCode;

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_EMAIL);
}
