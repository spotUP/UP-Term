"""wbprobe: read wbreset diag_ok and the Workbench window census from `wincensus` stdout (tests/amiga/wincensus.c).

Run `wincensus` on the rig (output also lands in T:wincensus.txt for WIN/SCREEN lines), pass its text here.
diag_ok / diag_wins come from the wbreset public record (the last CloseWorkBench: result, windows held it open);
windows is the live count of WIN lines on the WB screen, foreign the ones lacking WFLG_WBENCHWINDOW.
"""
import re


def read_wincensus(text):
    """Return {diag_ok, diag_wins, windows, foreign} or None diag fields when wbreset is not loaded (WBR none)."""
    m = re.search(r"diag_wins=(-?\d+) diag_ok=(-?\d+)", text)
    wins, ok, foreign, in_wb = 0, None, 0, False
    for line in text.splitlines():
        if line.startswith("SCREEN "):
            in_wb = " WB " in line
        elif line.startswith("WIN ") and in_wb:
            wins += 1
            foreign += "WB=NO" in line
    return {"diag_ok": int(m.group(2)) if m else None, "diag_wins": int(m.group(1)) if m else None,
            "windows": wins, "foreign": foreign}
