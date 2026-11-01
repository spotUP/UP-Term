#!/usr/bin/env python3
"""Writes dist/addons/<id>/ (the six files every upkg-only add-on carries) for the application ports of
dist/slim.txt, from dist/packages.txt and the one table below. The existing packages (git, perl, vim, bitchx,
vbcc) are written by hand and left alone; a new application is a row in packages.txt, rows in slim.txt and
a line here (title, blurb, command to try).

  python3 tools/mkappaddons.py            write the drawers of every id listed below
  python3 tools/mkappaddons.py --check    exit 1 when a drawer differs from what this would write
"""
import pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pkgs, slim

ROOT = pathlib.Path(__file__).resolve().parents[1]
# id: (name, what it is, command to try)
APPS = {
    'mutt': ('mutt', 'the mail client', 'mutt -v'),
    'lynx': ('lynx', 'the text web browser', 'lynx -version'),
    'joe': ('joe', "Joe's Own Editor", 'joe -help'),
    'mg': ('mg', 'the small Emacs-style editor', 'mg'),
    'nano': ('nano', 'the text editor', 'nano --version'),
    'tig': ('tig', 'a git browser', 'tig --version'),
    'ncdu': ('ncdu', 'the disk usage analyzer', 'ncdu --version'),
    'nnn': ('nnn', 'the file manager', 'nnn -V'),
    'lua': ('Lua', 'the scripting language', 'lua -v'),
    'jq': ('jq', 'the JSON processor', 'jq --version'),
    'hexedit': ('hexedit', 'the hex editor', 'hexedit'),
    'picocom': ('picocom', 'the serial terminal', 'picocom --help'),
    'nc': ('nc', 'the OpenBSD netcat', 'nc -h'),
    'fzy': ('fzy', 'the fuzzy finder', 'fzy --version'),
    'ctags': ('ctags', 'the source tag generator', 'ctags --version'),
    'zip': ('zip', 'the Info-ZIP archiver', 'zip -v'),
}
LICENCE = {  # SOURCES.txt of the ports kit
    'mutt': 'GPL v2 or later', 'lynx': 'GPL v2', 'joe': 'GPL v2 or later', 'mg': 'public domain, BSD and ISC',
    'nano': 'GPL v3 or later', 'tig': 'GPL v2 or later', 'ncdu': 'MIT',
    'nnn': 'BSD 2-clause', 'lua': 'MIT', 'jq': 'MIT', 'hexedit': 'GPL v2 or later', 'picocom': 'GPL v2 or later',
    'nc': 'ISC', 'fzy': 'MIT', 'ctags': 'GPL v2 or later', 'zip': 'Info-ZIP (BSD-like)',
}


def drawers(pats):
    """UP-Term: drawers the package's paths live in (bin, etc, share, licenses ...), in first-seen order."""
    out = []
    for p in pats:
        d = p.split('/')[0]
        if d not in out:
            out.append(d)
    return out


