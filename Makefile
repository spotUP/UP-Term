# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
BUILD   := build

ENGINE  := engine/vtengine.c
RENDER  := render/glyphmap.c handler/lineedit.c
SHELL_CORE := shell/sh_parse.c shell/sh_expand.c shell/sh_exec.c
TTY     := tty/ldisc.c
DEVICE_CORE := device/upc_core.c
CONF    := config/upconf.c
PREFS_CORE := prefs/prefs_core.c
ICONSPEC := install/iconspec.c
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_pcansi.c tests/test_glyph.c tests/test_mirror.c tests/test_lineedit.c \
           tests/test_sh_parse.c tests/test_sh_expand.c tests/test_sh_exec.c tests/test_ldisc.c \
           tests/test_upcon.c tests/test_upconf.c tests/test_prefs.c tests/test_iconspec.c

.PHONY: test test-ref te-diff test-terminfo test-rig dist golden vttest venv capture quirks amiga clean

test: $(BUILD)/vttest_host
	./$(BUILD)/vttest_host $(ONLY)

$(BUILD)/vttest_host: $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(CONF) $(PREFS_CORE) $(ICONSPEC) install/iconspec.h device/upc_core.h config/upconf.h prefs/prefs_core.h tty/ldisc.h shell/sh_parse.h shell/sh_expand.h shell/sh_exec.h engine/vtengine.h engine/vtwidth.h render/glyphmap.h handler/lineedit.h render/glyph_tables.inc $(TESTS) tests/harness.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(CONF) $(PREFS_CORE) $(ICONSPEC) $(TESTS)

render/glyph_tables.inc: tools/gen_glyph_tables.py engine/vtengine.c
	python3 tools/gen_glyph_tables.py

$(BUILD)/vtdump: $(ENGINE) engine/vtengine.h engine/vtwidth.h tests/dump_main.c
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) tests/dump_main.c

# libvterm (neovim's terminal) as the reference, built from source into build/.
LIBVTERM := $(BUILD)/third_party/libvterm
$(LIBVTERM)/src/vterm.c:
	@mkdir -p $(BUILD)/third_party
	git clone -q --depth 1 https://github.com/neovim/libvterm.git $(LIBVTERM)

$(BUILD)/vterm_dump: tools/vterm_dump.c $(LIBVTERM)/src/vterm.c
	$(HOSTCC) -O1 -std=c99 -I$(LIBVTERM)/include -I$(LIBVTERM)/src -o $@ tools/vterm_dump.c $(LIBVTERM)/src/*.c

# The xterm personality against libvterm and pyte on tests/streams (ONLY= a name part).
# pcansi against DCTelnet's term-engine.c on BBS art, pixel for pixel
# (tools/te_diff). Needs a DCTelnet checkout and an art directory.
DCTELNET ?= $(HOME)/Code/dctelnet-v2
ART ?= $(HOME)/Code/amiexpress-doorserver/bbs_ads
$(BUILD)/te_diff: tools/te_diff/te_diff.c tools/te_diff/te_shim.h engine/vtengine.c engine/vtengine.h engine/vtwidth.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=gnu99 -O1 -g -w -Itools/te_diff -I$(DCTELNET)/src/third_party/retro32-term \
		tools/te_diff/te_diff.c engine/vtengine.c -o $@
te-diff: $(BUILD)/te_diff
	find $(ART) -type f -iname '*.ans' -print0 | xargs -0 $(BUILD)/te_diff | grep -v ' 0 cells differ'

test-ref: $(BUILD)/vtdump $(BUILD)/vterm_dump
	.venv/bin/python tools/refdiff.py $(ONLY)

# Rewrite the golden grids of the XTERM_NOT_LIBVTERM cases (review the diff).
golden: $(BUILD)/vtdump
	@mkdir -p tests/golden
	.venv/bin/python tools/refdiff.py --write-golden $(ONLY)

venv:
	python3 -m venv .venv && .venv/bin/pip install pyte

# Regenerate the hand-written edge-case streams (tests/streams/quirk-*).
quirks:
	python3 tools/quirks.py

# vttest (Dickey) from source into build/third_party, for `make capture`.
vttest:
	@mkdir -p $(BUILD)/third_party
	cd $(BUILD)/third_party && curl -sSfL -o vttest.tgz https://invisible-island.net/datafiles/release/vttest.tar.gz \
	  && tar xzf vttest.tgz && cd vttest-* && ./configure -q && $(MAKE) -s

# terminfo/vtcon.terminfo compiled for the host programs of the capture.
$(BUILD)/terminfo/76/vtcon: terminfo/vtcon.terminfo
	@mkdir -p $(BUILD)/terminfo
	tic -x -o $(BUILD)/terminfo terminfo/vtcon.terminfo
	@# both hashed layouts: newer ncurses reads 76/ (hex), macOS's and screen 4.00 read v/
	@mkdir -p $(BUILD)/terminfo/v $(BUILD)/terminfo/76
	@for d in 76 v; do [ -f $(BUILD)/terminfo/$$d/vtcon ] && cp $(BUILD)/terminfo/$$d/vtcon $(BUILD)/terminfo/76/vtcon.tmp && break; done
	@cp $(BUILD)/terminfo/76/vtcon.tmp $(BUILD)/terminfo/v/vtcon; mv $(BUILD)/terminfo/76/vtcon.tmp $(BUILD)/terminfo/76/vtcon

# The kit's terminal entries: vtcon and GNU screen's, in first-letter
# directories (v/vtcon): the Amiga's ncurses 5.5 reads those, tic on macOS
# writes hex ones (76/).
KIT_TERMINFO := terminfo/vtcon.terminfo terminfo/screen.terminfo
$(BUILD)/kit-terminfo/stamp: $(KIT_TERMINFO)
	rm -rf $(BUILD)/kit-terminfo $(BUILD)/kit-terminfo.tmp
	mkdir -p $(BUILD)/kit-terminfo $(BUILD)/kit-terminfo.tmp
	for f in $(KIT_TERMINFO); do tic -x -o $(BUILD)/kit-terminfo.tmp $$f; done
	cd $(BUILD)/kit-terminfo.tmp && for p in */*; do n=$${p#*/}; c=$$(printf %s "$$n" | cut -c1); \
	  mkdir -p ../kit-terminfo/$$c && cp "$$p" ../kit-terminfo/$$c/; done
	rm -rf $(BUILD)/kit-terminfo.tmp
	touch $@

# GNU screen for the kit (P7.1): built in ~/Code/screen-amiga/src (make -f Makefile.amiga)
SCREEN_BIN ?= $(HOME)/Code/screen-amiga/src/screen
# tmux for the kit (P7.2): built in ~/Code/tmux-amiga (make -f Makefile.amiga)
TMUX_BIN ?= $(HOME)/Code/tmux-amiga/build/tmux-bin

# Recapture the programs with TERM=vtcon (tests/streams/ti-*), then check
# them: libvterm cell for cell and no sequence the engine ignored.
test-terminfo: $(BUILD)/terminfo/76/vtcon $(BUILD)/vtdump $(BUILD)/vterm_dump
	.venv/bin/python tools/capture.py vtcon
	.venv/bin/python tools/refdiff.py ti-

# Re-record tests/streams/ from real programs (vim, less, bash, ls, top).
capture:
	.venv/bin/python tools/capture.py

# --- Amiga (vbcc, NDK 3.2; the same setup as DCTelnet) -----------------------
VBCC_CFG ?= $(CURDIR)/tools/vbcc-aos68k.cfg
CPU      ?= 68020
# The AmigaOS 3.2 SDK headers. vendor/ is gitignored (4.1 MB of third-party
# headers), so unpack NDK3.2R4 there once, or point this at your own copy:
#   make amiga VTCON_NDK=~/Code/dctelnet-petscii-recovered/.ndk/Include_H
VTCON_NDK ?= $(CURDIR)/vendor/ndk-3.2r4-Include_H
VC       := vc +$(VBCC_CFG) -I$(VTCON_NDK) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors

GITREV  := $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet 2>/dev/null || echo -dirty)
HANDLER_SRC := handler/vtcon_handler.c handler/clip.c handler/lineedit.c handler/complete.c handler/brk.c $(ENGINE) render/amiga_render.c render/vtwin.c render/glyphmap.c tty/ldisc.c config/upconf.c
HANDLER_HDR := engine/vtengine.h engine/vtwidth.h render/amiga_render.h render/vtwin.h render/glyphmap.h render/glyph_tables.inc \
               handler/clip.h handler/lineedit.h handler/complete.h handler/brk.h handler/vtcon_packets.h tty/ldisc.h device/upc_public.h config/upconf.h

amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/up-console.device $(BUILD)/amiga/UPConsole $(BUILD)/amiga/pty-handler $(BUILD)/amiga/reach $(BUILD)/amiga/vtshow $(BUILD)/amiga/winbox $(BUILD)/amiga/sizewatch $(BUILD)/amiga/breakport $(BUILD)/amiga/ttyprobe $(BUILD)/amiga/ptytest $(BUILD)/amiga/ixkill $(BUILD)/amiga/vsh $(BUILD)/amiga/ixpipe-handler $(BUILD)/amiga/upprefs $(BUILD)/amiga/upicon

# The reachability probe (ledger V3), an ordinary program with vbcc's startup.
$(BUILD)/amiga/reach: tests/amiga/reach.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/reach.c

# vsh: the portable core (host-tested) and the AmigaDOS side. vbcc warns
# (153) on the (void) parameter casts the host compiler needs, and (65) on
# the parameters they are for.
VSH_SRC := shell/vsh.c shell/sh_exec.c shell/sh_expand.c shell/sh_parse.c
$(BUILD)/amiga/vsh: $(VSH_SRC) shell/sh_exec.h shell/sh_expand.h shell/sh_parse.h handler/vtcon_packets.h tty/ldisc.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -dontwarn=153,65 $(if $(DEBUG),-DVSH_DEBUG) -o $@ $(VSH_SRC)

$(BUILD)/amiga/vtshow: tests/amiga/vtshow.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/vtshow.c

$(BUILD)/amiga/winbox: tests/amiga/winbox.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/winbox.c

$(BUILD)/amiga/sizewatch: tests/amiga/sizewatch.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/sizewatch.c

$(BUILD)/amiga/breakport: tests/amiga/breakport.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/breakport.c

# the Prefs editor: the profiles in ENV: / ENVARC:up-term/up-term, two pages
# of GadTools gadgets; its model (prefs_core) is host-tested. A plain CLI
# program (its own window), so vc links it. vbcc warns (153, 65) on the
# (void) parameter casts of the file callbacks, as for vsh.
# The kit ships it as "UP-Term Prefs" (make cannot hold a space in a target).
$(BUILD)/amiga/upprefs: prefs/upprefs.c prefs/prefs_core.c prefs/prefs_core.h config/upconf.c config/upconf.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -dontwarn=153,65 -o $@ prefs/upprefs.c prefs/prefs_core.c config/upconf.c

# ixemul programs: bebbo's gcc (thoughts plan: TOOLCHAIN), linked against
# Aminet's ixemul SDK (no -m68020: the SDK has no libm020 multilib)
AGCC ?= $(HOME)/opt/amiga/bin/m68k-amigaos-gcc
$(BUILD)/amiga/ptyprobe: tests/amiga/ptyprobe.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ptyprobe.c

# P6 part 4: BSD ptys through the patched ixemul on PTY: (tools/rig/ixpty_rig.py)
$(BUILD)/amiga/ixpty: tests/amiga/ixpty.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixpty.c

$(BUILD)/amiga/ixwait: tests/amiga/ixwait.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixwait.c

# GNU screen's backtick/printcmd/blanker/lock children (tools/rig/screen_rig.py)
$(BUILD)/amiga/forkprobe: tests/amiga/forkprobe.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/forkprobe.c

# a pipe into a vfork + exec child, ixemul and native (screen's printcmd)
$(BUILD)/amiga/ixpipeprobe: tests/amiga/ixpipeprobe.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixpipeprobe.c

