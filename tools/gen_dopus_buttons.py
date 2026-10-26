#!/usr/bin/env python3
"""The Directory Opus 5 button bank UP-Term ships (plan 2026-10-25-amarchy-dopus-integration.md, D12): binary IFF
`FORM EPUS` (the id L_SaveButtonBank writes, source/Library/config_save.c:205; ID_EPUS dopus5.h:140), laid out as
upstream dopus5 v5.104 writes a bank: one BTNW chunk (CFG_BTNW, 132 bytes, dopus5.h:215-226), then per button a BUTN
chunk (CFG_BUTN 14 bytes dopus5.h:250-259, then per function a text and a label string; the label "\\1" means "same as
the text", config_save.c:440-495) followed by one FUNC chunk per function (CFG_FUNC 22 bytes dopus5.h:166-178, the
command string, a 16-bit instruction type, config_save.c:545-601). Read by L_OpenButtonBank, config_open.c:280-440.
The layout was checked against the stock DOpus5/Buttons/Hotkeys and Toolbar files of DOpus5_104_os3.lha (BTNW size 0x84).

usage: gen_dopus_buttons.py <out file>"""
import struct
import sys

INST_COMMAND, INST_AMIGADOS = 0, 1       # INST_* in dopus5.h (enum order)
FUNCF_RUN_ASYNC = 1 << 4                 # FUNCF_* in dopus5.h
FUNCF_CD_SOURCE = 1 << 5
FUNCF2_VALID_IX = 1 << 2                 # the stock Toolbar sets it with qual mask 0xffff
BUTNF_NEW_FORMAT = 1 << 4
BTNWF_HORIZ = 1 << 6
FTYPE_LEFT_BUTTON = 0
FPEN, BPEN = 0, 0                        # default pens

WINDOW = b"XCON:0/20/640/300/UP-Term/CLOSE/FRONT"
# (label, command, flags): {f} is the selected file, DOpus's own substitution (stock Dopus4Samples: `Ed {f}`)
BUTTONS = [
    (b"UP-Term here", b'Run >NIL: NewShell "' + WINDOW + b'"', FUNCF_RUN_ASYNC | FUNCF_CD_SOURCE),
    (b"Edit", b"Run >NIL: Execute UP-Term:bin/uterm-edit {f}", FUNCF_RUN_ASYNC | FUNCF_CD_SOURCE),
    (b"Page", b"Run >NIL: Execute UP-Term:bin/uterm-view {f}", FUNCF_RUN_ASYNC | FUNCF_CD_SOURCE),
    (b"Shell", b'Run >NIL: NewShell "' + WINDOW + b'"', FUNCF_RUN_ASYNC),
]


def chunk(chunk_id, body):
    return chunk_id + struct.pack(">I", len(body)) + body + (b"\0" if len(body) & 1 else b"")


def btnw(name, columns, rows):
    # CFG_BTNW: name[32], IBox 4 words, font_name[80], pad[3], font_size, columns, rows, flags (132 bytes)
    return (name.ljust(32, b"\0") + struct.pack(">hhhh", 0, 0, 0, 0) + b"\0" * 80 + b"\0\0\0" + b"\0"
            + struct.pack(">HHI", columns, rows, BTNWF_HORIZ))


def button(label, command, flags):
    head = struct.pack(">BBIhhI", FPEN, BPEN, BUTNF_NEW_FORMAT, 1, 0, 0)       # CFG_BUTN, 14 bytes
    func_head = struct.pack(">IIIHHHHH", flags, FUNCF2_VALID_IX, 0, 0xFFFF, 0, FTYPE_LEFT_BUTTON, 0xFFFF, 0)
    return (chunk(b"BUTN", head + label + b"\0" + b"\1\0")
            + chunk(b"FUNC", func_head + command + b"\0" + struct.pack(">h", INST_AMIGADOS)))


def generate():
    out = chunk(b"BTNW", btnw(b"UP-Term", len(BUTTONS), 1))
    for label, command, flags in BUTTONS:
        out += button(label, command, flags)
    return b"FORM" + struct.pack(">I", len(out) + 4) + b"EPUS" + out


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], "wb") as f:
        f.write(generate())
