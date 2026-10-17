# cexp missing in ixemul libm (2026-10-17)

Cause: gcc -O2 fuses sin(x)+cos(x) into sincos -> cexpi -> a call to cexp; ixemul-vtcon's compat/libm.a (what -lm finds in ixemul/lib) had no cexp.
Fix: ixemul-vtcon/compat/Makefile LIBM_NEWLIB gains cexp cexpf cexpl cimag cimagf cimagl creal crealf creall (newlib libm.a members, only reference exp/sin/cos and each other, no errno; libm_link and selfcall pass). `make -C compat libm.a && make -C compat install` put it in ~/opt/amiga/m68k-amigaos/ixemul/lib.
Test: vtcon tests/test_ixemul_sincos.py + tests/ixemul_sincos.c, `make test ONLY=ixemul_sincos`. Before the fix the link failed with `undefined reference to cexp` (reproduced on the old libm); after: links for default and -m68020 (-m68881 inlines fsincos, no cexp). Run on rig 1 (ami.py put + exec): PASS (fused sin/cos == separate calls).
cpython-amiga: workaround (CPLX_FUNCS/NEWLIBM copy into libnewlibwcs.a) removed; build/m68k/newlibwcs was stale, rm -rf it; `make compat m68k` relinks python.exe, cexp/cimag/creal now come from libm. Python not run on the rig after the relink (open).
Note: the installed libm.a is a host-side install; other machines need `make -C compat install`.
