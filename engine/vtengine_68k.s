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
	xdef	_vt_asm_put_ch
	xdef	_vt_asm_fill
	xdef	_vt_asm_ch_blank
	xdef	_vt_asm_csi
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

; long vt_asm_csi(const vt_u8 *p, long n, long *params, vt_u8 *sub)
;
; csi_fast's parameter scan (ASM1): the bytes from p (after "ESC [") are
; digits, ';' and ':' up to the first other byte, the final. Their values
; go to params / sub as the parser's own CSI states put them (feed): a
; value saturates at 65535 (VT_PARAM_MAX: from 6553 on the next digit
; makes it 65535), ':' marks the parameter it starts in sub, the 17th and
; later parameters run on into the 16th (VT_MAX_PARAMS). No parameter:
; params[0] = sub[0] = 0. Returns np << 16 | the final's offset from p, or
; -1 when the n bytes (at most 32767 are looked at) end first. Eight
; instructions a digit (C: twenty and more).
_vt_asm_csi:
	movem.l	d2-d7/a2-a3,-(sp)
	move.l	36(sp),a0		; p
	move.l	40(sp),d3		; n
	move.l	44(sp),a2		; params
	move.l	48(sp),a3		; sub
	move.l	a0,a1			; p, for the offset
	moveq	#0,d1			; v: the parameter being read
	moveq	#0,d2			; np
	moveq	#0,d0
	move.w	#6553,d5		; VT_PARAM_MAX / 10
	moveq	#'0',d6
	moveq	#9,d7
	cmp.l	#$7fff,d3
	bls.s	.cn
	move.w	#$7fff,d3
.cn:	subq.w	#1,d3			; the bytes left after the one read
	bmi	.cout
	move.b	(a0)+,d0
	sub.b	d6,d0
.ctop:	cmp.b	d7,d0
	bhi.s	.cnd			; not a digit
	tst.w	d2
	bne.s	.cdig
	moveq	#1,d2			; the first parameter starts
	clr.b	(a3)
.cdig:	cmp.w	d5,d1
	bcc.s	.csat
	mulu.w	#10,d1
	add.w	d0,d1
.cdn:	subq.w	#1,d3
	bmi.s	.cout
	move.b	(a0)+,d0
	sub.b	d6,d0
	cmp.b	d7,d0
	bls.s	.cdig
.cnd:	cmp.b	#';'-'0',d0
	beq.s	.csep
	cmp.b	#':'-'0',d0
	bne.s	.cfin
.csep:	tst.w	d2
	bne.s	.cs1
	moveq	#1,d2
	clr.b	(a3)
.cs1:	move.w	d2,d4
	lsl.w	#2,d4
	move.l	d1,-4(a2,d4.w)		; params[np - 1] = v
	cmp.w	#16,d2
	bcc.s	.csn			; all 16: the digits go on into the last
	moveq	#0,d4
	cmp.b	#':'-'0',d0
	bne.s	.cs2
	moveq	#1,d4
.cs2:	move.b	d4,(a3,d2.w)		; sub[np] = (c == ':')
	addq.w	#1,d2
	moveq	#0,d1
.csn:	subq.w	#1,d3
	bmi.s	.cout
	move.b	(a0)+,d0
	sub.b	d6,d0
	bra.s	.ctop
.csat:	move.w	#-1,d1			; 65535
	bra.s	.cdn
.cfin:	tst.w	d2
	beq.s	.cf0
	move.w	d2,d4
	lsl.w	#2,d4
	move.l	d1,-4(a2,d4.w)		; the last parameter
	bra.s	.cf1
.cf0:	clr.l	(a2)
	clr.b	(a3)
.cf1:	move.l	a0,d0
	sub.l	a1,d0
	subq.l	#1,d0			; the final's offset
	swap	d2
	clr.w	d2
	or.l	d2,d0
	movem.l	(sp)+,d2-d7/a2-a3
	rts
.cout:	moveq	#-1,d0
	movem.l	(sp)+,d2-d7/a2-a3
	rts

