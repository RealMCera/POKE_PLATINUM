#include "tv_broadcast.h"

#include <nitro.h>
#include <string.h>

#include "constants/tv_broadcast.h"

#include "struct_defs/struct_0202E7E4.h"
#include "struct_defs/struct_0202E7F0.h"
#include "struct_defs/struct_0202E7FC.h"
#include "struct_defs/struct_0202E808.h"
#include "struct_defs/struct_0202E810.h"
#include "struct_defs/struct_0202E81C.h"
#include "struct_defs/struct_0202E828.h"
#include "struct_defs/struct_0202E834.h"
#include "struct_defs/tv.h"
#include "struct_defs/tv_segment_contest_hall_showcased_pokemon.h"

#include "inlines.h"
#include "rtc.h"
#include "savedata.h"

// Segment IDs with bit 7 set refer to a Wi-Fi episode rather than a locally
// recorded segment (see TVBroadcast_IsLocalSegment).
#define TV_SEGMENT_WIFI_FLAG 0x80

static void TVBroadcast_ClearPlayedSegments(TVBroadcast *broadcast);
static TVSegmentInstance *TVBroadcast_GetSegmentInstances(TVBroadcast *broadcast, int programType);

int TVBroadcast_SaveSize(void)
{
    return sizeof(TVBroadcast);
}

void TVBroadcast_Init(TVBroadcast *broadcast)
{
    MI_CpuClearFast(broadcast, sizeof(TVBroadcast));
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Discards every locally recorded segment and resets the watch progress.
void TVBroadcast_ClearSegmentInstances(TVBroadcast *broadcast)
{
    MI_CpuClearFast(broadcast->trainerSightingSegments, sizeof(TVSegmentInstance) * 4);
    MI_CpuClearFast(broadcast->recordSegments, sizeof(TVSegmentInstance) * 4);
    MI_CpuClearFast(broadcast->interviewSegments, sizeof(TVSegmentInstance) * 4);

    TVBroadcast_ClearWatchProgress(broadcast);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Advances the current program's time slot. When the slot runs out, the next
// slot begins on the next 15-minute boundary and the played-segment list is
// cleared so the same segments can air again.
void TVBroadcast_UpdateProgramTimeSlot(TVBroadcast *broadcast, int deltaMinutes, int currentMinute)
{
    int i;

    if (broadcast->timeSlotMinutesRemaining > deltaMinutes) {
        broadcast->timeSlotMinutesRemaining -= deltaMinutes;
    } else {
        broadcast->timeSlotMinutesRemaining = 15 - currentMinute % 15;

        if (broadcast->timeSlotMinutesRemaining == 0) {
            broadcast->timeSlotMinutesRemaining = 15;
        }

        broadcast->programFinished = FALSE;

        for (i = 0; i < TV_BROADCAST_MAX_PLAYED_SEGMENTS; i++) {
            broadcast->playedSegments[i] = 0;
        }
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_ClearWatchProgress(TVBroadcast *broadcast)
{
    TVBroadcast_SetProgramFinished(broadcast, FALSE);
    TVBroadcast_ClearPlayedSegments(broadcast);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void TVBroadcast_SetProgramFinished(TVBroadcast *broadcast, BOOL finished)
{
    broadcast->programFinished = finished;
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

BOOL TVBroadcast_IsProgramFinished(const TVBroadcast *broadcast)
{
    return broadcast->programFinished;
}

// Marks a segment as played in the first free slot of the played-segment list.
void TVBroadcast_SetPlayedSegment(TVBroadcast *broadcast, int segmentID)
{
    int i;

    for (i = 0; i < TV_BROADCAST_MAX_PLAYED_SEGMENTS; i++) {
        if (broadcast->playedSegments[i] == NULL) {
            broadcast->playedSegments[i] = segmentID;
            SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
            return;
        }
    }

    GF_ASSERT(FALSE);

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

static void TVBroadcast_ClearPlayedSegments(TVBroadcast *broadcast)
{
    int i;

    for (i = 0; i < TV_BROADCAST_MAX_PLAYED_SEGMENTS; i++) {
        broadcast->playedSegments[i] = 0;
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

BOOL TVBroadcast_IsPlayedSegment(const TVBroadcast *broadcast, int segmentID)
{
    int i;

    for (i = 0; i < TV_BROADCAST_MAX_PLAYED_SEGMENTS; i++) {
        if (broadcast->playedSegments[i] == segmentID) {
            return TRUE;
        }
    }

    return FALSE;
}

int TVBroadcast_CountPlayedSegments(const TVBroadcast *broadcast)
{
    int i, playedCount;

    for (i = 0, playedCount = 0; i < TV_BROADCAST_MAX_PLAYED_SEGMENTS; i++) {
        if (broadcast->playedSegments[i] != 0) {
            playedCount++;
        }
    }

    return playedCount;
}

// Overwrites a segment instance with a freshly recorded segment, stamping it
// with the current date and resetting its play count.
static void TVBroadcast_InitSegmentInstance(TVSegmentInstance *instance, int segmentID, const u8 *segmentData)
{
    RTCDate time;
    GetCurrentDate(&time);

    instance->timestamp = Date_Encode(&time);
    instance->segmentID = segmentID;
    instance->timesPlayed = 0;

    MI_CpuCopyFast(segmentData, instance->segment, 40);
    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

// Stores a recorded segment in the local segment array for the given program
// type. An existing entry is only overwritten once it has aired three times;
// otherwise an empty slot, then a fully-aired slot, is reused. Returns TRUE if
// the segment was stored.
BOOL TVBroadcast_SaveSegmentData(TVBroadcast *broadcast, int programType, int segmentID, const u8 *segmentData)
{
    int i;
    TVSegmentInstance *instances = TVBroadcast_GetSegmentInstances(broadcast, programType);

    for (i = 0; i < 4; i++) {
        if (instances[i].segmentID == segmentID) {
            if (instances[i].timesPlayed >= 3) {
                TVBroadcast_InitSegmentInstance(&instances[i], segmentID, segmentData);
                return 1;
            }

            return 0;
        }
    }

    for (i = 0; i < 4; i++) {
        if (instances[i].segmentID == 0) {
            TVBroadcast_InitSegmentInstance(&instances[i], segmentID, segmentData);
            return 1;
        }
    }

    for (i = 0; i < 4; i++) {
        if (instances[i].timesPlayed >= 3) {
            TVBroadcast_InitSegmentInstance(&instances[i], segmentID, segmentData);
            return 1;
        }
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);

    return 0;
}

const u16 *TVWifiEpisode_GetTrainerName(const TVWifiEpisode *episode)
{
    return episode->name;
}

int TVWifiEpisode_GetLanguage(const TVWifiEpisode *episode)
{
    return episode->language;
}

int TVWifiEpisode_GetGameVersion(const TVWifiEpisode *episode)
{
    return episode->gameVersion;
}

TVSegmentInstance *TVWifiEpisode_GetDetails(TVWifiEpisode *episode)
{
    return &episode->details;
}

TVSegmentInstance *TVSegmentInstance_GetDetails(TVSegmentInstance *instance)
{
    return instance;
}

// Returns the local segment array for a program type.
static TVSegmentInstance *TVBroadcast_GetSegmentInstances(TVBroadcast *broadcast, int programType)
{
    TVSegmentInstance *instances = NULL;

    switch (programType) {
    case TV_PROGRAM_TYPE_INTERVIEWS:
        instances = broadcast->interviewSegments;
        break;
    case TV_PROGRAM_TYPE_TRAINER_SIGHTINGS:
        instances = broadcast->trainerSightingSegments;
        break;
    case TV_PROGRAM_TYPE_RECORDS:
        instances = broadcast->recordSegments;
        break;
    case TV_PROGRAM_TYPE_SINNOH_NOW:
    case TV_PROGRAM_TYPE_VARIETY_HOUR:
        GF_ASSERT(FALSE);
    }

    return instances;
}

// Returns the Wi-Fi episode array for a program type.
static TVWifiEpisode *TVBroadcast_GetWifiEpisodes(TVBroadcast *broadcast, int programType)
{
    TVWifiEpisode *episodes = NULL;

    switch (programType) {
    case TV_PROGRAM_TYPE_TRAINER_SIGHTINGS:
        episodes = broadcast->trainerSightingWifiEpisodes;
        break;
    case TV_PROGRAM_TYPE_RECORDS:
        episodes = broadcast->recordWifiEpisodes;
        break;
    case TV_PROGRAM_TYPE_INTERVIEWS:
        episodes = broadcast->interviewWifiEpisodes;
        break;
    case TV_PROGRAM_TYPE_SINNOH_NOW:
    case TV_PROGRAM_TYPE_VARIETY_HOUR:
        GF_ASSERT(FALSE);
    }

    return episodes;
}

int TVSegmentInstance_GetSegmentID(const TVSegmentInstance *instance)
{
    return instance->segmentID;
}

void TVSegmentInstance_IncrementTimesPlayed(TVSegmentInstance *instance)
{
    if (instance->timesPlayed < 3) {
        instance->timesPlayed++;
    }

    SaveData_SetChecksum(SAVE_TABLE_ENTRY_TV_BROADCAST);
}

void *TVSegmentInstance_GetSegmentData(TVSegmentInstance *instance)
{
    return instance->segment;
}

// Collects the 1-based indices of local segments matching segmentID. When
// playedOnly is set, only segments that have already aired are collected;
// otherwise only segments that have never aired are. Returns the count.
static int TVBroadcast_CollectLocalSegmentIndices(const TVSegmentInstance *instances, int count, int segmentID, BOOL playedOnly, u8 *indices)
{
    int i, indexCount;

    for (indexCount = 0, i = 0; i < count; i++) {
        if (instances[i].segmentID == segmentID) {
            int timesPlayed = instances[i].timesPlayed;

            if (playedOnly && timesPlayed) {
                indices[indexCount] = i + 1;
                indexCount++;
            } else if (!playedOnly && (timesPlayed == 0)) {
                indices[indexCount] = i + 1;
                indexCount++;
            }
        }
    }

    return indexCount;
}

// As TVBroadcast_CollectLocalSegmentIndices, but for Wi-Fi episodes. The
// collected indices have the Wi-Fi flag set.
static int TVBroadcast_CollectWifiEpisodeIndices(const TVWifiEpisode *episodes, int count, int segmentID, BOOL playedOnly, u8 *indices)
{
    int i, indexCount;

    for (indexCount = 0, i = 0; i < count; i++) {
        int timesPlayed = episodes[i].details.timesPlayed;

        if (episodes[i].details.segmentID == segmentID) {
            if (playedOnly && timesPlayed) {
                indices[indexCount] = TVBroadcast_MarkAsWifiSegment(i + 1);
                indexCount++;
            } else if (!playedOnly && (timesPlayed == 0)) {
                indices[indexCount] = TVBroadcast_MarkAsWifiSegment(i + 1);
                indexCount++;
            }
        }
    }

    return indexCount;
}

// Collects the indices of segments matching segmentID for a program type,
// choosing between local segments (local == TRUE) and Wi-Fi episodes. Returns
// the number of indices written to indices.
int TVBroadcast_CollectPendingSegments(const TVBroadcast *broadcast, int programType, int segmentID, BOOL local, BOOL playedOnly, u8 *indices)
{
    switch (programType) {
    case TV_PROGRAM_TYPE_TRAINER_SIGHTINGS:
        if (local) {
            return TVBroadcast_CollectLocalSegmentIndices(broadcast->trainerSightingSegments, 4, segmentID, playedOnly, indices);
        } else {
            return TVBroadcast_CollectWifiEpisodeIndices(broadcast->trainerSightingWifiEpisodes, 16, segmentID, playedOnly, indices);
        }
    case TV_PROGRAM_TYPE_RECORDS:
        if (local) {
            return TVBroadcast_CollectLocalSegmentIndices(broadcast->recordSegments, 4, segmentID, playedOnly, indices);
        } else {
            return TVBroadcast_CollectWifiEpisodeIndices(broadcast->recordWifiEpisodes, 8, segmentID, playedOnly, indices);
        }
    case TV_PROGRAM_TYPE_INTERVIEWS:
        if (local) {
            return TVBroadcast_CollectLocalSegmentIndices(broadcast->interviewSegments, 4, segmentID, playedOnly, indices);
        } else {
            return TVBroadcast_CollectWifiEpisodeIndices(broadcast->interviewWifiEpisodes, 8, segmentID, playedOnly, indices);
        }
    case TV_PROGRAM_TYPE_SINNOH_NOW:
        return 0;
    case TV_PROGRAM_TYPE_VARIETY_HOUR:
        return 0;
    }

    return 0;
}

// Returns TRUE if a segment with segmentID can be stored for the program type:
// either it has already aired three times, or the array has a free or
// fully-aired slot. A segment that is pending but has not aired yet blocks a
// new recording.
BOOL TVBroadcast_CanSaveSegment(TVBroadcast *broadcast, int programType, int segmentID)
{
    int count;
    int i;
    u8 indices[4];
    TVSegmentInstance *instance;

    MI_CpuClear8(indices, 4);

    count = TVBroadcast_CollectPendingSegments(broadcast, programType, segmentID, 1, 0, indices);

    if (count != 0) {
        return 0;
    }

    count = TVBroadcast_CollectPendingSegments(broadcast, programType, segmentID, 1, 1, indices);

    if (count != 0) {
        for (i = 0; i < 4 & indices[i] != 0; i++) {
            instance = TVBroadcast_GetSegmentInstance(broadcast, programType, indices[i]);

            if (instance->timesPlayed >= 3) {
                return 1;
            }
        }

        return 0;
    }

    for (i = 1; i <= 4; i++) {
        instance = TVBroadcast_GetSegmentInstance(broadcast, programType, i);

        if ((instance->segmentID == 0) || (instance->timesPlayed >= 3)) {
            return 1;
        }
    }

    return 0;
}

// Returns the local segment instance for a 1-based segment index.
TVSegmentInstance *TVBroadcast_GetSegmentInstance(TVBroadcast *broadcast, int programType, int segmentID)
{
    TVSegmentInstance *instances;

    GF_ASSERT(TVBroadcast_IsLocalSegment(segmentID) == 1);

    segmentID &= ~TV_SEGMENT_WIFI_FLAG;
    instances = TVBroadcast_GetSegmentInstances(broadcast, programType);

    return &instances[segmentID - 1];
}

// Returns the Wi-Fi episode for a 1-based segment index.
TVWifiEpisode *TVBroadcast_GetWifiEpisode(TVBroadcast *broadcast, int programType, int segmentID)
{
    TVWifiEpisode *episodes;

    GF_ASSERT(TVBroadcast_IsLocalSegment(segmentID) == 0);

    segmentID &= ~TV_SEGMENT_WIFI_FLAG;
    episodes = TVBroadcast_GetWifiEpisodes(broadcast, programType);

    return &episodes[segmentID - 1];
}

// Returns TRUE if segmentID refers to a locally recorded segment rather than a
// Wi-Fi episode.
BOOL TVBroadcast_IsLocalSegment(int segmentID)
{
    if (segmentID & TV_SEGMENT_WIFI_FLAG) {
        return 0;
    } else {
        return 1;
    }
}

// Sets the Wi-Fi flag on a 1-based segment index.
int TVBroadcast_MarkAsWifiSegment(int segmentID)
{
    return segmentID | TV_SEGMENT_WIFI_FLAG;
}

TVSegment_ContestHall_ShowcasedPokemon *TVBroadcast_GetShowcasedPokemon(TVBroadcast *broadcast)
{
    return &broadcast->showcasedPokemon;
}

TVSegment_AmitySquareWatchData *TVBroadcast_GetAmitySquareWatch(TVBroadcast *broadcast)
{
    return &broadcast->amitySquareWatch;
}

TVSegment_ThreeCheersForPoffinCornerData *TVBroadcast_GetPoffinCorner(TVBroadcast *broadcast)
{
    return &broadcast->poffinCorner;
}

TVSegment_BattleTowerCornerData *TVBroadcast_GetBattleTowerCorner(TVBroadcast *broadcast)
{
    return &broadcast->battleTowerCorner;
}

TVSegment_SafariGameData *TVBroadcast_GetSafariGameData(TVBroadcast *broadcast)
{
    return &broadcast->safariGame;
}

TVSegment_BattleFrontierFrontlineNewsSingleData *TVBroadcast_GetFrontlineNewsSingle(TVBroadcast *broadcast)
{
    return &broadcast->frontlineNewsSingle;
}

TVSegment_BattleFrontierFrontlineNewsMultiData *TVBroadcast_GetFrontlineNewsMulti(TVBroadcast *broadcast)
{
    return &broadcast->frontlineNewsMulti;
}

TVSegment_BattlePointsRecordData *TVBroadcast_GetBattlePointsRecord(TVBroadcast *broadcast)
{
    return &broadcast->battlePointsRecord;
}

TVSegment_GTSTradeRecordData *TVBroadcast_GetGTSTradeRecord(TVBroadcast *broadcast)
{
    return &broadcast->gtsTradeRecord;
}
