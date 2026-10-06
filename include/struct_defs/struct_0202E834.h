#ifndef POKEPLATINUM_STRUCT_0202E834_H
#define POKEPLATINUM_STRUCT_0202E834_H

typedef struct {
    u8 active; // Set when a GTS trade is made, cleared once the segment is saved.
    u8 padding_01;
    u16 tradeCount;
} TVSegment_GTSTradeRecordData;

#endif // POKEPLATINUM_STRUCT_0202E834_H
