# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wno-long-long -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -DVT_CHECK_USED
BUILD   := build

# The workspace directory that holds vtcon and the other UP-Term repos side by
# side (see the upterm meta-repo); every sibling default below hangs off it.
# tools/rig/paths.py reads the same variable, with the same default.
UPTERM_ROOT ?= $(abspath ..)

ENGINE  := engine/vtengine.c
RENDER  := render/glyphmap.c render/unifont.c render/emoji.c render/fontpair.c render/sbar.c render/otag.c render/painter.c handler/lineedit.c handler/slash.c handler/clipfmt.c handler/complete_core.c render/vtinput.c handler/waitset.c
SHELL_CORE := shell/sh_parse.c shell/sh_expand.c shell/sh_exec.c shell/sh_float.c
TTY     := tty/ldisc.c tty/bmsg.c
DEVICE_CORE := device/upc_core.c
CONF    := config/upconf.c
TERMURL := config/termurl.c
PREFS_CORE := prefs/prefs_core.c
ICONSPEC := install/iconspec.c
ZMODEM  := zm/zmodem.c
TELNET  := net/tn.c
# hl and mdv (view/): the lexer, themes, the Markdown renderer
VIEW_LEX  := view/hl_lex.c view/hl_langs.c view/hl_style.c view/vw_text.c
VIEW_HL   := $(VIEW_LEX) view/hl_view.c view/md.c # md.c: hl -p shows Markdown as mdv does
VIEW_MD   := $(VIEW_LEX) view/md.c
VIEW_CORE := $(VIEW_LEX) view/hl_view.c view/md.c
VIEW_HDR  := view/hl_lex.h view/hl_style.h view/hl_view.h view/vw_text.h view/md.h
VIEW_CLI  := view/vw_cli.c
# the Claude client's portable core (ledger A2); net_posix is the host transport
# A4 WP1: the input box's vim mode, history, themes, prompt prefixes, transcript viewer
CLAUDE_INPUT := claude/vim.c claude/hist.c claude/theme.c claude/input.c claude/tview.c
CLAUDE_CORE := claude/util.c claude/http.c claude/net_posix.c claude/json.c claude/sse.c claude/stream.c claude/conv.c \
               claude/path.c claude/tools.c claude/sys_posix.c claude/ui.c claude/repl.c \
               claude/regex.c claude/glob.c claude/schema.c claude/search.c claude/shells.c claude/html.c \
               claude/webfetch.c claude/subagent.c claude/tasks.c claude/sched.c claude/watch.c claude/trust.c \
               claude/keys.c claude/edit.c claude/tui.c claude/show.c $(CLAUDE_INPUT) \
               claude/config.c claude/memory.c claude/commands.c claude/hooks.c claude/session.c claude/checkpoint.c \
               claude/policy.c claude/slash.c claude/setup.c claude/cli.c claude/print.c
