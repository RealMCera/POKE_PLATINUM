#include "appearance.h"

#include <nitro.h>
#include <string.h>

#include "generated/trainer_classes.h"

#include "graphics.h"
#include "heap.h"
#include "string_template.h"

// Associates a trainer appearance value with the trainer classes that are
// shown for it. The appearance value is what TrainerInfo stores; the enum
// TrainerAppearance is the row of the appearance sprite sheet it maps to.
typedef struct Appearance {
    int index; // Appearance value stored in TrainerInfo.
    int class1; // Trainer class used when the appearance is shown as text.
    int class2; // Trainer class used for battle and sprite graphics.
} Appearance;

// Appearance data for every trainer appearance, split into a male block
// (indices 0..APPEARANCES_COUNT-1) and a female block
// (indices APPEARANCES_COUNT..2*APPEARANCES_COUNT-1).
static const Appearance sTrainerAppearances[APPEARANCES_COUNT * 2] = {
    // male appearances
    [TRAINER_APPEARANCE_SCHOOL_KID_M] = { 3, TRAINER_CLASS_SCHOOL_KID_MALE, TRAINER_CLASS_SCHOOL_KID_MALE },
    [TRAINER_APPEARANCE_BUG_CATCHER] = { 5, TRAINER_CLASS_BUG_CATCHER, TRAINER_CLASS_BUG_CATCHER },
    [TRAINER_APPEARANCE_ACE_TRAINER_M] = { 11, TRAINER_CLASS_ACE_TRAINER_MALE, TRAINER_CLASS_ACE_TRAINER_MALE },
    [TRAINER_APPEARANCE_ROUGHNECK] = { 31, TRAINER_CLASS_ROUGHNECK, TRAINER_CLASS_ROUGHNECK },
    [TRAINER_APPEARANCE_RUIN_MANIAC] = { 50, TRAINER_CLASS_RUIN_MANIAC, TRAINER_CLASS_RUIN_MANIAC },
    [TRAINER_APPEARANCE_BLACK_BELT] = { 51, TRAINER_CLASS_BLACK_BELT, TRAINER_CLASS_BLACK_BELT },
    [TRAINER_APPEARANCE_RICH_BOY] = { 62, TRAINER_CLASS_RICH_BOY, TRAINER_CLASS_RICH_BOY },
    [TRAINER_APPEARANCE_PSYCHIC_M] = { 70, TRAINER_CLASS_PSYCHIC_MALE, TRAINER_CLASS_PSYCHIC_MALE },

    // female appearances
    [TRAINER_APPEARANCE_LASS] = { 6, TRAINER_CLASS_LASS, TRAINER_CLASS_LASS },
    [TRAINER_APPEARANCE_BATTLE_GIRL] = { 7, TRAINER_CLASS_BATTLE_GIRL, TRAINER_CLASS_BATTLE_GIRL },
    [TRAINER_APPEARANCE_BEAUTY] = { 13, TRAINER_CLASS_BEAUTY, TRAINER_CLASS_BEAUTY },
    [TRAINER_APPEARANCE_ACE_TRAINER_F] = { 14, TRAINER_CLASS_ACE_TRAINER_FEMALE, TRAINER_CLASS_ACE_TRAINER_FEMALE },
    [TRAINER_APPEARANCE_IDOL] = { 35, TRAINER_CLASS_IDOL, TRAINER_CLASS_IDOL },
    [TRAINER_APPEARANCE_SOCIALITE] = { 37, TRAINER_CLASS_SOCIALITE, TRAINER_CLASS_SOCIALITE },
    [TRAINER_APPEARANCE_COWGIRL] = { 42, TRAINER_CLASS_COWGIRL, TRAINER_CLASS_COWGIRL },
    [TRAINER_APPEARANCE_LADY] = { 63, TRAINER_CLASS_LADY, TRAINER_CLASS_LADY }
};

