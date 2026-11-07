#ifndef AMARCHY_STKPROBE_H
#define AMARCHY_STKPROBE_H

/* Stack high-water probe: a measuring tool, off by default.
 *
 * `make STKPROBE=1 <targets>` adds -DAMDBG_STK and links stkprobe.c. stk_probe_init("AMIDX") at the top of main
 * fills the unused stack below the current SP with 0xA5 and publishes <prefix>_STK_LO / _HI / _SIZE as global
 * variables; stk_probe() scans for the first byte that is no longer 0xA5 and publishes <prefix>_STK_USED
 * (bytes from the top of the stack) whenever the maximum grows. Read them on the machine with `GetEnv AMIDX_STK_USED`.
 * One prefix per program, so two probes never share a variable.
 *
 * Without the knob both calls are empty macros and STK_BYTES keeps the production stack, so the binary is
 * byte-identical to a build that never heard of the probe. With it STK_BYTES is a measuring stack of 131072,
 * so a program that overruns its production stack still finishes and reports how far it went. */

#ifdef AMDBG_STK
void stk_probe_init(const char *prefix);
void stk_probe(void);
#define STK_BYTES(n) 131072
#else
#define stk_probe_init(prefix) ((void)0)
#define stk_probe() ((void)0)
#define STK_BYTES(n) n
#endif

/* `long __stack = STK_BYTES(16384);` and `const char x[] = STK_COOKIE(16384);` keep the two values side by side */
#define STK_STR2(x) #x
#define STK_STR(x) STK_STR2(x)
#define STK_COOKIE(n) "$STACK: " STK_STR(STK_BYTES(n))

#endif
