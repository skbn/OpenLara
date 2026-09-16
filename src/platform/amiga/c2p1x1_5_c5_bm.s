;
; tornado - Port of Tornado DI 1993 x86 ASM to C
;
; Tanausu M. 39:190/101@amiganet 2:341/207@fidonet
;
; 2018
;
; c2p1x1_5_c5_bm.s 1x1 5bpl cpu5 C2P for arbitrary BitMaps
;
; Based on c2p1x1_6_c5_bm by Mikael Kalms, adapted for 5 bitplanes
; Pass 1 produces bpl0-3 (lower 4 bits, with $0f0f0f0f masking)
; Pass 2 produces bpl4 (bit 4 only). Bit 4 is extracted via
; AND #$10101010 + LSR #4 per longword, then the full 4 bpl C2P
; pipeline runs. Only bpl0 (d7) is stored to bpl4; bpl1-3 are zero
;
; Restrictions:
; Chunky-buffer must be an even multiple of 32 pixels wide
; X-Offset must be set to an even multiple of 8
; If these conditions not are met, the routine will abort

    include graphics/gfx.i

        rsreset
C2P1X1_5_C5_BM_CHUNKYX    rs.w 1
C2P1X1_5_C5_BM_CHUNKYY    rs.w 1
C2P1X1_5_C5_BM_ROWMOD     rs.l 1
C2P1X1_5_C5_BM_SIZEOF     rs.b 0

    section code,code

; d0.w  chunkyx [chunky-pixels]
; d1.w  chunkyy [chunky-pixels]
; d2.w  offsx [screen-pixels]
; d3.w  offsy [screen-pixels]
; a0    chunkyscreen
; a1    BitMap

    XDEF _c2p1x1_5_c5_bm
    XDEF c2p1x1_5_c5_bm
_c2p1x1_5_c5_bm
c2p1x1_5_c5_bm
    movem.l d2-d7/a2-a6,-(sp)
    suba.l #C2P1X1_5_C5_BM_SIZEOF,sp

    cmpi.b #5,bm_Depth(a1)
    blo .exit
    move.w d0,d4
    move.w d2,d5
    andi.w #$1f,d4
    bne .exit
    andi.w #$7,d5
    bne .exit
    moveq #0,d4
    move.w bm_BytesPerRow(a1),d4

    move.w d0,C2P1X1_5_C5_BM_CHUNKYX(sp)
    beq .exit
    move.w d1,C2P1X1_5_C5_BM_CHUNKYY(sp)
    beq .exit

    ext.l d2
    mulu.w d4,d3
    lsr.l #3,d2
    add.l d2,d3

    lsl.w #3,d4
    sub.w d0,d4
    bmi .exit
    bne .c2p_mod

; ===================================================================
; Non-modulo path
; ===================================================================
    mulu.w d0,d1
    add.l a0,d1
    move.l d1,a2

    movem.l a0-a1/d3,-(sp)

; PASS 1: bpl0-3
    movem.l bm_Planes(a1),a3-a6
    add.l d3,a3
    add.l d3,a4
    add.l d3,a5
    add.l d3,a6

    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3

    move.l #$0f0f0f0f,d6
    and.l d6,d0
    and.l d6,d1
    and.l d6,d2
    and.l d6,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    move.l #$0f0f0f0f,d4
    and.l d4,d2
    and.l d4,d6
    and.l d4,d3
    and.l d4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    bra .x1start

.x1
    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3
    move.l d7,(a3)+

    move.l #$0f0f0f0f,d6
    and.l d6,d0
    and.l d6,d1
    and.l d6,d2
    and.l d6,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7
    move.l d4,(a4)+

    move.l #$0f0f0f0f,d4
    and.l d4,d2
    and.l d4,d6
    and.l d4,d3
    and.l d4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3
    move.l d5,(a5)+

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    move.l a1,(a6)+
.x1start
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d2
    lsl.l #8,d6
    lsl.l #8,d7
    eor.l d6,d1
    eor.l d7,d3

    move.l #$55555555,d4
    move.l d1,d5
    move.l d3,d7
    lsr.l #1,d5
    lsr.l #1,d7
    eor.l d0,d5
    eor.l d2,d7
    and.l d4,d5
    and.l d4,d7
    eor.l d5,d0
    eor.l d7,d2
    add.l d5,d5
    add.l d7,d7
    eor.l d1,d5
    eor.l d3,d7

    move.l d0,a1
    move.l d2,d4

    cmpa.l a0,a2
    bne .x1

    move.l d7,(a3)+
    move.l d4,(a4)+
    move.l d5,(a5)+
    move.l a1,(a6)+

    movem.l (sp)+,a0-a1/d3

