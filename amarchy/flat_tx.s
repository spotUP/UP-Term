; FLAT-BUTTON-FILL: entry of the graphics.library Text patch. Text(rp a1, string a0, count d0).
; Calls flat_tx_c(rp, string, count) on the C stack. d0/d1/a0/a1 are scratch for a library caller; C preserves the rest.
	xdef	_flat_tx_entry
	xref	_flat_tx_c
_flat_tx_entry:
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	move.l	a1,-(a7)
	jsr	_flat_tx_c
	lea	12(a7),a7
	rts
