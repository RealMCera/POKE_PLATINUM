#include "font_oam.h"

#include <nitro.h>
#include <string.h>

#include "struct_defs/struct_020127E8.h"

#include "bg_window.h"
#include "char_transfer.h"
#include "graphics.h"
#include "heap.h"
#include "sprite.h"

// Renders a Window's pixel data as OAM sprites. Text is first drawn into a
// Window (a BG tilemap-backed pixel buffer); this module copies the window's
// pixels into OBJ VRAM and creates one sprite per rectangular chunk of the
// window. A single OBJ VRAM transfer is limited to a block size, so the window
// is split into a list of chunks no larger than that block, and each chunk
// becomes its own sprite.
//
// A FontOAMManager owns the shared cell banks (one per chunk size class) and a
// fixed-size pool of FontOAM instances. A FontOAM is the per-text object: it
// owns the sprites for its chunks and can be positioned, shown/hidden, and
// recoloured as a unit.

// One sprite belonging to a FontOAM, plus its pixel offset from the FontOAM's
// origin. The offsets are the chunk's tile coordinates multiplied by the tile
// size (8 pixels).
typedef struct {
    Sprite *sprite;
    int offsetX;
    int offsetY;
} FontOAMSprite;

// A single rendered text object. It owns `spriteCount` sprites, one per chunk
// of the source window, and optionally follows a parent sprite.
typedef struct FontOAM {
    FontOAMSprite *sprites;
    int spriteCount;
    const Sprite *parentSprite;
    int x;
    int y;
} FontOAM;

// Owns the cell banks shared by every FontOAM and the pool of FontOAM
// instances. There are 12 cell banks, one for each entry of
// sFontOAMChunkSizes; a chunk selects its bank by size class.
typedef struct FontOAMManager {
    void *cellBankData[12];
    NNSG2dCellDataBank *cellBanks[12];
    FontOAM *oam;
    int capacity;
} FontOAMManager;

// A rectangle of the source window, in tiles. Note the field order: `y` comes
// before `x`, matching the order the chunk builder fills them in.
typedef struct {
    int y;
    int x;
    int width;
    int height;
} FontOAMRect;

// Working state for FontOAM_AddChunk. `rect` is the region still to be tiled;
// when a chunk leaves a horizontal remainder, it is stashed in `savedRect` and
// `hasSavedRect` is set so the next call resumes there.
typedef struct {
    FontOAMRect rect;
    FontOAMRect savedRect;
    u8 hasSavedRect;
} FontOAMRectSplit;

// A node in the circular, doubly-linked list of chunks that tile a window.
// `sizeClass` indexes sFontOAMChunkSizes to give the chunk's tile dimensions.
typedef struct FontOAMChunk {
    int x;
    int y;
    int sizeClass;
    struct FontOAMChunk *next;
    struct FontOAMChunk *prev;
} FontOAMChunk;

// A window whose chunk layout has been precomputed. It can report the VRAM
// size needed for the window and be reused to build several FontOAMs.
typedef struct FontOAMWindow {
    FontOAMChunk chunks;
    int chunkCount;
} FontOAMWindow;

static void FontOAM_Clear(FontOAM *param0);
static FontOAM *FontOAMManager_GetFreeOAM(const FontOAMManager *param0);
static int FontOAM_GetSizeClass(int param0, int param1);
static int FontOAM_BuildChunkList(int param0, int param1, enum HeapID heapID, FontOAMChunk *param3);
static void FontOAM_InitImageProxies(const Window *param0, const FontOAMChunk *param1, NNSG2dImageProxy *param2, int param3, int param4, enum HeapID heapID);
static int FontOAM_UploadChunk(const Window *param0, const FontOAMChunk *param1, NNSG2dImageProxy *param2, int param3, int param4, int param5, int param6, enum HeapID heapID);
static int FontOAM_GetChunkListSize(const FontOAMChunk *param0, int param1);
static void FontOAM_CreateSprites(const UnkStruct_020127E8 *param0, const FontOAMChunk *param1, const NNSG2dImageProxy *param2, FontOAM *param3);
static Sprite *FontOAM_CreateSprite(const UnkStruct_020127E8 *param0, const FontOAMChunk *param1, const NNSG2dImageProxy *param2);
static void FontOAM_DeleteSprites(FontOAM *param0);
static FontOAMChunk *FontOAMChunk_New(enum HeapID heapID);
static void FontOAMChunk_Free(FontOAMChunk *param0);
static void FontOAMChunkList_Free(FontOAMChunk *param0);
static void FontOAMChunkList_Insert(FontOAMChunk *param0, FontOAMChunk *param1);
static void FontOAM_CopyChunksToBuffer(const Window *param0, char *param1, const FontOAMChunk *param2, int param3, int param4);
static int FontOAM_CopyChunkToBuffer(const Window *param0, const FontOAMChunk *param1, char *param2, int param3, int param4, int param5, int param6);

