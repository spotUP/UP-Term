---
date: 2026-10-15
topic: jq on rig 2 raises ixemul "free: start of block corrupt!" (library/malloc.c:277)
tags: [jq, ixemul, amiga, parked]
status: draft
---
# jq heap-corrupt requester (parked)

Check at malloc.c:277 (free): block address odd, or block->magic != MAGIC1. Not an end-of-block check.

Measured on rig 2 (fresh rig, patched jq with exec_stack ALIGNMENT=8, commit 68c8a21 in build/src/jq):
- `jq -n 1` ok (prints 1). `jq -n 1 a.json` ok (file named, never read).
- `jq . a.json` and `jq -c . a.json`: requester "free: start of block corrupt!", task jq, only button Abort. Reproducible.
So the fault is in reading/parsing/printing a file, not in startup, option parsing, input-state add/free, or exit.
The unpatched jq failed the same way (execute.c:986 assert and the requester), so ALIGNMENT=2 was a real bug but is not the only one.

Ruled out by source read: libixcompat is not in the path (jq uses no getline/getdelim/reallocarray/strtonum; strptime/pthread only on demand). jv_dtoa.c endianness is set by configure (IEEE_MC68k), jvp_dtoa_context_free frees only MALLOC'd blocks (no Omit_Private_Memory).
Host build under glibc/macOS libc passes 19 cases, so the bug needs m68k layout or ixemul malloc.

Next probe (needs an instrumented build, not more rig runs): link jq with -Wl,--wrap=free/malloc/realloc logging pointer+size to a file, run `jq . a.json`, last free before the requester names the pointer. Suspects: jv_parser/jv_dump buffers, jvp_dtoa Bigint (value 3.14159 in a.json), bison yacc stack realloc.
Rig note: every aborted jq wedged wasabid (exec times out); restart rig 2 between probes.

UP-Term: requester: not seen on rig 2 (build/rig2/requesters.log has only the jq entry). userland_rig.kit_assigns only assigns when missing; rig 2 has UP-Term: -> VTCX:Apps from boot. amigapi_rig.py (drop_rig_assign, REMOVE) is the one script that removes it; likely rig 3 (other agent), not verified.
tmux link: the tmux requester ("Agent 2 report (tmux hang") is unverified; a heap-corrupt free in a port is a plausible class, no evidence tying them.
