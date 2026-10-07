#ifndef POKEPLATINUM_UNION_ROOM_DRAWING_COMM_H
#define POKEPLATINUM_UNION_ROOM_DRAWING_COMM_H

// Communication command handlers for the Union Room drawing (oekaki) app
// (overlay058). Register them with UnionRoomDrawing_RegisterCommHandlers,
// passing the app's UnionRoomDrawing state as the handler context.
void UnionRoomDrawing_RegisterCommHandlers(void *app);
void UnionRoomDrawing_HandleChunkTransfer(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_ReceivePlayerStatus(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_ReceiveAllStatuses(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_HandleTransferComplete(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_HandleConnectionAck(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_ReceiveUnusedCmd125(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_HandleBeginDrawing(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_ReceiveUnusedCmd121(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_ReceiveUnusedCmd122(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_HandleServerEndDrawing(int senderNetId, int unused, void *data, void *app);
void UnionRoomDrawing_HandleClientReady(int senderNetId, int unused, void *data, void *app);

#endif // POKEPLATINUM_UNION_ROOM_DRAWING_COMM_H
