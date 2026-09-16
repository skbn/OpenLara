#include "ecs_quant.h"
#include <string.h>
#include <stdint.h>

struct QuantEntry
{
    int luma;
    int sat;
    int nr;
    int ng;
    int nb;
};

static QuantEntry qent[64];

UWORD ecsPalette[64];
uint8 ecsRemap[256];
uint8 origLightmap[256 * 32];
uint8 *gFillmap;
uint8 gInvQuant[256];
uint8 lightmapNeedsSave = 1;
uint8 ecsDepth = 0;
uint8 numColors = 0;
QuantMethod quantMethod = QUANT_LLOYD3D;
uint8 *paletteCurrent = NULL;
uint8 *gLightmap = NULL;

static uint32_t sourceWeight[256];
static uint32_t outputWeight[256];

void ecsComputeColorWeights(const uint8 *lightmap, const uint8 *tiles, int tilesCount, int16_t colorWeight[256])
{
    int tile;
    int pixel;
    int color;
    int shade;
    int shift;
    uint32_t maxWeight;
    uint32_t totalWeight;
    uint32_t divisor;
    uint32_t weight;

    memset(sourceWeight, 0, sizeof(sourceWeight));

    for (tile = 0; tile < tilesCount; tile++)
    {
        for (pixel = 0; pixel < 256 * 256; pixel++)
            sourceWeight[*tiles++]++;
    }

    maxWeight = 0;

    for (color = 0; color < 256; color++)
    {
        if (sourceWeight[color] > maxWeight)
            maxWeight = sourceWeight[color];
    }

    shift = 0;

    while (maxWeight > 1024)
    {
        maxWeight >>= 1;
        shift++;
    }

    for (color = 0; color < 256; color++)
    {
        sourceWeight[color] >>= shift;

        if (sourceWeight[color] == 0)
            sourceWeight[color] = 1;
    }

    memset(outputWeight, 0, sizeof(outputWeight));

    for (shade = 0; shade < 32; shade++)
    {
        for (color = 0; color < 256; color++)
            outputWeight[lightmap[(shade << 8) | color]] += sourceWeight[color];
    }

    totalWeight = 0;

    for (color = 0; color < 256; color++)
        totalWeight += outputWeight[color];

    divisor = (totalWeight + 16383) / 16384;

    if (divisor == 0)
        divisor = 1;

    for (color = 0; color < 256; color++)
    {
        weight = outputWeight[color] / divisor;

        if (weight == 0 && outputWeight[color] != 0)
            weight = 1;

        colorWeight[color] = weight;
    }
}

void ecsBuildRemap_c(const uint8 *palette, uint8 *remap)
{
    uint8 pr[64];
    uint8 pg[64];
    uint8 pb[64];
    uint8 pal4r[256];
    uint8 pal4g[256];
    uint8 pal4b[256];

    for (int j = 0; j < numColors; j++)
    {
        pr[j] = (ecsPalette[j] >> 8) & 15;
        pg[j] = (ecsPalette[j] >> 4) & 15;
        pb[j] = ecsPalette[j] & 15;
    }

    for (int i = 0; i < 256; i++)
    {
        pal4r[i] = palette[0] >> 4;
        pal4g[i] = palette[1] >> 4;
        pal4b[i] = palette[2] >> 4;

        palette += 3;
    }

    for (int i = 0; i < 256; i++)
    {
        int r = pal4r[i];
        int g = pal4g[i];
        int b = pal4b[i];
        int bestDist = 0x7FFFFFFF;
        int best = 0;

        for (int j = 0; j < numColors; j++)
        {
            int dr = r - pr[j];
            int dg = g - pg[j];
            int db = b - pb[j];
            int dist = dr * dr * 30 + dg * dg * 59 + db * db * 11;

            if (dist < bestDist)
            {
                bestDist = dist;
                best = j;
            }

            if (bestDist == 0)
                break;
        }

        remap[i] = best;
    }
}

