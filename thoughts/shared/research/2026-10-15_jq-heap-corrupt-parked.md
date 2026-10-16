---
date: 2026-10-15
topic: jq on rig 2 raises ixemul "free: start of block corrupt!" (library/malloc.c:277)
tags: [jq, ixemul, amiga, parked]
status: not reproduced
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

## Hunt 2026-10-15

Status: foothold, root cause not found (budget spent: about 50 tool calls). Rig 2, `rig.py start --max`. Probe binary: jq 1.7.1 with patches 0001 and 0002, linked with `-Wl,--wrap=malloc,--wrap=free,--wrap=realloc,--wrap=calloc`. The probe checks the ixemul header (MAGIC1 at mem-4 and mem-8) on every free and realloc, keeps a ring of the last 48 heap events with frame-pointer backtraces, and adds probe_log() prints in bytecode_free and jq_teardown. Worktree: /Users/spot/Code/UP-Term/upterm-ports-jqprobe, branch probe/jq-heap. Commit 62fca88 holds pkgs/jq/probe/wrap.c. The print edits live only in build/obj/jq/src/{bytecode,execute}.c.

### Violated invariant

`jq_state->bc` must hold the pointer that `jq_compile_args` stored (a live, ixemul-headed block) until `jq_teardown` frees it. On a failing run the field reads 0x15 when jq_teardown starts. Some write between compile and teardown overwrites the field. ixemul's free then sees an odd pointer and raises "free: start of block corrupt!" (malloc.c:277).

### Evidence

- The failure is intermittent: about 1 run in 2 or 3. The output is always correct, and the failure always comes after the output.
- T:jqwrap.log: `JQWRAP free-bad ptr=0x15 known=1 frames=jv_mem_free+0xe, jq_teardown+0x22, main+0x438`.
  - The ring shows the preceding frees as valid. They come from bytecode_free+0xb4 (symbol_table_free) and from +0xd2, after jv_free(debuginfo).
  - The bad free is the final tail call of bytecode_free, `jv_mem_free(bc)` (src/bytecode.c, last line of bytecode_free; disassembly at 0x4880a: `movel a2,a5@(8); ...; jmp _jv_mem_free`).
- Prints from T:jqprobe.log. jq_state is at the same address (0x1089127c) in both runs.
  - Good run:
    ```
    PROBE teardown jq=0x1089127c bc=0x1093fce4
    PROBE teardown after reset bc=0x1093fce4
    ```
  - Bad run:
    ```
    PROBE teardown jq=0x1089127c bc=0x15
    PROBE teardown after reset bc=0x15
    PROBE bcfree top bc=0x15 code=0x10937e24 nsub=0 glob=0x10942fe4
    ```
- What this rules out:
  - The bad value is already in place when jq_teardown starts. So a2 is not clobbered, and jq_reset and the debuginfo jv_free do not corrupt the field.
  - "known=1" for 0x15 means the probe's own BSS hash table also holds 0x15. That fits a wild write (or a stray small integer that a tombstone step put there); it is not proven.
- On the bad run, the rig hung after the "post-dbg" print. The hang comes from the probe's free path dereferencing 0x15 (low memory) before _exit(99). It is a side effect of the probe, not a new bug.

### Suspected corrupting write

Unknown. 0x15 (21) is a small integer written over a heap pointer field in jq_state. The candidate class is a write through a stale pointer into a freed block that ixemul has re-issued as jq_state, or a write past the end of a neighbouring block.

ixemul's small-block cache (commit d8e75c3, 16-byte classes) re-issues freed blocks at once and in LIFO order. glibc does not, which explains why the host build passes. jq's struct jv is 16 bytes, and m68k aligns int to 2 bytes. Look at jv code that writes a 4-byte count or refcnt into a block that it has already released (jvp_* refcnt, or jv_number literal/decNumber caching, where DECLITEND is unchecked).

### Next probe (one step each)

1. Read `jq->bc` with probe_log after jq_compile_args, after jq_start, and after each jq_next/jv_dumpf in main.c. This bisects the window in which the field changes.
2. Then make the probe store the jq_state address. In __wrap_free and __wrap_realloc, check `jq->bc` after every call and dump the ring on the first change. The event that changes the field names the writer, with its frames.
3. Optional cross-check: an ixemul probe build (../ixemul-vtcon-jqprobe from feature/ixemul-80) with MCACHE_MAX 0. If the failure goes away, the trigger is reuse-after-free that the cache exposes.