// Candidate chunk dimensions in tiles, ordered from largest to smallest. A
// chunk picks the first entry that fits inside the region left to tile.
static const u8 sFontOAMChunkSizes[12][2] = {
    { 0x8, 0x8 },
    { 0x8, 0x4 },
    { 0x4, 0x8 },
    { 0x4, 0x4 },
    { 0x4, 0x2 },
    { 0x4, 0x1 },
    { 0x2, 0x4 },
    { 0x2, 0x2 },
    { 0x2, 0x1 },
    { 0x1, 0x4 },
    { 0x1, 0x2 },
    { 0x1, 0x1 }
};

// Allocates a manager with room for `capacity` FontOAMs and loads the 12
// shared cell banks from graphics bank 35.
FontOAMManager *FontOAMManager_New(int param0, enum HeapID heapID)
{
    FontOAMManager *v0;
    int v1;

    v0 = Heap_Alloc(heapID, sizeof(FontOAMManager));
    GF_ASSERT(v0);

    for (v1 = 0; v1 < 12; v1++) {
        v0->cellBankData[v1] = Graphics_GetCellBank(35, v1, 0, &v0->cellBanks[v1], heapID);

        GF_ASSERT(v0->cellBankData[v1]);
    }

    v0->oam = Heap_Alloc(heapID, sizeof(FontOAM) * param0);
    GF_ASSERT(v0->oam);

    v0->capacity = param0;
    memset(v0->oam, 0, sizeof(FontOAM) * param0);

    return v0;
}

void FontOAMManager_Free(FontOAMManager *param0)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < 12; v0++) {
        Heap_Free(param0->cellBankData[v0]);
    }

    Heap_Free(param0->oam);
    Heap_Free(param0);
}

// Builds a FontOAM from a template (UnkStruct_020127E8): the template supplies
// the manager, the source window, the sprite list and palette, an optional
// parent sprite to follow, the VRAM offset, the position, and the priorities.
FontOAM *FontOAM_New(const UnkStruct_020127E8 *param0)
{
    FontOAM *v0;
    FontOAMChunk v1;
    int v2;
    NNSG2dImageProxy *v3;

    GF_ASSERT(param0);

    v0 = FontOAMManager_GetFreeOAM(param0->unk_00);
    GF_ASSERT(v0);

    v0->parentSprite = param0->unk_10;
    v0->x = param0->unk_18;
    v0->y = param0->unk_1C;

    // Empty circular list head; the builder appends chunks after it.
    v1.next = &v1;
    v1.prev = &v1;

    v2 = FontOAM_BuildChunkList(param0->unk_04->width, param0->unk_04->height, param0->heapID, &v1);
    v3 = Heap_AllocAtEnd(param0->heapID, sizeof(NNSG2dImageProxy) * v2);

    v0->sprites = Heap_Alloc(param0->heapID, sizeof(FontOAMSprite) * v2);
    v0->spriteCount = v2;

    FontOAM_InitImageProxies(param0->unk_04, &v1, v3, param0->unk_14, param0->unk_28, param0->heapID);
    FontOAM_CreateSprites(param0, &v1, v3, v0);
    Heap_Free(v3);
    FontOAMChunkList_Free(&v1);

    return v0;
}

void FontOAM_Free(FontOAM *param0)
{
    GF_ASSERT(param0);
    GF_ASSERT(param0->sprites);

    FontOAM_DeleteSprites(param0);
    Heap_Free(param0->sprites);
    FontOAM_Clear(param0);
}

// Returns the VRAM size, in bytes, needed to upload `param0`'s pixels for the
// given VRAM type. Used to reserve a CharTransfer range before creating the
// FontOAM.
int FontOAM_GetWindowSize(const Window *param0, int param1, enum HeapID heapID)
{
    FontOAMChunk v0;
    int v1;

    v0.next = &v0;
    v0.prev = &v0;

    FontOAM_BuildChunkList(param0->width, param0->height, heapID, &v0);
    v1 = FontOAM_GetChunkListSize(&v0, param1);
    FontOAMChunkList_Free(&v0);

    return v1;
}

void FontOAM_SetXY(FontOAM *fontOAM, int x, int y)
{
    GF_ASSERT(fontOAM);

    fontOAM->x = x;
    fontOAM->y = y;

    x *= FX32_ONE;
    y *= FX32_ONE;

    if (fontOAM->parentSprite) {
        const VecFx32 *fontPos = Sprite_GetPosition(fontOAM->parentSprite);

        x += fontPos->x;
        y += fontPos->y;
    }

    VecFx32 spritePos;
    spritePos.z = 0;

    for (int v0 = 0; v0 < fontOAM->spriteCount; v0++) {
        spritePos.x = x + (fontOAM->sprites[v0].offsetX << FX32_SHIFT);
        spritePos.y = y + (fontOAM->sprites[v0].offsetY << FX32_SHIFT);

        Sprite_SetPosition(fontOAM->sprites[v0].sprite, &spritePos);
    }
}

// Re-applies the current x/y (plus the parent sprite's position) to every
// sprite. Call after the parent sprite has moved.
void FontOAM_UpdatePosition(FontOAM *param0)
{
    int v0;
    VecFx32 v1;
    const VecFx32 *v2;
    fx32 v3, v4;

    GF_ASSERT(param0);

    if (param0->parentSprite) {
        v3 = param0->x << FX32_SHIFT;
        v4 = param0->y << FX32_SHIFT;
        v2 = Sprite_GetPosition(param0->parentSprite);

        v3 += v2->x;
        v4 += v2->y;

        v1.z = 0;

        for (v0 = 0; v0 < param0->spriteCount; v0++) {
            v1.x = v3 + (param0->sprites[v0].offsetX << FX32_SHIFT);
            v1.y = v4 + (param0->sprites[v0].offsetY << FX32_SHIFT);

            Sprite_SetPosition(param0->sprites[v0].sprite, &v1);
        }
    }
}

void FontOAM_GetXY(const FontOAM *fontOAM, int *x, int *y)
{
    GF_ASSERT(fontOAM);
    GF_ASSERT(x);
    GF_ASSERT(y);

    *x = fontOAM->x;
    *y = fontOAM->y;
}

void FontOAM_SetDrawFlag(FontOAM *param0, BOOL draw)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetDrawFlag(param0->sprites[v0].sprite, draw);
    }
}

void FontOAM_SetExplicitPriority(FontOAM *param0, u8 param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetExplicitPriority(param0->sprites[v0].sprite, param1);
    }
}

void FontOAM_SetPriority(FontOAM *param0, u32 param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetPriority(param0->sprites[v0].sprite, param1);
    }
}

void FontOAM_SetExplicitPalette(FontOAM *param0, u32 param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetExplicitPalette(param0->sprites[v0].sprite, param1);
    }
}

void FontOAM_SetExplicitPaletteOffset(FontOAM *param0, u32 param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetExplicitPaletteOffset(param0->sprites[v0].sprite, param1);
    }
}

void FontOAM_SetExplicitPaletteOffsetAutoAdjust(FontOAM *param0, u32 param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetExplicitPaletteOffsetAutoAdjust(param0->sprites[v0].sprite, param1);
    }
}

void FontOAM_SetExplicitOAMMode(FontOAM *param0, GXOamMode param1)
{
    int v0;

    GF_ASSERT(param0);

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_SetExplicitOAMMode(param0->sprites[v0].sprite, param1);
    }
}

// Precomputes the chunk layout for a window so it can be reused to build
// several FontOAMs without re-tiling each time.
FontOAMWindow *FontOAMWindow_New(const Window *param0, enum HeapID heapID)
{
    FontOAMWindow *v0 = Heap_Alloc(heapID, sizeof(FontOAMWindow));

    v0->chunks.next = &v0->chunks;
    v0->chunks.prev = &v0->chunks;
    v0->chunkCount = FontOAM_BuildChunkList(param0->width, param0->height, heapID, &v0->chunks);

    return v0;
}

void FontOAMWindow_Free(FontOAMWindow *param0)
{
    FontOAMChunkList_Free(&param0->chunks);
    Heap_Free(param0);
}

int FontOAMWindow_GetSize(const FontOAMWindow *param0, int param1)
{
    return FontOAM_GetChunkListSize(&param0->chunks, param1);
}

// Like FontOAM_New, but reuses a precomputed FontOAMWindow layout instead of
// tiling the window again.
FontOAM *FontOAM_NewFromWindow(const UnkStruct_020127E8 *param0, const FontOAMWindow *param1)
{
    FontOAM *v0;
    NNSG2dImageProxy *v1;

    GF_ASSERT(param0);
    v0 = FontOAMManager_GetFreeOAM(param0->unk_00);

    GF_ASSERT(v0);

    v0->parentSprite = param0->unk_10;
    v0->x = param0->unk_18;
    v0->y = param0->unk_1C;

    v1 = Heap_AllocAtEnd(param0->heapID, sizeof(NNSG2dImageProxy) * param1->chunkCount);

    v0->sprites = Heap_Alloc(param0->heapID, sizeof(FontOAMSprite) * param1->chunkCount);
    v0->spriteCount = param1->chunkCount;

    FontOAM_InitImageProxies(param0->unk_04, &param1->chunks, v1, param0->unk_14, param0->unk_28, param0->heapID);
    FontOAM_CreateSprites(param0, &param1->chunks, v1, v0);
    Heap_Free(v1);

    return v0;
}

void FontOAM_Delete(FontOAM *param0)
{
    FontOAM_Free(param0);
}

// Copies the window's pixels into a temporary buffer and uploads them to the
// OBJ VRAM location already assigned to the FontOAM's first sprite.
void FontOAMWindow_UploadToVRAM(FontOAM *param0, const FontOAMWindow *param1, const Window *param2, enum HeapID heapID)
{
    int v0;
    char *v1;
    NNSG2dImageProxy *v2;
    Sprite *v3 = param0->sprites[0].sprite;
    int v4 = Sprite_GetVRamType(v3);
    v0 = FontOAMWindow_GetSize(param1, v4);
    v1 = (char *)Heap_AllocAtEnd(heapID, v0);

    memset(v1, 0, v0);

    FontOAM_CopyChunksToBuffer(param2, v1, &param1->chunks, v4, heapID);
    DC_FlushRange(v1, v0);

    v2 = Sprite_GetImageProxy(v3);

    if (v4 == NNS_G2D_VRAM_TYPE_2DMAIN) {
        GX_LoadOBJ(v1, NNS_G2dGetImageLocation(v2, NNS_G2D_VRAM_TYPE_2DMAIN), v0);
    } else {
        GXS_LoadOBJ(v1, NNS_G2dGetImageLocation(v2, NNS_G2D_VRAM_TYPE_2DSUB), v0);
    }

    Heap_Free(v1);
}

// Copies a rectangle of pixels from the window to the output starting at (x,y)
// and spanning (width,height). Window pixels are 4bpp, so each tile row is 32
// bytes wide.
void FontOAM_CopyWindowRect(
    const Window *window,
    int width,
    int height,
    int x,
    int y,
    char *output)
{
    int i;
    int dstOffset;
    int srcOffset;

    GF_ASSERT(window->width >= (width + x));
    GF_ASSERT(window->height >= (height + y));

    for (i = 0; i < height; i++) {
        dstOffset = i * width;
        dstOffset *= 32;
        srcOffset = ((i + y) * window->width);
        srcOffset += x;
        srcOffset *= 32;

        memcpy(output + dstOffset, (char *)(window->pixels) + srcOffset, 32 * width);
    }
}

static void FontOAM_Clear(FontOAM *param0)
{
    memset(param0, 0, sizeof(FontOAM));
}

// Returns the first unused FontOAM in the manager's pool, or NULL if full. A
// FontOAM is free when it has no sprites.
static FontOAM *FontOAMManager_GetFreeOAM(const FontOAMManager *param0)
{
    int v0;

    for (v0 = 0; v0 < param0->capacity; v0++) {
        if (param0->oam[v0].sprites == NULL) {
            return param0->oam + v0;
        }
    }

    return NULL;
}

