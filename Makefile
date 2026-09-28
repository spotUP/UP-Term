# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
BUILD   := build

ENGINE  := engine/vtengine.c
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_pcansi.c

.PHONY: test test-pyte venv capture amiga clean

test: $(BUILD)/vttest_host
	./$(BUILD)/vttest_host $(ONLY)

$(BUILD)/vttest_host: $(ENGINE) engine/vtengine.h $(TESTS) tests/harness.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) $(TESTS)

$(BUILD)/vtdump: $(ENGINE) engine/vtengine.h tests/dump_main.c
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) tests/dump_main.c

# The xterm personality against pyte, on the recorded streams (ONLY= a name part).
test-pyte: $(BUILD)/vtdump
	.venv/bin/python tools/pyte_diff.py $(ONLY)

venv:
	python3 -m venv .venv && .venv/bin/pip install pyte

# Re-record tests/streams/ from real programs (vim, less, bash, ls, top).
capture:
	.venv/bin/python tools/capture.py

# --- Amiga (vbcc, NDK 3.2; the same setup as DCTelnet) -----------------------
VBCC_CFG ?= $(CURDIR)/tools/vbcc-aos68k.cfg
CPU      ?= 68020
VC       := vc +$(VBCC_CFG) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors

amiga: $(BUILD)/amiga/vtengine-$(CPU).o

$(BUILD)/amiga/vtengine-$(CPU).o: $(ENGINE) engine/vtengine.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -c -o $@ $(ENGINE)

clean:
	rm -rf $(BUILD)
