/* FPU infinity and NaN semantics under the emulator (FS-UAE 3.1.66 arm64 native FPU got
   Inf wrong: fmove.x stored 3fff/ffffffff.. and fcmp Inf,Inf set NAN not Z, which made
   ixemul's Inf==Inf false).  Raw 68881 instructions, no libm.  Prints one line per case and
   [OK] fpuinf or [FAIL] fpuinf; exit code 0 only when all hold. */
#include <stdio.h>
#include <string.h>

typedef unsigned long u32;
#define FPSR_NAN (1UL << 24)
#define FPSR_Z   (1UL << 26)

static volatile unsigned long pinf[2] = {0x7ff00000, 0};
static volatile unsigned long ninf[2] = {0xfff00000, 0};
static volatile unsigned long qnan[2] = {0x7ff80000, 0};
static volatile double one = 1.0;

/* FPSR after fcmp.x fp1,fp0 with fp0 = *a, fp1 = *b (doubles in memory) */
static u32 cmp(volatile void *a, volatile void *b)
{
	u32 s;
	__asm__ volatile("fmove.d (%1),%%fp0\n\t fmove.d (%2),%%fp1\n\t fmove.l #0,%%fpsr\n\t"
			 "fcmp.x %%fp1,%%fp0\n\t fmove.l %%fpsr,%0"
			 : "=d"(s) : "a"(a), "a"(b) : "fp0", "fp1", "cc");
	return s;
}

static void store(volatile void *a, unsigned char *b)
{
	__asm__ volatile("fmove.d (%0),%%fp0\n\t fmove.x %%fp0,(%1)" :: "a"(a), "a"(b) : "fp0", "memory");
}

static int fails;

static void check(const char *name, int good, u32 fpsr)
{
	printf("%s %s (FPSR=%08lx)\n", good ? "ok  " : "FAIL", name, fpsr);
	fails += !good;
}

static void bytes(const char *name, volatile void *a, const unsigned char *want)
{
	unsigned char b[12];
	int i;
	memset(b, 0xAA, sizeof b);
	store(a, b);
	printf("%s %s bytes:", memcmp(b, want, 12) == 0 ? "ok  " : "FAIL", name);
	for (i = 0; i < 12; i++)
		printf(" %02x", b[i]);
	printf("\n");
	fails += memcmp(b, want, 12) != 0;
}

int main(void)
{
	static const unsigned char pinfx[12] = {0x7f, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	static const unsigned char ninfx[12] = {0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	u32 s;

	bytes("fmove.x +Inf", pinf, pinfx);
	bytes("fmove.x -Inf", ninf, ninfx);
	s = cmp(pinf, pinf);
	check("fcmp +Inf,+Inf sets Z, not NAN", (s & FPSR_Z) && !(s & FPSR_NAN), s);
	s = cmp(ninf, ninf);
	check("fcmp -Inf,-Inf sets Z, not NAN", (s & FPSR_Z) && !(s & FPSR_NAN), s);
	s = cmp(pinf, ninf);
	check("fcmp +Inf,-Inf clears Z", !(s & FPSR_Z) && !(s & FPSR_NAN), s);
	s = cmp((volatile void *)&one, (volatile void *)&one);
	check("fcmp 1.0,1.0 sets Z (control)", (s & FPSR_Z) != 0, s);
	s = cmp(qnan, (volatile void *)&one);
	check("fcmp NaN,1.0 unordered (NAN set, Z clear)", (s & FPSR_NAN) && !(s & FPSR_Z), s);
	s = cmp(qnan, qnan);
	check("fcmp NaN,NaN unordered (NAN set, Z clear)", (s & FPSR_NAN) && !(s & FPSR_Z), s);
	printf("%s fpuinf\n", fails ? "[FAIL]" : "[OK]");
	return fails != 0;
}
