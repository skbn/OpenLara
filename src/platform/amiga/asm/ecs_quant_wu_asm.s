;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _wuComputeMoments_asm
    XDEF wuComputeMoments_asm
    XDEF _wuBoxError_asm
    XDEF wuBoxError_asm
    XDEF _wuCutSearch_asm
    XDEF wuCutSearch_asm

    XREF _wuW
    XREF _wuR
    XREF _wuG
    XREF _wuB
    XREF _wuR2
    XREF _wuG2
    XREF _wuB2

; void wuComputeMoments_asm(const uint8 *palette, const int16_t *colorWeight)
; a0=palette, a1=colorWeight
_wuComputeMoments_asm:
wuComputeMoments_asm:
    movem.l d2-d7/a2-a6,-(sp)

    move.l a0,-(sp)
    move.l a1,-(sp)

    ; zero all 7 tables
    lea _wuW,a0
    bsr .zero_table
    lea _wuR,a0
    bsr .zero_table
    lea _wuG,a0
    bsr .zero_table
    lea _wuB,a0
    bsr .zero_table
    lea _wuR2,a0
    bsr .zero_table
    lea _wuG2,a0
    bsr .zero_table
    lea _wuB2,a0
    bsr .zero_table

    ; a2-a6=wW,wR,wG,wB,wR2, wG2/wB2 on stack
    lea _wuW,a2
    lea _wuR,a3
    lea _wuG,a4
    lea _wuB,a5
    lea _wuR2,a6
    lea _wuG2,a0
    move.l a0,-(sp)
    lea _wuB2,a0
    move.l a0,-(sp)

    ; stack: 0=wB2 4=wG2 8=cw 12=palette 16+=saved
    move.l 12(sp),a0
    move.l 8(sp),a1

    move.w #255,d7

.acc_loop:
    move.w (a1)+,d3
    beq .acc_skip
    ext.l d3

    ; r,g,b >> 3
    moveq #0,d0
    move.b (a0),d0
    lsr.w #3,d0
    moveq #0,d1
    move.b 1(a0),d1
    lsr.w #3,d1
    moveq #0,d2
    move.b 2(a0),d2
    lsr.w #3,d2
    addq.l #3,a0

    ; idx = (r+1)*1089 + (g+1)*33 + (b+1)
    move.l d0,d4
    addq.l #1,d4
    muls.w #1089,d4
    move.l d1,d5
    addq.l #1,d5
    muls.w #33,d5
    add.l d5,d4
    move.l d2,d5
    addq.l #1,d5
    add.l d5,d4

    ; accumulate moments: w, r*w, g*w, b*w, r*r*w, g*g*w, b*b*w
    add.l d3,(a2,d4.l*4)

    move.w d0,d5
    muls.w d3,d5
    add.l d5,(a3,d4.l*4)

    move.w d1,d5
    muls.w d3,d5
    add.l d5,(a4,d4.l*4)

    move.w d2,d5
    muls.w d3,d5
    add.l d5,(a5,d4.l*4)

    move.w d0,d5
    muls.w d0,d5
    muls.w d3,d5
    add.l d5,(a6,d4.l*4)

    ; wG2: save a0 (palette) in d6
    move.w d1,d5
    muls.w d1,d5
    muls.w d3,d5
    move.l a0,d6
    move.l 4(sp),a0
    add.l d5,(a0,d4.l*4)
    move.l d6,a0

    ; wB2: save a1 (cw) in d6
    move.w d2,d5
    muls.w d2,d5
    muls.w d3,d5
    move.l a1,d6
    move.l (sp),a1
    add.l d5,(a1,d4.l*4)
    move.l d6,a1

    dbra d7,.acc_loop
    bra .acc_done

.acc_skip:
    addq.l #3,a0
    dbra d7,.acc_loop

.acc_done:
    ; pop wB2,wG2,cw,palette
    lea 16(sp),sp

    ; prefix sum pass 1: b
    lea _wuW,a0
    lea _wuR,a1
    lea _wuG,a2
    lea _wuB,a3
    lea _wuR2,a4
    lea _wuG2,a5
    lea _wuB2,a6

    ; r = 0..32
    move.w #32,-(sp)

.ps_b_r:
    ; g = 0..32
    move.w #32,-(sp)

.ps_b_g:
    moveq #0,d0
    moveq #0,d1
    moveq #0,d2
    moveq #0,d3
    moveq #0,d4
    moveq #0,d5
    moveq #0,d6

    ; -> [r][g][1]
    addq.l #4,a0
    addq.l #4,a1
    addq.l #4,a2
    addq.l #4,a3
    addq.l #4,a4
    addq.l #4,a5
    addq.l #4,a6

    ; b = 1..32
    move.w #31,d7

.ps_b_inner:
    add.l (a0),d0
    move.l d0,(a0)+
    add.l (a1),d1
    move.l d1,(a1)+
    add.l (a2),d2
    move.l d2,(a2)+
    add.l (a3),d3
    move.l d3,(a3)+
    add.l (a4),d4
    move.l d4,(a4)+
    add.l (a5),d5
    move.l d5,(a5)+
    add.l (a6),d6
    move.l d6,(a6)+
    dbra d7,.ps_b_inner

    ; a0-a6 -> [r][g+1][0]
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_b_g

    ; pop g
    addq.l #2,sp
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_b_r

    ; pop r
    addq.l #2,sp

    ; prefix sum pass 2: g
    lea _wuW,a0
    lea _wuR,a1
    lea _wuG,a2
    lea _wuB,a3
    lea _wuR2,a4
    lea _wuG2,a5
    lea _wuB2,a6

    ; r = 0..32
    move.w #32,-(sp)

.ps_g_r:
    ; -> [r][1][0]
    lea 132(a0),a0
    lea 132(a1),a1
    lea 132(a2),a2
    lea 132(a3),a3
    lea 132(a4),a4
    lea 132(a5),a5
    lea 132(a6),a6

    ; g = 1..32
    move.w #31,-(sp)

.ps_g_g:
    ; b = 0..32
    move.w #32,d7

.ps_g_inner:
    move.l (-132,a0),d0
    add.l d0,(a0)+
    move.l (-132,a1),d1
    add.l d1,(a1)+
    move.l (-132,a2),d2
    add.l d2,(a2)+
    move.l (-132,a3),d3
    add.l d3,(a3)+
    move.l (-132,a4),d4
    add.l d4,(a4)+
    move.l (-132,a5),d5
    add.l d5,(a5)+
    move.l (-132,a6),d6
    add.l d6,(a6)+
    dbra d7,.ps_g_inner
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_g_g

    ; pop g
    addq.l #2,sp
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_g_r

    ; pop r
    addq.l #2,sp

    ; prefix sum pass 3: r
    lea _wuW,a0
    lea _wuR,a1
    lea _wuG,a2
    lea _wuB,a3
    lea _wuR2,a4
    lea _wuG2,a5
    lea _wuB2,a6

    ; -> [1][0][0]
    lea 4356(a0),a0
    lea 4356(a1),a1
    lea 4356(a2),a2
    lea 4356(a3),a3
    lea 4356(a4),a4
    lea 4356(a5),a5
    lea 4356(a6),a6

    ; r = 1..32
    move.w #31,-(sp)

.ps_r_r:
    ; g = 0..32
    move.w #32,-(sp)

.ps_r_g:
    ; b = 0..32
    move.w #32,d7

.ps_r_inner:
    move.l (-4356,a0),d0
    add.l d0,(a0)+
    move.l (-4356,a1),d1
    add.l d1,(a1)+
    move.l (-4356,a2),d2
    add.l d2,(a2)+
    move.l (-4356,a3),d3
    add.l d3,(a3)+
    move.l (-4356,a4),d4
    add.l d4,(a4)+
    move.l (-4356,a5),d5
    add.l d5,(a5)+
    move.l (-4356,a6),d6
    add.l d6,(a6)+
    dbra d7,.ps_r_inner
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_r_g

    ; pop g
    addq.l #2,sp
    move.w (sp),d7
    subq.w #1,d7
    move.w d7,(sp)
    bpl .ps_r_r

    ; pop r
    addq.l #2,sp

    movem.l (sp)+,d2-d7/a2-a6
    rts

; zero 33^3 table (35937 longs)
; a0=base, clobbers d0-d7
.zero_table:
    moveq #0,d0
    moveq #0,d1
    moveq #0,d2
    moveq #0,d3
    moveq #0,d4
    moveq #0,d5
    moveq #0,d6
    move.w #5133-1,d7

.zt_loop:
    movem.l d0-d6,(a0)
    lea 28(a0),a0
    dbra d7,.zt_loop
    
    ; last 6 longs
    movem.l d0-d5,(a0)
    rts


; int32_t wuBoxError_asm(int r1, int r2, int g1, int g2, int b1, int b2, int *outW, int32_t *outRs, int32_t *outGs, int32_t *outBs)
; d0=r1, d1=r2, d2=g1, d3=g2, d4=b1, d5=b2, a0=outW, a1=outRs, a2=outGs, a3=outBs

