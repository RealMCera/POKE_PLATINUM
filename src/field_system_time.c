#include "field_system_time.h"

#include <nitro.h>
#include <string.h>

#include "struct_decls/tv_broadcast.h"

#include "field/field_system.h"
#include "savedata/save_table.h"

#include "berry_patch_manager.h"
#include "field_system.h"
#include "inlines.h"
#include "party.h"
#include "pokemon.h"
#include "record_mixed_rng.h"
#include "rtc.h"
#include "script_manager.h"
#include "special_encounter.h"
#include "system_data.h"
#include "system_vars.h"
#include "trainer_case_badge_dirt.h"
#include "tv_segment.h"
#include "underground.h"
#include "tv_broadcast.h"
#include "battle_salon.h"
#include "vars_flags.h"
#include "wifi_history_save_data.h"

static void FieldSystem_HandleDailyEvents(FieldSystem *fieldSystem, s32 daysPassed);
static void FieldSystem_HandleElapsedTimeEvents(FieldSystem *fieldSystem, s32 minutesElapsed, const RTCTime *rtcTime);
static void FieldSystem_HandleDayChange(FieldSystem *fieldSystem, GameTime *gameTime, const RTCDate *currentDate);
static void FieldSystem_ProcessElapsedTime(FieldSystem *fieldSystem, GameTime *gameTime, const RTCDate *currentDate, const RTCTime *currentTime);

// Called when the player enters the overworld (e.g. after a map warp). Reads the
// RTC and advances the saved GameTime, firing any daily or elapsed-time events
// that happened while the game was not running.
void FieldSystem_UpdateGameTime(FieldSystem *fieldSystem)
{
    RTCDate currentDate;
    RTCTime currentTime;
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);

    // The canary is only set once GameTime_Clear has initialised the save's clock.
    if (gameTime->canary == FALSE) {
        return;
    }

    GetCurrentDateTime(&currentDate, &currentTime);
    FieldSystem_HandleDayChange(fieldSystem, gameTime, &currentDate);

    FieldSystem_ProcessElapsedTime(fieldSystem, gameTime, &currentDate, &currentTime);
}

// Compares the RTC's day counter against the day stored in the save. If the RTC
// has moved backwards (e.g. its battery was removed), the stored day is simply
// resynced. If it has moved forwards, the daily events for each elapsed day are
// processed.
static void FieldSystem_HandleDayChange(FieldSystem *fieldSystem, GameTime *gameTime, const RTCDate *currentDate)
{
    s32 currentDay = RTC_ConvertDateToDay(currentDate);

    if (currentDay < gameTime->day) {
        gameTime->day = currentDay;
    } else if (currentDay > gameTime->day) {
        FieldSystem_HandleDailyEvents(fieldSystem, currentDay - gameTime->day);
        gameTime->day = currentDay;
    }
}

// Computes how many whole minutes have elapsed between the timestamp stored in
// the save and the current RTC time. If the RTC is behind the save (clock rolled
// back), the stored timestamp is just resynced. Otherwise the playthrough
// penalty is decremented and the elapsed-time events are run before resyncing.
static void FieldSystem_ProcessElapsedTime(FieldSystem *fieldSystem, GameTime *gameTime, const RTCDate *currentDate, const RTCTime *currentTime)
{
    s64 currentTimestamp, storedTimestamp;
    s32 minutesElapsed;

    currentTimestamp = RTC_ConvertDateTimeToSecond(currentDate, currentTime);
    storedTimestamp = RTC_ConvertDateTimeToSecond(&gameTime->date, &gameTime->time);

    if (currentTimestamp < storedTimestamp) {
        gameTime->date = *currentDate;
        gameTime->time = *currentTime;
    } else {
        minutesElapsed = (currentTimestamp - storedTimestamp) / 60;

        if (minutesElapsed > 0) {
            GameTime_DecrementPenalty(gameTime, minutesElapsed);
            FieldSystem_HandleElapsedTimeEvents(fieldSystem, minutesElapsed, currentTime);

            gameTime->date = *currentDate;
            gameTime->time = *currentTime;
        }
    }
}

