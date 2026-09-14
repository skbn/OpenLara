#include "ecs_quant.h"
#include <string.h>
#include <stdint.h>

// Lloyd-Max per channel (https://en.wikipedia.org/wiki/Lloyd%27s_algorithm)

#ifdef QUANT_ASM
    extern "C"
    {
        void lloydMax_asm(int ch __asm("d0"), int nlev __asm("d1"), const uint8 *palette __asm("a0"), const int16_t *colorWeight __asm("a1"), uint8 *opt __asm("a2"));
        void lloyd3DRefine_asm(const uint8 *r5 __asm("a0"), const uint8 *g5 __asm("a1"), const uint8 *b5 __asm("a2"), const int16_t *cw __asm("a3"), int32_t *cr __asm("a4"), int32_t *cg, int32_t *cb __asm("a6"), int nColors __asm("d0"));
        void lloyd3DSeed_asm(const uint8 *r5 __asm("a0"), const uint8 *g5 __asm("a1"), const uint8 *b5 __asm("a2"), const int16_t *cw __asm("a3"), int32_t *cr __asm("a4"), int32_t *cg, int32_t *cb __asm("a6"), int32_t *dist, int nColors __asm("d0"));
    }

    #define lloydMax lloydMax_asm
#else
    #define lloydMax lloydMax_c
#endif

static void initLevels(int ch, int nlev, const uint8 *palette, const int16_t *colorWeight, uint8 *opt)
{
    int16_t hist[16];
    int16_t total = 0;
    int16_t cum = 0;
    int j = 0;
    int v;
    int nlev2 = nlev * 2;
    const uint8 *p = palette + ch;

    memset(hist, 0, sizeof(hist));

    for (int i = 0; i < 256; i++)
    {
        if (!colorWeight[i])
        {
            p += 3;
            continue;
        }

        v = p[0] >> 4;
        p += 3;

        hist[v] += colorWeight[i];
        total += colorWeight[i];
    }

    // seed at percentiles
    for (v = 0; v < 16 && j < nlev; v++)
    {
        cum += hist[v];

        int target = ((j * 2 + 1) * total) / nlev2;

        while (j < nlev && cum >= target)
        {
            opt[j++] = v;

            if (j < nlev)
                target = ((j * 2 + 1) * total) / nlev2;
        }
    }

    while (j < nlev)
        opt[j++] = 15;
}

void lloydMax_c(int ch, int nlev, const uint8 *palette, const int16_t *colorWeight, uint8 *opt)
{
    const uint8 *p = palette + ch;

    for (int iter = 0; iter < 8; iter++)
    {
        int32_t sums[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        int16_t counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        const uint8 *pp = p;
        const int16_t *cw = colorWeight;

        for (int i = 0; i < 256; i++)
        {
            int w = cw[0];
            cw++;

            if (!w)
            {
                pp += 3;
                continue;
            }

            int v = pp[0] >> 4;
            pp += 3;
            int best = 0;
            int bestDist = 256;

            for (int j = 0; j < nlev; j++)
            {
                int d = (v - opt[j]) * (v - opt[j]);

                if (d < bestDist)
                {
                    bestDist = d;
                    best = j;
                }
            }

            sums[best] += v * w;
            counts[best] += w;
        }

        for (int j = 0; j < nlev; j++)
        {
            if (counts[j] > 0)
                opt[j] = sums[j] / counts[j];
        }
    }
}

// r/g/b packed
static void buildPalette(const uint8 *optR, int nR, int sR, const uint8 *optG, int nG, int sG, const uint8 *optB, int nB, int sB, int numColors, UWORD *palette)
{
    for (int i = 0; i < numColors; i++)
    {
        uint8 r = optR[(i >> sR) & (nR - 1)];
        uint8 g = optG[(i >> sG) & (nG - 1)];
        uint8 b = optB[(i >> sB) & (nB - 1)];

        palette[i] = (r << 8) | (g << 4) | b;
    }
}

void ecsLloydQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette)
{
    // per-mode level counts and shifts
    static const struct
    {
        int nR;
        int sR;
        int nG;
        int sG;
        int nB;
        int sB;
    } cfg[3] =
    {
        {2, 3, 4, 1, 2, 0}, // 16 colors: 2x4x2
        {4, 3, 4, 1, 2, 0}, // 32 colors: 4x4x2
        {4, 4, 8, 1, 2, 0}, // 64 colors: 4x8x2
    };

    const int ci = (numColors == 16) ? 0 : (numColors == 32) ? 1 : 2;
    uint8 optR[8];
    uint8 optG[8];
    uint8 optB[2];

    initLevels(0, cfg[ci].nR, palette, colorWeight, optR);
    initLevels(1, cfg[ci].nG, palette, colorWeight, optG);
    initLevels(2, cfg[ci].nB, palette, colorWeight, optB);

    lloydMax(0, cfg[ci].nR, palette, colorWeight, optR);
    lloydMax(1, cfg[ci].nG, palette, colorWeight, optG);
    lloydMax(2, cfg[ci].nB, palette, colorWeight, optB);

    buildPalette(optR, cfg[ci].nR, cfg[ci].sR, optG, cfg[ci].nG, cfg[ci].sG, optB, cfg[ci].nB, cfg[ci].sB, numColors, outPalette);
}

