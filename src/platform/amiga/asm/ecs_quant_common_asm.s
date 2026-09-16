;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _ecsBuildRemap_asm
    XDEF ecsBuildRemap_asm
    XDEF _ecsRemapLightmap_asm
    XDEF ecsRemapLightmap_asm
    XDEF _ecsbuildFillmap_asm
    XDEF ecsbuildFillmap_asm

    XREF _ecsPalette
    XREF _numColors
    XREF _paletteCurrent
    XREF _gInvQuant
    XREF _gLightmap
    XREF _gFillmapData
    XREF _gFillmap


; void ecsBuildRemap_asm(const uint8 *palette, uint8 *remap)
; a0=palette (768 bytes), a1=remap (256 bytes)

    section .bss

pr: ds.b 64
pg: ds.b 64
pb: ds.b 64
pal4r: ds.b 256
pal4g: ds.b 256
pal4b: ds.b 256


    section .text,code

_ecsBuildRemap_asm:
ecsBuildRemap_asm:
    movem.l d2-d7/a2-a6,-(sp)
    subq.l #4,sp

    ; unpack ecsPalette -> pr/pg/pb nibbles
    lea _ecsPalette,a2
    lea pr,a3
    lea pg,a4
    lea pb,a5
    moveq #0,d7
    move.b _numColors,d7
    subq.w #1,d7
    blt .unpack_done

.unpack_loop:
    move.w (a2)+,d0
    move.l d0,d1
    move.l d0,d2
    lsr.w #8,d1
    and.w #15,d1
    lsr.w #4,d2
    and.w #15,d2
    and.w #15,d0
    move.b d1,(a3)+
    move.b d2,(a4)+
    move.b d0,(a5)+
    dbra d7,.unpack_loop

.unpack_done:
    ; palette >> 4 -> pal4r/g/b
    lea pal4r,a3
    lea pal4g,a4
    lea pal4b,a5
    move.w #255,d7

.pack_loop:
    move.b (a0)+,d0
    move.b (a0)+,d1
    move.b (a0)+,d2
    lsr.b #4,d0
    lsr.b #4,d1
    lsr.b #4,d2
    move.b d0,(a3)+
    move.b d1,(a4)+
    move.b d2,(a5)+
    dbra d7,.pack_loop

    ; nearest color
    lea pal4r,a3
    lea pal4g,a4
    lea pal4b,a5
    lea pr,a6
    moveq #0,d6
    move.b _numColors,d6
    move.w d6,2(sp)
    move.w #255,(sp)

.outer_loop:
    ; r,g,b
    moveq #0,d0
    move.b (a3)+,d0
    moveq #0,d1
    move.b (a4)+,d1
    moveq #0,d2
    move.b (a5)+,d2

    ; bestDist=MAX, best=0
    move.l #$7FFFFFFF,d3
    moveq #0,d4
    move.l a6,a2
    move.w 2(sp),d6
    move.w d6,d5
    subq.w #1,d5
    blt .inner_done

.inner_loop:
    ; NTSC-weighted squared distance
    moveq #0,d7
    move.b (a2),d7
    sub.l d0,d7
    muls.w d7,d7
    muls.w #30,d7

    ; dg*dg*59
    moveq #0,d6
    move.b 64(a2),d6
    sub.l d1,d6
    muls.w d6,d6
    muls.w #59,d6
    add.l d6,d7

    ; db*db*11
    moveq #0,d6
    move.b 128(a2),d6
    sub.l d2,d6
    muls.w d6,d6
    muls.w #11,d6
    add.l d6,d7

    ; if (dist < bestDist) best = j
    cmp.l d3,d7
    bge .no_better
    move.l d7,d3
    move.w 2(sp),d6
    subq.w #1,d6
    sub.w d5,d6
    move.w d6,d4

.no_better:
    ; exact match: stop
    tst.l d3
    beq .inner_done

    addq.l #1,a2
    dbra d5,.inner_loop

.inner_done:
    move.b d4,(a1)+
    subq.w #1,(sp)
    bpl .outer_loop

    addq.l #4,sp
    movem.l (sp)+,d2-d7/a2-a6
    rts


; void ecsRemapLightmap_asm(int depth, const uint8 *orig, uint8 *lightmap, const uint8 *remap)
; d0=depth, a0=orig, a1=lightmap, a2=remap

Q_LUMA = 0
Q_SAT = 2
Q_NR = 4
Q_NG = 6
Q_NB = 8
Q_R = 10
Q_G = 12
Q_B = 14

    section .bss

qCap: ds.l 1
qPrev: ds.l 1
firstIdx: ds.w 256
qent: ds.b 64*16
fam: ds.w 64
qFamN: ds.w 1
qPad: ds.w 1


    section .text,code

; buildFamily: d0=r d1=g d2=b -> fam, clobbers d0-d7/a3-a5

buildFamily:
    ; 0=ar 2=ag 4=ab 6=aLuma
    subq.l #8,sp
    move.w d0,(sp)
    move.w d1,2(sp)
    move.w d2,4(sp)

    ; aLuma = 30r+59g+11b
    move.w d0,d3
    muls.w #30,d3
    move.w d1,d4
    muls.w #59,d4
    add.w d4,d3
    move.w d2,d4
    muls.w #11,d4
    add.w d4,d3
    move.w d3,6(sp)

    ; deviations -> d0,d1,d2
    move.w (sp),d0
    add.w d0,d0
    sub.w 2(sp),d0
    sub.w 4(sp),d0
    move.w 2(sp),d1
    add.w d1,d1
    sub.w (sp),d1
    sub.w 4(sp),d1
    move.w d0,d2
    add.w d1,d2
    neg.w d2

    ; sat = max-min
    move.w (sp),d3
    move.w (sp),d4
    cmp.w 2(sp),d3
    bge .bf_mx1
    move.w 2(sp),d3

.bf_mx1:
    cmp.w 4(sp),d3
    bge .bf_mx2
    move.w 4(sp),d3

.bf_mx2:
    cmp.w 2(sp),d4
    ble .bf_mn1
    move.w 2(sp),d4

.bf_mn1:
    cmp.w 4(sp),d4
    ble .bf_mn2
    move.w 4(sp),d4

.bf_mn2:
    sub.w d4,d3
    beq .bf_dir0
    ext.l d0
    lsl.l #6,d0
    divs.w d3,d0
    ext.l d1
    lsl.l #6,d1
    divs.w d3,d1
    ext.l d2
    lsl.l #6,d2
    divs.w d3,d2
    bra .bf_sel

.bf_dir0:
    moveq #0,d0
    moveq #0,d1
    moveq #0,d2

.bf_sel:
    cmp.w #2,d3
    ble .bf_neutral

    move.l #$7FFFFFFF,d6
    lea qent,a4
    lea fam,a3
    moveq #0,d7
    moveq #0,d5
    move.b _numColors,d5
    subq.w #1,d5
    blt .bf_done

.bf_c1_loop:
    cmpi.w #2,Q_SAT(a4)
    ble .bf_c1_skip
    move.w Q_NR(a4),d4
    sub.w d0,d4
    bpl .bf_c1_a1
    neg.w d4

.bf_c1_a1:
    move.w Q_NG(a4),d3
    sub.w d1,d3
    bpl .bf_c1_a2
    neg.w d3

.bf_c1_a2:
    add.w d3,d4
    move.w Q_NB(a4),d3
    sub.w d2,d3
    bpl .bf_c1_a3
    neg.w d3

.bf_c1_a3:
    add.w d3,d4
    cmp.w #48,d4
    bgt .bf_c1_skip
    moveq #0,d3
    move.w Q_LUMA(a4),d3
    cmp.l d6,d3
    bge .bf_c1_nd
    move.l d3,d6

.bf_c1_nd:
    move.w d7,(a3)+

.bf_c1_skip:
    lea 16(a4),a4
    addq.w #1,d7
    dbra d5,.bf_c1_loop

    cmp.l #$7FFFFFFF,d6
    bne .bf_c1_have

    ; darkestLuma = aLuma
    moveq #0,d6
    move.w 6(sp),d6

.bf_c1_have:
    ; darker neutrals
    lea qent,a4
    moveq #0,d7
    moveq #0,d5
    move.b _numColors,d5
    subq.w #1,d5
    blt .bf_done

.bf_c2_loop:
    cmpi.w #2,Q_SAT(a4)
    bgt .bf_c2_skip
    moveq #0,d3
    move.w Q_LUMA(a4),d3
    cmp.l d6,d3
    bgt .bf_c2_skip
    move.w d7,(a3)+

.bf_c2_skip:
    lea 16(a4),a4
    addq.w #1,d7
    dbra d5,.bf_c2_loop
    bra .bf_done

.bf_neutral:
    ; neutrals
    lea qent,a4
    lea fam,a3
    moveq #0,d7
    moveq #0,d5
    move.b _numColors,d5
    subq.w #1,d5
    blt .bf_done

.bf_n_loop:
    cmpi.w #2,Q_SAT(a4)
    bgt .bf_n_skip
    move.w d7,(a3)+

.bf_n_skip:
    lea 16(a4),a4
    addq.w #1,d7
    dbra d5,.bf_n_loop

.bf_done:
    ; n = (a3 - fam) / 2
    move.l a3,d5
    lea fam,a4
    move.l a4,d4
    sub.l d4,d5
    asr.l #1,d5
    bne .bf_save

    ; empty: nearest color
    lea qent,a4
    moveq #0,d7
    move.l #$7FFFFFFF,d4
    moveq #0,d3
    moveq #0,d5
    move.b _numColors,d5
    subq.w #1,d5
    blt .bf_f_done

.bf_f_loop:
    move.w (sp),d0
    sub.w Q_R(a4),d0
    muls.w d0,d0
    muls.w #30,d0
    move.w 2(sp),d1
    sub.w Q_G(a4),d1
    muls.w d1,d1
    muls.w #59,d1
    add.l d1,d0
    move.w 4(sp),d1
    sub.w Q_B(a4),d1
    muls.w d1,d1
    muls.w #11,d1
    add.l d1,d0
    cmp.l d4,d0
    bge .bf_f_skip
    move.l d0,d4
    move.l d7,d3

.bf_f_skip:
    lea 16(a4),a4
    addq.l #1,d7
    dbra d5,.bf_f_loop

.bf_f_done:
    lea fam,a3
    move.w d3,(a3)
    moveq #1,d5

.bf_save:
    move.w d5,qFamN
    addq.l #8,sp
    rts


; pickStep: d0=tr d1=tg d2=tb -> d0 = idx, clobbers d0-d7/a3-a5

pickStep:
    move.l a6,-(sp)
    ; 0=bestDist 4=best 8=minLuma 12=minIdx 16=prevDist
    lea -20(sp),sp
    move.l #$7FFFFFFF,(sp)
    move.l #-1,4(sp)
    move.l #$7FFFFFFF,8(sp)
    move.l #$7FFFFFFF,16(sp)
    move.l qCap,d3
    move.l qPrev,d4
    lea fam,a3
    lea qent,a5
    moveq #0,d5
    move.w (a3),d5
    move.l d5,12(sp)
    moveq #0,d5
    move.w qFamN,d5
    add.w d5,d5
    move.l a3,a4
    adda.l d5,a4

.ps_loop:
    moveq #0,d7
    move.w (a3)+,d7
    moveq #0,d6
    move.w d7,d6
    lsl.w #4,d6
    moveq #0,d5
    move.w (a5,d6.l),d5
    cmp.l 8(sp),d5
    bge .ps_nmin
    move.l d5,8(sp)
    move.l d7,12(sp)

.ps_nmin:
    cmp.l d3,d5
    bgt .ps_next

    ; weighted squared distance
    move.w d0,d5
    sub.w Q_R(a5,d6.l),d5
    muls.w d5,d5
    muls.w #30,d5
    move.l d5,a6
    move.w d1,d5
    sub.w Q_G(a5,d6.l),d5
    muls.w d5,d5
    muls.w #59,d5
    adda.l d5,a6
    move.w d2,d5
    sub.w Q_B(a5,d6.l),d5
    muls.w d5,d5
    muls.w #11,d5
    adda.l d5,a6

    cmp.l d4,d7
    bne .ps_nprev
    move.l a6,16(sp)

.ps_nprev:
    cmpa.l (sp),a6
    bge .ps_next
    move.l a6,(sp)
    move.l d7,4(sp)

.ps_next:
    cmpa.l a4,a3
    bne .ps_loop

    move.l 4(sp),d0
    bge .ps_have
    move.l 12(sp),d0

.ps_have:
    tst.l d4
    blt .ps_set
    move.l 16(sp),d5
    cmp.l #$7FFFFFFF,d5
    beq .ps_set
    move.l (sp),d6
    add.l d6,d6
    cmp.l d6,d5
    bgt .ps_set
    move.l d4,d0

.ps_set:
    move.l d0,qPrev
    moveq #0,d6
    move.w d0,d6
    lsl.w #4,d6
    moveq #0,d5
    move.w (a5,d6.l),d5
    move.l d5,qCap
    lea 20(sp),sp
    move.l (sp)+,a6
    rts


_ecsRemapLightmap_asm:
ecsRemapLightmap_asm:
    movem.l d2-d7/a2-a6,-(sp)
    ; (sp)=SOFF 2(sp)=SCNT
    subq.l #4,sp

    ; if (depth <= 0 || depth > 8) return
    tst.l d0
    ble .ret
    cmp.l #8,d0
    bgt .ret

    ; numColors = 1 << depth
    moveq #1,d3
    asl.l d0,d3
    move.b d3,_numColors
    moveq #0,d3
    move.b _numColors,d3
    cmp.l #64,d3
    bgt .fallback

    ; unpack ecsPalette -> qent
    lea _ecsPalette,a4
    lea qent,a3
    moveq #0,d7
    move.b _numColors,d7
    subq.w #1,d7
    blt .qe_done

.qe_loop:
    move.w (a4)+,d0
    move.w d0,d3
    lsr.w #8,d3
    move.w d0,d4
    lsr.w #4,d4
    and.w #15,d4
    and.w #15,d0
    move.w d3,Q_R(a3)
    move.w d4,Q_G(a3)
    move.w d0,Q_B(a3)

    ; luma
    move.w d3,d5
    muls.w #30,d5
    move.w d4,d6
    muls.w #59,d6
    add.w d6,d5
    move.w d0,d6
    muls.w #11,d6
    add.w d6,d5
    move.w d5,Q_LUMA(a3)

    ; sat = max-min
    move.w d3,d5
    cmp.w d4,d5
    bge .qe_mx1
    move.w d4,d5

.qe_mx1:
    cmp.w d0,d5
    bge .qe_mx2
    move.w d0,d5

.qe_mx2:
    move.w d3,d6
    cmp.w d4,d6
    ble .qe_mn1
    move.w d4,d6

.qe_mn1:
    cmp.w d0,d6
    ble .qe_mn2
    move.w d0,d6

.qe_mn2:
    sub.w d6,d5
    move.w d5,Q_SAT(a3)
    beq .qe_dir0

    ; chroma dir = dev*64/sat
    move.w d3,d1
    add.w d1,d1
    sub.w d4,d1
    sub.w d0,d1
    move.w d4,d2
    add.w d2,d2
    sub.w d3,d2
    sub.w d0,d2
    move.w d1,d6
    add.w d2,d6
    neg.w d6
    ext.l d1
    lsl.l #6,d1
    divs.w d5,d1
    ext.l d2
    lsl.l #6,d2
    divs.w d5,d2
    ext.l d6
    lsl.l #6,d6
    divs.w d5,d6
    move.w d1,Q_NR(a3)
    move.w d2,Q_NG(a3)
    move.w d6,Q_NB(a3)
    bra .qe_next

.qe_dir0:
    clr.w Q_NR(a3)
    clr.w Q_NG(a3)
    clr.w Q_NB(a3)

.qe_next:
    lea 16(a3),a3
    dbra d7,.qe_loop

.qe_done:
    suba.l a6,a6

.col_loop:
    ; anchor = pal[c*3] >> 4
    move.l _paletteCurrent,a4
    move.l a6,d6
    move.l a6,d5
    add.l d6,d6
    add.l d5,d6
    adda.l d6,a4
    moveq #0,d0
    move.b (a4)+,d0
    lsr.b #4,d0
    moveq #0,d1
    move.b (a4)+,d1
    lsr.b #4,d1
    moveq #0,d2
    move.b (a4),d2
    lsr.b #4,d2
    bsr buildFamily

    move.l #$7FFFFFFF,qCap
    move.l #-1,qPrev
    ; SOFF = c + s*256
    move.l a6,d6
    move.w d6,(sp)
    move.w #31,2(sp)