CLAUDE_HDR := $(wildcard claude/*.h)
# every header a host binary can include: a header-only change rebuilds the host tests (tests/test_host_deps.py)
HOST_HDR := $(wildcard engine/*.h engine/*.inc render/*.h render/*.inc handler/*.h shell/*.h tty/*.h device/*.h config/*.h \
              prefs/*.h install/*.h zm/*.h net/*.h view/*.h demo/*.h demo/*.inc claude/*.h tests/*.h tests/exec_host/*.h)
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_reflow.c tests/test_sixel.c tests/test_pcansi.c tests/test_glyph.c tests/test_mirror.c tests/test_lineedit.c \
           tests/test_sh_parse.c tests/test_sh_expand.c tests/test_sh_exec.c tests/test_ldisc.c \
           tests/test_upcon.c tests/test_upconf.c tests/test_prefs.c tests/test_iconspec.c tests/test_zmodem.c tests/test_otag.c tests/test_slash.c tests/test_fontpair.c tests/test_updemo.c tests/test_pace.c tests/test_painter.c tests/test_text.c tests/test_clip.c \
           tests/test_input.c tests/test_protocol.c tests/test_sbar.c tests/test_telnet.c tests/test_complete.c tests/test_winmem.c tests/test_sbpack.c tests/test_hl.c tests/test_md.c \
           tests/claude_load.c tests/claude_screen.c tests/test_claude_http.c tests/test_claude_json.c tests/test_claude_stream.c tests/test_claude_tools.c tests/test_claude_match.c tests/test_claude_config.c tests/test_claude_repl.c tests/test_claude_cli.c tests/test_claude_tui.c tests/test_unifont.c tests/test_emoji.c tests/test_waitset.c tests/test_brk.c tests/test_sh_pipe.c tests/test_pty_name.c tests/test_bmsg.c tests/test_upassign.c

.PHONY: bashdiff ratchet-sort unifont emoji claude-tls-check widths demo-host view-host test test-ref te-diff test-terminfo test-rig dist dist-check golden vttest venv capture quirks amiga clean

# ONLY=uptelnetd runs the Mac end's tests alone (tools/test_uptelnetd.py)
test: $(BUILD)/vttest_host $(BUILD)/tn_host $(BUILD)/vsh_host $(BUILD)/vsh_host_leak $(BUILD)/hl $(BUILD)/mdv
	@if [ "$(ONLY)" != uptelnetd ] && [ "$(ONLY)" != fonts ] && [ "$(ONLY)" != bashdiff ] && [ "$(ONLY)" != installer ] && [ "$(ONLY)" != hl ] && [ "$(ONLY)" != entry ] && [ "$(ONLY)" != deps ]; then ./$(BUILD)/vttest_host $(ONLY); fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = uptelnetd ]; then python3 tools/test_uptelnetd.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = unifont ]; then python3 tests/test_gen_unifont.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = fonts ]; then python3 tests/test_dist_fonts.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_installer.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_rig_fixtures.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_cube_check.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = emoji ]; then python3 tests/test_gen_emoji.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = hl ]; then python3 tests/test_hl_plain.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = entry ]; then python3 tests/test_entry_stub.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = deps ]; then python3 tests/test_host_deps.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = bashdiff ]; then python3 tools/bashdiff.py --gate && python3 tools/bashdiff.py --leak; fi

$(BUILD)/vttest_host: $(HOST_HDR) $(CLAUDE_CORE) $(CLAUDE_HDR) $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(CONF) $(TERMURL) config/termurl.h $(PREFS_CORE) $(ICONSPEC) install/iconspec.h $(ZMODEM) $(TELNET) net/tn.h demo/updemo.c demo/updemo.h demo/tour_themes.inc zm/zmodem.h device/upc_core.h config/upconf.h prefs/prefs_core.h tty/ldisc.h tty/bmsg.h shell/sh_parse.h shell/sh_expand.h shell/sh_exec.h engine/vtengine.h engine/vtwidth.h handler/complete_core.h render/glyphmap.h render/unifont.h render/emoji.h render/fontpair.h render/sbar.h render/pace.h render/otag.h handler/lineedit.h handler/slash.h handler/menu_ids.h render/glyph_tables.inc render/synchold.h engine/vtcaps.inc terminfo/vtcon.terminfo $(VIEW_CORE) $(VIEW_HDR) $(TESTS) tests/harness.h tests/claude_screen.h handler/clipfmt.h render/vtinput.h handler/brk.c handler/brk.h tests/exec_host/exec_host.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -DVT_COUNT_ALLOC -Itests/exec_host -o $@ handler/brk.c $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(CONF) $(TERMURL) $(PREFS_CORE) $(ICONSPEC) $(ZMODEM) $(TELNET) demo/updemo.c $(VIEW_CORE) $(CLAUDE_CORE) $(TESTS)

# vsh's shell core on the host behind a POSIX sh_os (tests/vsh_host.c), and the
# differential run of tests/bash/probes against bash 5 (tools/bashdiff.py;
# ONLY=<area> runs one area, PROBE=<name> one probe, FAILED=1 the failing ones)
$(BUILD)/vsh_host: $(HOST_HDR) tests/vsh_host.c $(SHELL_CORE) tty/bmsg.c tty/bmsg.h shell/sh_parse.h shell/sh_expand.h shell/sh_exec.h shell/sh_hits.h shell/sh_float.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -DSH_HITS -o $@ tests/vsh_host.c $(SHELL_CORE) claude/regex.c tty/bmsg.c
# the same host shell with a counting allocator (tests/vh_alloc.c) instead of the sanitizers: the leak
# gate (tools/bashdiff.py --leak) runs it and fails when a ratchet probe leaves a block live (V44)
$(BUILD)/vsh_host_leak: $(HOST_HDR) tests/vsh_host.c tests/vh_alloc.c $(SHELL_CORE) tty/bmsg.c tty/bmsg.h shell/sh_parse.h shell/sh_expand.h shell/sh_exec.h shell/sh_hits.h shell/sh_float.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g -c -o $(BUILD)/vh_alloc.o tests/vh_alloc.c
	$(HOSTCC) -std=c89 -pedantic -Wno-long-long -Wall -Wextra -Werror -O1 -g -DSH_HITS -DVH_COUNT -Dmalloc=vh_malloc -Dcalloc=vh_calloc -Drealloc=vh_realloc -Dfree=vh_free -o $@ tests/vsh_host.c $(SHELL_CORE) claude/regex.c tty/bmsg.c $(BUILD)/vh_alloc.o
# the ratchet list in byte order (content unchanged: sort -o keeps every line)
ratchet-sort:
	LC_ALL=C sort -o tests/bash/ratchet.txt tests/bash/ratchet.txt

bashdiff: $(BUILD)/vsh_host
	python3 tools/bashdiff.py $(if $(ONLY),--only $(ONLY)) $(if $(PROBE),--probe $(PROBE)) $(if $(FAILED),--failed)

# The OpenSSL half of claude/tls_amissl.c checked against the host's OpenSSL 3
# (the AmiSSL SDK is not needed for this; OpenSSL's headers want C99).
OPENSSL_INC ?= /opt/homebrew/opt/openssl@3/include
claude-tls-check:
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -Wdeclaration-after-statement -fsyntax-only \
	  -DCL_TLS_HOSTCHECK -I$(OPENSSL_INC) claude/tls_amissl.c

engine/vtcaps.inc: tools/gen_vtcaps.py terminfo/vtcon.terminfo
	python3 tools/gen_vtcaps.py

# hl and mdv for the host terminal (and the timings): build/hl, build/mdv
view-host: $(BUILD)/hl $(BUILD)/mdv
$(BUILD)/hl: $(HOST_HDR) view/hl_main.c $(VIEW_HL) $(VIEW_CLI) view/vw_plat_posix.c $(VIEW_HDR) view/vw_cli.h view/vw_plat.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c89 -pedantic -Wall -Wextra -Werror -O2 -o $@ view/hl_main.c $(VIEW_HL) $(VIEW_CLI) view/vw_plat_posix.c
$(BUILD)/mdv: $(HOST_HDR) view/md_main.c $(VIEW_MD) $(VIEW_CLI) view/vw_plat_posix.c $(VIEW_HDR) view/vw_cli.h view/vw_plat.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c89 -pedantic -Wall -Wextra -Werror -O2 -o $@ view/md_main.c $(VIEW_MD) $(VIEW_CLI) view/vw_plat_posix.c

render/glyph_tables.inc: tools/gen_glyph_tables.py engine/vtengine.c
	python3 tools/gen_glyph_tables.py

# the themes the UPDemo tour switches between, from themes/ (committed)
demo/tour_themes.inc: tools/gen_tour_themes.py themes/dracula-default.conf themes/solarized-dark.conf themes/gruvbox-dark.conf themes/nord-default.conf
	python3 tools/gen_tour_themes.py

# engine/vtwidth.h from the Unicode character database (glibc's wcwidth rules;
# the UCD files are fetched once into build/ucd/). UNICODE= another version.
UNICODE ?= 16.0.0
widths:
	python3 tools/gen_width.py $(UNICODE)

# GNU Unifont (ledger U2): the .hex fetched once into build/unifont/ and
# checked against its pinned sha256, then one page file per 256 code points
# of the BMP ("4E") and of plane 1's emoji blocks U+1F000-1FAFF ("1F6") into
# build/unifont/pages/ (tools/gen_unifont.py; the kit's UP-Term:unifont/).
# UNIFONT_VER= another release (and UNIFONT_SHA256=).
UNIFONT_VER ?= 17.0.05
UNIFONT_SHA256 ?= 7b182454966046d35482469b979edce7d262fab5c53c2180e9b1fbb5d0b5e574
UNIFONT_HEX := $(BUILD)/unifont/unifont_all-$(UNIFONT_VER).hex.gz
UNIFONT_PAGES := $(BUILD)/unifont/pages
$(UNIFONT_HEX):
	@mkdir -p $(BUILD)/unifont
	curl -sSfL -o $@.tmp https://ftp.gnu.org/gnu/unifont/unifont-$(UNIFONT_VER)/unifont_all-$(UNIFONT_VER).hex.gz
	echo "$(UNIFONT_SHA256)  $@.tmp" | shasum -a 256 -c -
	mv $@.tmp $@
$(UNIFONT_PAGES)/stamp: $(UNIFONT_HEX) tools/gen_unifont.py
	python3 tools/gen_unifont.py $(UNIFONT_HEX) $(UNIFONT_PAGES)
	touch $@
unifont: $(UNIFONT_PAGES)/stamp

# Twemoji (ledger U4): the release archive fetched once into build/twemoji/
# and checked against its pinned sha256, then the colour pages of the
# two-cell emoji into build/emoji/pages/ (tools/gen_emoji.py, ~35 s; the
# kit's UP-Term:emoji/). TWEMOJI_VER= another release (and TWEMOJI_SHA256=).
TWEMOJI_VER ?= 17.0.3
TWEMOJI_SHA256 ?= a0855654b633045ae2337537e77f1bb4361162f7fcd910e613eaab1d6d9c5fca
TWEMOJI_TGZ := $(BUILD)/twemoji/twemoji-$(TWEMOJI_VER).tar.gz
EMOJI_PAGES := $(BUILD)/emoji/pages
$(TWEMOJI_TGZ):
	@mkdir -p $(BUILD)/twemoji
	curl -sSfL -o $@.tmp https://github.com/jdecked/twemoji/archive/refs/tags/v$(TWEMOJI_VER).tar.gz
	echo "$(TWEMOJI_SHA256)  $@.tmp" | shasum -a 256 -c -
	mv $@.tmp $@
$(EMOJI_PAGES)/stamp: $(TWEMOJI_TGZ) tools/gen_emoji.py engine/vtwidth.h
	python3 tools/gen_emoji.py $(TWEMOJI_TGZ) $(EMOJI_PAGES)
	touch $@
emoji: $(EMOJI_PAGES)/stamp

$(BUILD)/vtdump: $(ENGINE) engine/vtengine.h engine/vtwidth.h tests/dump_main.c
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) tests/dump_main.c

# The engine as a live terminal that answers a program's queries
# (tools/capture_claude.py).
$(BUILD)/vtreply: $(ENGINE) engine/vtengine.h engine/vtwidth.h tools/vtreply.c
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $(ENGINE) tools/vtreply.c

# uptelnet's protocol on a POSIX socket, for the interop test against
# tools/uptelnetd.py (tools/test_uptelnetd.py).
$(BUILD)/tn_host: $(HOST_HDR) net/tn.c net/tn.h tools/tn_host.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -o $@ net/tn.c tools/tn_host.c

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
DCTELNET ?= $(UPTERM_ROOT)/dctelnet-v2
ART ?= $(UPTERM_ROOT)/amiexpress-doorserver/bbs_ads
$(BUILD)/te_diff: tools/te_diff/te_diff.c tools/te_diff/te_shim.h engine/vtengine.c engine/vtengine.h engine/vtwidth.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=gnu99 -O1 -g -w -Itools/te_diff -I$(DCTELNET)/src/third_party/retro32-term \
		tools/te_diff/te_diff.c engine/vtengine.c -o $@
te-diff:
	@test -d $(DCTELNET)/src/third_party/retro32-term || { echo "[ERROR] te-diff needs a DCTelnet checkout in DCTELNET=$(DCTELNET) (dctelnet-v2, optional in upterm's repos.lock: upterm-bootstrap --only dctelnet-v2): set DCTELNET=<checkout>"; exit 1; }
	@$(MAKE) --no-print-directory $(BUILD)/te_diff
	@test -d $(ART) || { echo "[ERROR] te-diff needs BBS art in ART=$(ART) (amiexpress-doorserver, optional in upterm's repos.lock; private repo): set ART=<dir of .ans files>"; exit 1; }
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

# GNU screen for the kit (P7.1): built in screen-amiga/src (make -f Makefile.amiga)
SCREEN_BIN ?= $(UPTERM_ROOT)/screen-amiga/src/screen
# tmux for the kit (P7.2): built in tmux-amiga (make -f Makefile.amiga)
AMIGA_STRIP ?= $(HOME)/opt/amiga/bin/m68k-amigaos-strip
TMUX_BIN ?= $(UPTERM_ROOT)/tmux-amiga/build/tmux-bin

# Recapture the programs with TERM=vtcon (tests/streams/ti-*), then check
# them: libvterm cell for cell and no sequence the engine ignored.
test-terminfo: $(BUILD)/terminfo/76/vtcon $(BUILD)/vtdump $(BUILD)/vterm_dump
	.venv/bin/python tools/capture.py vtcon
	.venv/bin/python tools/refdiff.py ti-

# Re-record tests/streams/ from real programs (vim, less, bash, ls, top).
capture:
	.venv/bin/python tools/capture.py

# --- Amiga (vbcc, NDK 3.2; the same setup as DCTelnet) -----------------------
# vbcc's install prefix: tools/vbcc-aos68k.cfg.in names it, so the cfg vc reads
# is generated into build/ (rewritten only when its content changes).
ifndef VBCC_PREFIX
VBCC_PREFIX := $(shell brew --prefix vbcc 2>/dev/null || echo /opt/homebrew/opt/vbcc)
endif
VBCC_CFG ?= $(CURDIR)/$(BUILD)/vbcc-aos68k.cfg
VBCC_CFG_GEN := $(shell mkdir -p $(BUILD) && sed 's|@VBCC_PREFIX@|$(VBCC_PREFIX)|g' tools/vbcc-aos68k.cfg.in > $(BUILD)/vbcc-aos68k.cfg.tmp && { cmp -s $(BUILD)/vbcc-aos68k.cfg.tmp $(BUILD)/vbcc-aos68k.cfg && rm $(BUILD)/vbcc-aos68k.cfg.tmp || mv $(BUILD)/vbcc-aos68k.cfg.tmp $(BUILD)/vbcc-aos68k.cfg; })
# written by the line above; the empty rule lets `make build/vbcc-aos68k.cfg` ask for it
$(BUILD)/vbcc-aos68k.cfg: ;
CPU      ?= 68020
# The AmigaOS 3.2 SDK headers. vendor/ is gitignored (4.1 MB of third-party
# headers), so unpack NDK3.2R4 there once, or point this at your own copy:
#   make amiga VTCON_NDK=<path>/Include_H
VTCON_NDK ?= $(CURDIR)/vendor/ndk-3.2r4-Include_H
VC       := vc +$(VBCC_CFG) -I$(VTCON_NDK) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors

# The 68020 check (tty/cpucheck.c, built for the 68000): the program's main is
# renamed up_main (-Dmain=up_main) and the object below is linked in front of it.
$(BUILD)/amiga/obj/cpuchk-%.o: tty/cpucheck.c tty/bmsg.h tty/bmsg.c
	@mkdir -p $(BUILD)/amiga/obj
	$(subst -cpu=$(CPU),-cpu=68000,$(VC)) -DUP_PROG=$* -c -o $@ tty/cpucheck.c


GITREV  := $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet 2>/dev/null || echo -dirty)
# the engine's hot loops in assembler, for the builds that define VT_ASM (S1)
ENGINE_68K := engine/vtengine_68k.s
HANDLER_SRC := handler/handler_start.s $(ENGINE_68K) render/amiga_render_68k.s render/painter.c render/painter_68k.s render/painter.h handler/vtcon_handler.c handler/clip.c handler/lineedit.c handler/complete.c handler/complete_core.c handler/brk.c handler/waitset.c handler/slash.c handler/sbar_gad.c $(ENGINE) render/amiga_render.c render/vtwin.c render/vtinput.c render/sbar.c render/glyphmap.c render/unifont.c render/emoji.c render/fontpair.c render/outline.c render/otag.c tty/ldisc.c config/upconf.c config/termurl.c prefs/prefs_core.c prefs/prefs_dos.c handler/clipfmt.c
HANDLER_HDR := engine/vtengine.h engine/vtcaps.inc engine/vtwidth.h render/amiga_render.h render/vtwin.h render/synchold.h render/vtinput.h render/sbar.h handler/sbar_gad.h render/glyphmap.h render/unifont.h render/emoji.h render/glyph_tables.inc render/outline.h render/otag.h \
               handler/clip.h handler/clipfmt.h handler/lineedit.h handler/le_fns.h handler/complete.h handler/complete_core.h handler/brk.h handler/slash.h handler/menu_ids.h handler/vtcon_packets.h tty/ldisc.h device/upc_public.h config/upconf.h config/termurl.h config/upassign.h

amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/up-console.device $(BUILD)/amiga/UPConsole $(BUILD)/amiga/pty-handler $(BUILD)/amiga/reach $(BUILD)/amiga/vtshow $(BUILD)/amiga/winbox $(BUILD)/amiga/sizewatch $(BUILD)/amiga/breakport $(BUILD)/amiga/ttyprobe $(BUILD)/amiga/dsrtime $(BUILD)/amiga/dripens $(BUILD)/amiga/wasabikey $(BUILD)/amiga/UPDemo $(BUILD)/amiga/cellbench $(BUILD)/amiga/wprobe $(BUILD)/amiga/phaseprobe $(BUILD)/amiga/engbench $(BUILD)/amiga/ptytest $(BUILD)/amiga/ixkill $(BUILD)/amiga/vsh $(BUILD)/amiga/ixpipe-handler $(BUILD)/amiga/upprefs $(BUILD)/amiga/upicon $(BUILD)/amiga/sz $(BUILD)/amiga/rz $(BUILD)/amiga/upgetty $(BUILD)/amiga/UPTerm $(BUILD)/amiga/uptelnet $(BUILD)/amiga/hl $(BUILD)/amiga/mdv $(BUILD)/amiga/Claude

# hl and mdv: the portable view/ core and the AmigaDOS side (no ixemul).
# vbcc warns (153, 65) on the (void) parameter casts, as for vsh.
$(BUILD)/amiga/hl: view/hl_main.c $(VIEW_HL) $(VIEW_CLI) view/vw_plat_amiga.c $(VIEW_HDR) view/vw_cli.h view/vw_plat.h handler/vtcon_packets.h tty/ldisc.h $(BUILD)/amiga/obj/cpuchk-hl.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ view/hl_main.c $(VIEW_HL) $(VIEW_CLI) view/vw_plat_amiga.c $(BUILD)/amiga/obj/cpuchk-hl.o
$(BUILD)/amiga/mdv: view/md_main.c $(VIEW_MD) $(VIEW_CLI) view/vw_plat_amiga.c $(VIEW_HDR) view/vw_cli.h view/vw_plat.h handler/vtcon_packets.h tty/ldisc.h $(BUILD)/amiga/obj/cpuchk-mdv.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ view/md_main.c $(VIEW_MD) $(VIEW_CLI) view/vw_plat_amiga.c $(BUILD)/amiga/obj/cpuchk-mdv.o

# C:Claude, the native Claude client (ledger A2): the portable core
# (claude/*.c, host-tested) with bsdsocket and AmigaDOS. bsdsocket's headers
# are Roadshow's netinclude from NDK3.2R4 (SANA+RoadshowTCP-IP/netinclude;
# unpack it to vendor/ndk-3.2r4-netinclude, or pass VTCON_NETINCLUDE=); only
# net_amiga.c sees them (they carry an errno.h of their own). TLS: AmiSSL 5
# with AMISSL_SDK=<the SDK's top directory> (its include/ inside), else a
# build without TLS that refuses https (http URLs, the fixture, still work).
VTCON_NETINCLUDE ?= $(CURDIR)/vendor/ndk-3.2r4-netinclude
CLAUDE_PORTABLE := claude/util.c claude/http.c claude/json.c claude/sse.c claude/stream.c claude/conv.c \
                   claude/path.c claude/tools.c claude/ui.c claude/repl.c claude/sys_amiga.c claude/main_amiga.c \
                   claude/regex.c claude/glob.c claude/schema.c claude/search.c claude/shells.c claude/html.c \
                   claude/webfetch.c claude/subagent.c claude/tasks.c claude/sched.c claude/watch.c claude/trust.c \
                   claude/keys.c claude/edit.c claude/tui.c claude/show.c $(CLAUDE_INPUT) $(VIEW_MD) tty/ldisc.c tty/bmsg.c \
                   handler/clip.c handler/clipfmt.c \
                   claude/config.c claude/memory.c claude/commands.c claude/hooks.c claude/session.c claude/checkpoint.c \
                   claude/policy.c claude/slash.c claude/setup.c claude/cli.c claude/print.c
# the SDK unpacked into vendor/ (gitignored, like the NDK) is used when present
AMISSL_SDK ?= $(firstword $(wildcard $(CURDIR)/vendor/amissl-*/AmiSSL/Developer))
ifdef AMISSL_SDK
CLAUDE_TLS := claude/tls_amissl.c
CLAUDE_TLS_INC := -I$(AMISSL_SDK)/include
else
CLAUDE_TLS := claude/tls_none.c
endif
CLAUDE_FLAGS := AMISSL_SDK=$(AMISSL_SDK)
ifneq ($(CLAUDE_FLAGS),$(shell cat $(BUILD)/amiga/claude.flags 2>/dev/null))
CLAUDE_FORCE := FORCE
endif
$(BUILD)/amiga/Claude: $(CLAUDE_PORTABLE) claude/net_amiga.c $(CLAUDE_TLS) $(CLAUDE_HDR) $(VIEW_HDR) tty/ldisc.h tty/bmsg.h handler/vtcon_packets.h $(CLAUDE_FORCE) $(BUILD)/amiga/obj/cpuchk-Claude.o
	@mkdir -p $(BUILD)/amiga/obj/claude
	@echo '$(CLAUDE_FLAGS)' > $(BUILD)/amiga/claude.flags
	$(VC) -dontwarn=153,65 -I$(VTCON_NETINCLUDE) -c -o $(BUILD)/amiga/obj/claude/net_amiga.o claude/net_amiga.c
	$(VC) -dontwarn=153,65 $(CLAUDE_TLS_INC) -c -o $(BUILD)/amiga/obj/claude/tls.o $(CLAUDE_TLS)
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ $(CLAUDE_PORTABLE) $(BUILD)/amiga/obj/claude/net_amiga.o $(BUILD)/amiga/obj/claude/tls.o $(BUILD)/amiga/obj/cpuchk-Claude.o