def files(pid, row):
    name, what, tryit = APPS[pid]
    ver, arch = row['version'], row['archive']
    pats = slim.patterns(pid)
    dr = drawers(pats)
    tops = list(dict.fromkeys(d.split('/')[0] for d in dr))   # top drawers only: the page lines are 56 wide
    where = 'UP-Term:' + ', '.join(tops)
    lic = LICENCE[pid]
    kb = 'KBCOPY' + pid.upper()
    word = ' '.join(name.upper())
    mkdirs = ''.join('MakeDir >NIL: UP-Term:%s\n' % d for d in dr)
    install = f""".KEY DIR/A,STEP/K
.BRA {{
.KET }}
; {arch} install: {name}, {what}, at the paths the base up-term.lha used to carry
; Run by this archive's Install (an Installer script), which asks the questions:
;   Execute <archive>/Files/install.dos <archive>/Files [STEP <part>]
; From a Shell without Installer the same line installs everything. The Installer
; runs the parts one at a time with STEP (tests/test_dist_addons.py holds the two to
; the same list); the first part, check, stops the install when up-term.lha is
; missing or too old (the reason is in ENV:UPTCHECK).
FailAt 21
; the archive's Files drawer: every name below is relative to it
CD "{{DIR}}"
If NOT "{{STEP}}" EQ ""
  Skip {{STEP}}
EndIf
; @include dist/common/@CHECKFRAG@
Lab copy-{pid}
; {name}, {what} (dist/slim.txt names the files): the same paths in UP-Term: the base put them at
; (vsh finds the program on its path).
; userland holds what dist/slim.txt names; Copy of a drawer copies what is in it.
{mkdirs}Copy userland UP-Term: ALL CLONE QUIET
If WARN
  Quit 20
EndIf
If NOT "{{STEP}}" EQ ""
  Quit 0
EndIf
; @include dist/common/record.dos
Echo "{name} installed: type  {tryit}  in an UP-Term window (vsh)."
"""
    installer = f"""; @include dist/common/addon-head.installer

(if (not #update)
(
; welcome: n = 8, lead 3
(message
    "\\n\\n\\n{word}\\n\\n"
    "{name}, {what}, for an UP-Term window.\\n"
    "It goes into {where},\\n"
    "where the base had it.\\n"
    "Needs up-term.lha installed first.\\n"
    (all))
(set #kb-{pid} @{kb}@)
(set #totalkb @KBTOTAL@)
(set #free (getdiskspace (pathonly #dest)))
(if (and (> #free 0) (< (/ #free 1024) #totalkb))
    (abort "Nothing was installed. " #dest " has " (/ #free 1024) " KB free;\\n"
           "{name} needs about " #totalkb " KB. Free room and\\n"
           "run Install again."))

;  ready: n = 7, lead 3
(message
    (cat "\\n\\n\\nReady to install {name} into\\n\\n" #dest "\\n\\n"
         "about " (/ #totalkb 1024) " MB.\\n"
         "Proceed starts the install;\\n"
         "Abort Install leaves everything as it was."))
)
    ; Update: the options of the last install, from its record
    (
        (if (exists "@OPTS@/UPTOPTDEST") (run "Copy @OPTS@/#? ENV: QUIET"))
        (set #kb-{pid} 0)
        (set #totalkb (+ 1 (getenv "UPTUPDKB")))
        ))
(set #donekb 0)

(complete 0)

(P_OPTIONS)

(set #step "copy-{pid}" #kb #kb-{pid} #n 1
     #why "Copying {name} to {where}.")
(P_STEP)

(set #n 6)
(P_LEAD)
(set #done
    (cat #lead "{name} is installed. In an UP-Term window, in vsh:\\n"
         "  {tryit}\\n\\n"
         "Uninstall (beside Install) takes {name} out again."))
; @include dist/common/addon-tail.installer
"""
    parts = f"""# Which part of Files/install.dos installs each file of this archive (see dist/parts.txt).
-               Install
-               Install.info
-               Uninstall
-               Uninstall.info
-               README.txt
-               LICENSES.txt
-               Files/install.dos
-               Files/upupdate
record          Files/VERSIONS
copy-{pid}{' ' * max(1, 16 - len('copy-' + pid))}Files/userland
"""
    readme = f"""{arch}: {name}, {what}, for UP-Term

Contents: {', '.join(pats)}.
Licence: {lic}; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: ({', '.join(dr)}): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  {tryit}

Install: run Install (Installer) or, with upkg, install {arch}. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
"""
    licenses = f"""{arch}: parts, licences and where the source is

Part                          Licence                          Source
----------------------------  -------------------------------  --------------------------------------------
{name} ({', '.join(pats)}) {lic} upterm-ports (pkgs/{pid})

Notes
- These files were part of up-term.lha until the slim base (P9); the package installs them at the
  same paths in UP-Term:. Its row of SOURCES.txt is in UP-Term:licenses/{pid}/SOURCES.txt.
- The complete source with UP-Term's patches and build recipes is up-term-src.lha, published next to
  up-term.lha.
"""
    return {'install.dos': install, 'Install.installer': installer, 'parts.txt': parts, 'README.txt': readme,
            'LICENSES.txt': licenses, 'uninstall-body.dos': '; the paths dist/slim.txt names for this package, as the Install put them in UP-Term:\n@SLIMDELETE@\n'}


def main(a):
    check = a == ['--check']
    bad = 0
    byid = {r['id']: r for r in pkgs.rows()}
    for pid in APPS:
        d = ROOT / 'dist/addons' / pid
        for fn, text in files(pid, byid[pid]).items():
            if check:
                if not (d / fn).is_file() or (d / fn).read_text(encoding='utf-8') != text:
                    print('differs:', d / fn)
                    bad = 1
            else:
                d.mkdir(parents=True, exist_ok=True)
                (d / fn).write_text(text, encoding='utf-8')
    sys.exit(bad)


if __name__ == '__main__':
    main(sys.argv[1:])
