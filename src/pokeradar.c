#include "pokeradar.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle.h"

#include "struct_defs/radar_chain_records.h"

#include "field/field_system.h"
#include "overlay005/ov5_021F2D20.h"
#include "overlay006/radar_chain_records.h"

#include "bag.h"
#include "field_bgm.h"
#include "field_task.h"
#include "gfx_box_test.h"
#include "heap.h"
#include "inlines.h"
#include "map_matrix.h"
#include "map_object.h"
#include "map_tile_behavior.h"
#include "overworld_anim_manager.h"
#include "player_avatar.h"
#include "scrcmd.h"
#include "script_manager.h"
#include "sound_playback.h"
#include "special_encounter.h"
#include "terrain_collision_manager.h"

// A single shaking grass patch spawned by the Poké Radar. Patches are arranged
// in concentric rings around the player, with at most one patch per ring.
typedef struct {
    int x; // Tile X coordinate of the patch.
    int z; // Tile Z coordinate of the patch.
    int shakeType; // PATCH_SHAKE_SOFT or PATCH_SHAKE_HARD.
    BOOL active; // Whether this ring currently holds a valid patch.
    BOOL continueChain; // Whether stepping on this patch continues the radar chain.
    BOOL shiny; // Whether this patch triggers a shiny encounter.
    OverworldAnimManager *animManager; // Animation manager for the shaking patch sprite.
    VecFx32 position; // World-space position of the patch.
} GrassPatch;

// State of the Poké Radar chain. The chain tracks consecutive encounters
// started from shaking grass patches and is used to bias the encounter table
// and the shiny odds.
typedef struct RadarChain {
    int shakeType; // Shake type the chain is currently set to.
    int count; // Number of consecutive encounters in the chain (capped at 999).
    int species; // Species of the last radar encounter.
    int level; // Level of the last radar encounter.
    BOOL active; // Whether the radar chain is currently active.
    BOOL awaitingFirstEncounter; // TRUE until the first patch encounter starts the chain.
    BOOL patchEncounter; // TRUE if the current encounter was triggered by a patch.
    GrassPatch patch[NUM_GRASS_PATCHES]; // One patch per ring, outermost first.
    GFXTestBox grassPatchVolume; // View volume used to cull patches off-screen.
    u8 recordSlot; // Chain-record slot this chain is tracking.
} RadarChain;

static BOOL CheckTileIsGrass(FieldSystem *fieldSystem, const fx32 playerY, const int playerX, const int playerZ, const u8 xOffset, const u8 zOffset, GrassPatch *patch);
static BOOL PlayerStandingInPatch(const RadarChain *chain, const int x, const int z, u8 *patchMatch);
static void TryReplaceLowestChainRecord(FieldSystem *fieldSystem, RadarChain *chain);
static u8 GetLowestChainRecordSlot(FieldSystem *fieldSystem);
static BOOL CheckPatchContinueChain(const u8 patchRing, const int battleResult);
static BOOL CheckPatchShiny(const int chainCount);
static void IncWithCap(int *count);

RadarChain *RadarChain_Init(const enum HeapID heapID)
{
    RadarChain *chain = Heap_Alloc(heapID, sizeof(RadarChain));
    GFXBoxTest_MakeBox(FX32_ONE * 16, FX32_ONE * 8, FX32_ONE * 16, &chain->grassPatchVolume);
    return chain;
}

void RadarChain_Free(RadarChain *chain)
{
    Heap_Free(chain);
}

void RadarChain_Clear(RadarChain *chain)
{
    chain->count = 0;
    chain->shakeType = PATCH_SHAKE_SOFT;
    chain->species = 0;
    chain->level = 0;
    chain->active = FALSE;
    chain->recordSlot = 0;
    chain->awaitingFirstEncounter = 1;
    chain->patchEncounter = 0;
    MI_CpuClear8(chain->patch, sizeof(GrassPatch) * NUM_GRASS_PATCHES);
    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        chain->patch[patchRing].active = FALSE;
    }
}

