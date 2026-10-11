# Source lists another program builds from in place: vtcon's Makefile includes
# this file, and so does amiga-pi's (~/Code/amiga-pi/Makefile, with
# CL_DIR = $(VTCON_DIR)/), so C:Claude's core, the view and llm/ are listed once.
# Every path is below $(CL_DIR) (empty here).
CL_DIR ?=
cl_src = $(addprefix $(CL_DIR),$(1))

# hl and mdv (view/): the lexer, themes, the Markdown renderer
VIEW_LEX  := $(call cl_src,view/hl_lex.c view/hl_langs.c view/hl_style.c view/vw_text.c)
VIEW_MD   := $(VIEW_LEX) $(call cl_src,view/md.c)
VIEW_CORE := $(VIEW_LEX) $(call cl_src,view/hl_view.c view/md.c)
VIEW_HDR  := $(call cl_src,view/hl_lex.h view/hl_style.h view/hl_view.h view/vw_text.h view/md.h)

# the Claude client's portable core (ledger A2); net_posix is the host transport
# A4 WP1: the input box's vim mode, history, themes, prompt prefixes, transcript viewer
# amiga-pi's provider-neutral LLM layer (llm/, shared with C:Claude; plan 2026-10-07-amiga-pi B)
# what C:Claude links of it: the events, the Anthropic backend, the request
LLM_CLAUDE := $(call cl_src,llm/event.c llm/be_anthropic.c llm/request.c)
LLM_CORE := $(LLM_CLAUDE) $(call cl_src,llm/llm.c llm/accum.c llm/be_anthropic_pi.c llm/retry.c llm/be_openai_chat.c llm/transform.c)
LLM_HDR := $(wildcard $(CL_DIR)llm/*.h)
CLAUDE_INPUT := $(call cl_src,claude/vim.c claude/hist.c claude/theme.c claude/input.c claude/tview.c)
CLAUDE_CORE := $(call cl_src,claude/util.c claude/http.c claude/net_posix.c claude/json.c claude/sse.c claude/stream.c claude/conv.c \
               claude/path.c claude/tools.c claude/sys_posix.c claude/ui.c claude/repl.c \
               claude/regex.c claude/glob.c claude/schema.c claude/search.c claude/shells.c claude/html.c \
               claude/webfetch.c claude/subagent.c claude/tasks.c claude/sched.c claude/watch.c claude/trust.c \
               claude/keys.c claude/edit.c claude/tui.c claude/show.c) $(CLAUDE_INPUT) \
               $(call cl_src,claude/config.c claude/memory.c claude/commands.c claude/hooks.c claude/session.c claude/checkpoint.c \
               claude/policy.c claude/slash.c claude/setup.c claude/cli.c claude/print.c claude/datadir.c) $(LLM_CORE)
CLAUDE_HDR := $(wildcard $(CL_DIR)claude/*.h) $(LLM_HDR)

# C:Claude on the Amiga: the same core over sys_amiga, the console (io_amiga) and
# its main; net_amiga and the TLS layer are compiled on their own (their headers)
CLAUDE_PORTABLE := $(call cl_src,claude/util.c claude/http.c claude/json.c claude/sse.c claude/stream.c claude/conv.c \
                   claude/path.c claude/tools.c claude/ui.c claude/repl.c claude/sys_amiga.c claude/datadir_amiga.c claude/main_amiga.c claude/io_amiga.c \
                   claude/regex.c claude/glob.c claude/schema.c claude/search.c claude/shells.c claude/html.c \
                   claude/webfetch.c claude/subagent.c claude/tasks.c claude/sched.c claude/watch.c claude/trust.c \
                   claude/keys.c claude/edit.c claude/tui.c claude/show.c) $(CLAUDE_INPUT) $(VIEW_MD) $(call cl_src,tty/ldisc.c tty/bmsg.c \
                   handler/clip.c handler/clipfmt.c \
                   claude/config.c claude/memory.c claude/commands.c claude/hooks.c claude/session.c claude/checkpoint.c \
                   claude/policy.c claude/slash.c claude/setup.c claude/cli.c claude/print.c claude/datadir.c) $(LLM_CLAUDE)
