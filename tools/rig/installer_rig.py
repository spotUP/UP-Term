#!/usr/bin/env python3
"""installer_rig.py -- REAL-INSTALLER-TEST: the kit's Install icon the way a
user starts it. The real Installer runs build/dist/up-term/Install (the
Installer script, dist/Install.installer) with the icon's tooltypes, visible
on the rig's screen; this script reads its pages through wasabi's UITREE
and clicks them, then checks what landed and runs Uninstall.

  default   AVERAGE (the icon's MINUSER/DEFUSER), every page answered with
            its default: the drawer UP-Term on the local disk with the most free room (SYS: when none)
  dest      AVERAGE, the drawer page given VTC:KitTest/UP-Term (typed into the
            askdir string gadget)
  space     AVERAGE on a System: partition made too full for the default choices (K7: a filler file leaves
            about 12 MB; the kit needs more): the drawer page is answered with SYS:UP-Term (the default is the
            biggest disk, VTCX: on the rig), and the room message
            (the choices need N KB, the drawer's disk has M KB; Proceed asks for another drawer) comes up
            before anything is copied. The case Aborts on the drawer page that follows it, and checks that
            nothing was installed. Every page is saved as build/<rig>/installer/space-<n>.png to read.
  novice    NOVICE: no question asked, only the pages marked (all) -- the
            welcome and the summary -- and the default drawer
  update    AVERAGE as default, then the kit with one file changed
            (Files/unifont/SOURCE.txt, its MANIFEST made again) and Install
            again: the first page offers Update (the default), and after it
            only that file's date in UP-Term: has changed, UP-Term:MANIFEST is
            the new kit's, S:User-Startup and the recorded options are as
            they were; both times are logged (installer update plan U8)

After each Install: UP-Term: names the drawer, ENVARC:up-term/Dir, the
marked UP-Term: block in S:User-Startup (before the GG: block), the files in
the drawer and in C:/L:/LIBS: (the original ixemul kept as .orig), every file
of the kit's unifont drawer copied (copyfiles (all) takes the subdrawers),
UPConsole STATUS prints the kit line. Then Uninstall:
S:User-Startup byte for byte as before, no UP-Term:, the drawer gone, the
original libraries back. (install_rig.py drives install.dos directly, with
the full set of checks; this one proves the Installer path reaches the same
result.)

  addon-<id>  (one per add-on of dist/packages.txt, K2-11) the base by its Installer, then the add-on's own
            Install icon, both AVERAGE with every page answered by its default; the add-on's files, its
            record and its program answering; its Uninstall; the base Uninstall
  migrate     the single kit of 0cd6f0b (LEGACY_KIT) by its Installer, then the new base's Install over it
            (the Update path: no drawer page, an edit planted in an unchanged file survives, the handed
            nvim keeps its date, UP-Term:pkg/legacy is kept), then the nvim add-on's Install (Update from
            "installed by the single kit", the planted edit survives, the changed files are the new ones);
            the base Uninstall takes everything away

Every page seen is logged to build/rig/installer/<case>.pages.txt (UITREE
text, for reading what the Installer showed); the verdict goes to
build/rig/installer/<case>.json with the fingerprint of the kit's scripts.
Resumable:
  installer_rig.py                the cases not yet passed with this kit
  installer_rig.py ONLY dest      one case (re-run even when it passed; --only dest is the same)
  installer_rig.py FAILED         only the cases that failed last time
  installer_rig.py FORCE          all, from zero
  installer_rig.py ONLY addon-dopus FORCE --no-dopus --purpose "<who: what>"   the case on a rig started without Directory Opus
Needs `make dist` and the default rig (tools/rig/rig.py, the 3.1 machine
install_rig.py is written for; it is started fresh here). Not run by `make
test`: it needs the emulator.

Confirmed on the rig (2026-10-07, Installer 47.19): the Installer's custom gadgets have no labels, so pages
are driven by gadget id (90 Proceed, 91 Abort, 89 Make New Drawer, 92 the askdir string); the string gadget keeps
only a path that exists, so the dest drawer is made through Make New Drawer (dialog: string 1, OK 90, Cancel 91).
A page unchanged 60 s after a click is a FAIL (never a hang)."""
import os, pathlib, re, shlex, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import verdicts
import install_rig as ir
import amarchy_window_rig as amw
from assign_rig import rig

ROOT = ir.ROOT
VTC = ir.VTC
OUT = paths.RIG / "installer"
OUT.mkdir(parents=True, exist_ok=True)  # pages are logged before the first verdict is written
KIT = ir.KIT
# a clean image (--os) has a 12 MB system partition: the kit goes to its Work: drive
CLEAN = "--os" in os.environ.get("UPTERM_RIG_FLAGS", "")
WORKDEST = "Work:Apps/UP-Term"
CASES = {"default": ("AVERAGE", None), "dest": ("AVERAGE", WORKDEST if CLEAN else "VTC:KitTest/UP-Term"),
         "novice": ("NOVICE", None), "update": ("AVERAGE", WORKDEST if CLEAN else None), "space": ("AVERAGE", "SYS:UP-Term"),
         "kingcon": ("AVERAGE", None)}
# UPDATE-DETECT: the update case over an install that left no ENVARC:up-term/opts (a Shell install.dos run)
CASES["update-noopts"] = CASES["update"]
for _pkg in ir.addon_ids():
    CASES["addon-" + _pkg] = ("AVERAGE", None)
CASES["migrate"] = ("AVERAGE", None)
FULLDEST = "DH1:KitTest-full"   # the fullsuite case's drawer for UP-Term: never SYS: (it is small and the owner's)
FULL_DRAWER = "nvim"            # the add-on staged as an unpacked drawer; the rest are .lha files
CASES["fullsuite"] = ("AVERAGE", None)
# F1: the fresh kit, every box on every page ticked (only drawer and install mode stay default), a reboot, Amarchy up
CASES["fresh-all"] = ("AVERAGE", None)
# The default drawer is UP-Term on the local disk with the most free room (UPBigVol -> ENV:UPTBIGVOL), which on the rig
# is VTCX: or BOOTX: (host directories, 2 GB), not SYS:. The `space` case asks for SYS:UP-Term itself (it needs a small disk).
# BIGVOL: the drawer page's default is UP-Term on the local disk with the most free room (UPBigVol), not SYS:
CASES["bigvol"] = ("AVERAGE", None)
# the rig: DH0 System 106 MB (SYS:, 217222 blocks), DH1 Rushhours 519 MB, VTCX and BOOTX (uaefs directories) 2 GB
ROOMDEST = "DH1:KitTest-default"   # the default case's answer to the room page: Rushhours, about 1 GB free
UPDATED = "Files/unifont/SOURCE.txt"   # the update case changes this file of a copy part (copy-unifont)
KITWINDOW = re.compile(r'VTC:(distkit|kit-\w+|legacykit)/Install')   # the Installer window's title: the kit it runs
RAMIGA = 0x0080
# INSTALLER_KEEP=1 with `ONLY fresh-all`: stop after the reboot check, leaving the fully ticked kit installed on the rig
# (for timing work on a kit-installed machine); Uninstall and the fixture restore are skipped.
KEEP_INSTALLED = bool(os.environ.get("INSTALLER_KEEP"))
# INSTALLER_CHAIN=1: before each Proceed log input.device's handler chain (VTC:inhandlers ADDR: name, node, is_Code,
# is_Data), so a hang at a click names the handler the click ran (INSTALLER-END-CRASH). `make build/amiga/inhandlers` first.
CHAIN = bool(os.environ.get("INSTALLER_CHAIN"))
# INSTALLER_OWNERMAP=1 (with INSTALLER_CHAIN): also log VTC:ownermap (seglist, library, device ranges, commodities brokers
# with a live-task flag) before each Proceed; INSTALLER_OWNERMAP=<hex addr> adds the owner of that address. `make build/amiga/ownermap`.
OWNERMAP = os.environ.get("INSTALLER_OWNERMAP", "")
KEY_X, KEY_RETURN = 0x32, 0x44


def case_packages(case):
    """([packages the case installs], [more files its fingerprint reads])."""
    if case and case.startswith("addon-"):
        return ["base", case[len("addon-"):]], []
    if case == "fullsuite":
        return ["base"] + ir.addon_ids(), []
    if case == "migrate":
        return ["base", "nvim"], [ir.LEGACY_KIT / "Files/MANIFEST", ir.LEGACY_KIT / "Install"]
    return ["base"], []


def fingerprint(case=None):
    pk, extra = case_packages(case)
    built = [f for p in pk for f in (ir.kit_dir(p) / "Install", ir.kit_dir(p) / "Files/install.dos")]
    return verdicts.fingerprint(ir.kit_fingerprint(*pk).encode(), *built, *extra)


STORE = verdicts.Store(OUT)
verdict, record_verdict = STORE.verdict, STORE.record


def record(case, ok, detail):
    STORE.record(case, ok, detail, fingerprint(case))


Checks = verdicts.Checks


def windows(tree):
    """UITREE as [(title, [(label, (x, y, w, h))])] for the front screen's windows."""
    out = []
    for line in tree.splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            out.append((f[-1], []))
        elif line.startswith('G ') and out:
            f = line.split(None, 5)
            w, h = map(int, f[4].split('x'))
            out[-1][1].append(((f[5] if len(f) > 5 else '').strip().strip('"'), (int(f[2]), int(f[3]), w, h)))
    return out


def drawer_delete(drawer):
    """A drawer and its icon gone: Delete DIR ALL leaves DIR.info, and Workbench then shows a drawer icon
    that opens nothing (VTCX:Apps/UP-Term.info on rig 1, 2026-10-14, left by the migrate case)."""
    return 'Delete "%s" "%s.info" ALL QUIET' % (drawer, drawer)


def dest_cleanup(dest, parent_existed):
    """The commands that take a case's chosen drawer away: the drawer with its icon, and the drawer
    above it with its icon when the case made it (the Installer writes the parent's .info; VTCX:Apps.info
    stayed on rig 1 after Apps was deleted, 2026-10-14). A parent that was there before is not ours."""
    cmds = [drawer_delete(dest)]
    parent = dest.rsplit('/', 1)[0]
    if not parent_existed and '/' in dest:
        cmds.append(drawer_delete(parent))
    return cmds


def make_dest_parent(dest):
    """MakeDir the parent of a chosen drawer; True when it was there already."""
    parent = dest.rsplit('/', 1)[0]
    existed = ir.run('List >NIL: "%s"' % parent)[0] == 0
    ir.run('MakeDir >NIL: "%s"' % parent)
    return existed


def click(box):
    # UITREE gives screen pixels, CLICK takes Intuition pointer units: the same on the 3.1 rig's RTG screen,
    # y doubled on 3.2's native non-interlaced Workbench (a raw click at Proceed's y 191 landed at y 95, in
    # the page's text: the 3.2 stall of 2026-10-08). ami.click_px scales.
    x, y, w, h = box
    ami.click_px(x + w // 2, y + h // 2)


BUSY_RETRY = 5   # s between asks while wasabid refuses connections


def requester_detail():
    """Title, task and buttons of every requester open, as `ami.py requesters` gives them: the D10 log said only "a
    system requester came up", and the two Software Failure requesters of the boot had to be read by hand."""
    try:
        return "; ".join(ami.describe_requester(r) for r in ami.requesters()) or "none left to read"
    except Exception as e:   # a log line only: a wasabi error here must not hide the failure it describes
        return "not read (%s)" % e


def installer_running(inst):
    # Status COM matches the command as it was started (the full path when one was given)
    try:
        return ir.run('Status >NIL: COM "%s"' % inst)[0] == 0
    except SystemExit as e:
        if 'still running' in str(e):   # the agent is busy with an earlier command: the Installer is not gone
            return True
        raise
    except ConnectionError:   # wasabid refuses or resets while a nested Install (AmiSSL) hogs the machine: busy, not gone
        return True


def find_installer():
    for p in ('Installer', 'SYS:System/Installer', 'SYS:Utilities/Installer', 'C:Installer', 'SYS:C/Installer'):
        if ir.run('Which >NIL: "%s"' % p)[0] == 0 or ir.run('List >NIL: "%s"' % p)[0] == 0:
            return p
    return None


def gadget_text(gid):
    """The string gadget `gid`'s text in the Installer's window, from UITREE (G id x y WxH string state "" "text")."""
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        f = line.split(None, 2)
        if line.startswith('G ') and f[1] == str(gid):
            return line.rsplit('"', 2)[-2] if line.count('"') >= 4 else ''
    return None


def type_into(box, gid, text, tries=6):
    """Click string gadget `gid`, clear it (RAmiga-X, retried: the qualified key is sometimes lost under load and
    then types an X), type text, Return. True when UITREE shows the text typed, before Return."""
    for _ in range(tries):
        click(box)
        time.sleep(1)
        for _ in range(tries):
            ami.key(KEY_X, RAMIGA)
            time.sleep(0.5)
            if gadget_text(gid) == '':
                break
            if 'X' in (gadget_text(gid) or ''):
                pass
        if gadget_text(gid) != '':
            continue
        ami.req(0x08, bytes([4]) + text.encode('latin-1'))
        time.sleep(0.5)
        if gadget_text(gid) == text:
            ami.key(KEY_RETURN)
            time.sleep(1.5)
            return True
    return False


def gadgets(tree):
    """UITREE as {window title: {gadget id: ((x, y, w, h), label)}}."""
    out, cur = {}, None
    for line in tree.splitlines():
        if line.startswith('W '):
            cur = out.setdefault(shlex.split(line)[-1], {})
        elif line.startswith('G ') and cur is not None:
            f = line.split(None, 5)
            w, h = map(int, f[4].split('x'))
            cur[int(f[1])] = ((int(f[2]), int(f[3]), w, h), (f[5] if len(f) > 5 else '').strip().strip('"'))
    return out


def window_box(tree):
    """(x, y, w, h) of the kit's Installer window in a UITREE, or None."""
    for line in tree.splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            if KITWINDOW.match(f[-1]):
                w, h = map(int, f[4].split('x'))
                return int(f[2]), int(f[3]), w, h
    return None


def window_ocr(tree, name, y_to=None, scales=(4,), resamplers=None, thresholds=(100,)):
    """The text of the Installer window (UITREE has none), read from the screen's pixels with tesseract:
    the window's rows down to screen row `y_to`, scaled up, black on white. The last image is kept as
    OUT/<name>.png. Returns one text for the first scale, or with several `scales` x `resamplers` the list of
    every pass's text (Topaz digits read differently at each: a leading 8 comes back as $ or 3 now and then)."""
    import subprocess
    from PIL import Image
    box = window_box(tree)
    if not box:
        return '' if len(scales) == 1 and not resamplers else []
    x0, y0, w, h = box
    W, H, rows = ami.grab()
    y1 = min(H, y_to or y0 + h)
    im = Image.frombytes('RGB', (W, H), b''.join(bytes(r) for r in rows)).crop((x0 + 5, y0 + 14, min(W, x0 + w - 5), y1)).convert('L')
    outs = []
    for sc in scales:
        for rs in (resamplers or (Image.LANCZOS,)):
            for th in thresholds:
                big = im.resize((im.width * sc, im.height * sc), rs).point(lambda v: 255 if v > th else 0)
                png = OUT / (name + '.png')
                big.save(png)
                out = subprocess.run(['tesseract', str(png), 'stdout', '--psm', '6'], capture_output=True, text=True, timeout=60).stdout
                outs.append(' '.join(out.split()))
    return outs if (len(scales) > 1 or resamplers) else outs[0]


def prompt_digest(tree, gads):
    """A digest of the page's prompt rows (above the first checkbox): two option pages with the same gadget
    ids (the serial/wasabi page and the console page) are told apart by it."""
    import hashlib
    box = window_box(tree)
    if not box:
        return None
    x0, y0, w, h = box
    y1 = min(g[0][1] for k, g in gads.items() if 2 <= k < 90) - 2 if any(2 <= k < 90 for k in gads) else y0 + h
    W, H, rows = ami.grab()
    return hashlib.md5(b''.join(bytes(rows[y][(x0 + 5) * 3:(x0 + w - 5) * 3]) for y in range(y0 + 14, min(H, y1)))).hexdigest()


# bv_room_text's refusal (install/bigvol.h): the needed figure is on the line after the free one
ROOM_RE = re.compile(r'has\s+(\S+)\s*KB\s+\w+.*?need\s+about\s+(\S+)\s*KB', re.S)


def read_room(tree, case):
    """(KB free, KB needed, text) of the Installer's room page ("<dest> has N KB free; the parts you chose
    need about M KB"), read from the screen; None when the page is no room page. The digits are voted: the
    number each of N and M most of the OCR passes (5 scales x 2 resamplings x 2 thresholds) read; a digit the font gets wrong ($ or 3 for 8) loses the vote. Up to 3 reads, a second apart."""
    from collections import Counter
    from PIL import Image
    for i in range(3):
        time.sleep(1)
        txts = window_ocr(tree, case + '-room', scales=(3, 4, 5, 6, 8), resamplers=(Image.LANCZOS, Image.BICUBIC), thresholds=(100, 140))
        free, need = Counter(), Counter()
        for t in txts:
            m = ROOM_RE.search(t)
            if m and m.group(1).isdigit() and m.group(2).isdigit():   # a misread digit ($) is no vote
                free[m.group(1)] += 1
                need[m.group(2)] += 1
        (OUT / (case + '-room.votes.txt')).write_text('free %s\nneed %s\n' % (dict(free), dict(need)))
        n, mm = [c.most_common(1)[0][0] if c else '' for c in (free, need)]
        if n.isdigit() and mm.isdigit():
            return int(n), int(mm), '%s [free %s, need %s of %d passes]' % (txts[-1], free[n], need[mm], len(txts))
        tree = ami.req(0x0D).decode('latin-1')
    return None


def installer_page(tree):
    """(title, {id: (box, label)}) of the Installer's window: the one with Proceed (90) or Abort Install (91)."""
    for title, gads in gadgets(tree).items():
        if 90 in gads or 91 in gads:
            return title, gads
    return None, {}


def unticked_boxes(gads):
    """[(id, box)] of the Installer page's checkboxes still to tick: its enabled, unselected buttons and
    toggles 2..88. Only `gads`, the Installer window's own gadgets: on a --no-dopus rig the plain Workbench
    window behind it has its own buttons 3..6, and reading those from the whole UITREE clicked ids the
    page does not have (KeyError: 5, 2026-10-29)."""
    out = []
    for gid, (box, rest) in gads.items():
        m = re.match(r'(?:button|toggle)( \w+)?', rest)
        if m and 2 <= gid < 89 and not any(w in (m.group(1) or '') for w in ('disabled', 'selected')):
            out.append((gid, box))
    return out


def own_ask_answer(gads):
    """The gadget to click on a page of the Installer's own ask/confirm kind, or None. Install-AmiSSL's
    pages (the AmiSSL: assign upgrade question, the CPU choice, 2026-10-18) have no Proceed 90: 1 and 2 are
    the two buttons (2 left, the affirmative: Yes / Proceed; 1 right: No / Skip), 91 Abort Install, 100 Help.
    The choice stays at its default, so the click is the left button (the rig rule: only destination and
    mode stay default; these pages have no boxes to tick)."""
    if 90 not in gads and {1, 2, 91, 100} <= set(gads) and gads[2][0][0] < gads[1][0][0]:
        return 2
    return None


def layout_judged(tree, why):
    """Whether a page dump's text is judged for layout faults. The Make New Drawer dialog is the Installer's
    own, drawn over the askdir page (its frames read as a panel, the drawer list under it as stray text: 3.1
    dest, 2026-10-08). Two Installer windows (a nested add-on Install over the base) overlap while one closes.
    A window with neither Proceed (90) nor Abort Install (91) is a page half drawn: the last page's buttons
    stood in the bottom strip before their gadgets were added, "text in the bottom border strip (y 204..205)
    on [None]" (fresh-all, rig 9 2026-10-29). Only our whole pages are judged."""
    if why in ('dialog', 'no dialog', 'after Make New Drawer'):
        return False
    if sum(1 for t in gadgets(tree) if 'Install' in t) > 1:
        return False
    return installer_page(tree)[0] is not None


def installer_name(title):
    """An Installer window's title without its progress: "Install-AmiSSL (100% done)" is "Install-AmiSSL"."""
    return re.sub(r' \(\d+% done\)$', '', title or '')


def reboot_later(gads, text):
    """The gadget that answers P_END's "Reboot now?" with Later, or None when `text` (the page's OCR) does
    not ask it. The page is matched by its question, not by a gadget list or the bare word: P_END's
    (message #done) page before it says "AmiSSL installed: LIBS: has it now, usable at once." and is answered with Proceed
    (rig 9 2026-10-28). Later is the right button: id 1 on a page with the ask gadgets, id 91 on a
    Proceed/Abort-shaped page whose right button reads Later. A reboot question with neither returns 0: no
    gadget is safe to click (the default is Reboot)."""
    if not re.search(r'reboot\s+now', text, re.I):
        return None
    return 1 if 1 in gads else 91 if 91 in gads and 'later' in text.lower() else 0


def text_faults(tree):
    """Text of the Installer's page that the user cannot read whole, from the screen's pixels (UITREE has
    no text): ([fault], [text band]). Text is the black pen with the rules taken out (a run of more than 30
    pixels across, or 16 or more down) and the gadgets' own boxes left out. Faults: text in the bottom
    strip of the window (a line cut by its border: the welcome page's last line on 3.2's 560x176 window,
    2026-10-08), text under the button row, text beside copyfiles' panel (P_COPY's lines peeking out)."""
    box, gads = None, []
    for line in tree.splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            cur = bool(KITWINDOW.match(f[-1]))
            if cur:
                w, h = map(int, f[4].split('x'))
                box = (int(f[2]), int(f[3]), w, h)
        elif line.startswith('G ') and box and cur and 'sys:' not in line:
            f = line.split()
            gw, gh = map(int, f[4].split('x'))
            gads.append((int(f[2]), int(f[3]), gw, gh))
    if not box:
        return [], []
    x0, y0, w, h = box
    W, H, rows = ami.grab()
    px, cols = set(), {}
    for y in range(y0 + 12, min(H, y0 + h)):
        r = rows[y]
        xs = [x for x in range(x0 + 5, min(W, x0 + w - 5)) if r[x * 3:x * 3 + 3] == b'\0\0\0']
        run = [xs[0]] if xs else []
        for x in xs[1:] + [None]:
            if x is not None and x == run[-1] + 1:
                run.append(x)
                continue
            if len(run) <= 30:
                px.update((xx, y) for xx in run)
            else:
                cols.setdefault('h', []).append((y, run[0], run[-1]))
            run = [x] if x is not None else []
    vert = {}
    for x, y in px:
        vert.setdefault(x, []).append(y)
    vrules = []
    for x, ys in vert.items():
        ys.sort()
        run = [ys[0]]
        for y in ys[1:] + [None]:
            if y is not None and y == run[-1] + 1:
                run.append(y)
                continue
            if len(run) >= 16:
                px.difference_update((x, yy) for yy in run)
                vrules.append((x, run[0], run[-1]))
            run = [y] if y is not None else []
    px = {(x, y) for x, y in px
          if not any(gx - 1 <= x <= gx + gw and gy - 1 <= y <= gy + gh for gx, gy, gw, gh in gads)}
    faults = []
    low = [y for y in (y for _, y in px) if y >= y0 + h - 6]
    if low:
        faults.append('text in the bottom border strip (y %d..%d)' % (min(low) - y0, max(low) - y0))
    btop = min((gy for gx, gy, gw, gh in gads if gy + gh >= y0 + h - 40 and gh >= 12), default=None)
    if btop is not None:
        under = sorted(y for _, y in px if btop <= y < y0 + h - 6)
        if under:
            faults.append('text under the button row (y %d..%d)' % (under[0] - y0, under[-1] - y0))
    for vx, vt, vb in vrules:   # copyfiles' panel: its right edge runs >= 100 lines, its bottom rule ends there
        if vb - vt < 100:
            continue
        left = min((a for y, a, z in cols.get('h', []) if vb - 1 <= y <= vb + 2 and abs(z - vx) <= 2), default=None)
        if left is None:
            continue
        side = sorted((x, y) for x, y in px if vt <= y <= vb and (x < left - 1 or x > vx + 1))
        if side:
            faults.append('text beside the copy panel (x %s, y %d..%d)' % (
                ' '.join(sorted({str(x - x0) for x, _ in side})[:6]), min(y for _, y in side) - y0,
                max(y for _, y in side) - y0))
    bands, cur = [], None
    for y in sorted({y for _, y in px}):
        xs = [x for x, yy in px if yy == y]
        if cur and cur[1] >= y - 1:
            cur[1], cur[2], cur[3] = y, min(cur[2], min(xs)), max(cur[3], max(xs))
        else:
            cur = [y, y, min(xs), max(xs)]
            bands.append(cur)
    return faults, ['y%d-%d x%d-%d' % (a - y0, b - y0, c - x0, d - x0) for a, b, c, d in bands]


SETTLE_STEP = 0.4   # drive: how often the UI tree is asked while it waits for the page to change


def wait_change(tree, secs):
    """Wait up to `secs` for the Installer's UI tree to differ from `tree` (a new page, new gadgets, a gadget gone,
    a percent step), asking every SETTLE_STEP s. Returns at once when it changed; the bound is the old fixed sleep."""
    end = time.time() + secs
    while time.time() < end:
        time.sleep(min(SETTLE_STEP, max(end - time.time(), 0.01)))
        try:
            if ami.req(0x0D).decode('latin-1') != tree:
                return True
        except ConnectionError:   # wasabid busy: keep waiting inside the bound
            pass
    return False


def drive(case, inst, dest, baseline_titles, **kw):
    """drive_pages, but a link lost mid-run (wasabi's "connection closed by the Amiga" is a SystemExit) prints the
    pages answered so far before it ends the run: without them a crash on the last page left no trace (rig 9,
    2026-10-28). Returns (finished, log)."""
    log = []
    try:
        return drive_pages(log, case, inst, dest, baseline_titles, **kw)
    except SystemExit:
        print("installer_rig: %s: the link dropped after:\n  %s" % (case, "\n  ".join(log) or "(no page)"))
        raise


def drive_pages(log, case, inst, dest, baseline_titles, timeout=5400, stall=60, work_limit=2700, poll=20,
                expect_abort=False, room_dest=None, console=False, tick_all=False, askdir_probe=None):
    """Answer the Installer's pages by gadget id (its custom gadgets have no labels): 90 Proceed, 91 Abort
    (never), 89 Make New Drawer (never), 1 on the install-mode page Install for Real, 92 the askdir string.
    room_dest: the drawer to ask for when the room page ("<dest> has N KB free; the parts you chose need about
    M KB") comes after the drawer page (read from the screen, logged as `room page: N KB free, M KB needed`);
    console: tick "UP-Term serves CON: and RAW:" on the console page.
    askdir_probe: a list; at the first drawer page drive appends the string gadget's text, clicks Abort Install (91) and returns.
    tick_all: on every options page tick every box not ticked (and not disabled) before Proceed; a page with the
    install-mode gadget 1 is a choice, not a list of boxes. Each Installer (the nested ones too: their window
    title is their script) asks its own drawer and room pages.
    A page unchanged `stall` s after a click is a FAIL; a page without Proceed is the copy: waited out
    (percent logged) up to work_limit s. log collects what happened, in order. Returns (finished, log)."""
    pages = OUT / (case + ".pages.txt")
    pages.write_text('')
    t0 = time.time()
    last_sig, clicked_at, retried, typed, chose = None, None, None, False, set()
    askdirs, shots = 0, 0
    last_page_at = time.time()
    work_since, last_pct, dumped_pct = None, None, set()
    room, after_ask, asks, ticked, drawer = None, False, 0, False, DrawerAsks()
    cur_title, ticked_sigs = None, set()
    outer = None   # the Installer this case started; a nested add-on Install (AmiSSL) has another window name

    seen_faults = set()

    def geom(tree):
        # where the checkboxes sit tells two option pages with the same gadget ids apart
        return tuple(re.findall(r'^G [2-9]\d? (\d+ \d+) \d+x\d+ (?:button|toggle)', tree, re.M))

    def dump(tree, why):
        faults, bands = text_faults(tree) if layout_judged(tree, why) else ([], [])
        with pages.open('a') as f:
            f.write('---- %.0f s %s\n%s\ntext: %s\n' % (time.time() - t0, why, tree, ' '.join(bands)))
            for x in faults:
                f.write('LAYOUT: %s\n' % x)
        for x in faults:
            if x not in seen_faults:
                seen_faults.add(x)
                log.append('LAYOUT %s on [%s]' % (x, installer_page(tree)[0]))

    while time.time() - t0 < timeout:
        if not installer_running(inst):
            return True, log
        try:
            tree = ami.req(0x0D).decode('latin-1')
        except ConnectionError:   # wasabid not answering yet (nested Install at work): ask again; timeout still ends the run
            time.sleep(BUSY_RETRY)
            continue
        if any(t in ('System Request', 'Volume Request') for t, _ in windows(tree)):
            log.append('a system requester came up: %s' % requester_detail())
            dump(tree, 'requester')
            return False, log
        title, gads = installer_page(tree)
        if title and title != cur_title:
            outer = outer or installer_name(title)
            if cur_title:   # another Installer (a nested add-on Install) with its own drawer and room pages
                typed, room, drawer = False, None, DrawerAsks()
                log.append('Installer window [%s]' % title)
            cur_title = title
        sig = (title, tuple(sorted(gads)), tuple(l for _, l in gads.values()), prompt_digest(tree, gads) if console and 90 in gads else None)   # message pages with the same ids (installed, refused, open) differ by their text
        if sig != last_sig:
            dump(tree, 'page')
            log.append('page [%s] ids %s' % (title, ' '.join(map(str, sorted(gads)))))
            if case == 'update-2':   # the pages file holds geometry only: the Update page's text is read from the pixels
                time.sleep(1)
                log.append('page text: %s' % window_ocr(tree, case + '-page'))
            last_sig, clicked_at, retried = sig, None, None
            last_page_at = time.time()
            if 89 in gads and 92 in gads and askdir_probe is not None:
                askdir_probe.append(gadget_text(92))
                click(gads[91][0])   # Abort Install: nothing was copied
                time.sleep(5)
                return True, log
            asks = drawer.see(gads)
            if room_dest and asks >= 1 and not room and set(gads) <= {0, 90, 91}:   # a message page after the drawer page
                room = read_room(tree, case)
                if room:
                    log.append('room page: %d KB free, %d KB needed' % room[:2])
                    log.append('room page text: %s' % room[2])
            after_ask = False
            if console and ticked and 90 in gads:
                time.sleep(1)
                log.append('page text: %s' % window_ocr(tree, case + '-page'))
            if room_dest and asks >= 2 and not room:
                log.append('the drawer page came a second time but the room page was not read (%s-room.png)' % case)
                return False, log
            if not expect_abort and asks > (2 if room_dest else 1):
                # the room page sent us back to the drawer page once more than this case answers: the drawer
                # typed last is too small too, and Proceed would loop drawer -> room page until the timeout
                log.append('room page repeats: the drawer page came %d times in [%s]' % (asks, title))
                dump(tree, 'room loop')
                return False, log
            if expect_abort:
                # K7: every page as an image (to read the room message), and the drawer page counted: a second
                # one is the room message's Proceed (ask for another drawer)
                shots += 1
                w, h, rows = ami.grab()
                ami.png(str(OUT / ('%s-%02d.png' % (case, shots))), w, h, rows)
                if 89 in gads and 92 in gads:
                    askdirs += 1
        m = re.findall(r'(\d+)% done', tree)
        if m and m[-1] != last_pct:
            last_pct = m[-1]
            log.append('%s%% done at %.0f s' % (last_pct, time.time() - t0))
        if expect_abort and askdirs >= 2 and 91 in gads:
            # the room message was answered with Proceed and the drawer page came back: Abort Install ends it
            log.append('room message seen: the drawer page came a second time (page %d); Abort Install (id 91)' % shots)
            click(gads[91][0])
            time.sleep(5)
            continue
        ask = own_ask_answer(gads)
        if (ask is None and '(100% done)' in (title or '') and installer_name(title) == outer
                and (not clicked_at or time.time() - clicked_at > 2)):
            # the last pages: P_END's Reboot question must never be clicked through to its default. Only the
            # outer Installer asks it: a nested Install-AmiSSL's last page ("Please reboot for modifications to
            # take effect", Proceed only) is a message, answered with Proceed (rig 9 2026-10-28)
            txt = window_ocr(tree, case + '-ask')
            log.append('last page text: %s' % ' '.join(txt.split()))
            later = reboot_later(gads, txt)
            if later == 0:
                log.append('reboot page [%s] without a Later gadget, ids %s: not clicked' % (title, sorted(gads)))
                dump(tree, 'reboot page')
                return False, log
            if later:
                click(gads[later][0])
                clicked_at = time.time()
                log.append('reboot offered: answered Later (id %d)' % later)
                wait_change(tree, 3)
                continue
        if ask is not None:
            if not clicked_at or time.time() - clicked_at > 15:   # a click made while the page redraws is lost
                # the last page asks to reboot (P_END): Later (id 1, the right button), or the rig's Amiga restarts
                if 'Reboot' in window_ocr(tree, case + '-ask'):
                    ask = 1
                    log.append('reboot offered: answered Later (id 1)')
                click(gads[ask][0])
                clicked_at = time.time()
                log.append('answered [%s] with id %d' % (title, ask))
            wait_change(tree, 3)
            continue
        if 90 not in gads:
            # copying or waiting for the next page: only Abort (or nothing) on it
            if m and m[-1] == last_pct and last_pct not in dumped_pct:
                dumped_pct.add(last_pct)
                dump(tree, 'work %s%%' % last_pct)
            work_since = work_since or time.time()
            if time.time() - work_since > work_limit:
                log.append('no Proceed page for %d s: %s' % (work_limit, last_sig))
                return False, log
            wait_change(tree, poll)
            continue
        work_since = None
        if clicked_at and time.time() - clicked_at > stall:
            log.append('STALL: page unchanged %d s after a click, ids %s' % (stall, sorted(gads)))
            dump(tree, 'stall')
            return False, log
        if clicked_at:
            if time.time() - (retried or clicked_at) > 15:   # a click made while the page still redraws is lost
                click(gads[90][0])
                retried = time.time()
                log.append('Proceed (id 90) again: page unchanged 15 s, at %.0f s' % (time.time() - t0))
            wait_change(tree, 3)
            continue
        target = dest or (room_dest if room else None)
        if target and not typed and 89 in gads and 92 in gads:   # askdir page
            # The string gadget only keeps a path that exists; a new drawer is made through Make New Drawer
            # (89), whose dialog takes the whole path in its string gadget 1 (OK 90, Cancel 91).
            typed = True
            click(gads[89][0])
            dlg = None
            for _ in range(10):
                time.sleep(1)
                t2 = ami.req(0x0D).decode('latin-1')
                dlg = installer_page(t2)[1]
                if 1 in dlg and 89 not in dlg:
                    break
            else:
                log.append('Make New Drawer dialog did not come up')
                dump(t2, 'no dialog')
                return False, log
            dump(t2, 'dialog')
            if not type_into(dlg[1][0], 1, target):
                log.append('could not type %s into the Make New Drawer dialog' % target)
                return False, log
            click(dlg[90][0])
            time.sleep(3)
            t2 = ami.req(0x0D).decode('latin-1')
            got = gadget_text(92)
            log.append('made %s through Make New Drawer, askdir string shows [%s]' % (target, got))
            dump(t2, 'after Make New Drawer')
            if got != target:
                return False, log
            last_sig = None   # the page is redrawn: log it and go on to Proceed
            continue
        if tick_all and 90 in gads and (sig, geom(tree), tree.count('selected')) not in ticked_sigs and 89 not in gads and not re.search(r'^G 1 \d+ \d+ \d+x\d+ button', tree, re.M):
            ticked_sigs.add((sig, geom(tree), tree.count('selected')))
            boxes = unticked_boxes(gads)
            for gid, _ in boxes:
                click(gads[gid][0])
                wait_change(tree, 1)
            if boxes:
                log.append('ticked ids %s on [%s]' % (' '.join(str(g) for g, _ in boxes), title))
                time.sleep(2)
                continue   # the page is read again: a box this one enabled is ticked too
        if 1 in gads and 5 in gads and title not in chose:   # install-mode page: Install for Real
            click(gads[1][0])
            chose.add(title)
            log.append('chose Install for Real (id 1)')
            time.sleep(1)
        if console and not ticked and 2 in gads and 3 in gads and 4 not in gads and 89 not in gads:
            txt = window_ocr(tree, case + '-options', min(g[0][1] for k, g in gads.items() if 2 <= k < 90) - 2)
            if 'console' in txt.lower():
                click(gads[2][0])
                ticked = True
                time.sleep(2)
                state = [l for l in ami.req(0x0D).decode('latin-1').splitlines() if l.startswith('G 2 ')]
                log.append('console page [%s]: ticked id 2, now [%s]' % (txt, state[0] if state else '?'))
                if not state or 'selected' not in state[0]:
                    return False, log
        after_ask = 89 in gads and 92 in gads
        if CHAIN:
            mapped = ['chain: ' + l for l in ir.run('VTC:inhandlers ADDR')[1].splitlines()]
            if OWNERMAP:
                addr = '' if OWNERMAP == '1' else OWNERMAP
                mapped += ['ownermap: ' + l for l in ir.run('VTC:ownermap ' + addr)[1].splitlines()]
            log.extend(mapped)
            # a hung guest never returns the log: keep every mapped on disk as it is taken
            with open(OUT / 'chain-live.txt', 'a') as f:
                f.write('---- before Proceed at %.0f s on [%s]\n' % (time.time() - t0, title) + '\n'.join(mapped) + '\n')
        click(gads[90][0])
        clicked_at = time.time()
        log.append('Proceed (id 90) at %.0f s' % (time.time() - t0))
        wait_change(tree, 3)
    log.append('the Installer did not finish in %d s' % timeout)
    return False, log


def dates():
    """{file: 'date time'} of everything in UP-Term: and of the files Install puts elsewhere."""
    out = ir.run_slow('dates', 'List UP-Term: ALL FILES LFORMAT "%P%N|%D %T"', 900)[1]
    out += ir.run('List C:vsh C:UPConsole L:vtcon-handler L:pty-handler LIBS:ixemul.library LIBS:ixnet.library '
                  'DEVS:DOSDrivers/XCON S:User-Startup LFORMAT "%P%N|%D %T"')[1]
    return dict(l.rsplit('|', 1) for l in out.splitlines() if '|' in l)


def opts_now():
    """ENVARC:up-term/opts as {name: contents}: what record.dos kept of the install's options."""
    names = ir.run('List ENVARC:up-term/opts LFORMAT "%N"')[1].split()
    return {n: ami.read_file('ENVARC:up-term/opts/' + n) for n in names}


def update_phase(ck, inst, level, base, t_install, noopts=False):
    """The update case's second Install: the kit with UPDATED changed, Update chosen (the default).
    noopts (UPDATE-DETECT): the recorded options are deleted first, as an install from a Shell leaves
    none; the Update reads them back from the Amiga and records the same ones again."""
    import subprocess
    before, startup = dates(), ir.run('Type S:User-Startup')[1]
    opts = ir.run('List ENVARC:up-term/opts LFORMAT "%N"')[1]
    optvals = opts_now()
    if noopts:
        ir.run('Delete >NIL: ENVARC:up-term/opts ALL QUIET')
        ck(ir.run('List >NIL: ENVARC:up-term/opts/UPTOPTDEST')[0] != 0, 'no recorded options before the Update')
    f = VTC / "distkit" / UPDATED
    f.write_bytes(f.read_bytes() + b"\n(installer_rig update case)\n")
    subprocess.run([sys.executable, str(ROOT / "tools/mkmanifest.py"), str(VTC / "distkit")], check=True)
    t = time.time()
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER %s DEFUSER %s NOLOG' % (inst, level, level), 30)
    time.sleep(5)
    finished, log = drive('update-2', inst, None, base)
    t = time.time() - t
    (OUT / "update-2.log.txt").write_text('\n'.join(log) + '\n')
    print('installer_rig update: full install %.0f s, update %.0f s' % (t_install, t))
    ck(finished, 'the Update ran to its end (%.0f s; the full install %.0f s)' % (t, t_install), log[-1] if log else '')
    ck(any(l.startswith('reboot offered') for l in log), 'the Update\'s last page offered a reboot (answered Later)')
    after = dates()
    changed = sorted(k for k in set(before) | set(after)
                     if before.get(k) != after.get(k) and k.lower() != 'up-term:manifest')
    ck(changed == ['UP-Term:unifont/SOURCE.txt'], 'the Update copied only the changed file', ' '.join(changed[:8]))
    got = ami.read_file('UP-Term:' + UPDATED.split('/', 1)[1]) or b''
    ck(got.endswith(b"(installer_rig update case)\n"), 'the changed file is the new kit\'s')
    ck(ami.read_file('UP-Term:MANIFEST') == (VTC / "distkit/Files/MANIFEST").read_bytes(),
       'UP-Term:MANIFEST is the new kit\'s (the record part ran)')
    ck(ir.run('Type S:User-Startup')[1] == startup, 'S:User-Startup as it was')
    ck(ir.run('List ENVARC:up-term/opts LFORMAT "%N"')[1] == opts, 'the recorded options as they were', opts)
    if noopts:
        got = opts_now()
        ck(got == optvals, 'the options read back are the full install\'s', '%r != %r' % (got, optvals))
        shown = ' '.join(' '.join(l.split()) for l in log if l.startswith('page text:'))
        ck('read from this Amiga' in shown, 'the Update page says the choices were read back')
    lay = [l for l in log if l.startswith('LAYOUT')]
    ck(not lay, 'the Update\'s pages readable', '; '.join(lay[:4]))
    pct = [int(x) for l in log for x in re.findall(r'(\d+)% done', l)]
    ck(pct and max(pct) <= 100, 'the bar counts only the Update\'s parts (at most 100%%: %s)' % (max(pct) if pct else '-'))


SPACE_LEFT = 12 * 1024 * 1024    # what the space case leaves free on SYS:
FILLER = 'SYS:space-filler'


def sys_free_bytes():
    """The free bytes of SYS: as the rig's Info prints them (blocks of 512)."""
    out = ir.run('Info')[1]
    for line in out.splitlines():
        f = line.split()
        if len(f) > 5 and f[0].upper() == 'DH0:':
            return int(f[3]) * 512
    raise SystemExit('installer_rig: cannot read the free space of SYS: from Info: %r' % out)


def fill_sys():
    """A file on SYS: that leaves SPACE_LEFT free (the kit's default choices need more): written on the host in VTC: and
    copied, then the host copy removed."""
    ir.run('Delete >NIL: %s QUIET' % FILLER)
    n = sys_free_bytes() - SPACE_LEFT
    if n <= 0:
        return
    host = VTC / 'space-filler'
    with open(host, 'wb') as f:
        f.truncate(n)
    try:
        ir.run_slow('fillsys', 'Copy VTC:space-filler %s' % FILLER, 900)
    finally:
        host.unlink(missing_ok=True)
    print('installer_rig space: SYS: has %d KB free after the filler' % (sys_free_bytes() // 1024))


def big_drawer():
    """The drawer the Installer offers as its default when no earlier install's assign exists: what the kit's UPBigVol
    leaves in ENV:UPTBIGVOL (the variable is deleted again, so a stale value cannot stand in for the Installer's own
    run of the helper), SYS:UP-Term when no volume qualifies."""
    ir.run('Delete >NIL: ENV:UPTBIGVOL QUIET')
    ir.run('VTC:distkit/Files/UPBigVol', 60)
    rc, out = ir.run('Type ENV:UPTBIGVOL')
    ir.run('Delete >NIL: ENV:UPTBIGVOL QUIET')
    return out.strip() if rc == 0 and out.strip() else 'SYS:UP-Term'


def run_bigvol():
    """BIGVOL, through the real Installer: click Proceed to the drawer page (gadgets 89 and 92), read the string
    gadget 92 and Abort. PASS: it shows the drawer big_drawer() reads from ENV:UPTBIGVOL, the volume UPBigVol LIST marks with `*`, not SYS:UP-Term
    (what the Installer offered before this case existed), and that volume has more free room than SYS:."""
    ck = Checks()
    ir.prepare(None)
    # the case is a first install: an UP-Term: assign (rig.py install leaves one) is an earlier install, and
    # P_ASKDEST offers that drawer before the biggest volume. Fixtures.restore puts the assign back.
    ir.run('Assign >NIL: UP-Term:')
    inst = find_installer()
    if not inst:
        return False, 'no Installer on the rig'
    listing = ir.run('VTC:distkit/Files/UPBigVol LIST', 60)[1]
    best = [l for l in listing.splitlines() if l.startswith('* ')]
    expect = big_drawer()
    ck(len(best) == 1 and best[0][2:].startswith(expect.split(':')[0] + ':'), 'UPBigVol LIST marks %s' % expect, listing)
    ck(not expect.upper().startswith('SYS:'), 'the biggest local disk on the rig is not SYS:', expect)
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER AVERAGE DEFUSER AVERAGE NOLOG' % inst, 30)
    time.sleep(5)
    probe = []
    ok, log = drive('bigvol', inst, None, set(), timeout=300, askdir_probe=probe)
    got = probe[0] if probe else None
    ck(got == expect, 'the askdir string (gadget 92) shows %s' % expect, 'shows [%s]' % got)
    ck(got != 'SYS:UP-Term', 'the default is not SYS:UP-Term')
    print('installer_rig bigvol: askdir default [%s]; UPBigVol LIST:\n%s' % (got, listing))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def run_case(case):
    if case == "space":
        try:
            fill_sys()
            return run_base_case(case)
        finally:
            ir.run_slow('unfillsys', 'Delete >NIL: %s QUIET' % FILLER, 300)
            ir.run_slow('deletedest', drawer_delete(CASES[case][1]))   # the empty drawer Make New Drawer made on the way to the room message
    if case.startswith("addon-"):
        return run_addon_case(case[len("addon-"):])
    if case == "migrate":
        return run_migrate()
    if case == "fullsuite":
        return run_fullsuite()
    if case == "kingcon":
        return run_kingcon()
    if case == "bigvol":
        return run_bigvol()
    return run_base_case(case)


FOREIGN = 'AHI-Handler'   # L:AHI-Handler: the audio handler, only started when someone opens CON:, which nothing here does


def run_kingcon():
    """c51c7fd6: a CON: that another handler serves (KingCON's way: Assign CON: RAW: DISMOUNT, then a Mount of
    CON: with a Handler) makes `C:UPConsole CON ON` refuse. The Installer's CONSOLE option must then say
    so on its last page, write no console boot block, take out an older Install's one, and leave no
    ENV:UPTREFUSED*. The rig is rebooted at the end: CON: is the ROM's again."""
    ck = Checks()
    ir.prepare(None)
    old = os.environ.get('UPTERM_INSTALL_DOS')   # fail-before proof: the install.dos of c51c7fd6^ in the kit copy
    if old:
        import shutil
        shutil.copyfile(old, VTC / 'distkit/Files/install.dos')
    ir.run_long('rununinstall')
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote ENV:UPTREFUSED#? QUIET')
    ir.run_slow('deleteroom', drawer_delete(ROOMDEST))
    startup_before = ir.run('Type S:User-Startup')[1]
    # an older Install's console block (the A1200 has one): the refused switch must take it out too (Uninstall
    # at the end takes every UP-Term block, so startup_before still holds after it)
    for l in (';BEGIN UP-Term console', 'C:UPConsole >NIL: CON ON', ';END UP-Term console'):
        ir.run('Echo >>S:User-Startup "%s"' % l)
    inst = find_installer()
    if not inst:
        return False, 'no Installer on the rig (Which Installer, SYS:System, SYS:Utilities, C:)'
    (VTC / 'foreigncon.ml').write_text('CON:\n  Handler = L:%s\n  StackSize = 4000\n  Priority = 5\n  GlobVec = -1\n#\n' % FOREIGN)
    (VTC / 'foreigncon').write_text('Assign CON: RAW: DISMOUNT\nMount CON: FROM VTC:foreigncon.ml\n')
    try:
        rc, out = ir.run('Execute VTC:foreigncon')
        rc, out = ir.run('VTC:UPConsole STATUS')
        log = ['CON: after the mount: %s' % ' '.join(out.split())[:200]]
        ck('served by' in out or 'AHI' in out.upper(), 'CON: is served by a foreign handler (%s)' % FOREIGN, out)
        base = {t for t, _ in windows(ami.req(0x0D).decode('latin-1'))}
        ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER AVERAGE DEFUSER AVERAGE NOLOG' % inst, 30)
        time.sleep(5)
        finished, dlog = drive('kingcon', inst, None, base, console=True)
        log += dlog
        (OUT / 'kingcon.log.txt').write_text('\n'.join(log) + '\n')
        ck(finished, 'the Installer ran to its end', log[-1])
        if not finished and installer_running(inst):
            ir.run('Break >NIL: `Status COM "%s"` C' % inst)
        # the last pages: the text is drawn, so it is read from the screen at the end of the run
        texts = [l[len('page text: '):] for l in dlog if l.startswith('page text: ')]
        rc, out = ir.run('Type S:User-Startup')
        ck(';BEGIN UP-Term console' not in out, '(a) S:User-Startup has no ;BEGIN UP-Term console block', out[-200:])
        ck(ir.run('List >NIL: ENV:UPTREFUSEDCON')[0] != 0 and ir.run('List >NIL: ENV:UPTREFUSEDDEV')[0] != 0,
           '(c) ENV:UPTREFUSED#? is gone at the end')
        ck(any(re.search(r'did\s+NOT\s+take\s+over\s+CON', t) for t in texts), '(b) the Installer\'s last pages say "did NOT take over CON:"',
           ' | '.join(texts[-2:])[-300:])
        blocks = [t.split('Startup blocks:', 1)[1] for t in texts if 'Startup blocks:' in t]
        ck(blocks and 'CON:' not in blocks[-1], '(d) "CON:" is not in the "Startup blocks:" line (and the line was read: %s)' % (blocks[-1][:80] if blocks else '-'),
           ' | '.join(blocks[-1:]))
    finally:
        ir.run('Delete >NIL: VTC:foreigncon VTC:foreigncon.ml QUIET')
    rc, out = ir.run_long('rununinstall')
    ir.run_slow('deleteroom', drawer_delete(ROOMDEST))
    ck(ir.run('Type S:User-Startup')[1] == startup_before, 'after Uninstall: S:User-Startup as it was, byte for byte')
    # CON: is the foreign handler's until the next boot: every write is done (Uninstall returned), 5 s of quiet, then reboot
    time.sleep(5)
    rig("stop")
    rig_start()
    ir._shown = None
    ck(ir.run('VTC:UPConsole STATUS')[0] in (0, 5), 'the rig was rebooted: CON: is the ROM\'s again')
    print('installer_rig kingcon: passed %d of %d' % (ck.total - len(ck.failed), ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


ROOM_CASES = ()   # the default drawer is on the biggest disk (VTCX: on the rig), so no case meets the room page by default; `space` meets it on SYS:


def amarchy_window():
    return any(t == 'Amarchy' for t, _ in windows(ami.req(0x0D).decode('latin-1')))


class DrawerAsks:
    """How often the drawer page (ids 89 Make New Drawer and 92 the path string) came, counted when it arrives from
    another page. A redraw of the page itself (the path typed through Make New Drawer, then the string updated) is the
    same ask: counting every redraw stopped a typed destination with "room page repeats" (master build, 2026-10-27)."""
    def __init__(self):
        self.n, self.on = 0, False

    def see(self, gads):
        on = 89 in gads and 92 in gads
        self.n += on and not self.on
        self.on = on
        return self.n


def reboot_amarchy_check(ck, log):
    """F1: after the install the rig is rebooted (once the Installer's writes are quiet); the startup block must
    have started Amarchy: its ARexx port answers STATUS, and Left Amiga + Space opens its menu (an XCON: window
    titled Amarchy)."""
    import tabstate
    time.sleep(10)   # the Installer's last writes and the volume's flush, never a reset mid-write
    rig("reboot")    # DH0: kept: a stop + start clones it anew from the master and the install is gone
    ir._shown = None
    time.sleep(5)
    m = ami._m()
    try:
        with ami._conn() as conn:
            st = m.arexx(conn, 'AMARCHY', 'STATUS')[2].strip()
    except m.WasabiError:
        st = ''
    ck(bool(st), 'after the reboot: the AMARCHY ARexx port answers STATUS', st or 'no port')
    print('amarchy STATUS after reboot: %s' % st)
    tabstate.open_host('f1host')
    # over the f1host terminal Left Amiga + Space opens amarchy-menu in the terminal while menu-in-terminal = text
    # (the default); the drawn menu, titled Amarchy, needs graphical (FRESH-ALL-AFTER-END 32, as amarchy_window_rig)
    saved = amw.drawn_menu_prefs()
    try:
        ami.key(0x40, 0x40)   # Left Amiga + Space
        t0 = time.time()
        while time.time() - t0 < 20 and not amarchy_window():
            time.sleep(0.5)
        up = amarchy_window()
        ck(up, 'after the reboot: Left Amiga + Space opens the Amarchy menu window', 'no window titled Amarchy in 20 s')
        print('menu window after reboot: %s' % up)
        if not up:   # amarchy_window reads the front screen only: say where every window is
            with ami._conn() as conn:
                for s in ami._m().list_windows(conn)[0]:
                    print('screen [%s]: %s' % (s.get('title'), ' | '.join(w['title'] for w in s['windows'])))
        ami.key(0x45)         # Esc at the root leaves the menu
        time.sleep(3)
    finally:
        amw.restore_prefs(saved)
    ir.stock_libs()       # the boot put VTC:ixp6 first in LIBS: again; the Uninstall checks read the system's


def run_base_case(case):
    level, dest = CASES[case]
    ck = Checks()
    ir.prepare(dest)
    drawer = dest or big_drawer()   # the Installer's own default (UPBigVol), not SYS:
    sys_had = ir.run('List >NIL: SYS:UP-Term')[0] == 0   # the rig's baseline assign UP-Term: may name an empty SYS:UP-Term: leave it, and test only what this run adds
    ir.run_long('rununinstall')  # a run that stopped half-way left things behind
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote QUIET')
    parent_existed = True
    if dest:
        parent_existed = make_dest_parent(dest)   # the new drawer's parent must exist
        ir.run_slow('deletedest', drawer_delete(dest))
    if case in ROOM_CASES:
        ir.run_slow('deleteroom', drawer_delete(ROOMDEST))
    sys_kb = sys_free_bytes() // 1024   # what the room page must say SYS: has
    startup_before = ir.run('Type S:User-Startup')[1]
    before = ir.lib_state()
    rig_gg = ir.run('Assign >NIL: GG: EXISTS')[0] == 0
    if rig_gg:
        ir.run('Assign GG:')   # as install_rig: Install's own GG: is what gets tested
    inst = find_installer()
    if not inst:
        return False, 'no Installer on the rig (Which Installer, SYS:System, SYS:Utilities, C:)'
    base = {t for t, _ in windows(ami.req(0x0D).decode('latin-1'))}
    # the icon: default tool Installer, tooltypes APPNAME=UP-Term MINUSER=AVERAGE DEFUSER=AVERAGE (Makefile dist)
    t_install = time.time()
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER %s DEFUSER %s NOLOG' % (inst, level, level), 30)
    time.sleep(5)
    finished, log = drive(case, inst, dest, base, expect_abort=(case == 'space'), room_dest=ROOMDEST if case in ROOM_CASES else None, tick_all=(case == 'fresh-all'))
    t_install = time.time() - t_install
    (OUT / (case + ".log.txt")).write_text('\n'.join(log) + '\n')
    ck(finished, 'the Installer ran to its end (%s)' % level, log[-1] if log else '')
    if case != 'space':
        ck(any(l.startswith('reboot offered') for l in log), 'the last page offered a reboot (answered Later)')
    room = [l for l in log if l.startswith('room page: ')]
    if room:
        # SYS: has too little room for the default parts: the user takes another drive (ROOMDEST) and the install goes there
        n_kb, m_kb = map(int, re.findall(r'\d+', room[0]))
        ck(n_kb == sys_kb, 'the room page says SYS: has %d KB free (Info: %d KB)' % (n_kb, sys_kb))
        ck(n_kb < m_kb and (case == 'fresh-all' or 60000 <= m_kb <= 110000), 'the room page\'s need (%d KB) is plausible for the default parts (60000 to 110000 KB, above the free room)' % m_kb)
        drawer = ROOMDEST
        ck(any(l.startswith('made %s through Make New Drawer' % ROOMDEST) for l in log), 'the drawer on the other drive was asked for')
    lay = [l for l in log if l.startswith('LAYOUT')]
    # the room message is the Installer's own abort dialog drawn over the page, not one of our pages
    ck(not lay or case == 'space', 'every page\'s text readable: no line cut by the border, under the buttons or beside the copy panel',
       '; '.join(lay[:4]))
    if not finished:
        if installer_running(inst):
            ir.run('Break >NIL: `Status COM "%s"` C' % inst)
        return False, '; '.join(ck.failed + log[-3:])
    if case == 'space':
        # the room check: Install on a volume with too little room says so and changes nothing
        ck(any(l.startswith('room message seen') for l in log), 'the room message came up and led back to the drawer page', '; '.join(log[-3:]))
        ck(not any('% done' in l for l in log), 'the Installer ended without a copy phase', '; '.join(log[-3:]))
        ck(ir.run('List >NIL: SYS:UP-Term/VERSIONS')[0] != 0, 'nothing was copied into the drawer', '')
        ck(ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'no UP-Term: assign', '')
        ck(ir.run('Type S:User-Startup')[1] == startup_before, 'S:User-Startup as it was, byte for byte', '')
        st = ir.lib_state()
        ck(st == before, 'LIBS: as it was', 'now %s, before %s' % (st, before))
        print('installer_rig %s: passed %d of %d' % (case, ck.total - len(ck.failed), ck.total))
        return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total
    # what the Installer's install.dos parts and copyfiles put there
    rc, out = ir.run('Assign LIST')
    al = [l for l in out.splitlines() if l.lower().startswith('up-term')]
    ck(al and al[0].split(None, 1)[-1].strip().lower().endswith(drawer.split(':', 1)[1].lower()),
       'UP-Term: is assigned to %s' % drawer, out)
    rc, out = ir.run('Type ENVARC:up-term/Dir')
    ck(rc == 0 and out.strip().lower() == drawer.lower(), 'ENVARC:up-term/Dir names %s' % drawer, out)
    rc, out = ir.run('Type S:User-Startup')
    ck(';BEGIN UP-Term assign' in out and ';END UP-Term assign' in out, 'the UP-Term: block is marked', out[-300:])
    # an AmigaDOS $name ends at the first non-alphanumeric: $o_dest went in
    # literally and a Replay's boot failed on it (2026-10-07)
    added = [l for l in out.splitlines() if l not in startup_before.splitlines()]
    ck(not any(re.search(r'\$[A-Za-z0-9_]', l) for l in added),
       'S:User-Startup gets no unexpanded $variable', '\n'.join(l for l in added if '$' in l))
    ck(('Assign UP-Term: "%s"' % drawer).lower() in out.lower(), 'S:User-Startup assigns UP-Term: to %s' % drawer, out[-300:])
    ck(out.find('Assign UP-Term:') >= 0 and (out.find('Assign GG:') < 0 or out.find('Assign UP-Term:') < out.find('Assign GG:')),
       'the UP-Term: block comes before the blocks that use it', out[-300:])
    if drawer.upper() != 'SYS:UP-TERM':
        ck(ir.run('List >NIL: SYS:UP-Term/VERSIONS')[0] != 0 if sys_had else ir.run('List >NIL: SYS:UP-Term')[0] != 0, 'a drawer on another disk leaves SYS:UP-Term unmade')
    for f in ('"%s/VERSIONS"' % drawer, 'UP-Term:bin/sh', 'UP-Term:bin/ls', 'UP-Term:unifont/00', 'UP-Term:emoji/1F6',
              'C:vsh', 'C:tmux', 'C:UPConsole', 'L:vtcon-handler', 'L:pty-handler', 'DEVS:DOSDrivers/XCON',
              'SYS:System/UP-Term.info', 'SYS:Prefs/UP-Term-Prefs.info', 'ENVARC:up-term/unstartup.sh',
              'UP-Term:MANIFEST', 'ENVARC:up-term/opts/UPTOPTDEST'):
        ck(ir.run('List >NIL: %s' % f)[0] == 0, '%s is there' % f)
    st = ir.lib_state()
    ck(st.get('ixemul.library.orig') == before.get('ixemul.library'), 'the original ixemul kept as .orig', str(st))
    ck(ir.run('Search >NIL: LIBS:ixemul.library %s' % ir.kit_ixemul_tag())[0] == 0, 'the patched ixemul is in LIBS:')
    ck(ir.run('Assign >NIL: PTY: EXISTS DEVICES')[0] == 0, 'PTY: is mounted')
    rc, out = ir.run('C:UPConsole STATUS')
    ck('kit:' in out, 'UPConsole STATUS prints the kit line (UP-Term:VERSIONS)', out)
    if case == 'fresh-all':
        reboot_amarchy_check(ck, log)
        if KEEP_INSTALLED:
            print('installer_rig fresh-all: INSTALLER_KEEP set, the kit stays installed (no Uninstall, no fixture restore)')
            return not ck.failed, '; '.join(ck.failed) or 'all %d checks (kit kept)' % ck.total
    if case in ('update', 'update-noopts'):
        update_phase(ck, inst, level, base, t_install, noopts=case == 'update-noopts')
    # a running C:wasabid keeps running through Uninstall, the kit's or the
    # owner's: it may be the machine's only control channel, and an Uninstall
    # run over wasabi died with it (the A1200, 2026-10-17). The rig's own
    # wasabid runs as boot/wasabid, so a C:Wait copy stands in when no
    # C:wasabid runs. The kit's C:wasabid file (its marker) goes all the same.
    kit_wasabid = ir.run('List >NIL: ENVARC:up-term/wasabid')[0] == 0
    stand_in = ir.run('Status >NIL: COM C:wasabid')[0] != 0
    if stand_in:
        if ir.run('List >NIL: C:wasabid')[0] != 0:
            ir.run('Copy >NIL: C:Wait C:wasabid')
        ir.run('Run >NIL: C:wasabid 600')
    # Uninstall gives it all back
    rc, out = ir.run_long('rununinstall')
    ck(rc == 0, 'Uninstall runs', out)
    ck(ir.run('Status >NIL: COM C:wasabid')[0] == 0, 'after Uninstall: the running C:wasabid still runs')
    if kit_wasabid:
        ck(ir.run('List >NIL: C:wasabid')[0] != 0, "after Uninstall: the kit's C:wasabid file is gone")
    ir.run('Status >ENV:upw COM C:wasabid')
    ir.run('Break $upw C')
    if stand_in and not kit_wasabid:
        ir.run('Delete >NIL: C:wasabid QUIET')
    ir.run('Delete >NIL: ENV:upw QUIET')
    rc, out = ir.run('Type S:User-Startup')
    ck(out == startup_before, 'after Uninstall: S:User-Startup as it was, byte for byte',
       'differs: %r vs %r' % (out[-80:], startup_before[-80:]))
    ck(ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'after Uninstall: no UP-Term: assign')
    ck(ir.run('List >NIL: "%s"' % drawer)[0] != 0, 'after Uninstall: %s is gone' % drawer)
    st = ir.lib_state()
    ck(st.get('ixemul.library') == before.get('ixemul.library') and 'ixemul.library.orig' not in st,
       'after Uninstall: the original ixemul back, no .orig', 'now %s, before %s' % (st, before))
    ck(st.get('ixnet.library') == before.get('ixnet.library') and 'ixnet.library.orig' not in st,
       'after Uninstall: the original ixnet back, no .orig', 'now %s, before %s' % (st, before))
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote QUIET')
    if rig_gg:
        ir.run('Assign GG: VTC:gg')
    ir.run('Avail >NIL: FLUSH')
    for line in ir.rig_libs():
        ir.run(line)
    if room:
        ir.run_slow('deleteroom', drawer_delete(ROOMDEST))   # Uninstall took the drawer; its icon in the volume's root goes too
    if dest:
        for cmd in dest_cleanup(dest, parent_existed)[1:]:   # the drawer went with Uninstall; the parent we made goes too
            ir.run_slow('deletedest', cmd)
    print('installer_rig %s: passed %d of %d' % (case, ck.total - len(ck.failed), ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def manifest_entries(text):
    """{kit path: (size, crc)} of the files in a MANIFEST text (its install.dos#, copy# and handed# lines left out)."""
    out = {}
    for line in text.splitlines():
        f = line.split(None, 3)
        if len(f) == 4 and "#" not in f[3]:
            out[f[3]] = (f[1], f[2])
    return out


def unchanged_file(old, new, prefix):
    """A kit path under `prefix` that both manifests list with the same size and CRC, a text file by preference:
    an Update leaves it alone, a full Install copies it again. None when there is none."""
    both = sorted(p for p in new if p.startswith(prefix) and old.get(p) == new[p] and new[p][0] != "0")
    texty = [p for p in both if p.lower().endswith((".txt", ".md", ".vim", ".lua"))]
    return (texty or both or [None])[0]


def changed_files(old, new, prefix):
    """The kit paths under `prefix` an Update copies: new, or listed with another size or CRC."""
    return sorted(p for p in new if p.startswith(prefix) and old.get(p) != new[p])


def asked_drawer(log):
    """True when the Installer showed the drawer page (the askdir string, id 92): a full Install. An Update asks nothing."""
    return any(l.startswith("page [") and re.search(r"\bids\b.*\b92\b", l) for l in log)


def installer_run(tag, inst, kitdir, appname, level, base, dest=None):
    """Start the kit's Install icon (VTC:<kitdir>/Install) in the real Installer and answer its pages:
    (finished, log, seconds)."""
    t = time.time()
    ir.run('Run >NIL: "%s" VTC:%s/Install APPNAME %s MINUSER %s DEFUSER %s NOLOG' % (inst, kitdir, appname, level, level), 30)
    time.sleep(5)
    finished, log = drive(tag, inst, dest, base, tick_all=(tag == "fullsuite" or (tag.startswith("addon-") and not tag.endswith("-base"))))
    (OUT / (tag + ".log.txt")).write_text("\n".join(log) + "\n")
    return finished, log, time.time() - t


def installer_done(ck, inst, tag, finished, log, readable=True):
    """The checks every Installer run has: it ran to its end, its pages are readable. False when it did not finish
    (the Installer is then ended, the case stops)."""
    ck(finished, "%s: the Installer ran to its end" % tag, log[-1] if log else "")
    if readable:
        lay = [l for l in log if l.startswith("LAYOUT")]
        ck(not lay, "%s: every page\'s text readable" % tag, "; ".join(lay[:4]))
    if not finished and installer_running(inst):
        ir.run("Break >NIL: `Status COM \"%s\"` C" % inst)
    return finished


def read_installer():
    inst = find_installer()
    if not inst:
        raise SystemExit("installer_rig: no Installer on the rig (Which Installer, SYS:System, SYS:Utilities, C:)")
    return inst, {t for t, _ in windows(ami.req(0x0D).decode("latin-1"))}


def run_addon_case(pkg):
    """The base by its Installer, then the add-on's own Install icon, both with every page answered by its default.
    On a 68000 rig a stand-alone add-on runs alone (run_addon_68000)."""
    if "--a500" in RIG_FLAGS and pkg in ir.standalone_ids():
        return run_addon_68000(pkg)
    case, ck = "addon-" + pkg, Checks()
    ir.prepare(None, [pkg])
    before = ir.Before()
    ir.plant(pkg)
    inst, base = read_installer()
    ok = installer_done(ck, inst, case + " base", *installer_run(case + "-base", inst, "distkit", "UP-Term", "AVERAGE", base)[:2])
    if ok:
        ck(ir.run("List >NIL: UP-Term:UP-Term.version")[0] == 0, "the base Installer wrote UP-Term:UP-Term.version")
        ir.addon_absent(ck, pkg, "before its Install")
        if pkg == "claude":
            ir.plant_old_claude()
        ok = installer_done(ck, inst, case, *installer_run(case, inst, "kit-" + pkg, ir.archive(pkg), "AVERAGE", base)[:2])
    if ok:
        ir.addon_verify(ck, pkg, remote=False)
        ir.base_whole(ck, "with %s installed" % pkg)
        rc, out = ir.run_long("rununinstall-" + pkg)
        ck(rc == 0, "%s: its Uninstall runs" % pkg, out)
        ir.addon_gone(ck, pkg, remote=False)
        ir.base_whole(ck, "after %s\'s Uninstall" % pkg)
    if not ok:
        ir.run_long("rununinstall")
    else:
        # DOpus stays after Uninstall (owner rule): its own ;BEGIN DOpus5 block stays in S:User-Startup
        ir.base_uninstall(ck, before, leaves=ir.startup_leaves([pkg]))
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


def run_addon_68000(pkg):
    """A stand-alone add-on (needs-base "-") on the 68000 rig (--a500, plan D10): the base Installer refuses the 68000
    by design (distkit Install: "UP-Term needs a 68020 or better"), so the add-on's own Install icon runs alone, on a
    Workbench with no UP-Term: assign, as on a real 68000. Its Install picks the 68000 build (ENV:UPTOPTCPU68000);
    for dopus the installed DOpus5:DirectoryOpus must be the os3-68000 one by md5. Then its Uninstall, and
    S:User-Startup as before apart from the blocks the package leaves (DOpus stays, owner rule). Before: the base ran
    first, aborted, and the case failed on "a system requester came up" (RIG-A500-KIT-BOOT, 2026-10-29)."""
    case, ck = "addon-" + pkg, Checks()
    ir.prepare(None, [pkg])
    before = ir.Before()
    ir.plant(pkg)
    ir.run("Assign UP-Term:")
    ck(ir.run("Assign >NIL: UP-Term: EXISTS")[0] != 0, "%s: there is no UP-Term: assign" % case)
    ir.addon_absent(ck, pkg, "before its Install")
    inst, base = read_installer()
    ok = installer_done(ck, inst, case, *installer_run(case, inst, "kit-" + pkg, ir.archive(pkg), "AVERAGE", base)[:2])
    if ok:
        ir.addon_verify(ck, pkg, remote=False, with_base=False)
        if pkg == "dopus":
            dopus_build_check(ck, "os3-68000")
        rc, out = ir.run_long("rununinstall-" + pkg)
        ck(rc == 0, "%s: its Uninstall runs" % pkg, out)
        ir.addon_gone(ck, pkg, remote=False, with_base=False)
        ir.check_startup_after_uninstall(ck, before.startup, ir.run("Type S:User-Startup")[1], ir.startup_leaves([pkg]), case)
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


def dopus_build_check(ck, build):
    """DOpus5:DirectoryOpus equals the kit's Files/<build>/DOpus5/DirectoryOpus by md5 (read back through VTC:)."""
    import hashlib
    want = hashlib.md5((ir.kit_dir("dopus") / "Files" / build / "DOpus5/DirectoryOpus").read_bytes()).hexdigest()
    back = ir.VTC / "dopus-build-check.tmp"
    back.unlink(missing_ok=True)
    ir.run("Copy DOpus5:DirectoryOpus VTC:dopus-build-check.tmp CLONE")
    got = hashlib.md5(back.read_bytes()).hexdigest() if back.exists() else "missing"
    back.unlink(missing_ok=True)
    ck(got == want, "dopus: DOpus5:DirectoryOpus is the %s build (md5 %s)" % (build, want), "md5 " + got)


def run_fullsuite():
    """The FULL package (L6): the base kit drawer with all the add-ons beside it (FULL_DRAWER as an unpacked
    drawer, the others as .lha files, so both ways of finding them run). Every box on every page of the base and of
    each nested Install is ticked and every question answered yes (drive tick_all); the drawer for UP-Term: is
    FULLDEST on DH1:, never SYS:. PASS: each add-on's record, the base record, CON: served by UP-Term with its
    startup block, the lha files unpacked beside themselves, no "Add-on not installed" on the last page."""
    import subprocess
    case, ck = "fullsuite", Checks()
    ids = ir.addon_ids()
    ir.prepare(None, [])
    for pkg in ids:   # no stale drawer or lha beside the kit: what is found is what this run staged
        shutil.rmtree(ir.VTC / ir.archive(pkg), ignore_errors=True)
        (ir.VTC / (ir.archive(pkg) + ".lha")).unlink(missing_ok=True)
        if pkg == FULL_DRAWER:
            shutil.copytree(ir.kit_dir(pkg), ir.VTC / ir.archive(pkg))
        else:
            lha = ir.VTC / (ir.archive(pkg) + ".lha")
            subprocess.run(["lha", "-aq", str(lha), ir.archive(pkg)], cwd=ir.kit_dir(pkg).parent, check=True)
    ir.run_slow("freeroom", "Delete >NIL: SYS:space-filler QUIET")
    ir.run_slow("deleteroom", drawer_delete(FULLDEST))
    before = ir.Before()
    inst, base = read_installer()
    for pkg in ids:
        ir.addon_absent(ck, pkg, "before the base Install")   # a record left over would pass the checks below untested
    finished, log, _ = installer_run(case, inst, "distkit", "UP-Term", "AVERAGE", base, dest=FULLDEST)
    ok = installer_done(ck, inst, case, finished, log)
    pages = (OUT / (case + ".pages.txt")).read_text(errors="replace")
    ck(any(("made %s through Make New Drawer" % FULLDEST) in l for l in log), "the drawer typed was %s (DH1:, not SYS:)" % FULLDEST, "; ".join(log[:14]))
    ck(not any("SYS:" in l and "Make New Drawer" in l for l in log), "no drawer was made on SYS:")
    if ok:
        ck(ir.run("List >NIL: UP-Term:MANIFEST")[0] == 0, "the base record UP-Term:MANIFEST exists")
        ck(ir.run("Assign UP-Term: EXISTS")[0] == 0 and ir.run("List >NIL: %s/UP-Term.version" % FULLDEST)[0] == 0, "UP-Term: is on %s" % FULLDEST)
        for pkg in ids:
            arc = ir.archive(pkg)
            ck(ir.run("List >NIL: VTC:%s/Install" % arc)[0] == 0, "%s: its Install is beside the kit (%s)" % (pkg, "drawer" if pkg == FULL_DRAWER else "lha unpacked"))
            ck(any("VTC:%s/Install" % arc in l for l in log), "%s: a nested Installer window ran its script" % pkg, "; ".join(l for l in log if "Installer window" in l))
            ir.addon_verify(ck, pkg, remote=False)
        ck(ir.run("C:UPConsole STATUS")[1].find("CON: UP-Term") >= 0, "CON: is served by UP-Term (the console handler replaced)", ir.run("C:UPConsole STATUS")[1] + " | refused: " + ir.run("Type ENV:UPTREFUSEDCON")[1] + " | startup: " + ir.run("Type S:User-Startup")[1][-600:])
        ck(";BEGIN UP-Term console" in ir.run("Type S:User-Startup")[1], "S:User-Startup has the UP-Term console block")
        ck("Add-on not installed" not in pages, "the last page has no Add-on not installed note")
        ir.base_whole(ck, "with every add-on installed")
        for pkg in reversed(ids):
            (ir.VTC / ("rununinstall-" + pkg)).write_text("Set upvardelete YES\nCD VTC:%s\nExecute Uninstall\n" % ir.archive(pkg))
            rc, out = ir.run_long("rununinstall-" + pkg)
            ck(rc == 0, "%s: its Uninstall runs" % pkg, out)
        ir.base_uninstall(ck, before)
    else:
        ir.run_long("rununinstall")
    for pkg in ids:
        ir.run_slow("deleteunpacked", "Delete VTC:%s ALL QUIET" % ir.archive(pkg))
        ir.run_slow("deletelha", "Delete VTC:%s.lha QUIET" % ir.archive(pkg))
    ir.run_slow("deleteroom", drawer_delete(FULLDEST))
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


def plant_edit(path):
    """Append a line to an installed file; (it survives an Update that leaves the file alone, a full Install overwrites it)."""
    ir.run("Echo >>%s \"installer_rig planted\"" % path)


def edit_survived(path):
    return ir.run("Type %s" % path)[1].rstrip().endswith("installer_rig planted")


def run_migrate():
    """The single kit's Installer, then the new base over it, then the nvim add-on: the Update path both times."""
    case, ck = "migrate", Checks()
    ir.prepare(None, ["nvim"])
    ir.prepare_legacy()
    try:
        return _migrate(case, ck)
    finally:
        ir.run_slow("deletelegacy", "Delete VTC:legacykit ALL QUIET")


def _migrate(case, ck):
    legacy = manifest_entries((ir.LEGACY_KIT / "Files/MANIFEST").read_text(errors="replace"))
    newbase = manifest_entries((ir.kit_dir("base") / "Files/MANIFEST").read_text(errors="replace"))
    newnvim = manifest_entries((ir.kit_dir("nvim") / "Files/MANIFEST").read_text(errors="replace"))
    p_base = unchanged_file(legacy, newbase, "Files/unifont/")
    p_nvim = unchanged_file(legacy, newnvim, "Files/nvim/")
    ck(p_base and p_nvim, "the single kit and the new kits share an unchanged file to plant an edit in", "%s %s" % (p_base, p_nvim))
    if not (p_base and p_nvim):
        return False, "; ".join(ck.failed)
    at = lambda p: "UP-Term:" + p.split("/", 1)[1]
    before = ir.Before()
    inst, base = read_installer()
    # The single kit with Python and Neovim ticked (its defaults) is 75 MB: more than the 3.1 rig's System volume
    # has free (65 MB), so Neovim never got copied and the Installer's last requester was clicked away. The drawer
    # goes to the host drawer VTC: as in the dest case; the new kits find it through the UP-Term: assign (Update).
    legacy_dest = "VTC:KitTest/UP-Term"
    apps_existed = make_dest_parent(legacy_dest)
    ir.run_slow("deletedest", drawer_delete(legacy_dest))
    if not installer_done(ck, inst, "migrate legacy", *installer_run("migrate-legacy", inst, "legacykit", "UP-Term", "AVERAGE", base, legacy_dest)[:2], readable=False):
        ir.run_long("rununinstall")
        for cmd in dest_cleanup(legacy_dest, apps_existed):
            ir.run_slow("deletedest", cmd)
        return False, "; ".join(ck.failed)
    for f in ("UP-Term:nvim/bin/nvim", "UP-Term:Python3/bin/python3", "C:Claude", "C:amiga-pi"):
        ck(ir.run("List >NIL: %s" % f)[0] == 0, "the single kit installed %s" % f)
    ck(ir.run("List >NIL: UP-Term:pkg")[0] != 0, "the single kit leaves no UP-Term:pkg")
    nvim_date = ir.run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip()
    plant_edit(at(p_base))
    # the new base over the single kit: the Installer offers Update (no drawer page), hands nvim over
    finished, log, _ = installer_run("migrate-base", inst, "distkit", "UP-Term", "AVERAGE", base)
    if installer_done(ck, inst, "migrate base", finished, log):
        ck(not asked_drawer(log), "the base Install over the single kit is the Update path (no drawer page)", "; ".join(log[:6]))
        ck(edit_survived(at(p_base)), "the Update left an unchanged file alone (%s kept the planted edit)" % at(p_base))
        ck(ir.run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip() == nvim_date, "the handed nvim keeps its date", nvim_date)
        rc, out = ir.run_slow("nvimverrun", "UP-Term:nvim/bin/nvim --version", 400)
        ck(rc == 0 and "NVIM v0.12" in out, "nvim still answers after the new base", out[-200:])
        for f in ("MANIFEST", "VERSIONS"):
            ck(ir.run("List >NIL: UP-Term:pkg/legacy/%s" % f)[0] == 0, "UP-Term:pkg/legacy/%s was kept" % f)
        ck(ami.read_file("UP-Term:MANIFEST") == (ir.kit_dir("base") / "Files/MANIFEST").read_bytes(), "UP-Term:MANIFEST is the new base\'s")
        ck(ir.run("List >NIL: UP-Term:UP-Term.version")[0] == 0, "the base wrote UP-Term:UP-Term.version")
        # the nvim add-on over the single kit's files: Update "installed by the single kit"
        plant_edit(at(p_nvim))
        finished, log, _ = installer_run("migrate-nvim", inst, "kit-nvim", ir.archive("nvim"), "AVERAGE", base)
        if installer_done(ck, inst, "migrate nvim", finished, log):
            ck(edit_survived(at(p_nvim)), "the add-on\'s Update left an unchanged file alone (%s kept the planted edit)" % at(p_nvim))
            changed = changed_files(legacy, newnvim, "Files/nvim/")[:20]
            if changed:
                sizes = [ir.run('List "%s" LFORMAT "%%L"' % at(p))[1].strip() for p in changed]
                want = [str(newnvim[p][0]) for p in changed]
                ck(sizes == want, "the files that differ are the add-on kit\'s now (%d)" % len(changed), "%s vs %s" % (sizes, want))
            ck(ir.run("List >NIL: UP-Term:pkg/up-nvim/MANIFEST")[0] == 0 and
               ami.read_file("UP-Term:pkg/up-nvim/MANIFEST") == (ir.kit_dir("nvim") / "Files/MANIFEST").read_bytes(),
               "the nvim add-on made its record (its MANIFEST)")
            rc, out = ir.run_slow("nvimverrun", "UP-Term:nvim/bin/nvim --version", 400)
            ck(rc == 0 and "NVIM v0.12" in out, "nvim answers after its add-on", out[-200:])
    ir.base_uninstall(ck, before)
    for f in ("C:Claude", "C:ClaudeCode", "C:amiga-pi", "UP-Term:nvim"):
        ck(ir.run("List >NIL: %s" % f)[0] != 0, "after the base Uninstall %s is gone (the legacy block)" % f)
    ck(ir.run("Assign >NIL: Python3: EXISTS")[0] != 0, "after the base Uninstall no Python3: assign")
    for cmd in dest_cleanup(legacy_dest, apps_existed):   # VTC:KitTest and VTC:KitTest.info, when this case made them
        ir.run_slow("deletedest", cmd)
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


PURPOSE = []   # ["--purpose", text] for every rig start (the FS-UAE window title)
RIG_FLAGS = []   # other rig.py start options from the command line (--no-dopus, --a500), kept apart from PURPOSE


def set_purpose(text):
    PURPOSE[:] = ["--purpose", text]


def rig_start():
    rig("start", *(PURPOSE + RIG_FLAGS))


def parse_args(argv):
    """(only, flags) from the command line. An unknown word or case stops the run before the rig is touched."""
    args = list(argv)
    only, flags = [], set()
    while args:
        a = args.pop(0)
        if a in ("ONLY", "--only"):
            if not args:
                raise SystemExit("installer_rig: %s needs a case name" % a)
            only.append(args.pop(0))
        elif a in ("FAILED", "FORCE"):
            flags.add(a)
        elif a == "--max":   # max is the rig default (rig.py): accepted so the rig.py line can be reused
            pass
        elif a in ("--no-dopus", "--a500"):   # without Directory Opus (the dopus add-on installs it); a 68000 A500
            if a not in RIG_FLAGS:
                RIG_FLAGS.append(a)
        elif a == "--purpose":
            if not args:
                raise SystemExit("installer_rig: --purpose needs a text")
            set_purpose(args.pop(0))
        else:   # an unknown word used to be ignored, and the run went on with every case
            raise SystemExit("installer_rig: unknown argument %r (ONLY <case>, FAILED, FORCE)" % a)
    unknown = [c for c in only if c not in CASES]
    if unknown:
        raise SystemExit("installer_rig: no case %s; cases: %s" % (" ".join(unknown), " ".join(CASES)))
    return only, flags


def main():
    only, flags = parse_args(sys.argv[1:])
    fx = ir.Fixtures(('ENVARC:up-term/Dir',))
    with paths.rig_lock():
        try:
            return _main(fx, only, flags)
        finally:
            if not KEEP_INSTALLED:
                fx.restore()   # a killed or failing run leaves nothing planted
            ir.show_close()


def _main(fx, only, flags):
    if CHAIN:
        shutil.copyfile(ROOT / 'build/amiga/inhandlers', VTC / 'inhandlers')
        if os.environ.get("INSTALLER_OWNERMAP"):
            shutil.copyfile(ROOT / 'build/amiga/ownermap', VTC / 'ownermap')
    # space fills SYS: with a file first (K7); it is not part of the default run
    names = [c for c in CASES if only or c != 'space']
    for case in (c for c in names if not only or c in only):
        for pkg in case_packages(case)[0]:
            if not (ir.kit_dir(pkg) / "Install").exists():
                raise SystemExit('no kit %s: make dist first' % ir.kit_dir(pkg))
    todo, skip = verdicts.select(STORE, names, fingerprint, only, "FAILED" in flags, "FORCE" in flags)
    for c, why in skip:
        print("[SKIP] %s: %s" % (c, why))
    booted = False
    for case, _ in todo:
        if not booted:
            rig("stop")
            rig_start()
            ir._shown = None   # a fresh boot: the watched window (UPTERM_RIG_SHOW) opens again
            fx.take()   # the user's files and assigns, before any case plants its own
            booted = True
        try:
            ok, detail = run_case(case)
        except SystemExit as e:  # the agent or the rig is gone: not a verdict on the kit
            print("[ABORT] %s: %s (no verdict written)" % (case, e))
            return 2
        record(case, ok, detail)
    bad = STORE.failed([c for c, _ in todo])
    print("installer_rig: %d of %d cases failed: %s" % (len(bad), len(todo), " ".join(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
