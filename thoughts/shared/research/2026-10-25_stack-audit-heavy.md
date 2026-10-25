# Stack audit, heavy code paths (2026-10-25)

Method: `stkrun` helper (below) runs a program on a 256 KB stack pre-filled with 0xA5 and reports the high-water mark. One rig (rig 6, `UPTERM_RIG=6`), binaries from `build/amiga/` (dated 25 Okt 13:48, not rebuilt; freshness against source not verified). Peak includes startup and dos.library calls on the caller stack, not interrupts. Validity: `stackprobe` under stkrun reports 262144, and a `__stack = 16384` build also reports 262144, so vbcc startup does not swap stacks and the measurement sees real use.

| Program | Heavy path exercised | Peak bytes | Current | Headroom | Verdict |
|---|---|---|---|---|---|
| uptelnet | live session to host uptelnetd, password login, 30 typed lines of ~60 chars echoed, Ctrl-] (refused connect only: 976) | 1622 | 16384 | 10.1x | OK |
| UPTerm | window open and scrolled after `List SYS: ALL`; `help`, `state`, `cursor bar`, `cursor block` | 884 | 16384 | 18.5x | OK |
| UPConsole | `STATUS` 1288; `CON ON HANDLER VTC:vtcon-handler`, `DEVICE ON/OFF`, `EXCLUDE foo`, `EXCLUDE CLEAR`, `CON OFF` (880 each) | 1288 | 16384 | 12.7x | OK (handler process stack not measured) |
| top | interactive in XCON window, ~8 s redraws, `q` (batch 846) | 886 | 16384 | 18.5x | OK |
| UPDemo | XCON 700x400, all 24 scenes stepped (`b`, 26 spaces at 3 s), `q` | 928 | 16384 | 17.7x | OK |
| upicon | `RAM:ic WINDOWDEV=XCON`, `TOOL=UP-Term:bin/vsh`, `WINDOWDEV=CON` (icon written); same as usage path | 1172 | 16384 | 14.0x | OK |
| UP-Term Prefs | whole `prefs_rig.ui_main()`: every gadget on all pages, asl requesters (screen mode, font, fallback font, theme), Save/Use/Cancel, profile New/Rename/Delete, live colour; 62 PASS | 4064 | 16384 | 4.0x | OK (2x peak = 8128, would round to 8192) |
| rz / sz | NOT MEASURED (see below). Earlier idle audit: rz 2658, sz 3186 | n/a | 16384 | about 5x on idle figure | OK on idle figure, heavy path unverified |
| Reboot | NOT RUN (static estimate only; earlier audit: MAXVOL 32 table + packet, under 4 KB) | est. <4096 | 16384 | >4x | OK (estimate) |

No program needs RAISE. Rule used: RAISE to next 4096 multiple >= 2x peak when 2x peak exceeds current size.

## Blocked measurement: rz / sz

`$S/stk/zm_stk.py` (zmodem_rig wiring: serial wire, upgetty + vsh, host lrz/lsz, 300 KB payload, sz/rz under stkrun) never saw the vsh prompt (`prompt False` within 30 s), so no transfer ran. Cause not diagnosed (not the helper; likely PTY/getty start-up under rig 6, which differs from the rig used by tools/rig/zmodem_rig.py). Repeat by running `UPTERM_RIG=6 python3 tools/rig/zmodem_rig.py` first to confirm the wiring, then wrapping sz/rz with `VTC:stkrun RAM:z1.txt VTC:sz VTC:zm-send.bin` and `VTC:stkrun RAM:z2.txt VTC:rz`.

## Repeat commands

