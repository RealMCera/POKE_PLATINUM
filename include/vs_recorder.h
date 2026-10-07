#ifndef POKEPLATINUM_VS_RECORDER_H
#define POKEPLATINUM_VS_RECORDER_H

#include "struct_defs/struct_0208BA84.h"
#include "struct_defs/struct_0208C06C.h"

#include "overlay_manager.h"

// Vs. Recorder application. Hosts the overlay062 viewer for the Vs. Recorder
// and the Global Terminal's online features (Battle Videos, Rankings,
// Dress-Up Data and Box Data), and replays recorded battles on request.
UnkStruct_0208C06C *VsRecorder_GetState(ApplicationManager *appMan);
void VsRecorder_SetPlaybackRequest(VsRecorderPlaybackRequest *request, BOOL requested, int param2);
const ApplicationManagerTemplate *VsRecorder_GetAppTemplate(int mode);
BOOL VsRecorder_CheckFirstArrivalBattlePark(UnkStruct_0208C06C *vsRecorder);

#endif // POKEPLATINUM_VS_RECORDER_H
