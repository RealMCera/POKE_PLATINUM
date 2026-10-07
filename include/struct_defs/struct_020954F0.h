#ifndef POKEPLATINUM_STRUCT_020954F0_H
#define POKEPLATINUM_STRUCT_020954F0_H

// A contest judge. Each contest-type field is 0 if the judge does not judge
// that type, 1 for a normal judge, or 2 for a special judge (drawn from a
// separate pool when the judges are chosen).
typedef struct {
    u16 judgeNameMessageID;
    u8 padding_02[2];
    u16 cool : 2;
    u16 beauty : 2;
    u16 cute : 2;
    u16 smart : 2;
    u16 tough : 2;
    u16 contestRank : 2;
    u16 : 4;
    u16 padding_06;
} ContestJudge;

#endif // POKEPLATINUM_STRUCT_020954F0_H
