; uprom_tag.s -- UP-Term in a Kickstart ROM (ledger R1, research
; 2026-10-04_upterm-in-rom.md). The module's RomTag, the three functions
; InternalLoadSeg calls to load a hunk file that lies in ROM, and the two
; hunk files themselves (incbin of the disk builds, unchanged).
;
; RTF_AFTERDOS: dos.library runs the init (InitCode(RTF_AFTERDOS)) once DOS
; is up, as it does for ramlib (40.63's only AFTERDOS module, measured with
; tools/mkrom.py). The init (device/uprom.c) loads both files into RAM with
; InternalLoadSeg and switches as UPConsole DEVICE ON + CON ON do.
;
; Everything here is read-only: the module runs from ROM. One hunk (vlink
; -sc), no DATA, no BSS -- tools/mkrom.py refuses a module with either.

	section	code,code

	xref	_uprom_init
	xdef	_uprom_read
	xdef	_uprom_alloc
	xdef	_uprom_free
	xdef	_uprom_device
	xdef	_uprom_device_end
	xdef	_uprom_handler
	xdef	_uprom_handler_end

romtag:
	dc.w	$4AFC			; RTC_MATCHWORD
	dc.l	romtag
	dc.l	endskip
	dc.b	4			; RTF_AFTERDOS
	dc.b	1			; rt_Version
	dc.b	0			; NT_UNKNOWN
	dc.b	-101			; after ramlib (-100)
	dc.l	name
	dc.l	idstring
	dc.l	_uprom_init		; a6 = exec
endskip:

; InternalLoadSeg's FuncTable (dos.library V36; the ABI as AROS documents it
; for binary compatibility, rom/dos/internalloadseg.c):
;   [0] actual = ReadFunc(readhandle d1, buffer a0, length d0), DOSBase a6
;   [1] memory = AllocFunc(size d0, flags d1), ExecBase a6
;   [2] FreeFunc(memory a1, size d0), ExecBase a6
; The read handle is a uprom_cursor (device/uprom.c): { pos, left }.
_uprom_read:
	movem.l	d2/a2,-(sp)
	move.l	d1,a2
	move.l	4(a2),d2		; bytes left
	cmp.l	d2,d0
	bls.s	.n
	move.l	d2,d0
.n	sub.l	d0,4(a2)
	move.l	(a2),a1
	move.l	d0,d1
	bra.s	.t
.c	move.b	(a1)+,(a0)+
.t	subq.l	#1,d1
	bpl.s	.c
	move.l	a1,(a2)
	movem.l	(sp)+,d2/a2
	rts

; exec's own AllocMem / FreeMem (exec from 4.w whatever a6 is): UnLoadSeg
; and RemDevice free these hunks as they free LoadSeg's
_uprom_alloc:
	move.l	a6,-(sp)
	move.l	4.w,a6
	jsr	-198(a6)		; AllocMem(d0 size, d1 flags)
	move.l	(sp)+,a6
	rts

_uprom_free:
	move.l	a6,-(sp)
	move.l	4.w,a6
	jsr	-210(a6)		; FreeMem(a1, d0)
	move.l	(sp)+,a6
	rts

name:
	dc.b	"UP-Term ROM",0
idstring:
	dc.b	"UP-Term ROM 1.0 (4.10.2026)",13,10,0

	cnop	0,4
_uprom_device:
	incbin	"up-console.device"
_uprom_device_end:
	cnop	0,4
_uprom_handler:
	incbin	"vtcon-handler"
_uprom_handler_end:
	cnop	0,4