// For each trainer-ID bucket, the order in which the APPEARANCES_COUNT
// appearances are assigned to the VARIANTS_COUNT variants.
static const int sAppearanceShuffleTable[APPEARANCES_COUNT][VARIANTS_COUNT] = {
    { 0, 1, 2, 3 },
    { 1, 6, 7, 0 },
    { 2, 3, 4, 5 },
    { 3, 0, 5, 6 },
    { 4, 1, 2, 7 },
    { 5, 2, 7, 0 },
    { 6, 3, 4, 1 },
    { 7, 4, 5, 6 }
};

// Fills the string template's VARIANTS_COUNT trainer-class slots with the
// classes of the appearances assigned to trainerId.
void Appearance_LoadVariants(u32 trainerId, int trainerGender, StringTemplate *stringTemplate)
{
    int rnd = trainerId % APPEARANCES_COUNT;
    int variant;

    for (variant = 0; variant < VARIANTS_COUNT; variant++) {
        int appearanceIndex = sAppearanceShuffleTable[rnd][variant] + APPEARANCES_COUNT * trainerGender;
        StringTemplate_SetTrainerClassName(stringTemplate, variant, sTrainerAppearances[appearanceIndex].class1);
    }
}

// Returns the appearance value assigned to the given variant of the trainer
// identified by trainerId.
int Appearance_CalculateFromTrainerInfo(u32 trainerId, int trainerGender, u32 variant)
{
    int rnd = trainerId % APPEARANCES_COUNT;
    int appearanceIndex = sAppearanceShuffleTable[rnd][variant] + APPEARANCES_COUNT * trainerGender;

    return sTrainerAppearances[appearanceIndex].index;
}

// Finds the appearance whose stored value matches appearance, searching the
// block for the given gender. Returns 0 if no match is found.
static enum TrainerAppearance GetAppearanceIndex(int gender, int appearance)
{
    for (int i = 0; i < APPEARANCES_COUNT; i++) {
        if (sTrainerAppearances[i + gender * APPEARANCES_COUNT].index == appearance) {
            return i + gender * APPEARANCES_COUNT;
        }
    }

    return 0;
}

// Public wrapper around GetAppearanceIndex.
enum TrainerAppearance Appearance_GetIndex(int gender, int appearance)
{
    return GetAppearanceIndex(gender, appearance);
}

// Returns the requested datum (sprite row or trainer class) for the appearance
// whose stored value matches appearance.
int Appearance_GetData(int gender, int appearance, enum AppearanceDataParam param)
{
    enum TrainerAppearance appearanceIndex = GetAppearanceIndex(gender, appearance);

    switch (param) {
    case APPEARANCE_DATA_INDEX:
        return appearanceIndex;
    case APPEARANCE_DATA_TRAINER_CLASS_1:
        return sTrainerAppearances[appearanceIndex].class1;
    case APPEARANCE_DATA_TRAINER_CLASS_2:
        return sTrainerAppearances[appearanceIndex].class2;
    default:
        GF_ASSERT(FALSE);
        return 0;
    }
}

// Loads the trainer appearance palette (record.narc member 7) and copies its
// 16x16 colors into a freshly allocated 16x18 palette buffer. The last two
// rows of the buffer are left uninitialized.
u16 *Appearance_LoadTrainerPalette(enum HeapID heapID)
{
    void *plttHeapData;
    NNSG2dPaletteData *plttData;
    u16 *palette;
    u16 *rawData;
    int i;

    plttHeapData = Graphics_GetPlttData(NARC_INDEX_GRAPHIC__RECORD, 7, &plttData, heapID);
    palette = Heap_Alloc(heapID, 16 * 18 * 2);
    rawData = (u16 *)plttData->pRawData;

    for (i = 0; i < 16 * 16; i++) {
        palette[i] = rawData[i];
    }

    Heap_Free(plttHeapData);

    return palette;
}
