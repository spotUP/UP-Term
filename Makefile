# vtcon -- host tests and the Amiga cross build. See RULES.md ## Commands.

HOSTCC  ?= cc
HOSTCFLAGS := -std=c89 -pedantic -Wno-long-long -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -DVT_CHECK_USED
BUILD   := build

# a recipe that fails must not leave its target behind: a 0-byte build/amiga/wbrun once counted as up to date (M2)
.DELETE_ON_ERROR:

# The workspace directory that holds vtcon and the other UP-Term repos side by
# side (see the upterm meta-repo); every sibling default below hangs off it.
# tools/rig/paths.py reads the same variable, with the same default.
UPTERM_ROOT ?= $(abspath $(CURDIR)/..)
# The directory of the projects that are not UP-Term repos (DCTelnet/, AmiExpress/, UpRough/): CODE_ROOT, else the
# nearest ancestor of UPTERM_ROOT holding DCTelnet/. One rule for the flat layout and the ~/Code/UP-Term umbrella;
# it lives in tools/rig/paths.py (code_root), which the rig scripts read too.
ifndef CODE_ROOT
CODE_ROOT := $(shell UPTERM_ROOT='$(UPTERM_ROOT)' python3 -I $(CURDIR)/tools/rig/paths.py code-root)
endif
DCTELNET_ROOT ?= $(CODE_ROOT)/DCTelnet
AMIEXPRESS_ROOT ?= $(CODE_ROOT)/AmiExpress

ENGINE  := engine/vtengine.c
RENDER  := render/glyphmap.c render/unifont.c render/emoji.c render/fontpair.c render/sbar.c render/otag.c render/painter.c handler/lineedit.c handler/kcmode.c handler/slash.c handler/clipfmt.c handler/complete_core.c render/vtinput.c handler/waitset.c handler/histfile.c
SHELL_CORE := shell/sh_parse.c shell/sh_expand.c shell/sh_exec.c shell/sh_float.c
TTY     := tty/ldisc.c tty/bmsg.c
DEVICE_CORE := device/upc_core.c
PROC_CORE := proc/proc_core.c
CONF    := config/upconf.c
TERMURL := config/termurl.c
PREFS_CORE := prefs/prefs_core.c prefs/prefs_ui.c
ICONSPEC := install/iconspec.c install/updiff.c
ZMODEM  := zm/zmodem.c
TELNET  := net/tn.c net/upagent.c
# hl and mdv (view/): the lexer, themes, the Markdown renderer
include sources.mk
VIEW_HL   := $(VIEW_LEX) view/hl_view.c view/md.c # md.c: hl -p shows Markdown as mdv does
VIEW_CLI  := view/vw_cli.c
# the view, LLM and Claude source lists live in sources.mk (amiga-pi builds from them too)
# every header a host binary can include: a header-only change rebuilds the host tests (tests/test_host_deps.py)
HOST_HDR := $(wildcard engine/*.h engine/*.inc render/*.h render/*.inc handler/*.h shell/*.h tty/*.h device/*.h config/*.h \
              prefs/*.h install/*.h zm/*.h net/*.h view/*.h demo/*.h demo/*.inc claude/*.h llm/*.h tests/*.h tests/exec_host/*.h)
TESTS   := tests/harness.c tests/test_main.c tests/test_xterm.c tests/test_keys.c \
           tests/test_amiga.c tests/test_reflow.c tests/test_sixel.c tests/test_pcansi.c tests/test_glyph.c tests/test_mirror.c tests/test_lineedit.c \
           tests/test_sh_parse.c tests/test_sh_expand.c tests/test_sh_exec.c tests/test_ldisc.c \
           tests/test_upcon.c tests/test_upconf.c tests/test_prefs.c tests/test_iconspec.c tests/test_updiff.c tests/test_zmodem.c tests/test_otag.c tests/test_slash.c tests/test_fontpair.c tests/test_updemo.c tests/test_pace.c tests/test_painter.c tests/test_text.c tests/test_clip.c \
           tests/test_input.c tests/test_protocol.c tests/test_sbar.c tests/test_telnet.c tests/test_upagent.c tests/test_complete.c tests/test_winmem.c tests/test_sbpack.c tests/test_hl.c tests/test_md.c \
           tests/claude_load.c tests/claude_vol.c tests/claude_screen.c tests/pi_stub.c tests/test_claude_http.c tests/test_claude_json.c tests/test_claude_stream.c tests/test_claude_tools.c tests/test_claude_match.c tests/test_claude_config.c tests/test_pi_llm.c tests/test_claude_repl.c tests/test_claude_cli.c tests/test_claude_tui.c tests/test_unifont.c tests/test_emoji.c tests/test_waitset.c tests/test_brk.c tests/test_sh_pipe.c tests/test_sh_ixargv.c tests/test_sh_own.c tests/test_histfile.c tests/test_pens.c tests/test_proc.c tests/test_pty_name.c tests/test_bmsg.c tests/test_qmark.c tests/test_ttyown.c tests/test_upassign.c

.PHONY: wasabi wasabi-clean bebbossh bebbossh-clean bashdiff ratchet-sort unifont emoji claude-tls-check widths demo-host view-host test test-ref te-diff test-terminfo test-rig dist dist-check golden vttest venv capture quirks amiga clean

# ONLY=claude_fixture the fake Messages API's routes (tools/test_claude_fixture.py)
# ONLY=rig_paths a worktree's rig scripts use the main checkout's build/rig<N> (tools/test_rig_paths.py)
# ONLY=rig_wasabimcp tools/rig/wasabimcp.py picks the wasabid target from the environment (tools/test_rig_wasabimcp.py)
# ONLY=rig_ami tools/rig/ami.py over wasabid, every function against wasabi's mock daemon (tools/test_rig_ami.py; needs `make wasabi`)
# ONLY=rig_wasabi rig.py wasabi: arguments, the port per rig, the log paths, the process list parser (tools/test_rig_wasabi.py)
# ONLY=rig_muforce rig.py muforce: the MuForce report parser and argument handling (tools/test_rig_muforce.py)
# ONLY=rig_claude_import importing tools/rig/claude_rig.py runs nothing (tools/test_rig_claude_import.py)
# ONLY=rig_stdin amiga-pi in a rig window script gets <NIL: (tools/test_rig_stdin.py)
# ONLY=rig_libs the rig's boot keeps AmiSSL's drawer in LIBS: (tools/test_rig_libs.py)
# ONLY=rig_kit a rig with a kit boots the builds of VTC: over the kit's copies in SYS: (tools/test_rig_kit.py)
# ONLY=rig_upterm the rig has UP-Term:bin/vsh, the SYS:System icon and no global TERM (tools/test_rig_upterm.py)
# ONLY=pi_recordings amiga-pi's OpenAI recordings against OpenAI's schema (tools/test_pi_recordings.py)
# ONLY=layers amiga-pi's layering rule over llm/, claude/http.c and UPTERM_ROOT/amiga-pi (tools/check_layers.py)
# ONLY=render render/amiga_render.c on the host against a fake RastPort (tests/test_render.c, tests/render_host/)
# ONLY=uptelnetd runs the Mac end's tests alone (tools/test_uptelnetd.py); ONLY=ixemul the patched
# ixemul's host tests (IXEMUL_SRC/tests/host: its argument line parser)
test: $(BUILD)/vttest_host $(BUILD)/vtdump $(BUILD)/tn_host $(BUILD)/vsh_host $(BUILD)/vsh_host_leak $(BUILD)/hl $(BUILD)/mdv $(BUILD)/unorm_drv $(BUILD)/pi_search_drv $(BUILD)/updiff_stack_drv $(BUILD)/render_host
	@if [ "$(ONLY)" != uptelnetd ] && [ "$(ONLY)" != render ] && [ "$(ONLY)" != terminfo_amiga ] && [ "$(ONLY)" != claude_fixture ] && [ "$(ONLY)" != rig_paths ] && [ "$(ONLY)" != rig_stdin ] && [ "$(ONLY)" != rig_claude_import ] && [ "$(ONLY)" != rig_wasabimcp ] && [ "$(ONLY)" != rig_muforce ] && [ "$(ONLY)" != rig_wasabi ] && [ "$(ONLY)" != rig_ami ] && [ "$(ONLY)" != rig_libs ] && [ "$(ONLY)" != rig_upterm ] && [ "$(ONLY)" != rig_kit ] && [ "$(ONLY)" != layers ] && [ "$(ONLY)" != pi_recordings ] && [ "$(ONLY)" != fonts ] && [ "$(ONLY)" != bashdiff ] && [ "$(ONLY)" != installer ] && [ "$(ONLY)" != docs_setenv ] && [ "$(ONLY)" != hl ] && [ "$(ONLY)" != entry ] && [ "$(ONLY)" != deps ] && [ "$(ONLY)" != ixemul ] && [ "$(ONLY)" != man ] && [ "$(ONLY)" != unorm ] && [ "$(ONLY)" != pi_search ] && [ "$(ONLY)" != make_hygiene ]; then ./$(BUILD)/vttest_host $(ONLY); fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = uptelnetd ]; then python3 tools/test_uptelnetd.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = claude_fixture ]; then python3 tools/test_claude_fixture.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_paths ]; then python3 -I tools/test_rig_paths.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_wasabimcp ]; then python3 -I tools/test_rig_wasabimcp.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_muforce ]; then python3 -I tools/test_rig_muforce.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_wasabi ]; then python3 -I tools/test_rig_wasabi.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_ami ]; then if [ -f "$(BUILD)/wasabi/src/tests/mock-wasabid.py" ] || [ -f "$(CURDIR)/../../$(BUILD)/wasabi/src/tests/mock-wasabid.py" ] || [ -n "$(ONLY)" ]; then python3 -I tools/test_rig_ami.py; else echo "[SKIP] rig_ami: wasabi is not built (make wasabi)"; fi; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_claude_import ]; then python3 -I tools/test_rig_claude_import.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_stdin ]; then python3 -I tools/test_rig_stdin.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_libs ]; then python3 -I tools/test_rig_libs.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_upterm ]; then python3 -I tools/test_rig_upterm.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = rig_kit ]; then python3 -I tools/test_rig_kit.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = pi_recordings ]; then python3 -I tools/test_pi_recordings.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = layers ]; then python3 -I tools/test_check_layers.py && python3 -I tools/check_layers.py --strict; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = pi_llm ]; then python3 -I tools/gen_models_builtin.py --check || { echo "[ERROR] llm/models_builtin.json or .h is out of date: python3 tools/gen_models_builtin.py --from-pi && python3 tools/gen_models_builtin.py"; exit 1; }; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = unifont ]; then python3 tests/test_gen_unifont.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = unorm ]; then python3 -I tests/test_unorm.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = pi_search ]; then python3 -I tests/test_pi_search.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = updiff ]; then ./$(BUILD)/updiff_stack_drv; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = render ]; then ./$(BUILD)/render_host; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = fonts ]; then python3 tests/test_dist_fonts.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_installer.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_manifest.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_kitsize.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_bebbossh.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ] || [ "$(ONLY)" = addons ]; then python3 tests/test_dist_addons.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ] || [ "$(ONLY)" = addons ]; then python3 tests/test_dist_userdata.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ] || [ "$(ONLY)" = packages ]; then python3 tests/test_dist_packages.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ] || [ "$(ONLY)" = packages ]; then python3 tests/test_dist_assemble.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_dist_modes.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ] || [ "$(ONLY)" = docs_setenv ]; then python3 -I tests/test_doc_setenv_save.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_rig_fixtures.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_rig_verdicts.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = installer ]; then python3 tests/test_cube_check.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = terminfo_amiga ]; then python3 tests/test_terminfo_amiga.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = emoji ]; then python3 tests/test_gen_emoji.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = make_hygiene ]; then python3 -I tests/test_make_delete_on_error.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = hl ]; then python3 tests/test_hl_plain.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = vshrc_net ]; then python3 tests/test_vshrc_net.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = entry ]; then python3 tests/test_entry_stub.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = qmark ]; then python3 -I tests/test_qmark_commands.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = deps ]; then python3 tests/test_host_deps.py; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = ixemul ]; then $(MAKE) -s -C $(IXEMUL_SRC)/tests/host test; fi
	@# UP-Term's own manual pages (man/*.1, mdoc): no mandoc warning (macOS has mandoc)
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = man ]; then for f in man/*.1; do mandoc -T lint -W warning $$f || exit 1; done; echo "[OK] man/*.1 lint clean"; fi
	@if [ -z "$(ONLY)" ] || [ "$(ONLY)" = bashdiff ]; then python3 tools/bashdiff.py --gate && python3 tools/bashdiff.py --leak; fi

$(BUILD)/vttest_host: $(HOST_HDR) $(CLAUDE_CORE) $(CLAUDE_HDR) $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(PROC_CORE) proc/proc_core.h $(CONF) $(TERMURL) config/termurl.h $(PREFS_CORE) $(ICONSPEC) install/iconspec.h install/updiff.h $(ZMODEM) $(TELNET) net/tn.h demo/updemo.c demo/updemo.h demo/tour_themes.inc zm/zmodem.h device/upc_core.h config/upconf.h prefs/prefs_core.h tty/ldisc.h tty/bmsg.h tty/qmark.h handler/ttyown.h shell/sh_parse.h shell/sh_expand.h shell/sh_exec.h engine/vtengine.h engine/vtwidth.h handler/complete_core.h render/glyphmap.h render/unifont.h render/emoji.h render/fontpair.h render/sbar.h render/pace.h render/otag.h handler/lineedit.h handler/slash.h handler/menu_ids.h render/glyph_tables.inc render/synchold.h engine/vtcaps.inc terminfo/vtcon.terminfo $(VIEW_CORE) $(VIEW_HDR) $(TESTS) tests/harness.h tests/claude_screen.h handler/clipfmt.h render/vtinput.h handler/brk.c handler/brk.h tests/exec_host/exec_host.h
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -DVT_COUNT_ALLOC -Itests/exec_host -o $@ handler/brk.c $(ENGINE) $(RENDER) $(SHELL_CORE) $(TTY) $(DEVICE_CORE) $(PROC_CORE) $(CONF) $(TERMURL) $(PREFS_CORE) $(ICONSPEC) $(ZMODEM) $(TELNET) demo/updemo.c $(VIEW_CORE) $(CLAUDE_CORE) $(TESTS)

# render/amiga_render.c itself on the host (ledger R8): the NDK's structs and a fake graphics.library
# (tests/render_host/gfx_fake.c) that logs the calls and draws into a pixel canvas. Its own binary: the
# NDK headers and the engine's strict host build cannot share a translation unit (struct timeval).
# Needs the NDK of `make amiga` (VTCON_NDK); the sanitizers stay on.
RENDER_HOST_SRC := tests/test_render.c tests/render_host/gfx_fake.c render/amiga_render.c tests/harness.c $(ENGINE) render/glyphmap.c render/unifont.c render/emoji.c render/painter.c render/vtinput.c
$(BUILD)/render_host: $(HOST_HDR) tests/render_host/gfx_fake.h tests/render_host/proto/*.h $(RENDER_HOST_SRC)
	@mkdir -p $(BUILD)
	@test -d $(VTCON_NDK) || { echo "[ERROR] the NDK headers are missing: unpack NDK3.2R4 to $(VTCON_NDK) (see make amiga)"; exit 1; }
	$(HOSTCC) -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter -O1 -g -fsanitize=address,undefined -Itests -Itests/render_host -I$(VTCON_NDK) -DAMIGA_STDC_C99 -DVR_HOST_TEST -o $@ $(RENDER_HOST_SRC)

# amiga-pi's NFKC (claude/unorm.c) for tests/test_unorm.py
$(BUILD)/unorm_drv: tests/unorm_drv.c claude/unorm.c claude/unorm.h claude/unorm_tab.h claude/json.c claude/util.c
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -I. -Iclaude -o $@ tests/unorm_drv.c claude/unorm.c claude/json.c claude/util.c

# amiga-pi's grep and find (claude/search_pi.c) for tests/test_pi_search.py (against rg and git)
$(BUILD)/pi_search_drv: tests/pi_search_drv.c $(CLAUDE_CORE) $(CLAUDE_PI) $(CLAUDE_HDR) $(VIEW_CORE)
	@mkdir -p $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -I. -o $@ tests/pi_search_drv.c $(CLAUDE_CORE) $(CLAUDE_PI) $(VIEW_CORE) tty/bmsg.c handler/clipfmt.c

# ud_compare's stack use against the Amiga budget: plain -O2, NO sanitizers (they inflate frames)
$(BUILD)/updiff_stack_drv: tests/updiff_stack_drv.c install/updiff.c install/updiff.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c89 -pedantic -Wall -Wextra -Werror -O2 -I. -o $@ tests/updiff_stack_drv.c install/updiff.c

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
$(BUILD)/tn_host: $(HOST_HDR) net/tn.c net/tn.h llm/sha256.c llm/sha256.h tools/tn_host.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -o $@ net/tn.c llm/sha256.c tools/tn_host.c

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
DCTELNET ?= $(DCTELNET_ROOT)/dctelnet-v2
ART ?= $(AMIEXPRESS_ROOT)/amiexpress-doorserver/bbs_ads
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

# wasabi (MIT, https://github.com/creep-ltx/wasabi) pinned, for `rig.py wasabi`: wasabid (Amiga daemon, Bebbo gcc)
# and its stdlib Python client in build/wasabi. Pinned at 9e18cd0 (0.10, 2026-10-15: builds without the CyberGraphX developer headers). wasabid's TLS (optional at run
# time since 0.8: an emulator on loopback and an Amiga without AmiSSL or keys run plain) is compiled against the
# AmiSSL 5.27 SDK: AMISSL_SDK (the one `make amiga` uses, vendor/amissl-5.27/AmiSSL/Developer). The CyberGraphX
# headers are upstream's own since 0.9 (include/wasabi_cgx.h); no shim. The upstream Makefile's own flags (-fbbb=-: Bebbo's
# loop pass miscompiles small structs) are kept; only CC and the SDK are ours.
# Idempotent: the stamp holds the pinned commit. Uninstall: make wasabi-clean.
WASABI_REPO := https://github.com/creep-ltx/wasabi
WASABI_REV  := $(strip $(shell cat tools/nas/wasabi.rev))
wasabi: $(BUILD)/wasabi/.stamp-$(WASABI_REV)
	@echo "[OK] wasabi $(WASABI_REV) built: $(BUILD)/wasabi/wasabid and the client $(BUILD)/wasabi/wasabi."
$(BUILD)/wasabi/.stamp-$(WASABI_REV):
	@test -d "$(AMISSL_SDK)/include" || { echo "[ERROR] wasabi needs the AmiSSL 5.27 SDK: AMISSL_SDK=<its Developer directory>"; exit 1; }
	rm -rf $(BUILD)/wasabi
	mkdir -p $(BUILD)/wasabi
	git clone -q $(WASABI_REPO) $(BUILD)/wasabi/src
	git -C $(BUILD)/wasabi/src checkout -q $(WASABI_REV)
	$(MAKE) -C $(BUILD)/wasabi/src wasabid CC=$(AGCC) SSL=$(AMISSL_SDK) \
	  CFLAGS="-O2 -fbbb=- -noixemul -fomit-frame-pointer -Wall -Wno-pointer-sign -Iinclude"
	cp $(BUILD)/wasabi/src/wasabid $(BUILD)/wasabi/src/wasabi $(BUILD)/wasabi/src/wasabi_*.py $(BUILD)/wasabi/src/LICENSE $(BUILD)/wasabi/
	touch $@

wasabi-clean:
	rm -rf $(BUILD)/wasabi

# BebboSSH 1.46.1 built from source (owner 2026-10-13: ours, not the fork's binaries): the 1.45 tarball unpacked in
# build/bebbossh/src, the 19 patches applied in order (any reject stops the build), built with the Bebbo gcc at AGCCBIN
# by the source's own Makefile and flags (tools/build_bebbossh.sh). Out: build/bebbossh/out (bebbossh, bebboscp,
# bebbosshd, bebbosshkeygen, libcryptossh.library 020/060, SHA256SUMS) which `make dist` puts in the kit's
# net/bebbossh-1.46.1. Needs the libcodesets 6.22 headers (git clone, build time only). Rebuilds when the tarball or a
# patch changes. Uninstall: make bebbossh-clean.
BEBBO_DIR := dist/net/bebbossh-1.46.1
AGCCBIN   ?= $(patsubst %/,%,$(dir $(AGCC)))
bebbossh: $(BUILD)/bebbossh/.stamp
	@echo "[OK] bebbossh built: $(BUILD)/bebbossh/out"
$(BUILD)/bebbossh/.stamp: tools/build_bebbossh.sh $(BEBBO_DIR)/bebbossh-1.45-src.tar.bz2 $(wildcard $(BEBBO_DIR)/patches/*.patch)
	tools/build_bebbossh.sh $(BEBBO_DIR) $(BUILD)/bebbossh $(AGCCBIN)
	touch $@
bebbossh-clean:
	rm -rf $(BUILD)/bebbossh

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
# directories (v/vtcon), which the Amiga's ncurses 5.5 reads, and in the hex
# ones tic on macOS writes (76/vtcon), which ncurses 6 reads (upterm-ports'
# less, nano, tput: built with cf_cv_mixedcase=no, Amiga names ignore case).
# vtcon-amiga says use=vtcon: after it, and TERMINFO points tic at the entries made so far.
# W46 /colors 16: xterm-16color and vtcon-amiga-16color are made from the compiled vtcon
# (tools/gen_16color_terminfo.py says why they are generated, not written).
KIT_TERMINFO := terminfo/vtcon.terminfo terminfo/vtcon-amiga.terminfo terminfo/screen.terminfo
$(BUILD)/kit-terminfo/stamp: $(KIT_TERMINFO) tools/gen_16color_terminfo.py
	rm -rf $(BUILD)/kit-terminfo $(BUILD)/kit-terminfo.tmp
	mkdir -p $(BUILD)/kit-terminfo $(BUILD)/kit-terminfo.tmp
	for f in $(KIT_TERMINFO); do TERMINFO=$(CURDIR)/$(BUILD)/kit-terminfo.tmp tic -x -o $(BUILD)/kit-terminfo.tmp $$f || exit 1; done
	python3 -I tools/gen_16color_terminfo.py $(BUILD)/kit-terminfo.tmp $(BUILD)/kit-16color.terminfo
	TERMINFO=$(CURDIR)/$(BUILD)/kit-terminfo.tmp tic -x -o $(BUILD)/kit-terminfo.tmp $(BUILD)/kit-16color.terminfo
	cd $(BUILD)/kit-terminfo.tmp && for p in */*; do n=$${p#*/}; c=$$(printf %s "$$n" | cut -c1); \
	  mkdir -p ../kit-terminfo/$$c && cp "$$p" ../kit-terminfo/$$c/; done
	cp -R $(BUILD)/kit-terminfo.tmp/. $(BUILD)/kit-terminfo/
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
# the console's history file (handler/histfile.c) by C:Claude's rule for growing files, over its DOS calls alone
HANDLER_DATADIR := claude/datadir.c claude/path.c claude/util.c claude/datadir_amiga.c
HANDLER_SRC := handler/handler_start.s $(ENGINE_68K) render/amiga_render_68k.s render/painter.c render/painter_68k.s render/painter.h handler/vtcon_handler.c handler/clip.c handler/lineedit.c handler/kcmode.c handler/complete.c handler/complete_core.c prefs/prefs_asl.c handler/brk.c handler/waitset.c handler/slash.c handler/sbar_gad.c $(ENGINE) render/amiga_render.c render/vtwin.c render/vtinput.c render/sbar.c render/glyphmap.c render/unifont.c render/emoji.c render/fontpair.c render/outline.c render/otag.c tty/ldisc.c config/upconf.c config/termurl.c prefs/prefs_core.c prefs/prefs_dos.c handler/clipfmt.c handler/histfile.c $(HANDLER_DATADIR)
HANDLER_HDR := engine/vtengine.h engine/vtcaps.inc engine/vtwidth.h render/amiga_render.h render/vtwin.h render/synchold.h render/vtinput.h render/sbar.h handler/sbar_gad.h render/glyphmap.h render/unifont.h render/emoji.h render/glyph_tables.inc render/outline.h render/otag.h \
               handler/clip.h handler/clipfmt.h handler/lineedit.h handler/le_fns.h handler/complete.h handler/complete_core.h handler/histfile.h claude/datadir.h claude/sys.h claude/datadir_amiga.h handler/brk.h handler/ttyown.h handler/slash.h handler/menu_ids.h handler/vtcon_packets.h tty/ldisc.h device/upc_public.h config/upconf.h config/termurl.h config/upassign.h

amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/up-console.device $(BUILD)/amiga/UPConsole $(BUILD)/amiga/pty-handler $(BUILD)/amiga/reach $(BUILD)/amiga/vtshow $(BUILD)/amiga/winbox $(BUILD)/amiga/sizewatch $(BUILD)/amiga/breakport $(BUILD)/amiga/ttyprobe $(BUILD)/amiga/dsrtime $(BUILD)/amiga/dripens $(BUILD)/amiga/wasabikey $(BUILD)/amiga/UPDemo $(BUILD)/amiga/cellbench $(BUILD)/amiga/wprobe $(BUILD)/amiga/phaseprobe $(BUILD)/amiga/engbench $(BUILD)/amiga/ptytest $(BUILD)/amiga/ixkill $(BUILD)/amiga/vsh $(BUILD)/amiga/ixpipe-handler $(BUILD)/amiga/upprefs $(BUILD)/amiga/upicon $(BUILD)/amiga/upupdate $(BUILD)/amiga/sz $(BUILD)/amiga/rz $(BUILD)/amiga/upgetty $(BUILD)/amiga/UPTerm $(BUILD)/amiga/uptelnet $(BUILD)/amiga/hl $(BUILD)/amiga/mdv $(BUILD)/amiga/ps $(BUILD)/amiga/top $(BUILD)/amiga/Claude

# hl and mdv: the portable view/ core and the AmigaDOS side (no ixemul).
# vbcc warns (153, 65) on the (void) parameter casts, as for vsh.
# ps and top (Unix tool ports 3.3): proc/proc_core.c host-tested (suite proc); top's VBlank sampler in proc/vbsample.s
$(BUILD)/amiga/ps: proc/ps_amiga.c proc/tasks_amiga.c proc/tasks_amiga.h $(PROC_CORE) proc/proc_core.h $(BUILD)/amiga/obj/cpuchk-ps.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ proc/ps_amiga.c proc/tasks_amiga.c $(PROC_CORE) $(BUILD)/amiga/obj/cpuchk-ps.o
$(BUILD)/amiga/top: proc/top_amiga.c proc/tasks_amiga.c proc/tasks_amiga.h proc/vbsample.s $(PROC_CORE) proc/proc_core.h handler/vtcon_packets.h tty/ldisc.h $(BUILD)/amiga/obj/cpuchk-top.o
	@mkdir -p $(BUILD)/amiga/obj
	vasmm68k_mot -quiet -Fhunk -o $(BUILD)/amiga/obj/vbsample.o proc/vbsample.s
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ proc/top_amiga.c proc/tasks_amiga.c $(PROC_CORE) $(BUILD)/amiga/obj/vbsample.o $(BUILD)/amiga/obj/cpuchk-top.o
$(BUILD)/amiga/busyloop: tests/amiga/busyloop.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/busyloop.c
$(BUILD)/amiga/intlist: tests/amiga/intlist.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/intlist.c
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
# amiga-pi B12: one prompt through llm_request on the Amiga (tools/rig/amigapi_rig.py); plain http only
PI_PROBE_SRC := tests/amiga/pi_llmprobe.c $(LLM_CORE) claude/json.c claude/util.c claude/http.c claude/sse.c \
                claude/tls_none.c tty/bmsg.c
$(BUILD)/amiga/pi-llmprobe: $(PI_PROBE_SRC) claude/net_amiga.c $(CLAUDE_HDR) tty/bmsg.h
	@mkdir -p $(BUILD)/amiga/obj/pi
	$(VC) -dontwarn=153,65 -I$(VTCON_NETINCLUDE) -c -o $(BUILD)/amiga/obj/pi/net_amiga.o claude/net_amiga.c
	$(VC) -dontwarn=153,65 -o $@ $(PI_PROBE_SRC) $(BUILD)/amiga/obj/pi/net_amiga.o

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
VSH_SRC := shell/vsh.c shell/vsh_mem.c shell/sh_exec.c shell/sh_expand.c shell/sh_parse.c shell/sh_float.c claude/regex.c config/termurl.c tty/bmsg.c
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

# muhit: one illegal access on purpose, for MuForce (rig.py muforce, tools/rig/muforce.py)
$(BUILD)/amiga/muhit: tests/amiga/muhit.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/muhit.c

$(BUILD)/amiga/pipeprobe: tests/amiga/pipeprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/pipeprobe.c

# amiga-pi K1: which pipe handler gives a spawned child line-buffered stdio and EOF
$(BUILD)/amiga/spawnprobe: tests/amiga/spawnprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/spawnprobe.c

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
$(BUILD)/amiga/upprefs: prefs/upprefs.c prefs/prefs_core.c prefs/prefs_core.h prefs/prefs_dos.c prefs/prefs_dos.h prefs/prefs_asl.c prefs/prefs_asl.h prefs/prefs_ui.c prefs/prefs_ui.h handler/kcmode.c handler/kcmode.h handler/slash.c handler/slash.h config/upconf.c config/upconf.h $(BUILD)/amiga/obj/cpuchk-upprefs.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ prefs/upprefs.c prefs/prefs_core.c prefs/prefs_dos.c prefs/prefs_asl.c prefs/prefs_ui.c handler/kcmode.c handler/slash.c config/upconf.c $(BUILD)/amiga/obj/cpuchk-upprefs.o

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

# FIONREAD on a vsh pipe (PTY:<id>/r): what it holds, 0 at end of file (cat -n)
$(BUILD)/amiga/ixfionread: tests/amiga/ixfionread.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixfionread.c

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

# the kit's update tool (Install: what changed since the installed kit; installer update plan U4)
$(BUILD)/amiga/upupdate: install/upupdate.c install/updiff.c install/updiff.h $(BUILD)/amiga/obj/cpuchk-upupdate.o
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -dontwarn=153,65 -o $@ install/upupdate.c install/updiff.c $(BUILD)/amiga/obj/cpuchk-upupdate.o

$(BUILD)/amiga/iconprobe: tests/amiga/iconprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/iconprobe.c

$(BUILD)/amiga/wbrun: tests/amiga/wbrun.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/wbrun.c

$(BUILD)/amiga/taskdump: tests/amiga/taskdump.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/taskdump.c

$(BUILD)/amiga/inwho: tests/amiga/inwho.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/inwho.c

$(BUILD)/amiga/taskpath: tests/amiga/taskpath.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/taskpath.c

# How dos.library's ReadItem/ReadArgs read an argument line, and the argv
# ixemul's _cli_parse makes of the same line (tools/rig/ixargv_rig.py)
$(BUILD)/amiga/readitem: tests/amiga/readitem.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/readitem.c

$(BUILD)/amiga/ixargv: tests/amiga/ixargv.c
	@mkdir -p $(BUILD)/amiga
	$(AGCC) -mcrt=ixemul -O2 -Wall -o $@ tests/amiga/ixargv.c

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
# W-A3: the commands answering a lone ? (tty/qmark.h, header-only) rebuild when it changes
$(BUILD)/amiga/UPDemo $(BUILD)/amiga/UPConsole $(BUILD)/amiga/UPTerm $(BUILD)/amiga/upicon $(BUILD)/amiga/upupdate $(BUILD)/amiga/upgetty $(BUILD)/amiga/uptelnet $(BUILD)/amiga/sz $(BUILD)/amiga/rz $(BUILD)/amiga/Claude $(BUILD)/amiga/upprefs $(BUILD)/amiga/hl $(BUILD)/amiga/mdv $(BUILD)/amiga/ps $(BUILD)/amiga/top $(BUILD)/amiga/ixkill $(BUILD)/amiga/vsh: tty/qmark.h

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

# Picasso96's 24-bit pens (RPTAG_PenMode/FgColor/BgColor) on the Workbench screen (W-T13)
$(BUILD)/amiga/rgbprobe: tests/amiga/rgbprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/rgbprobe.c

# What a window on a public screen can get for direct colours (UT4: depth, entries, exclusive pens)
$(BUILD)/amiga/penprobe: tests/amiga/penprobe.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/penprobe.c

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
VTCON_NETINC ?= $(firstword $(wildcard $(CURDIR)/vendor/roadshow-netinclude $(DCTELNET_ROOT)/dctelnet-v2/src/third_party/netinclude) $(CURDIR)/vendor/roadshow-netinclude)
$(BUILD)/amiga/uptelnet: net/uptelnet.c net/tn.c net/tn.h net/upagent.c net/upagent.h llm/sha256.c llm/sha256.h handler/vtcon_packets.h tty/ldisc.h tty/bmsg.h $(BUILD)/amiga/obj/cpuchk-uptelnet.o
	@test -d $(VTCON_NETINC) || { echo "[ERROR] C:uptelnet needs the Roadshow netinclude headers, not found at $(VTCON_NETINC): unpack them to vendor/roadshow-netinclude, or pass VTCON_NETINC=<dir> (DCTelnet's copy: dctelnet-v2, optional in upterm's repos.lock)"; exit 1; }
	@mkdir -p $(BUILD)/amiga
	$(VC) -Dmain=up_main -I$(VTCON_NETINC) -o $@ net/uptelnet.c net/tn.c net/upagent.c llm/sha256.c tty/bmsg.c $(BUILD)/amiga/obj/cpuchk-uptelnet.o

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

# DV5's close probe (tools/rig/devverify_rig.py): CloseDevice with a read pending
$(BUILD)/amiga/closeread: tests/amiga/closeread.c
	@mkdir -p $(BUILD)/amiga
	$(VC) -o $@ tests/amiga/closeread.c

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
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/histfile.o handler/histfile.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/hd_datadir.o claude/datadir.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/hd_path.o claude/path.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -c -o $(BUILD)/amiga/obj/hd_util.o claude/util.c
	$(VC) -DVT_AMIGA_EXEC_ALLOC -dontwarn=153,65 -c -o $(BUILD)/amiga/obj/hd_datadir_amiga.o claude/datadir_amiga.c
	$(VC) -c -o $(BUILD)/amiga/obj/complete_core.o handler/complete_core.c
	$(VC) -c -o $(BUILD)/amiga/obj/kcmode.o handler/kcmode.c
	$(VC) -c -o $(BUILD)/amiga/obj/prefs_asl.o prefs/prefs_asl.c
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
	  $(BUILD)/amiga/obj/clip.o $(BUILD)/amiga/obj/clipfmt.o $(BUILD)/amiga/obj/lineedit.o $(BUILD)/amiga/obj/kcmode.o $(BUILD)/amiga/obj/complete.o $(BUILD)/amiga/obj/complete_core.o \
	  $(BUILD)/amiga/obj/brk.o $(BUILD)/amiga/obj/waitset.o $(BUILD)/amiga/obj/slash.o $(BUILD)/amiga/obj/ldisc.o $(BUILD)/amiga/obj/upconf.o $(BUILD)/amiga/obj/termurl.o \
	  $(BUILD)/amiga/obj/prefs_core.o $(BUILD)/amiga/obj/prefs_dos.o $(BUILD)/amiga/obj/prefs_asl.o \
	  $(BUILD)/amiga/obj/histfile.o $(BUILD)/amiga/obj/hd_datadir.o $(BUILD)/amiga/obj/hd_path.o $(BUILD)/amiga/obj/hd_util.o $(BUILD)/amiga/obj/hd_datadir_amiga.o \
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
# the archive names are dist/packages.txt's (Claude, amiga-pi and Python3 stand alone)
KIT_claude := $(BUILD)/dist/Claude
KIT_pi := $(BUILD)/dist/amiga-pi
KIT_nvim := $(BUILD)/dist/UP-Term-nvim
KIT_python := $(BUILD)/dist/Python3
# the patched ixemul (P6): built in ixemul-vtcon with sh docker/build.sh
IXEMUL_LIB ?= $(UPTERM_ROOT)/ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library
# Python 3.14 and Neovim 0.12, built in their own repos (make dist there)
PYTHON_DIST ?= $(UPTERM_ROOT)/cpython-amiga/build/m68k/dist/Python3
NVIM_DIST ?= $(UPTERM_ROOT)/neovim-amiga/build/v012/dist/nvim
# ixnet.library from the same build (ixnet refuses an ixemul of another revision)
IXNET_LIB ?= $(dir $(IXEMUL_LIB))../../../../ixnet/68020/amigaos/ixnet.library
# the Unix tool ports: staged by `make kit-stage` in upterm-ports (programs,
# pages, licences, SOURCES.txt) with their source beside it; the source goes
# into build/UP-Term-src.lha, published next to the kit (plan D2)
UPTERM_PORTS ?= $(UPTERM_ROOT)/upterm-ports
PORTS_KIT ?= $(UPTERM_PORTS)/build/kit/userland
# amiga-pi, the pi coding agent port (its own repo): built by its Makefile, which reads
# vtcon's core in place; the kit gets the binary as C:Claude's sits beside it
AMIGA_PI ?= $(UPTERM_ROOT)/amiga-pi
AMIGA_PI_BIN := $(AMIGA_PI)/build/amiga/amiga-pi
# the Amiga-dev skills amiga-pi ships (one drawer each, SKILL.md): the add-on installs them beside the user's own
AMIGA_PI_SKILLS := $(AMIGA_PI)/skills
.PHONY: amiga-pi-bin
amiga-pi-bin:
	$(MAKE) -C $(AMIGA_PI) UPTERM_ROOT=$(UPTERM_ROOT) amiga
dist: dist-base dist-claude dist-pi dist-nvim dist-python dist-src

# every archive of dist/packages.txt, and the source archive
# the base archive build/UP-Term.lha: the terminal, vsh, the Unix userland, the network tools. Neovim,
# Python, Claude and amiga-pi are archives of their own (dist/packages.txt, kit-split plan)
dist-base: amiga bebbossh $(BUILD)/amiga/UPConsole $(BUILD)/amiga/up-console.device $(BUILD)/terminfo/76/vtcon $(BUILD)/kit-terminfo/stamp $(UNIFONT_PAGES)/stamp $(EMOJI_PAGES)/stamp
	rm -rf $(KIT) && mkdir -p $(KIT)/Files/terminfo $(KIT)/Files/libs
	cd $(BUILD)/kit-terminfo && cp -R [a-z] [0-9]* $(CURDIR)/$(KIT)/Files/terminfo/
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
	cp $(BUILD)/amiga/vtcon-handler $(BUILD)/amiga/UPConsole $(BUILD)/amiga/up-console.device $(BUILD)/amiga/pty-handler $(BUILD)/amiga/ixpipe-handler $(BUILD)/amiga/vsh $(BUILD)/amiga/ixkill dist/XCON dist/PTY dist/IXPIPE dist/vshrc \
	  dist/up-term.conf $(KIT)/Files/
	mkdir -p $(KIT)/Files/themes && cp themes/*.conf themes/ATTRIBUTION.md $(KIT)/Files/themes/
	rm -rf $(KIT)/Files/coreutils && mkdir -p $(KIT)/Files/coreutils
	cp -R dist/gg/coreutils-5.2.1/bin dist/gg/coreutils-5.2.1/COPYING dist/gg/coreutils-5.2.1/SOURCE.txt $(KIT)/Files/coreutils/
	@# the ports (copy-userland: bin; userland: share, etc, licenses) and
	@# UP-Term's own pages beside theirs (sz.1 is rz's too: no links on AmigaOS)
	@test -f $(PORTS_KIT)/SOURCES.txt || { echo "dist: no $(PORTS_KIT)/SOURCES.txt: make kit-stage in upterm-ports first"; exit 1; }
	rm -rf $(KIT)/Files/userland && cp -R $(PORTS_KIT) $(KIT)/Files/userland
	mkdir -p $(KIT)/Files/userland/share/man/man1 && cp man/*.1 $(KIT)/Files/userland/share/man/man1/
	cp man/sz.1 $(KIT)/Files/userland/share/man/man1/rz.1
	cp $(BUILD)/amiga/upprefs "$(KIT)/Files/UP-Term Prefs"
	cp $(BUILD)/amiga/upicon $(KIT)/Files/upicon
	cp $(BUILD)/amiga/upupdate $(KIT)/Files/upupdate
	cp $(BUILD)/amiga/sz $(BUILD)/amiga/rz $(BUILD)/amiga/upgetty $(BUILD)/amiga/UPTerm $(BUILD)/amiga/UPDemo $(BUILD)/amiga/uptelnet $(BUILD)/amiga/hl $(BUILD)/amiga/mdv $(BUILD)/amiga/ps $(BUILD)/amiga/top $(KIT)/Files/
	rm -rf $(KIT)/Files/net && cp -R dist/net $(KIT)/Files/net
	# BebboSSH: our build (make bebbossh) over the drawer's sources, patches and licence; SHA256SUMS and TOOLCHAIN.txt from the build
	cp $(BUILD)/bebbossh/out/* $(KIT)/Files/net/bebbossh-1.46.1/
	# curl.020/.040/.060 are vendored Aminet binaries shipped with symbol hunks (~44 KB each); strip the kit's copies with the gcc-track strip (never on vbcc-built files)
	for f in $(KIT)/Files/net/curl-*/curl.0?0; do $(AMIGA_STRIP) $$f; done
	rm -rf $(KIT)/Files/fonts && cp -R dist/fonts $(KIT)/Files/fonts
	mkdir -p $(KIT)/Files/unifont && cp $(UNIFONT_PAGES)/[0-9A-F][0-9A-F] $(UNIFONT_PAGES)/1F[0-9A] dist/unifont/OFL-1.1.txt dist/unifont/SOURCE.txt $(KIT)/Files/unifont/
	mkdir -p $(KIT)/Files/emoji && cp $(EMOJI_PAGES)/[0-9A-F][0-9A-F] $(EMOJI_PAGES)/1F[0-9A] dist/emoji/CC-BY-4.0.txt dist/emoji/SOURCE.txt $(KIT)/Files/emoji/
	@# wasabi: LICENSE and SOURCE.txt from dist/wasabi, the daemon and the client from the build (make wasabi, the pin tools/nas/wasabi.rev)
	@test -f $(BUILD)/wasabi/wasabid -a -f $(BUILD)/wasabi/wasabi -a -f $(BUILD)/wasabi/wasabi_mcp.py || { echo "dist: no $(BUILD)/wasabi/{wasabid,wasabi,wasabi_mcp.py}: make wasabi first"; exit 1; }
	rm -rf $(KIT)/Files/wasabi && cp -R dist/wasabi $(KIT)/Files/wasabi
	cp $(BUILD)/wasabi/wasabid $(BUILD)/wasabi/wasabi $(BUILD)/wasabi/wasabi_mcp.py $(BUILD)/wasabi/wasabi_client.py $(BUILD)/amiga/wasabikey $(KIT)/Files/wasabi/
	cp terminfo/vtcon.termcap $(KIT)/Files/termcap.vtcon
	# the shared tail of every archive: the scripts assembled from dist/ and dist/common/, texts, icons,
	# VERSIONS, UP-Term.version, modes, manifest, the lha archive and its Aminet readme (tools/mkkit.py)
	python3 tools/mkkit.py base
	@ls -la $(BUILD)/UP-Term.lha

# The add-on archives (dist/packages.txt): each stages its payload into build/dist/<archive>/Files and
# tools/mkkit.py does the rest. UP-Term-nvim needs the base (UP-Term.lha) installed first on the
# Amiga; Claude, amiga-pi and Python3 stand alone.
ADDON_STAGE = rm -rf $(BUILD)/dist/$(call pkg,$(1),archive) && mkdir -p $(BUILD)/dist/$(call pkg,$(1),archive)/Files \
	&& cp $(BUILD)/amiga/upupdate $(BUILD)/dist/$(call pkg,$(1),archive)/Files/
pkg = $(shell python3 tools/pkgs.py get $(1) $(2))
# W-I1: a quick-launch icon of an add-on, staged in Files/launch (the add-on's install.dos copies it into
# UP-Term:, part launch): an IconX script (dist/addons/<id>/launch/<name>) and its icon, which opens an UP-Term
# window (XCON:, close gadget, closed when the app ends; no DELAY tooltype: IconX's own few seconds keep an error
# readable, and DELAY=0 kept the window open for ever, rig 3 2026-10-14). $(1) kit drawer, $(2) script, $(3) name, $(4) size.
LAUNCHER = mkdir -p "$(1)/Files/launch" && cp "$(2)" "$(1)/Files/launch/$(3)" \
	&& python3 tools/mkicon.py "$(1)/Files/launch/$(3).info" --tool C:IconX \
	  --tooltype "WINDOW=XCON:0/12/$(4)/$(3)/CLOSE"
.PHONY: dist-claude dist-pi dist-nvim dist-python
dist-claude: $(BUILD)/amiga/Claude $(BUILD)/amiga/uptelnet $(BUILD)/amiga/upupdate
	$(call ADDON_STAGE,claude)
	cp $(BUILD)/amiga/Claude $(BUILD)/amiga/uptelnet $(KIT_claude)/Files/
	@test -f $(BUILD)/wasabi/wasabid || { echo "dist-claude: no $(BUILD)/wasabi/wasabid: make wasabi first"; exit 1; }
	cp $(BUILD)/wasabi/wasabid $(KIT_claude)/Files/wasabid
	cp dist/wasabi/LICENSE $(KIT_claude)/Files/wasabid.LICENSE
	$(call LAUNCHER,$(KIT_claude),dist/addons/claude/launch/Claude,Claude,800/560)
	python3 tools/mkkit.py claude
dist-pi: amiga-pi-bin $(BUILD)/amiga/upupdate
	$(call ADDON_STAGE,pi)
	cp $(AMIGA_PI_BIN) $(KIT_pi)/Files/amiga-pi
	cp $(AMIGA_PI)/CHANGELOG.md $(KIT_pi)/Files/CHANGELOG.md
	cp -R $(AMIGA_PI_SKILLS) $(KIT_pi)/Files/skills
	$(call LAUNCHER,$(KIT_pi),dist/addons/pi/launch/amiga-pi,amiga-pi,800/560)
	python3 tools/mkkit.py pi
dist-nvim: $(BUILD)/amiga/upupdate
	$(call ADDON_STAGE,nvim)
	cp -R $(NVIM_DIST) $(KIT_nvim)/Files/nvim
	cp dist/addons/nvim/sysinit.vim $(KIT_nvim)/Files/nvim/share/nvim/sysinit.vim
	$(call LAUNCHER,$(KIT_nvim),dist/addons/nvim/launch/Neovim,Neovim,800/560)
	python3 tools/mkkit.py nvim
dist-python: $(BUILD)/amiga/upupdate
	$(call ADDON_STAGE,python)
	cp -R $(PYTHON_DIST) $(KIT_python)/Files/python3
	mkdir -p $(KIT_python)/Files/libs
	cp $(IXEMUL_LIB) $(KIT_python)/Files/libs/ixemul.library
	cp $(IXNET_LIB) $(KIT_python)/Files/libs/ixnet.library
	$(call LAUNCHER,$(KIT_python),dist/addons/python/launch/Python 3,Python 3,640/400)
	python3 tools/mkkit.py python

# build/UP-Term-src.lha, published next to the kit: the Unix tools' source (upterm-ports) and the
# complete source of GNU coreutils (GPL v2 section 3: the same place as the binaries)
.PHONY: dist-base dist-src
dist-src:
	@test -f $(PORTS_KIT)/SOURCES.txt || { echo "dist-src: no $(PORTS_KIT)/SOURCES.txt: make kit-stage in upterm-ports first"; exit 1; }
	rm -rf $(BUILD)/dist-src && mkdir -p $(BUILD)/dist-src/coreutils-5.2.1
	cp dist/gg/coreutils-5.2.1/coreutils-5.2.1-src.tar.bz2 dist/gg/coreutils-5.2.1/SOURCE.txt dist/gg/coreutils-5.2.1/COPYING $(BUILD)/dist-src/coreutils-5.2.1/
	cd $(PORTS_KIT)/.. && rm -f $(CURDIR)/$(BUILD)/UP-Term-src.lha && lha -aq $(CURDIR)/$(BUILD)/UP-Term-src.lha userland-src
	cd $(BUILD)/dist-src && lha -aq ../UP-Term-src.lha coreutils-5.2.1
	python3 tools/mkreadme.py src
	@ls -la $(BUILD)/UP-Term-src.lha

# The kit says what it was built from: Files/VERSIONS has a line per part, and
# it is in the lha archive.
dist-check:
	python3 tests/test_dist_installer.py
	KITSIZE_REQUIRE=1 python3 tests/test_dist_kitsize.py
	python3 tests/test_dist_addons.py
	python3 tests/test_dist_launch.py
	python3 tests/test_dist_userdata.py
	python3 tests/test_rig_fixtures.py
	python3 tests/test_rig_verdicts.py
	@set -e; for a in $$(python3 tools/pkgs.py archives); do \
	  test -s $(BUILD)/$$a.lha || { echo "dist-check: $(BUILD)/$$a.lha missing: make dist"; exit 1; }; \
	  lha l $(BUILD)/$$a.lha > $(BUILD)/$$a.lst; \
	  for f in Install Uninstall Files/VERSIONS Files/MANIFEST Files/install.dos; do \
	    grep -q " $$a/$$f$$" $(BUILD)/$$a.lst || { echo "dist-check: $$a.lha lacks $$a/$$f"; exit 1; }; done; \
	  test -s $(BUILD)/$$a.readme || { echo "dist-check: $(BUILD)/$$a.readme missing"; exit 1; }; \
	  echo "dist-check: $$a"; \
	  KIT=$(BUILD)/dist/$$a python3 tests/test_dist_versions.py; \
	  KIT=$(BUILD)/dist/$$a python3 tests/test_dist_manifest.py; \
	  KIT=$(BUILD)/dist/$$a python3 tests/test_dist_modes.py; \
	done
	@for s in amigados arexx amiga-c-build amiga-autodocs; do grep -q " amiga-pi/Files/skills/$$s/SKILL.md$$" $(BUILD)/amiga-pi.lst || { echo "dist-check: amiga-pi.lha lacks Files/skills/$$s/SKILL.md"; exit 1; }; done
	@test -s $(BUILD)/UP-Term-src.lha || { echo "dist-check: $(BUILD)/UP-Term-src.lha missing: make dist"; exit 1; }
	@lha l $(BUILD)/UP-Term-src.lha | grep -q "coreutils-5.2.1/coreutils-5.2.1-src.tar.bz2" || { echo "dist-check: UP-Term-src.lha lacks the coreutils source"; exit 1; }

# The one reachability test: XCON: through DOS on the running rig.
test-rig: amiga
	python3 tools/rig/reach.py

clean:
	rm -rf $(BUILD)
