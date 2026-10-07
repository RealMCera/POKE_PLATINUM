#ifndef POKEPLATINUM_COMM_FIELD_CMD_H
#define POKEPLATINUM_COMM_FIELD_CMD_H

// Field (overworld) communication command table. CommFieldCmd_Init registers
// the table with the comm system; the packet-size helpers are shared with the
// Mix Records and Union Room drawing command tables.
void CommFieldCmd_NoOp(int param0, int param1, void *param2, void *param3);
void CommFieldCmd_Init(void *param0);
int CommFieldCmd_PacketSizeOf_RecordData(void);
int CommFieldCmd_PacketSizeOf_DrawingChunk(void);
int CommFieldCmd_PacketSizeOf_DrawingPlayerStatus(void);
int CommFieldCmd_PacketSizeOf_DrawingAllStatuses(void);

#endif // POKEPLATINUM_COMM_FIELD_CMD_H