static void ecsChromaDir(int r, int g, int b, int *nr, int *ng, int *nb)
{
    int sum = r + g + b;
    int dr = 3 * r - sum;
    int dg = 3 * g - sum;
    int db = 3 * b - sum;
    int mx = r;
    int mn = r;

    if (g > mx)
        mx = g;

    if (b > mx)
        mx = b;

    if (g < mn)
        mn = g;

    if (b < mn)
        mn = b;

    int sat = mx - mn;

    if (sat <= 0)
    {
        *nr = 0;
        *ng = 0;
        *nb = 0;
        return;
    }

    *nr = (dr * 64) / sat;
    *ng = (dg * 64) / sat;
    *nb = (db * 64) / sat;
}

static int ecsLuma4(int r, int g, int b)
{
    return 30 * r + 59 * g + 11 * b;
}

static int ecsBuildFamily(int ar, int ag, int ab, uint8 *fam)
{
    int anr;
    int ang;
    int anb;
    int n = 0;
    int darkestLuma;
    int asat;
    int mn = ar;
    int mx = ar;
    int aLuma;
    int i;

    ecsChromaDir(ar, ag, ab, &anr, &ang, &anb);

    aLuma = ecsLuma4(ar, ag, ab);

    if (ag > mx)
        mx = ag;

    if (ab > mx)
        mx = ab;

    if (ag < mn)
        mn = ag;

    if (ab < mn)
        mn = ab;

    asat = mx - mn;

    if (asat <= 2)
    {
        for (i = 0; i < numColors; i++)
        {
            if (qent[i].sat <= 2)
                fam[n++] = i;
        }
    }
    else
    {
        darkestLuma = 0x7FFFFFFF;

        for (i = 0; i < numColors; i++)
        {
            int dist;

            if (qent[i].sat <= 2)
                continue;

            dist = (anr > qent[i].nr ? anr - qent[i].nr : qent[i].nr - anr) + (ang > qent[i].ng ? ang - qent[i].ng : qent[i].ng - ang) + (anb > qent[i].nb ? anb - qent[i].nb : qent[i].nb - anb);

            if (dist <= 48)
            {
                if (qent[i].luma < darkestLuma)
                    darkestLuma = qent[i].luma;

                fam[n++] = i;
            }
        }

        if (darkestLuma == 0x7FFFFFFF)
            darkestLuma = aLuma;

        for (i = 0; i < numColors; i++)
        {
            if (qent[i].sat <= 2 && qent[i].luma <= darkestLuma)
                fam[n++] = i;
        }
    }

    if (n == 0)
    {
        int bestDist = 0x7FFFFFFF;
        int best = 0;

        for (i = 0; i < numColors; i++)
        {
            int dr = ar - ((ecsPalette[i] >> 8) & 15);
            int dg = ag - ((ecsPalette[i] >> 4) & 15);
            int db = ab - (ecsPalette[i] & 15);
            int dist = dr * dr * 30 + dg * dg * 59 + db * db * 11;

            if (dist < bestDist)
            {
                bestDist = dist;
                best = i;
            }
        }

        fam[n++] = best;
    }

    return n;
}

static int ecsPickStep(const uint8 *fam, int n, int r, int g, int b, int capLuma, int prevIdx)
{
    int bestDist = 0x7FFFFFFF;
    int best = -1;
    int prevDist = 0x7FFFFFFF;
    int minLuma = 0x7FFFFFFF;
    int minIdx = fam[0];
    int i;

    for (i = 0; i < n; i++)
    {
        int j = fam[i];

        if (qent[j].luma < minLuma)
        {
            minLuma = qent[j].luma;
            minIdx = j;
        }

        if (qent[j].luma > capLuma)
            continue;

        int qr = (ecsPalette[j] >> 8) & 15;
        int qg = (ecsPalette[j] >> 4) & 15;
        int qb = ecsPalette[j] & 15;
        int dr = r - qr;
        int dg = g - qg;
        int db = b - qb;
        int dist = dr * dr * 30 + dg * dg * 59 + db * db * 11;

        if (j == prevIdx)
            prevDist = dist;

        if (dist < bestDist)
        {
            bestDist = dist;
            best = j;
        }
    }

    if (best < 0)
        return minIdx;

    if (prevIdx >= 0 && prevDist < 0x7FFFFFFF && prevDist <= bestDist * 2)
        return prevIdx;

    return best;
}

