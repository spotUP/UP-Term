# Kit rebuild with the fixed m68k cc1 - result (2026-10-17)

Plan: 2026-10-17_kit-rebuild-inventory.md. Compiler: ~/opt/amiga/libexec/gcc/m68k-amigaos/6.5.0b/cc1.

## Repos

| Repo | State | Source commit |
|---|---|---|
| libixcompat | rebuilt | none needed |
| upterm-ports libs (ncurses zlib bzip2 xz), programs, kit-stage | rebuilt | none needed |
| tmux-amiga | rebuilt | a420e3a (in6_addr, getline from the SDK headers) |
| screen-amiga | rebuilt | none (src/screen.c carries someone else's uncommitted edits, built in) |
| neovim-amiga | rebuilt | 829f6b0 (netinet/in.h _IXEMUL_IN6_DEFINED guard) |
| vtcon wasabi + bebbossh | rebuilt | none |
| cpython-amiga | rebuilt | db25c41 (installed cc1), 773dbe9 (cexp, cimag, creal from newlib libm) |
| vtcon make dist, dist-check | rc 0 | none |

## Binaries

before.sha256 2497 files, after 2500: 314 changed, 2183 unchanged, 3 new (lists in the session scratchpad table.txt).

## Findings

- ixemul libm (ixemul-vtcon, ~/opt/amiga/m68k-amigaos/ixemul/lib/libm.a) has no cexp. gcc -O2 fuses sin(x) and cos(x) of one argument into a cexp call, so cpython failed to link once the SDK headers made sin/cos builtins. cpython now takes cexp, cimag, creal from the toolchain's newlib libm. Proper fix belongs in ixemul-vtcon libm.
- cpython `make compat dist` does not depend on `m68k`: it succeeds on a stale python.exe. Run `make m68k` explicitly.
- SDK headers changed 16 Oct (struct in6_addr, getline, AF_INET6 = 24): tmux and neovim needed header-drift fixes.

## Rig 3 install_rig --force: 10 of 19 cases failed

Passed: claude, base-libs, base-var, python-bare, python-ixemul, nobase, net, net-upgrade, wasabi.

Stale expectations in the test (not rebuild regressions):
- base 46: test expects "GNU coreutils 5.2.1"; the ports ls is 9.12.
- pi, pi-bare, all: test expects amiga-pi 1.1.0; the binary reports 1.2.0.
- base 1 and 86: TERMINFO is /VTC/terminfo, a fixture left on the rig.

Cause found for nvim and python (and likely the cascades in base-dest, base-move, all, legacy): rig 3 System: (DH0, 106 MB) is full. After the base Install plus the Python3 Install, `Info DH0:` shows 217221 used, 1 free, and the Install prints "Can't open UP-Term:MANIFEST for output - disk is full" and "Can't open Python 3 for output - disk is full". The python3 binary then exists but the case reports "Unknown command" (reproduced by hand: base Install, Python3 Install, then python3). Run directly with the kit ixemul.library in LIBS: and `Stack 1000000`, the new python3 prints 42. After both Uninstalls DH0 is back to 85460 used, 131762 free. So the failures are a rig-capacity problem; no binary fault seen. Unverified: whether the kit also grew with this rebuild (no sha or size baseline of an installed kit exists). Need: a larger System: on rig 3, or clean DH0 leftovers, then `install_rig.py --failed`.

Not done: userland_rig.py runs for the changed packages (blocked by the full System: partition on rig 3; rig 2 not started). claude-bare failure reason unread.
