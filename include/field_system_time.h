#ifndef POKEPLATINUM_FIELD_SYSTEM_TIME_H
#define POKEPLATINUM_FIELD_SYSTEM_TIME_H

#include <nitro/rtc.h>

#include "field/field_system_decl.h"

#include "rtc.h"

void FieldSystem_UpdateGameTime(FieldSystem *fieldSystem);
enum TimeOfDay FieldSystem_GetTimeOfDay(const FieldSystem *fieldSystem);
int FieldSystem_GetMonth(const FieldSystem *fieldSystem);
int FieldSystem_GetDayOfMonth(const FieldSystem *fieldSystem);
int FieldSystem_GetWeek(const FieldSystem *fieldSystem);
int FieldSystem_GetHour(const FieldSystem *fieldSystem);
int FieldSystem_GetMinute(const FieldSystem *fieldSystem);
void FieldSystem_GetStartTimestamp(const FieldSystem *fieldSystem, RTCDate *destDate, RTCTime *destTime);
void FieldSystem_GetFirstCompletionTimestamp(const FieldSystem *fieldSystem, RTCDate *destDate, RTCTime *destTime);
void FieldSystem_RecordFirstCompletion(const FieldSystem *fieldSystem);
BOOL FieldSystem_HasPenalty(FieldSystem *fieldSystem);

#endif // POKEPLATINUM_FIELD_SYSTEM_TIME_H