// Attempts to spawn one shaking patch in each ring around the player. Returns
// TRUE if at least one patch was placed; otherwise the chain is cleared and the
// radar BGM fades out.
BOOL RadarSpawnPatches(FieldSystem *fieldSystem, const int playerX, const int playerZ, RadarChain *chain)
{
    u8 v1, v2;
    u8 v3;
    u8 v4;
    int v5, v6;
    u8 patchesSpawned;
    u8 ringTileCount[NUM_GRASS_PATCHES] = { // Number of tiles in each ring of the radar. Lowest being the most outer ring
        32,
        24,
        16,
        8
    };

    const VecFx32 *playerPos = PlayerAvatar_GetPos(fieldSystem->playerAvatar);
    patchesSpawned = 0;

    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        v3 = LCRNG_RandMod(ringTileCount[patchRing]);
        v1 = 9 - (patchRing * 2);
        v2 = 9 - (patchRing * 2);
        v4 = v3 / v1;

        if (v4 == 0) {
            v5 = patchRing + v3 % v1;
            v6 = patchRing;
        } else if (v4 == 1) {
            v5 = patchRing + v3 % v1;
            v6 = patchRing + v2 - 1;
        } else {
            GF_ASSERT(v3 >= (v1 * 2));
            v4 = v3 - (v1 * 2);
            v6 = patchRing + (v4 / 2) + 1;
            if (v4 % 2 == 0) {
                v5 = patchRing;
            } else {
                v5 = patchRing + v1 - 1;
            }
        }

        BOOL spawned = CheckTileIsGrass(fieldSystem, playerPos->y, playerX, playerZ, v5, v6, &chain->patch[patchRing]);
        if (spawned) {
            patchesSpawned++;
        }
    }

    if (patchesSpawned == 0) {
        RadarChain_Clear(chain);
        FieldBGM_TryFadeOut(fieldSystem, FieldBGM_GetEffective(fieldSystem, fieldSystem->location->mapHeaderID), 1);
    } else {
        chain->active = TRUE;
    }

    return chain->active;
}

// Decides, for every active patch, whether it continues the chain and what
// shake type/shiny state it should display. `battleResult` is the result of the
// battle that spawned these patches.
void SetupGrassPatches(FieldSystem *fieldSystem, const int battleResult, RadarChain *chain)
{
    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        if (chain->patch[patchRing].active) {
            chain->patch[patchRing].continueChain = CheckPatchContinueChain(patchRing, battleResult);
            if (!chain->patch[patchRing].continueChain) {
                if (LCRNG_RandMod(100) < 50) { // If the patch will break the chain, it has a 50/50 chance of shaking the other type
                    chain->patch[patchRing].shakeType = PATCH_SHAKE_SOFT;
                } else {
                    chain->patch[patchRing].shakeType = PATCH_SHAKE_HARD;
                }
                chain->patch[patchRing].shiny = FALSE;
            } else {
                chain->patch[patchRing].shakeType = chain->shakeType; // A patch that continues the chain, shakes the type the chain is set to
                chain->patch[patchRing].shiny = CheckPatchShiny(fieldSystem->chain->count);
            }
        }
    }
}

void FieldSystem_CreateShakingRadarPatches(FieldSystem *fieldSystem, RadarChain *chain)
{
    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        if (chain->patch[patchRing].active) {
            int x = chain->patch[patchRing].x;
            int z = chain->patch[patchRing].z;
            if (chain->patch[patchRing].shiny) {
                chain->patch[patchRing].animManager = ov5_021F3154(fieldSystem, x, z, 2);
            } else {
                if (chain->patch[patchRing].shakeType == PATCH_SHAKE_SOFT) {
                    chain->patch[patchRing].animManager = ov5_021F3154(fieldSystem, x, z, 0);
                } else {
                    chain->patch[patchRing].animManager = ov5_021F3154(fieldSystem, x, z, 1);
                }
            }
        } else {
            chain->patch[patchRing].animManager = NULL;
        }
    }
}

// Returns TRUE once every patch's shake animation has finished (or has no
// animation), finishing and releasing each animation manager as it completes.
BOOL RadarChain_ArePatchesFinished(RadarChain *chain)
{
    u8 finishedPatches = 0;

    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        if (chain->patch[patchRing].animManager != NULL) {
            if (ov5_021F31A8(chain->patch[patchRing].animManager)) {
                OverworldAnimManager_Finish(chain->patch[patchRing].animManager);
                chain->patch[patchRing].animManager = NULL;
                finishedPatches++;
            }
        } else {
            finishedPatches++;
        }
    }

    if (finishedPatches >= 4) {
        return TRUE;
    }

    return FALSE;
}

// Called when the player may have stepped onto a shaking patch. If so, it
// updates the chain (incrementing the count and preserving the chain when the
// patch continues it) and reports the shake type, whether the chain is
// preserved, and whether the encounter is shiny.
BOOL PokeRadar_ShouldDoRadarEncounter(const int playerX, const int playerZ, FieldSystem *fieldSystem, RadarChain *chain, int *shake, BOOL *preserveChain, BOOL *isShiny)
{
    u8 patchRing;
    *preserveChain = 0;
    *isShiny = 0;

    if (!PlayerStandingInPatch(chain, playerX, playerZ, &patchRing)) {
        return FALSE;
    }

    chain->patchEncounter = 1;
    BOOL continueChain = chain->patch[patchRing].continueChain;
    int shakeType = chain->patch[patchRing].shakeType;

    if (chain->awaitingFirstEncounter == 0) {
        if (continueChain) {
            IncWithCap(&(chain->count));
            *shake = shakeType;
            *preserveChain = 1;
            TryReplaceLowestChainRecord(fieldSystem, chain);
            *isShiny = chain->patch[patchRing].shiny;
            return TRUE;
        } else {
            *shake = shakeType;
        }
    } else {
        *shake = shakeType;
        chain->awaitingFirstEncounter = 0;
        chain->recordSlot = GetLowestChainRecordSlot(fieldSystem);
    }

    chain->shakeType = *shake;
    return TRUE;
}

void SetRadarMon(RadarChain *chain, const int species, const int level)
{
    GF_ASSERT(species != 0);
    chain->species = species;
    chain->level = level;
}

void GetRadarMon(RadarChain *chain, int *species, int *level)
{
    *species = chain->species;
    *level = chain->level;
}

// Returns TRUE if the current encounter was triggered by a shaking patch.
const BOOL RadarChain_IsPatchEncounter(const RadarChain *chain)
{
    return chain->patchEncounter;
}

void PokeRadar_ClearIfAllOutOfView(FieldSystem *fieldSystem)
{
    BOOL patchInView;
    GrassPatch *patch;
    int patchRing;

    if (!fieldSystem->chain->active || fieldSystem->task != NULL) {
        return;
    }

    for (patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        patch = &(fieldSystem->chain->patch[patchRing]);
        patchInView = GFXBoxTest_IsBoxAtPositionInView(&patch->position, &fieldSystem->chain->grassPatchVolume);
        if (patch->active && !patchInView) {
            patch->active = FALSE;
        }
    }

    int inactiveRadarRings = 0;
    for (patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        patch = &(fieldSystem->chain->patch[patchRing]);
        if (patch->active == 0) {
            inactiveRadarRings++;
        }
    }

    if (inactiveRadarRings == 4) {
        RadarChain_Clear(fieldSystem->chain);
        FieldBGM_TryFadeOut(fieldSystem, FieldBGM_GetEffective(fieldSystem, fieldSystem->location->mapHeaderID), 1);
    }
}

BOOL GetRadarChainActive(const RadarChain *chain)
{
    return chain->active;
}

// Tests whether the tile at the given ring offset is tall grass on the same map
// and at the same height as the player, and if so records it as a patch.
static BOOL CheckTileIsGrass(FieldSystem *fieldSystem, const fx32 playerY, const int playerX, const int playerZ, const u8 xOffset, const u8 zOffset, GrassPatch *patch)
{
    int x = (playerX - (9 / 2)) + xOffset;
    int z = (playerZ - (9 / 2)) + zOffset;
    patch->x = x;
    patch->z = z;
    u8 tileBehavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

    if (TileBehavior_IsTallGrass(tileBehavior)) {
        u8 height;
        patch->position.x = FX32_ONE * 16 * x;
        patch->position.z = FX32_ONE * 16 * z;
        patch->position.y = TerrainCollisionManager_GetHeight(fieldSystem, 0, patch->position.x, patch->position.z, &height);

        if (playerY != patch->position.y) {
            patch->active = FALSE;
            return FALSE;
        }

        int mapX = x / 32;
        int mapZ = z / 32;
        int mapHeaderID = MapMatrix_GetMapHeaderIDAtCoords(fieldSystem->mapMatrix, mapX, mapZ);
        if (fieldSystem->location->mapHeaderID != mapHeaderID) {
            patch->active = FALSE;
            return FALSE;
        }
        patch->active = TRUE;
        return TRUE;
    } else {
        patch->active = FALSE;
        return FALSE;
    }
}

// Checks if the player is standing in any of the shaking patches.
static BOOL PlayerStandingInPatch(const RadarChain *chain, const int x, const int z, u8 *patchMatch)
{
    for (u8 patchRing = 0; patchRing < NUM_GRASS_PATCHES; patchRing++) {
        if (chain->patch[patchRing].active) {
            if ((chain->patch[patchRing].x == x) && (chain->patch[patchRing].z == z)) {
                *patchMatch = patchRing;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void TryReplaceLowestChainRecord(FieldSystem *fieldSystem, RadarChain *chain)
{
    RadarChainRecords *chainRecordData = SpecialEncounter_GetRadarChainRecords(SaveData_GetSpecialEncounters(fieldSystem->saveData));
    int lowestRecord = chainRecordData->records[chain->recordSlot].chainCount;

    if (lowestRecord < chain->count) {
        chainRecordData->records[chain->recordSlot].chainCount = chain->count;
        chainRecordData->records[chain->recordSlot].species = chain->species;
        RadarChainRecords_SortSavedRecords(chainRecordData);
        if (chainRecordData->records[chain->recordSlot].chainCount <= chain->count) {
            for (int i = 0; i < NUM_RADAR_RECORDS; i++) {
                if (chainRecordData->records[((NUM_RADAR_RECORDS - 1) - i)].chainCount == chain->count) {
                    chain->recordSlot = ((NUM_RADAR_RECORDS - 1) - i);
                    return;
                }
            }
            GF_ASSERT(FALSE);
        }
    }
}

// Returns the index of the record with the lowest chain, or the first empty slot if there is one.
static u8 GetLowestChainRecordSlot(FieldSystem *fieldSystem)
{
    u8 slotToReplace;
    BOOL lowerChain;
    RadarChainRecords *recordsData = SpecialEncounter_GetRadarChainRecords(SaveData_GetSpecialEncounters(fieldSystem->saveData));

    for (slotToReplace = 0; slotToReplace < NUM_RADAR_RECORDS; slotToReplace++) {
        if (recordsData->records[slotToReplace].species == 0) {
            return slotToReplace;
        }
    }

    lowerChain = recordsData->records[0].chainCount < recordsData->records[1].chainCount ? 1 : 0;
    if (lowerChain) {
        slotToReplace = 0;
    } else {
        slotToReplace = 1;
    }

    lowerChain = recordsData->records[slotToReplace].chainCount < recordsData->records[2].chainCount ? 1 : 0;
    if (!lowerChain) {
        slotToReplace = 2;
    }

    return slotToReplace;
}

// Rolls whether a patch continues the chain. The chance depends on the ring
// (outer rings are more likely) and is boosted when the previous battle ended
// in a capture rather than a faint.
static BOOL CheckPatchContinueChain(const u8 patchRing, const int battleResult)
{
    u8 *rates;
    u8 ratesNormal[4] = { 88, 68, 48, 28 };
    u8 ratesBoosted[4] = { 98, 78, 58, 38 };

    if (battleResult == BATTLE_RESULT_WIN) { // If the battle resulted in fainting the mon, use the regular rates
        rates = ratesNormal;
    } else if (battleResult == BATTLE_RESULT_CAPTURED_MON) { // If the battle resulted in a capture, use the boosted rates
        rates = ratesBoosted;
    }

    if (LCRNG_RandMod(100) < rates[patchRing]) { // Check if random number falls within the rates
        return TRUE; // Patch will continue the chain
    } else {
        return FALSE; // Patch will break the chain
    }
}

BOOL RefreshRadarChain(FieldTask *taskMan)
{
    FieldSystem *fieldSystem = FieldTask_GetFieldSystem(taskMan);
    int *state = FieldTask_GetEnv(taskMan);

    switch (*state) {
    case 0:
        MapObjectMan_PauseAllMovement(fieldSystem->mapObjMan);
        u8 *radarCharge = SpecialEncounter_GetRadarCharge(SaveData_GetSpecialEncounters(fieldSystem->saveData));

        if (*radarCharge < RADAR_BATTERY_STEPS) {
            ScriptManager_Start(taskMan, SCRIPT_ID(POKE_RADAR, 0), NULL, NULL);
            *(u16 *)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_DATA_PARAMETER_0) = RADAR_BATTERY_STEPS - (*radarCharge);
            *state = 4;
        } else {
            *radarCharge = 0;
            int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
            int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
            RadarSpawnPatches(fieldSystem, playerX, playerZ, fieldSystem->chain);
            if (fieldSystem->chain->active) {
                SetupGrassPatches(fieldSystem, 0x1, fieldSystem->chain);
                FieldSystem_CreateShakingRadarPatches(fieldSystem, fieldSystem->chain);
                *state = 1;
            } else {
                *state = 3;
            }
        }
        break;
    case 1:
        Sound_PlayBGM(SEQ_KUSAGASA_sseq);
        *state = 2;
        break;
    case 2:
        if (RadarChain_ArePatchesFinished(fieldSystem->chain)) {
            *state = 4;
        }
        break;
    case 4:
        Heap_Free(state);
        MapObjectMan_UnpauseAllMovement(fieldSystem->mapObjMan);
        return TRUE;
        break;
    case 3:
        ScriptManager_Start(taskMan, SCRIPT_ID(POKE_RADAR, 1), NULL, NULL);
        *state = 4;
        break;
    }

    return FALSE;
}

// Rolls whether a patch is shiny. The rate starts at 1/8200 and improves by
// 200 per chain step, down to a floor of 1/200.
static BOOL CheckPatchShiny(const int chainCount)
{
    if (!chainCount) {
        return FALSE;
    }

    int rate = 8200 - (chainCount * 200);
    if (rate < 200) {
        rate = 200;
    }

    if (!LCRNG_RandMod(rate)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void RadarChain_Increment(FieldSystem *fieldSystem)
{
    IncWithCap(&(fieldSystem->chain->count));
    TryReplaceLowestChainRecord(fieldSystem, fieldSystem->chain);
}

int GetChainCount(FieldSystem *fieldSystem)
{
    return fieldSystem->chain->count;
}

void RadarChargeStep(FieldSystem *fieldSystem)
{
    u8 *radarCharge;

    if (Bag_CanRemoveItem(SaveData_GetBag(fieldSystem->saveData), ITEM_POKE_RADAR, 1, HEAP_ID_FIELD1) == TRUE) {
        radarCharge = SpecialEncounter_GetRadarCharge(SaveData_GetSpecialEncounters(fieldSystem->saveData));
        if ((*radarCharge) < RADAR_BATTERY_STEPS) {
            (*radarCharge)++;
        }
    }
}

static void IncWithCap(int *count)
{
    (*count)++;
    if ((*count) > 999) {
        (*count) = 999;
    }
}
