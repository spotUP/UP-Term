"""tools/rig: the two rig helpers that replace LIBS: refuse a real Amiga.

Regression (LIBS-ASSIGN-AUDIT, 2026-11-01): ixpty_rig.use_ixemul runs `Assign LIBS: VTC:ixp6` (or
`Assign LIBS: DH0:Libs`) and install_rig.stock_libs runs `Assign LIBS: DH0:Libs`, both without ADD. Many
rig probes call them. Pointed at the A1200 (AMI_HOST) they would drop its AmiSSL:Libs and MUI:Libs ADDs
and assign drawers it does not have. Both now stop before the first command when AMI_HOST or WASABI_HOST
names a machine other than this Mac.
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import install_rig  # noqa: E402
import ixpty_rig  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def attempt(mod, call, env):
    """Run call() with mod.run recording commands under env: (exit message or None, commands sent)."""
    sent = []
    saved_run, saved_env = mod.run, dict(os.environ)
    mod.run = lambda cmd, *a, **k: (sent.append(cmd), (0, ""))[1]
    for k in ("AMI_HOST", "WASABI_HOST"):
        os.environ.pop(k, None)
    os.environ.update(env)
    try:
        call()
        return None, sent
    except SystemExit as e:
        return str(e), sent
    finally:
        mod.run = saved_run
        os.environ.clear()
        os.environ.update(saved_env)


ixpty_rig.booted_ixemul = lambda: None   # the rig's VTC:ixp6 is not here; that guard has its own test
install_rig.amissl_libs = lambda: []

for name, mod, call in (("ixpty_rig.use_ixemul", ixpty_rig, lambda: ixpty_rig.use_ixemul(False)),
                        ("ixpty_rig.use_ixemul --orig", ixpty_rig, lambda: ixpty_rig.use_ixemul(True)),
                        ("install_rig.stock_libs", install_rig, install_rig.stock_libs)):
    for env in ({"AMI_HOST": "192.168.0.74"}, {"WASABI_HOST": "192.168.0.74"}):
        msg, sent = attempt(mod, call, env)
        check(msg and "rig only" in msg, "%s refuses %s: %r" % (name, env, msg))
        check(sent == [], "%s sends nothing to %s: %r" % (name, env, sent))
    for env in ({}, {"AMI_HOST": "127.0.0.1"}):
        msg, sent = attempt(mod, call, env)
        check(msg is None, "%s runs on a rig %s: %r" % (name, env, msg))
        check(any(c.startswith("Assign LIBS: ") for c in sent), "%s assigns LIBS: on a rig %s: %r" % (name, env, sent))

print("rig_libs_rig_only: %s" % ("FAIL %d" % fails if fails else "ok"))
sys.exit(1 if fails else 0)
