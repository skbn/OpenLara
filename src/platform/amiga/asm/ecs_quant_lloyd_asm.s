;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _lloydMax_asm
    XDEF lloydMax_asm
    XDEF _lloyd3DSeed_asm
    XDEF lloyd3DSeed_asm
    XDEF _lloyd3DRefine_asm
    XDEF lloyd3DRefine_asm

; void lloydMax_asm(int ch, int nlev, const uint8 *palette, const int16_t *colorWeight, uint8 *opt)
; d0=ch, d1=nlev, a0=palette, a1=colorWeight, a2=opt
_lloydMax_asm:
lloydMax_asm:
    movem.l d2-d7/a2-a6,-(sp)
    lea -52(sp),sp

    move.l a2,a5
    move.w d1,d7
    subq.w #1,d7
    move.l a0,a3
    add.l d0,a3
    move.l a1,a4

    move.w #7,48(sp)

.outer_loop:
    ; zero sums[8] and counts[8] on stack
    moveq #0,d0
    moveq #0,d1
    moveq #0,d2
    moveq #0,d3
    movem.l d0-d3,(sp)
    movem.l d0-d3,16(sp)
    movem.l d0-d3,32(sp)

    lea 32(sp),a6

    move.l a3,a0
    move.l a4,a1

    move.w #255,d6

.inner_loop:
    move.w (a1)+,d5
    beq .skip

    ; v = *pp >> 4
    moveq #0,d4
    move.b (a0),d4
    lsr.w #4,d4
    addq.l #3,a0

    ; find nearest level
    move.l a5,a2
    move.w d7,d3
    moveq #0,d1
    moveq #0,d2
    move.w #256,d2

.search_loop:
    moveq #0,d0
    move.b (a2)+,d0
    sub.w d4,d0
    muls.w d0,d0
    cmp.w d2,d0
    bge .search_next

    move.w d0,d2
    move.w d7,d1
    sub.w d3,d1

.search_next:
    dbra d3,.search_loop

    ; accumulate weighted sums and counts per level
    muls.w d5,d4
    add.l d4,(sp,d1.l*4)

    add.w d5,(a6,d1.l*2)

    dbra d6,.inner_loop
    bra .inner_done

.skip:
    addq.l #3,a0
    dbra d6,.inner_loop

.inner_done:
    ; recompute centroids
    moveq #0,d3
    move.l sp,a0

.update_loop:
    move.l (a0)+,d0
    move.w (a6)+,d2
    ble .update_skip

    divs.w d2,d0
    move.b d0,(a5,d3.l)