# The reachability probe (ledger V3), an ordinary program with vbcc's startup.
$(BUILD)/amiga/reach: tests/amiga/reach.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/reach.c

# vsh: the portable core (host-tested) and the AmigaDOS side. vbcc warns
# (153) on the (void) parameter casts the host compiler needs, and (65) on
# the parameters they are for.
# Size pass: vbcc -O=1 -size is 6.8 KB smaller than -O2 (bit 2 of -O
# and -O2 grow vsh); -D__NOINLINE__ stops vbcc's string.h from inlining
# strcmp/strlen/strcpy at every call (another 3.5 KB). Measured 2026-10-07.
VSH_OPT := -O=1 -size -D__NOINLINE__
VSH_SRC := shell/vsh.c shell/sh_exec.c shell/sh_expand.c shell/sh_parse.c shell/sh_float.c claude/regex.c config/termurl.c tty/bmsg.c
$(BUILD)/amiga/vsh: $(VSH_SRC) config/termurl.h shell/sh_exec.h shell/sh_expand.h shell/sh_parse.h handler/vtcon_packets.h tty/ldisc.h tty/bmsg.h $(BUILD)/amiga/obj/cpuchk-vsh.o
	@mkdir -p $(BUILD)/amiga
	$(subst -O2,$(VSH_OPT),$(VC)) -Dmain=up_main -dontwarn=153,65,79 $(if $(DEBUG),-DVSH_DEBUG) -o $@ $(VSH_SRC) $(BUILD)/amiga/obj/cpuchk-vsh.o

