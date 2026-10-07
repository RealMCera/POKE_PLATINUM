#ifndef POKEPLATINUM_EMAIL_H
#define POKEPLATINUM_EMAIL_H

#include "struct_defs/struct_02030CEC.h"
#include "struct_defs/world_exchange.h"

#include "savedata.h"

int Email_SaveSize(void);
void Email_Init(EmailSaveData *emailSaveData);
void Email_InitSaveData(SaveData *saveData);
BOOL Email_HasEmailString(SaveData *saveData);
void Email_SetEmailString(SaveData *saveData, const char *emailString);
char *Email_GetEmailString(SaveData *saveData);
void Email_SetValue(SaveData *saveData, int index, u32 value);
u32 Email_GetValue(SaveData *saveData, int index);
u32 WorldExchange_InitTrainer(SaveData *saveData, WorldExchangeTrainer *trainer);
void WorldExchange_GetTrainerObject(SaveData *saveData, WorldExchangeTrainer *trainer);

#endif // POKEPLATINUM_EMAIL_H