### Proposed fix

None can be stated yet. The fix belongs at the writer (a jq use-after-free or overflow, or an ixemul cache bug), not in bytecode_free or teardown. Do not guard the free.

## Hunt part 2 (2026-10-15/16, opus agent, rig 2)

Status: cause NARROWED, writer file:line NOT proven. No fix made (fix/jq-heap branch and ../upterm-ports-jqfix worktree not created, because the brief allows a fix only for a proven cause).

### Proven

1. jq->bc is 0x15 from compile on. It is never overwritten later. Stage logs from bad runs:
   - `PROBE cargs bound n=0 bc=0x0`
   - `PROBE block_compile bc=0x15 out=0x1082c844 *out=0x15`
2. The 0x15 comes from ixemul malloc. Ring line: `JQWATCH ev m 0x15 64 ... _jv_mem_alloc _block_compile+0x20`.
   - malloc(64) for struct bytecode RETURNED 0x15. 0x15 = 0x1 + 0x14 (realblock offset).
   - So the head link of the class-3 (49..64 byte) small-block cache was 0x1.
   - ixemul malloc pops `u_mcache[c] = *(void**)&res->realblock` and does not check it. A corrupt first word in a cached free block becomes a malloc result.
3. A jv refcount operation runs on a freed, cached block. This is deterministic: it happens on every run, also the good ones. Freed-list checker, 4 runs, the same hit each time:
   - `JQFL hit at pre-free block=0x108e3f1c n=54 word0=ffffffff words=62333234 0000004c 00000026 4572726f`
   - The block is a jvp_string: refcnt@0, length_hashed 0x4c (len 38), data "Erro...".
   - It was allocated by jv_string_sized from jq_yylex: a string-literal token from the builtin source.
   - It was freed by jv_free in inst_free+0x9c, during block_bind_referenced in builtins_bind. That is the free of an unreferenced builtin def.
   - Later its refcnt went from 0 to -1 (the ixemul cache link went from NULL to 0xffffffff). This was found before the free at inst_free+0x2a.
   - Read: one string constant has two owners but only one reference. There is a missing jv_copy or one jv_free too many.
   - The jq refcnt is the first word of the block, and that word is also ixemul's cache link. So any late inc/dec on a freed jv corrupts the cache. The bad 0x1 head is most likely the same fault: a NULL link plus one increment.
4. Probable string family: the m68k-only "Error: <name>/N not found at build time" builtins (builtin.c:1859-1867 LIBM_*_NO plus libm.h).
   - On the m68k build, configure finds none of these: acos acosh asin atanh cosh drem exp exp10 exp2 fmod gamma hypot j0 j1 jn ldexp lgamma log log10 log2 nexttoward pow pow10 remainder scalb sinh sqrt tgamma y0 y1 yn lgamma_r.
   - So jq compiles about 31 extra `def f: "Error..."|error;` builtins. Most are unreferenced and get freed in builtins_bind.
   - The 38-char strings are the names with 5 letters, for example "Error: acosh/0 not found at build time".

### Ruled out

- Host ASan build of jq 1.7.1 plus our patches, with the m68k libm set forced off (the HAVE_* list above removed from DEFS; `jq -n '4|sqrt'` gives the "not found" error): `jq . a.json` is CLEAN.
  - Build: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/a0f37c83-2beb-4734-924e-3705a1070dfb/scratchpad/hjq2
  - So the extra refcount operation is not in jq's portable code path. It is m68k-specific: compiler, ABI, ixemul/ixcompat, or patch 0001/0002 behaviour.
- Stack overflow: the host passes down to a 64K stack.
- Patch 0001 per-TU pthread slots: consistent per TU.

### Open: next probe, ready but not run

Rig 2 stopped taking commands after a restart on 2026-10-16:
- Ram Disk showed 0 MB and T: was missing (Error 218).
- ami.py exec answered "could not start the command" for 5 min.
- `rig.py stop` waits up to 20 min for an idle answer.

