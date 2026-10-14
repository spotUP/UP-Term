"""tools/rig/muforce.py: `rig.py muforce on|off|log` -- crash feedback on the rigs (inventory Need 4).

The report parser is fed a real MuForce 46.4 report (rig 3, 68060 with MMU, 2026-10-14: build/amiga/muhit read
address 0 and wrote ROM), the argument handling and the MMU check are driven the way rig.py calls them, and
`rig.py muforce` is run as a program for its usage line. The reachability (MuForce really reports on a rig) is
the hit line of `rig.py muforce on`, `ami.py exec VTC:muhit`, `rig.py muforce log`.
"""
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import muforce  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


SAMPLE = '\n14-Oct-26  19:17:42\x07\nLONG READ from 00000000                        PC: 40646644\nUSP : 4113E0CC SR: 0004  (U0)(-)(-)  TCB: 406454B0\nData: 00000001 00000000 0000FFFF 00000001 00001000 0000000C 1015E5E1 4064622C\nAddr: 4089E07C 4089E054 40647AE0 405C2F10 405C9F3E 405C1DA0 40000810 40002278\nStck: 406465F0 00000001 405C1DB0 4061ADE5 406454B0 40646470 00000001 405C1DB0\nStck: 00000000 405C1DA0 00FA06D6 00040000 40645E94 00000000 3EEC1F00 00000000\nName: "Background CLI"  CLI: "VTC:muhit"  Hunk 0000 Offset 00000414\n\n\n14-Oct-26  19:17:43\x07\nWORD WRITE to  00F80000        data=4E71       PC: 4064662E\nUSP : 4113E0CC SR: 0000  (U0)(-)(-)  TCB: 406454B0\nData: 00000000 00000000 00000003 00000002 00001000 0000000C 1015E5E1 4064622C\nAddr: 405C1DC0 406466A4 40647AE0 405C1F60 405CADE6 405C1DA0 40000810 4000227C\nStck: 406465F0 00000002 405C1DB0 4061ADE8 406454B0 40646470 00000002 405C1DB0\nStck: 00000000 405C1DA0 00FA06D6 00040000 40645E94 00000000 3EEC1F00 00000000\nName: "Background CLI"  CLI: "VTC:muhit"  Hunk 0000 Offset 000003FE\n\n'

hits = muforce.parse_hits(SAMPLE)
check(len(hits) == 2, "a report with two hits parses to two hits, got %d" % len(hits))
if len(hits) == 2:
    a, b = hits
    check((a["kind"], a["address"], a["pc"]) == ("LONG READ", "00000000", "40646644"), "read of address 0: kind, address, PC: %r" % a)
    check((b["kind"], b["address"], b["pc"]) == ("WORD WRITE", "00F80000", "4064662E"), "write to ROM: kind, address, PC: %r" % b)
    check(a["task"] == "Background CLI" and a["cli"] == "VTC:muhit", "the task and the CLI command name are read: %r" % a)
text = muforce.summary(hits)
check("LONG READ address 00000000 PC 40646644" in text and "VTC:muhit" in text, "summary line names access, address, PC, program")
check(muforce.parse_hits("") == [] and muforce.summary([]) == "no hits", "an empty log has no hits")
check(muforce.parse_hits("Alert !! Alert 35000000 TCB: 00218EE0\n") == [], "an alert report is not a memory hit")
check(len(muforce.parse_hits(SAMPLE[:SAMPLE.index("WORD WRITE")])) == 1, "a log cut after the first hit has one hit")

# argument handling
check(muforce.parse_args(["on"]) == ("on", set()), "on")
check(muforce.parse_args(["log", "--raw"]) == ("log", {"raw"}), "log --raw")
for bad in ([], ["start"], ["on", "--raw"], ["log", "--bogus"]):
    try:
        muforce.parse_args(bad)
        check(False, "arguments %r are refused" % (bad,))
    except SystemExit as e:
        check("usage" in str(e), "arguments %r are refused with the usage line" % (bad,))

# the rig needs an MMU: only a 68030/040/060 config without -NOMMU has one
check(muforce.has_mmu("[fs-uae]\ncpu = 68060\n"), "cpu = 68060 has an MMU")
check(not muforce.has_mmu("[fs-uae]\ncpu = 68060-NOMMU\n"), "cpu = 68060-NOMMU has none")
check(not muforce.has_mmu("[fs-uae]\ncpu = 68020\n") and not muforce.has_mmu(""), "a 68020 or no config has none")

# the program: rig.py muforce with a bad command prints the usage and does not touch any rig
r = subprocess.run([sys.executable, "-I", str(HERE / "rig/rig.py"), "muforce", "bogus"],
                   capture_output=True, text=True, env={k: v for k, v in os.environ.items() if k != "UPTERM_RIG"})
check(r.returncode != 0 and "usage: rig.py muforce" in r.stderr, "rig.py muforce bogus: usage, non-zero: %r" % r.stderr[-120:])

sys.exit(1 if fails else 0)
