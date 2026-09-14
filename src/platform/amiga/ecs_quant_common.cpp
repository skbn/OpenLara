#include "ecs_quant.h"
#include <string.h>
#include <stdint.h>

UWORD ecsPalette[64];
uint8 ecsRemap[256];
uint8 origLightmap[256 * 32];
uint8 lightmapNeedsSave = 1;
uint8 ecsDepth = 0;
uint8 numColors = 0;
QuantMethod quantMethod = QUANT_LLOYD;
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

void ecsRemapLightmap_c(uint8 depth, const uint8 *orig, uint8 *lightmap, const uint8 *remap)
{
    int16_t firstIdx[256];

    if (depth <= 0 || depth > 8)
        return;

    numColors = 1 << depth;

    // shadow table at 0x1A00
    for (int i = 0; i < numColors; i++)
        firstIdx[i] = -1;

    for (int j = 0; j < 256; j++)
    {
        int idx = remap[j];

        if (idx < numColors && firstIdx[idx] < 0)
            firstIdx[idx] = j;
    }

    for (int i = 0; i < 256 * 32; i++)
        lightmap[i] = remap[orig[i]];

    for (int i = 0; i < numColors; i++)
    {
        int j = firstIdx[i];

        if (j >= 0)
            lightmap[0x1A00 + i] = remap[orig[0x1A00 + j]];
    }
}
