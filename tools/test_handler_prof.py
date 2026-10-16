"""A PROF=1 handler prints its phase profile to the serial port without the
per-packet DBG trace (FP3.4, 2026-10-16).

The profile used to print through DBG, so it needed SERIAL=1, and SERIAL=1
also turned on the "packet"/"reply" trace lines (about 5.4 ms each on the
020) inside the very phases being measured. This compiles the handler the
way the Makefile does and reads the string literals in the object file:
the PROF lines must be there, the trace lines must not.

    python3 -m pytest -q tools/test_handler_prof.py
"""
import pathlib, shlex, subprocess, tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent


def compile_handler(flags, out):
    """The Makefile's own handler.o command (make -n), written to out."""
    dry = subprocess.run(["make", "-n", "-B", "build/amiga/vtcon-handler"] + flags,
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout
    line = next(l for l in dry.splitlines() if "handler.o handler/vtcon_handler.c" in l)
    argv = shlex.split(line)
    argv[argv.index("-o") + 1] = str(out)
    subprocess.run(argv, cwd=ROOT, check=True, capture_output=True, timeout=600)
    return out.read_bytes()


def test_prof_build_prints_phases_without_packet_trace():
    with tempfile.TemporaryDirectory() as d:
        obj = compile_handler(["PROF=1"], pathlib.Path(d) / "handler.o")
    assert b"PROF eclock/total" in obj, "the PROF=1 build lost its phase lines"
    assert b"PROF pkwrite/n" in obj
    for trace in (b"packet\0", b"reply\0"):
        assert trace not in obj, "PROF=1 alone compiled in the DBG trace %r" % trace


def test_serial_build_keeps_its_trace():
    with tempfile.TemporaryDirectory() as d:
        obj = compile_handler(["SERIAL=1"], pathlib.Path(d) / "handler.o")
    assert b"packet\0" in obj and b"reply\0" in obj
    assert b"PROF eclock/total" not in obj
