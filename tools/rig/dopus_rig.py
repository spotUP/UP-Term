#!/usr/bin/env python3
"""dopus_rig.py -- `rig.py dopus`: install Directory Opus 5.104 once into this rig's host VTC: drawer (VTCX:DOpus5,
build/rigN/vtc/DOpus5, it survives every clone) with the archive's own Installer, startup mode "Instead of
Workbench", and record what the clone edit (rig.sys_hdf_dopus) needs beside it:

  vtc/dopus5-user-startup.txt   the Installer's ';BEGIN DOpus5' ... ';END DOpus5' block of S:User-Startup
  vtc/dopus5-envarc/DOpus5*     the ENVARC:DOpus5* files it wrote
  vtc/dopus5.stamp              the archive's md5 (dist/addons/dopus/archives.txt), written last

The rig boots with --no-dopus (no DOpus5: assign, no 5.82 running: the Installer takes its fresh-install path),
the Installer runs with CD set to the unpacked tree (it needs the DOpus5 drawer next to the script), and its pages
are driven by gadget id with installer_rig's helpers (Proceed 90, askdir string 92, radio buttons 2..4)."""
import hashlib, os, pathlib, re, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import install_rig as ir
import installer_rig as inst
import rig

inst.KITWINDOW = re.compile(r'Install DOpus5')   # the Installer window's title is the script's name
ARCHIVE = rig.ROOT / "vendor/dopus5-5.104" / rig.DOPUS_ARCHIVE
TREE = "dopus_install"
SCRIPT = "dopus_run.script"
PURPOSE = "vtcon-05: DOpus 5.104 install into the rig's VTC:"


def page():
    tree = ami.req(0x0D).decode('latin-1')
    title, gads = inst.installer_page(tree)
    return tree, title, gads


def answer(tree, title, gads):
    """The gadget ids to click on this page, by its ids (the Installer's custom gadgets carry no labels), or None."""
    ids = set(gads) - {0}
    if 92 in ids:   # askdir: the parent of the DOpus5 drawer
        if not inst.type_into(gads[92][0], 92, "VTCX:"):
            sys.exit("dopus: could not type VTCX: into the drawer page")
        return [90]
    if {1, 2, 3, 4, 5} <= ids:   # Installation Options: Install for Real, no log (the defaults)
        return [90]
    if {1, 2, 3, 4} <= ids and 90 in ids:   # the user level page: Average, so the questions are asked
        return [3, 90]
    if {2, 3, 4} <= ids and 90 in ids:   # how should Directory Opus 5 start: Instead of Workbench
        return [4, 90]
    if {2, 3} <= ids and 90 in ids:   # the image set: the default (the radio already set)
        return [90]
    if ids <= {90, 91} and 90 in ids:   # the end messages
        return [90]
    return None


def drive(timeout=900):
    t0, last, log = time.time(), None, []
    seen_end = False
    while time.time() - t0 < timeout:
        if ir.run('Status >NIL: COM "Installer"')[0] != 0 and not page()[2]:
            return log
        try:
            tree, title, gads = page()
        except ConnectionError:
            time.sleep(5)
            continue
        if not gads:
            time.sleep(3)
            continue
        sig = (title, tuple(sorted(gads)))
        if sig == last:
            time.sleep(3)
            continue
        ids = answer(tree, title, gads)
        if ids is None:
            sys.exit("dopus: a page this driver does not know: %s %s\n%s" % (title, sorted(gads), inst.window_ocr(tree, "dopus-unknown")))
        log.append("%s %s -> %s" % (title, sorted(gads), ids))
        last = sig
        for g in ids:
            tree, title, gads = page()
            if g not in gads:
                break
            inst.click(gads[g][0])
            time.sleep(2)
            if g == 4:   # verify the start mode took before Proceed
                tree, title, gads = page()
                if 'selected' not in gads[4][1]:
                    sys.exit("dopus: 'Instead of Workbench' (gadget 4) is not selected: %s" % gads)
        time.sleep(3)
    sys.exit("dopus: the Installer did not finish in %d s; pages:\n  %s" % (timeout, "\n  ".join(log)))


def block_of(data):
    m = re.search(rb';BEGIN DOpus5\n.*?;END DOpus5\n', data, re.S)
    if not m:
        sys.exit("dopus: no ';BEGIN DOpus5' block in S:User-Startup after the install")
    return m.group(0)


def main():
    vtc = rig.RIG / "vtc"
    md5 = rig.dopus_archive_md5()
    got = hashlib.md5(ARCHIVE.read_bytes()).hexdigest()
    if got != md5:
        sys.exit("dopus: %s md5 %s, archives.txt says %s" % (ARCHIVE, got, md5))
    if "--force" not in sys.argv and (vtc / rig.DOPUS_STAMP).exists() and (vtc / rig.DOPUS_STAMP).read_text().strip() == md5:
        print("dopus: 5.104 is already installed in %s (stamp %s); --force installs again" % (vtc, md5))
        return
    if rig.mine():
        sys.exit("dopus: this rig runs; stop it first (the install boots it with --no-dopus)")
    shutil.which("lha") or sys.exit("dopus: lha is missing on the host (brew install lhasa)")
    for n in (TREE, "DOpus5", rig.DOPUS_ENVARC, rig.DOPUS_STAMP, rig.DOPUS_BLOCK):
        p = vtc / n
        shutil.rmtree(p) if p.is_dir() else p.unlink(missing_ok=True)
    (vtc / TREE).mkdir(parents=True)
    subprocess.run(["lha", "xq", str(ARCHIVE)], cwd=vtc / TREE, check=True)
    (vtc / SCRIPT).write_text('CD VTCX:%s\nInstaller "Install DOpus5" NOLOG\n' % TREE)
    try:
        subprocess.run([sys.executable, str(pathlib.Path(rig.__file__)), "start", "--max", "--no-dopus", "--purpose", PURPOSE], check=True)
        if ir.run('Assign >NIL: DOpus5: EXISTS')[0] == 0:
            sys.exit("dopus: DOpus5: is assigned on the boot; the Installer would offer an upgrade of it")
        ir.run('Run >NIL: Execute VTCX:%s' % SCRIPT)
        time.sleep(8)
        log = drive()
        print("dopus: pages answered:\n  " + "\n  ".join(log))
        time.sleep(5)
        ir.run('Delete >NIL: RAM:dopus5-us RAM:dopus5-envarc ALL QUIET')
        ir.run('MakeDir RAM:dopus5-envarc')
        ir.run('Copy S:User-Startup RAM:dopus5-us')
        ir.run('Copy ENVARC:DOpus5#? RAM:dopus5-envarc')
        ir.run('Copy RAM:dopus5-us VTCX:dopus5-us.tmp')
        ir.run('Copy RAM:dopus5-envarc VTCX:%s ALL' % rig.DOPUS_ENVARC)
        ir.run('Delete >NIL: RAM:dopus5-us RAM:dopus5-envarc ALL QUIET')
        us = vtc / "dopus5-us.tmp"
        (vtc / rig.DOPUS_BLOCK).write_bytes(block_of(us.read_bytes()))
        us.unlink()
        if not (vtc / "DOpus5/DirectoryOpus").exists():
            sys.exit("dopus: the Installer left no VTCX:DOpus5/DirectoryOpus")
        (vtc / rig.DOPUS_STAMP).write_text(md5 + "\n")
        print("dopus: installed; block:\n" + (vtc / rig.DOPUS_BLOCK).read_text())
    finally:
        shutil.rmtree(vtc / TREE, ignore_errors=True)
        (vtc / SCRIPT).unlink(missing_ok=True)
        time.sleep(5)   # the command's last writes land before the stop
        rig.stop()


if __name__ == "__main__":
    main()
