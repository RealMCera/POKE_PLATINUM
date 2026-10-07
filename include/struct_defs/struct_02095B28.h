#ifndef POKEPLATINUM_STRUCT_02095B28_H
#define POKEPLATINUM_STRUCT_02095B28_H

// A value exchanged during visual competition scoring.
typedef struct {
    u32 value; // score or timer value
    u8 finished; // set once the contestant has finished scoring
    u8 padding_05[3];
} ContestCommValue;

#endif // POKEPLATINUM_STRUCT_02095B28_H