# C99/POSIX additions to ixemul (library + libixcompat) for libevent/tmux
$(BUILD)/amiga/ixc99: tests/amiga/ixc99.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixc99.c -lixcompat

# a terminal's answer to a query is termios input (XCON: handler cb_reply)
$(BUILD)/amiga/ixreply: tests/amiga/ixreply.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixreply.c

# ixemul's malloc/free/realloc timings (the small-block cache)
$(BUILD)/amiga/ixmalloc: tests/amiga/ixmalloc.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixmalloc.c

$(BUILD)/amiga/ixbg: tests/amiga/ixbg.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixbg.c

# ixemul's IXPIPE: handler (its utils/ixpipe-handler.c): execve hands a
# pipe or socket to a native program through it (screen's printcmd to vsh)
IXEMUL_SRC ?= $(HOME)/Code/ixemul-vtcon
$(BUILD)/amiga/ixpipe-handler: $(IXEMUL_SRC)/utils/ixpipe-handler.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -I$(IXEMUL_SRC)/include -nostdlib -o $@ $< -lc

# vsh's signal helper (S8): an ixemul program, so bebbo's gcc
$(BUILD)/amiga/ixkill: shell/ixkill.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ shell/ixkill.c

# P8: the stack vsh gives a command (plain, and with a $STACK: cookie)
$(BUILD)/amiga/stackprobe: tests/amiga/stackprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/stackprobe.c
	$(VC) -DSTACK_COOKIE -o $(BUILD)/amiga/stackprobe50k tests/amiga/stackprobe.c

# the kit's icon tool (Install: the Shell icon opens UP-Term)
$(BUILD)/amiga/upicon: install/upicon.c install/iconspec.c install/iconspec.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ install/upicon.c install/iconspec.c

$(BUILD)/amiga/iconprobe: tests/amiga/iconprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/iconprobe.c

$(BUILD)/amiga/wbrun: tests/amiga/wbrun.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/wbrun.c

$(BUILD)/amiga/taskpath: tests/amiga/taskpath.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/taskpath.c

$(BUILD)/amiga/ixsock: tests/amiga/ixsock.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixsock.c

$(BUILD)/amiga/ptytest: tests/amiga/ptytest.c handler/vtcon_packets.h tty/ldisc.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/ptytest.c

$(BUILD)/amiga/ttyprobe: tests/amiga/ttyprobe.c handler/vtcon_packets.h tty/ldisc.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/ttyprobe.c

# UP-Term's console.device (console plan D1.6): upcon_rom.o first (the
# RomTag and the ROM forwards), no C startup, as the handler.
DEVICE_SRC := device/upcon_device.c device/upcon_unit.c device/upcon_input.c device/upc_core.c \
              render/vtwin.c render/amiga_render.c render/glyphmap.c handler/clip.c $(ENGINE)
DEVICE_HDR := device/upcon.h device/upc_public.h device/upc_core.h render/vtwin.h render/amiga_render.h render/glyphmap.h \
              render/glyph_tables.inc handler/clip.h engine/vtengine.h engine/vtwidth.h
