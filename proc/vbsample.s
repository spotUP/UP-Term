; vbsample -- top's VBlank interrupt server (proc/proc_core.h): each vertical blank (50 Hz on PAL) the task the
; CPU was running goes into the ring, bit 0 set when that task is not running (exec idles in Wait with ThisTask
; left on the last task). Called by exec with a1 = is_Data (a pc_ring: ULONG count, ULONG slot[1024]);
; d0/d1/a0/a1 are scratch. Returns with Z set (d0 = 0): the next VERTB server runs too.
	section	code,code
	xdef	_vb_sample
_vb_sample:
	move.l	4.w,a0			; SysBase (AbsExecBase: a6 is not documented for every server chain)
	move.l	276(a0),a0		; SysBase->ThisTask
	move.l	a0,d1
	cmp.b	#2,15(a0)		; tc_State == TS_RUN?
	beq.s	.run
	bset	#0,d1			; idle
.run:
	move.l	(a1),d0			; count
	addq.l	#1,(a1)+		; count++; a1 -> slot[0]
	and.l	#1023,d0
	lsl.l	#2,d0
	move.l	d1,0(a1,d0.l)
	moveq	#0,d0
	rts
