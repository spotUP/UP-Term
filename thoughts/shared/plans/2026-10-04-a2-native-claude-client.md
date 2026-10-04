---
date: 2026-10-04
topic: A2 -- a native Claude client for AmigaOS 3.x (68020+), chat and Claude Code-style tools, in C
tags: [claude, a2, amissl, bsdsocket, sse, json, tool-use, upterm]
status: draft
---

# A2: a native Claude client for the Amiga

Owner, 2026-10-04: "claude cli on the amiga is on the top of my wish list". Research:
research/2026-10-04_native-claude-client.md (read it first): Claude Code itself is Node.js and
cannot run on a 68k; NodeAmiga is a fallback runtime only; the client is native C.

Done = on the owner's 68030/68060 (and the stock-rig 68020), `Claude` in an UP-Term window
holds a streamed conversation with Claude over HTTPS, renders the answer as Markdown while it
arrives, and lets Claude read, search and edit files and run commands through vsh -- each tool
call shown and confirmed by the user -- with the API key never shown or logged. One
reachability test drives the shipped binary end to end against a recorded API stream.

## Decisions (made; change only by the owner)

- Language C89 (vbcc, the project's toolchain), no ixemul: bsdsocket.library + AmiSSL directly,
  like the A1 telnet client. Program name `Claude` (C:Claude in the kit), one binary, 68020+.
- API: `POST https://api.anthropic.com/v1/messages`, headers `x-api-key`, `anthropic-version:
  2023-06-01`, `content-type: application/json`, body with `"stream": true` always (long answers;
  no request timeout to fight).
- Model default `claude-opus-5-5` (the current default model), selectable with `MODEL=` and the
  profile; `max_tokens` 64000 (streaming default). Thinking stays on (Claude Opus 5.5 cannot turn
  it off; effort is the control): `output_config: {effort: "medium"}` default, `EFFORT=` to change.
  Refusal fallbacks on by default (`anthropic-beta: server-side-fallback-2026-07-01`,
  `"fallbacks": "default"`), and `stop_reason: "refusal"` is shown, never treated as text.
- The key: ENV:ANTHROPIC_API_KEY, else ENVARC:Claude/key (the owner's choice of location;
  documented). Never echoed, never in a log, never in a crash dump; `--debug` logs redact it.
- History is APPEND-ONLY and kept as the exact JSON of every block the API returned (thinking
  blocks with their signatures included): Claude Opus 5.5 checks that replayed thinking blocks
  are unedited ("preserved thinking"); the client never rewrites earlier turns.
- Tools are client tools with JSON schemas (`strict: true`), `tool_choice` auto (forced tool
  choice is a 400 on Claude Opus 5.5), `eager_input_streaming: true`, every input validated
  before it runs, parallel calls answered with all `tool_result` blocks in ONE user message.
- Permissions: read-only tools (read, list, grep) may be allowed for the session with one key;
  write/edit/command ask every time unless the user chose "allow for this session" for that tool.
  Paths outside the start directory ask always. No tool runs while the user has not answered.
- Output: the answer's text goes through the highlight/markdown renderer (feature/highlight-
  markdown, merged first) as a library; input through UP-Term's line editor (the window's
  cooked mode) -- Claude is an ordinary console program, no own window.

## Architecture (modules, each host-testable except the transport)

```
claude/                      the program (new directory)
  main.c        argument parsing (ReadArgs), the REPL, slash commands (/model /effort /clear
                /save /cost /exit), Ctrl+C = cancel the stream
  net_amiga.c   bsdsocket + AmiSSL: connect, TLS (SNI, certificate verification ON), send,
                receive; one interface (net.h) so the host build swaps in a socket/file stub
  http.c        HTTP/1.1 request writer, response status + headers, chunked transfer decoding
  sse.c         server-sent events: "event:" / "data:" lines -> (type, json) callbacks, any
                split of the byte stream
  json.c        a small allocation-light JSON reader (tokens over a buffer) and writer (escaping,
                UTF-8), enough for the Messages API and tool inputs; no recursion limit surprises
  conv.c        the conversation: blocks as raw JSON, append-only, request body assembly,
                usage accounting (input/output/cache tokens -> /cost)
  stream.c      the event state machine: message_start, content_block_start/delta/stop,
                message_delta (stop_reason, usage), message_stop, ping, error; deltas: text_delta
                (to the renderer at once), input_json_delta (accumulated per block),
                thinking_delta / signature_delta (kept verbatim, shown as "thinking..." only)
  tools.c       tool definitions (JSON schemas) + validation + execution: read_file, list_dir,
                grep, write_file, edit_file (exact-string replace, as Claude Code's), run_command
                (through vsh: SystemTags with output captured, a timeout, Ctrl+C), each returning
                a tool_result (is_error on failure); AmigaOS paths (volumes, assigns, "/" parent)
  ui.c          prompts, the permission question, spinner while waiting, the Markdown stream
tests/test_claude_*.c        host suites, in the CI glob
tests/claude/*.sse           recorded streams (text, tool_use with input_json_delta, refusal,
                             overloaded error, max_tokens) for the replay tests
```

The request body (shape the client writes):

