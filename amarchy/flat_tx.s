; FLAT-BUTTON-FILL: entry of the graphics.library Text patch. Text(rp a1, string a0, count d0).
; Calls flat_tx_c(rp, string, count) on the C stack. d0/d1/a0/a1 are scratch for a library caller; C preserves the rest.
;
; FLAT-UNLOAD-INFLIGHT: a TEMPLATE, copied by flat.c into a never-freed AllocMem block with the counter operands patched
; (at _flat_tx_tpl_c1 + 2 and _flat_tx_tpl_c2 + 2); see flat_rf.s for why the live entry cannot sit in the freeable segment.
	xdef	_flat_tx_tpl
	xdef	_flat_tx_tpl_c1
	xdef	_flat_tx_tpl_c2
	xdef	_flat_tx_tpl_end
	xref	_flat_tx_c
_flat_tx_tpl:
_flat_tx_tpl_c1:
	addq.l	#1,($12345678).l
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_tx_c).l
	lea	12(a7),a7
_flat_tx_tpl_c2:
	subq.l	#1,($12345678).l
	rts
_flat_tx_tpl_end:
