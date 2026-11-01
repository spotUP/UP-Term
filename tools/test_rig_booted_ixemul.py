"""tools/rig/ixpty_rig.py: a rig probe stops when the rig did not boot the ixemul it means to test.

Regression (s23, 2026-10-29): sigexit_rc_rig gave rc 0 on rig 9 after build295 was rebuilt with the fix.
use_ixemul copied the new ixemul.library into VTC:ixp6 and ran Avail FLUSH, but vsh and wasabi keep the
library open, so the boot's old copy stayed resident and the probe tested the old code. use_ixemul now
leaves VTC:ixp6 alone and stops when it differs from IXEMUL / IXNET, before any command reaches the rig.
"""
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import ixpty_rig  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def probe(booted_emul, booted_net):
    """use_ixemul on a fake rig whose VTC:ixp6 holds the given bytes: (exit message or None, commands sent,
    VTC:ixp6/ixemul.library afterwards)."""
    with tempfile.TemporaryDirectory() as d:
        d = pathlib.Path(d)
        (d / "build").mkdir()
        (d / "vtc/ixp6").mkdir(parents=True)
        (d / "build/ixemul.library").write_bytes(b"new emul")
        (d / "build/ixnet.library").write_bytes(b"new net")
        (d / "vtc/ixp6/ixemul.library").write_bytes(booted_emul)
        (d / "vtc/ixp6/ixnet.library").write_bytes(booted_net)
        sent = []
        saved = ixpty_rig.VTC, ixpty_rig.IXEMUL, ixpty_rig.IXNET, ixpty_rig.run
        ixpty_rig.VTC, ixpty_rig.IXEMUL, ixpty_rig.IXNET = d / "vtc", d / "build/ixemul.library", d / "build/ixnet.library"
        ixpty_rig.run = lambda cmd, timeout=60: (sent.append(cmd), (0, ""))[1]
        stop = None
        try:
            ixpty_rig.use_ixemul()
        except SystemExit as e:
            stop = str(e.code)
        finally:
            ixpty_rig.VTC, ixpty_rig.IXEMUL, ixpty_rig.IXNET, ixpty_rig.run = saved
        return stop, sent, (d / "vtc/ixp6/ixemul.library").read_bytes()


stop, sent, left = probe(b"old emul", b"new net")
check(stop and "restart it" in stop, "a stale VTC:ixp6/ixemul.library stops the probe: %r" % stop)
check(sent == [], "nothing reaches the rig when it booted another ixemul: %r" % sent)
check(left == b"old emul", "VTC:ixp6 keeps what the rig booted: %r" % left)

stop, sent, _ = probe(b"new emul", b"old net")
check(stop and "ixnet.library" in stop, "a stale VTC:ixp6/ixnet.library stops the probe too: %r" % stop)

stop, sent, _ = probe(b"new emul", b"new net")
check(stop is None, "the booted build runs: %r" % stop)
check(any(c.startswith("Assign LIBS: VTC:ixp6") for c in sent), "VTC:ixp6 goes first in LIBS: %r" % sent)

print("test_rig_booted_ixemul: %s" % ("FAIL" if fails else "OK"))
sys.exit(1 if fails else 0)
