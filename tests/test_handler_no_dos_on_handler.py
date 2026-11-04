#!/usr/bin/env python3
"""No DOS call runs on the handler process from the packet loop (CON-87000004).

A Lock/Open/Examine on the handler process does DoPkt on pr_MsgPort, the handler's own packet port, and
takes a client packet off it: the boot failed with "Program failed (error #87000004)" (AN_AsyncPkt) once
fonts_ttf_exists (a Lock of FONTS:SymbolsNerdFontMono.ttf) ran in apply_profile for every window open.
The file reads belong to the worker processes. Source guard: fonts_ttf_exists is called only from
config_worker and watch_worker, and apply_profile / parse_spec / profile_switch call no DOS function."""
import os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
src = open(os.path.join(ROOT, "handler/vtcon_handler.c")).read()


def body(name):
    m = re.search(r"\nstatic [^\n;]*\b%s\([^\n;]*\)\n\{" % name, src)
    assert m, name + " not found"
    return src[m.end():src.find("\n}\n", m.end())]


DOS = r"\b(fonts_ttf_exists|Lock|Open|Examine|ExNext|Read|GetVar|Delay|DeviceProc|DoPkt|SendPkt)\s*\("
for fn in ("apply_profile", "parse_spec", "profile_switch"):
    hit = re.search(DOS, body(fn))
    assert not hit, "%s calls %s on the handler process" % (fn, hit.group(1))

# the whole file: fonts_ttf_exists is called from the two workers only (its definition aside)
callers = set()
for m in re.finditer(r"\bfonts_ttf_exists\(", src):
    if src[max(0, m.start() - 12):m.start()].endswith("static int "):
        continue
    start = src.rfind("\nstatic ", 0, m.start())
    callers.add(re.match(r"\nstatic [^\n(]*?\b(\w+)\(", src[start:]).group(1))
assert callers == {"config_worker", "watch_worker"}, "fonts_ttf_exists called from %s" % sorted(callers)
assert "nerd_ttf" in body("apply_profile"), "apply_profile does not use the worker's nerd_ttf"
print("ok")
