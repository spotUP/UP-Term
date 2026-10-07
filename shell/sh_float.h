/* printf's floating conversions (%f %F %e %E %g %G) without a floating-point
 * unit, libm or libc formatting: the argument is read into an IEEE double
 * (sign, 53-bit mantissa, binary exponent) and printed from its exact value
 * with small multi-limb integers, rounded half to even as C's printf does. */
#ifndef SH_FLOAT_H
#define SH_FLOAT_H

#define SH_FLOAT_MAXPREC 400
#define SH_FLOAT_BODY 1100   /* a body buffer's size */

/* Format arg (a number as printf reads one: decimal, 0x integer, inf, nan, or
 * 'c for a character's value; empty means 0) by conversion spec into body,
 * NUL-ended. *pl is the length of the sign in front (zero padding goes after
 * it); *zero is 0 where zero padding must not be used (inf, nan). Returns 1
 * when arg is not a whole number, 2 when it is out of the double's range
 * (the value is still formatted), else 0. */
int sh_float_format(const char *arg, int spec, int plus, int space, int alt, long prec,
                    char *body, int *pl, int *zero);

#endif
