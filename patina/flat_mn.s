; FLAT-MENU-PANEL: entries of the graphics.library SetRast and Draw patches. Intuition 3.1 renders a pull-down panel into a
; layerless off-screen bitmap: SetRast(rp a1, pen d0) fills the panel ground, Draw(rp a1, x d0, y d1) draws the outline.
; Calls flat_sr_c(rp, pen, caller) / flat_dr_c(rp, x, y, caller) on the C stack: caller is the return address of the call.
; Same FLAT-UNLOAD-INFLIGHT template and magic operands as flat_rf.s ($11111111 counter, $22222222 closed flag, $33333333 the
; original function): flat.c gate_make copies each template into a never-freed block.
	xdef	_flat_sr_tpl
	xdef	_flat_sr_tpl_end
	xdef	_flat_dr_tpl
	xdef	_flat_dr_tpl_end
	xdef	_flat_bb_tpl
	xdef	_flat_bb_tpl_end
	xref	_flat_sr_c
	xref	_flat_dr_c
	xref	_flat_bb_c
_flat_sr_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_sr_tpl_pass
	move.l	(a7),-(a7)
	move.l	d0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_sr_c).l
	lea	12(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_sr_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_sr_tpl_end:
_flat_dr_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_dr_tpl_pass
	move.l	(a7),-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_dr_c).l
	lea	16(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_dr_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_dr_tpl_end:
; BltBitMapRastPort(src a0, xs d0, ys d1, rp a1, xd d2, yd d3, w d4, h d5, minterm d6): flat_bb_c(src, xs, ys, rp, xd, yd, w, h,
; minterm, caller) blits and gives an item image's pen-0 ground the menu pen.
_flat_bb_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_bb_tpl_pass
	move.l	(a7),-(a7)
	move.l	d6,-(a7)
	move.l	d5,-(a7)
	move.l	d4,-(a7)
	move.l	d3,-(a7)
	move.l	d2,-(a7)
	move.l	a1,-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	(_flat_bb_c).l
	lea	40(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_bb_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_bb_tpl_end:
