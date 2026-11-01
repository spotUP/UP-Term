#!/usr/bin/env python3
"""The Directory Opus 5 filetype file UP-Term ships (plan 2026-10-25-amarchy-dopus-integration.md, D11): binary IFF
`FORM OPUS`, one TYPE chunk with its FUNC chunks, written in the layout of upstream dopus5 v5.104
archive/tools/filetype_tool.py `encode` (read by L_ReadFiletypes in source/Library/config_open.c). Check the output
with that tool: `python3 -I filetype_tool.py check FILE` and `lint FILE`.

usage: gen_dopus_filetype.py <out file>"""
import struct
import sys

MATCHNAME = 2                      # FTOP_MATCHNAME: index in filetype_tool.OPS
EV_USER1, EV_DOUBLE_CLICK = 0, 2   # index in filetype_tool.EVENTS (the FUNC header's func_type)
INST_AMIGADOS = 1                  # INST_* in dopus5.h
FUNCF_ASYNC = 1 << 4               # FUNCF_* in dopus5.h: do not wait for the command

TEXT_PATTERN = b"#?.(c|h|txt|md|rexx)"


def chunk(chunk_id, body):
    return chunk_id + struct.pack(">I", len(body)) + body + (b"\0" if len(body) & 1 else b"")


def type_header(name, ident, priority):
    # CFG_TYPE: name[32], id[8], flags, priority, pad, count (46 bytes)
    return (name.ljust(32, b"\0") + ident.ljust(8, b"\0") + struct.pack(">HbBH", 0, priority, 0, 16))


def func_header(event):
    # CFG_FUNC: flags, flags2, pad, code 0xffff, qual, func_type, qual_mask 0xffff, qual_same (22 bytes)
    return struct.pack(">IIIHHHHH", FUNCF_ASYNC, 0, 0, 0xFFFF, 0, event, 0xFFFF, 0)


def func(event, command):
    return chunk(b"FUNC", func_header(event) + command + b"\0" + struct.pack(">h", INST_AMIGADOS))


def generate():
    recognition = bytes([MATCHNAME]) + TEXT_PATTERN
    body = type_header(b"UP-Term", b"UPTERM", -10) + recognition + b"\0" + b"\0" + b"\0" * 16
    out = chunk(b"TYPE", body)
    out += func(EV_DOUBLE_CLICK, b'Run >NIL: Execute UP-Term:bin/uterm-edit "%s"')
    out += func(EV_USER1, b'Run >NIL: Execute UP-Term:bin/uterm-view "%s"')
    return b"FORM" + struct.pack(">I", len(out) + 4) + b"OPUS" + out


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], "wb") as f:
        f.write(generate())