.s_loop:
    ; t = orig[(s<<8)|c]
    move.w (sp),d6
    moveq #0,d7
    move.b (a0,d6.w),d7
    
    ; target = pal[t*3] >> 4
    move.l _paletteCurrent,a4
    move.w d7,d5
    add.w d5,d5
    add.w d7,d5
    adda.w d5,a4
    moveq #0,d0
    move.b (a4)+,d0
    lsr.b #4,d0
    moveq #0,d1
    move.b (a4)+,d1
    lsr.b #4,d1
    moveq #0,d2
    move.b (a4),d2
    lsr.b #4,d2
    bsr pickStep
    move.w (sp),d6
    move.b d0,(a1,d6.w)
    addi.w #256,(sp)
    subq.w #1,2(sp)
    bpl .s_loop

    addq.l #1,a6
    cmpa.l #256,a6
    bne .col_loop

    ; firstIdx[i] = -1
    lea firstIdx,a3
    moveq #0,d3
    move.b _numColors,d3
    move.w d3,d4
    subq.w #1,d4
    blt .init_done

.init_loop:
    move.w #-1,(a3)+
    dbra d4,.init_loop

.init_done:
    ; first occurrence of each mapped color
    lea firstIdx,a3
    moveq #0,d4
    move.w #255,d6

.fill_loop:
    moveq #0,d7
    move.b (a2)+,d7
    cmp.w d3,d7
    bge .fill_next
    move.w (a3,d7.l*2),d5
    bge .fill_next
    move.w d4,(a3,d7.l*2)

.fill_next:
    addq.w #1,d4
    dbra d6,.fill_loop

    ; shadow table at 0x1A00
    suba.l a6,a6

.sh_loop:
    moveq #0,d7
    move.b _numColors,d7
    cmpa.l d7,a6
    bge .ret
    lea firstIdx,a4
    moveq #0,d7
    move.w (a4,a6.l*2),d7
    blt .sh_next

    ; anchor = ecsPalette[i]
    lea _ecsPalette,a4
    move.w (a4,a6.l*2),d0
    move.w d0,d1
    lsr.w #4,d1
    and.w #15,d1
    move.w d0,d2
    and.w #15,d2
    lsr.w #8,d0
    bsr buildFamily

    ; target = pal[orig[0x1A00+j]*3] >> 4
    lea firstIdx,a4
    moveq #0,d7
    move.w (a4,a6.l*2),d7
    moveq #0,d5
    move.b 6656(a0,d7.l),d5
    move.l _paletteCurrent,a4
    move.w d5,d6
    add.w d6,d6
    add.w d5,d6
    adda.w d6,a4
    moveq #0,d0
    move.b (a4)+,d0
    lsr.b #4,d0
    moveq #0,d1
    move.b (a4)+,d1
    lsr.b #4,d1
    moveq #0,d2
    move.b (a4),d2
    lsr.b #4,d2
    move.l #$7FFFFFFF,qCap
    move.l #-1,qPrev
    bsr pickStep
    move.b d0,6656(a1,a6.l)

.sh_next:
    addq.l #1,a6
    bra .sh_loop

.fallback:
    ; numColors > 64: plain remap
    move.l a0,a3
    move.l a1,a4
    move.l a2,a5
    move.w #8192-1,d6

.fb_loop:
    moveq #0,d7
    move.b (a3)+,d7
    move.b (a5,d7.l),d7
    move.b d7,(a4)+
    dbra d6,.fb_loop

.ret:
    addq.l #4,sp
    movem.l (sp)+,d2-d7/a2-a6
    rts


; void ecsbuildFillmap_asm(void)
_ecsbuildFillmap_asm:
ecsbuildFillmap_asm:
    movem.l d2-d7/a2-a6,-(sp)

    ; pal4r/g/b[256] = paletteCurrent[i*3+0..2] >> 4
    move.l _paletteCurrent,a0
    lea pal4r,a1
    lea pal4g,a2
    lea pal4b,a3
    move.w #255,d7

.fm_pre:
    moveq #0,d0
    move.b (a0)+,d0
    lsr.b #4,d0
    move.b d0,(a1)+
    moveq #0,d1
    move.b (a0)+,d1
    lsr.b #4,d1
    move.b d1,(a2)+
    moveq #0,d2
    move.b (a0)+,d2
    lsr.b #4,d2
    move.b d2,(a3)+
    dbra d7,.fm_pre

    lea _ecsPalette,a0
    lea pal4r,a1
    lea _gInvQuant,a4

    moveq #0,d6
    move.b _numColors,d6
    move.l d6,a3
    sub.l a6,a6