DEVICE_FLAGS := DEBUG=$(DEBUG)
DEVICE_FLAGS_OLD := $(shell cat $(BUILD)/amiga/device.flags 2>/dev/null)
ifneq ($(DEVICE_FLAGS),$(DEVICE_FLAGS_OLD))
DEVICE_FORCE := FORCE
endif
$(BUILD)/amiga/up-console.device: device/upcon_rom.s $(DEVICE_SRC) $(DEVICE_HDR) $(DEVICE_FORCE)
	@mkdir -p $(BUILD)/amiga/devobj
	@echo '$(DEVICE_FLAGS)' > $(BUILD)/amiga/device.flags
	vasmm68k_mot -quiet -Fhunk -m68020 -o $(BUILD)/amiga/devobj/upcon_rom.o device/upcon_rom.s
	$(VC) $(if $(DEBUG),-DUPCON_DEBUG) -c -o $(BUILD)/amiga/devobj/upcon_device.o device/upcon_device.c
	$(VC) -dontwarn=65 $(if $(DEBUG),-DUPCON_DEBUG) -c -o $(BUILD)/amiga/devobj/upcon_unit.o device/upcon_unit.c
	$(VC) $(if $(DEBUG),-DUPCON_DEBUG) -c -o $(BUILD)/amiga/devobj/upcon_input.o device/upcon_input.c
	$(VC) -c -o $(BUILD)/amiga/devobj/upc_core.o device/upc_core.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/devobj/vtwin.o render/vtwin.c
	$(VC) -c -o $(BUILD)/amiga/devobj/amiga_render.o render/amiga_render.c
	$(VC) -c -o $(BUILD)/amiga/devobj/glyphmap.o render/glyphmap.c
	$(VC) -c -o $(BUILD)/amiga/devobj/clip.o handler/clip.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/devobj/vtengine.o $(ENGINE)
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/devobj/upcon_rom.o \
	  $(BUILD)/amiga/devobj/upcon_device.o $(BUILD)/amiga/devobj/upcon_unit.o $(BUILD)/amiga/devobj/upcon_input.o \
	  $(BUILD)/amiga/devobj/upc_core.o $(BUILD)/amiga/devobj/vtwin.o $(BUILD)/amiga/devobj/amiga_render.o \
	  $(BUILD)/amiga/devobj/glyphmap.o $(BUILD)/amiga/devobj/clip.o $(BUILD)/amiga/devobj/vtengine.o \
	  -L/opt/homebrew/opt/vbcc/targets/m68k-amigaos/lib -lvc -lamiga

# C:upgetty: a shell over the serial port through a PTY: pair (ledger T4)
$(BUILD)/amiga/upgetty: device/upgetty.c handler/vtcon_packets.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -dontwarn=153 -o $@ device/upgetty.c

# C:UPConsole: CON:/RAW: to UP-Term and back (console plan H5.4)
$(BUILD)/amiga/UPConsole: device/upconsole.c device/upc_public.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ device/upconsole.c

# DV6's signal-bit probe (tools/rig/soak_rig.py)
$(BUILD)/amiga/sigprobe: tests/amiga/sigprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/sigprobe.c

# DV3's clock (tools/rig/devspeed_rig.py)
$(BUILD)/amiga/stamp: tests/amiga/stamp.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/stamp.c

# DV4's probe (tools/rig/devverify_rig.py)
$(BUILD)/amiga/memprobe: tests/amiga/memprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/memprobe.c

# D4.3's patch (tools/rig/devctl_rig.py; test only)
$(BUILD)/amiga/patchcon: tests/amiga/patchcon.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/patchcon.c

# D1.3's copy probe (tools/rig/snip_rig.py)
$(BUILD)/amiga/snipprobe: tests/amiga/snipprobe.c handler/clip.c handler/clip.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/snipprobe.c handler/clip.c

# D3.2's probe (tools/rig/cudump_rig.py)
$(BUILD)/amiga/cudump: tests/amiga/cudump.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/cudump.c

# D2.3's probe (tools/rig/rkc_rig.py)
$(BUILD)/amiga/rkcprobe: tests/amiga/rkcprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/rkcprobe.c

# DV1's probe (tools/rig/condev_rig.py)
$(BUILD)/amiga/devwho: tests/amiga/devwho.c device/upc_public.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/devwho.c

# H5.6's probes (tools/rig/concon_rig.py)
$(BUILD)/amiga/conwho: tests/amiga/conwho.c handler/vtcon_packets.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/conwho.c
$(BUILD)/amiga/romprobe: tests/amiga/romprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/romprobe.c

# Phase DP of the console.device plan: DP1 input chain, DP3 DosList, DP4 ROM
# commands (run by tools/rig/<name>_rig.py)
CONPROBES := chainprobe dosnode cdprobe mediumprobe medshell autoprobe
$(CONPROBES:%=$(BUILD)/amiga/%): $(BUILD)/amiga/%: tests/amiga/%.c tests/amiga/probeout.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/$*.c

$(BUILD)/amiga/vtengine-$(CPU).o: $(ENGINE) engine/vtengine.h engine/vtwidth.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -c -o $@ $(ENGINE)

# The handler: no C startup (handler_entry is the first code), exec memory.
# Gotcha: with -O2, vbcc can loop forever on a source that has a compile
# error instead of reporting it; build with -O0 to see the error.
# Rebuild when the flags change (DEBUG=1, DIRECT=1 on or off): the last
# flags are read while make parses, and a difference forces the link (a
# stamp file's mtime could equal the binary's to the second and be ignored).
HANDLER_FLAGS := DEBUG=$(DEBUG) SERIAL=$(SERIAL) CPU=$(CPU) DIRECT=$(DIRECT)
HANDLER_FLAGS_OLD := $(shell cat $(BUILD)/amiga/handler.flags 2>/dev/null)
ifneq ($(HANDLER_FLAGS),$(HANDLER_FLAGS_OLD))
HANDLER_FORCE := FORCE
endif
FORCE:

$(BUILD)/amiga/vtcon-handler: $(HANDLER_SRC) $(HANDLER_HDR) $(HANDLER_FORCE)
	@mkdir -p $(BUILD)/amiga/obj
	@echo '$(HANDLER_FLAGS)' > $(BUILD)/amiga/handler.flags
	$(VC) $(if $(DEBUG),-DVTCON_DEBUG) $(if $(SERIAL),-DVTCON_SERIAL) -DVT_AMIGA_EXEC_ALLOC -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/handler.o handler/vtcon_handler.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/vtengine.o $(ENGINE)
	$(VC) $(if $(DIRECT),-DVTCON_DIRECT) -c -o $(BUILD)/amiga/obj/amiga_render.o render/amiga_render.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/vtwin.o render/vtwin.c
	$(VC) -c -o $(BUILD)/amiga/obj/glyphmap.o render/glyphmap.c
	$(VC) -c -o $(BUILD)/amiga/obj/clip.o handler/clip.c
	$(VC) -c -o $(BUILD)/amiga/obj/lineedit.o handler/lineedit.c
	$(VC) -c -o $(BUILD)/amiga/obj/complete.o handler/complete.c
	$(VC) -c -o $(BUILD)/amiga/obj/brk.o handler/brk.c
	$(VC) -c -o $(BUILD)/amiga/obj/ldisc.o tty/ldisc.c
	$(VC) -c -o $(BUILD)/amiga/obj/upconf.o $(CONF)
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/handler.o \
	  $(BUILD)/amiga/obj/vtengine.o $(BUILD)/amiga/obj/amiga_render.o $(BUILD)/amiga/obj/vtwin.o $(BUILD)/amiga/obj/glyphmap.o \
	  $(BUILD)/amiga/obj/clip.o $(BUILD)/amiga/obj/lineedit.o $(BUILD)/amiga/obj/complete.o \
	  $(BUILD)/amiga/obj/brk.o $(BUILD)/amiga/obj/ldisc.o $(BUILD)/amiga/obj/upconf.o \
	  -L/opt/homebrew/opt/vbcc/targets/m68k-amigaos/lib -lvc -lamiga

# PTY: (P5): pseudo-terminals on the same line discipline. No C startup.
PTY_FLAGS := DEBUG=$(DEBUG)
ifneq ($(PTY_FLAGS),$(shell cat $(BUILD)/amiga/pty.flags 2>/dev/null))
PTY_FORCE := FORCE
endif
$(BUILD)/amiga/pty-handler: handler/pty_handler.c $(PTY_FORCE) handler/brk.c handler/brk.h handler/vtcon_packets.h tty/ldisc.c tty/ldisc.h
	@mkdir -p $(BUILD)/amiga/obj/pty
	@echo '$(PTY_FLAGS)' > $(BUILD)/amiga/pty.flags
	$(VC) $(if $(DEBUG),-DPTY_DEBUG) -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/pty/pty_handler.o handler/pty_handler.c
	$(VC) -c -o $(BUILD)/amiga/obj/pty/brk.o handler/brk.c
	$(VC) -c -o $(BUILD)/amiga/obj/pty/ldisc.o tty/ldisc.c
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/pty/pty_handler.o \
	  $(BUILD)/amiga/obj/pty/brk.o $(BUILD)/amiga/obj/pty/ldisc.o \
	  -L/opt/homebrew/opt/vbcc/targets/m68k-amigaos/lib -lvc -lamiga

# The install kit: build/UP-Term.lha -- a drawer UP-Term with Install (an
# Installer script and its icon), Uninstall, README.txt and Files/ (the rest).
KIT := $(BUILD)/dist/UP-Term
# the patched ixemul (P6): built in ~/Code/ixemul-vtcon with sh docker/build.sh
IXEMUL_LIB ?= $(HOME)/Code/ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library
dist: amiga $(BUILD)/amiga/UPConsole $(BUILD)/amiga/up-console.device $(BUILD)/terminfo/76/vtcon $(BUILD)/kit-terminfo/stamp
	rm -rf $(BUILD)/dist && mkdir -p $(KIT)/Files/terminfo $(KIT)/Files/libs
	cd $(BUILD)/kit-terminfo && cp -R [a-z] $(CURDIR)/$(KIT)/Files/terminfo/
	cp $(SCREEN_BIN) $(KIT)/Files/screen
	cp dist/screenrc $(KIT)/Files/screenrc
	cp $(TMUX_BIN) $(KIT)/Files/tmux
	cp dist/tmux.conf dist/unstartup.sh $(KIT)/Files/
	cp $(IXEMUL_LIB) $(KIT)/Files/libs/ixemul.library
	python3 tools/ans2utf8.py art/up_rough_banner.ans $(KIT)/Files/banner
	python3 tools/mkicon.py $(KIT)/Files/UP-Term.info
	printf 'UP-Term: double-click the icon to open a terminal with vsh.\n' > $(KIT)/Files/UP-Term
	python3 tools/mkicon.py $(KIT)/Files/UP-Term-Prefs.info --tool "C:UP-Term Prefs" --plain
	printf 'UP-Term Prefs: edit the profiles in ENVARC:up-term/up-term.\n' > $(KIT)/Files/UP-Term-Prefs
	cp $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/UPConsole $(BUILD)/amiga/up-console.device $(BUILD)/amiga/pty-handler $(BUILD)/amiga/ixpipe-handler $(BUILD)/amiga/vsh $(BUILD)/amiga/ixkill dist/XCON dist/PTY dist/IXPIPE dist/install.dos dist/vshrc \
	  dist/up-term.conf $(KIT)/Files/
	mkdir -p $(KIT)/Files/themes && cp themes/*.conf themes/ATTRIBUTION.md $(KIT)/Files/themes/
	rm -rf $(KIT)/Files/coreutils && mkdir -p $(KIT)/Files/coreutils
	cp -R dist/gg/coreutils-5.2.1/bin dist/gg/coreutils-5.2.1/COPYING dist/gg/coreutils-5.2.1/SOURCE.txt dist/gg/coreutils-5.2.1/coreutils-5.2.1-src.tar.bz2 $(KIT)/Files/coreutils/
	cp $(BUILD)/amiga/upprefs "$(KIT)/Files/UP-Term Prefs"
	cp $(BUILD)/amiga/upicon $(KIT)/Files/upicon
	cp terminfo/vtcon.termcap $(KIT)/Files/termcap.vtcon
	# the top drawer: Install (the Installer script), Uninstall, README, Files
	cp dist/Install.installer $(KIT)/Install
	python3 tools/mkicon.py $(KIT)/Install.info --tool Installer --plain \
	  --tooltype APPNAME=UP-Term --tooltype MINUSER=AVERAGE --tooltype DEFUSER=AVERAGE
	cp dist/Uninstall dist/README.txt $(KIT)/
	python3 tools/mkicon.py $(KIT)/Uninstall.info --tool C:IconX --plain
	cd $(BUILD)/dist && rm -f ../UP-Term.lha && lha -aq ../UP-Term.lha UP-Term
	@ls -la $(BUILD)/UP-Term.lha

# The one reachability test: XCON: through DOS on the running rig.
test-rig: amiga
	python3 tools/rig/reach.py

clean:
	rm -rf $(BUILD)
