---
date: 2026-10-07
topic: A pi.dev-like, provider-agnostic agent harness native on AmigaOS, built from C:Claude
tags: [claude, pi, providers, agent, mcp, extensions, sessions, research, design]
status: draft
---

# A pi-like agent harness on AmigaOS, from C:Claude (research, 2026-10-07)

Goal (owner): pi's whole feature set (pi.dev, the "pi" coding agent) as a native AmigaOS program
(68020+, AmiSSL 5), built from C:Claude: a provider layer (Anthropic, OpenAI-compatible, Google,
Azure, Bedrock), tree-structured sessions, a mid-session model switch, project instruction files,
MCP, skills, prompt templates, extensions, and the modes TUI / print-JSON / RPC / SDK.

Sources:
- C:Claude: the C code in /Users/spot/Code/vtcon/claude/*.c (branch feature/ixemul-80, clean, HEAD
  3e6eff6), Makefile, tests/test_claude_*.c, tests/claude/*.sse, tools/claude_fixture.py, and
  thoughts/shared/research/2026-10-04_native-claude-client.md, 2026-10-05_claude-parity-audit.md.
- pi: fetched 2026-10-07 by a research subagent. pi.dev links to
  https://github.com/earendil-works/pi (NOT badlogic/pi-mono any more; npm @earendil-works/pi-*,
  MIT, Node 22.19+). Docs read raw from
  https://raw.githubusercontent.com/earendil-works/pi/main/packages/coding-agent/docs/ (cli,
  sessions, session-format, message-types, json, rpc, rpc-commands, rpc-extension-ui, extensions,
  skills, prompt-templates, packages, settings, models, providers, custom-provider, compaction,
  configuration, mcp, security, sdk, slash-commands, usage, how-pi-works, environment-variables);
  source read: packages/ai/src/types.ts, packages/ai/README.md, packages/agent/README.md,
  packages/coding-agent/src/core/{extensions/types.ts, system-prompt.ts, resource-loader.ts,
  skills.ts, tools/*.ts}. The 2025 blog post (mariozechner.at/posts/2025-11-30-pi-coding-agent/)
  was read only through a summariser; its quotes are not raw.
- Provider wire formats for OpenAI chat completions / Responses, Gemini, Azure and Bedrock below
  are from my knowledge of those APIs, NOT fetched this session (pi's own adapters,
  packages/ai/src/api/*.ts, were not read). Each is marked [unverified] where it decides a design.

Nothing here was run on an Amiga or on the FS-UAE rig.

---

## 1. C:Claude as it is (file:line)

Size: 39,622 lines of C in claude/*.c,*.h (wc, 2026-10-07); build/amiga/Claude is 661,020 bytes
(build of 2026-10-06 23:09). Portable C89 core, host-tested; only main_amiga.c, net_amiga.c,
sys_amiga.c and tls_amissl.c are Amiga-specific.

### 1.1 Layers and seams

| Layer | Where | What |
|---|---|---|
| Transport | claude/net.h:13-27 (`cl_net`: open/send/recv/close/err) | One byte pipe. net_amiga.c (bsdsocket + tls.h), net_posix.c (host, plain TCP), test stubs (tests/test_claude_repl.c:47-120 `s_open/s_send/s_recv`). |
| TLS | claude/tls.h; tls_amissl.c (199 lines, AmiSSL 5 = OpenSSL 3 via amisslmaster, SNI + chain + name check, non-blocking WANT_READ/WRITE); tls_none.c (refuses every https with bmsg_amissl_missing()) | Selected in Makefile:285-291 (`AMISSL_SDK` set -> tls_amissl.c). |
| HTTP/1.1 | claude/http.h, http.c; request head http.c:95-127 | Head is Anthropic-specific and hard-coded: `anthropic-version: 2023-06-01`, `x-api-key`, `anthropic-beta`, `Accept: text/event-stream`, keep-alive (http.c:109-126). `http_redact` blanks x-api-key only (http.c:140). Response parser is incremental (`http_resp_feed` http.c:317): chunked / length / to-close, `retry_after`, `request_id`, `event_stream`. |
| SSE | claude/sse.h, sse.c:84 `sse_feed` | Generic, provider-neutral (event:, data:, any line ending, split anywhere). |
| Stream decoder | claude/stream.h, stream.c:194 `stream_event`, :253 `stream_content` | Anthropic Messages events only (message_start, content_block_*, message_delta, ...). Accumulates `sblock`s: B_TEXT, B_THINKING (+signature byte-exact), B_REDACTED, B_TOOL (input_json_delta parsed strictly at stop), B_FALLBACK, B_SERVER, B_OTHER (kept verbatim). UI callbacks `cl_stream_ui` (text, block, thinking, stop). Usage in_tok/out_tok/cache_w/cache_r. |
| JSON | claude/json.h, json.c (578 lines) | No tree: a value is a slice (`jv`), walked in place; strict RFC 8259 validation, depth cap 64 and no recursion (68k stack safety); writer `jw` converts Latin-1 input to UTF-8. Provider-neutral. |
| Conversation | claude/conv.h, conv.c | Append-only list of messages, each the exact Anthropic content array. `conv_body` conv.c:216-268 builds the Messages body (thinking adaptive/disabled, output_config.effort, `fallbacks`, top-level and system `cache_control`, tools + tool_choice, CLAUDE_CODE_EXTRA_BODY merge). `conv_caps`, `conv_beta`, `conv_advisor_ok`. |
| Cost / context | conv.c:280-300 `conv_price` (hard-coded table: opus-5-5, opus-5, sonnet-5-5, haiku-4-5, $/MTok*100); `conv_usage` per model (CONV_MODELS 6); repl.c:800 `repl_window` (1M, or 200k for haiku / DISABLE_1M); repl.c:1148 `ctx_used = in+cache_r+cache_w+out` of the last response; repl.c:862 `repl_compact_at` (CL_COMPACT_PCT 92, repl.h:36); compaction repl.c:1461 `repl_compact`. |
| Request / retry | repl.c:292 `post` (connect, send, read, feed http -> sse -> stream), :478 `request_retry` (CL_TRIES 5, repl.h:33; 429/5xx/overloaded; feed->retry), :535 `request`; idle limit 180 s (repl.c:37). One connection (`r->connected`) reused; any host change needs a reconnect. |
| Agent loop | repl.c:1047 `turn`: up to 64 rounds (repl.c:1110); per round build body, `request`, usage, stop_reason checks (refusal, max_tokens inside a tool call, pause_turn), `stream_content` -> `conv_add`, then `run_tools` (repl.c:838, called at :1216) answering every tool_use in one user message via `pol_call` (policy.c:562). Fallback model on overload (repl.c:1128). |
| Tools | claude/tools.c:385-412 `defs[]` (counted from the table: 28 entries) | Read, Write, Edit, MultiEdit, Glob, Grep, Bash, BashOutput, KillShell, WebFetch, TodoWrite, AskUserQuestion, ExitPlanMode, EnterPlanMode, Task, Skill, SlashCommand, WebSearch, TaskCreate/Get/List/Update/Stop, Monitor, CronCreate/Delete/List, ScheduleWakeup. Schemas are JSON strings in tools.c; validation schema.c. Machine access only through `cl_sys` (sys.h: read/write/list/kind/canon/run/bg_*/append/mkdir/remove/getenv/setenv/clip/now/pause/rename). |
| Permissions | tools.h:85 `PERM_DEFAULT, PERM_ACCEPT, PERM_PLAN, PERM_BYPASS`; tools.c:758 `perm_name`; rule syntax config.h:13-24 (Tool(pattern), deny > ask > allow); policy.c (rules, PreToolUse hooks, checkpoints); ASKP_ASK / ASKP_DENY in repl.h (dontAsk, print mode) | |
| Subagents | subagent.c (AGENT_ROUNDS 60, QUERY_MAX_TOKENS 8192), `agent_query` subagent.c:148 | Synchronous nested conversation, one at a time (no threads). |
| Sessions | claude/session.h, session.c (`sess_save` :186, `sess_branch` :252, `sess_load` :461) | Claude Code's layout: ENVARC:Claude/projects/<slug>/<id>.jsonl, append-only, one line per message `{"type":"user","index":N,"message":{...}}`; a line whose index is not past the end replaces and truncates (a linear history; rewind/compact are appended lines). Index file `<slug>/sessions`. IDs 8 hex digits (FFS 30-char names); slug <= 30 chars. checkpoint.c for file rewinds. |
| Instruction files | memory.h, memory.c | CLAUDE.md, AMIGA.md, AGENTS.md, .claude/CLAUDE.md, CLAUDE.local.md; user, ancestors from the volume down, root; @imports 4 hops; .claude/rules; auto memory MEMORY.md. Into the system prompt (stable cached prefix). |
| Skills / commands / agents / styles | commands.h:1-20, commands.c | `commands/NAME.md` ($ARGUMENTS, $1..$9, @file, !`cmd`), `agents/NAME.md`, `skills/NAME/SKILL.md` (name, description, allowed-tools), output-styles. User dir ENVARC:Claude, project <root>/.claude. ext.h is the interface the tools see. |
| Hooks | hooks.h | Claude Code's events, each an AmigaDOS command run as `<cmd> < <file>` through sys->run with the event JSON; exit 2 blocks; JSON output (decision, permissionDecision, updatedInput, additionalContext). Already an out-of-process extension mechanism, one process per event. |
| Settings | config.h:1-24 | settings.json user / project / local, merged; permissions, hooks, env, model, effort. |
| UI | ui.h (cooked line mode, `cl_render` hook, control characters stripped from model text); tui.h:1-30 + tui.c (2561 lines: DECSTBM scroll region, footer diffed per row, synchronized output ?2026); input.c, keys.c, edit.c, vim.c, hist.c, tview.c (Ctrl+O), theme.c, show.c | |
| Print mode | print.h:1-20, print.c | text / json / stream-json in Claude Code's (Agent SDK) shapes; stream-json input. Fed by `cl_feed` (repl.h:40-59: message, event, denied, sub, retry, hook) -- the existing seam for JSON/RPC event output. |
| Remote mode | main_amiga.c:233-271 `remote()`, used at :371-377 | No key + ENVARC:Claude/remote ("host port") -> runs `uptelnet "host" port` in the same window (SystemTags) to Claude Code on another computer. Not part of the native client; irrelevant to the harness. |
| Key | main_amiga.c:213-230 | ENV:ANTHROPIC_API_KEY, else ENVARC:Claude/key; never logged; cleared at exit. Default URL repl.h:29 `https://api.anthropic.com/v1/messages`; URL= overrides (the fixture). |

### 1.2 Memory, stack, build

- Stack: `$STACK: 32768` cookie (main_amiga.c:53); refuses below MIN_STACK 16000 (main_amiga.c:55, :347). JSON is non-recursive by design (json.h, depth 64).
- Heap: the whole conversation is held in RAM as JSON text (conv.h); Read inlines images/PDF up to 3.75 MB (thoughts/shared/plans/2026-10-05-a4-gaps-progress.md:174). Peak heap per session was NOT measured: no measurement found in thoughts/ (grepped plans and research for budget/heap/AvailMem) or the code. [unconfirmed]
- Build: vbcc, C89, `-cpu=$(CPU)` default 68020, `-O2 -warnings-as-errors` (Makefile:236, :241); Claude rule Makefile:276-301. No ixemul: AmigaDOS + bsdsocket directly. Host: `-std=c89 -pedantic -Wall -Wextra -Werror -O1 -fsanitize=address,undefined` (Makefile:4), the whole core in the one host test binary `$(BUILD)/vttest_host` (Makefile:58-60, `make test`). `make claude-tls-check` compiles tls_amissl.c against host OpenSSL 3.
- Tests: tests/test_claude_{http,json,stream,tools,match,config,repl,cli,tui}.c (11,272 lines); 36 recordings in tests/claude/ (*.sse, print_*.json*); the stub net replays them in chunks with a Ctrl+C injection point. tools/claude_fixture.py serves POST /v1/messages from the recordings over plain HTTP (no key) for the rig / a real Amiga, routing on prompt words.

