#!/usr/bin/env python3
"""upkg_wb_rig.py -- reachability run of the upkg Workbench window (plan UPKG-WB-UI, item R1).

The product's entry point is the icon: upkg started from Workbench with no command line (argc 0) opens its
window. Steps, each verdict printed as [OK] / [FAIL] and appended to <out>/report.txt as it completes:
  1  the m68k upkg and a file: feed are put on the rig (Assign TMP: T:, Assign UP-Term: VTCX:Apps, feed in
     RAM:feed, UP-Term:etc/upkg.conf with `src/gz upterm file:/RAM/feed`); `upkg update` from a Shell works.
  2  upkg is started from an icon through RAM:wbrun (build/amiga/wbrun): the sentinel line
     "window opened, N rows" with N > 0 must appear in UP-Term:log/upkg-wb.log (patches 0030/0031).
  3  the first row is clicked, then Install (gadget id 2 of the window "UP-Term Add-ons"); the package must
     appear in UP-Term:pkg/upkg/status. KNOWN OPEN BUG: Install hangs (vfork() of the ixemul build deadlocks in
     the Workbench-started process). The wait is bounded (--install-secs): a hang ends as a [FAIL] naming it.
  4  stock `upkg list` from a Shell still returns 0 with at least one line.
  5  stack: the host binary's `$STACK: N` cookie and the stack of the icon used (do_StackSize of the .info).
  --wbcd   the exit test instead of steps 3-5 (needs the rig started with `--mmu` and `rig.py muforce on`): the icon
           start, the window closed through its close gadget, then: zero new MuForce hits, no new CPU reset in the
           FS-UAE log, the machine answers. Without the ixemul fix every Workbench-started ixemul program UnLocks
           the launcher's wa_Lock a second time at exit (DOS frees pr_CurrentDir): MuForce shows 4 hits, a rig
           without an MMU resets. IXEMUL=<path> puts that ixemul.library in LIBS: first (the kit's is restored by
           the caller); the run FAILS with the kit 80.1 library and PASSES with the fix (ixemul fix/wb-currentdir).
Exit status is non-zero on any FAIL.

Needs the rig up with `UPTERM_RIG=<n> python3 tools/rig/rig.py start --os 32 --max` and UPTERM_RIG set (an unset
variable means the owner's rig 1: the script refuses). Needs build/amiga/wbrun (make build/amiga/wbrun) and a feed
drawer (Packages, Packages.gz, .ipk files: make dist-ipk). The rig is left running.

  UPTERM_RIG=8 python3 tools/rig/upkg_wb_rig.py [--upkg PATH] [--feed DIR] [--icon FILE.info] [--out DIR]
                                                [--install-secs N]
  UPTERM_RIG=8 IXEMUL=<ixemul.library> python3 tools/rig/upkg_wb_rig.py --wbcd
"""
import os, re, struct, sys

if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
    print(__doc__)
    sys.exit(0)
if not os.environ.get('UPTERM_RIG'):
    sys.exit('upkg_wb_rig: UPTERM_RIG is not set (unset means the owner\'s rig 1). Run as UPTERM_RIG=<n> python3 tools/rig/upkg_wb_rig.py')

import argparse, pathlib, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import upkg_rig

DEFAULT_UPKG = '/Users/spot/Code/UP-Term/upterm-ports-wbui/build/obj/opkg/src/upkg'
DEFAULT_FEED = ir.ROOT / 'build/ipk'
SENTINEL = re.compile(r'^window opened, (\d+) rows$', re.M)
LOG = 'UP-Term:log/upkg-wb.log'
WINDOW = 'UP-Term Add-ons'
INSTALL_GADGET = '2'
ROW_PX = (60, 60)        # first row of the list: rows start at y 58 on the 800x600 screen (y 50 is above the listview: nothing is selected)
STOCK_ICON = 'SYS:Utilities/Clock.info'
FAILS = []
report = None


def verdict(ok, what, detail=''):
    line = '[%s] %s%s' % ('OK' if ok else 'FAIL', what, (' -- ' + detail) if detail else '')
    print(line, flush=True)
    with open(report, 'a') as f:
        f.write(line + '\n')
    if not ok:
        FAILS.append(what)
    return ok


def stack_cookie(path):
    """N of the `$STACK: N` cookie in the binary (None when it has none)."""
    m = re.search(rb'\$STACK: (\d+)', pathlib.Path(path).read_bytes())
    return int(m.group(1)) if m else None


def icon_stack(data):
    """do_StackSize of a DiskObject (.info): 78-byte header then offset 74 is the stack (0 = the default 4096)."""
    if len(data) < 78 or data[:2] != b'\xe3\x10':
        return None
    return struct.unpack('>I', data[74:78])[0]


def installed_names(status):
    return set(re.findall(r'^Package: (\S+)', status, re.M))


def setup(upkg, feed, icon_remote):
    """The rig ready: assigns, the feed and conf, upkg and its icon in RAM:U, wbrun in RAM:."""
    for cmd in ('Assign TMP: T:', 'Assign UP-Term: VTCX:Apps'):
        ir.run(cmd)
    for d in ('UP-Term:log', 'UP-Term:etc', 'UP-Term:pkg', 'UP-Term:pkg/upkg', 'UP-Term:pkg/upkg/lists',
              'UP-Term:pkg/upkg/info', 'UP-Term:pkg/upkg/cache', 'RAM:feed', 'RAM:U'):
        ir.run('MakeDir >NIL: ' + d)
    for f in sorted(feed.iterdir()):
        if f.name in ('Packages', 'Packages.gz') or f.suffix == '.ipk':
            ami.put(str(f), 'RAM:feed/t.tmp')      # ami.put fails for destination names of ~30+ characters
            ir.run('Rename RAM:feed/t.tmp RAM:feed/' + f.name)
    conf = upkg_rig.conf_for((ir.ROOT / 'dist/upkg.conf').read_text(), 'file:/RAM/feed')
    (ir.VTC / 'wb-rig.conf').write_text(conf)
    ir.run('Copy VTC:wb-rig.conf UP-Term:etc/upkg.conf')
    ir.run('Echo >UP-Term:VERSIONS "UP-Term 1.0"')
    ir.run('Delete >NIL: UP-Term:pkg/upkg/status UP-Term:pkg/upkg/lock %s QUIET' % LOG)
    ami.put(str(upkg), 'RAM:U/upkg')
    ir.run('Protect RAM:U/upkg +rwed')
    ir.run('Copy %s RAM:U/upkg.info' % icon_remote)
    ami.put(str(ir.ROOT / 'build/amiga/wbrun'), 'RAM:wbrun')
    ir.run('Protect RAM:wbrun +rwed')


def shell_upkg(args):
    """`RAM:U/upkg <args>` from a vsh script: (rc, output)."""
    (ir.VTC / 'wb-cmd.sh').write_text('RAM:U/upkg %s 2>&1\necho "[exit $?]"\n' % args)
    rc, out = ir.run_slow('wb-cmd', 'VTC:vsh VTC:wb-cmd.sh', 300)
    m = re.search(r'\[exit (\d+)\]', out)
    return (int(m.group(1)) if m else 1000 + rc), out


def read(path):
    return ami.read_file(path).decode('latin-1') if upkg_rig.exists(path) else ''


def wait_for(fn, secs, every=3):
    end = time.time() + secs
    while time.time() < end:
        v = fn()
        if v:
            return v
        time.sleep(every)
    return fn()


def main(argv):
    global report
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--upkg', default=DEFAULT_UPKG, help='the m68k upkg (default: the upterm-ports-wbui build)')
    ap.add_argument('--feed', default=str(DEFAULT_FEED), help='drawer with Packages, Packages.gz and the .ipk files')
    ap.add_argument('--icon', help='host .info file for the upkg icon (default: the rig\'s %s, a stand-in)' % STOCK_ICON)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-wb'))
    ap.add_argument('--install-secs', type=int, default=120, help='how long Install may take before the run calls it a hang')
    ap.add_argument('--wbcd', action='store_true', help='the exit test: zero MuForce hits, no CPU reset (see the header)')
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    report = out / 'report.txt'
    report.write_text('')
    upkg, feed = pathlib.Path(a.upkg), pathlib.Path(a.feed)
    for need in (upkg, feed / 'Packages.gz', ir.ROOT / 'build/amiga/wbrun'):
        if not need.exists():
            raise SystemExit('upkg_wb_rig: no %s' % need)
    print('rig %d (UPTERM_RIG), upkg %s' % (paths.RIG_N, upkg))

    with paths.rig_lock():
        icon_remote = STOCK_ICON
        icon_note = 'stand-in %s' % STOCK_ICON
        if a.icon:
            ami.put(a.icon, 'RAM:upkg-icon.info')
            icon_remote, icon_note = 'RAM:upkg-icon.info', a.icon
        setup(upkg, feed, icon_remote)

        if a.wbcd:
            return wbcd(a)

        # step 1: the feed works from a Shell
        rc, o = shell_upkg('update')
        if not verdict(rc == 0, 'step 1: setup, `upkg update` from a Shell returns 0', 'rc %d' % rc if rc else ''):
            print(o[-500:])
            return 1
        rc, o = shell_upkg('list')
        rows = [l for l in o.split('\n') if ' - ' in l]
        if not verdict(bool(rows), 'step 1: the feed lists packages', '%d rows' % len(rows)):
            return 1
        first = rows[0].split(' - ')[0].strip()
        before = installed_names(read(upkg_rig.STATUS))

        # step 2: start from the icon, read the sentinel
        w0 = set()
        try:
            import wbrun_rig
            w0 = wbrun_rig.window_ids()
        except Exception:
            pass
        ir.run('Run >NIL: RAM:wbrun RAM:U/upkg', 30)
        m = wait_for(lambda: SENTINEL.search(read(LOG)), 90)
        n = int(m.group(1)) if m else 0
        if not verdict(n > 0, 'step 2: icon start writes "window opened, N rows" with N > 0 to %s' % LOG,
                       'N = %d' % n if m else 'no sentinel after 90 s: %r' % read(LOG)[-200:]):
            return finish()
        tree = ami.req(0x0D).decode('latin-1')
        verdict(WINDOW in tree, 'step 2: the window "%s" is in the Intuition tree' % WINDOW)

        # step 3: select the first row, click Install, bounded wait for the status record
        ami.click_px(*ROW_PX)
        time.sleep(2)
        try:
            ami.main(['gclick', WINDOW, INSTALL_GADGET])
        except SystemExit as e:
            verdict(False, 'step 3: Install gadget %s found in "%s"' % (INSTALL_GADGET, WINDOW), str(e))
            return finish()
        done = wait_for(lambda: first in installed_names(read(upkg_rig.STATUS)) - before, a.install_secs, 5)
        if not done:
            tmp = ir.run('List T: ALL LFORMAT "%s%s"' % ('%p', '%n'))[1]
            unpack = [l for l in tmp.split('\n') if 'opkg-' in l or 'unpack' in l]
            verdict(False, 'step 3: Install of %s' % first,
                    'HANG: no status record for %s after %d s (known open bug: vfork() of ixemul deadlocks in the '
                    'Workbench-started upkg; the unpack drawer in T: stays: %s)' % (first, a.install_secs, ' '.join(unpack[:3]) or 'none seen'))
        else:
            verdict(True, 'step 3: Install of %s reached UP-Term:pkg/upkg/status' % first)

        return finish_after(upkg, icon_remote, icon_note)


def wbcd(a):
    """Workbench start and exit of an ixemul program under MuForce: no new hit, no CPU reset, machine answers."""
    import muforce
    if not muforce.running():
        return verdict(False, 'wbcd: MuForce is running (rig started --mmu, `rig.py muforce on`)') and 0 or finish()
    lib = os.environ.get('IXEMUL')
    if lib:
        ami.put(lib, 'LIBS:ixemul.library')
        for _ in range(2):
            ir.run('Avail FLUSH')
    print('[INFO] wbcd: ixemul.library in LIBS: %s' % (lib or 'as installed'))
    hits0 = len(muforce.parse_hits(muforce.drawer().joinpath(muforce.LOG).read_text(encoding='latin-1')
                                   if muforce.drawer().joinpath(muforce.LOG).is_file() else ''))
    crash0 = paths.fsuae_crash()
    ir.run('Run >NIL: RAM:wbrun RAM:U/upkg', 30)
    m = wait_for(lambda: SENTINEL.search(read(LOG)), 90)
    if not verdict(bool(m), 'wbcd: the icon start opened the window (sentinel in %s)' % LOG):
        return finish()
    try:
        ami.main(['gclick', WINDOW, 'sys:close'])
    except SystemExit as e:
        verdict(False, 'wbcd: close gadget of "%s" clicked' % WINDOW, str(e))
        return finish()
    time.sleep(20)
    rc, o = ir.run('Echo ok', 30)
    verdict('ok' in o, 'wbcd: the machine answers after the window closed')
    f = muforce.drawer() / muforce.LOG
    new = muforce.parse_hits(f.read_text(encoding='latin-1') if f.is_file() else '')[hits0:]
    verdict(not new, 'wbcd: zero new MuForce hits after the window closed',
            '%d new: %s' % (len(new), muforce.summary(new).replace('\n', ' | ')) if new else '')
    verdict(paths.fsuae_crash() == crash0, 'wbcd: no new CPU reset in the FS-UAE log')
    return finish()


def finish_after(upkg, icon_remote, icon_note):
    # step 4: the window may still be open and hold the lock after a hang: break it, then stock list
    ir.run('Break >NIL: NAME upkg ALL')
    time.sleep(3)
    ir.run('Delete >NIL: UP-Term:pkg/upkg/lock QUIET')
    rc, o = shell_upkg('list')
    lines = [l for l in o.split('\n') if ' - ' in l]
    verdict(rc == 0 and bool(lines), 'step 4: stock `upkg list` from a Shell returns 0 with output', 'rc %d, %d lines' % (rc, len(lines)))
    # step 5: stack
    cookie = stack_cookie(upkg)
    verdict(cookie is not None and cookie >= 262144, 'step 5: host binary `$STACK` cookie >= 262144', 'cookie %s' % cookie)
    ic = icon_stack(ami.read_file(icon_remote)) if upkg_rig.exists(icon_remote) else None
    if icon_note.startswith('stand-in'):
        print('[INFO] step 5: icon stack of %s is %s; a stand-in, not the product icon (pass --icon for the real one)' % (icon_note, ic))
    else:
        verdict(ic is not None and ic >= 65536, 'step 5: icon %s stack >= 65536' % icon_note, 'do_StackSize %s' % ic)
    return finish()


def finish():
    print('Verdict: %s' % ('FAIL (%s)' % '; '.join(FAILS) if FAILS else 'PASS'))
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
