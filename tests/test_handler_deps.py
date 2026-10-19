#!/usr/bin/env python3
"""build/amiga/vtcon-handler is rebuilt when a header vtcon_handler.c includes changes.

HANDLER_HDR did not list handler/tabprog.h: after f17424a8 changed the tab's command line, `make amiga` said
"Nothing to be done" and the rig ran the old handler (Help > Shell tutorial ran `tutorial`, not `tutorial next`).
Each header the handler includes is declared newer (make -W) and the handler must be due for a rebuild."""
import os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
ENV = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
src = open(os.path.join(ROOT, "handler/vtcon_handler.c")).read()
headers = sorted({os.path.normpath(os.path.join("handler", h)) for h in re.findall(r'#include "([^"]+\.h)"', src)})
bad = []
for h in headers:
    if not os.path.exists(os.path.join(ROOT, h)):
        continue
    r = subprocess.run(["make", "-n", "-C", ROOT, "-W", h, "build/amiga/vtcon-handler"], capture_output=True, text=True, env=ENV)
    if "Nothing to be done" in r.stdout + r.stderr or "is up to date" in r.stdout + r.stderr:
        bad.append(h)
assert headers, "no includes found"
assert not bad, "a change to these does not rebuild the handler: %s (add them to HANDLER_HDR)" % " ".join(bad)
print("handler deps: %d headers rebuild the handler" % len(headers))
