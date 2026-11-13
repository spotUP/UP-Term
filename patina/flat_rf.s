; FLAT-OUTER-EDGE: entry of the graphics.library RectFill patch. RectFill(rp a1, xMin d0, yMin d1, xMax d2, yMax d3).
; Calls flat_rf_c(rp, x0, y0, x1, y1, caller) on the C stack: caller is the return address of the RectFill call.
; d0/d1/a0/a1 are scratch for a library caller, so the C call may clobber them; the registers C preserves it preserves.
;
; FLAT-UNLOAD-INFLIGHT: this is a TEMPLATE, not the live entry. flat.c copies [_flat_rf_tpl, _flat_rf_tpl_end) into a small
; AllocMem block that is never freed and points the vector at the copy, because the count-out and the final rts run after
; the C handler returns, and by then the unload may have freed the segment this text lives in. The copy is position
; independent except the two counter operands (patched to the block's own counter, at _flat_rf_tpl_c1 + 2 and
; _flat_rf_tpl_c2 + 2); the jsr target is relocated by the loader before the copy is made. Same pushes, same stack as before.
	xdef	_flat_rf_tpl
	xdef	_flat_rf_tpl_end
	xref	_flat_rf_c
_flat_rf_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_rf_tpl_pass
	move.l	(a7),-(a7)
	move.l	d3,-(a7)
	move.l	d2,-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_rf_c).l
	lea	24(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_rf_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_rf_tpl_end:
;
; FLAT-UNLOAD-INFLIGHT closed-flag gate. Order is fixed: count in FIRST, then test the closed flag, so a task that has fetched
; the vector but not yet run addq is the only one unload cannot see, and it is still counted before it decides. Closed:
; count out and run the ORIGINAL function (pass-through; nothing of ours runs, no register and no stack byte is used: the
; original's address is pushed and rts jumps to it, leaving the caller's return address on top as at the real entry).
; Four operands are magic words that flat.c gate_make replaces in the copy: $11111111 the counter, $22222222 the closed
; flag, $33333333 the original function (set by gate_open after the swap), $44444444 the handler (flat_dg.s only).
