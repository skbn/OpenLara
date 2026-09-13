#include "ecs_quant.h"
#include <string.h>
#include <stdint.h>

// Wu quant (https://www.ece.mcmaster.ca/~xwu/cq.c)

typedef struct
{
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;
    int16_t weight;
    int32_t error;
    int32_t rSum;
    int32_t gSum;
    int32_t bSum;
} WuBox;

// 5-bit per channel
int32_t wuW[33][33][33];
int32_t wuR[33][33][33];
int32_t wuG[33][33][33];
int32_t wuB[33][33][33];
int32_t wuR2[33][33][33];
int32_t wuG2[33][33][33];
int32_t wuB2[33][33][33];

#define WU_BOX_SUM32(p, r1, r2, g1, g2, b1, b2) \
    (p[(r2)+1][(g2)+1][(b2)+1] \
   - p[(r1)][(g2)+1][(b2)+1] \
   - p[(r2)+1][(g1)][(b2)+1] \
   - p[(r2)+1][(g2)+1][(b1)] \
   + p[(r1)][(g1)][(b2)+1] \
   + p[(r1)][(g2)+1][(b1)] \
   + p[(r2)+1][(g1)][(b1)] \
   - p[(r1)][(g1)][(b1)])

#ifdef QUANT_ASM
    extern "C"
    {
        void wuComputeMoments_asm(const uint8 *palette __asm("a0"), const int16_t *colorWeight __asm("a1"));
        int32_t wuBoxError_asm(int r1 __asm("d0"), int r2 __asm("d1"), int g1 __asm("d2"), int g2 __asm("d3"), int b1 __asm("d4"), int b2 __asm("d5"), int *outW __asm("a0"), int32_t *outRs __asm("a1"), int32_t *outGs __asm("a2"), int32_t *outBs __asm("a3"));
        int32_t wuCutSearch_asm(int r1 __asm("d0"), int r2 __asm("d1"), int g1 __asm("d2"), int g2 __asm("d3"), int b1 __asm("d4"), int b2 __asm("d5"), int32_t boxError __asm("d6"), int *outCh __asm("a0"), int *outCut __asm("a1"));
    }
    
    #define wuComputeMoments wuComputeMoments_asm
    #define wuBoxError wuBoxError_asm
#else
    #define wuComputeMoments wuComputeMoments_c
    #define wuBoxError wuBoxError_c
#endif

static int32_t boxSumFlat(const int32_t *p, int o1, int o2, int o3, int o4, int o5, int o6, int o7, int o8)
{
    return p[o1] - p[o2] - p[o3] - p[o4] + p[o5] + p[o6] + p[o7] - p[o8];
}

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

