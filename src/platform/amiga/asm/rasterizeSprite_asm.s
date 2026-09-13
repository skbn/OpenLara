;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _rasterizeSprite_asm
    XDEF rasterizeSprite_asm

; Sprite rasterizer, transparent
_rasterizeSprite_asm:
rasterizeSprite_asm:
    movem.l d2-d7/a2-a6,-(sp)
    subq.l #4,sp

    lea 16(a2),a2

    ; ft_lightmap = &gLightmap[shade << 8]
    moveq #0,d0
    move.b 6(a1),d0
    lsl.l #8,d0
    move.l _gLightmap,a3
    lea (a3,d0.l),a3

    move.l _gTile,a4

    move.w 0(a2),d4
    move.w 0(a1),d0
    sub.w d0,d4
    ext.l d4
    ble .return
    cmp.l #1024,d4
    bgt .return

    move.w 2(a2),d2
    move.w 2(a1),d0
    sub.w d0,d2
    ext.l d2
    ble .return
    cmp.l #1024,d2
    bgt .return

    lea _divTable,a5
    move.w (a5,d4.w*2),d6
    move.w (a5,d2.w*2),d0

    move.w 8(a2),d1
    mulu.w d6,d1
    asr.l #8,d1
    move.l d1,d6

    move.w 10(a2),d1
    mulu.w d0,d1
    asr.l #8,d1
    move.l d1,0(sp)

    moveq #0,d3
    move.w 8(a1),d3
    moveq #0,d5
    move.w 10(a1),d5

    ; clip Y top
    move.w 2(a1),d0
    ext.l d0
    bge .y_top_done

    move.l d0,d1
    neg.l d1
    move.l d1,d7
    asl.l #6,d7
    asl.l #8,d1
    add.l d7,d1
    add.l d1,a0

    move.l d0,d1
    muls.l 0(sp),d1
    sub.l d1,d5

    add.l d0,d2

.y_top_done:
    ; clip Y bottom
    move.w 2(a2),d0
    cmp.w #200,d0
    ble .y_bot_done
    sub.w #200,d0
    ext.l d0
    sub.l d0,d2

.y_bot_done:
    move.l a0,a6

    tst.l d2
    ble .return

    add.w 0(a1),a6

    ; clip X left
    move.w 0(a1),d0
    ext.l d0
    bge .x_left_done

    sub.l d0,a6

    move.l d0,d1
    muls.l d6,d1
    sub.l d1,d3

    add.l d0,d4

.x_left_done:
    ; clip X right
    move.w 0(a2),d0
    cmp.w #320,d0
    ble .x_right_done
    sub.w #320,d0
    ext.l d0
    sub.l d0,d4

.x_right_done:
    tst.l d4
    ble .return

    subq.l #1,d2

.scanline_loop:
    ; xtile = tile + (v & 0xFF00)
    move.l d5,d0
    and.l #$0000FF00,d0
    lea (a4,d0.l),a2

    move.l a6,a5
    move.l d3,d1

    move.l d4,d7
    subq.l #1,d7

.pixel_loop:
    ; tile index = xu >> 8
    move.l d1,d0
    lsr.l #8,d0
    move.b (a2,d0.l),d0
    and.l #$000000FF,d0
    beq .pixel_skip

    move.b (a3,d0.l),d0
    move.b d0,(a5)

.pixel_skip:
    add.l d6,d1
    addq.l #1,a5
    dbra d7,.pixel_loop

    add.l 0(sp),d5

    lea 320(a6),a6

    dbra d2,.scanline_loop

.return:
    addq.l #4,sp
    movem.l (sp)+,d2-d7/a2-a6
    rts