// Runs the once-per-day events for each day that passed while the game was off.
static void FieldSystem_HandleDailyEvents(FieldSystem *fieldSystem, s32 daysPassed)
{
    Underground_HandleDailyEvents(FieldSystem_GetSaveData(fieldSystem), daysPassed);
    FieldSystem_ClearDailyFlags(fieldSystem);
    TrainerCase_AccumulateBadgeDirt(fieldSystem->saveData, daysPassed);
    RecordMixedRNG_AdvanceEntries(SaveData_GetRecordMixedRNG(fieldSystem->saveData), daysPassed);
    SpecialEncounter_SetMixedRecordDailies(SaveData_GetSpecialEncounters(fieldSystem->saveData), RecordMixedRNG_GetRand(SaveData_GetRecordMixedRNG(fieldSystem->saveData)));

    Party *party = SaveData_GetParty(fieldSystem->saveData);
    Party_UpdatePokerusStatus(party, daysPassed);

    VarsFlags *varsFlags = SaveData_GetVarsFlags(fieldSystem->saveData);
    u16 deadlineInDays = SystemVars_GetNewsPressDeadline(varsFlags);

    if (deadlineInDays > daysPassed) {
        deadlineInDays -= daysPassed;
    } else {
        deadlineInDays = 0;
    }

    SystemVars_SetNewsPressDeadline(varsFlags, deadlineInDays);
    SystemVars_SynchronizeJubilifeLotteryTrainerID(fieldSystem->saveData, daysPassed);
    SystemVars_InitDailyRandomLevel(fieldSystem->saveData);
    SystemVars_UpdateVillaVisitor(fieldSystem->saveData);
    FieldSystem_ClearDailyHiddenItemFlags(fieldSystem);
    BattleSalon_SeedRng(fieldSystem->saveData);
    WiFiHistory_UpdateGeonetCommunicationMap(SaveData_WiFiHistory(fieldSystem->saveData));
    TVSegment_ResetDailyRecords(fieldSystem->saveData);
}

// Runs the events that advance with real time: berry patches, honey trees, the
// Underground gift penalty, TV broadcast time slots and Shaymin's form.
static void FieldSystem_HandleElapsedTimeEvents(FieldSystem *fieldSystem, s32 minutesElapsed, const RTCTime *rtcTime)
{
    BerryPatches_ElapseTime(fieldSystem, minutesElapsed);
    SpecialEncounter_DecrementHoneyTreeTimers(fieldSystem->saveData, minutesElapsed);
    Underground_ProgressGiftPenalty(fieldSystem->saveData, minutesElapsed, FieldSystem_HasPenalty(fieldSystem));

    TVBroadcast *broadcast = SaveData_GetTVBroadcast(fieldSystem->saveData);
    TVBroadcast_UpdateProgramTimeSlot(broadcast, minutesElapsed, rtcTime->minute);

    Party *party = SaveData_GetParty(fieldSystem->saveData);
    Party_SetShayminForm(party, minutesElapsed, rtcTime);
}

enum TimeOfDay FieldSystem_GetTimeOfDay(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return TimeOfDayForHour(gameTime->time.hour);
}

int FieldSystem_GetMonth(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return gameTime->date.month;
}

int FieldSystem_GetDayOfMonth(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return gameTime->date.day;
}

int FieldSystem_GetWeek(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return gameTime->date.week;
}

int FieldSystem_GetHour(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return gameTime->time.hour;
}

int FieldSystem_GetMinute(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return gameTime->time.minute;
}

void FieldSystem_GetStartTimestamp(const FieldSystem *fieldSystem, RTCDate *destDate, RTCTime *destTime)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    RTC_ConvertSecondToDateTime(destDate, destTime, gameTime->startTimestamp);
}

void FieldSystem_GetFirstCompletionTimestamp(const FieldSystem *fieldSystem, RTCDate *destDate, RTCTime *destTime)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    RTC_ConvertSecondToDateTime(destDate, destTime, gameTime->firstCompletionTimestamp);
}

void FieldSystem_RecordFirstCompletion(const FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);

    gameTime->firstCompletionTimestamp = GetTimestamp();
}

BOOL FieldSystem_HasPenalty(FieldSystem *fieldSystem)
{
    GameTime *gameTime = SaveData_GetGameTime(fieldSystem->saveData);
    return GameTime_HasPenalty(gameTime);
}