_wuBoxError_asm:
wuBoxError_asm:
    movem.l d2-d7/a2-a6,-(sp)

    move.l a0,-(sp)
    move.l a1,-(sp)
    move.l a2,-(sp)
    move.l a3,-(sp)

    ; R2 = (r2+1)*1089, R1 = r1*1089
    move.l d1,d6
    addq.l #1,d6
    mulu.w #1089,d6
    move.l d0,d7
    mulu.w #1089,d7

    ; G2 = (g2+1)*33, G1 = g1*33
    move.l d3,d0
    addq.l #1,d0
    mulu.w #33,d0
    move.l d2,d1
    mulu.w #33,d1

    ; B2 = b2+1, B1 = b1 (a0 free)
    move.l d5,d2
    addq.l #1,d2
    move.l d4,a0

    ; 8 corner offsets (unscaled; box_sum scales *4)
    ; o1 = R2+G2+B2
    move.l d6,d4
    add.l d0,d4
    add.l d2,d4

    ; o2 = R1+G2+B2
    move.l d7,d5
    add.l d0,d5
    add.l d2,d5

    ; o3 = R2+G1+B2
    move.l d6,a4
    add.l d1,a4
    add.l d2,a4

    ; o4 = R2+G2+B1
    move.l d6,a5
    add.l d0,a5
    add.l a0,a5

    ; o5 = R1+G1+B2
    move.l d7,a6
    add.l d1,a6
    add.l d2,a6

    ; o7 = R2+G1+B1 (B2 dead)
    move.l d6,d2
    add.l d1,d2
    add.l a0,d2

    ; o8 = R1+G1+B1 (B1 dead)
    move.l d7,d3
    add.l d1,d3
    add.l a0,d3

    ; o6 = R1+G2+B1 (G1 dead)
    move.l d7,d1
    add.l d0,d1
    add.l a0,d1

    ; d4=o1 d5=o2 a4=o3 a5=o4 a6=o5 d1=o6 d2=o7 d3=o8
    ; free: d0, d6, d7, a0

    ; w, rs, gs, bs
    lea _wuW,a1
    bsr wu_box_sum
    move.l d0,d6

    lea _wuR,a1
    bsr wu_box_sum
    move.l d0,d7

    lea _wuG,a1
    bsr wu_box_sum
    move.l d0,a2

    lea _wuB,a1
    bsr wu_box_sum
    move.l d0,a3

    ; stack: 0=outBs 4=outGs 8=outRs 12=outW
    ; d6=w d7=rs a2=gs a3=bs

    ; store w,rs,gs,bs if outW != NULL
    move.l 12(sp),a0
    move.l a0,d0
    beq .skip_out
    move.l d6,(a0)
    move.l 8(sp),a1
    move.l d7,(a1)
    move.l 4(sp),a0
    move.l a2,(a0)
    move.l (sp),a1
    move.l a3,(a1)

.skip_out:
    ; w == 0 -> return 0
    move.l d6,d0
    beq .ret_zero

    ; rsq, gsq, bsq
    lea _wuR2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    lea _wuG2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    lea _wuB2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    move.l (sp)+,d5
    move.l (sp)+,d4
    move.l (sp)+,d3

    ; d6=w d7=rs a2=gs a3=bs d3=rsq d4=gsq d5=bsq

    ; mr = rs / w
    move.l d7,d0
    divu.w d6,d0
    moveq #0,d1
    move.w d0,d1

    ; mr * rs (muls.l: rs may exceed 16 bits)
    muls.l d7,d1
    move.l d1,d7

    ; mg = gs / w
    move.l a2,d0
    divu.w d6,d0
    moveq #0,d1
    move.w d0,d1

    ; mg * gs
    move.l a2,d0
    muls.l d0,d1

    ; mb = bs / w
    move.l a3,d0
    divu.w d6,d0
    moveq #0,d2
    move.w d0,d2

    ; mb * bs
    move.l a3,d0
    muls.l d0,d2

    ; rsq + gsq + bsq - (mr*rs + mg*gs + mb*bs)
    move.l d3,d0
    add.l d4,d0
    add.l d5,d0
    sub.l d7,d0
    sub.l d1,d0
    sub.l d2,d0

    bra .done

.ret_zero:
    moveq #0,d0

.done:
    lea 16(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts

; box_sum: d0 = a1[o1]-a1[o2]-a1[o3]-a1[o4]+a1[o5]+a1[o6]+a1[o7]-a1[o8]
; unscaled offsets, scaled index *4
; d4=o1 d5=o2 a4=o3 a5=o4 a6=o5 d1=o6 d2=o7 d3=o8
wu_box_sum:
    move.l (a1,d4.l*4),d0
    sub.l (a1,d5.l*4),d0
    sub.l (a1,a4.l*4),d0
    sub.l (a1,a5.l*4),d0
    add.l (a1,a6.l*4),d0
    add.l (a1,d1.l*4),d0
    add.l (a1,d2.l*4),d0
    sub.l (a1,d3.l*4),d0
    rts

; int32_t wuCutSearch_asm(int r1, int r2, int g1, int g2, int b1, int b2, int32_t boxError, int *outCh, int *outCut)
; d0=r1, d1=r2, d2=g1, d3=g2, d4=b1, d5=b2, d6=boxError, a0=outCh, a1=outCut

_wuCutSearch_asm:
wuCutSearch_asm:
    movem.l d2-d7/a2-a6,-(sp)
    lea -80(sp),sp

    ; stash box coords, boxError, out ptrs
    move.l d0,0(sp)
    move.l d1,4(sp)
    move.l d2,8(sp)
    move.l d3,12(sp)
    move.l d4,16(sp)
    move.l d5,20(sp)
    move.l d6,24(sp)
    move.l a0,28(sp)
    move.l a1,32(sp)

    ; best reduce/ch/cut = 0
    clr.l 36(sp)
    clr.l 40(sp)
    clr.l 44(sp)

    move.l 0(sp),d0
    mulu.w #1089,d0
    move.l d0,56(sp)

    move.l 4(sp),d0
    addq.l #1,d0
    mulu.w #1089,d0
    move.l d0,60(sp)

    move.l 8(sp),d0
    mulu.w #33,d0
    move.l d0,64(sp)

    move.l 12(sp),d0
    addq.l #1,d0
    mulu.w #33,d0
    move.l d0,68(sp)

    move.l 20(sp),d0
    addq.l #1,d0
    move.l d0,72(sp)

    ; R channel
    move.l 0(sp),d6
    move.l 4(sp),d7
    cmp.l d7,d6
    bge .cs_ch1
    addq.l #1,d6

.cs_ch0_loop:
    move.l d6,52(sp)

    move.l d6,d0
    mulu.w #1089,d0
    move.l d0,76(sp)

    ; e1 = boxError(r1, cut-1, g1, g2, b1, b2)
    move.l 56(sp),d0
    move.l 76(sp),d1
    move.l 64(sp),d2
    move.l 68(sp),d3
    move.l 16(sp),d4
    move.l 72(sp),d5
    bsr .cut_error
    move.l d0,48(sp)

    ; e2 = boxError(cut, r2, g1, g2, b1, b2)
    move.l 76(sp),d0
    move.l 60(sp),d1
    move.l 64(sp),d2
    move.l 68(sp),d3
    move.l 16(sp),d4
    move.l 72(sp),d5
    bsr .cut_error

    ; reduce = boxError - e1 - e2
    move.l 24(sp),d1
    sub.l 48(sp),d1
    sub.l d0,d1
    cmp.l 36(sp),d1
    ble .cs_ch0_next

    ; save best
    move.l d1,36(sp)
    clr.l 40(sp)
    move.l 52(sp),d0
    move.l d0,44(sp)

.cs_ch0_next:
    move.l 52(sp),d6
    addq.l #1,d6
    move.l 4(sp),d7
    cmp.l d7,d6
    ble .cs_ch0_loop

.cs_ch1:
    ; G channel
    move.l 8(sp),d6
    move.l 12(sp),d7
    cmp.l d7,d6
    bge .cs_ch2
    addq.l #1,d6

.cs_ch1_loop:
    move.l d6,52(sp)

    move.l d6,d0
    mulu.w #33,d0
    move.l d0,76(sp)

    ; e1 = boxError(r1, r2, g1, cut-1, b1, b2)
    move.l 56(sp),d0
    move.l 60(sp),d1
    move.l 64(sp),d2
    move.l 76(sp),d3
    move.l 16(sp),d4
    move.l 72(sp),d5
    bsr .cut_error
    move.l d0,48(sp)

    ; e2 = boxError(r1, r2, cut, g2, b1, b2)
    move.l 56(sp),d0
    move.l 60(sp),d1
    move.l 76(sp),d2
    move.l 68(sp),d3
    move.l 16(sp),d4
    move.l 72(sp),d5
    bsr .cut_error

    ; reduce = boxError - e1 - e2
    move.l 24(sp),d1
    sub.l 48(sp),d1
    sub.l d0,d1
    cmp.l 36(sp),d1
    ble .cs_ch1_next

    ; save best
    move.l d1,36(sp)
    moveq #1,d0
    move.l d0,40(sp)
    move.l 52(sp),d0
    move.l d0,44(sp)

.cs_ch1_next:
    move.l 52(sp),d6
    addq.l #1,d6
    move.l 12(sp),d7
    cmp.l d7,d6
    ble .cs_ch1_loop

.cs_ch2:
    ; B channel
    move.l 16(sp),d6
    move.l 20(sp),d7
    cmp.l d7,d6
    bge .cs_done
    addq.l #1,d6

.cs_ch2_loop:
    move.l d6,52(sp)

    ; e1 = boxError(r1, r2, g1, g2, b1, cut-1)
    move.l 56(sp),d0
    move.l 60(sp),d1
    move.l 64(sp),d2
    move.l 68(sp),d3
    move.l 16(sp),d4
    move.l d6,d5
    bsr .cut_error
    move.l d0,48(sp)

    ; e2 = boxError(r1, r2, g1, g2, cut, b2)
    move.l 56(sp),d0
    move.l 60(sp),d1
    move.l 64(sp),d2
    move.l 68(sp),d3
    move.l 52(sp),d4
    move.l 72(sp),d5
    bsr .cut_error

    ; reduce = boxError - e1 - e2
    move.l 24(sp),d1
    sub.l 48(sp),d1
    sub.l d0,d1
    cmp.l 36(sp),d1
    ble .cs_ch2_next

    ; save best
    move.l d1,36(sp)
    moveq #2,d0
    move.l d0,40(sp)
    move.l 52(sp),d0
    move.l d0,44(sp)

.cs_ch2_next:
    move.l 52(sp),d6
    addq.l #1,d6
    move.l 20(sp),d7
    cmp.l d7,d6
    ble .cs_ch2_loop

.cs_done:
    ; *outCh = best ch, *outCut = best cut
    move.l 28(sp),a0
    move.l 40(sp),d0
    move.l d0,(a0)
    move.l 32(sp),a0
    move.l 44(sp),d0
    move.l d0,(a0)

    ; return best reduce
    move.l 36(sp),d0

    lea 80(sp),sp
    movem.l (sp)+,d2-d7/a2-a6
    rts

; clobbers d0-d7/a0-a6
; d0=R1 d1=R2 d2=G1 d3=G2 d4=B1 d5=B2 (pre-scaled)
.cut_error:
    move.l d0,d7
    move.l d1,d6
    move.l d3,d0
    move.l d2,d1
    move.l d4,a0
    move.l d5,d2

    ; 8 corner offsets (wu_box_sum scales *4)
    move.l d6,d4
    add.l d0,d4
    add.l d2,d4

    move.l d7,d5
    add.l d0,d5
    add.l d2,d5

    move.l d6,a4
    add.l d1,a4
    add.l d2,a4

    move.l d6,a5
    add.l d0,a5
    add.l a0,a5

    move.l d7,a6
    add.l d1,a6
    add.l d2,a6

    move.l d6,d2
    add.l d1,d2
    add.l a0,d2

    move.l d7,d3
    add.l d1,d3
    add.l a0,d3

    move.l d7,d1
    add.l d0,d1
    add.l a0,d1

    ; w, rs, gs, bs
    lea _wuW,a1
    bsr wu_box_sum
    move.l d0,d6

    ; w == 0 -> return 0
    tst.l d6
    beq .ce_zero

    lea _wuR,a1
    bsr wu_box_sum
    move.l d0,d7

    lea _wuG,a1
    bsr wu_box_sum
    move.l d0,a2

    lea _wuB,a1
    bsr wu_box_sum
    move.l d0,a3

    ; rsq, gsq, bsq
    lea _wuR2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    lea _wuG2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    lea _wuB2,a1
    bsr wu_box_sum
    move.l d0,-(sp)

    ; pop bsq, gsq, rsq
    move.l (sp)+,d5
    move.l (sp)+,d4
    move.l (sp)+,d3

    ; mr = rs / w
    move.l d7,d0
    divu.w d6,d0
    moveq #0,d1
    move.w d0,d1
    muls.l d7,d1
    move.l d1,d7

    ; mg = gs / w
    move.l a2,d0
    divu.w d6,d0
    moveq #0,d1
    move.w d0,d1
    move.l a2,d0
    muls.l d0,d1

    ; mb = bs / w
    move.l a3,d0
    divu.w d6,d0
    moveq #0,d2
    move.w d0,d2
    move.l a3,d0
    muls.l d0,d2

    ; rsq + gsq + bsq - (mr*rs + mg*gs + mb*bs)
    move.l d3,d0
    add.l d4,d0
    add.l d5,d0
    sub.l d7,d0
    sub.l d1,d0
    sub.l d2,d0
    rts

.ce_zero:
    moveq #0,d0
    rts