```
# build helper
vasmm68k_mot -Fhunk -o runon.o runon.s
vc +build/vbcc-aos68k.cfg -Ivendor/ndk-3.2r4-Include_H -cpu=68020 -O2 -o stkrun stkrun.c runon.o
# rig
UPTERM_RIG=6 python3 tools/rig/rig.py start --max
UPTERM_RIG=6 python3 tools/rig/rig.py install     # then copy stkrun and binaries into build/rig6/vtc/ (VTC:)
# in the guest (or XCON window via tools/rig typewin.open_xcon):
VTC:stkrun RAM:out.txt VTC:<prog> <args>     # then: Type RAM:out.txt  ->  prog <p> rc <rc> peak <n>
# Prefs: monkeypatch prefs_rig.start_app to 'run >NIL: VTC:stkrun RAM:s6.txt VTC:upprefs', then prefs_rig.ui_main()
# telnet: tools/uptelnetd.py on 127.0.0.1, then VTC:stkrun RAM:s2.txt VTC:uptelnet 127.0.0.1 <port> NOAGENT KEY RAM:upt-a.key
UPTERM_RIG=6 python3 tools/rig/rig.py stop
```

Args reach the program's ReadArgs by pushing the arg line back onto Input() with UnGetC (SetVBuf on Input broke this in XCON windows).

Driver scripts (not in repo): `$S/stk/stkmeasure.py`, `$S/stk/zm_stk.py`, with `$S` = /private/tmp/claude-501/-Users-spot-Code-UP-Term/5983afe9-3b58-421c-8142-35583cf17ab1/scratchpad.

## runon.s

```
	; long runon(entry, args, len, sss)  -- sss: {lower, upper, pointer, rc}
	; runs entry(D0=len, A0=args) on the stack in sss; stores rc in sss+12
	section code,code
	xdef _runon
_runon:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a3
	move.l	52(sp),a4
	move.l	56(sp),d2
	move.l	60(sp),a2
	move.l	4.w,a6
	move.l	a2,a0
	jsr	-732(a6)		; StackSwap: now on the new stack
	move.l	a2,-(sp)		; keep sss on the new stack
	move.l	d2,d0
	move.l	a4,a0
	jsr	(a3)
	move.l	(sp)+,a2
	move.l	d0,12(a2)
	move.l	4.w,a6
	move.l	a2,a0
	jsr	-732(a6)		; back on the old stack
	movem.l	(sp)+,d2-d7/a2-a6
	rts
```

## stkrun.c

```c
/* stkrun OUTFILE PROGRAM [args...]: run PROGRAM (LoadSeg) on a 256 KB stack filled 0xA5, write the peak to OUTFILE */
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <dos/stdio.h>
#define SZ (256*1024)
struct SSS { APTR lower, upper, ptr; long rc; };
extern long runon(void *entry, char *args, long len, struct SSS *sss);
int main(void)
{
    char buf[512], out[128], prog[256], *p, *a;
    struct SSS sss; unsigned char *mem; long i, n, rc, peak; BPTR seg, f; unsigned long *entry;
    { struct RDArgs *ra; LONG av[3] = {0,0,0};
      /* template: OUT/A,PROG/A,ARGS/F */
      ra = ReadArgs("OUT/A,PROG/A,ARGS/F", av, 0);
      if (!ra) { PutStr("stkrun OUT PROG [ARGS]\n"); return 20; }
      strcpy(out, (char *)av[0]); strcpy(prog, (char *)av[1]);
      strcpy(buf, av[2] ? (char *)av[2] : ""); strcat(buf, "\n");
      FreeArgs(ra); }
    { BPTR in = Input(); long k;   /* ReadArgs reads the argument line from Input(): push it back as the shell does */
      for (k = (long)strlen(buf) - 1; k >= 0; k--) UnGetC(in, buf[k]); }
    seg = LoadSeg(prog);
    if (!seg) { PutStr("LoadSeg failed\n"); return 20; }
    mem = AllocMem(SZ, MEMF_PUBLIC);
    if (!mem) return 20;
    memset(mem, 0xA5, SZ);
    sss.lower = mem; sss.upper = mem + SZ; sss.ptr = mem + SZ; sss.rc = -1;
    entry = (unsigned long *)(((unsigned long)seg << 2) + 4);
    rc = runon(entry, buf, (long)strlen(buf), &sss);
    for (i = 1024; i < SZ && mem[i] == 0xA5; i++) ;
    peak = SZ - i;
    f = Open(out, MODE_NEWFILE);
    if (f) { FPrintf(f, "prog %s rc %ld peak %ld\n", prog, sss.rc, peak); Close(f); }
    FreeMem(mem, SZ); UnLoadSeg(seg);
    return 0;
}
```
