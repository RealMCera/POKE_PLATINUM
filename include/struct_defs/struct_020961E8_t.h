#ifndef POKEPLATINUM_STRUCT_020961E8_T_H
#define POKEPLATINUM_STRUCT_020961E8_T_H

#include <nnsys.h>

#include "struct_defs/struct_0203DE34.h"
#include "struct_defs/struct_020961E8_sub1.h"

#include "overlay059/struct_ov59_021D109C.h"
#include "overlay059/struct_ov59_021D30E0.h"

#include "bg_window.h"
#include "menu.h"
#include "message.h"
#include "savedata.h"
#include "sprite.h"
#include "sprite_resource.h"
#include "sprite_util.h"
#include "string_gf.h"
#include "string_template.h"
#include "sys_task_manager.h"
#include "trainer_info.h"

// Communication state for the Mix Records application (overlay 059). One
// instance is owned by the app and registered as the active command handler
// via CommCmd_Init. It holds the app's UI resources plus the per-player record
// exchange state used while mixing records in the Union Room.
struct MixRecordsComm {
    BgConfig *bgConfig; // Background configuration shared with the app.
    BOOL unk_04; // Unused.
    UnkStruct_0203DE34 *appArgs; // Application arguments (save data, options, ...).
    UnkStruct_ov59_021D109C animation; // Rotating record-icon animation state.
    SysTask *vBlankTask; // Per-VBlank task driving the animation.
    StringTemplate *strTemplate; // Template used to format messages.
    MessageLoader *msgLoader; // Loader for the app's message bank.
    String *trainerNameStrings[5]; // Per-player trainer name strings.
    String *unk_40; // Unused.
    String *formattedMessage; // Scratch string for the current message.
    String *headerString; // Header text shown at the top of the screen.
    int textPrinterID; // Handle of the active text printer (0xff when none).
    SpriteList *spriteList; // Sprite list for the record icons.
    G2dRenderer g2dRenderer; // 2D renderer for the sprite list.
    SpriteResourceCollection *spriteResourceCollections[4]; // Sprite resource collections.
    SpriteResource *spriteResources[3][4]; // Loaded sprite resources.
    SpriteResourcesHeader unk_220; // Unused.
    SpriteResourcesHeader unk_244; // Unused.
    SpriteResourcesHeader spriteHeader; // Header for the record-icon sprites.
    Sprite *sprites[14]; // Record-icon sprites.
    Sprite *unk_2C4[14]; // Unused.
    Window playerWindows[5]; // Per-player list windows.
    Window messageWindow; // Message box window.
    Window unk_35C; // Unused empty window.
    Window headerWindow; // Header text window.
    Window *unk_37C[2]; // Unused.
    Menu *yesNoMenu; // Yes/No choice menu.
    void *charDataBuffers[2]; // Character data buffers (freed on exit).
    NNSG2dCharacterData *charData[2]; // Character data for the trainer sprites.
    void *plttDataBuffers[2]; // Palette data buffers (freed on exit).
    NNSG2dPaletteData *plttData[2]; // Palette data for the trainer sprites.
    int state; // Current state-machine state.
    int nextState; // State to switch to once the current one finishes.
    int unk_3B0; // Unused.
    int timer; // Frame counter used by several states.
    u8 unk_3B8[8][2]; // Unused.
    TrainerInfo *trainerInfo[5][2]; // Per-player trainer info (current/previous).
    int spriteStates[5]; // Per-player sprite transition state.
    int syncSaveState; // Save-sync state machine (see sub_02038EDC).
    u16 paletteCycleAngle; // Angle used to pulse the record-icon palette.
    u16 *paletteBuffer; // Palette buffer for the trainer sprites.
    u8 unk_410; // Unused.
    u8 unk_411; // Unused.
    int unk_414; // Written but never read.
    UnkStruct_ov59_021D30E0 localRecord; // Local player's record, sent on command 116.
    UnkStruct_ov59_021D30E0 receivedRecords[5]; // Records received from each player.
    int receivedRecordCount; // Number of records received so far.
    SaveData *saveData; // Save data being mixed.
    int confirmFlag; // Local player's confirm flag (command 117).
    u8 disconnected; // Set when a peer drops out (command 115).
    volatile int prevConnectedCount; // Connected count at the last state update.
    int expectedPlayerCount; // Expected number of connected players.
    int playerCountExceeded; // Set when more players than expected are present.
    u32 confirmedBitmap; // Server-side bitmap of confirmed players.
    u8 confirmPending; // Set while waiting for a connection-confirm reply.
    s8 maxConnectionsMode; // How to adjust max connections (see ov59_021D28D8).
    u8 recordPlayerCount; // Player count captured before sending records.
    u8 disconnectSent; // Set once the disconnect command has been sent.
    s32 connectedCountSnapshot; // Connected count captured when the exchange starts.
    u16 serverPlayerCount; // Player count reported by the server.
    s16 retryTimer; // Frames waited before retrying the connection confirm.
    int timeoutTimer; // Frames remaining before the exchange times out.
    MixRecordsTrainerId trainerIds[5][2]; // Per-player trainer ID (current/previous).
};

#endif // POKEPLATINUM_STRUCT_020961E8_T_H