# vsh that logs how deep its stacks went (RAM:vsh_hw.log, shell/vsh.c VSH_STACKHW): the $$STACK measurement
$(BUILD)/amiga/vsh_hw: $(VSH_SRC) $(HOST_HDR) $(BUILD)/amiga/obj/cpuchk-vsh.o
	@mkdir -p $(BUILD)/amiga
	$(subst -O2,$(VSH_OPT),$(VC)) -Dmain=up_main -dontwarn=153,65,79 -DVSH_STACKHW -o $@ $(VSH_SRC) $(BUILD)/amiga/obj/cpuchk-vsh.o

$(BUILD)/amiga/vtshow: tests/amiga/vtshow.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/vtshow.c

$(BUILD)/amiga/winbox: tests/amiga/winbox.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/winbox.c

$(BUILD)/amiga/sizewatch: tests/amiga/sizewatch.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/sizewatch.c

$(BUILD)/amiga/pipeprobe: tests/amiga/pipeprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/pipeprobe.c

$(BUILD)/amiga/compprobe: tests/amiga/compprobe.c handler/vtcon_packets.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/compprobe.c

$(BUILD)/amiga/breakport: tests/amiga/breakport.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/breakport.c

# the Prefs editor: the profiles in ENV: / ENVARC:up-term/up-term, two pages
# of GadTools gadgets; its model (prefs_core) is host-tested. A plain CLI
# program (its own window), so vc links it. vbcc warns (153, 65) on the
# (void) parameter casts of the file callbacks, as for vsh.
# The kit ships it as "UP-Term Prefs" (make cannot hold a space in a target).
$(BUILD)/amiga/upprefs: prefs/upprefs.c prefs/prefs_core.c prefs/prefs_core.h prefs/prefs_dos.c prefs/prefs_dos.h config/upconf.c config/upconf.h $(BUILD)/amiga/obj/cpuchk-upprefs.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ prefs/upprefs.c prefs/prefs_core.c prefs/prefs_dos.c config/upconf.c $(BUILD)/amiga/obj/cpuchk-upprefs.o

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

