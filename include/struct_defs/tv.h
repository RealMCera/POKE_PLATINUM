#ifndef POKEPLATINUM_STRUCT_DEF_TV_H
#define POKEPLATINUM_STRUCT_DEF_TV_H

#include "constants/tv_broadcast.h"

#include "struct_defs/struct_0202E7E4.h"
#include "struct_defs/struct_0202E7F0.h"
#include "struct_defs/struct_0202E7FC.h"
#include "struct_defs/struct_0202E808.h"
#include "struct_defs/struct_0202E810.h"
#include "struct_defs/struct_0202E81C.h"
#include "struct_defs/struct_0202E828.h"
#include "struct_defs/struct_0202E834.h"
#include "struct_defs/tv_segment_contest_hall_showcased_pokemon.h"

#define TV_BROADCAST_MAX_PLAYED_SEGMENTS 4

typedef struct TVSegmentInstance {
    u8 segmentID;
    u8 timesPlayed;
    u32 timestamp;
    u8 segment[40];
} TVSegmentInstance;

typedef struct TVEpisode {
    u8 segmentID;
    u8 gameVersion;
    u8 language;
    u8 gender;
    u16 name[8];
    TVSegmentInstance *details;
} TVEpisode;

typedef struct TVWifiEpisode {
    u32 trainerID;
    u8 gender;
    u8 gameVersion;
    u8 language;
    u16 name[8];
    TVSegmentInstance details;
} TVWifiEpisode;

typedef struct TVBroadcast {
    u8 playedSegments[TV_BROADCAST_MAX_PLAYED_SEGMENTS];
    int timeSlotMinutesRemaining;
    u8 programFinished;
    // Locally recorded segments, one array per program type (see
    // TVBroadcast_GetSegmentInstances).
    TVSegmentInstance trainerSightingSegments[4];
    TVSegmentInstance recordSegments[4];
    TVSegmentInstance interviewSegments[4];
    // Episodes received from other players over Wi-Fi, one array per program
    // type (see TVBroadcast_GetWifiEpisodes).
    TVWifiEpisode trainerSightingWifiEpisodes[16];
    TVWifiEpisode recordWifiEpisodes[8];
    TVWifiEpisode interviewWifiEpisodes[8];
    TVSegment_ContestHall_ShowcasedPokemon showcasedPokemon;
    // Pending segment data recorded by the player, consumed when the matching
    // TV segment is saved.
    TVSegment_AmitySquareWatchData amitySquareWatch;
    TVSegment_ThreeCheersForPoffinCornerData poffinCorner;
    TVSegment_BattleTowerCornerData battleTowerCorner;
    TVSegment_SafariGameData safariGame;
    TVSegment_BattleFrontierFrontlineNewsSingleData frontlineNewsSingle;
    TVSegment_BattleFrontierFrontlineNewsMultiData frontlineNewsMulti;
    TVSegment_BattlePointsRecordData battlePointsRecord;
    TVSegment_GTSTradeRecordData gtsTradeRecord;
} TVBroadcast;

#endif // POKEPLATINUM_STRUCT_DEF_TV_H