; PASS 2: bpl4 (bit 4 only)
; Extract bit 4 via AND #$10101010 + LSR #4 per longword, then
; run the full 4 bpl C2P pipeline. Only d7 (bpl0) is stored.
    move.l bm_Planes+4*4(a1),a3
    add.l d3,a3

    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3

    andi.l #$10101010,d0
    andi.l #$10101010,d1
    andi.l #$10101010,d2
    andi.l #$10101010,d3
    lsr.l #4,d0
    lsr.l #4,d1
    lsr.l #4,d2
    lsr.l #4,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    andi.l #$10101010,d2
    andi.l #$10101010,d6
    andi.l #$10101010,d3
    andi.l #$10101010,d7
    lsr.l #4,d2
    lsr.l #4,d6
    lsr.l #4,d3
    lsr.l #4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    bra .x2start

.x2
    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3
    move.l d7,(a3)+

    andi.l #$10101010,d0
    andi.l #$10101010,d1
    andi.l #$10101010,d2
    andi.l #$10101010,d3
    lsr.l #4,d0
    lsr.l #4,d1
    lsr.l #4,d2
    lsr.l #4,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    andi.l #$10101010,d2
    andi.l #$10101010,d6
    andi.l #$10101010,d3
    andi.l #$10101010,d7
    lsr.l #4,d2
    lsr.l #4,d6
    lsr.l #4,d3
    lsr.l #4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
.x2start
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d2
    lsl.l #8,d6
    lsl.l #8,d7
    eor.l d6,d1
    eor.l d7,d3

    move.l #$55555555,d4
    move.l d1,d5
    move.l d3,d7
    lsr.l #1,d5
    lsr.l #1,d7
    eor.l d0,d5
    eor.l d2,d7
    and.l d4,d5
    and.l d4,d7
    eor.l d5,d0
    eor.l d7,d2
    add.l d5,d5
    add.l d7,d7
    eor.l d1,d5
    eor.l d3,d7

    cmpa.l a0,a2
    bne .x2

    move.l d7,(a3)+

    bra .exit

; ===================================================================
; Modulo path
; ===================================================================
.c2p_mod
    lsr.w #3,d4
    move.l d4,C2P1X1_5_C5_BM_ROWMOD(sp)

    move.l a0,a2
    add.w C2P1X1_5_C5_BM_CHUNKYX(sp),a2
    add.w #32,a2

    movem.l a0-a2/d1/d3,-(sp)

; PASS 1 (modulo): bpl0-3
    movem.l bm_Planes(a1),a3-a6
    add.l d3,a3
    add.l d3,a4
    add.l d3,a5
    add.l d3,a6

    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3

    move.l #$0f0f0f0f,d6
    and.l d6,d0
    and.l d6,d1
    and.l d6,d2
    and.l d6,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    move.l #$0f0f0f0f,d4
    and.l d4,d2
    and.l d4,d6
    and.l d4,d3
    and.l d4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    bra .modx1start

.modx1y
    add.w C2P1X1_5_C5_BM_CHUNKYX+20(sp),a2
    move.l C2P1X1_5_C5_BM_ROWMOD+20(sp),d0
    add.l d0,a3
    add.l d0,a4
    add.l d0,a5
    add.l d0,a6
.modx1
    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3
    move.l d7,(a3)+

    move.l #$0f0f0f0f,d6
    and.l d6,d0
    and.l d6,d1
    and.l d6,d2
    and.l d6,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7
    move.l d4,(a4)+

    move.l #$0f0f0f0f,d4
    and.l d4,d2
    and.l d4,d6
    and.l d4,d3
    and.l d4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3
    move.l d5,(a5)+

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    move.l a1,(a6)+
.modx1start
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d2
    lsl.l #8,d6
    lsl.l #8,d7
    eor.l d6,d1
    eor.l d7,d3

    move.l #$55555555,d4
    move.l d1,d5
    move.l d3,d7
    lsr.l #1,d5
    lsr.l #1,d7
    eor.l d0,d5
    eor.l d2,d7
    and.l d4,d5
    and.l d4,d7
    eor.l d5,d0
    eor.l d7,d2
    add.l d5,d5
    add.l d7,d7
    eor.l d1,d5
    eor.l d3,d7

    move.l d0,a1
    move.l d2,d4

    cmpa.l a0,a2
    bne .modx1

    subq.w #1,C2P1X1_5_C5_BM_CHUNKYY+20(sp)
    bne .modx1y

    movem.l (sp)+,a0-a2/d1/d3

    move.w d1,C2P1X1_5_C5_BM_CHUNKYY(sp)