// Returns the index of the largest entry of sFontOAMChunkSizes that fits
// within a width x height tile region.
static int FontOAM_GetSizeClass(int param0, int param1)
{
    int v0;

    for (v0 = 0; v0 < 12; v0++) {
        if ((sFontOAMChunkSizes[v0][0] <= param0) && (sFontOAMChunkSizes[v0][1] <= param1)) {
            return v0;
        }
    }

    return 12;
}

// Carves one chunk out of the region described by `param0` and appends it to
// the list `param1`. Returns 1 when the current row is fully tiled, or 0 when
// there is more to do (the caller loops until it returns 1).
static BOOL FontOAM_AddChunk(FontOAMRectSplit *param0, FontOAMChunk *param1, enum HeapID heapID)
{
    FontOAMChunk *v0;
    int v1;
    int v2;

    v0 = FontOAMChunk_New(heapID);
    FontOAMChunkList_Insert(v0, param1->prev);

    v0->sizeClass = FontOAM_GetSizeClass(param0->rect.width, param0->rect.height);
    v0->x = param0->rect.x;
    v0->y = param0->rect.y;

    v2 = param0->rect.width - sFontOAMChunkSizes[v0->sizeClass][0];
    v1 = param0->rect.height - sFontOAMChunkSizes[v0->sizeClass][1];

    if (v2 > 0) {
        // The chunk did not span the full width; save the remainder to tile
        // after the current row is finished.
        param0->savedRect.height = param0->rect.height;
        param0->savedRect.width = v2;
        param0->savedRect.y = param0->rect.y;
        param0->savedRect.x = param0->rect.x + sFontOAMChunkSizes[v0->sizeClass][0];

        GF_ASSERT(param0->hasSavedRect != 1);
        param0->hasSavedRect = 1;
    }

    if (v1 > 0) {
        // The chunk did not span the full height; continue with the rest.
        param0->rect.y = param0->rect.y + sFontOAMChunkSizes[v0->sizeClass][1];
        param0->rect.height = v1;
    } else {
        if (param0->hasSavedRect == 1) {
            param0->rect = param0->savedRect;
            param0->hasSavedRect = 0;
        } else {
            return 1;
        }
    }

    return 0;
}

// Tiles a width x height tile region into chunks, appending them to the
// circular list `param3`, and returns the number of chunks created.
static int FontOAM_BuildChunkList(int param0, int param1, enum HeapID heapID, FontOAMChunk *param3)
{
    FontOAMRect v0;
    FontOAMRectSplit v1;
    int v2;
    int v3;

    GF_ASSERT(param0);
    GF_ASSERT(param1);

    v3 = 0;

    v1.rect.y = 0;
    v1.rect.x = 0;
    v1.rect.width = param0;
    v1.rect.height = param1;
    v1.hasSavedRect = 0;

    v0.x = 0;
    v0.width = param0;

    // Process the region one row at a time. Each row is as tall as the chunk
    // chosen for it; FontOAM_AddChunk fills the row left to right.
    while (v1.rect.height != 0) {
        v2 = FontOAM_GetSizeClass(v1.rect.width, v1.rect.height);

        v0.y = v1.rect.y + sFontOAMChunkSizes[v2][1];
        v0.height = v1.rect.height - sFontOAMChunkSizes[v2][1];
        v1.rect.height = sFontOAMChunkSizes[v2][1];

        do {
            v3++;
        } while (FontOAM_AddChunk(&v1, param3, heapID) == 0);

        v1.rect = v0;
    }

    return v3;
}

// Uploads each chunk's pixels to VRAM and initialises one image proxy per
// chunk. `param3` is the running VRAM offset, advanced by each upload.
static void FontOAM_InitImageProxies(const Window *param0, const FontOAMChunk *param1, NNSG2dImageProxy *param2, int param3, int param4, enum HeapID heapID)
{
    FontOAMChunk *v0;
    int v1;
    int v2;
    GXOBJVRamModeChar v3;

    if (param4 == NNS_G2D_VRAM_TYPE_2DMAIN) {
        v3 = GX_GetOBJVRamModeChar();
    } else {
        v3 = GXS_GetOBJVRamModeChar();
    }

    v2 = CharTransfer_GetBlockSize(v3);
    v1 = 0;
    v0 = param1->next;

    while (v0 != param1) {
        NNS_G2dInitImageProxy(param2 + v1);
        param3 = FontOAM_UploadChunk(param0, v0, param2 + v1, v2, v3, param3, param4, heapID);
        v0 = v0->next;
        v1++;
    }
}

// Copies one chunk out of the window, uploads it to VRAM at `param5`, and
// fills in its image proxy. Returns the VRAM offset for the next chunk. The
// upload is padded up to the VRAM transfer block size.
static int FontOAM_UploadChunk(const Window *param0, const FontOAMChunk *param1, NNSG2dImageProxy *param2, int param3, int param4, int param5, int param6, enum HeapID heapID)
{
    char *v0;
    int v1;
    int v2 = sFontOAMChunkSizes[param1->sizeClass][0];
    int v3 = sFontOAMChunkSizes[param1->sizeClass][1];
    v1 = v2;
    v1 *= v3;

    if (v1 < param3) {
        v1 = param3;
    }

    v1 *= 32;
    v0 = Heap_AllocAtEnd(heapID, v1);

    FontOAM_CopyWindowRect(param0, v2, v3, param1->x, param1->y, v0);
    DC_FlushRange(v0, v1);

    if (param6 == NNS_G2D_VRAM_TYPE_2DMAIN) {
        GX_LoadOBJ(v0, param5, v1);
        param2->vramLocation.baseAddrOfVram[NNS_G2D_VRAM_TYPE_2DMAIN] = param5;
        param2->attr.mappingType = GX_GetOBJVRamModeChar();
    } else {
        GXS_LoadOBJ(v0, param5, v1);
        param2->vramLocation.baseAddrOfVram[NNS_G2D_VRAM_TYPE_2DSUB] = param5;
        param2->attr.mappingType = GXS_GetOBJVRamModeChar();
    }

    param2->attr.sizeS = NNS_G2D_1D_MAPPING_CHAR_SIZE;
    param2->attr.sizeT = NNS_G2D_1D_MAPPING_CHAR_SIZE;
    param2->attr.fmt = GX_TEXFMT_PLTT16;
    param2->attr.bExtendedPlt = 0;
    param2->attr.plttUse = GX_TEXPLTTCOLOR0_TRNS;
    param2->attr.mappingType = param4;

    Heap_Free(v0);

    return param5 + v1;
}

// Copies every chunk's pixels into a single contiguous buffer, in list order.
static void FontOAM_CopyChunksToBuffer(const Window *param0, char *param1, const FontOAMChunk *param2, int param3, int param4)
{
    FontOAMChunk *v0;
    int v1;
    int v2;
    int v3;
    GXOBJVRamModeChar v4;

    if (param3 == NNS_G2D_VRAM_TYPE_2DMAIN) {
        v4 = GX_GetOBJVRamModeChar();
    } else {
        v4 = GXS_GetOBJVRamModeChar();
    }

    v2 = CharTransfer_GetBlockSize(v4);
    v3 = 0;
    v0 = param2->next;

    while (v0 != param2) {
        v3 = FontOAM_CopyChunkToBuffer(param0, v0, param1, v3, v2, v4, param4);
        v0 = v0->next;
    }
}

// Copies one chunk into `param2` at byte offset `param3`, padded to the VRAM
// transfer block size, and returns the offset for the next chunk.
static int FontOAM_CopyChunkToBuffer(const Window *param0, const FontOAMChunk *param1, char *param2, int param3, int param4, int param5, int param6)
{
    int v0;
    int v1 = sFontOAMChunkSizes[param1->sizeClass][0];
    int v2 = sFontOAMChunkSizes[param1->sizeClass][1];
    v0 = v1;
    v0 *= v2;

    if (v0 < param4) {
        v0 = param4;
    }

    v0 *= 32;
    FontOAM_CopyWindowRect(param0, v1, v2, param1->x, param1->y, &param2[param3]);

    return param3 + v0;
}

// Returns the total VRAM size, in bytes, needed for every chunk in the list,
// each padded to the VRAM transfer block size.
static int FontOAM_GetChunkListSize(const FontOAMChunk *param0, int param1)
{
    FontOAMChunk *v0;
    int v1;
    GXOBJVRamModeChar v2;
    int v3;
    int v4;
    int v5, v6;

    if (param1 == NNS_G2D_VRAM_TYPE_2DMAIN) {
        v2 = GX_GetOBJVRamModeChar();
    } else {
        v2 = GXS_GetOBJVRamModeChar();
    }

    v1 = CharTransfer_GetBlockSize(v2);
    v3 = 0;
    v0 = param0->next;

    while (v0 != param0) {
        v5 = sFontOAMChunkSizes[v0->sizeClass][0];
        v6 = sFontOAMChunkSizes[v0->sizeClass][1];
        v4 = v5 * v6;

        if (v4 < v1) {
            v4 = v1;
        }

        v3 += v4 * 32;
        v0 = v0->next;
    }

    return v3;
}

