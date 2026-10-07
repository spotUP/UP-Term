; The first code of vtcon-handler. The handler is built without a C startup:
; AmigaDOS starts it at the first byte of its first code hunk. This object is
; linked first, so that byte jumps to handler_entry wherever the compiler put
; it (a header with a function body included above handler_entry made that
; function the entry, and every CON: window hung, 79872bd).
	section	"CODE",code
	xref	_handler_entry
	jmp	_handler_entry
