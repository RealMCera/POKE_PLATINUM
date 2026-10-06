#ifndef POKEPLATINUM_TV_BROADCAST_H
#define POKEPLATINUM_TV_BROADCAST_H

#include "struct_decls/tv_broadcast.h"
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

int TVBroadcast_SaveSize(void);
void TVBroadcast_Init(TVBroadcast *broadcast);
void TVBroadcast_ClearSegmentInstances(TVBroadcast *broadcast);
void TVBroadcast_UpdateProgramTimeSlot(TVBroadcast *broadcast, int deltaMinutes, int currentMinute);
void TVBroadcast_ClearWatchProgress(TVBroadcast *broadcast);
void TVBroadcast_SetProgramFinished(TVBroadcast *broadcast, BOOL finished);
BOOL TVBroadcast_IsProgramFinished(const TVBroadcast *broadcast);
void TVBroadcast_SetPlayedSegment(TVBroadcast *broadcast, int segmentID);
BOOL TVBroadcast_IsPlayedSegment(const TVBroadcast *broadcast, int segmentID);
int TVBroadcast_CountPlayedSegments(const TVBroadcast *broadcast);
BOOL TVBroadcast_SaveSegmentData(TVBroadcast *broadcast, int programType, int segmentID, const u8 *segmentData);
const u16 *TVWifiEpisode_GetTrainerName(const TVWifiEpisode *episode);
int TVWifiEpisode_GetLanguage(const TVWifiEpisode *episode);
int TVWifiEpisode_GetGameVersion(const TVWifiEpisode *episode);
TVSegmentInstance *TVWifiEpisode_GetDetails(TVWifiEpisode *episode);
TVSegmentInstance *TVSegmentInstance_GetDetails(TVSegmentInstance *instance);
int TVSegmentInstance_GetSegmentID(const TVSegmentInstance *instance);
void TVSegmentInstance_IncrementTimesPlayed(TVSegmentInstance *instance);
void *TVSegmentInstance_GetSegmentData(TVSegmentInstance *instance);
int TVBroadcast_CollectPendingSegments(const TVBroadcast *broadcast, int programType, int segmentID, BOOL local, BOOL playedOnly, u8 *indices);
BOOL TVBroadcast_CanSaveSegment(TVBroadcast *broadcast, int programType, int segmentID);
TVSegmentInstance *TVBroadcast_GetSegmentInstance(TVBroadcast *broadcast, int programType, int segmentID);
TVWifiEpisode *TVBroadcast_GetWifiEpisode(TVBroadcast *broadcast, int programType, int segmentID);
BOOL TVBroadcast_IsLocalSegment(int segmentID);
int TVBroadcast_MarkAsWifiSegment(int segmentID);
TVSegment_ContestHall_ShowcasedPokemon *TVBroadcast_GetShowcasedPokemon(TVBroadcast *broadcast);
TVSegment_AmitySquareWatchData *TVBroadcast_GetAmitySquareWatch(TVBroadcast *broadcast);
TVSegment_ThreeCheersForPoffinCornerData *TVBroadcast_GetPoffinCorner(TVBroadcast *broadcast);
TVSegment_BattleTowerCornerData *TVBroadcast_GetBattleTowerCorner(TVBroadcast *broadcast);
TVSegment_SafariGameData *TVBroadcast_GetSafariGameData(TVBroadcast *broadcast);
TVSegment_BattleFrontierFrontlineNewsSingleData *TVBroadcast_GetFrontlineNewsSingle(TVBroadcast *broadcast);
TVSegment_BattleFrontierFrontlineNewsMultiData *TVBroadcast_GetFrontlineNewsMulti(TVBroadcast *broadcast);
TVSegment_BattlePointsRecordData *TVBroadcast_GetBattlePointsRecord(TVBroadcast *broadcast);
TVSegment_GTSTradeRecordData *TVBroadcast_GetGTSTradeRecord(TVBroadcast *broadcast);

#endif // POKEPLATINUM_TV_BROADCAST_H