### 1.3 What is Anthropic-specific (the refactor surface)

1. http.c:109-126 request head (auth header, version, beta) and http.c:140 redaction.
2. stream.c event decoder (Anthropic event names and block types).
3. conv.c `conv_body` (body shape, thinking/effort/cache_control/fallbacks) and the stored message format (Anthropic content arrays).
4. conv.c price table, `conv_caps`, `conv_beta`; repl.c `repl_window` (model ids by prefix).
5. Server tools (web_search, advisor) in tools.c / stream.c B_SERVER.
6. session.c line format (Claude Code's) and print.c output shapes (Agent SDK's).

Everything else (net, tls, http response, sse, json, sys, tools, policy, hooks, tui, input) is provider-neutral and reusable as is.

---

## 2. pi, as needed to design against

### 2.1 Provider layer (pi-ai; packages/ai/src/types.ts, ai/README.md)

- APIs: openai-completions, openai-responses, azure-openai-responses, openai-codex-responses, anthropic-messages, bedrock-converse-stream, google-generative-ai, google-vertex, mistral-conversations, pi-messages (+ image/classifier APIs, out of scope). 40+ provider ids; most are openai-completions with another baseUrl plus `compat` flags.
- Content blocks: `text{text,textSignature?}`, `thinking{thinking,thinkingSignature?,redacted?}`, `image{data(base64),mimeType}`, `toolCall{id,name,arguments(object),thoughtSignature?(Google),namespace?}`. Signatures are opaque replay data.
- Messages (all with `timestamp` ms): system{content, sections?, toolsAdded?, toolsRemoved?}, user{content}, assistant{content, api, provider, model, usage, stopReason, errorMessage?, ...}, toolResult{toolCallId, toolName, content, details?, isError}. StopReason: stop|length|toolUse|error|aborted|deferred (pending is never persisted).
- Usage: {input, output, cacheRead, cacheWrite, totalTokens, cost{input,output,cacheRead,cacheWrite,total}}; cost = tokens x model.cost ($/MTok), optional tiers.
- Stream events: start; text_/thinking_/toolcall_ start|delta|end keyed by contentIndex (blocks may interleave); done{reason}; error{reason aborted|error, partial message}. Runtime failures go in the stream and are never thrown. Abort yields stopReason "aborted" with partial content, which may be appended and continued.
- Model descriptor: {id, name, api, provider, baseUrl, input[text,image], cost, reasoning, thinkingLevelMap, contextWindow, maxTokens, headers?, compat?, promptCache?}. Thinking levels off|minimal|low|medium|high|xhigh|max, clamped per model.
- models.json (agent dir): `{"providers":{"ollama":{"baseUrl":...,"api":"openai-completions","apiKey":"ollama","models":[{"id":"qwen2.5-coder:7b"}]}}}`; `modelOverrides`; apiKey/header values are `$NAME`, a literal, or `!command`. Credentials: --api-key > auth.json > models.json > env vars.
- Compat flags for openai-completions (what makes "OpenAI-compatible" actually work): maxTokensField (max_completion_tokens|max_tokens), supportsDeveloperRole, supportsUsageInStreaming, supportsReasoningEffort, requiresToolResultName, requiresAssistantAfterToolResult, requiresThinkingAsText, thinkingFormat (openai|openrouter|deepseek|qwen|zai|...), supportsStrictMode, cacheControlFormat "anthropic", and more.
- Cross-provider handoff: user and toolResult unchanged; assistant messages of the same provider/API kept with signatures; from another provider, thinking becomes text in `<thinking>` tags; text and tool calls kept. Tool-call id normalisation [not read: ai/src transform code].
- Retry: retry.maxRetries 3, baseDelayMs 2000, maxAgentDelayMs 60000; httpIdleTimeoutMs 300000.

### 2.2 Sessions (docs/session-format.md, sessions.md, compaction.md)

- File: `~/.pi/agent/sessions/--<cwd slug>--/<timestamp>_<id>.jsonl`, version 3. Header line `{"type":"session","version":3,"id","timestamp","cwd","parentSession"?}`. Every other line `{type, id(8 hex), parentId|null, timestamp(ISO), ...}`.
- Entry types: message{message}, model_change{provider,modelId}, thinking_level_change{thinkingLevel}, usage, compaction{summary, firstKeptEntryId, tokensBefore, details{readFiles,modifiedFiles}}, context_edit{targetId, replacement}, branch_summary{fromId, summary}, custom{customType,data} (not in context), custom_message{customType, content, display} (in context, as user), label{targetId,label}, session_info{name}. Message roles add bashExecution, custom, branchSummary, compactionSummary.
- Tree: the leaf is the current position; append with parentId = leaf. Context = walk leaf to root; take the latest compaction on the path (summary + entries from firstKeptEntryId), apply the latest context_edit per target, map entries to messages. /tree moves the leaf in the same file (editing an earlier user message makes a sibling branch; optional LLM branch_summary of the abandoned path). /fork = new file from an earlier user message (parentSession). /clone copies the branch.
- Compaction: when contextTokens > contextWindow - reserveTokens (16384); keep the newest keepRecentTokens (20000); cut only at user/assistant/bashExecution/custom_message/branch_summary, never at a toolResult; split-turn double summary; summary format "## Goal / ## Constraints & Preferences / ## Progress / ## Key Decisions / ## Next Steps / ## Critical Context" + read/modified file lists; history serialised as "[User]: ..." text, tool results cut to 2000 chars. Summariser prompt text [not read].

### 2.3 Instruction files, skills, templates

- Context files per directory, first found wins: AGENTS.override.md, AGENTS.md, AGENTS.MD, CLAUDE.md, CLAUDE.MD; the agent dir first, then ancestors root-most to cwd. Injected as `<project_instructions path="...">` in a `project_context` section. SYSTEM.md replaces the default prompt, APPEND_SYSTEM.md appends (project .pi/ over the agent dir; not combined; trust-gated). The default prompt is under ~1000 tokens, built of named sections.
- Skills (agentskills.io spec): a directory with SKILL.md; frontmatter name (<=64, [a-z0-9-]), description (required, <=1024), license, compatibility, metadata, allowed-tools, disable-model-invocation. Discovery ~/.pi/agent/skills, .pi/skills, ~/.agents/skills, .agents/skills (ancestors to the repo root), settings, --skill. Progressive disclosure: only name/description/location in an `<available_skills>` XML section; the model loads a skill with `read` (no Skill tool). `/skill:name args` forces it.
- Prompt templates: prompts/*.md -> /name; frontmatter description, argument-hint; `$1`, `$@`/`$ARGUMENTS`, `${1:-default}`, `${@:N}`, `${@:N:L}`, shell-like quoting.

### 2.4 Extensions, MCP, tools, permissions

- Extensions: TS/JS modules loaded in-process with jiti; `export default (pi) => {...}`. Events (types.ts): session_start/shutdown/before_switch/before_fork/before_compact/compact/before_tree/tree, context, context_with_system, before_agent_start, agent_start/end/settled, turn_start/end, message_start/update/end, tool_execution_start/update/end, tool_call (block/terminate, mutate input), tool_result (replace content/isError), input (transform/consume), user_bash, model_select, thinking_level_select, provider_stream_event, after_provider_response, project_trust, resources_discover, mcp_servers_change, and others. Registration: registerTool, registerCommand, registerShortcut, registerFlag, registerProvider, registerMcpServer, registerToolRenderer, registerEntryRenderer, the pi.events bus. Actions: sendMessage (steer/followUp/nextTurn), sendUserMessage, appendEntry, setLabel, exec, set/getActiveTools, setModel, setThinkingLevel. ctx.ui: select, confirm, input, editor, notify, setStatus, setWidget, setTitle, setFooter, custom(component), ... Packages via `pi install npm:|git:`.
- MCP: the 2025 blog rejected MCP; current docs (mcp.md) make it native as builtin:mcp. stdio and streamable HTTP (legacy SSE rejected); mcp.json `{"mcpServers":{name:{command,args,env,cwd | url,headers,oauth,timeout,enabled,exposure,toolExposure}}}`; tools named `mcp__<server>__<tool>`; exposure codemode (default; QuickJS scripts) | deferred (tool_search) | direct | hidden; results over 20 KB middle-truncated to a temp file; OAuth with dynamic client registration.
- Tools: default read{path,offset?,limit?}, bash{command,timeout?}, edit{path, edits[{oldText,newText}]} (matched against the original, unique, non-overlapping), write{path,content}; optional grep, find, ls. Truncation 2000 lines / 50 KB, full output to a temp file. Tool calls of one message may run in parallel.
- Permissions: none. docs/security.md: "it does not ask for approval before every tool call"; a permission gate is an extension on tool_call. Project trust (one question per project for .pi resources, trust.json) is the only gate.

### 2.5 Modes

- Interactive TUI (pi-tui, differential rendering); steering queue (Enter while running: delivered after the current tool calls) and follow-up queue (Alt+Enter: delivered when the agent would stop); Ctrl+P model cycle within a scope; Shift+Tab thinking cycle; /tree, /fork, /clone, /compact, /model, /export (HTML), /share, `!cmd` / `!!cmd`.
- Print: `-p`, final text. JSON: `--mode json`, JSONL on stdout: session header, agent_start/end/settled, turn_start/end, message_start/update/end (message_update carries the delta event), tool_execution_start/update/end, queue_update, compaction_start/end, auto_retry_start/end.
- RPC: `--mode rpc`, JSONL commands on stdin with an optional `id`, responses `{"id","type":"response","command","success","data"|"error"}` plus the same events. Commands: prompt{message,images?,streamingBehavior?}, steer, follow_up, abort, clear_queue, new_session, get_state, get_messages, set_model, cycle_model, get_available_models, set_thinking_level, cycle_thinking_level, set_steering_mode, set_follow_up_mode, compact, set_auto_compaction, set_auto_retry, abort_retry, bash, abort_bash, get_session_stats, export_html, switch_session, fork, clone, get_fork_messages, get_entries{since}, get_tree, get_last_assistant_text, set_session_name, get_commands. Extension UI over RPC: `extension_ui_request` / `extension_ui_response`.
- SDK: `createAgentSession({...})` -> session.prompt/steer/followUp/abort/waitForIdle/subscribe.

---

## 3. Feature by feature: Amiga constraints, options, pick

Common constraints: 68020-68060, often 8-64 MB fast RAM; no memory protection (a bad command can take the machine down); FFS names <= 30 characters; one CPU and no threads in C:Claude's design (subagents run synchronously, so pi's parallel tool calls become sequential); TLS handshakes cost seconds on a 68030 (A2 research expectation; no measured value found); AmigaDOS has PIPE: (queue-handler) and this project ships PTY: (handler/pty_handler.c) and IXPIPE: (Makefile:400-403); no Node and no usable JS runtime (2026-10-04_native-claude-client.md); the CPython 3.14 port exists but needs a 68030+FPU and ~16 MB and has no ssl module (~/Code/cpython-amiga/thoughts/shared/plans/2026-10-04-py1-progress.md:22, :157-159: 19 of 24 items done).

### 3.0 The architectural decision: shared core, provider-neutral messages

Options:
- A. Keep Anthropic content arrays as the canonical history (what conv.c stores today); each other provider's adapter translates history to its wire format per request and its stream back into sblocks. Cost: smallest; conv/session/print untouched; Anthropic replay stays byte-exact. Fails: pi's session format needs a second translation; opaque fields of other providers (Responses encrypted reasoning and textSignature, Gemini thoughtSignature) have no slot and must ride in invented block types; model_change and similar entries have no place in the conv list.
- B. pi's unified message model as the canonical one (text/thinking/image/toolCall/toolResult with opaque signature fields, assistant messages tagged api/provider/model), stored as pi session v3 entries; every provider including Anthropic is an adapter. Cost: largest -- a new message module, the Anthropic adapter rebuilt from conv_body/stream.c, session.c replaced, print shapes redone. Anthropic replay keeps the thinking text and signature values exactly (my reading: the API checks values, not JSON bytes -- not re-checked this session). Fails: Anthropic server tools (web_search results, advisor) need an opaque provider-block pass-through, which pi does not model.
- C. Store each assistant message in its provider's raw wire form; convert only on a provider switch. Cost: medium. Fails: every pair of providers needs a converter (N x N), and sessions are unreadable by anything else.

Pick: **B**, plus one `raw` opaque block type (api + verbatim JSON) for blocks the unified model lacks (Anthropic server_tool_use / *_tool_result, fallback), sent back only to the same api and dropped or turned into text on handoff, as pi does with thinking. Reason: pi's features (session tree, handoff, JSON/RPC events, models.json) are all defined over this model; A would re-derive each through a translation layer. The cost is front-loaded into phases 1-2.

Packaging: a new program sharing the core (working name only; the name is the owner's), not a mode of C:Claude. C:Claude stays a Claude Code clone (its session files, print shapes and permission system are Claude Code's contract); the new harness follows pi's contract. Both link the same net/tls/http/sse/json/sys/tools/tui/input modules; C:Claude moves onto the backend interface in phase 1 so the Anthropic adapter is shared, not duplicated.

### 3.1 Provider layer

Interface (phase 1): `cl_backend` = { api name; `head(model, auth, out)` request line + headers + redaction of its own secret header; `body(ctx, opts, jw)` from unified messages; `feed(state, event)` -> unified events (start / block start|delta|end by contentIndex / done / error) into a provider-neutral accumulator (today stream.c's sblock array); framing SSE or binary }. stream.c becomes the anthropic-messages adapter. `cl_net` gets one connection per host (a small pool): a model switch, a subagent on another model, WebFetch and MCP over HTTP may each hit another host, and a TLS reconnect costs seconds.

Per API:
- anthropic-messages: existing code moved behind the interface; x-api-key or Bearer (OAuth token).
- openai-completions [wire format unverified this session]: POST {baseUrl}/chat/completions, `Authorization: Bearer`, `stream:true`, `stream_options:{include_usage:true}` when compat.supportsUsageInStreaming; SSE `data:` JSON chunks ending `data: [DONE]`; deltas `choices[0].delta.content`, reasoning per thinkingFormat (`reasoning_content` / `reasoning`), `tool_calls[i]{index,id?,function{name?,arguments chunk}}`; finish_reason stop|length|tool_calls; tool results as `role:"tool"` messages with tool_call_id. Covers OpenAI, OpenRouter, Groq, DeepSeek, xAI, Together, Fireworks, and LAN servers (Ollama, llama.cpp, LM Studio). LAN servers over plain HTTP need no TLS and no AmiSSL: the cheapest real use on a 68k.
- openai-responses / azure-openai-responses [unverified]: POST /v1/responses (Azure: https://<resource>.openai.azure.com/openai/..., `api-key` header, `api-version` query); typed SSE events (`response.output_text.delta`, `response.function_call_arguments.delta`, `response.reasoning_summary_text.delta`, `response.completed` with usage); encrypted reasoning items replayed as opaque data.
- google-generative-ai [unverified]: POST /v1beta/models/{id}:streamGenerateContent?alt=sse, `x-goog-api-key`; each SSE chunk is a whole GenerateContentResponse (`candidates[0].content.parts[]`: text, `thought:true` parts, functionCall{name,args} whole in one part, thoughtSignature), `usageMetadata`. No tool-call ids: the adapter makes them. Vertex: same body with an OAuth bearer from a service-account JWT (RS256 -- possible through AmiSSL) or a `!command` key; defer.
- bedrock-converse-stream [unverified]: POST /model/{id}/converse-stream; the answer is AWS event-stream BINARY framing (prelude, headers, payload, CRC32s), not SSE, so a second framing decoder; auth SigV4 (HMAC-SHA256 through AmiSSL/OpenSSL 3), or Bedrock API keys as a bearer token if the account has them [from memory, unverified]. Highest cost of the five; last.
- models.json and keys: ENVARC:<harness>/models.json in pi's shape; keys from ENV:<PROVIDER>_API_KEY, auth.json, or `!command` run through sys->run (vsh). OAuth subscription logins (pi /login) need a browser redirect; on the Amiga only a paste-the-code or device-code flow -- deferred, owner decision.
- Cost and context: a model registry (contextWindow, maxTokens, cost, thinking map) replaces conv_price / repl_window / conv_caps. Without usage from a provider, context is estimated at chars/4 (C:Claude already does this at repl.c:1524 and :2495).

Fails to cover: pi's 40+ provider catalog and its live overlay (`pi update --models`) -- ship a small bundled catalog plus models.json; the websocket transport; cache warming; image and classifier APIs.

### 3.2 Sessions as a tree (pi v3 JSONL)

Options: (a) write pi's v3 format (8-hex ids, parentId, the entry types above); (b) extend C:Claude's indexed-line format with a parent field; (c) a binary index plus JSON payloads. Pick (a): /tree, /fork, compaction, get_entries/get_tree and the JSON mode are defined over it; files open in pi on the Mac (export/import for free); append-only suits FFS. Amiga adjustments: file name `<8-hex id>.jsonl` (pi's `<timestamp>_<uuid>.jsonl` exceeds 30 characters; the header keeps the full id and timestamp, as session.c already does with UUIDs); directory slug <= 30 chars (`sess_slug` exists); ISO timestamps from the local clock (stock OS 3.x has no time zone -- see decisions). Memory: an index of all entries (id, parentId, type, file offset, length; about 24 bytes each) in RAM, only the active branch's messages loaded, off-branch entries read on demand (/tree view, branch summary). Fails: `usage` and `custom` entries from pi extensions this harness does not have are kept and shown as unknown, never interpreted.

### 3.3 Mid-session model switch

model_change and thinking_level_change entries; the next request goes through the new model's adapter with handoff conversion (same api: signatures kept; other api: thinking -> `<thinking>` text, raw blocks dropped or turned into text, tool-call ids remapped to the target's id rules). /model picker, Ctrl+P cycling within enabledModels, `--model provider/id:level`. Low cost once 3.0 is in. Fails: providers that reject replayed tool-call ids of a foreign format until each id rule is known [pi's normaliser not read].

### 3.4 Project instruction files, SYSTEM.md, APPEND_SYSTEM.md

Reuse memory.c's walk (volume root down to the start directory, assigns resolved through sys->canon); change the candidates to pi's list (AGENTS.override.md, AGENTS.md, CLAUDE.md, first found per directory), the agent dir first. Add SYSTEM.md / APPEND_SYSTEM.md and pi's section-built default prompt, with C:Claude's Amiga paragraph (repl.c:39-55) as an always-on section. Trust gating: trust.c exists. Cost low. Fails: nothing material; the Amiga paragraph is a deliberate addition to pi's prompt.

### 3.5 Skills and prompt templates

commands.c already parses frontmatter and expands $ARGUMENTS / $1..$9 / @file / !`cmd`. Skills: switch to pi's progressive disclosure (`<available_skills>` section; the model loads with read; `/skill:name`), the agentskills.io field rules, and pi's discovery paths mapped to ENVARC:<harness>/skills, <root>/.pi/skills, <root>/.agents/skills. Templates: prompts/*.md with pi's richer substitution (`${1:-default}`, `${@:N:L}`, `$@`, shell-like quoting). Cost low. Fails: npm/git packages (no npm on the Amiga; `git:` needs a git client) -- packages become copied directories.

### 3.6 Extensions (the hard one)

pi's extensions are in-process TypeScript with ~40 events, synchronous return values (block a tool call, rewrite input, replace a result, add entries) and UI calls. Options:

- **Lua 5.4 embedded.** In process; small (roughly 250-300 KB of 68k code -- an estimate, not built); MIT (Lua's own notice must ship with it); clean C that builds as C89 with LUA_USE_C89. The API maps one to one: `pi.on("tool_call", function(e) return {block=true, reason="..."} end)`, `pi.registerTool{...}`, `pi.registerCommand`, `ctx.ui.confirm` (synchronous, which suits a single-threaded harness). JSON crosses as Lua tables through a bridge over json.c. Cost: medium (bridge for events, tools, UI; sandboxing is moot -- pi gives full OS access too). Risks: doubles mean soft-float on a 68020 without FPU (`LUA_32BITS` makes integers and floats 32-bit and cheaper); setjmp/longjmp under vbcc; neither verified -- a one-day probe decides. Fails: pi's TypeScript extensions do not run; they must be rewritten in Lua.
- **Python 3 from the kit, out of process** (JSONL over a pipe in pi's RPC/event shapes). Cost: medium (the protocol is the RPC mode's, built anyway). Fails: needs 68030+FPU and ~16 MB, slow start, no ssl; every synchronous hook (tool_call) becomes a round trip; 68020 owners get no extensions.
- **Native commands per event** (hooks.c today: run a command with the event JSON, read a JSON decision). Cost: lowest, exists. Fails: no registerTool/registerCommand state, a process start per event (slow on a 68k), no UI calls; maybe a third of pi's API.
- **QuickJS** (ES2023 in C). Would run JS, even pi's codemode. Fails: C99, larger (roughly 600 KB-1 MB, estimate), heavy on a 68020; pi extensions import Node and pi packages, so they would not run unchanged anyway.
- **ARexx port.** Amiga-native; any language that speaks ARexx can drive the harness. Fails: asynchronous messages are awkward for synchronous decisions; better as an automation/SDK surface (3.9) than as the extension system.

Pick: **Lua 5.4 embedded**, mirroring pi's ExtensionAPI names and event payloads, with hooks.c's command mechanism kept as the zero-cost path for simple per-event scripts; both feed one event dispatcher. Gate: the Lua-on-vbcc probe; if it fails, build Lua with the bebbo gcc toolchain (~/Code/amiga-gcc) and link it, or fall back to the out-of-process protocol. Owner decision, because it adds a language to the kit.

### 3.7 MCP

Transports on AmigaOS:
- stdio: start the server with SystemTags(SYS_Asynch, SYS_Input = a PIPE: for its input, SYS_Output = a PIPE: for its output) or on a PTY: pair, newline-delimited JSON-RPC 2.0. Needs a new `cl_sys` call (spawn with two pipes; read with timeout and Ctrl+C); the bg_* job calls write to a file and give no stdin. Which handler behaves right (PIPE: buffering and EOF, IXPIPE:, PTY:) is [unverified -- needs a probe on the Amiga]. Most MCP servers are Node or Python programs, so on the Amiga stdio MCP means native servers, or Python ones on a big machine.
- streamable HTTP: POST JSON-RPC to the server URL; the answer is JSON or an SSE stream; `Mcp-Session-Id` header. Reuses http.c/sse.c and the connection pool directly. The practical path: MCP servers on the NAS/Mac over plain http on the LAN (no TLS cost).

Exposure: `direct` (declared as tools `mcp__server__tool`) first; `deferred` with a tool_search tool second (it saves context, which matters on a slow machine); `codemode` not at all (needs a JS engine). Config: pi's mcp.json shape in ENVARC:<harness>/mcp.json and <root>/.pi/mcp.json. Results over 20 KB truncated, the full text in T:. MCP OAuth: deferred. Cost: medium. Fails: codemode, OAuth servers, legacy SSE transport (pi rejects it too).

### 3.8 Built-in tools and permissions

Tools: pi's read/write/edit/bash/grep/find/ls with pi's parameters map onto tools.c's Read/Write/MultiEdit/Bash/Grep/Glob implementations (all through sys.h); expose them under pi's names and schemas (edit = pi's `edits[]` matched against the original). Truncation 2000 lines / 50 KB, the rest in T:. C:Claude's extra tools (Task, WebFetch, Todo, Cron, ...) are not pi's; offer them as built-in extensions, off by default.

Permissions: pi has none; C:Claude has Claude Code's modes and rules. An Amiga has no memory protection and no container to run in, and a wrong `Delete #? ALL` is unrecoverable. Recommended: C:Claude's permission policy as a built-in extension on tool_call (pi's own recommended pattern), default ask for bash/write/edit, a setting to switch to pi's no-questions behaviour. Owner decision.

### 3.9 Modes

- TUI: reuse tui.c/input.c/keys.c/edit.c/tview.c; add the steering and follow-up queues (C:Claude already has a type-ahead queue, tui.h:24), Ctrl+P cycling, Shift+Tab thinking, a /tree view.
- Print and JSON: one event emitter (`cl_feed` generalised) writing pi's JSONL event shapes; print = final text.
- RPC: the same emitter plus a command reader on Input(). On the Amiga the client is another program over PIPE:, a PTY:, or a TCP socket (a front end on the Mac, a MUI GUI later). Commands as in 2.5; extension UI as `extension_ui_request` / `extension_ui_response`.
- SDK: a C API header over the same core (static link: session new / prompt / steer / abort / subscribe), with RPC for every other language. An ARexx port as an optional Amiga automation surface. An Amiga shared .library is not worth its cost now.

### 3.10 Where JSON handling lives

Keep json.c (no-tree slices, strict, non-recursive) and jw. Unified messages are stored as JSON text per entry (as conv.c does today) plus small C structs for the fields the loop needs (role, stop reason, tool calls). No DOM library: adapters read with json_get/json_iter and write with jw. Lua gets tables built from slices at the bridge.

---

## 4. Phases, success criteria, tests

Every phase: host tests in `make test` (vttest_host, ASan/UBSan, C89 pedantic) with recorded streams replayed through the stub cl_net; one reachability test per feature through the top-level entry (`repl_turn`, or the new harness's prompt entry) with a call-count sentinel; tools/claude_fixture.py extended for the rig and a real Amiga. Rig and hardware runs are not part of this research (the FS-UAE rig belongs to another agent).

1. **Backend interface + OpenAI-compatible adapter.** cl_backend; the provider-neutral accumulator; Anthropic moved behind it (C:Claude unchanged in behaviour); openai-completions with compat flags maxTokensField, supportsUsageInStreaming, supportsDeveloperRole, requiresToolResultName; per-host connection pool; Bearer header redacted in the debug log.
   Success: (a) every existing tests/test_claude_* passes unchanged; (b) a recorded OpenAI stream with text deltas, two interleaved tool_calls with chunked arguments and finish_reason tool_calls, then a final answer, gives the same tool executions and history shape as the matching Anthropic recording; (c) usage from the include_usage chunk lands in the counters and the cost; (d) `[DONE]`, a cut stream, a 429 with Retry-After, and Ctrl+C mid-stream each give the documented result; (e) the request body per compat combination equals a golden file; (f) no Authorization value in the debug log. Tests: tests/test_claude_backend.c, tests/claude/openai_*.sse; reachability: a full turn through the top-level entry against the OpenAI stub, adapter call counter > 0. claude_fixture.py answers POST /v1/chat/completions so a real Amiga can run against it over plain HTTP.
2. **Model registry, models.json, mid-session switch, handoff.** Success: /model to another api mid-session sends converted history (golden bodies: thinking -> `<thinking>` text; signatures kept for the same api); cost per model from the registry; context % from contextWindow; `!command` keys resolved once per process.
3. **Session tree v3.** Writer/reader, context build with compaction and context_edit, /tree, /fork, /clone, branch summary, resume, compaction cut rules. Success: pi's documented example sessions (copied from pi's docs as fixtures) build the context pi documents; names <= 30 chars; a file written here opens in pi on the Mac (manual, owner).
4. **Responses, Azure, Google adapters.** Recorded streams per API; golden bodies; a table-driven handoff test across every api pair.
5. **Bedrock.** Event-stream decoder (CRC32 checked) and SigV4 signer against AWS's published SigV4 test vectors; a recorded converse-stream.
6. **Resources.** AGENTS.md walk, SYSTEM.md / APPEND_SYSTEM.md, skills section and /skill:name, prompt templates with pi's substitution table, trust. Tests on a temporary tree (sys_posix).
7. **Modes.** JSON events and the RPC command set compared line by line with golden JSONL taken from pi's json.md / rpc-commands.md; the C SDK header with an example program built in the tests.
8. **MCP.** HTTP transport against a Python fake MCP server on the host (initialize, tools/list, tools/call, SSE answers, session id); stdio through a new sys spawn-with-pipes call and its posix implementation; the Amiga pipe probe (owner or rig owner).
9. **Extensions (Lua).** Probe first (Lua builds with vbcc, runs a script, soft-float speed acceptable); then the bridge: tool_call block, input transform, registerTool, registerCommand, ui.confirm, appendEntry; host tests drive Lua scripts.
10. **TUI parity.** Steering and follow-up queues, Ctrl+P, Shift+Tab, /tree view, /export HTML.

---

## 5. Open decisions for the owner

1. A separate program (recommended; its name?) or a provider switch inside C:Claude.
2. Canonical format = pi's unified messages and pi v3 session files (recommended), accepting a rewrite of conv/session/stream for the new harness.
3. Permissions: C:Claude's ask-by-default policy as a built-in extension (recommended) or pi's no-questions default.
4. Extension language: Lua 5.4 in the kit (recommended, after a vbcc probe), Python-only out of process, or hook commands only.
5. Provider order after OpenAI-compatible (Google, Azure, Bedrock), and whether OAuth subscription logins (Claude Pro/Max, ChatGPT, Copilot) are wanted on the Amiga at all.
6. Time stamps without a time zone: local time with no offset, or a TZ variable.

## 6. Not confirmed, and where I looked

- Provider wire details for OpenAI, Azure, Google, Bedrock: from knowledge, not fetched; pi's adapters (packages/ai/src/api/*.ts) not read.
- pi's compaction and branch-summary prompt text (src/core/compaction/*.ts), tool-call id normalisation on handoff, the auth.json OAuth shape, theme and keybinding formats: not read.
- C:Claude's peak heap per session: no measurement found in thoughts/ or the code.
- Lua on vbcc, PIPE:/IXPIPE:/PTY: behaviour for MCP stdio, the TLS handshake time on a 68030: not tested; each needs a probe.
- Size figures for Lua and QuickJS are estimates.