static UWORD packRGB12(int r, int g, int b)
{
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;

    return ((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1);
}

void ecsLloyd3DQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette)
{
    uint8 r5[256];
    uint8 g5[256];
    uint8 b5[256];
    int32_t cr[64];
    int32_t cg[64];
    int32_t cb[64];
    int32_t cweight[64];
    int32_t csumR[64];
    int32_t csumG[64];
    int32_t csumB[64];
    int32_t dist[256];
    int nC = 0;
    int nC_max = numColors;
    int best;
    int bestW;

    if (nC_max < 1)
        nC_max = 32;

    if (nC_max > 64)
        nC_max = 64;

    for (int i = 0; i < 256; i++)
    {
        r5[i] = palette[0] >> 3;
        g5[i] = palette[1] >> 3;
        b5[i] = palette[2] >> 3;

        palette += 3;
    }

    best = 0;
    bestW = -1;

    for (int i = 0; i < 256; i++)
    {
        if (!colorWeight[i])
            continue;

        if (colorWeight[i] > bestW)
        {
            bestW = colorWeight[i];
            best = i;
        }
    }

    cr[0] = r5[best];
    cg[0] = g5[best];
    cb[0] = b5[best];

#ifdef QUANT_ASM
    lloyd3DSeed_asm(r5, g5, b5, colorWeight, cr, cg, cb, dist, nC_max);
#else

    nC = 1;

    for (int i = 0; i < 256; i++)
    {
        if (!colorWeight[i])
            continue;

        int dr = r5[i] - cr[0];
        int dg = g5[i] - cg[0];
        int db = b5[i] - cb[0];

        dist[i] = dr * dr * 30 + dg * dg * 59 + db * db * 11;
    }

    while (nC < nC_max)
    {
        int32_t bestScore = -1;

        best = -1;

        for (int i = 0; i < 256; i++)
        {
            if (!colorWeight[i])
                continue;

            int32_t score = dist[i] * colorWeight[i];

            if (score > bestScore)
            {
                bestScore = score;
                best = i;
            }
        }

        if (best < 0)
        {
            while (nC < nC_max)
            {
                cr[nC] = cr[nC - 1];
                cg[nC] = cg[nC - 1];
                cb[nC] = cb[nC - 1];
                nC++;
            }

            break;
        }

        cr[nC] = r5[best];
        cg[nC] = g5[best];
        cb[nC] = b5[best];
        nC++;

        for (int i = 0; i < 256; i++)
        {
            if (!colorWeight[i])
                continue;

            int dr = r5[i] - cr[nC - 1];
            int dg = g5[i] - cg[nC - 1];
            int db = b5[i] - cb[nC - 1];
            int32_t d = dr * dr * 30 + dg * dg * 59 + db * db * 11;

            if (d < dist[i]) dist[i] = d;
        }
    }
#endif

#ifdef QUANT_ASM
    lloyd3DRefine_asm(r5, g5, b5, colorWeight, cr, cg, cb, nC_max);
#else

    for (int iter = 0; iter < 8; iter++)
    {
        memset(cweight, 0, nC_max * sizeof(int32_t));
        memset(csumR, 0, nC_max * sizeof(int32_t));
        memset(csumG, 0, nC_max * sizeof(int32_t));
        memset(csumB, 0, nC_max * sizeof(int32_t));

        for (int i = 0; i < 256; i++)
        {
            int w = colorWeight[i];

            if (!w)
                continue;

            int r = r5[i];
            int g = g5[i];
            int b = b5[i];
            int bestJ = 0;
            int32_t bestDist = 0x7FFFFFFF;

            for (int j = 0; j < nC_max; j++)
            {
                int dr = r - cr[j];
                int dg = g - cg[j];
                int db = b - cb[j];
                int32_t d = dr * dr * 30 + dg * dg * 59 + db * db * 11;

                if (d < bestDist)
                {
                    bestDist = d;
                    bestJ = j;
                }
            }

            csumR[bestJ] += r * w;
            csumG[bestJ] += g * w;
            csumB[bestJ] += b * w;
            cweight[bestJ] += w;
        }

        for (int j = 0; j < nC_max; j++)
        {
            if (cweight[j] > 0)
            {
                cr[j] = csumR[j] / cweight[j];
                cg[j] = csumG[j] / cweight[j];
                cb[j] = csumB[j] / cweight[j];
            }
        }
    }
#endif

    for (int i = 0; i < nC_max; i++)
        outPalette[i] = packRGB12(cr[i], cg[i], cb[i]);

    outPalette[0] = 0x000;
}
