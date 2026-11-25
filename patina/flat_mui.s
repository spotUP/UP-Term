; MUI-FLAT: entry of the two muimaster.library patches, MUI_NewObjectA (LVO -30: class a0, tags a1) and MUI_MakeObjectA
; (LVO -120: type d0, params a0). Calls the handler (C, patched in by gate_make) with (d0, a0, a1) on the C stack; the
; handler calls the original itself (muimaster base in a6) and returns the object in d0. d0/d1/a0/a1 are scratch for a
; library caller; C preserves the rest.
;
; FLAT-UNLOAD-INFLIGHT: a TEMPLATE copied by flat.c gate_make into a never-freed block, one copy per vector; see flat_cg.s.
	xdef	_flat_mui_tpl
	xdef	_flat_mui_tpl_end
_flat_mui_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_mui_tpl_pass
	move.l	a1,-(a7)
	move.l	a0,-(a7)
	move.l	d0,-(a7)
	jsr	($44444444).l
	lea	12(a7),a7
	subq.l	#1,($11111111).l
	rts
_flat_mui_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_mui_tpl_end:
