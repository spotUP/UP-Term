; S63-KS31-STACK: run a C function on a private stack. LONG flat_on_stack(LONG (*fn)(void *), void *arg, struct StackSwapStruct *sss)
; StackSwap(sss), fn(arg) on the new stack, StackSwap(sss) back, return fn's result. In asm because C cannot touch its own
; frame across a StackSwap: fn, arg, the result and sss live in registers that survive the call (d2/d3/a2/a3 are saved).
	xdef	_flat_on_stack
_flat_on_stack:
	movem.l	d2/d3/a2/a3/a6,-(a7)
	move.l	24(a7),a2		; fn
	move.l	28(a7),a3		; arg
	move.l	32(a7),d2		; sss
	move.l	4.w,a6
	move.l	d2,a0
	jsr	-732(a6)		; _LVOStackSwap: now on the private stack
	move.l	a3,-(a7)
	jsr	(a2)
	addq.l	#4,a7
	move.l	d0,d3
	move.l	4.w,a6
	move.l	d2,a0
	jsr	-732(a6)		; swap back to the caller's stack
	move.l	d3,d0
	movem.l	(a7)+,d2/d3/a2/a3/a6
	rts