# The signal mask across ixemul's stack extension (tools/rig/stackext_rig.py).
# -mstackextend exists only in ixemul's gcc 2.95.3 (ixemul-vtcon docker/build.sh
# builds the image): compile there, link with the usual cross gcc.
IXGCC295 ?= docker run --rm --platform linux/amd64 -v "$(CURDIR)":/w -v "$(HOME)/opt/amiga/m68k-amigaos/ndk-include":/ndk:ro ixemul-gcc295
$(BUILD)/amiga/ixstackext: tests/amiga/ixstackext.c
	@mkdir -p $(BUILD)/amiga
	PATH=/Applications/Docker.app/Contents/Resources/bin:$$PATH $(IXGCC295) sh -c 'cd /w && m68k-amigaos-gcc -idirafter /ndk -m68020 -O2 -Wall -mstackextend -c -o $(BUILD)/amiga/ixstackext.o tests/amiga/ixstackext.c'
	$(AGCC) -mcrt=ixemul -o $@ $(BUILD)/amiga/ixstackext.o

# vsh's umask builtin reaching the programs it starts (tools/rig/umask_rig.py).
$(BUILD)/amiga/ixumask: tests/amiga/ixumask.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixumask.c

# A resized XCON: window reaching an ixemul program (tools/rig/winch_rig.py).
$(BUILD)/amiga/ixwinch: tests/amiga/ixwinch.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixwinch.c

# The characters typed after a Ctrl-C that ended a read (tools/rig/intr_rig.py).
$(BUILD)/amiga/ixintr: tests/amiga/ixintr.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixintr.c

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
IXEMUL_SRC ?= $(UPTERM_ROOT)/ixemul-vtcon
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

# sz / rz: ZMODEM over the shell's stream, no ixemul (ledger T4 G3)
$(BUILD)/amiga/sz: zm/zm_amiga.c zm/zmodem.c zm/zmodem.h $(BUILD)/amiga/obj/cpuchk-sz.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -DZM_SZ -o $@ zm/zm_amiga.c zm/zmodem.c $(BUILD)/amiga/obj/cpuchk-sz.o

$(BUILD)/amiga/rz: zm/zm_amiga.c zm/zmodem.c zm/zmodem.h $(BUILD)/amiga/obj/cpuchk-rz.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -DZM_RZ -o $@ zm/zm_amiga.c zm/zmodem.c $(BUILD)/amiga/obj/cpuchk-rz.o

# the kit's icon tool (Install: the Shell icon opens UP-Term)
$(BUILD)/amiga/upicon: install/upicon.c install/iconspec.c install/iconspec.h $(BUILD)/amiga/obj/cpuchk-upicon.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -o $@ install/upicon.c install/iconspec.c $(BUILD)/amiga/obj/cpuchk-upicon.o

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

# The console's answer time for Neovim 0.12's startup query (OSC 11 + DSR 5n)
$(BUILD)/amiga/dsrtime: tests/amiga/dsrtime.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/dsrtime.c

# UP-Term's show-off (demo/updemo.h): the Amiga program, and the same
# scenes for a Unix terminal (make demo-host; build/updemo)
$(BUILD)/amiga/UPDemo: demo/updemo.c demo/updemo_amiga.c demo/updemo.h demo/tour_themes.inc $(BUILD)/amiga/obj/cpuchk-UPDemo.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ demo/updemo.c demo/updemo_amiga.c $(BUILD)/amiga/obj/cpuchk-UPDemo.o