The next probe is built. Run it when rig 2 is healthy:
- What it is: a `probe_refcheck(c, +-1)` hook in jvp_refcnt_inc/dec (build/obj/jq/src/jv.c) and wrap.c. It logs to T:jqrc.log the first 4 refcount operations on a block in the freed list, with caller frames.
- What it gives: the exact caller of the late jv_free/jv_copy, which is the writer.
- How to run:
  - Copy the binary from upterm-ports-jqprobe/build/obj/jq/jq to build/rig2/vtc/jqp/jq.
  - Run `jq . a.json`.
  - Get the log: `ami.py get T:jqrc.log`.
  - Symbolize: `python3 sym2.py syms.txt` (syms.txt has already been regenerated for this binary).
- Then make the fix at that caller as a recipe patch. Host check: an ixemul-like LIFO reuse shim, or ASan with the same condition.

Teardown guard: none added, as briefed.

## Hunt part 3 (2026-10-16, opus agent, rig 2)

Status: late refcount op (the writer) PROVEN, file:line and chain below. Why the dangling reference exists is NOT proven yet; no code changed.

Rig 2: healthy after `make wasabi` + `UPTERM_RIG=2 rig.py start --max` (status was "down" at start; `info` rc 0, T: = Ram Disk:T). Stopped at the end.

### Run

`cd VTC:jqp; stack 262144; jq . a.json` (probe binary, cmp-identical to upterm-ports-jqprobe build/obj/jq/jq). Output correct, no requester this run (the late op is deterministic, the crash is not). Log: scratchpad jqrc.log, symbolized with sym2.py (global symbols only; `X+0x...` past a small function means an inlined/static callee, resolved with `/opt/amiga/bin/m68k-amigaos-objdump -d`, scratchpad jq.dis).

```
JQRC inc block=0x1091902c n=49 word0=00000000 words=00000000 00000002 00000020
JQRC now    _jv_copy+0x38 _gen_collect+0x7c _yyparse+0x11a0 _jq_parse+0x5e _jq_parse_library+0x18 _builtins_bind+0x2c _jq_compile_args+0xf4
JQRC malloc ___wrap_malloc+0x54 _jv_mem_alloc+0xe _jv_array_indexes+0x822 _jv_string_concat+0x6e _binop_plus+0x204 _yylex+0xdca
JQRC free   _jv_mem_free+0xe _jv_string_concat+0x84 _binop_plus+0x204 _yylex+0xdca _yyparse+0x8fe _jq_parse+0x5e
JQRC dec block=0x1091902c n=49 word0=00000001 ...
JQRC now    _jv_contains+0xede _bytecode_free+0x26c _gen_collect+0x18a _yyparse+0x11a0 ...
JQRC inc block=0x1091902c n=49 word0=10919018 ...   (same inc/dec pair again, after the block went back into the ixemul cache)
JQRC dec block=0x1091902c n=49 word0=10919019 ...
```

### Reading

- Block: a jvp_string, length 1 (length_hashed 2), alloc_length 32 (16+32+1 = 49). That shape is only made by jvp_string_append's grow branch (allocsz = max(2*len, 32)): a folded string constant `"" + "x"`.
- Allocated and freed by constant folding in the parser: parser.y:214 `case '+': res = binop_plus(jv_a, jv_b)` (constant_fold, inlined into gen_binop; call site 0x34f3e) -> binop_plus -> jv_string_concat -> jvp_string_append (jv.c). The free is the grow branch's free of the old string when the 1-char string is appended to again: refcnt was 1, so it was freed.
- WRITER (late inc): compile.c gen_const_array, `a = jv_array_append(a, jv_copy(i->imm.constant));` (inlined into gen_collect; return 0x4c09e, constant read from 18(a6) = inst->imm.constant). Chain: builtins_bind -> jq_parse_library -> jq_parse -> yyparse (`'[' Exp ']'` -> gen_collect) -> gen_const_array -> jv_copy.
- Late dec: the same gen_const_array's `jv_free(a)` / block_free(expr) (gen_collect+0x18a -> inst/array free -> jvp_string_free).
- So a LOADK instruction in a `[ ... ]` expression still holds a string constant that constant_fold already consumed. Its pointer has no reference of its own.
- Mechanism of the 0x15 malloc: jv_copy turns the cached free block's link from L to L+1; jv_array_append allocates in between, the cache pops this block and its head becomes L+1; with L = NULL the next malloc returns 0x1+0x14 = 0x15. That is the bytecode pointer of part 2.
- Sources are identical to the host ASan build (parser.c/parser.y/lexer.c/builtin.c same; compile.c and jv.c differ only by the probe lines), and the host run is clean. So the extra owner comes from m68k codegen or ABI (16-byte jv struct passing/returns, hidden struct-return pointer in a0), not from jq's C.

### Proposed fix (NOT applied; for review)

The writer is the victim side; patching gen_const_array would be a band-aid. The fix belongs where the second owner is made, and that site is not proven yet. Next probe (one run): in constant_fold, log a.first/b.first inst addresses and jv_a.u.ptr before block_free(a); in gen_const_array, log each LOADK inst address and constant ptr. The matching inst address shows (1) block_free(a) did not unlink/free that inst (block aliasing, so a codegen fault in the block-by-value passing around block_free/block_const), or (2) a different inst got a copy of the jv without jv_copy (struct copy miscompile). Fix then: (1)/(2) as a recipe patch that removes the by-value hazard at that site, or a compiler flag (-fno-... per the miscompiled TU), checked on the host with an ixemul-like LIFO free-list shim, plus the rig repro (20 runs, no 0x15).

### Side: why configure finds no libm functions on m68k

config.log:5592-5601: `checking for sqrt in -lm` links /Users/spot/opt/amiga/m68k-amigaos/lib/libm.a (newlib, lib_a-w_sqrt.o), which fails with `undefined reference to '_impure_ptr'` (newlib reent; ixemul does not export it). Every libm probe fails the same way, so all HAVE_* are off and the ~31 "Error: f/N not found at build time" builtins get compiled. Fix: link ixemul's libm (or provide the math functions from libixcompat), so the probes pass.

## Hunt part 4 (2026-10-16, opus agent, no rig run)

Status: ROOT CAUSE FOUND. The second owner is a compiler miscompile in m68k-amigaos-gcc 6.5.0b (bebbo, build 20260819), not jq's C, not the AmigaOS ABI as specified, and not patches 0001/0002 (their jv.c hunk is the pthread/jv_thread subset, nowhere near jvp_string_append). Rig 2 was not started: the site was proven statically, so the planned constant_fold / gen_const_array probes were not needed.

### Site

jv.c `jv_string_concat` (0x1275a in the part-3 binary, scratchpad jq.dis):

```c
jv jv_string_concat(jv a, jv b) {
  a = jvp_string_append(a, jv_string_value(b), jvp_string_length(jvp_string_ptr(b)));
  jv_free(b);
  return a;
}
```

```
127b6  lea 8(a5),a2          ; a2 = &a
127ba  move.l (a2)+,(a0)+    ; x4: push a by value; a2 is now 24(a5) = &b
127c2  movea.l a2,a0         ; sret pointer for jvp_string_append = &b  (WRONG, should be &a)
127c4  jsr jvp_string_append
127da  jsr jv_free           ; frees 24(a5) = the RESULT
127e0  move.l (a2)+,(a0)+    ; returns 24(a5) = the freed result
```

So every string concat returns a string whose only reference was just dropped. Grow branch: new block (len 1, alloc 32 for `"" + "x"` in the QQString rule) is freed at once (= part 3's free at `_jv_string_concat+0x84`). In-place branch: `a` itself is freed, real `b` leaks. constant_fold then gen_const()s that dead pointer, the LOADK holds it, and gen_const_array's jv_copy is the late inc on a freed block. Second site, same bug: `jv_string_append_codepoint` (128de): sret = 24(a5), which writes 12 bytes past its own args into the caller's frame (stack corruption, return value happens to be right). Whole-binary scan for "sret from a post-incremented reg": exactly these 2 sites, both calls to the static jvp_string_append.

### Cause (pass level)

Reduced repro, standalone (upterm-ports-jqprobe pkgs/jq/probe/sret-cse.c + .O2.s, commit 1c7c8d0 on probe/jq-heap): wrong at -O1, -O2, -Os; right at -O0. RTL dumps: expand is correct (copy uses pseudo 43 = argptr+8 with post_inc; sret a0 from pseudo 52 = argptr+8). cse1 replaces 52 with 43 as an equal expression, ignoring the post_inc side effects on 43 in the 4 copy insns. In the full jv.c, gcse's cprop does the same when cse is off. Root: the backend's by-pieces block move emits post_inc addressing at expand time (before the auto_inc_dec pass), and cse/cprop do not invalidate pseudos auto-modified that early. Not bbb (-fbbb=- no change).

### Fix proposed (NOT applied)

1. Root fix, in the toolchain (owner: toolchain agent): make the m68k by-pieces / movmem expansion not use post_inc on a pseudo copied from a shareable address before auto_inc_dec (e.g. USE_LOAD_POST_INCREMENT / USE_STORE_POST_INCREMENT 0 at expand, or a fresh non-CSE-able pseudo), or make cse/cprop invalidate REG_INC pseudos. Report upstream to bebbo/amiga-gcc with sret-cse.c. This bug hits every port that passes or returns structs by value, not only jq.
2. Interim, only if the toolchain fix is not soon: per-file flags for jq's jv.c `-fdisable-rtl-cse1 -fno-rerun-cse-after-loop -fno-gcse` (asm check: concat sret becomes `lea (8,a5),a0`; cse flags alone are NOT enough in the full TU). Unverified on the rig; would need the 20-run rig repro (no 0x15) and a whole-binary rescan.
3. Patching gen_const_array or constant_fold is a band-aid and is not proposed.

## Compiler fix (2026-10-16, opus agent, no rig run)

Root cause in gcc, one level below "cse ignores post_inc": bebbo's m68k movmem/setmem expanders (config/m68k/m68k.c "SBF: allocate tmp reg") mark the address-copy insn `(set (reg 43) (virtual-incoming-args))` with REG_INC, and cse.c/cprop.c (SBF hunks) skip recording such registers. function.c `instantiate_virtual_regs_in_insn`, the "straight copy from a virtual register" branch (function.c:1554-1577 on amiga6), replaces that insn with `(set (reg 43) (plus argptr 8))` and deletes the old one, dropping the note; cse1/cprop then treat reg 43 as argptr+8 after it was post-incremented. No upstream gcc commit exists (upstream has no pre-auto_inc_dec post_inc or REG_INC marker); fix is amiga6-only.

- Fix: /Users/spot/Code/UP-Term/amiga-gcc/projects/gcc (fresh clone, origin franke.ms/git/bebbo/gcc), branch fix/sret-autoinc-cse, commit 82f7243a5 on amiga6 8c1fd39db: move the REG_INC note to the last insn of the replacement sequence. Test gcc.c-torture/execute/sret-autoinc-1.c.
- Verified (vamos -C 68020, -noixemul): test aborts at -O1/-O2/-Os with the installed cc1 and with the fix reverted; passes -O0..-O3 with the fix. jq jv.c -O2: both sites `lea (8,a5),a0`; whole-file scan 2 bad sites before, 0 after. Not run: dejagnu, rig.
- cc1 built at scratchpad gccobj (not installed). PR draft: thoughts/shared/research/2026-10-16_gcc-sret-cse-pr.md.

Install and rebuild (owner, after the other agent stops using /Users/spot/opt/amiga):
1. The installed cc1 is from /Users/spot/Code/UP-Term/cpython-amiga/build/gcc/src branch amiga-cpython (amiga6 + btst, bbb opt_strcpy, fibonacci_heap clang fix). Cherry-pick 82f7243a5 onto that branch (or rebase those onto fix/sret-autoinc-cse), `make -C .../cpython-amiga/build/gcc/obj all-gcc`, back up the old cc1 as cc1.pre-sret-2026-10-16, copy the new cc1 to /Users/spot/opt/amiga/libexec/gcc/m68k-amigaos/6.5.0b/cc1. Installing the plain amiga6 build instead would drop those 3 patches.
2. Rebuild libgcc/libnix only if a whole-toolchain rebuild is done anyway (not proven affected; scan them with the same asm check first).
3. Rebuild every upterm-ports package (gcc 6 + ixemul; jq first, then the userland rig), and vtcon's AGCC-built programs (`make amiga`). ixemul-vtcon needs no rebuild: it is built with gcc 2.95.3 in docker. Drop the interim per-file flag plan (fix item 2 above).

## 2026-10-16: not reproduced after bf2f974

FP4.1 run on rig 2 (060): jq rebuilt after upterm-ports bf2f974 (dropped the single-threaded pthread patch; the ixemul SDK headers carry pthread) passed 19/19 cases in 5 of 5 runs, no requester. Candidate mechanism, unverified: the dropped patch declared its own `typedef int pthread_mutex_t` / `pthread_once_t`, sizes that may differ from what other objects saw, so a write through one layout could land on a neighbouring jq_state field. Reopen with the --wrap probe if the requester returns.
