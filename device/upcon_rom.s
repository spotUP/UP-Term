; upcon_rom.s -- first in up-console.device (plan 2026-09-30-console-device.md,
; D1.2, DD11): a plain run of the file returns at once; the RomTag that
; UPConsole DEVICE ON finds and gives to InitResident; and the console
; vectors that go to the ROM device unchanged.
;
; The ROM base is at UPC_ROMBASE_OFFSET (40) in our base (device/upcon.h).
; A stub keeps every register as the caller gave it, except a6 for the
; call; CDInputHandler also gets the ROM base as its device argument (a1).

	section	code,code

	xref	_upc_init
	xdef	_upc_romtag
	xdef	_upc_fwd_cdinputhandler
	xdef	_upc_fwd_rawkeyconvert
	xdef	_upc_fwd_54
	xdef	_upc_fwd_60
	xdef	_upc_fwd_66
	xdef	_upc_fwd_72
	xdef	_upc_rom_open
	xdef	_upc_rom_beginio
	xdef	_upc_name
	xdef	_upc_idstring
	xdef	_upc_con_name

ROMBASE	equ	40

start:
	moveq	#-1,d0
	rts

_upc_romtag:
	dc.w	$4AFC			; RTC_MATCHWORD
	dc.l	_upc_romtag
	dc.l	endskip
	dc.b	0			; rt_Flags: not RTF_AUTOINIT, _upc_init builds the base
	dc.b	40			; rt_Version (Init copies the ROM's into lib_Version)
	dc.b	3			; NT_DEVICE
	dc.b	0			; rt_Pri
	dc.l	_upc_name
	dc.l	_upc_idstring
	dc.l	_upc_init
endskip:

; CDInputHandler(events a0, consoleDevice a1): the ROM's, on its own base
_upc_fwd_cdinputhandler:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	move.l	a6,a1
	jsr	-42(a6)
	move.l	(sp)+,a6
	rts

; RawKeyConvert(events a0, buffer a1, length d1, keyMap a2)
_upc_fwd_rawkeyconvert:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	jsr	-48(a6)
	move.l	(sp)+,a6
	rts

; the private vectors (snip interface): unknown arguments, passed as they are
_upc_fwd_54:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	jsr	-54(a6)
	move.l	(sp)+,a6
	rts

_upc_fwd_60:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	jsr	-60(a6)
	move.l	(sp)+,a6
	rts

_upc_fwd_66:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	jsr	-66(a6)
	move.l	(sp)+,a6
	rts

_upc_fwd_72:
	move.l	a6,-(sp)
	move.l	ROMBASE(a6),a6
	jsr	-72(a6)
	move.l	(sp)+,a6
	rts

; upc_rom_open(io a1, unit d0, flags d1, rom a0): the ROM's Open, as exec's
; OpenDevice calls it (DD16: an open forwarded to the ROM's unit)
_upc_rom_open:
	move.l	a6,-(sp)
	move.l	a0,a6
	jsr	-6(a6)
	move.l	(sp)+,a6
	rts

; upc_rom_beginio(io a1, rom a0): the ROM's BeginIO (the default keymap)
_upc_rom_beginio:
	move.l	a6,-(sp)
	move.l	a0,a6
	jsr	-30(a6)
	move.l	(sp)+,a6
	rts

_upc_name:
	dc.b	"UP-Term console.device",0
_upc_con_name:
	dc.b	"console.device",0
_upc_idstring:
	dc.b	"console.device (UP-Term)",13,10,0
	even