void wuComputeMoments_c(const uint8 *palette, const int16_t *colorWeight)
{
    const uint8 *p = palette;
    const int16_t *cw = colorWeight;
    int32_t *wW = &wuW[0][0][0];
    int32_t *wR = &wuR[0][0][0];
    int32_t *wG = &wuG[0][0][0];
    int32_t *wB = &wuB[0][0][0];
    int32_t *wR2 = &wuR2[0][0][0];
    int32_t *wG2 = &wuG2[0][0][0];
    int32_t *wB2 = &wuB2[0][0][0];

    memset(wuW, 0, sizeof(wuW));
    memset(wuR, 0, sizeof(wuR));
    memset(wuG, 0, sizeof(wuG));
    memset(wuB, 0, sizeof(wuB));
    memset(wuR2, 0, sizeof(wuR2));
    memset(wuG2, 0, sizeof(wuG2));
    memset(wuB2, 0, sizeof(wuB2));

    for (int i = 0; i < 256; i++)
    {
        int w = cw[0];
        
        cw++;

        if (!w)
        {
            p += 3;
            continue;
        }

        int r = p[0] >> 3;
        int g = p[1] >> 3;
        int b = p[2] >> 3;
        p += 3;

        int r2 = r * r;
        int g2 = g * g;
        int b2 = b * b;
        int idx = (r + 1) * 1089 + (g + 1) * 33 + (b + 1);

        wW[idx] += w;
        wR[idx] += r * w;
        wG[idx] += g * w;
        wB[idx] += b * w;
        wR2[idx] += r2 * w;
        wG2[idx] += g2 * w;
        wB2[idx] += b2 * w;
    }

    // prefix sums
    for (int r = 0; r <= 32; r++)
    {
        for (int g = 0; g <= 32; g++)
        {
            for (int b = 1; b <= 32; b++)
            {
                wuW[r][g][b] += wuW[r][g][b - 1];

                wuR[r][g][b] += wuR[r][g][b - 1];
                wuG[r][g][b] += wuG[r][g][b - 1];
                wuB[r][g][b] += wuB[r][g][b - 1];

                wuR2[r][g][b] += wuR2[r][g][b - 1];
                wuG2[r][g][b] += wuG2[r][g][b - 1];
                wuB2[r][g][b] += wuB2[r][g][b - 1];
            }
        }
    }

    for (int r = 0; r <= 32; r++)
    {
        for (int g = 1; g <= 32; g++)
        {
            for (int b = 0; b <= 32; b++)
            {
                wuW[r][g][b] += wuW[r][g - 1][b];

                wuR[r][g][b] += wuR[r][g - 1][b];
                wuG[r][g][b] += wuG[r][g - 1][b];
                wuB[r][g][b] += wuB[r][g - 1][b];

                wuR2[r][g][b] += wuR2[r][g - 1][b];
                wuG2[r][g][b] += wuG2[r][g - 1][b];
                wuB2[r][g][b] += wuB2[r][g - 1][b];
            }
        }
    }

    for (int r = 1; r <= 32; r++)
    {
        for (int g = 0; g <= 32; g++)
        {
            for (int b = 0; b <= 32; b++)
            {
                wuW[r][g][b] += wuW[r - 1][g][b];
                
                wuR[r][g][b] += wuR[r - 1][g][b];
                wuG[r][g][b] += wuG[r - 1][g][b];
                wuB[r][g][b] += wuB[r - 1][g][b];

                wuR2[r][g][b] += wuR2[r - 1][g][b];
                wuG2[r][g][b] += wuG2[r - 1][g][b];
                wuB2[r][g][b] += wuB2[r - 1][g][b];
            }
        }
    }
}

int32_t wuBoxError_c(int r1, int r2, int g1, int g2, int b1, int b2, int *outW, int32_t *outRs, int32_t *outGs, int32_t *outBs)
{
    // shared flat offsets
    int R2 = (r2 + 1) * 1089;
    int R1 = r1 * 1089;
    int G2 = (g2 + 1) * 33;
    int G1 = g1 * 33;
    int B2 = b2 + 1;
    int B1 = b1;

    int o1 = R2 + G2 + B2;
    int o2 = R1 + G2 + B2;
    int o3 = R2 + G1 + B2;
    int o4 = R2 + G2 + B1;
    int o5 = R1 + G1 + B2;
    int o6 = R1 + G2 + B1;
    int o7 = R2 + G1 + B1;
    int o8 = R1 + G1 + B1;

    int32_t *wW = &wuW[0][0][0];
    int32_t *wR = &wuR[0][0][0];
    int32_t *wG = &wuG[0][0][0];
    int32_t *wB = &wuB[0][0][0];

    int w = boxSumFlat(wW, o1, o2, o3, o4, o5, o6, o7, o8);
    int32_t rs = boxSumFlat(wR, o1, o2, o3, o4, o5, o6, o7, o8);
    int32_t gs = boxSumFlat(wG, o1, o2, o3, o4, o5, o6, o7, o8);
    int32_t bs = boxSumFlat(wB, o1, o2, o3, o4, o5, o6, o7, o8);

    if (outW)
    {
        *outW = w;
        *outRs = rs;
        *outGs = gs;
        *outBs = bs;
    }

    if (w == 0)
        return 0;

    int32_t *wR2 = &wuR2[0][0][0];
    int32_t *wG2 = &wuG2[0][0][0];
    int32_t *wB2 = &wuB2[0][0][0];

    int32_t rsq = boxSumFlat(wR2, o1, o2, o3, o4, o5, o6, o7, o8);
    int32_t gsq = boxSumFlat(wG2, o1, o2, o3, o4, o5, o6, o7, o8);
    int32_t bsq = boxSumFlat(wB2, o1, o2, o3, o4, o5, o6, o7, o8);

    int32_t mr = rs / w;
    int32_t mg = gs / w;
    int32_t mb = bs / w;

    return rsq + gsq + bsq - (mr * rs + mg * gs + mb * bs);
}

