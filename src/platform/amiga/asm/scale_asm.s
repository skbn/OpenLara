;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _scaleNearest2x_asm
    XDEF scaleNearest2x_asm
    XDEF _scale2x_asm
    XDEF scale2x_asm

FW = 320
FH = 200


; void scaleNearest2x_asm(const uint8 *src, uint8 *dst, int displayWidth)
; a0=src, a1=dst, d0=displayWidth
_scaleNearest2x_asm:
scaleNearest2x_asm:
    movem.l d2-d7/a2-a4,-(sp)

    ; d7 = 2*stride, a2 = src end
    move.l d0,d7
    add.l d7,d7
    lea FW*FH(a0),a2
    moveq #0,d1

.sn_row:
    ; a4 = row0, a3 = row1, 4 src px per pass
    move.l a1,a4
    lea (a1,d0.l),a3
    moveq #FW/4-1,d6

.sn_col4:
    ; w0 = p0|p0|p1|p1
    move.b (a0)+,d1
    move.l d1,d2
    lsl.l #8,d2
    or.l d1,d2
    swap d2
    move.b (a0)+,d1
    move.l d1,d3
    lsl.l #8,d3
    or.l d1,d3
    or.l d3,d2
    move.l d2,(a1)+
    move.l d2,(a3)+

    ; w1 = p2|p2|p3|p3
    move.b (a0)+,d1
    move.l d1,d2
    lsl.l #8,d2
    or.l d1,d2
    swap d2
    move.b (a0)+,d1
    move.l d1,d3
    lsl.l #8,d3
    or.l d1,d3
    or.l d3,d2
    move.l d2,(a1)+
    move.l d2,(a3)+

    dbra d6,.sn_col4

    ; skip the two dst rows we just filled
    move.l a4,a1
    add.l d7,a1
    cmp.l a2,a0
    bcs .sn_row

    movem.l (sp)+,d2-d7/a2-a4
    rts


; void scale2x_asm(const uint8 *src, uint8 *dst, int displayWidth)
; a0=src, a1=dst, d0=displayWidth
; b=up d=left e=center f=right h=down
; e0=(d==b)?d:e  e1=(b==f)?f:e  e2=(d==h)?d:e  e3=(h==f)?f:e
_scale2x_asm:
scale2x_asm:
    movem.l d2-d7/a2-a6,-(sp)

    move.l d0,d7
    lea FW*FH(a0),a6

    ; a2 = rowUp (clamps to row0)
    move.l a0,a2
    move.w #FH-1,d5

.s2_y:
    ; d5 doubles as y counter, save it across the inner loop
    move.l d5,-(sp)

    ; rowDn = rowMid+FW, clamp to rowMid on the last row
    lea FW(a0),a3
    cmp.l a6,a3
    bcs .s2_dn_ok
    sub.l #FW,a3

.s2_dn_ok:
    move.l a1,a4
    lea (a1,d7.l),a5

    ; prevE = rowMid[0], serves as left neighbour for x=0
    move.b (a0),d1

    ; inner loop runs x=0..FW-2, last column is peeled so f needs no clamp
    move.w #FW-2,d6

.s2_x:
    move.b (a2)+,d0
    move.b (a0)+,d2
    move.b (a3)+,d3
    move.b (a0),d4
    cmp.b d0,d3
    beq .s2_noedge
    cmp.b d1,d4
    beq .s2_noedge

    ; top word = e0|e1
    move.b d2,d5
    cmp.b d0,d1
    bne .s2_tl
    move.b d1,d5

.s2_tl:
    cmp.b d0,d4
    beq .s2_tr
    move.b d2,d0

.s2_tr:
    lsl.w #8,d5
    or.b d0,d5
    move.w d5,(a4)+

    ; bot word = e2|e3
    move.b d2,d5
    cmp.b d3,d1
    bne .s2_bl
    move.b d1,d5

.s2_bl:
    cmp.b d3,d4
    beq .s2_br
    move.b d2,d4

.s2_br:
    lsl.w #8,d5
    or.b d4,d5
    move.w d5,(a5)+

    move.b d2,d1
    dbra d6,.s2_x
    bra .s2_last

.s2_noedge:
    move.b d2,d5
    lsl.w #8,d5
    or.b d2,d5
    move.w d5,(a4)+
    move.w d5,(a5)+
    move.b d2,d1
    dbra d6,.s2_x
    bra .s2_last

.s2_last:
    ; x=FW-1, right neighbour clamps to the centre pixel
    move.b (a2)+,d0
    move.b (a0)+,d2
    move.b (a3)+,d3
    move.b d2,d4
    cmp.b d0,d3
    beq .s2_last_noedge
    cmp.b d1,d4
    beq .s2_last_noedge

    move.b d2,d5
    cmp.b d0,d1
    bne .s2_last_tl
    move.b d1,d5

.s2_last_tl:
    cmp.b d0,d4
    beq .s2_last_tr
    move.b d2,d0

.s2_last_tr:
    lsl.w #8,d5
    or.b d0,d5
    move.w d5,(a4)+

    move.b d2,d5
    cmp.b d3,d1
    bne .s2_last_bl
    move.b d1,d5

.s2_last_bl:
    cmp.b d3,d4
    beq .s2_last_br
    move.b d2,d4
    
.s2_last_br:
    lsl.w #8,d5
    or.b d4,d5
    move.w d5,(a5)+
    bra .s2_y_next

.s2_last_noedge:
    move.b d2,d5
    lsl.w #8,d5
    or.b d2,d5
    move.w d5,(a4)+
    move.w d5,(a5)+
    bra .s2_y_next

.s2_y_next:
    move.l (sp)+,d5
    add.l d7,a1
    add.l d7,a1
    ; rowUp for next row = current rowMid
    lea -FW(a0),a2
    dbra d5,.s2_y

    movem.l (sp)+,d2-d7/a2-a6
    rts
