"""emuwin.py -- what the emulator's own window shows, captured on the host.

wasabid reads the guest from inside, so it sees nothing when the guest is stuck: an Exec alert ("Software
Failure", "Guru Meditation #xxxxxxxx.xxxxxxxx") is drawn outside the Workbench screen, and a boot that waits in
a DOS requester ("Please insert volume System: in any drive", rig 94, 2026-11-08) never starts wasabid at all.
This module finds the rig's FS-UAE window (by the config path in its command line), saves a screenshot of it
with macOS screencapture and reads it with Tesseract.

Shared by standalone_rig.py (a guru while a test runs) and rig.py start (a boot that never answers)."""
import pathlib, re, subprocess, time

SWIFT_WINDOWS = (
    'import CoreGraphics\nimport Foundation\n'
    'let l = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]\n'
    'for w in l { let b = w["kCGWindowBounds"] as! [String: Any]\n'
    '  print(w["kCGWindowNumber"]!, w["kCGWindowOwnerPID"]!, b["Width"]!, b["Height"]!) }\n')

# the words of the screens a stuck guest shows: a DOS volume requester, an Exec alert
REQUESTER_WORDS = ("insert volume", "please insert", "software failure", "guru", "recoverable alert",
                   "not a dos disk", "is full", "write-protected", "read/write error")


def requester_lines(lines):
    """The OCR lines that name a requester or an alert (case-insensitive), in their order."""
    return [l for l in lines if any(w in l.lower() for w in REQUESTER_WORDS)]


def window_text(cfg, out_dir, name, top=1.0):
    """Capture the FS-UAE window started with config `cfg` and OCR it.

    The full screenshot is kept as out_dir/<name>-<HHMMSS>.png (out_dir must survive a rig stop: a paths.REPORTS
    folder). `top` is the share of the window height that Tesseract reads (0.2 = the top fifth, where an Exec
    alert is drawn). Returns (png path or None, non-empty OCR lines); on a failure the lines hold one note why."""
    try:
        pid = subprocess.run(['pgrep', '-f', 'fs-uae.*' + re.escape(str(cfg))],
                             capture_output=True, text=True).stdout.split()
        if not pid:
            return None, ['(no FS-UAE process for this rig)']
        out_dir = pathlib.Path(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        swift = out_dir / '_windows.swift'
        swift.write_text(SWIFT_WINDOWS)
        wins = [l.split() for l in subprocess.run(['swift', str(swift)], capture_output=True, text=True).stdout.splitlines()]
        big = [w for w in wins if len(w) == 4 and w[1] == pid[0] and int(w[2]) > 600 and int(w[3]) > 400]
        if not big:
            return None, ['(no FS-UAE window found)']
        png = out_dir / ('%s-%s.png' % (name, time.strftime('%H%M%S')))
        subprocess.run(['screencapture', '-x', '-o', '-l' + big[0][0], str(png)], check=True)
        from PIL import Image
        im = Image.open(png).convert('RGB')
        w, h = im.size
        part = out_dir / '_ocr.png'
        ch = max(1, int(h * top))
        im.crop((0, 0, w, ch)).resize((w * 2, ch * 2)).save(part)
        out = subprocess.run(['tesseract', str(part), '-', '--psm', '6'], capture_output=True, text=True).stdout
        return png, [l.strip() for l in out.splitlines() if l.strip()]
    except (OSError, ValueError, subprocess.SubprocessError, ImportError) as e:
        return None, ['(emulator window unreadable: %s)' % e]