static void wuBoxStats(WuBox *box)
{
    int w;
    int32_t rs;
    int32_t gs;
    int32_t bs;

    box->error = wuBoxError(box->r1, box->r2, box->g1, box->g2, box->b1, box->b2, &w, &rs, &gs, &bs);
    box->weight = w;
    box->rSum = rs;
    box->gSum = gs;
    box->bSum = bs;
}

static int32_t wuTryCut(WuBox *bx, int ch, int cut, int *outCut)
{
    int32_t e1;
    int32_t e2;
    int32_t reduce;

    if (ch == 0)
    {
        e1 = wuBoxError(bx->r1, cut - 1, bx->g1, bx->g2, bx->b1, bx->b2, NULL, NULL, NULL, NULL);
        e2 = wuBoxError(cut, bx->r2, bx->g1, bx->g2, bx->b1, bx->b2, NULL, NULL, NULL, NULL);
    }
    else if (ch == 1)
    {
        e1 = wuBoxError(bx->r1, bx->r2, bx->g1, cut - 1, bx->b1, bx->b2, NULL, NULL, NULL, NULL);
        e2 = wuBoxError(bx->r1, bx->r2, cut, bx->g2, bx->b1, bx->b2, NULL, NULL, NULL, NULL);
    }
    else
    {
        e1 = wuBoxError(bx->r1, bx->r2, bx->g1, bx->g2, bx->b1, cut - 1, NULL, NULL, NULL, NULL);
        e2 = wuBoxError(bx->r1, bx->r2, bx->g1, bx->g2, cut, bx->b2, NULL, NULL, NULL, NULL);
    }

    reduce = bx->error - e1 - e2;

    if (reduce > 0 && outCut)
        *outCut = cut;

    return reduce;
}

void ecsWuQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette)
{
    WuBox boxes[64];
    int nBoxes = 1;

#ifdef QUANT_ASM
    int bestLocalCh = 0;
    int bestLocalCut = 0;
#endif

    wuComputeMoments(palette, colorWeight);

    boxes[0].r1 = 0;
    boxes[0].r2 = 31;
    boxes[0].g1 = 0;
    boxes[0].g2 = 31;
    boxes[0].b1 = 0;
    boxes[0].b2 = 31;

    wuBoxStats(&boxes[0]);

    while (nBoxes < numColors)
    {
        int worst = -1;
        int32_t worstErr = -1;
        WuBox *bx = NULL;
        int bestCh = 0;
        int bestCut = 0;
        int32_t bestReduce = 0;
        WuBox newBox;

        for (int i = 0; i < nBoxes; i++)
        {
            if (boxes[i].weight == 0)
                continue;

            if (boxes[i].error > worstErr)
            {
                worstErr = boxes[i].error;
                worst = i;
            }
        }

        if (worst < 0)
            break;

        bx = &boxes[worst];

#ifdef QUANT_ASM
        bestReduce = wuCutSearch_asm(bx->r1, bx->r2, bx->g1, bx->g2, bx->b1, bx->b2, bx->error, &bestLocalCh, &bestLocalCut);
        bestCh = bestLocalCh;
        bestCut = bestLocalCut;
#else

        for (int ch = 0; ch < 3; ch++)
        {
            int lo;
            int hi;
            int bestLocalCut = 0;
            int32_t bestLocalReduce = 0;

            if (ch == 0)
            {
                lo = bx->r1;
                hi = bx->r2;
            }
            else if (ch == 1)
            {
                lo = bx->g1;
                hi = bx->g2;
            }
            else
            {
                lo = bx->b1;
                hi = bx->b2;
            }

            if (hi <= lo)
                continue;

            for (int cut = lo + 1; cut <= hi; cut++)
            {
                int cutVal = 0;
                int32_t reduce = wuTryCut(bx, ch, cut, &cutVal);

                if (reduce > bestLocalReduce)
                {
                    bestLocalReduce = reduce;
                    bestLocalCut = cutVal;
                }
            }

            if (bestLocalReduce > bestReduce)
            {
                bestReduce = bestLocalReduce;
                bestCh = ch;
                bestCut = bestLocalCut;
            }
        }
#endif

        // fallback: split heaviest box
        if (bestReduce <= 0)
        {
            int splitOk = 0;
            int32_t bestW = 0;

            for (int i = 0; i < nBoxes; i++)
            {
                int lo = -1;
                int hi = -1;
                int splitCh = -1;

                if (boxes[i].weight <= 0)
                    continue;

                if (boxes[i].r2 > boxes[i].r1)
                {
                    lo = boxes[i].r1;
                    hi = boxes[i].r2;

                    splitCh = 0;
                }
                else if (boxes[i].g2 > boxes[i].g1)
                {
                    lo = boxes[i].g1;
                    hi = boxes[i].g2;

                    splitCh = 1;
                }
                else if (boxes[i].b2 > boxes[i].b1)
                {
                    lo = boxes[i].b1;
                    hi = boxes[i].b2;
                    
                    splitCh = 2;
                }

                if (splitCh < 0)
                    continue;

                int cut = (lo + hi + 1) >> 1;
                int w1;
                int w2;

                if (splitCh == 0)
                {
                    w1 = WU_BOX_SUM32(wuW, boxes[i].r1, cut - 1, boxes[i].g1, boxes[i].g2, boxes[i].b1, boxes[i].b2);
                    w2 = WU_BOX_SUM32(wuW, cut, boxes[i].r2, boxes[i].g1, boxes[i].g2, boxes[i].b1, boxes[i].b2);
                }
                else if (splitCh == 1)
                {
                    w1 = WU_BOX_SUM32(wuW, boxes[i].r1, boxes[i].r2, boxes[i].g1, cut - 1, boxes[i].b1, boxes[i].b2);
                    w2 = WU_BOX_SUM32(wuW, boxes[i].r1, boxes[i].r2, cut, boxes[i].g2, boxes[i].b1, boxes[i].b2);
                }
                else
                {
                    w1 = WU_BOX_SUM32(wuW, boxes[i].r1, boxes[i].r2, boxes[i].g1, boxes[i].g2, boxes[i].b1, cut - 1);
                    w2 = WU_BOX_SUM32(wuW, boxes[i].r1, boxes[i].r2, boxes[i].g1, boxes[i].g2, cut, boxes[i].b2);
                }

                if (w1 <= 0 || w2 <= 0)
                    continue;

                if (boxes[i].weight > bestW)
                {
                    bestW = boxes[i].weight;
                    bestCh = splitCh;
                    bestCut = cut;
                    worst = i;
                    bx = &boxes[i];
                    splitOk = 1;
                }
            }

            if (!splitOk)
                break;
        }

        newBox = *bx;

        if (bestCh == 0)
        {
            bx->r2 = bestCut - 1;
            newBox.r1 = bestCut;
        }
        else if (bestCh == 1)
        {
            bx->g2 = bestCut - 1;
            newBox.g1 = bestCut;
        }
        else
        {
            bx->b2 = bestCut - 1;
            newBox.b1 = bestCut;
        }

        wuBoxStats(bx);
        wuBoxStats(&newBox);

        boxes[nBoxes++] = newBox;
    }

    // heaviest box for fill
    int bestIdx = 0;
    int32_t bestW = 0;

    for (int j = 0; j < nBoxes; j++)
    {
        if (boxes[j].weight > bestW)
        {
            bestW = boxes[j].weight;
            bestIdx = j;
        }
    }

    UWORD fill = 0;

    if (bestW > 0)
    {
        int r = (int)(boxes[bestIdx].rSum / boxes[bestIdx].weight);
        int g = (int)(boxes[bestIdx].gSum / boxes[bestIdx].weight);
        int b = (int)(boxes[bestIdx].bSum / boxes[bestIdx].weight);

        fill = packRGB12(r, g, b);
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
            outPalette[i] = fill;
    }
}
