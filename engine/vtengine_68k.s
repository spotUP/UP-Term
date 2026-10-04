; vtengine_68k.s -- the engine's hot loops in 68000 assembler (ledger S1;
; the C in vtengine.c is the reference and what every other build uses:
; these are linked only where VT_ASM is defined). Plain 68000 code: the
; engine is also built for a 68000 (DCTelnet).
;
; vbcc's convention: arguments on the stack, the result in d0, d0/d1/a0/a1
; free, the rest kept.
;
; vt_cell (vtengine.h; vt_asm_layout in vtengine.c fails the build when
; this is no longer its layout):
;    0 fg.l   4 bg.l   8 ch.w   10 attr.w   12 width.b   13 deco.b
;   14 ext.b  15 pad.b                                    16 bytes

	section	"CODE",code

	xdef	_vt_asm_put_run
	xdef	_vt_asm_fill
	xdef	_vt_asm_rows_up
	xdef	_vt_asm_cells_move

; void vt_asm_cells_move(vt_cell *dst, const vt_cell *src, long n)
;
; n cells from src to dst, which may overlap (insert / delete character:
; the C library's memmove moved 1136 bytes one at a time, 1.6 ms).
_vt_asm_cells_move:
	move.l	4(sp),a0		; dst
	move.l	8(sp),a1		; src
	move.l	12(sp),d0		; n
	ble.s	.mnone
	cmp.l	a1,a0
	bhi.s	.mback			; dst above src: from the end
.mfwd:	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	subq.l	#1,d0
	bne.s	.mfwd
.mnone:	rts
.mback:	move.l	d0,d1
	lsl.l	#4,d1			; n cells of 16 bytes
	add.l	d1,a0
	add.l	d1,a1
.mbk:	move.l	-(a1),-(a0)
	move.l	-(a1),-(a0)
	move.l	-(a1),-(a0)
	move.l	-(a1),-(a0)
	subq.l	#1,d0
	bne.s	.mbk
	rts
	xdef	_vt_asm_rows_down

; void vt_asm_rows_up(vt_line **p, long k):   p[0] = p[1] ... k times, upwards
; void vt_asm_rows_down(vt_line **p, long k): p[0] = p[-1] ... k times, downwards
; (scroll_up / scroll_down: the row pointers of a scrolling region)
_vt_asm_rows_up:
	move.l	4(sp),a0
	move.l	8(sp),d0
	ble.s	.unone
	lea	4(a0),a1
.urow:	move.l	(a1)+,(a0)+
	subq.l	#1,d0
	bne.s	.urow
.unone:	rts

_vt_asm_rows_down:
	move.l	4(sp),a0
	move.l	8(sp),d0
	ble.s	.dnone
	addq.l	#4,a0			; one past p[0]
	lea	-4(a0),a1		; one past p[-1]
.drow:	move.l	-(a1),-(a0)
	subq.l	#1,d0
	bne.s	.drow
.dnone:	rts

; void vt_asm_fill(vt_cell *c, long n, const vt_cell *proto)
;
; cells_blank's loop: n copies of proto from c.
_vt_asm_fill:
	move.l	4(sp),a0		; c
	move.l	8(sp),d0		; n
	move.l	12(sp),a1		; proto
	ble.s	.fnone			; (the flags are n's: a1's move does not set them)
	movem.l	d2-d4,-(sp)
	move.l	(a1)+,d1
	move.l	(a1)+,d2
	move.l	(a1)+,d3
	move.l	(a1),d4
.fcell:	move.l	d1,(a0)+
	move.l	d2,(a0)+
	move.l	d3,(a0)+
	move.l	d4,(a0)+
	subq.l	#1,d0
	bne.s	.fcell
	movem.l	(sp)+,d2-d4
.fnone:	rts

; long vt_asm_put_run(vt_cell *c, const vt_u8 *b, long n, const vt_cell *proto)
;
; put_ascii_run's loop: the bytes b[0..n) into the cells from c, each cell
; proto with the byte as its character. It stops before a byte that is
; not printable ASCII ($20..$7e), and before a cell that is
; not a plain one (width != 1: half of a wide glyph) or whose right
; neighbour is the second half of one (width 0); c[n] is always inside
; the row (the caller leaves the row's last column out). The number of
; cells written.
_vt_asm_put_run:
	movem.l	d2-d5/a2,-(sp)
	move.l	24(sp),a0		; c
	move.l	28(sp),a1		; b
	move.l	32(sp),d0		; n
	move.l	36(sp),a2		; proto
	move.l	d0,d5
	ble.s	.none
	move.l	(a2),d1			; fg
	move.l	4(a2),d2		; bg
	move.w	10(a2),d3		; attr
	move.l	12(a2),d4		; width (1), deco, ext, pad
	movem.l	d6-d7,-(sp)
	moveq	#0,d6
	moveq	#$5f,d7
.cell:	move.b	(a1)+,d6
	sub.b	#$20,d6			; $20..$7e -> 0..$5e
	cmp.b	d7,d6
	bcc.s	.stop7			; a control, DEL or an 8-bit byte: the parser's
	cmp.b	#1,12(a0)
	bne.s	.stop7
	tst.b	28(a0)			; the neighbour's width
	beq.s	.stop7
	add.b	#$20,d6
	move.l	d1,(a0)+
	move.l	d2,(a0)+
	move.w	d6,(a0)+		; ch
	move.w	d3,(a0)+
	move.l	d4,(a0)+
	subq.l	#1,d0
	bne.s	.cell
.stop7:	movem.l	(sp)+,d6-d7
.stop:	sub.l	d0,d5			; n less what is left
	move.l	d5,d0
	movem.l	(sp)+,d2-d5/a2
	rts
.none:	moveq	#0,d0
	movem.l	(sp)+,d2-d5/a2
	rts
