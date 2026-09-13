;
; OpenLara - Amiga port
;
; 68020+
;

    section .text,code

    XDEF _flush_asm
    XDEF flush_asm

    XREF _gVertices
    XREF _gVerticesBase
    XREF _gFaces
    XREF _gFacesBase
    XREF _gOT
    XREF _level
    XREF _gTile
    XREF _fb
    XREF _drawPoly_asm
    XREF _rasterizeS_asm
    XREF _rasterizeF_asm
    XREF _rasterizeFT_asm
    XREF _rasterizeFTA_asm
    XREF _rasterizeGT_asm
    XREF _rasterizeGTA_asm
    XREF _rasterizeSprite_asm
    XREF _rasterizeFillS_asm
    XREF _rasterizeLineH_asm
    XREF _rasterizeLineV_asm

; VertexLink layout (16 bytes)
; Vertex v: x@0, y@2, z@4, g@6, clip@7
; TexCoord t: v@8, u@10 (union with uint32 t@8)
; int8 prev@12, int8 next@13, uint16 padding@14
VL_Y = 2

; Face layout (16 bytes)
; uint32 flags@0, Face* next@4, int16 indices[4]@8
FC_FLAGS = 0
FC_NEXT = 4
FC_IDX0 = 8
FC_IDX1 = 10
FC_IDX2 = 12
FC_IDX3 = 14

; Texture layout (12 bytes)
; uint32 tile@0, uint32 uv01@4, uint32 uv23@8
TEX_TILE = 0
TEX_UV01 = 4
TEX_UV23 = 8

; Sprite layout (16 bytes)
; uint32 tile@0, uint32 uwvh@4, int16 l@8, t@10, r@12, b@14
SPR_TILE = 0
SPR_UWVH = 4

; Level struct offsets
LVL_TEXTURES = 92
LVL_SPRITES = 96

; Flag bits
FACE_TRIANGLE = $80000
FACE_CLIPPED = $40000
FACE_TEX_MASK = $3FFF

; OT_SIZE = 641, last slot at _gOT + 640*4
OT_LAST_OFF = 2560

; void flush_asm()
; Iterate OT back-to-front, setup VertexLink per face, dispatch to rasterizer
; a2=face, a4=level.textures, a5=gVertices, a6=v (stack)
; d3=gOT base, d5=type, d6=flags, d7=OT slot ptr
_flush_asm:
flush_asm:
    movem.l d2-d7/a2-a6,-(sp)
    sub.l #128,sp

    lea _gVertices,a0
    move.l a0,_gVerticesBase

    move.l _gFacesBase,d0
    lea _gFaces,a0
    cmp.l a0,d0
    beq .done

    move.l a0,_gFacesBase

    ; VertexLink prev/next packed as word (prev<<8 | next)
    move.w #$0301,12(sp)
    move.w #$FF01,28(sp)
    move.w #$FF01,44(sp)
    move.w #$FFFD,60(sp)
    move.w #$0201,76(sp)
    move.w #$FF01,92(sp)
    move.w #$FFFE,108(sp)

    lea _level,a0
    move.l LVL_TEXTURES(a0),a4

    lea _gVertices,a5

    move.l sp,a6

    move.l #_gOT,d3
    move.l d3,d7
    add.l #OT_LAST_OFF,d7

.ot_loop:
    move.l d7,a0
    move.l (a0),a2
    tst.l a2
    beq .ot_next
    clr.l (a0)

.face_loop:
    move.l FC_FLAGS(a2),d6

    ; type = (flags >> 14) & 15
    move.l d6,d5
    lsr.l #7,d5
    lsr.l #7,d5
    and.l #15,d5

    cmp.l #5,d5
    bhi .else_branch

    ; d4 = ptr offset: 0=quad, 64=triangle
    moveq #0,d4
    btst #19,d6
    beq .ptr_set
    moveq #64,d4

.ptr_set:
    ; textured if type > 1
    cmp.l #1,d5
    bls .no_texture

    ; tex = &level.textures[flags & 0x3FFF]
    move.l d6,d0
    and.l #FACE_TEX_MASK,d0
    lea (a4,d0.l*8),a1
    lea (a1,d0.l*4),a0

    move.l TEX_TILE(a0),_gTile

    ; UV coords: mask 0xFF00FF00, shift 8 for odd vertices
    move.l TEX_UV01(a0),d0
    and.l #$FF00FF00,d0
    move.l d0,8(a6,d4.l)

    move.l TEX_UV01(a0),d0
    asl.l #8,d0
    and.l #$FF00FF00,d0
    move.l d0,24(a6,d4.l)

    move.l TEX_UV23(a0),d0
    and.l #$FF00FF00,d0
    move.l d0,40(a6,d4.l)

    move.l TEX_UV23(a0),d0
    asl.l #8,d0
    and.l #$FF00FF00,d0
    move.l d0,56(a6,d4.l)

.no_texture:
    ; copy vertices from gVertices by index
    moveq #0,d0
    move.w FC_IDX0(a2),d0
    lea (a5,d0.l*8),a0
    move.l (a0),(a6,d4.l)
    move.l 4(a0),4(a6,d4.l)

    moveq #0,d0
    move.w FC_IDX1(a2),d0
    lea (a5,d0.l*8),a0
    move.l (a0),16(a6,d4.l)
    move.l 4(a0),20(a6,d4.l)

    moveq #0,d0
    move.w FC_IDX2(a2),d0
    lea (a5,d0.l*8),a0
    move.l (a0),32(a6,d4.l)
    move.l 4(a0),36(a6,d4.l)

    btst #19,d6
    bne .skip_4th
    moveq #0,d0
    move.w FC_IDX3(a2),d0
    lea (a5,d0.l*8),a0
    move.l (a0),48(a6,d4.l)
    move.l 4(a0),52(a6,d4.l)

.skip_4th:
    btst #18,d6
    bne .clipped

    ; top vertex = minimum y
    lea (a6,d4.l),a1
    move.w VL_Y(a1),d0

    cmp.w 18(a6,d4.l),d0
    ble .top1
    lea 16(a6,d4.l),a1
    move.w VL_Y(a1),d0

.top1:
    cmp.w 34(a6,d4.l),d0
    ble .top2
    lea 32(a6,d4.l),a1
    move.w VL_Y(a1),d0

.top2:
    btst #19,d6
    bne .do_raster
    cmp.w 50(a6),d0
    ble .do_raster
    lea 48(a6,d4.l),a1

.do_raster:
    bra .rasterize_call

.clipped:
    move.l d6,d0
    lea (a6,d4.l),a0
    jsr _drawPoly_asm
    bra .next_face

; type >= 6: SPRITE, FILL_S, LINE_H, LINE_V
.else_branch:
    moveq #0,d0
    move.w FC_IDX0(a2),d0
    lea (a5,d0.l*8),a0
    move.l (a0),(a6)
    move.l 4(a0),4(a6)
    move.l 8(a0),16(a6)
    move.l 12(a0),20(a6)

    cmp.l #6,d5
    bne .sprite_call

    ; sprite = &level.sprites[flags & 0x3FFF]
    move.l d6,d0
    and.l #FACE_TEX_MASK,d0
    asl.l #4,d0
    lea _level,a0
    move.l LVL_SPRITES(a0),a0
    lea (a0,d0.l),a0

    move.l SPR_TILE(a0),_gTile

    ; UV: uwvh & 0xFF00FF00 for v[0], & 0x00FF00FF for v[1]
    move.l SPR_UWVH(a0),d0
    move.l d0,d1
    and.l #$FF00FF00,d0
    move.l d0,8(a6)
    and.l #$00FF00FF,d1
    move.l d1,24(a6)

.sprite_call:
    move.l a6,a1

; a1=top, d5=type, d6=flags
; a2 clobbered by R arg, save face ptr in d2
.rasterize_call:
    move.l a2,d2

    ; pixel = fb + top->v.y * 320
    moveq #0,d0
    move.w VL_Y(a1),d0
    move.l d0,d1
    asl.l #6,d1
    asl.l #8,d0
    add.l d1,d0
    lea _fb,a0
    add.l d0,a0

    ; R = (type==1) ? (flags & 0xFF) : top
    move.l a1,a2
    cmp.l #1,d5
    bne .R_ok
    move.l d6,d0
    and.l #$FF,d0
    move.l d0,a2

.R_ok:
    ; dispatch via jump table indexed by type
    move.l d5,d0
    asl.l #2,d0
    lea .jt(pc),a3
    move.l (a3,d0.l),a3
    jsr (a3)

    move.l d2,a2

.next_face:
    move.l FC_NEXT(a2),a2
    tst.l a2
    bne .face_loop

.ot_next:
    subq.l #4,d7
    cmp.l d3,d7
    blt .done
    bra .ot_loop

.done:
    add.l #128,sp
    movem.l (sp)+,d2-d7/a2-a6
    rts

.jt:
    dc.l _rasterizeS_asm
    dc.l _rasterizeF_asm
    dc.l _rasterizeFT_asm
    dc.l _rasterizeFTA_asm
    dc.l _rasterizeGT_asm
    dc.l _rasterizeGTA_asm
    dc.l _rasterizeSprite_asm
    dc.l _rasterizeFillS_asm
    dc.l _rasterizeLineH_asm
    dc.l _rasterizeLineV_asm
