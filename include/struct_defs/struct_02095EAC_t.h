#ifndef POKEPLATINUM_STRUCT_02095EAC_T_H
#define POKEPLATINUM_STRUCT_02095EAC_T_H

#include "struct_defs/struct_0203DDFC.h"
#include "struct_defs/struct_02095EAC_sub1.h"

#include "overlay058/struct_ov58_021D2754.h"
#include "overlay058/struct_ov58_021D2820.h"

#include "bg_window.h"
#include "message.h"
#include "sprite.h"
#include "sprite_resource.h"
#include "sprite_util.h"
#include "string_gf.h"
#include "string_template.h"
#include "trainer_info.h"
#include "yes_no_touch_menu.h"

// State for the Union Room drawing (oekaki) app (overlay058). The app lets up
// to five players share a canvas; the comm handlers in
// union_room_drawing_comm.c operate on this struct.
struct UnionRoomDrawing {
    BgConfig *unk_00;
    BOOL unk_04;
    UnkStruct_0203DDFC *unk_08;
    StringTemplate *unk_0C;
    MessageLoader *unk_10;
    String *unk_14[5];
    String *unk_28;
    String *unk_2C;
    int unk_30;
    SpriteList *unk_34;
    G2dRenderer unk_38;
    SpriteResourceCollection *unk_1C4[4];
    SpriteResource *unk_1D4[2][4];
    SpriteResourcesHeader unk_1F4;
    SpriteResourcesHeader unk_218;
    Sprite *unk_23C[14];
    Sprite *unk_274[14];
    Sprite *unk_2AC[12];
    Window unk_2DC[5];
    Window drawingWindow; // 30x15-tile canvas the players draw on
    Window unk_33C;
    Window unk_34C;
    Window *unk_35C[2];
    int appState; // top-level app state, mirrored from the app's *param1
    int unk_368;
    int unk_36C;
    int unk_370;
    int unk_374;
    int unk_378;
    int connectedCount; // connected player count snapshot
    int connectedBitmap; // connected net-ID bitmap snapshot
    int drawingPlayerNetId; // net ID of the player currently drawing
    u8 unk_388[8][2];
    TrainerInfo *unk_398[8][2];
    u8 unk_3D8[16384];
    u16 unk_43D8;
    u8 unk_43DA;
    u8 unk_43DB;
    UnkStruct_ov58_021D2820 localDrawingStatus; // this player's pen/cursor status
    UnkStruct_ov58_021D2820 drawingStatus[5]; // every player's pen/cursor status
    UnkStruct_ov58_021D2754 unk_4418[5];
    u8 *unk_442C;
    int sendChunkIndex; // index of the canvas chunk currently being streamed
    u8 drawingTiles[14400]; // canvas tile data (30 * 15 tiles of 32 bytes)
    UnionRoomDrawingChunk sendChunk; // chunk currently being streamed to clients
    UnionRoomDrawingChunk recvChunks[5]; // per-net-ID receive buffers for incoming chunks
    s32 unk_9414;
    u32 ackedNetIds; // server: net IDs that accepted the join handshake
    u16 ackedCount; // client: connected count reported by the server
    s16 unk_941E;
    u8 unk_9420;
    UnkStruct_ov58_021D2820 drawingStatusBroadcast[5]; // server: statuses relayed to all players
    YesNoTouchMenu *unk_9454;
    int drawingState; // 1 = idle/ready, 2 = drawing in progress
    int unk_945C;
    int clientReadySent; // server: a client has reported ready
    int unk_9464;
    int unk_9468;
    int unk_946C;
};

#endif // POKEPLATINUM_STRUCT_02095EAC_T_H
