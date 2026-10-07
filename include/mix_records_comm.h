#ifndef POKEPLATINUM_MIX_RECORDS_COMM_H
#define POKEPLATINUM_MIX_RECORDS_COMM_H

void MixRecordsComm_Init(void *param0);
void MixRecordsComm_HandleDisconnect(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleEraseMessage(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleConnectionConfirm(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandlePlayerReady(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleUnused108(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleUnused109(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleCancelTrade(int param0, int param1, void *param2, void *param3);
void MixRecordsComm_HandleReadyRequest(int param0, int param1, void *param2, void *param3);

#endif // POKEPLATINUM_MIX_RECORDS_COMM_H
