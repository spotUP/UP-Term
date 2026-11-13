; FLAT-BUTTON-FILL: entry of the graphics.library Text patch. Text(rp a1, string a0, count d0).
; Calls flat_tx_c(rp, string, count) on the C stack. d0/d1/a0/a1 are scratch for a library caller; C preserves the rest.
;
; FLAT-UNLOAD-INFLIGHT: a TEMPLATE, copied by flat.c into a never-freed AllocMem block with the counter operands patched
; (at _flat_tx_tpl_c1 + 2 and _flat_tx_tpl_c2 + 2); see flat_rf.s for why the live entry cannot sit in the freeable segment.
	xdef	_flat_tx_tpl
	xdef	_flat_tx_tpl_end
	xref	_flat_tx_c
_flat_tx_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_tx_tpl_pass
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	move.l	a1,-(a7)
	jsr	(_flat_tx_c).l
	lea	12(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_tx_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_tx_tpl_end:
;
; FLAT-UNLOAD-INFLIGHT closed-flag gate. Order is fixed: count in FIRST, then test the closed flag, so a task that has fetched
; the vector but not yet run addq is the only one unload cannot see, and it is still counted before it decides. Closed:
; count out and run the ORIGINAL function (pass-through; nothing of ours runs, no register and no stack byte is used: the
; original's address is pushed and rts jumps to it, leaving the caller's return address on top as at the real entry).
; Four operands are magic words that flat.c gate_make replaces in the copy: $11111111 the counter, $22222222 the closed
; flag, $33333333 the original function (set by gate_open after the swap), $44444444 the handler (flat_dg.s only).
