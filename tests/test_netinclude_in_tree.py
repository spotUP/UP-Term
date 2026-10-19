#!/usr/bin/env python3
"""C:uptelnet and C:Claude compile against the Roadshow headers in this tree (make_hygiene).

C:uptelnet's VTCON_NETINC fell back to DCTelnet's checkout (dctelnet-v2, optional in
upterm's repos.lock), so a fresh worktree without it failed with "[ERROR] C:uptelnet
needs the Roadshow netinclude headers". The headers now live in
third_party/roadshow-netinclude and both programs use VTCON_NETINCLUDE.
Checks, with no override set: both compile lines name the in-tree directory, none names
DCTelnet, and the headers the sources include are there.
"""
import os, subprocess

ROOT = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
NETINC = os.path.join(ROOT, "third_party", "roadshow-netinclude")
DROP = ("MAKEFLAGS", "MFLAGS", "MAKELEVEL", "VTCON_NETINC", "VTCON_NETINCLUDE", "DCTELNET_ROOT")
ENV = {k: v for k, v in os.environ.items() if k not in DROP}

for target in ("build/amiga/uptelnet", "build/amiga/Claude"):
    r = subprocess.run(["make", "-n", "-B", "-C", ROOT, target],
                       capture_output=True, text=True, env=ENV)
    out = r.stdout + r.stderr
    line = [l for l in out.splitlines() if l.startswith("vc ") and ("uptelnet.c" in l or "net_amiga.c" in l)]
    assert r.returncode == 0 and line, target + ": no compile line:\n" + out
    assert "-I" + NETINC in line[0].split(), target + " does not use " + NETINC + ":\n" + line[0]
    assert "dctelnet" not in out.lower(), target + " still names DCTelnet:\n" + out

for h in ("netdb.h", "sys/socket.h", "netinet/in.h", "arpa/inet.h", "proto/bsdsocket.h"):
    assert os.path.isfile(os.path.join(NETINC, h)), "missing " + h + " in " + NETINC
print("[OK] Roadshow headers come from third_party/roadshow-netinclude")
