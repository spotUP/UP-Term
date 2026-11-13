; S74-TIME-ROW: entry of the gadtools.library CreateGadgetA patch (LVO -30).
; CreateGadgetA(kind d0, previous a0, newgadget a1, taglist a2). Calls flat_cg_c(kind, previous, newgadget, taglist) on the
; C stack; flat_cg_c calls the original itself (gadtools base in a6) and returns the gadget in d0, which this stub leaves
; untouched on the way out. d0/d1/a0/a1 are scratch for a library caller; C preserves the rest (a2 included).
;
; FLAT-UNLOAD-INFLIGHT: a TEMPLATE, copied by flat.c gate_make into a never-freed AllocMem block with the counter, closed
; flag and original operands patched; see flat_rf.s for why the live entry cannot sit in the freeable segment and for the
; closed pass-through (count out, push the original, rts into it).
	xdef	_flat_cg_tpl
	xdef	_flat_cg_tpl_end
	xref	_flat_cg_c
_flat_cg_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_cg_tpl_pass
	move.l	a2,-(a7)
	move.l	a1,-(a7)
	move.l	a0,-(a7)
	move.l	d0,-(a7)
	jsr	(_flat_cg_c).l
	lea	16(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_cg_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_cg_tpl_end:
