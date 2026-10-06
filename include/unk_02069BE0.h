#ifndef POKEPLATINUM_UNK_02069BE0_H
#define POKEPLATINUM_UNK_02069BE0_H

#include "struct_decls/map_object.h"

#include "overworld_anim_manager.h"

void MapObjectMovement_FollowPlayer_Init(MapObject *param0);
void MapObjectMovement_FollowPlayer_Update(MapObject *param0);
void MapObjectMovement_FollowPlayer_Load(MapObject *param0);
void MapObjectMovement_FollowPartnerTrainer_Init(MapObject *param0);
void MapObjectMovement_FollowPartnerTrainer_Update(MapObject *param0);
void MapObjectMovement_FollowPartnerTrainer_Free(MapObject *param0);
void MapObjectMovement_FollowPartnerTrainer_Load(MapObject *param0);
MapObject *MapObjectMovement_FindPartnerTrainer(MapObject *param0);
void MapObjectMovement_DisguiseSnow_Init(MapObject *param0);
void MapObjectMovement_DisguiseSand_Init(MapObject *param0);
void MapObjectMovement_DisguiseRock_Init(MapObject *param0);
void MapObjectMovement_DisguiseGrass_Init(MapObject *param0);
void MapObjectMovement_Disguise_Update(MapObject *param0);
void MapObjectMovement_Disguise_Free(MapObject *param0);
void MapObjectMovement_Disguise_Load(MapObject *param0);
void Disguise_SetAnimManager(MapObject *param0, OverworldAnimManager *param1);
OverworldAnimManager *Disguise_GetAnimManager(MapObject *param0);
void Disguise_MarkRevealed(MapObject *param0);
void MapObjectMovement_WalkWithPlayer_Init(MapObject *param0);
void MapObjectMovement_WalkWithPlayer_Init056(MapObject *param0);
void MapObjectMovement_WalkWithPlayer_Init057(MapObject *param0);
void MapObjectMovement_WalkWithPlayer_Init058(MapObject *param0);
void MapObjectMovement_WalkWithPlayerTallGrass_Init(MapObject *param0);
void MapObjectMovement_WalkWithPlayerTallGrass_Init060(MapObject *param0);
void MapObjectMovement_WalkWithPlayerTallGrass_Init061(MapObject *param0);
void MapObjectMovement_WalkWithPlayerTallGrass_Init062(MapObject *param0);
void MapObjectMovement_WalkWithPlayer_Update(MapObject *param0);
void MapObjectMovement_WanderAvoidObstacles_Init(MapObject *param0);
void MapObjectMovement_WanderAvoidObstacles_Init064(MapObject *param0);
void MapObjectMovement_WanderAvoidObstacles_Init065(MapObject *param0);
void MapObjectMovement_WanderAvoidObstacles_Init066(MapObject *param0);
void MapObjectMovement_WanderAvoidObstacles_Update(MapObject *param0);

#endif // POKEPLATINUM_UNK_02069BE0_H
