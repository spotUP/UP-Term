; FLAT-UNLOAD-INFLIGHT: the gate template of the class-dispatcher entries (sysiclass, frameiclass, dopusiclass). flat.c copies
; [_flat_dg_tpl, _flat_dg_tpl_end) into a never-freed AllocMem block, per dispatcher, with the handler address patched in.
; The gate adds ONE return address (4 bytes) to the caller's stack and touches no register: the handler gets the caller's
; registers (a0 hook, a2 object, a1 message for the register convention) unchanged. The stack-convention handlers
; (c2_disp_stk, c8_disp_stk) take one leading dummy parameter that absorbs the caller's return address, so the arguments
; need not be pushed again: stack inside the handler is [gate ret][caller ret][hook][obj][msg].
	xdef	_flat_dg_tpl
	xdef	_flat_dg_tpl_end
_flat_dg_tpl:
	addq.l	#1,($11111111).l
	tst.l	($22222222).l
	bne.s	_flat_dg_tpl_pass
	jsr	($44444444).l
	subq.l	#1,($11111111).l
	rts
_flat_dg_tpl_pass:
	subq.l	#1,($11111111).l
	move.l	($33333333).l,-(a7)
	rts
_flat_dg_tpl_end:
;
; FLAT-UNLOAD-INFLIGHT closed-flag gate. Order is fixed: count in FIRST, then test the closed flag, so a task that has fetched
; the vector but not yet run addq is the only one unload cannot see, and it is still counted before it decides. Closed:
; count out and run the ORIGINAL function (pass-through; nothing of ours runs, no register and no stack byte is used: the
; original's address is pushed and rts jumps to it, leaving the caller's return address on top as at the real entry).
; Four operands are magic words that flat.c gate_make replaces in the copy: $11111111 the counter, $22222222 the closed
; flag, $33333333 the original function (set by gate_open after the swap), $44444444 the handler (flat_dg.s only).
