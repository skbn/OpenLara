#ifndef AMIGA_SCALERS_H
#define AMIGA_SCALERS_H

#define FILTER_NEAREST 0
#define FILTER_SCALE2X 1
#define FILTER_NONE 2

void scaleNearest2x_c(const uint8 *src, uint8 *dst, int displayWidth);
void scale2x_c(const uint8 *src, uint8 *dst, int displayWidth);

#ifdef USE_ASM
extern "C"
{
    void scaleNearest2x_asm(const uint8 *src __asm("a0"), uint8 *dst __asm("a1"), int displayWidth __asm("d0"));
    void scale2x_asm(const uint8 *src __asm("a0"), uint8 *dst __asm("a1"), int displayWidth __asm("d0"));
}

    #define scaleNearest2x scaleNearest2x_asm
    #define scale2x scale2x_asm
#else
    #define scaleNearest2x scaleNearest2x_c
    #define scale2x scale2x_c
#endif

#endif