demo-host: $(BUILD)/updemo
$(BUILD)/updemo: demo/updemo.c demo/updemo_posix.c demo/updemo.h demo/tour_themes.inc
	@mkdir -p $(BUILD)
	$(HOSTCC) -O2 -Wall -Wextra -o $@ demo/updemo.c demo/updemo_posix.c

# The kit's wasabid key maker (Install, wasabi ticked)
$(BUILD)/amiga/wasabikey: install/wasabikey.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ install/wasabikey.c

# The engine alone on the 68k: bytes a second per workload (S1)
$(BUILD)/amiga/engbench: tests/amiga/engbench.c $(ENGINE) $(ENGINE_68K) render/amiga_render_68k.s render/painter.c render/painter_68k.s render/painter.h engine/vtengine.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -dontwarn=153,65 -DVT_ASM -DVP_ASM -o $@ tests/amiga/engbench.c $(ENGINE) $(ENGINE_68K) render/amiga_render_68k.s render/painter.c render/painter_68k.s

# Where a write's time goes: conbench's per-write shapes, apart (S1)
$(BUILD)/amiga/wprobe: tests/amiga/wprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/wprobe.c

$(BUILD)/amiga/phaseprobe: tests/amiga/phaseprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/phaseprobe.c

# What a full screen of each kind of cell costs the terminal (S1)
$(BUILD)/amiga/cellbench: tests/amiga/cellbench.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/cellbench.c

# A public screen's DrawInfo pens and their colours (ownscreen_rig)
$(BUILD)/amiga/dripens: tests/amiga/dripens.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/dripens.c

$(BUILD)/amiga/ttyprobe: tests/amiga/ttyprobe.c handler/vtcon_packets.h tty/ldisc.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/ttyprobe.c

# UP-Term's console.device (console plan D1.6): upcon_rom.o first (the
# RomTag and the ROM forwards), no C startup, as the handler.
DEVICE_SRC := device/upcon_device.c device/upcon_unit.c device/upcon_input.c device/upc_core.c \
              render/vtwin.c render/vtinput.c render/sbar.c render/amiga_render.c render/painter.c render/glyphmap.c render/unifont.c render/emoji.c render/fontpair.c render/outline.c render/otag.c handler/clip.c handler/clipfmt.c $(ENGINE)
DEVICE_HDR := device/upcon.h device/upc_public.h device/upc_core.h render/vtwin.h render/vtinput.h render/sbar.h render/amiga_render.h render/painter.h render/glyphmap.h render/unifont.h render/emoji.h \
              render/glyph_tables.inc handler/clip.h handler/clipfmt.h engine/vtengine.h engine/vtwidth.h
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
	$(VC) -c -o $(BUILD)/amiga/devobj/vtinput.o render/vtinput.c
	$(VC) -c -o $(BUILD)/amiga/devobj/sbar.o render/sbar.c
	$(VC) -c -o $(BUILD)/amiga/devobj/amiga_render.o render/amiga_render.c
	$(VC) -c -o $(BUILD)/amiga/devobj/painter.o render/painter.c
	$(VC) -c -o $(BUILD)/amiga/devobj/glyphmap.o render/glyphmap.c
	$(VC) -c -o $(BUILD)/amiga/devobj/unifont.o render/unifont.c
	$(VC) -c -o $(BUILD)/amiga/devobj/emoji.o render/emoji.c
	$(VC) -c -o $(BUILD)/amiga/devobj/fontpair.o render/fontpair.c
	$(VC) -c -o $(BUILD)/amiga/devobj/outline.o render/outline.c
	$(VC) -c -o $(BUILD)/amiga/devobj/otag.o render/otag.c
	$(VC) -c -o $(BUILD)/amiga/devobj/clip.o handler/clip.c
	$(VC) -c -o $(BUILD)/amiga/devobj/clipfmt.o handler/clipfmt.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/devobj/vtengine.o $(ENGINE)
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/devobj/upcon_rom.o \
	  $(BUILD)/amiga/devobj/upcon_device.o $(BUILD)/amiga/devobj/upcon_unit.o $(BUILD)/amiga/devobj/upcon_input.o \
	  $(BUILD)/amiga/devobj/upc_core.o $(BUILD)/amiga/devobj/vtwin.o $(BUILD)/amiga/devobj/vtinput.o $(BUILD)/amiga/devobj/sbar.o $(BUILD)/amiga/devobj/amiga_render.o \
	  $(BUILD)/amiga/devobj/glyphmap.o $(BUILD)/amiga/devobj/unifont.o $(BUILD)/amiga/devobj/emoji.o $(BUILD)/amiga/devobj/outline.o $(BUILD)/amiga/devobj/otag.o \
	  $(BUILD)/amiga/devobj/fontpair.o $(BUILD)/amiga/devobj/painter.o \
	  $(BUILD)/amiga/devobj/clip.o $(BUILD)/amiga/devobj/clipfmt.o $(BUILD)/amiga/devobj/vtengine.o \
	  -L$(VBCC_PREFIX)/targets/m68k-amigaos/lib -lvc -lamiga

# C:UPTerm: the slash commands from scripts (ledger C1)
$(BUILD)/amiga/UPTerm: handler/upterm.c handler/vtcon_packets.h $(BUILD)/amiga/obj/cpuchk-UPTerm.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -o $@ handler/upterm.c $(BUILD)/amiga/obj/cpuchk-UPTerm.o

# C:uptelnet: a telnet client for the window, no ixemul (ledger A1.1). The
# Roadshow SDK's network headers ("Freely Distributable"): vendor/ (gitignored)
# when unpacked there, else DCTelnet's copy, or VTCON_NETINC=<netinclude>.
VTCON_NETINC ?= $(firstword $(wildcard $(CURDIR)/vendor/roadshow-netinclude $(UPTERM_ROOT)/dctelnet-v2/src/third_party/netinclude) $(CURDIR)/vendor/roadshow-netinclude)
$(BUILD)/amiga/uptelnet: net/uptelnet.c net/tn.c net/tn.h handler/vtcon_packets.h tty/ldisc.h tty/bmsg.h $(BUILD)/amiga/obj/cpuchk-uptelnet.o
	@test -d $(VTCON_NETINC) || { echo "[ERROR] C:uptelnet needs the Roadshow netinclude headers, not found at $(VTCON_NETINC): unpack them to vendor/roadshow-netinclude, or pass VTCON_NETINC=<dir> (DCTelnet's copy: dctelnet-v2, optional in upterm's repos.lock)"; exit 1; }
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -I$(VTCON_NETINC) -o $@ net/uptelnet.c net/tn.c tty/bmsg.c $(BUILD)/amiga/obj/cpuchk-uptelnet.o

# C:upgetty: a shell over the serial port through a PTY: pair (ledger T4)
$(BUILD)/amiga/upgetty: device/upgetty.c handler/vtcon_packets.h $(BUILD)/amiga/obj/cpuchk-upgetty.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153 -o $@ device/upgetty.c $(BUILD)/amiga/obj/cpuchk-upgetty.o

# C:UPConsole: CON:/RAW: to UP-Term and back (console plan H5.4)
$(BUILD)/amiga/UPConsole: device/upconsole.c device/upc_public.h $(BUILD)/amiga/obj/cpuchk-UPConsole.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -o $@ device/upconsole.c $(BUILD)/amiga/obj/cpuchk-UPConsole.o

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

$(BUILD)/amiga/allocwatch: tests/amiga/allocwatch.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/allocwatch.c

# D4.3's patch (tools/rig/devctl_rig.py; test only)
$(BUILD)/amiga/patchcon: tests/amiga/patchcon.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/patchcon.c

# D1.3's copy probe (tools/rig/snip_rig.py)
$(BUILD)/amiga/snipprobe: tests/amiga/snipprobe.c handler/clip.c handler/clip.h handler/clipfmt.c handler/clipfmt.h
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/snipprobe.c handler/clip.c handler/clipfmt.c

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
	$(VC) $(if $(DEBUG),-DVTCON_DEBUG) $(if $(SERIAL),-DVTCON_SERIAL) $(if $(PROF),-DVTCON_PROF) -DVT_AMIGA_EXEC_ALLOC -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/handler.o handler/vtcon_handler.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -DVT_ASM -c -o $(BUILD)/amiga/obj/vtengine.o $(ENGINE)
	vasmm68k_mot -quiet -Fhunk -o $(BUILD)/amiga/obj/vtengine_68k.o $(ENGINE_68K)
	$(VC) $(if $(DIRECT),-DVTCON_DIRECT) $(if $(PROF),-DVTCON_PROF) -DVR_ASM -c -o $(BUILD)/amiga/obj/amiga_render.o render/amiga_render.c
	vasmm68k_mot -quiet -Fhunk -o $(BUILD)/amiga/obj/amiga_render_68k.o render/amiga_render_68k.s
	$(VC) -DVP_ASM -c -o $(BUILD)/amiga/obj/painter.o render/painter.c
	vasmm68k_mot -quiet -Fhunk -m68020 -o $(BUILD)/amiga/obj/painter_68k.o render/painter_68k.s
	$(VC) $(if $(PROF),-DVTCON_PROF) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/vtwin.o render/vtwin.c
	$(VC) -c -o $(BUILD)/amiga/obj/vtinput.o render/vtinput.c
	$(VC) -c -o $(BUILD)/amiga/obj/sbar.o render/sbar.c
	$(VC) -c -o $(BUILD)/amiga/obj/sbar_gad.o handler/sbar_gad.c
	$(VC) -c -o $(BUILD)/amiga/obj/glyphmap.o render/glyphmap.c
	$(VC) -c -o $(BUILD)/amiga/obj/unifont.o render/unifont.c
	$(VC) -c -o $(BUILD)/amiga/obj/emoji.o render/emoji.c
	$(VC) -c -o $(BUILD)/amiga/obj/fontpair.o render/fontpair.c
	$(VC) -c -o $(BUILD)/amiga/obj/outline.o render/outline.c
	$(VC) -c -o $(BUILD)/amiga/obj/otag.o render/otag.c
	$(VC) -c -o $(BUILD)/amiga/obj/clip.o handler/clip.c
	$(VC) -c -o $(BUILD)/amiga/obj/clipfmt.o handler/clipfmt.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/lineedit.o handler/lineedit.c
	$(VC) -c -o $(BUILD)/amiga/obj/complete.o handler/complete.c
	$(VC) -c -o $(BUILD)/amiga/obj/complete_core.o handler/complete_core.c
	$(VC) -c -o $(BUILD)/amiga/obj/brk.o handler/brk.c
	$(VC) -c -o $(BUILD)/amiga/obj/waitset.o handler/waitset.c
	$(VC) -c -o $(BUILD)/amiga/obj/slash.o handler/slash.c
	$(VC) -c -o $(BUILD)/amiga/obj/ldisc.o tty/ldisc.c
	$(VC) -c -o $(BUILD)/amiga/obj/upconf.o $(CONF)
	$(VC) -c -o $(BUILD)/amiga/obj/termurl.o $(TERMURL)
	$(VC) -dontwarn=153,65 -c -o $(BUILD)/amiga/obj/prefs_core.o prefs/prefs_core.c
	$(VC) -dontwarn=153,65 -c -o $(BUILD)/amiga/obj/prefs_dos.o prefs/prefs_dos.c
	vasmm68k_mot -quiet -Fhunk -o $(BUILD)/amiga/obj/handler_start.o handler/handler_start.s
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/handler_start.o $(BUILD)/amiga/obj/handler.o \
	  $(BUILD)/amiga/obj/vtengine.o $(BUILD)/amiga/obj/vtengine_68k.o $(BUILD)/amiga/obj/amiga_render.o $(BUILD)/amiga/obj/amiga_render_68k.o $(BUILD)/amiga/obj/painter.o $(BUILD)/amiga/obj/painter_68k.o $(BUILD)/amiga/obj/vtwin.o $(BUILD)/amiga/obj/vtinput.o $(BUILD)/amiga/obj/sbar.o $(BUILD)/amiga/obj/sbar_gad.o $(BUILD)/amiga/obj/glyphmap.o $(BUILD)/amiga/obj/unifont.o $(BUILD)/amiga/obj/emoji.o \
	  $(BUILD)/amiga/obj/outline.o $(BUILD)/amiga/obj/otag.o $(BUILD)/amiga/obj/fontpair.o \
	  $(BUILD)/amiga/obj/clip.o $(BUILD)/amiga/obj/clipfmt.o $(BUILD)/amiga/obj/lineedit.o $(BUILD)/amiga/obj/complete.o $(BUILD)/amiga/obj/complete_core.o \
	  $(BUILD)/amiga/obj/brk.o $(BUILD)/amiga/obj/waitset.o $(BUILD)/amiga/obj/slash.o $(BUILD)/amiga/obj/ldisc.o $(BUILD)/amiga/obj/upconf.o $(BUILD)/amiga/obj/termurl.o \
	  $(BUILD)/amiga/obj/prefs_core.o $(BUILD)/amiga/obj/prefs_dos.o \
	  -L$(VBCC_PREFIX)/targets/m68k-amigaos/lib -lvc -lamiga

# PTY: (P5): pseudo-terminals on the same line discipline. No C startup:
# handler/handler_start.s (shared with vtcon-handler) is linked first.
PTY_FLAGS := DEBUG=$(DEBUG)
ifneq ($(PTY_FLAGS),$(shell cat $(BUILD)/amiga/pty.flags 2>/dev/null))
PTY_FORCE := FORCE
endif
$(BUILD)/amiga/pty-handler: handler/pty_handler.c handler/pty_name.h handler/handler_start.s $(PTY_FORCE) handler/brk.c handler/brk.h handler/vtcon_packets.h tty/ldisc.c tty/ldisc.h handler/waitset.c handler/waitset.h
	@mkdir -p $(BUILD)/amiga/obj/pty
	@echo '$(PTY_FLAGS)' > $(BUILD)/amiga/pty.flags
	$(VC) $(if $(DEBUG),-DPTY_DEBUG) -DVTCON_BUILD=$(subst -,_,$(GITREV)) -c -o $(BUILD)/amiga/obj/pty/pty_handler.o handler/pty_handler.c
	$(VC) -c -o $(BUILD)/amiga/obj/pty/brk.o handler/brk.c
	$(VC) -c -o $(BUILD)/amiga/obj/pty/waitset.o handler/waitset.c
	$(VC) -c -o $(BUILD)/amiga/obj/pty/ldisc.o tty/ldisc.c
	vasmm68k_mot -quiet -Fhunk -o $(BUILD)/amiga/obj/pty/handler_start.o handler/handler_start.s
	vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -s -o $@ $(BUILD)/amiga/obj/pty/handler_start.o $(BUILD)/amiga/obj/pty/pty_handler.o \
	  $(BUILD)/amiga/obj/pty/brk.o $(BUILD)/amiga/obj/pty/waitset.o $(BUILD)/amiga/obj/pty/ldisc.o \
	  -L$(VBCC_PREFIX)/targets/m68k-amigaos/lib -lvc -lamiga

# The install kit: build/UP-Term.lha -- a drawer UP-Term with Install (an
# Installer script and its icon), Uninstall, README.txt, LICENSES.txt and Files/ (the rest).
KIT := $(BUILD)/dist/UP-Term
# the patched ixemul (P6): built in ixemul-vtcon with sh docker/build.sh
IXEMUL_LIB ?= $(UPTERM_ROOT)/ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library
# Python 3.14 and Neovim 0.12, built in their own repos (make dist there)
PYTHON_DIST ?= $(UPTERM_ROOT)/cpython-amiga/build/m68k/dist/Python3
NVIM_DIST ?= $(UPTERM_ROOT)/neovim-amiga/build/v012/dist/nvim
# ixnet.library from the same build (ixnet refuses an ixemul of another revision)
IXNET_LIB ?= $(dir $(IXEMUL_LIB))../../../../ixnet/68020/amigaos/ixnet.library
dist: amiga $(BUILD)/amiga/UPConsole $(BUILD)/amiga/up-console.device $(BUILD)/terminfo/76/vtcon $(BUILD)/kit-terminfo/stamp $(UNIFONT_PAGES)/stamp $(EMOJI_PAGES)/stamp
	rm -rf $(BUILD)/dist && mkdir -p $(KIT)/Files/terminfo $(KIT)/Files/libs
	cd $(BUILD)/kit-terminfo && cp -R [a-z] $(CURDIR)/$(KIT)/Files/terminfo/
	cp $(SCREEN_BIN) $(KIT)/Files/screen
	cp dist/screenrc $(KIT)/Files/screenrc
	cp $(TMUX_BIN) $(KIT)/Files/tmux
	cp dist/tmux.conf dist/unstartup.sh dist/reassign.sh $(KIT)/Files/
	cp $(IXEMUL_LIB) $(KIT)/Files/libs/ixemul.library
	cp $(IXNET_LIB) $(KIT)/Files/libs/ixnet.library
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
	cp $(BUILD)/amiga/sz $(BUILD)/amiga/rz $(BUILD)/amiga/upgetty $(BUILD)/amiga/UPTerm $(BUILD)/amiga/UPDemo $(BUILD)/amiga/uptelnet $(BUILD)/amiga/hl $(BUILD)/amiga/mdv $(BUILD)/amiga/Claude $(KIT)/Files/
	rm -rf $(KIT)/Files/net && cp -R dist/net $(KIT)/Files/net
	# curl.020/.040/.060 are vendored Aminet binaries shipped with symbol hunks (~44 KB each); strip the kit's copies with the gcc-track strip (never on vbcc-built files)
	for f in $(KIT)/Files/net/curl-*/curl.0?0; do $(AMIGA_STRIP) $$f; done
	rm -rf $(KIT)/Files/fonts && cp -R dist/fonts $(KIT)/Files/fonts
	mkdir -p $(KIT)/Files/unifont && cp $(UNIFONT_PAGES)/[0-9A-F][0-9A-F] $(UNIFONT_PAGES)/1F[0-9A] dist/unifont/OFL-1.1.txt dist/unifont/SOURCE.txt $(KIT)/Files/unifont/
	mkdir -p $(KIT)/Files/emoji && cp $(EMOJI_PAGES)/[0-9A-F][0-9A-F] $(EMOJI_PAGES)/1F[0-9A] dist/emoji/CC-BY-4.0.txt dist/emoji/SOURCE.txt $(KIT)/Files/emoji/
	rm -rf $(KIT)/Files/wasabi && cp -R dist/wasabi $(KIT)/Files/wasabi
	cp $(BUILD)/amiga/wasabikey $(KIT)/Files/wasabi/
	cp terminfo/vtcon.termcap $(KIT)/Files/termcap.vtcon
	# the top drawer: Install (the Installer script), Uninstall, README, Files
	cp dist/Install.installer $(KIT)/Install
	python3 tools/mkicon.py $(KIT)/Install.info --tool Installer --plain \
	  --tooltype APPNAME=UP-Term --tooltype MINUSER=AVERAGE --tooltype DEFUSER=AVERAGE
	cp dist/Uninstall dist/README.txt dist/LICENSES.txt $(KIT)/
	sh tools/mkversions.sh $(UPTERM_ROOT) > $(KIT)/Files/VERSIONS
	rm -rf $(KIT)/Files/python3 $(KIT)/Files/nvim
	cp -R $(PYTHON_DIST) $(KIT)/Files/python3
	cp -R $(NVIM_DIST) $(KIT)/Files/nvim
	cp dist/ClaudeCode $(KIT)/ClaudeCode
	cp dist/ClaudeCode $(KIT)/Files/ClaudeCode
	python3 tools/mkicon.py $(KIT)/ClaudeCode.info --tool C:IconX \
	  --tooltype "WINDOW=XCON:0/12/800/560/Claude Code/CLOSE" --tooltype DELAY=0
	python3 tools/mkicon.py $(KIT)/Uninstall.info --tool C:IconX --plain
	cd $(BUILD)/dist && rm -f ../UP-Term.lha && lha -aq ../UP-Term.lha UP-Term
	@ls -la $(BUILD)/UP-Term.lha

# The kit says what it was built from: Files/VERSIONS has a line per part, and
# it is in the lha archive.
dist-check:
	python3 tests/test_dist_versions.py
	python3 tests/test_dist_installer.py
	python3 tests/test_rig_fixtures.py

# The one reachability test: XCON: through DOS on the running rig.
test-rig: amiga
	python3 tools/rig/reach.py

clean:
	rm -rf $(BUILD)