// Creates one sprite per chunk and records each sprite's pixel offset from the
// FontOAM's origin (chunk tile coordinates times the 8-pixel tile size).
static void FontOAM_CreateSprites(const UnkStruct_020127E8 *param0, const FontOAMChunk *param1, const NNSG2dImageProxy *param2, FontOAM *param3)
{
    FontOAMChunk *v0;
    int v1 = 0;
    v0 = param1->next;

    while (v0 != param1) {
        param3->sprites[v1].sprite = FontOAM_CreateSprite(param0, v0, param2 + v1);
        GF_ASSERT(param3->sprites[v1].sprite);

        param3->sprites[v1].offsetX = v0->x * 8;
        param3->sprites[v1].offsetY = v0->y * 8;

        v0 = v0->next;
        v1++;
    }
}

static void FontOAM_DeleteSprites(FontOAM *param0)
{
    int v0;

    for (v0 = 0; v0 < param0->spriteCount; v0++) {
        Sprite_Delete(param0->sprites[v0].sprite);
    }
}

// Adds one sprite for `param1` to the template's sprite list, using the cell
// bank selected by the chunk's size class and the chunk's image proxy.
static Sprite *FontOAM_CreateSprite(const UnkStruct_020127E8 *param0, const FontOAMChunk *param1, const NNSG2dImageProxy *param2)
{
    SpriteListTemplate v0;
    SpriteResourcesHeader v1;

    v1.imageProxy = param2;
    v1.charData = NULL;
    v1.paletteProxy = param0->unk_0C;
    v1.cellBank = param0->unk_00->cellBanks[param1->sizeClass];
    v1.cellAnimBank = NULL;
    v1.multiCellBank = NULL;
    v1.multiCellAnimBank = NULL;
    v1.isVRamTransfer = 0;
    v1.priority = param0->unk_20;

    v0.list = param0->unk_08;
    v0.resourceData = &v1;
    v0.priority = param0->unk_24;
    v0.vramType = param0->unk_28;
    v0.heapID = param0->heapID;
    v0.position.x = 0;
    v0.position.y = 0;
    v0.position.z = 0;

    if (param0->unk_10) {
        const VecFx32 *v2;

        v2 = Sprite_GetPosition(param0->unk_10);
        v0.position = *v2;
    }

    v0.position.x += (param0->unk_18 << FX32_SHIFT) + ((param1->x * 8) << FX32_SHIFT);
    v0.position.y += (param0->unk_1C << FX32_SHIFT) + ((param1->y * 8) << FX32_SHIFT);

    return SpriteList_Add(&v0);
}

static FontOAMChunk *FontOAMChunk_New(enum HeapID heapID)
{
    FontOAMChunk *v0 = Heap_AllocAtEnd(heapID, sizeof(FontOAMChunk));
    GF_ASSERT(v0);

    v0->next = NULL;
    v0->prev = NULL;

    return v0;
}

static void FontOAMChunk_Free(FontOAMChunk *param0)
{
    GF_ASSERT(param0);
    Heap_Free(param0);
}

// Frees every node in the circular list, leaving the head `param0` itself
// untouched (it is usually embedded in a stack or heap struct).
static void FontOAMChunkList_Free(FontOAMChunk *param0)
{
    FontOAMChunk *v0;
    FontOAMChunk *v1;

    v0 = param0->next;

    while (v0 != param0) {
        v1 = v0->next;
        FontOAMChunk_Free(v0);
        v0 = v1;
    }
}

// Inserts `param0` immediately after `param1` in the circular list.
static void FontOAMChunkList_Insert(FontOAMChunk *param0, FontOAMChunk *param1)
{
    param0->next = param1->next;
    param0->prev = param1;
    param1->next->prev = param0;
    param1->next = param0;
}

void FontOAM_SetParentSprite(FontOAM *param0, const Sprite *param1)
{
    param0->parentSprite = param1;
    FontOAM_UpdatePosition(param0);
}
