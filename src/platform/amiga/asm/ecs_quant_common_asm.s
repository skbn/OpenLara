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

    XREF _ecsPalette
    XREF _numColors


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

    section .bss

firstIdx: ds.w 256


    section .text,code

_ecsRemapLightmap_asm:
ecsRemapLightmap_asm:
    movem.l d2-d7/a2-a6,-(sp)

    ; if (depth <= 0 || depth > 8) return
    tst.l d0
    ble .ret
    cmp.l #8,d0
    bgt .ret

    ; numColors = 1 << depth
    moveq #1,d3
    asl.l d0,d3
    move.b d3,_numColors

    ; firstIdx[i] = -1
    lea firstIdx,a3
    move.w d3,d4
    subq.w #1,d4
    blt .init_done

.init_loop:
    move.w #-1,(a3)+
    dbra d4,.init_loop

.init_done:
    ; first occurrence of each mapped color
    lea firstIdx,a3
    move.l a2,a5
    moveq #0,d4
    moveq #0,d5
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

    ; remap main lightmap
    move.l a0,a3
    move.l a1,a4
    move.w #8192-1,d6

.remap_loop:
    moveq #0,d7
    move.b (a3)+,d7
    move.b (a5,d7.l),d7
    move.b d7,(a4)+
    dbra d6,.remap_loop

    ; shadow table at 0x1A00
    lea firstIdx,a3
    move.l a0,a6
    add.l #6656,a6
    move.l a1,a4
    add.l #6656,a4
    move.w d3,d6
    subq.w #1,d6
    blt .ret

.shadow_loop:
    move.w (a3)+,d7
    blt .shadow_skip
    moveq #0,d5
    move.b (a6,d7.l),d5
    move.b (a5,d5.l),d5
    move.b d5,(a4)

.shadow_skip:
    addq.l #1,a4
    dbra d6,.shadow_loop

.ret:
    movem.l (sp)+,d2-d7/a2-a6
    rts

