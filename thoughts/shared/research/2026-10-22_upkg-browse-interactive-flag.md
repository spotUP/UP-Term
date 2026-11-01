# upkg-browse `interactive` returns false on the rig (2026-10-22, rig 4, rig.py start --max)

Failing step: the glob `"$listsdir"/*` with listsdir=/UP-Term/pkg/upkg/lists does not expand under vsh, so the
for-loop runs once with the literal `.../lists/*`, `[ -f "$f" ] || continue` skips it, `interactive` returns 1.

Checks (rig 4, UP-Term:bin/upkg-browse is 9543 bytes, the dist copy):
1. UP-Term:pkg/upkg/lists holds one file, `upterm` (2114 bytes, from 00:27); vsh `ls -l` shows it. OK.
2. No `upkg update` needed.
3. File is plain text (`P a c k`), grep -c UP-Term-Interactive = 1, Package: up-claude at line 1. OK.
4. vsh script: `for f in "$listsdir"/*; do echo "F=$f"; [ -f "$f" ] && echo isfile; done` prints
   `F=/UP-Term/pkg/upkg/lists/*` and no `isfile`. FAIL.
5. interactive() from dist/upkg-browse lines 84-99 sourced in the same script: `interactive up-claude` -> rc=1. FAIL.

Glob matrix under vsh (cwd = the exec default):
- `/UP-Term/pkg/upkg/lists/*`, `$d/*`, `"$d"/*`, `/UP-Term/etc/*`, `./lists/*`: unexpanded.
- `UP-Term:pkg/upkg/lists/*`, relative `lists/*` (after cd), `*` inside the dir, `$PWD/*` (System:UP-Term/...): expand.
- After `cd /`, `/UP-Term/pkg/upkg/lists/*` expands. So absolute /VOL/ glob patterns only work with cwd `/`.
Not examined: why (vsh glob code). Fix candidate: in upkg-browse `cd "$listsdir"` in a subshell and loop `*`
(or use the UP-Term: form), plus a regression test.
