#include "common.h"

#define Object AmiObject
#include <exec/types.h>
#undef Object

#include "amiga_scalers.h"

void scaleNearest2x_c(const uint8 *src, uint8 *dst, int displayWidth)
{
    int stride = displayWidth;

    for (int y = 0; y < FRAME_HEIGHT; y++)
    {
        const uint8 *srcP = src + y * FRAME_WIDTH;
        uint8 *d0 = dst + (y * 2) * stride;
        uint8 *d1 = d0 + stride;

        for (int x = 0; x < FRAME_WIDTH; x += 4)
        {
            uint8 p0 = *srcP++;
            uint8 p1 = *srcP++;
            uint8 p2 = *srcP++;
            uint8 p3 = *srcP++;

            uint32 w0 = ((uint32)p0 << 24) | ((uint32)p0 << 16) | ((uint32)p1 << 8) | p1;
            uint32 w1 = ((uint32)p2 << 24) | ((uint32)p2 << 16) | ((uint32)p3 << 8) | p3;

            *(uint32 *)d0 = w0;
            d0 += 4;
            *(uint32 *)d0 = w1;
            d0 += 4;
            *(uint32 *)d1 = w0;
            d1 += 4;
            *(uint32 *)d1 = w1;
            d1 += 4;
        }
    }
}

X_INLINE static void scale2x_pixel(uint8 b, uint8 d, uint8 e, uint8 f, uint8 h, uint8 *&o0, uint8 *&o1)
{
    uint8 e0 = e;
    uint8 e1 = e;
    uint8 e2 = e;
    uint8 e3 = e;

    if (b != h && d != f)
    {
        e0 = (d == b) ? d : e;
        e1 = (b == f) ? f : e;
        e2 = (d == h) ? d : e;
        e3 = (h == f) ? f : e;
    }

    *(uint16 *)o0 = (e0 << 8) | e1;
    o0 += 2;
    *(uint16 *)o1 = (e2 << 8) | e3;
    o1 += 2;
}

void scale2x_c(const uint8 *src, uint8 *dst, int displayWidth)
{
    int stride = displayWidth;

    for (int y = 0; y < FRAME_HEIGHT; y++)
    {
        const uint8 *rowUp = src + (y - (y > 0)) * FRAME_WIDTH;
        const uint8 *rowMid = src + y * FRAME_WIDTH;
        const uint8 *rowDn = src + (y + (y < FRAME_HEIGHT - 1)) * FRAME_WIDTH;
        uint8 *d0 = dst + (y * 2) * stride;
        uint8 *d1 = d0 + stride;

        uint8 e = rowMid[0];
        uint8 b = rowUp[0];
        uint8 d;
        uint8 f = rowMid[1];
        uint8 h = rowDn[0];
        uint8 *p0 = d0;
        uint8 *p1 = d1;

        scale2x_pixel(b, e, e, f, h, p0, p1);

        const uint8 *up = rowUp + 1;
        const uint8 *mid = rowMid + 1;
        const uint8 *dn = rowDn + 1;
        uint8 *o0 = d0 + 2;
        uint8 *o1 = d1 + 2;
        uint8 prevE = rowMid[0];

        for (int x = 1; x < FRAME_WIDTH - 1; x++)
        {
            b = *up++;
            e = *mid++;
            h = *dn++;
            d = prevE;
            f = *mid;

            scale2x_pixel(b, d, e, f, h, o0, o1);

            prevE = e;
        }

        int li = FRAME_WIDTH - 1;

        e = rowMid[li];
        b = rowUp[li];
        d = rowMid[li - 1];
        h = rowDn[li];
        p0 = d0 + li * 2;
        p1 = d1 + li * 2;

        scale2x_pixel(b, d, e, e, h, p0, p1);
    }
}
