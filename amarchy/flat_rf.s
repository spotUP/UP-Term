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
	xdef	_flat_rf_tpl_c1
	xdef	_flat_rf_tpl_c2
	xdef	_flat_rf_tpl_end
	xref	_flat_rf_c
_flat_rf_tpl:
_flat_rf_tpl_c1:
	addq.l	#1,($12345678).l
	move.l	(a7),-(a7)
	move.l	d3,-(a7)
	move.l	d2,-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_rf_c).l
	lea	24(a7),a7
_flat_rf_tpl_c2:
	subq.l	#1,($12345678).l
	rts
_flat_rf_tpl_end:
