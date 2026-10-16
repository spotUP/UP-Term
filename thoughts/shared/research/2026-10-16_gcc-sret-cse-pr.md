---
date: 2026-10-16
topic: PR draft for bebbo/gcc (amiga6) - REG_INC marker lost in instantiate_virtual_regs
status: submitted 2026-10-16 as https://codeberg.org/bebbo/gcc/pulls/13
branch: fix/sret-autoinc-cse in /Users/spot/Code/UP-Term/amiga-gcc/projects/gcc, commit 82f7243a5 on amiga6 8c1fd39db
---

# PR title

Keep the REG_INC marker when instantiating a virtual-register copy

# PR body

## Bug

A function that passes a struct by value and calls a function returning a
struct writes the call's result to the wrong place at -O1, -O2, -Os and -O3
(-O0 is right). Found in jq 1.7.1 (`jv_string_concat`,
`jv_string_append_codepoint`): every string concat returned a string whose
only reference had just been freed.

## Repro

```c
typedef struct { unsigned char kind, pad; unsigned short off; int size;
                 union { void *ptr; double n; } u; } jv;
jv app(jv s, const char *d, unsigned len);
const char *val(jv s);
unsigned len_of(jv s);
void jfree(jv s);
jv concat(jv a, jv b) {
  a = app(a, val(b), len_of(b));
  jfree(b);
  return a;
}
```

`m68k-amigaos-gcc -m68020 -O2 -S`:

```
	lea (8,a5),a2
	move.l (a2)+,(a0)+     ; x4, copy of a for the call
	move.l a2,a0           ; struct-return pointer = &b, should be &a
	jsr _app
```

With the fix: `lea (8,a5),a0` before `jsr _app`.

## Root cause

The m68k movmem/setmem expanders (config/m68k/m68k.c, "SBF: allocate tmp
reg") copy the block address into a fresh pseudo, put a REG_INC note on that
copy and then use the pseudo with post_inc. cse.c (cse_insn, "SBF: ignore
regs marked as REG_INC") and cprop.c skip recording an equivalence for a
register whose setter carries the note.

For an incoming by-value argument the copy is
`(set (reg 43) (reg virtual-incoming-args))` with the note.
`instantiate_virtual_regs_in_insn` (function.c, the "straight copy from a
virtual register" case) replaces that insn by a new
`(set (reg 43) (plus argptr 8))` and deletes the old one, so the note is
dropped. cse1 then records reg 43 == argptr+8 and substitutes it for the
struct-return address `(plus argptr 8)` after the four post_inc moves
(dumps: insn 33 has REG_INC in .expand, its replacement insn 64 has none in
.vregs; insn 39 `a0 = reg 52` becomes `a0 = reg 43` in .cse1). gcse cprop
does the same in larger functions.

Upstream gcc has no equivalent fix: upstream does not emit post_inc before
the auto_inc_dec pass and has no REG_INC marker convention, so this is
specific to the amiga6 tree. gcse.c already moves the note in the same way
when it replaces an insn ("SBF: move REG_INC note").

## Fix

In `instantiate_virtual_regs_in_insn`, when the replaced copy carried a
REG_INC note for its destination, add it to the last insn of the new
sequence (the one that now sets the register).

## Test

`gcc.c-torture/execute/sret-autoinc-1.c`, built `-noixemul -m68020` and run
under vamos (`-C 68020`):

| | -O0 | -O1 | -O2 | -Os | -O3 |
|---|---|---|---|---|---|
| without the fix | pass | abort | abort | abort | n/a |
| with the fix | pass | pass | pass | pass | pass |

jq 1.7.1 jv.c (`-m68020 -O2`): the struct-return pointer for both
`jvp_string_append` calls is now `lea (8,a5),a0`; a scan of the whole file
for "post-incremented register copied to a0" finds 2 sites before, 0 after.

Not run: the dejagnu testsuite (no m68k simulator board configured here).