.fm_inv_outer:
    cmp.l a3,a6
    bge .fm_inv_pass

    ; qr/qg/qb from ecsPalette[q]
    move.w (a0,a6.l*2),d7
    move.l d7,d0
    lsr.w #8,d0
    and.w #15,d0
    move.l d7,d1
    lsr.w #4,d1
    and.w #15,d1
    and.w #15,d7
    move.l d7,d2

    ; best=pal4r[0] ptr, bestDist=MAX
    move.l a1,d3
    move.l #$7FFFFFFF,d4

    move.l a1,a5
    move.w #255,d5

.fm_inv_inner:
    ; dr = pal4r[i] - qr, acc = dr*dr*30
    sub.l a2,a2
    moveq #0,d6
    move.b (a5),d6
    sub.l d0,d6
    muls.w d6,d6
    
    ; *30 = *32 - *2
    move.l d6,d7
    lsl.l #5,d6
    add.l d7,d7
    sub.l d7,d6
    add.l d6,a2

    ; dg = pal4g[i] - qg, acc += dg*dg*59
    moveq #0,d6
    move.b 256(a5),d6
    sub.l d1,d6
    muls.w d6,d6

    ; *59 = *64 - *5 = *64 - (*4 + *1)
    move.l d6,d7
    lsl.l #2,d7
    add.l d6,d7
    lsl.l #6,d6
    sub.l d7,d6
    add.l d6,a2

    ; db = pal4b[i] - qb, acc += db*db*11
    moveq #0,d6
    move.b 512(a5),d6
    sub.l d2,d6
    muls.w d6,d6

    ; *11 = *8 + *3 = *8 + (*2 + *1)
    move.l d6,d7
    lsl.l #1,d7
    add.l d6,d7
    lsl.l #3,d6
    add.l d7,d6
    add.l d6,a2

    cmp.l d4,a2
    bge .fm_inv_nobetter
    move.l a2,d4
    move.l a5,d3

.fm_inv_nobetter:
    tst.l d4
    beq .fm_inv_inner_done

    addq.l #1,a5
    dbra d5,.fm_inv_inner

.fm_inv_inner_done:
    ; gInvQuant[q] = best - pal4r
    move.l d3,d6
    sub.l a1,d6
    move.b d6,(a4,a6.l)
    bra .fm_inv_next

.fm_inv_pass:
    ; q >= numColors: gInvQuant[q] = q
    move.l a6,d6
    move.b d6,(a4,d6.l)

.fm_inv_next:
    addq.l #1,a6
    cmp.l #256,a6
    blt .fm_inv_outer

    ; fillmap: 32 shades x 256 entries
    lea _gLightmap,a0
    lea _gFillmapData,a1
    move.l a3,d3
    moveq #31,d7

.fm_fm_outer:
    lea _gInvQuant,a2
    move.l a0,a3
    move.l a1,a4

    ; q < numColors: dst[q] = src[gInvQuant[q]]
    move.l d3,d5
    beq .fm_fm_q2
    subq.l #1,d5
    moveq #0,d0

.fm_fm_q1:
    move.b (a2)+,d0
    move.b (a3,d0.w),(a4)+
    dbra d5,.fm_fm_q1

.fm_fm_q2:
    ; q >= numColors: dst[q] = q
    move.l d3,d0
    moveq #0,d5
    move.w #255,d5
    sub.w d3,d5
    blt .fm_fm_next

    ; align to long boundary
    move.l d0,d1
    neg.w d1
    and.w #3,d1
    beq .fm_fm_q2_check

.fm_fm_q2_align:
    move.b d0,(a4)+
    addq.l #1,d0
    subq.l #1,d5
    subq.l #1,d1
    bne .fm_fm_q2_align

.fm_fm_q2_check:
    tst.l d5
    blt .fm_fm_next

    ; pack 4 sequential bytes into long: q*0x01010101 + 0x00010203
    move.l d0,d6
    lsl.l #8,d6
    or.l d0,d6
    lsl.l #8,d6
    or.l d0,d6
    lsl.l #8,d6
    or.l d0,d6
    add.l #$00010203,d6

    move.l #$04040404,d2
    lsr.l #2,d5

.fm_fm_q2_batch:
    move.l d6,(a4)+
    add.l d2,d6
    dbra d5,.fm_fm_q2_batch

.fm_fm_next:
    lea 256(a0),a0
    lea 256(a1),a1
    dbra d7,.fm_fm_outer

    lea _gFillmapData,a0
    move.l a0,_gFillmap

    movem.l (sp)+,d2-d7/a2-a6
    rts

