# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
BUILD   := build

ENGINE  := engine/vtengine.c
RENDER  := render/glyphmap.c
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_pcansi.c tests/test_glyph.c

.PHONY: test test-ref golden vttest venv capture quirks amiga clean

test: $(BUILD)/vttest_host
	./$(BUILD)/vttest_host $(ONLY)

$(BUILD)/vttest_host: $(ENGINE) $(RENDER) engine/vtengine.h render/glyphmap.h render/glyph_tables.inc $(TESTS) tests/harness.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) $(RENDER) $(TESTS)

render/glyph_tables.inc: tools/gen_glyph_tables.py
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

# Re-record tests/streams/ from real programs (vim, less, bash, ls, top).
capture:
	.venv/bin/python tools/capture.py

# --- Amiga (vbcc, NDK 3.2; the same setup as DCTelnet) -----------------------
VBCC_CFG ?= $(CURDIR)/tools/vbcc-aos68k.cfg
CPU      ?= 68020
VC       := vc +$(VBCC_CFG) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors

GITREV  := $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet 2>/dev/null || echo -dirty)
HANDLER_SRC := handler/vtcon_handler.c $(ENGINE) render/amiga_render.c render/glyphmap.c
HANDLER_HDR := engine/vtengine.h render/amiga_render.h render/glyphmap.h render/glyph_tables.inc

amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vtcon-handler

$(BUILD)/amiga/vtengine-$(CPU).o: $(ENGINE) engine/vtengine.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -c -o $@ $(ENGINE)

# The handler: no C startup (handler_entry is the first code), exec memory.
# Gotcha: with -O2, vbcc can loop forever on a source that has a compile
# error instead of reporting it; build with -O0 to see the error.
# Rebuild when the flags change (DEBUG=1 on or off): the stamp holds them.
HANDLER_FLAGS := DEBUG=$(DEBUG) CPU=$(CPU)
$(BUILD)/amiga/handler.flags: FORCE
	@mkdir -p $(BUILD)/amiga
	@echo '$(HANDLER_FLAGS)' | cmp -s - $@ || echo '$(HANDLER_FLAGS)' > $@
FORCE:

$(BUILD)/amiga/vtcon-handler: $(HANDLER_SRC) $(HANDLER_HDR) $(BUILD)/amiga/handler.flags
	@mkdir -p $(BUILD)/amiga/obj
	$(VC) $(if $(DEBUG),-DVTCON_DEBUG) -DVT_AMIGA_EXEC_ALLOC -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/handler.o handler/vtcon_handler.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/vtengine.o $(ENGINE)
	$(VC) -c -o $(BUILD)/amiga/obj/amiga_render.o render/amiga_render.c
	$(VC) -c -o $(BUILD)/amiga/obj/glyphmap.o render/glyphmap.c
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/handler.o \
	  $(BUILD)/amiga/obj/vtengine.o $(BUILD)/amiga/obj/amiga_render.o $(BUILD)/amiga/obj/glyphmap.o \
	  -L/opt/homebrew/opt/vbcc/targets/m68k-amigaos/lib -lvc -lamiga

clean:
	rm -rf $(BUILD)
