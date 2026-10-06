#ifndef POKEPLATINUM_UNION_ROOM_H
#define POKEPLATINUM_UNION_ROOM_H

#include <nitro/wm.h>

#include "struct_decls/struct_0205B43C_decl.h"

#include "field/field_system_decl.h"

#include "easy_chat_sentence.h"
#include "easy_chat_words.h"
#include "string_template.h"
#include "sys_task_manager.h"
#include "trainer_info.h"

UnionRoom *FieldSystem_InitCommUnionRoom(FieldSystem *fieldSystem);
void UnionRoom_Exit(FieldSystem *fieldSystem);
void UnionRoom_Update(SysTask *param0, void *param1);
FieldSystem *UnionRoom_GetFieldSystem(UnionRoom *param0);
WMBssDesc *UnionRoom_GetBssDesc(UnionRoom *param0, int param1);
int UnionRoom_GetTrainerStatus(UnionRoom *param0, int param1);
int UnionRoom_RequestActivity(UnionRoom *param0, int param1, u16 param2);
u32 UnionRoom_GetConnectState(UnionRoom *param0);
u32 UnionRoom_GetPeerActivity(UnionRoom *param0);
u32 UnionRoom_GetActivity(UnionRoom *param0);
void UnionRoom_SendActivityRequest(UnionRoom *param0, int param1, u32 param2);
void UnionRoom_HandleNoOpTrainerInfo(int param0, int param1, void *param2, void *param3);
void UnionRoom_HandleNoOpNetId(int param0, int param1, void *param2, void *param3);
void UnionRoom_HandleResetState(int param0, int param1, void *param2, void *param3);
void UnionRoom_HandleSetActivity(int param0, int param1, void *param2, void *param3);
void UnionRoom_HandlePeerActivity(int param0, int param1, void *param2, void *param3);
void UnionRoom_HandlePeerNoActivity(int param0, int param1, void *param2, void *param3);
int UnionRoom_GetPeerNoActivity(UnionRoom *param0);
int UnionRoom_CancelActivity(UnionRoom *param0, int param1);
void UnionRoom_HandleTrainerCase(int param0, int param1, void *param2, void *param3);
u8 *UnionRoom_GetTrainerCaseBuffer(int param0, void *param1, int param2);
void UnionRoom_HandleMenuChoice(int param0, int param1, void *param2, void *param3);
u16 UnionRoom_GetCancelState(UnionRoom *param0);
void UnionRoom_SendMenuChoice(int param0);
int UnionRoom_GetTrainerCasePlayerMessage(StringTemplate *strTemplate);
int UnionRoom_GetMessage(UnionRoom *param0, int param1, int msgType, StringTemplate *strTemplate);
u8 UnionRoom_GetCommInfoGameCode(void);
void UnionRoom_BroadcastActivity(int param0);
int UnionRoom_GetTealaMessage(UnionRoom *param0, StringTemplate *strTemplate);
void UnionRoom_SetEasyChatSentence(UnionRoom *param0, EasyChatSentence *param1);
EasyChatSentence *UnionRoom_TakeEasyChatSentence(UnionRoom *param0);
void UnionRoom_DoGreeting(StringTemplate *strTemplate, int param1, int param2, TrainerInfo *playerTrainerInfo, UnlockedEasyChatWords *unlockedWords);
void UnionRoom_InitGameInfo(EasyChatSentence *param0);
void UnionRoom_ResetActivity(UnionRoom *param0);
void *UnionRoom_GetTrainerCase(UnionRoom *param0);
void UnionRoom_FreeTrainerCase(UnionRoom *param0);
void UnionRoom_SendTrainerCase(UnionRoom *param0);

#endif // POKEPLATINUM_UNION_ROOM_H
