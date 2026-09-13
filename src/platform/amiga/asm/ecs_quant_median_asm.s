;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _boxVariance_asm
    XDEF boxVariance_asm
    XDEF _findMedianCut_asm
    XDEF findMedianCut_asm

CB_WEIGHT = 24
CB_RSUM = 26
CB_GSUM = 30
CB_BSUM = 34
CB_MEMBERS = 38
CB_NMEMBERS = 294

; int64_t boxVariance_asm(const ColorBox *box, const uint8 *pal5r, const uint8 *pal5g, const uint8 *pal5b, const int16_t *colorWeight)
; a0=box, a1=pal5r, a2=pal5g, a3=pal5b, a4=colorWeight
_boxVariance_asm:
boxVariance_asm:
    movem.l d2-d7/a2-a6,-(sp)

    move.w CB_WEIGHT(a0),d0
    beq .var_zero

    ; mean color
    move.l CB_RSUM(a0),d1
    divs.w d0,d1
    moveq #0,d2
    move.w d1,d2

    move.l CB_GSUM(a0),d1
    divs.w d0,d1
    moveq #0,d3
    move.w d1,d3

    move.l CB_BSUM(a0),d1
    divs.w d0,d1
    moveq #0,d4
    move.w d1,d4

    ; var = 0 (d5=hi, d6=lo)
    moveq #0,d5
    moveq #0,d6

    lea CB_MEMBERS(a0),a5
    move.l CB_NMEMBERS(a0),d7
    subq.l #1,d7
    blt .var_ret

    ; a0 free now, idx temp
    ; a1=pal5r a2=pal5g a3=pal5b a4=cw
    moveq #0,d1

.var_loop:
    move.b (a5)+,d1
    move.l d1,a0

    ; NTSC-weighted squared distance
    moveq #0,d0
    move.b (a1,a0.l),d0
    sub.l d2,d0
    muls.w d0,d0
    muls.w #30,d0

    move.b (a2,a0.l),d1
    sub.l d3,d1
    muls.w d1,d1
    muls.w #59,d1
    add.l d1,d0

    moveq #0,d1
    move.b (a3,a0.l),d1
    sub.l d4,d1
    muls.w d1,d1
    muls.w #11,d1
    add.l d1,d0

    ; weight by cw[idx]
    move.w (a4,a0.l*2),d1
    ext.l d1
    muls.l d1,d0

    ; var += term (64-bit)
    moveq #0,d1
    add.l d0,d6
    addx.l d1,d5

    dbra d7,.var_loop

.var_ret:
    move.l d5,d0
    move.l d6,d1
    movem.l (sp)+,d2-d7/a2-a6
    rts

.var_zero:
    moveq #0,d0
    moveq #0,d1
    movem.l (sp)+,d2-d7/a2-a6
    rts


; int findMedianCut_asm(const ColorBox *box, int ch, const uint8 *pal5r, const uint8 *pal5g, const uint8 *pal5b, const int16_t *colorWeight)
; a0=box, d0=ch, a1=pal5r, a2=pal5g, a3=pal5b, a4=colorWeight
_findMedianCut_asm:
findMedianCut_asm:
    movem.l d2-d7/a2-a6,-(sp)
    lea -64(sp),sp

    ; pick chan: 0->r 1->g 2->b
    move.l a1,a5
    subq.l #1,d0
    beq .ch_g
    blt .ch_done
    move.l a3,a5
    bra .ch_done

.ch_g:
    move.l a2,a5

.ch_done:
    ; zero hist[32] on stack
    moveq #0,d0
    moveq #0,d1
    moveq #0,d2
    moveq #0,d3
    move.l sp,a6
    movem.l d0-d3,(a6)
    movem.l d0-d3,16(a6)
    movem.l d0-d3,32(a6)
    movem.l d0-d3,48(a6)

    ; build histogram of channel values weighted by cw
    lea CB_MEMBERS(a0),a6
    move.l CB_NMEMBERS(a0),d7
    subq.l #1,d7
    blt .scan_init
    moveq #0,d1
    moveq #0,d2

.hist_loop:
    move.b (a6)+,d1
    move.b (a5,d1.l),d2
    move.w (a4,d1.l*2),d3
    add.w d3,(sp,d2.l*2)
    dbra d7,.hist_loop

.scan_init:
    ; find lo, hi, total
    move.l sp,a6
    moveq #32,d4
    moveq #-1,d5
    moveq #0,d6
    moveq #31,d7
    moveq #0,d3

.scan_loop:
    move.w (a6)+,d2
    beq .scan_next
    cmp.w #32,d4
    bne .scan_hi
    move.l d3,d4

.scan_hi:
    move.l d3,d5
    add.w d2,d6

.scan_next:
    addq.l #1,d3
    dbra d7,.scan_loop

    ; empty or single bin: bail
    cmp.l d4,d5
    ble .ret_lo
    tst.w d6
    beq .ret_lo

    ; split at median (cum*2 >= total)
    moveq #0,d3
    move.l d4,d7
    lea (sp,d4.l*2),a6

.median_loop:
    add.w (a6)+,d3
    move.w d3,d2
    asl.w #1,d2
    cmp.w d6,d2
    bge .ret_v
    addq.l #1,d7
    cmp.l d5,d7
    ble .median_loop

    ; fallback: midpoint
    move.l d4,d0
    add.l d5,d0
    asr.l #1,d0
    bra .done

.ret_lo:
    move.l d4,d0
    bra .done

.ret_v:
    move.l d7,d0

.done:
    lea 64(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts

