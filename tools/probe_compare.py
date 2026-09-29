#!/usr/bin/env python3
"""Compare the ROM console with the engine's amiga personality, case by case.

  probe_compare.py ROM_OUTPUT      (the output of romprobe on the rig)

Each case of tests/probes/amiga_cases.txt is run through build/vtdump
(amiga personality, the window size the ROM reported) after an ESC c; the
cursor must land where the ROM console put it. Prints one line per case.
"""
import pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

def unescape(s):
    out, i = bytearray(), 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            i += 1
            e = s[i]
            if e == "x":
                out.append(int(s[i + 1:i + 3], 16)); i += 3; continue
            out.extend({"e": b"\x1b", "n": b"\n", "r": b"\r", "t": b"\t", "b": b"\b", "\\": b"\\"}.get(e, e.encode()))
            i += 1
            continue
        out.extend(c.encode("latin-1")); i += 1
    return bytes(out)

def main():
    rom = {}
    size = None
    for line in open(sys.argv[1], encoding="latin-1"):
        line = line.strip()
        if line.startswith("size "):
            m = re.match(r"size (\d+);(\d+)", line)
            size = (int(m.group(2)), int(m.group(1)))  # cols, rows
        elif " " in line:
            k, v = line.split(" ", 1)
            rom[k] = v
    cols, rows = size
    bad = 0
    for line in open(ROOT / "tests/probes/amiga_cases.txt", encoding="latin-1"):
        if line.startswith("#") or "\t" not in line:
            continue
        label, raw = line.rstrip("\n").split("\t", 1)
        data = b"\x1bc" + unescape(raw)
        out = subprocess.run([str(ROOT / "build/vtdump"), str(cols), str(rows), "amiga"], input=data,
                             capture_output=True, check=True).stdout.decode("utf-8", "replace").split("\n")
        m = re.match(r"@(\d+),(\d+)", out[2 * rows])
        ours = "%d;%d" % (int(m.group(2)) + 1, int(m.group(1)) + 1)
        r = rom.get(label, "?")
        ok = r == ours
        bad += not ok
        print("%-4s %-16s rom %-8s engine %s" % ("OK" if ok else "DIFF", label, r, ours))
    print("%d cases differ (window %dx%d)" % (bad, cols, rows))
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
