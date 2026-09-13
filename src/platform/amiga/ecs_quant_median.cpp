#include "ecs_quant.h"
#include <string.h>
#include <stdint.h>

// Median cut (https://en.wikipedia.org/wiki/Medianut)

typedef struct
{
    int rMin;
    int rMax;
    int gMin;
    int gMax;
    int bMin;
    int bMax;
    int16_t weight;
    int32_t rSum;
    int32_t gSum;
    int32_t bSum;
    uint8 members[256];
    int nMembers;
} ColorBox;

#ifdef QUANT_ASM
    extern "C"
    {
        int64_t boxVariance_asm(const ColorBox *box __asm("a0"), const uint8 *pal5r __asm("a1"), const uint8 *pal5g __asm("a2"), const uint8 *pal5b __asm("a3"), const int16_t *colorWeight __asm("a4"));
        int findMedianCut_asm(const ColorBox *box __asm("a0"), int ch __asm("d0"), const uint8 *pal5r __asm("a1"), const uint8 *pal5g __asm("a2"), const uint8 *pal5b __asm("a3"), const int16_t *colorWeight __asm("a4"));
    }
    
    #define boxVariance boxVariance_asm
    #define findMedianCut findMedianCut_asm
#else
    #define boxVariance boxVariance_c
    #define findMedianCut findMedianCut_c
#endif

static UWORD packRGB12(int r, int g, int b)
{
    if (r < 0)
        r = 0;

    if (g < 0)
        g = 0;

    if (b < 0)
        b = 0;

    if (r > 31)
        r = 31;

    if (g > 31)
        g = 31;

    if (b > 31)
        b = 31;

    return ((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1);
}

static void boxInit(ColorBox *box)
{
    box->rMin = 32;
    box->gMin = 32;
    box->bMin = 32;
    box->rMax = -1;
    box->gMax = -1;
    box->bMax = -1;
    box->weight = 0;
    box->rSum = 0;
    box->gSum = 0;
    box->bSum = 0;
    box->nMembers = 0;
}

static void boxAdd(ColorBox *box, int idx, int r, int g, int b, int w)
{
    if (r < box->rMin)
        box->rMin = r;

    if (r > box->rMax)
        box->rMax = r;

    if (g < box->gMin)
        box->gMin = g;

    if (g > box->gMax)
        box->gMax = g;

    if (b < box->bMin)
        box->bMin = b;

    if (b > box->bMax)
        box->bMax = b;

    box->weight += w;
    box->rSum += r * w;
    box->gSum += g * w;
    box->bSum += b * w;
    box->members[box->nMembers++] = idx;
}

// NTSC luma variance
int64_t boxVariance_c(const ColorBox *box, const uint8 *pal5r, const uint8 *pal5g, const uint8 *pal5b, const int16_t *colorWeight)
{
    int64_t w = box->weight;

    if (w == 0)
        return 0;

    int64_t mr = box->rSum / w;
    int64_t mg = box->gSum / w;
    int64_t mb = box->bSum / w;
    int64_t var = 0;
    const uint8 *m = box->members;

    for (int i = 0; i < box->nMembers; i++)
    {
        int idx = m[0];
        m++;

        int64_t dr = pal5r[idx] - mr;
        int64_t dg = pal5g[idx] - mg;
        int64_t db = pal5b[idx] - mb;

        var += (dr * dr * 30 + dg * dg * 59 + db * db * 11) * colorWeight[idx];
    }

    return var;
}

// split on one axis
int findMedianCut_c(const ColorBox *box, int ch, const uint8 *pal5r, const uint8 *pal5g, const uint8 *pal5b, const int16_t *colorWeight)
{
    const uint8 *chan = (ch == 0) ? pal5r : (ch == 1) ? pal5g : pal5b;
    int16_t hist[32];
    int lo = 32;
    int hi = -1;
    int16_t total = 0;
    int16_t cum = 0;

    memset(hist, 0, sizeof(hist));

    const uint8 *m = box->members;

    for (int i = 0; i < box->nMembers; i++)
    {
        int idx = m[0];
        m++;

        hist[chan[idx]] += colorWeight[idx];
    }

    for (int v = 0; v < 32; v++)
    {
        if (!hist[v])
            continue;

        if (lo == 32)
            lo = v;

        hi = v;
        total += hist[v];
    }

    if (hi <= lo || total == 0)
        return lo;

    for (int v = lo; v <= hi; v++)
    {
        cum += hist[v];

        if (cum * 2 >= total)
            return v;
    }

    return (lo + hi) >> 1;
}

void ecsMedianQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette)
{
    ColorBox boxes[64];
    int nBoxes = 1;
    uint8 pal5r[256];
    uint8 pal5g[256];
    uint8 pal5b[256];
    int unsplittable[64] = {0};

    for (int i = 0; i < 256; i++)
    {
        pal5r[i] = palette[0] >> 3;
        pal5g[i] = palette[1] >> 3;
        pal5b[i] = palette[2] >> 3;
        
        palette += 3;
    }

    boxInit(&boxes[0]);

    const int16_t *cw = colorWeight;

    for (int i = 0; i < 256; i++)
    {
        int w = cw[0];
        cw++;

        if (!w)
            continue;

        boxAdd(&boxes[0], i, pal5r[i], pal5g[i], pal5b[i], w);
    }

    while (nBoxes < numColors)
    {
        int worst = -1;
        int64_t worstVar = -1;
        ColorBox *bx = NULL;
        int ch;
        int cutVal;
        int splitOk;
        ColorBox newBox;
        uint8 savedMembers[256];
        int nSaved;

        for (int i = 0; i < nBoxes; i++)
        {
            if (boxes[i].weight == 0 || unsplittable[i])
                continue;

            int64_t var = boxVariance(&boxes[i], pal5r, pal5g, pal5b, colorWeight);

            if (var > worstVar)
            {
                worstVar = var;
                worst = i;
            }
        }

        if (worst < 0 || worstVar <= 0)
            break;

        bx = &boxes[worst];

        // variance-based axis pick
        int64_t chVar[3];

        chVar[0] = 0;
        chVar[1] = 0;
        chVar[2] = 0;

        int32_t mr = bx->rSum / bx->weight;
        int32_t mg = bx->gSum / bx->weight;
        int32_t mb = bx->bSum / bx->weight;

        const uint8 *m = bx->members;

        for (int i = 0; i < bx->nMembers; i++)
        {
            int idx = m[0];
            m++;

            int dr = pal5r[idx] - mr;
            int dg = pal5g[idx] - mg;
            int db = pal5b[idx] - mb;
            int w = colorWeight[idx];

            chVar[0] += dr * dr * w;
            chVar[1] += dg * dg * w;
            chVar[2] += db * db * w;
        }

        int ranges[3];

        ranges[0] = bx->rMax - bx->rMin;
        ranges[1] = bx->gMax - bx->gMin;
        ranges[2] = bx->bMax - bx->bMin;

        int order[3] = {0, 1, 2};

        for (int a = 0; a < 2; a++)
        {
            for (int b = a + 1; b < 3; b++)
            {
                if (chVar[order[a]] < chVar[order[b]])
                {
                    int t = order[a];
                    
                    order[a] = order[b];
                    order[b] = t;
                }
            }
        }

        nSaved = bx->nMembers;
        
        memcpy(savedMembers, bx->members, nSaved);

        splitOk = 0;

        for (int attempt = 0; attempt < 3 && !splitOk; attempt++)
        {
            const uint8 *chan = NULL;
            ch = order[attempt];

            if (ranges[ch] <= 0)
                continue;

            // rebuild bx
            boxInit(bx);

            const uint8 *sv = savedMembers;

            for (int i = 0; i < nSaved; i++)
            {
                int idx = sv[0];
                sv++;

                boxAdd(bx, idx, pal5r[idx], pal5g[idx], pal5b[idx], colorWeight[idx]);
            }

            cutVal = findMedianCut(bx, ch, pal5r, pal5g, pal5b, colorWeight);

            boxInit(&newBox);
            boxInit(bx);

            chan = (ch == 0) ? pal5r : (ch == 1) ? pal5g : pal5b;

            const uint8 *sv2 = savedMembers;

            for (int i = 0; i < nSaved; i++)
            {
                int idx = sv2[0];
                sv2++;

                int r = pal5r[idx];
                int g = pal5g[idx];
                int b = pal5b[idx];
                int w = colorWeight[idx];

                if (chan[idx] <= cutVal)
                    boxAdd(bx, idx, r, g, b, w);
                else
                    boxAdd(&newBox, idx, r, g, b, w);
            }

            if (bx->weight > 0 && newBox.weight > 0)
                splitOk = 1;
        }

        if (splitOk)
        {
            boxes[nBoxes++] = newBox;
        }
        else
        {
            // bx dirty, rebuild
            boxInit(bx);

            const uint8 *sv = savedMembers;

            for (int i = 0; i < nSaved; i++)
            {
                int idx = sv[0];
                sv++;

                boxAdd(bx, idx, pal5r[idx], pal5g[idx], pal5b[idx], colorWeight[idx]);
            }

            unsplittable[worst] = 1;
        }
    }

    for (int i = 0; i < numColors; i++)
    {
        if (i < nBoxes && boxes[i].weight > 0)
        {
            int r = (int)(boxes[i].rSum / boxes[i].weight);
            int g = (int)(boxes[i].gSum / boxes[i].weight);
            int b = (int)(boxes[i].bSum / boxes[i].weight);

            outPalette[i] = packRGB12(r, g, b);
        }
        else
        {
            outPalette[i] = 0;
        }
    }
}
