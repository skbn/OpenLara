#ifndef ECS_QUANT_H
#define ECS_QUANT_H

#include <exec/types.h>
#include <stdint.h>

typedef unsigned char uint8;

enum QuantMethod
{
    QUANT_LLOYD,
    QUANT_LLOYD3D,
    QUANT_WU,
    QUANT_MEDIAN
};

extern UWORD ecsPalette[64];
extern uint8 ecsRemap[256];
extern uint8 origLightmap[256 * 32];
extern uint8 lightmapNeedsSave;
extern uint8 ecsDepth;
extern uint8 numColors;
extern QuantMethod quantMethod;
extern uint8 *paletteCurrent;
extern uint8 *gLightmap;

#ifdef QUANT_ASM
    extern "C"
    {
        void ecsBuildRemap_asm(const uint8 *palette __asm("a0"), uint8 *remap __asm("a1"));
        void ecsRemapLightmap_asm(uint8 depth __asm("d0"), const uint8 *orig __asm("a0"), uint8 *lightmap __asm("a1"), const uint8 *remap __asm("a2"));
    }

    #define ecsBuildRemap ecsBuildRemap_asm
    #define ecsRemapLightmap ecsRemapLightmap_asm
#else
    #define ecsBuildRemap ecsBuildRemap_c
    #define ecsRemapLightmap ecsRemapLightmap_c
#endif

void ecsComputeColorWeights(const uint8 *lightmap, int16_t colorWeight[256]);

void ecsBuildRemap_c(const uint8 *palette, uint8 *remap);
void ecsRemapLightmap_c(uint8 depth, const uint8 *orig, uint8 *lightmap, const uint8 *remap);

void ecsLloydQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette);
void ecsLloyd3DQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette);
void ecsMedianQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette);
void ecsWuQuant(const uint8 *palette, const int16_t *colorWeight, UWORD *outPalette);

#endif