```json
{"model":"claude-opus-5-5","max_tokens":64000,"stream":true,
 "output_config":{"effort":"medium"},
 "fallbacks":"default",
 "system":[{"type":"text","text":"<Amiga + UP-Term + tools system prompt>",
            "cache_control":{"type":"ephemeral"}}],
 "tools":[{"name":"read_file","description":"...","strict":true,"eager_input_streaming":true,
           "input_schema":{"type":"object","properties":{"path":{"type":"string"}},
                           "required":["path"],"additionalProperties":false}}, ...],
 "messages":[ ...append-only history... ]}
```

The stream it parses (from the API reference):

```
event: message_start        data: {"type":"message_start","message":{"id":"msg_...",...}}
event: content_block_start  data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}
event: content_block_delta  data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}
event: content_block_stop   data: {"type":"content_block_stop","index":0}
event: message_delta        data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":12}}
event: message_stop         data: {"type":"message_stop"}
```

Tool round trip: the assistant turn (all its blocks, thinking included, verbatim) is appended;
when `stop_reason` is `tool_use`, each `tool_use` block's input is parsed (never string-matched),
validated, confirmed, run; one user message with every `tool_result` (`tool_use_id`, `content`,
`is_error`) is appended; the loop continues until `end_turn`. `max_tokens` / `refusal` stop
reasons are checked BEFORE any tool runs. Errors: 429 and 529 (`overloaded_error`) and 5xx and
dropped connections retry with backoff honouring `retry-after`; 400/401/403/404 do not retry and
are shown with the API's message; an `error` event mid-stream ends the turn and is shown.

## Phases (each ends with its checks green; manual checks collected for the owner)

### Phase 1 -- the transport (A2.1)
1. net.h + net_amiga.c: bsdsocket.library (Roadshow/Miami/AmiTCP), AmiSSL (amisslmaster.library,
   OpenSSL 3 API through AmiSSL 5): TLS with SNI `api.anthropic.com`, certificate verification
   against AmiSSL's CA store (a failure is an error, never a silent downgrade).
2. http.c: request writer (Host, Content-Length, the three API headers, keep-alive), status line,
   headers (`retry-after`, `request-id` kept for error reports), chunked decoding.
3. A host build of net.h over plain POSIX sockets + a fixture server for tests.
Automated: host tests for http.c (status/headers/chunked split at every byte) and the request
writer (golden bytes; key redaction in the debug dump). make amiga clean.
Manual (owner, rig with AmiSSL installed, then the A1200): `Claude --ping` prints the HTTP status
of a 1-token request and the TLS handshake time.

### Phase 2 -- JSON, SSE and the stream (A2.1 cont.)
json.c reader/writer; sse.c; stream.c state machine; conv.c history and body assembly.
Automated: JSON round trips incl. escapes and UTF-8 (surrogate pairs), malformed input refused;
SSE split at every byte position; the recorded streams in tests/claude/ replayed: text appears
in order, a tool_use input assembled from input_json_delta equals the expected object, thinking
blocks and signatures are stored byte-exact, refusal / max_tokens / overloaded recognised.

### Phase 3 -- chat in UP-Term (A2.2)
main.c REPL + ui.c: the line editor's prompt, streamed Markdown (the highlight/markdown
renderer), a spinner during the wait, Ctrl+C cancels the stream (connection closed, the partial
assistant turn NOT appended -- the history stays valid), /model /effort /clear /save /cost /exit.
Automated: a reachability test running the program's REPL core against a recorded stream (stub
transport) with a call-count sentinel on the renderer; usage/cost accounting test.
Manual: a short conversation on the rig; Markdown, code blocks and tables look right in a 77x20
and a full-screen window; Ctrl+C mid-answer leaves a usable session.

### Phase 4 -- tools (A2.3)
tools.c: read_file (size cap, binary detection), list_dir, grep (substring and simple pattern,
over a directory tree with limits), write_file, edit_file (exact unique match, as Claude Code's
str_replace), run_command (vsh, captured output with a cap, a timeout, exit code); the permission
model above; path resolution for AmigaOS (volume:, assigns, `/` as parent) with the start
directory as the default root.
Automated: each tool against a temp tree on the host (portable layer), schema validation
rejecting bad inputs with an is_error result, parallel tool_use answered in one message,
permission matrix (allow-once, allow-for-session, deny; outside-root always asks).
Manual: on the rig, "list the files in SYS:S and show me the startup-sequence" (read tools
allowed for the session), "add a comment to RAM:test.txt" (asks before writing), "run Version"
(asks before running).

### Phase 5 -- the kit and the owner's machines (A2.4)
Installer entry (optional part, needs AmiSSL), AmigaGuide/README section (the key, costs are
the owner's API usage, the permission model), ENVARC:Claude/ settings. Rig check end to end, then
the owner's A1200/68030/68060: TLS handshake time, time to first token, streaming smoothness.

## Success criteria

- Automated: all claude host suites green and in the CI glob; the reachability test passes;
  make amiga clean (68020 build; a 68000 build only if it costs nothing).
- Manual (owner): Phases 1, 3, 4, 5 checks above; the key never appears on screen or in logs.

## Not in A2

Claude Code's full feature set (MCP, sub-agents, hooks, IDE integration); image input; prompt
editing of earlier turns (the history is append-only by design); Python (PY1) or NodeAmiga.