; PASS 2 (modulo): bpl4 (bit 4 only)
    move.l bm_Planes+4*4(a1),a3
    add.l d3,a3

    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3

    andi.l #$10101010,d0
    andi.l #$10101010,d1
    andi.l #$10101010,d2
    andi.l #$10101010,d3
    lsr.l #4,d0
    lsr.l #4,d1
    lsr.l #4,d2
    lsr.l #4,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    andi.l #$10101010,d2
    andi.l #$10101010,d6
    andi.l #$10101010,d3
    andi.l #$10101010,d7
    lsr.l #4,d2
    lsr.l #4,d6
    lsr.l #4,d3
    lsr.l #4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
    bra .modx2start

.modx2y
    add.w C2P1X1_5_C5_BM_CHUNKYX(sp),a2
    move.l C2P1X1_5_C5_BM_ROWMOD(sp),d0
    add.l d0,a3
.modx2
    move.l (a0)+,d0
    move.l (a0)+,d2
    move.l (a0)+,d1
    move.l (a0)+,d3
    move.l d7,(a3)+

    andi.l #$10101010,d0
    andi.l #$10101010,d1
    andi.l #$10101010,d2
    andi.l #$10101010,d3
    lsr.l #4,d0
    lsr.l #4,d1
    lsr.l #4,d2
    lsr.l #4,d3
    lsl.l #4,d0
    lsl.l #4,d1
    or.l d2,d0
    or.l d3,d1

    move.l (a0)+,d2
    move.l (a0)+,d6
    move.l (a0)+,d3
    move.l (a0)+,d7

    andi.l #$10101010,d2
    andi.l #$10101010,d6
    andi.l #$10101010,d3
    andi.l #$10101010,d7
    lsr.l #4,d2
    lsr.l #4,d6
    lsr.l #4,d3
    lsr.l #4,d7
    lsl.l #4,d2
    lsl.l #4,d3
    or.l d6,d2
    or.l d7,d3

    move.w d2,d6
    move.w d3,d7
    move.w d0,d2
    move.w d1,d3
    swap d2
    swap d3
    move.w d2,d0
    move.w d3,d1
    move.w d6,d2
    move.w d7,d3

    move.l #$33333333,d4
    move.l d2,d6
    move.l d3,d7
    lsr.l #2,d6
    lsr.l #2,d7
    eor.l d0,d6
    eor.l d1,d7
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d1
    lsl.l #2,d6
    lsl.l #2,d7
    eor.l d6,d2
    eor.l d7,d3

    move.l #$00ff00ff,d4
    move.l d1,d6
    move.l d3,d7
    lsr.l #8,d6
    lsr.l #8,d7
    eor.l d0,d6
    eor.l d2,d7
.modx2start
    and.l d4,d6
    and.l d4,d7
    eor.l d6,d0
    eor.l d7,d2
    lsl.l #8,d6
    lsl.l #8,d7
    eor.l d6,d1
    eor.l d7,d3

    move.l #$55555555,d4
    move.l d1,d5
    move.l d3,d7
    lsr.l #1,d5
    lsr.l #1,d7
    eor.l d0,d5
    eor.l d2,d7
    and.l d4,d5
    and.l d4,d7
    eor.l d5,d0
    eor.l d7,d2
    add.l d5,d5
    add.l d7,d7
    eor.l d1,d5
    eor.l d3,d7

    cmpa.l a0,a2
    bne .modx2

    subq.w #1,C2P1X1_5_C5_BM_CHUNKYY(sp)
    bne .modx2y

    move.l d7,(a3)+

.exit
    adda.l #C2P1X1_5_C5_BM_SIZEOF,sp
    movem.l (sp)+,d2-d7/a2-a6
    rts