; void vt_asm_ch_blank(vt_cell *c, long n)
;
; line_clear of a chonly line: the characters of n cells back to a space,
; the rest of each cell is the default blank's already. Four cells a turn
; of the loop, six instructions (a whole cell is five).
_vt_asm_ch_blank:
	move.l	8(sp),d0		; n
	ble.s	.bnone
	move.l	4(sp),a0
	addq.l	#8,a0			; at ch
	moveq	#3,d1
	and.w	d0,d1			; the odd cells first
	lsr.l	#2,d0			; then fours
	bra.s	.b1e
.b1:	move.w	#$20,(a0)
	lea	16(a0),a0
.b1e:	dbra	d1,.b1
	bra.s	.b4e
.b4:	move.w	#$20,(a0)
	move.w	#$20,16(a0)
	move.w	#$20,32(a0)
	move.w	#$20,48(a0)
	lea	64(a0),a0
.b4e:	dbra	d0,.b4
.bnone:	rts

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

; long vt_asm_put_ch(vt_cell *c, const vt_u8 *b, long n)
;
; put_ascii_run's plain case (ledger S1): the cells are untouched default
; blanks (past the line's `used`) and the text is in the default colours,
; so of each cell only the character changes -- and of the character only
; its low byte (a default blank's ch is $0020). Printable ASCII is stored
; until n or the first other byte; the cells written are returned.
; Four bytes a long (creep's printable scan; ASM1): a long is all
; printable when no byte is under $20 and none is $7f or over:
;   ((x + $01010101) | (x - $20202020)) & $80808080 == 0
; (the lowest bad byte gets no carry or borrow from the bytes below it,
; so a bad long is always seen; the byte loop then finds the byte).
; 17 instructions four characters, against 36. Longs are read from an
; even address (a 68000 traps on an odd one): an odd start takes a byte.
_vt_asm_put_ch:
	move.l	12(sp),d0		; n
	ble	.pnone
	movem.l	d2-d7,-(sp)
	move.l	4+24(sp),a0		; c
	move.l	8+24(sp),a1		; b
	lea	9(a0),a0		; at ch's low byte
	move.l	a1,d1
	btst	#0,d1
	beq.s	.peven
	move.b	(a1),d1			; an odd start: one byte first
	cmp.b	#$20,d1
	bcs	.pdone			; a control
	cmp.b	#$7f,d1
	bcc	.pdone			; DEL or an 8-bit byte
	addq.l	#1,a1
	move.b	d1,(a0)
	lea	16(a0),a0
	subq.l	#1,d0
	beq	.pdone
.peven:	moveq	#3,d7
	and.l	d0,d7			; the bytes after the whole longs
	move.l	d0,d6
	lsr.l	#2,d6			; whole longs
	beq.s	.ptail
	move.l	#$01010101,d3
	move.l	#$20202020,d4
	move.l	#$80808080,d5
	subq.w	#1,d6
.plong:	move.l	(a1),d2			; b0 b1 b2 b3
	move.l	d2,d1
	add.l	d3,d1
	move.l	d2,d0
	sub.l	d4,d0
	or.l	d0,d1
	and.l	d5,d1
	bne.s	.pbad			; a byte in it ends the run
	addq.l	#4,a1
	move.b	d2,48(a0)		; b3
	lsr.w	#8,d2
	move.b	d2,32(a0)		; b2
	swap	d2
	move.b	d2,16(a0)		; b1
	lsr.w	#8,d2
	move.b	d2,(a0)			; b0
	lea	64(a0),a0
	dbra	d6,.plong
.ptail:	move.l	d7,d0
	bne.s	.pbyte
	bra.s	.pdone
.pbad:	moveq	#4,d0			; the byte that ends it is in this long
.pbyte:	move.b	(a1),d1
	cmp.b	#$20,d1
	bcs.s	.pdone
	cmp.b	#$7f,d1
	bcc.s	.pdone
	addq.l	#1,a1
	move.b	d1,(a0)
	lea	16(a0),a0
	subq.l	#1,d0
	bne.s	.pbyte
.pdone:	move.l	a1,d0
	sub.l	8+24(sp),d0		; the bytes taken: the cells written
	movem.l	(sp)+,d2-d7
	rts
.pnone:	moveq	#0,d0
	rts
