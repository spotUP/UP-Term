; amiga_render_68k.s -- the renderer's hot loops in 68000 assembler (ledger
; S1). Linked where VR_ASM is defined (the handler); the C beside each call
; in amiga_render.c is the reference.
;
; vbcc's convention: arguments on the stack, d0/d1/a0/a1 free.

	section	"CODE",code

	xdef	_vr_asm_cell

; void vr_asm_cell(UBYTE **planes, long depth, long off, long bpr,
;                  const UBYTE *rows, long h, long fg, long bg, long mask)
;
; direct_cell's plane loop: one character cell, 8 pixels wide and h rows
; high, straight into the bitplanes at byte offset `off`. rows[0..h) is
; the glyph. In each plane the cell's bytes are the glyph where only the
; foreground pen has that plane's bit, its inverse where only the
; background has it, $ff or 0 where both or neither do. Planes whose bit
; is clear in `mask` are not touched (they hold zeros; see vr_render.mask).
_vr_asm_cell:
	movem.l	d2-d7/a2-a4,-(sp)	; 9 registers: the arguments from 40(sp)
	move.l	40(sp),a0		; planes
	move.l	44(sp),d0		; depth
	lsl.l	#2,d0
	lea	0(a0,d0.l),a4		; the end of the plane pointers
	move.l	48(sp),d1		; off
	move.l	52(sp),d2		; bpr
	move.l	56(sp),a3		; rows
	move.l	60(sp),d3		; h
	move.l	64(sp),d4		; fg
	move.l	68(sp),d5		; bg
	move.l	72(sp),d6		; mask
.plane:	move.l	(a0)+,a2
	lsr.l	#1,d6
	bcc.s	.next			; a plane not in use
	add.l	d1,a2
	move.l	d3,d0
	move.l	a3,a1
	btst	#0,d4
	bne.s	.fgset
	btst	#0,d5
	bne.s	.inv
.zero:	clr.b	(a2)			; neither pen
	add.l	d2,a2
	subq.l	#1,d0
	bne.s	.zero
	bra.s	.next
.fgset:	btst	#0,d5
	bne.s	.ones
.copy:	move.b	(a1)+,(a2)		; the foreground only: the glyph
	add.l	d2,a2
	subq.l	#1,d0
	bne.s	.copy
	bra.s	.next
.ones:	st	(a2)			; both pens
	add.l	d2,a2
	subq.l	#1,d0
	bne.s	.ones
	bra.s	.next
.inv:	move.b	(a1)+,d7		; the background only: the glyph's inverse
	not.b	d7
	move.b	d7,(a2)
	add.l	d2,a2
	subq.l	#1,d0
	bne.s	.inv
.next:	lsr.l	#1,d4
	lsr.l	#1,d5
	cmp.l	a4,a0
	bne.s	.plane
	movem.l	(sp)+,d2-d7/a2-a4
	rts
