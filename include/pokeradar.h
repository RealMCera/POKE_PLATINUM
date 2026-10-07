#ifndef POKEPLATINUM_POKERADAR_H
#define POKEPLATINUM_POKERADAR_H

#include "field/field_system_decl.h"

#include "field_task.h"

typedef struct RadarChain RadarChain;

#define NUM_GRASS_PATCHES   4
#define RADAR_BATTERY_STEPS 50

enum PatchShakeType {
    PATCH_SHAKE_SOFT = 0,
    PATCH_SHAKE_HARD = 1
};

RadarChain *RadarChain_Init(const enum HeapID heapID);
void RadarChain_Free(RadarChain *chain);
void RadarChain_Clear(RadarChain *chain);
BOOL RadarSpawnPatches(FieldSystem *fieldSystem, const int playerX, const int playerZ, RadarChain *chain);
void SetupGrassPatches(FieldSystem *fieldSystem, const int battleResult, RadarChain *chain);
void FieldSystem_CreateShakingRadarPatches(FieldSystem *fieldSystem, RadarChain *chain);
BOOL RadarChain_ArePatchesFinished(RadarChain *chain);
int PokeRadar_ShouldDoRadarEncounter(const int playerX, const int playerZ, FieldSystem *fieldSystem, RadarChain *chain, int *shake, BOOL *preserveChain, BOOL *isShiny);
void SetRadarMon(RadarChain *chain, const int species, const int level);
void GetRadarMon(RadarChain *chain, int *species, int *level);
const BOOL RadarChain_IsPatchEncounter(const RadarChain *chain);
void PokeRadar_ClearIfAllOutOfView(FieldSystem *fieldSystem);
BOOL GetRadarChainActive(const RadarChain *chain);
BOOL RefreshRadarChain(FieldTask *taskMan);
void RadarChain_Increment(FieldSystem *fieldSystem);
int GetChainCount(FieldSystem *fieldSystem);
void RadarChargeStep(FieldSystem *fieldSystem);

#endif // POKEPLATINUM_POKERADAR_H
