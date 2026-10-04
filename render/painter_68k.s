; painter_68k.s -- vp_span's plane loop on the 68020 (ledger S1; painter.h).
; Four cells a long: the glyph bytes of four cells side by side in a
; register, written at the span's bit phase by one BFINS -- the 68020's
; bit-field insert keeps the bits around them, so the phase costs nothing.
; Both loops fit the 256-byte instruction cache: on a stock A1200 every
; instruction fetched from outside it is a chip-RAM access.
;
; void vp_asm_plane(vp_u8 *dst, long bpr, long s, const vp_u8 **gp, long n,
;                   long h, long pattern, long constant)
; dst: the first byte the span touches in this plane, row 0; s: the bit
; phase (0-7); gp[i]: cell i's glyph, row 0 (rows h apart... one byte a
; row: gp[i][r] is row r); pattern: 0 = the glyph, -1 = its inverse; with
; constant != 0 every bit becomes pattern (both pens or neither).
; vbcc: arguments on the stack, d0/d1/a0/a1 free.

	mc68020				; BFINS (the caller checks for a 68020, AttnFlags)
	section	"CODE",code
	xdef	_vp_asm_plane

_vp_asm_plane:
	movem.l	d2-d7/a2-a5,-(sp)	; 10 registers: arguments from 44(sp)
	move.l	44(sp),a5		; dst
	move.l	48(sp),d2		; bpr
	move.l	52(sp),d3		; s
	move.l	56(sp),a3		; gp
	move.l	60(sp),d4		; n
	move.l	64(sp),d5		; h
	move.l	68(sp),d6		; pattern
	tst.l	72(sp)
	bne.s	.crow
	moveq	#0,d7			; r: the glyph row
.row:	move.l	a5,a1
	move.l	a3,a2
	move.l	d4,d0
	subq.l	#4,d0
	bmi.s	.tail
.quad:	move.l	(a2)+,a4
	move.b	(a4,d7.l),d1
	lsl.l	#8,d1
	move.l	(a2)+,a4
	move.b	(a4,d7.l),d1
	lsl.l	#8,d1
	move.l	(a2)+,a4
	move.b	(a4,d7.l),d1
	lsl.l	#8,d1
	move.l	(a2)+,a4
	move.b	(a4,d7.l),d1
	eor.l	d6,d1
	bfins	d1,(a1){d3:32}
	addq.l	#4,a1
	subq.l	#4,d0
	bpl.s	.quad
.tail:	addq.l	#4,d0
	beq.s	.next
.t1:	move.l	(a2)+,a4
	move.b	(a4,d7.l),d1
	eor.b	d6,d1
	bfins	d1,(a1){d3:8}
	addq.l	#1,a1
	subq.l	#1,d0
	bne.s	.t1
.next:	add.l	d2,a5
	addq.l	#1,d7
	cmp.l	d5,d7
	bne.s	.row
	bra.s	.done
.crow:	move.l	a5,a1			; every bit the pattern
	move.l	d4,d0
	subq.l	#4,d0
	bmi.s	.ctail
.cq:	bfins	d6,(a1){d3:32}
	addq.l	#4,a1
	subq.l	#4,d0
	bpl.s	.cq
.ctail:	addq.l	#4,d0
	beq.s	.cnext
.ct1:	bfins	d6,(a1){d3:8}
	addq.l	#1,a1
	subq.l	#1,d0
	bne.s	.ct1
.cnext:	add.l	d2,a5
	subq.l	#1,d5
	bne.s	.crow
.done:	movem.l	(sp)+,d2-d7/a2-a5
	rts
