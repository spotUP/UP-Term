# W8a rerun (s66): legw on KS 3.2 with real ports kit
Verdict: NOT RUN (blocked at kit build; no rig started, no install attempted).

Step: `nice -n 19 make dist-base` in vtcon. Everything vtcon-side built (jbmono, bebbossh OK), then PORTS_KIT_FRESH (Makefile:1969-1978) died:
  dist: /Users/spot/Code/UP-Term/upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports
Evidence: upterm-ports/build/kit/userland dated 4 Nov 08:02, has no STAGED-FROM file; ports HEAD is 93d1fb3; build/state holds only 50 files
(bzip2 built/checked ...); staged bin has 21 tools (bsdcat..xz, upkg, upkg-ui) and NO grep, although pkgs/grep exists.
So the existing "real" kit is an old/partial stage, the same class as the stub kit that gave "grep: not found" in s64.

Needed (outside my brief: upterm-ports is read-only for me, and kit-stage depends on KIT_PKGS = a build of the ports):
  cd /Users/spot/Code/UP-Term/upterm-ports && make kit-stage   (nice 19; builds/stamps all pkgs incl. grep, writes STAGED-FROM)
then: cd vtcon && nice -n 19 make dist-base
then: python3 tools/rig/rig.py start --os 32 --max --purpose "up-term-bb s66: W8a KS 3.2 legw install"  (free numbers: not 3,63,64,66,71)
      python3 tools/rig/install_rig.py --only legw   (against that rig; check how rig number is selected, tools/rig/paths.py RIG_N)
Not examined: install.dos ~569 / unstartup.sh cause (no new run evidence). Cleanup: nothing to stop; scratch log only (small, not deleted).