.update_skip:
    addq.w #1,d3
    cmp.w d7,d3
    ble .update_loop

    subq.w #1,48(sp)
    bpl .outer_loop

    lea 52(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts

    section .bss,bss

l3d_csumR: ds.l 64
l3d_csumG: ds.l 64
l3d_csumB: ds.l 64
l3d_cw: ds.l 64
    section .text,code

R5B = 0
G5B = 4
B5B = 8
CWB = 12
CRB = 16
CGB = 20
CBB = 24
NCOL = 28
WSP = 32
ITER = 36
CNT = 38
L3D_FR = 40

_lloyd3DRefine_asm:
lloyd3DRefine_asm:
    movem.l d2-d7/a2-a6,-(sp)
    lea -L3D_FR(sp),sp

    ; stash args in stack frame
    move.l a0,R5B(sp)
    move.l a1,G5B(sp)
    move.l a2,B5B(sp)
    move.l a3,CWB(sp)
    move.l a4,CRB(sp)
    move.l a6,CBB(sp)
    move.l d0,NCOL(sp)

    ; 8 refinement passes
    move.w #7,ITER(sp)

    ; cg passed on stack
    move.l 88(sp),a5
    move.l a5,CGB(sp)

.l3d_iter:
    ; reload bases
    move.l R5B(sp),a0
    move.l G5B(sp),a1
    move.l B5B(sp),a2
    move.l CWB(sp),a3

    move.l NCOL(sp),d7
    beq .l3d_zero_done
    subq.l #1,d7

    ; zero csum/cw tables
    lea l3d_csumR,a4
    lea l3d_csumG,a5
    lea l3d_csumB,a6
    lea l3d_cw,a3

.l3d_zero:
    clr.l (a4)+
    clr.l (a5)+
    clr.l (a6)+
    clr.l (a3)+
    dbra d7,.l3d_zero

    move.l CWB(sp),a3

.l3d_zero_done:
    move.w #255,CNT(sp)

.l3d_outer:
    ; r,g,b = *r5++, *g5++, *b5++
    moveq #0,d0
    move.b (a0)+,d0
    moveq #0,d1
    move.b (a1)+,d1
    moveq #0,d2
    move.b (a2)+,d2

    move.w (a3)+,d3
    beq .l3d_skip

    ; w = *cw++
    ext.l d3
    move.l d3,WSP(sp)

    move.l CRB(sp),a4
    move.l CGB(sp),a5
    move.l CBB(sp),a6

    ; find nearest centroid
    moveq #0,d4
    move.l #$7FFFFFFF,d3
    moveq #0,d5

.l3d_inner:
    move.l (a4,d5.l*4),d6
    sub.l d0,d6
    muls.w d6,d6
    muls.w #30,d6

    move.l (a5,d5.l*4),d7
    sub.l d1,d7
    muls.w d7,d7
    muls.w #59,d7
    add.l d7,d6

    move.l (a6,d5.l*4),d7
    sub.l d2,d7
    muls.w d7,d7
    muls.w #11,d7
    add.l d7,d6

    cmp.l d3,d6
    bge .l3d_no_better
    move.l d6,d3
    move.l d5,d4

.l3d_no_better:
    addq.l #1,d5
    cmp.l NCOL(sp),d5
    blt .l3d_inner

    ; accumulate weighted sums and counts
    move.l WSP(sp),d3
    lea l3d_csumR,a4
    move.l d0,d7
    muls.w d3,d7
    add.l d7,(a4,d4.l*4)

    lea l3d_csumG,a4
    move.l d1,d7
    muls.w d3,d7
    add.l d7,(a4,d4.l*4)

    lea l3d_csumB,a4
    move.l d2,d7
    muls.w d3,d7
    add.l d7,(a4,d4.l*4)

    lea l3d_cw,a4
    add.l d3,(a4,d4.l*4)

.l3d_skip:
    subq.w #1,CNT(sp)
    bpl .l3d_outer

    ; recompute centroids
    move.l NCOL(sp),d6
    beq .l3d_update_done
    moveq #0,d5
    lea l3d_cw,a0
    lea l3d_csumR,a1
    lea l3d_csumG,a2
    lea l3d_csumB,a3
    move.l CRB(sp),a4
    move.l CGB(sp),a5
    move.l CBB(sp),a6

.l3d_update:
    move.l (a0,d5.l*4),d7
    tst.l d7
    ble .l3d_update_skip

    move.l (a1,d5.l*4),d0
    divs.l d7,d0
    move.l d0,(a4,d5.l*4)

    move.l (a2,d5.l*4),d0
    divs.l d7,d0
    move.l d0,(a5,d5.l*4)

    move.l (a3,d5.l*4),d0
    divs.l d7,d0
    move.l d0,(a6,d5.l*4)

.l3d_update_skip:
    addq.l #1,d5
    cmp.l d6,d5
    blt .l3d_update

.l3d_update_done:
    subq.w #1,ITER(sp)
    bpl .l3d_iter

    lea L3D_FR(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts


; void lloyd3DSeed_asm(const uint8 *r5, const uint8 *g5, const uint8 *b5, const int16_t *cw, int32_t *cr, int32_t *cg, int32_t *cb, int32_t *dist, int nColors)
; a0=r5, a1=g5, a2=b5, a3=cw, a4=cr, a6=cb, d0=nColors, 4(sp)=cg, 8(sp)=dist

_lloyd3DSeed_asm:
lloyd3DSeed_asm:
    movem.l d2-d7/a2-a6,-(sp)
    lea -40(sp),sp

    move.l a0,0(sp)
    move.l a1,4(sp)
    move.l a2,8(sp)
    move.l a3,12(sp)
    move.l a4,16(sp)
    move.l a6,24(sp)
    move.l 88(sp),a5
    move.l a5,20(sp)
    move.l 92(sp),a5
    move.l a5,28(sp)
    move.l d0,32(sp)

    move.l (a4),d4
    move.l 20(sp),a5
    move.l (a5),d5
    move.l (a6),d3

    move.l 28(sp),a4
    moveq #0,d6
    move.w #255,d7

.seed_init:
    move.w (a3,d6.l*2),d1
    beq .seed_init_skip

    moveq #0,d0
    move.b (a0,d6.l),d0
    sub.l d4,d0
    muls.w d0,d0
    muls.w #30,d0

    moveq #0,d2
    move.b (a1,d6.l),d2
    sub.l d5,d2
    muls.w d2,d2
    muls.w #59,d2
    add.l d2,d0

    moveq #0,d2
    move.b (a2,d6.l),d2
    sub.l d3,d2
    muls.w d2,d2
    muls.w #11,d2
    add.l d2,d0

    move.l d0,(a4,d6.l*4)

.seed_init_skip:
    addq.l #1,d6
    dbra d7,.seed_init

    moveq #1,d0
    move.l d0,36(sp)

.seed_while:
    move.l 36(sp),d6
    cmp.l 32(sp),d6
    bge .seed_done

    ; pick farthest point
    moveq #-1,d3
    moveq #-1,d4
    moveq #0,d6
    move.w #255,d7

.seed_find:
    move.w (a3,d6.l*2),d5
    beq .seed_find_skip

    move.l (a4,d6.l*4),d2
    ext.l d5
    muls.l d5,d2
    cmp.l d3,d2
    ble .seed_find_skip

    move.l d2,d3
    move.l d6,d4

.seed_find_skip:
    addq.l #1,d6
    dbra d7,.seed_find

    tst.l d4
    blt .seed_fill

    move.l d4,d7
    move.l 36(sp),d6

    ; cr/cg/cb[nC] = r5/g5/b5[best]
    moveq #0,d4
    move.b (a0,d7.l),d4
    move.l 16(sp),a5
    move.l d4,(a5,d6.l*4)

    moveq #0,d5
    move.b (a1,d7.l),d5
    move.l 20(sp),a5
    move.l d5,(a5,d6.l*4)

    moveq #0,d3
    move.b (a2,d7.l),d3
    move.l 24(sp),a5
    move.l d3,(a5,d6.l*4)

    addq.l #1,d6
    move.l d6,36(sp)

    ; d4/d5/d3 = new centroid
    moveq #0,d6
    move.w #255,d7

.seed_update:
    move.w (a3,d6.l*2),d1
    beq .seed_update_skip

    moveq #0,d0
    move.b (a0,d6.l),d0
    sub.l d4,d0
    muls.w d0,d0
    muls.w #30,d0

    moveq #0,d2
    move.b (a1,d6.l),d2
    sub.l d5,d2
    muls.w d2,d2
    muls.w #59,d2
    add.l d2,d0

    moveq #0,d2
    move.b (a2,d6.l),d2
    sub.l d3,d2
    muls.w d2,d2
    muls.w #11,d2
    add.l d2,d0

    cmp.l (a4,d6.l*4),d0
    bge .seed_update_skip
    move.l d0,(a4,d6.l*4)

.seed_update_skip:
    addq.l #1,d6
    dbra d7,.seed_update

    bra .seed_while

.seed_fill:
    move.l 36(sp),d6
    move.l 16(sp),a4
    move.l 20(sp),a5
    move.l 24(sp),a6

.seed_fill_loop:
    cmp.l 32(sp),d6
    bge .seed_done

    move.l -4(a4,d6.l*4),d0
    move.l d0,(a4,d6.l*4)

    move.l -4(a5,d6.l*4),d0
    move.l d0,(a5,d6.l*4)

    move.l -4(a6,d6.l*4),d0
    move.l d0,(a6,d6.l*4)

    addq.l #1,d6
    bra .seed_fill_loop

.seed_done:
    lea 40(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts
