# vtcon.mk -- what a program built from vtcon's sources needs of vtcon: the paths of the sibling checkouts, the
# vbcc toolchain, the git stamp, the generated headers and the list of vtcon files Patina links.
# vtcon's Makefile includes it, and after the repo split so does Patina's (with VTCON=<vtcon checkout>;
# plan thoughts/shared/plans/2026-11-24-split-patina-repo.md, S3). Every vtcon path in here is below $(VTCON).
# Needs BUILD set before the include.
# Its rules must not become the includer's default goal: the goal is kept here and handed back at the end.
VTCON_MK_GOAL := $(.DEFAULT_GOAL)
VTCON ?= $(CURDIR)

# the generated version header; named here because the host rules above its own rule list it as a prerequisite
UPTERM_VERSION_H := $(BUILD)/amiga/upterm_version.h

# The workspace directory that holds vtcon and the other UP-Term repos side by
# side (see the upterm meta-repo); every sibling default below hangs off it.
# tools/rig/paths.py reads the same variable, with the same default.
# A git worktree sits inside the main checkout (.claude/worktrees/x), so the workspace is the main checkout's parent,
# and vendor/ (gitignored) is the main checkout's when the worktree has none: both asked of tools/rig/paths.py.
VENDOR := $(shell python3 -I $(VTCON)/tools/rig/paths.py vendor)
UPTERM_ROOT ?= $(abspath $(shell python3 -I $(VTCON)/tools/rig/paths.py main-checkout)/..)
# the upterm-ports checkout (a worktree works too); every upterm-ports path uses it
# default: the kit-main worktree (the stale main checkout lacks git_SUMMARY); dist-check-ports refuses a checkout without it
UPTERM_PORTS ?= $(if $(wildcard $(UPTERM_ROOT)/upterm-ports/.claude/worktrees/kit-main),$(UPTERM_ROOT)/upterm-ports/.claude/worktrees/kit-main,$(UPTERM_ROOT)/upterm-ports)
MAIN_CHECKOUT := $(shell python3 -I $(VTCON)/tools/rig/paths.py main-checkout)

# --- Amiga (vbcc, NDK 3.2; the same setup as DCTelnet) -----------------------
# vbcc's install prefix: tools/vbcc-aos68k.cfg.in names it, so the cfg vc reads
# is generated into build/ (rewritten only when its content changes).
ifndef VBCC_PREFIX
VBCC_PREFIX := $(shell brew --prefix vbcc 2>/dev/null || echo /opt/homebrew/opt/vbcc)
endif
VBCC_CFG ?= $(CURDIR)/$(BUILD)/vbcc-aos68k.cfg
VBCC_CFG_GEN := $(shell mkdir -p $(BUILD) && sed 's|@VBCC_PREFIX@|$(VBCC_PREFIX)|g' $(VTCON)/tools/vbcc-aos68k.cfg.in > $(BUILD)/vbcc-aos68k.cfg.tmp && { cmp -s $(BUILD)/vbcc-aos68k.cfg.tmp $(BUILD)/vbcc-aos68k.cfg && rm $(BUILD)/vbcc-aos68k.cfg.tmp || mv $(BUILD)/vbcc-aos68k.cfg.tmp $(BUILD)/vbcc-aos68k.cfg; })
# written by the line above; the empty rule lets `make build/vbcc-aos68k.cfg` ask for it
$(BUILD)/vbcc-aos68k.cfg: ;
CPU      ?= 68020
# The AmigaOS 3.2 SDK headers. vendor/ is gitignored (4.1 MB of third-party
# headers), so unpack NDK3.2R4 there once, or point this at your own copy:
#   make amiga VTCON_NDK=<path>/Include_H
VTCON_NDK ?= $(VENDOR)/ndk-3.2r4-Include_H
VC       := vc +$(VBCC_CFG) -I$(VTCON_NDK) -cpu=$(CPU) -O2 -warn=-1 -dontwarn=163,166,167,168,170,306,307,81 -warnings-as-errors
# Roadshow's bsdsocket headers (NDK3.2R4 SANA+RoadshowTCP-IP/netinclude), in the tree and freely distributable;
# C:Claude, C:uptelnet and Patina compile against them (see the C:Claude block of the Makefile)
VTCON_NETINCLUDE ?= $(VTCON)/third_party/roadshow-netinclude

GITREV  := $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet 2>/dev/null || echo -dirty)
# The base's version and the build day in the $VER of every base program (C:UPTerm, vtcon-handler,
# pty-handler, vsh, hl, mdv, icat, sz, rz, uptelnet, upgetty, UPConsole, UPDemo), so "Version UPTerm" says the base it
# came with: the version from dist/packages.txt (the one tools/mkkit.py writes into UP-Term.version), the
# day as mkkit.py writes it. C:Claude ships in the up-claude add-on and says that row's version
# (UPCLAUDE_VERSION). The header is rewritten only when it changes, so the programs rebuild when a
# version or the day moves, not on every make.
# Written while the Makefile is read, not by a FORCE rule: `-q` runs no recipe, so a FORCE header counted as remade
# and every program including it was stale on every query (rig.py ensure_built rebuilt 17 programs on every start,
# S92-STALE-STAGE2). The mtime moves only when the text does, so a build and a query agree.
UPTERM_VERSION_WRITE := $(shell mkdir -p $(dir $(UPTERM_VERSION_H)) && printf '\#define UPTERM_VERSION "%s"\n\#define UPTERM_DATE "%s"\n\#define UPCLAUDE_VERSION "%s"\n' "$$(python3 $(VTCON)/tools/pkgs.py get base version)" "$$(date +%d.%m.%y)" "$$(python3 $(VTCON)/tools/pkgs.py get claude version)" > $(UPTERM_VERSION_H).new && if cmp -s $(UPTERM_VERSION_H).new $(UPTERM_VERSION_H); then rm $(UPTERM_VERSION_H).new; else mv $(UPTERM_VERSION_H).new $(UPTERM_VERSION_H); fi)

# libupkgui (patina-menu) from a PINNED upterm-ports commit; the upkgui target and the why are in the Makefile.
UPKGUI_REV := $(strip $(shell cat $(VTCON)/tools/upkgui.rev))
# The sysroot is built ONCE per pinned commit into the MAIN checkout's build/upkgui/<rev>, shared by every worktree (a
# worktree used to build its own copy: minutes lost per agent, and the documented sibling path did not exist). One drawer
# per rev, so a worktree on another rev never deletes the one a peer links against.
UPKGUI_HOME ?= $(MAIN_CHECKOUT)/$(BUILD)/upkgui/$(UPKGUI_REV)
UPKGUI_SYS ?= $(UPKGUI_HOME)/src/build/sysroot/UP-Term

KEYMAP_DEFAULT_INC := $(BUILD)/amiga/keymap_default.inc
# the shipped keymap, compiled into the handler (the table when no keymap file exists)
$(KEYMAP_DEFAULT_INC): $(VTCON)/tools/gen_keymap_default.py $(VTCON)/dist/keymap
	@mkdir -p $(dir $@)
	python3 $(VTCON)/tools/gen_keymap_default.py $(VTCON)/dist/keymap $@

# the empty _chkabort the programs that handle Ctrl-C in their own Wait link (CHKABORT_PROGRAMS in the Makefile)
NOBREAK := $(VTCON)/handler/nobreak.c

# The vtcon files the Patina programs compile in (PATINA_VTCON_SRC) and include (PATINA_VTCON_HDR), paths below
# $(VTCON). They stay in vtcon after the split (decision D2); a change here is a change to Patina's build surface.
PATINA_VTCON_SRC := claude/util.c \
	config/amconf_core.c config/amconf_dos.c config/amconf_keys.c config/cxdesc.c config/keymap_load.c config/keymap.c \
	config/upconf_rows.c config/upconf.c config/win_core.c \
	handler/be32.c handler/clip.c handler/clipfmt.c handler/frontscr.c handler/kcmode.c handler/keymap_act.c \
	handler/pubscr_core.c handler/pubscr.c handler/slash.c handler/upactive.c handler/upmru.c handler/upreq.c \
	install/hardvol.c \
	prefs/prefs_asl.c prefs/prefs_core.c prefs/prefs_dos.c prefs/settings_def.c \
	render/aa_ink.c render/desk_core.c render/emoji.c render/flat_core.c render/fontpair.c render/gthemes_core.c \
	render/gthemes_scan_dos.c render/iconpen_core.c render/otag.c render/outline.c render/pens_amiga.c render/ttf.c \
	render/unifont.c
PATINA_VTCON_HDR := claude/util.h \
	config/amconf_core.h config/amconf_dos.h config/cmdpath.h config/cxdesc.h config/keymap_load.h config/keymap.h \
	config/segstack.h config/upassign.h config/upconf_lex.h config/upconf.h config/win_core.h \
	handler/clip.h handler/frontscr.h handler/keymap_act.h handler/menu_ids.h handler/pubscr.h handler/tabprog.h \
	handler/upactive.h handler/upreq.h \
	install/hardvol.h \
	prefs/prefs_asl.h prefs/prefs_core.h prefs/prefs_dos.h prefs/settings_def.h \
	render/aa_ink_rule.h render/aa_ink.h render/amiga_render.h render/desk_core.h render/emoji.h render/flat_core.h \
	render/flat_glyph.h render/flat_shared.h render/fontpair.h render/gthemes_core.h render/gthemes_scan_dos.h \
	render/iconpen_core.h render/outline.h render/pen_match.h render/pen_rule.h render/pens_amiga.h render/ttf.h \
	tty/qmark.h

# an empty value lets the includer's next target be the default, as if this file had no rules
.DEFAULT_GOAL := $(VTCON_MK_GOAL)
