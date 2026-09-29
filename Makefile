# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
BUILD   := build

ENGINE  := engine/vtengine.c
RENDER  := render/glyphmap.c handler/lineedit.c
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_pcansi.c tests/test_glyph.c tests/test_mirror.c tests/test_lineedit.c

.PHONY: test test-ref te-diff test-terminfo test-rig dist golden vttest venv capture quirks amiga clean

test: $(BUILD)/vttest_host
	./$(BUILD)/vttest_host $(ONLY)

$(BUILD)/vttest_host: $(ENGINE) $(RENDER) engine/vtengine.h render/glyphmap.h handler/lineedit.h render/glyph_tables.inc $(TESTS) tests/harness.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) $(RENDER) $(TESTS)

render/glyph_tables.inc: tools/gen_glyph_tables.py engine/vtengine.c
	python3 tools/gen_glyph_tables.py

$(BUILD)/vtdump: $(ENGINE) engine/vtengine.h tests/dump_main.c
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
$(BUILD)/te_diff: tools/te_diff/te_diff.c tools/te_diff/te_shim.h engine/vtengine.c engine/vtengine.h
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
VC       := vc +$(VBCC_CFG) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors

GITREV  := $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet 2>/dev/null || echo -dirty)
HANDLER_SRC := handler/vtcon_handler.c handler/clip.c handler/lineedit.c handler/complete.c $(ENGINE) render/amiga_render.c render/glyphmap.c
HANDLER_HDR := engine/vtengine.h render/amiga_render.h render/glyphmap.h render/glyph_tables.inc \
               handler/clip.h handler/lineedit.h handler/complete.h

amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/reach $(BUILD)/amiga/vtshow $(BUILD)/amiga/winbox $(BUILD)/amiga/sizewatch

# The reachability probe (ledger V3), an ordinary program with vbcc's startup.
$(BUILD)/amiga/reach: tests/amiga/reach.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/reach.c

$(BUILD)/amiga/vtshow: tests/amiga/vtshow.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/vtshow.c

$(BUILD)/amiga/winbox: tests/amiga/winbox.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/winbox.c

$(BUILD)/amiga/sizewatch: tests/amiga/sizewatch.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/sizewatch.c

$(BUILD)/amiga/vtengine-$(CPU).o: $(ENGINE) engine/vtengine.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -c -o $@ $(ENGINE)

# The handler: no C startup (handler_entry is the first code), exec memory.
# Gotcha: with -O2, vbcc can loop forever on a source that has a compile
# error instead of reporting it; build with -O0 to see the error.
# Rebuild when the flags change (DEBUG=1, DIRECT=1 on or off): the last
# flags are read while make parses, and a difference forces the link (a
# stamp file's mtime could equal the binary's to the second and be ignored).
HANDLER_FLAGS := DEBUG=$(DEBUG) CPU=$(CPU) DIRECT=$(DIRECT)
HANDLER_FLAGS_OLD := $(shell cat $(BUILD)/amiga/handler.flags 2>/dev/null)
ifneq ($(HANDLER_FLAGS),$(HANDLER_FLAGS_OLD))
HANDLER_FORCE := FORCE
endif
FORCE:

$(BUILD)/amiga/vtcon-handler: $(HANDLER_SRC) $(HANDLER_HDR) $(HANDLER_FORCE)
	@mkdir -p $(BUILD)/amiga/obj
	@echo '$(HANDLER_FLAGS)' > $(BUILD)/amiga/handler.flags
	$(VC) $(if $(DEBUG),-DVTCON_DEBUG) -DVT_AMIGA_EXEC_ALLOC -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/handler.o handler/vtcon_handler.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/vtengine.o $(ENGINE)
	$(VC) $(if $(DIRECT),-DVTCON_DIRECT) -c -o $(BUILD)/amiga/obj/amiga_render.o render/amiga_render.c
	$(VC) -c -o $(BUILD)/amiga/obj/glyphmap.o render/glyphmap.c
	$(VC) -c -o $(BUILD)/amiga/obj/clip.o handler/clip.c
	$(VC) -c -o $(BUILD)/amiga/obj/lineedit.o handler/lineedit.c
	$(VC) -c -o $(BUILD)/amiga/obj/complete.o handler/complete.c
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/handler.o \
	  $(BUILD)/amiga/obj/vtengine.o $(BUILD)/amiga/obj/amiga_render.o $(BUILD)/amiga/obj/glyphmap.o \
	  $(BUILD)/amiga/obj/clip.o $(BUILD)/amiga/obj/lineedit.o $(BUILD)/amiga/obj/complete.o \
	  -L/opt/homebrew/opt/vbcc/targets/m68k-amigaos/lib -lvc -lamiga

# The install kit: build/vtcon.lha (handler, DOSDrivers entry, terminfo,
# termcap, Install script, README), unpacking to a drawer "vtcon".
dist: amiga $(BUILD)/terminfo/76/vtcon
	rm -rf $(BUILD)/dist && mkdir -p $(BUILD)/dist/vtcon/terminfo/v
	cp $(BUILD)/amiga/vtcon-handler dist/XCON dist/Install dist/README.txt $(BUILD)/dist/vtcon/
	cp $(BUILD)/terminfo/v/vtcon $(BUILD)/dist/vtcon/terminfo/v/vtcon
	cp terminfo/vtcon.termcap $(BUILD)/dist/vtcon/termcap.vtcon
	cd $(BUILD)/dist && rm -f ../vtcon.lha && lha -aq ../vtcon.lha vtcon
	@ls -la $(BUILD)/vtcon.lha

# The one reachability test: XCON: through DOS on the running rig.
test-rig: amiga
	python3 tools/rig/reach.py

clean:
	rm -rf $(BUILD)