void ecsRemapLightmap_c(uint8 depth, const uint8 *orig, uint8 *lightmap, const uint8 *remap)
{
    int16_t firstIdx[256];
    uint8 fam[64];
    int i;
    int j;
    int c;
    int s;

    if (depth <= 0 || depth > 8)
        return;

    numColors = 1 << depth;

    if (numColors > 64)
    {
        for (i = 0; i < 256 * 32; i++)
            lightmap[i] = remap[orig[i]];

        return;
    }

    for (i = 0; i < numColors; i++)
    {
        int r = (ecsPalette[i] >> 8) & 15;
        int g = (ecsPalette[i] >> 4) & 15;
        int b = ecsPalette[i] & 15;
        int mx = r;
        int mn = r;

        if (g > mx)
            mx = g;

        if (b > mx)
            mx = b;

        if (g < mn)
            mn = g;

        if (b < mn)
            mn = b;

        qent[i].luma = ecsLuma4(r, g, b);
        qent[i].sat = mx - mn;

        ecsChromaDir(r, g, b, &qent[i].nr, &qent[i].ng, &qent[i].nb);
    }

    for (c = 0; c < 256; c++)
    {
        const uint8 *src = paletteCurrent + c * 3;
        int ar = src[0] >> 4;
        int ag = src[1] >> 4;
        int ab = src[2] >> 4;
        int fn = ecsBuildFamily(ar, ag, ab, fam);
        int capLuma = 0x7FFFFFFF;
        int prevIdx = -1;

        for (s = 0; s < 32; s++)
        {
            const uint8 *tc = paletteCurrent + orig[(s << 8) | c] * 3;
            int idx = ecsPickStep(fam, fn, tc[0] >> 4, tc[1] >> 4, tc[2] >> 4, capLuma, prevIdx);

            lightmap[(s << 8) | c] = idx;
            capLuma = qent[idx].luma;
            prevIdx = idx;
        }
    }

    for (i = 0; i < numColors; i++)
        firstIdx[i] = -1;

    for (j = 0; j < 256; j++)
    {
        int idx = remap[j];

        if (idx < numColors && firstIdx[idx] < 0)
            firstIdx[idx] = j;
    }

    for (i = 0; i < numColors; i++)
    {
        int r = (ecsPalette[i] >> 8) & 15;
        int g = (ecsPalette[i] >> 4) & 15;
        int b = ecsPalette[i] & 15;
        int fn;
        const uint8 *tc;

        j = firstIdx[i];

        if (j < 0)
            continue;

        fn = ecsBuildFamily(r, g, b, fam);

        tc = paletteCurrent + orig[0x1A00 + j] * 3;

        lightmap[0x1A00 + i] = ecsPickStep(fam, fn, tc[0] >> 4, tc[1] >> 4, tc[2] >> 4, 0x7FFFFFFF, -1);
    }
}

void ecsbuildFillmap_c()
{
    uint8 pal4r[256];
    uint8 pal4g[256];
    uint8 pal4b[256];

    for (int i = 0; i < 256; i++)
    {
        pal4r[i] = paletteCurrent[i * 3 + 0] >> 4;
        pal4g[i] = paletteCurrent[i * 3 + 1] >> 4;
        pal4b[i] = paletteCurrent[i * 3 + 2] >> 4;
    }

    for (int q = 0; q < 256; q++)
    {
        if (q >= numColors)
        {
            gInvQuant[q] = q;
            continue;
        }

        int qr = (ecsPalette[q] >> 8) & 15;
        int qg = (ecsPalette[q] >> 4) & 15;
        int qb = ecsPalette[q] & 15;
        int best = 0;
        int bestDist = 0x7FFFFFFF;

        for (int i = 0; i < 256; i++)
        {
            int dr = pal4r[i] - qr;
            int dg = pal4g[i] - qg;
            int db = pal4b[i] - qb;
            int dist = dr * dr * 30 + dg * dg * 59 + db * db * 11;

            if (dist < bestDist)
            {
                bestDist = dist;
                best = i;
            }

            if (bestDist == 0)
                break;
        }

        gInvQuant[q] = best;
    }

    for (int s = 0; s < 32; s++)
    {
        const uint8 *src = gLightmap + (s << 8);
        uint8 *dst = gFillmapData + (s << 8);

        for (int q = 0; q < 256; q++)
            dst[q] = (q < numColors) ? src[gInvQuant[q]] : q;
    }

    gFillmap = gFillmapData;
}
