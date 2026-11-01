; FLAT-OUTER-EDGE: entry of the graphics.library RectFill patch. RectFill(rp a1, xMin d0, yMin d1, xMax d2, yMax d3).
; Calls flat_rf_c(rp, x0, y0, x1, y1, caller) on the C stack: caller is the return address of the RectFill call.
; d0/d1/a0/a1 are scratch for a library caller, so the C call may clobber them; the registers C preserves it preserves.
	xdef	_flat_rf_entry
	xref	_flat_rf_c
_flat_rf_entry:
	move.l	(a7),-(a7)
	move.l	d3,-(a7)
	move.l	d2,-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	move.l	a1,-(a7)
	jsr	_flat_rf_c
	lea	24(a7),a7
	rts
