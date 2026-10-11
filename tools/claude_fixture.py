#!/usr/bin/env python3
"""claude_fixture -- the Messages API's streamed answers from recordings, over
plain HTTP on the LAN, for C:Claude on the rig or a real Amiga (ledger A2).

It never talks to Anthropic and needs no key. It answers POST /v1/messages
with one of tests/claude/*.sse, chunked, a few bytes at a time like the real
stream (A2/A3, and the A4 WP2 tools):

  - a Claude Haiku request (WebFetch's small model)   -> fetch_answer.sse
  - a Task subagent's request ("file search specialist" in its system
    prompt): its first -> agent_tool.sse (Grep in S), then agent_final.sse
  - the request's last message holds a tool_result: by the call it answers
    EnterPlanMode -> tool_exitplan.sse, a background Bash -> tool_bgout.sse
    (BashOutput bash_1), ScheduleWakeup -> loop_final.sse, anything else ->
    tool_final.sse
  - the last prompt mentions "loop test" (checked first: /loop's skill text
    names other words of this list)               -> tool_loop.sse
    (ScheduleWakeup, 60 s, prompt "/loop loop test", noop: type
    "/loop loop test"; a wakeup fires each minute as "Claude resuming /loop
    wakeup", quiet ones in a row fold into one line, Esc cancels the next)
  - the last prompt mentions "search the web"         -> tool_websearch.sse
    (Claude Code's WebSearch client tool, allowed_domains example.org); the
    search's own request ("performing a web search" in its system prompt)
    -> websearch.sse (the web_search server tool's blocks)
  - mentions "monitor"                                -> tool_monitor.sse (Monitor: three
    ticks a second apart, each an event between turns)
  - mentions "schedule"                               -> tool_cron.sse (CronCreate, once, the
    next minute: the prompt fires between turns)
  - mentions "time limit"                             -> tool_timelimit.sse (Bash List SYS: ALL
    with a 1 s timeout: moved to the background at its time limit, its end
    reported later)
  - mentions "fetch"                                  -> tool_fetch.sse
    (WebFetch http://127.0.0.1:8080/page -- this server's own GET /page; on a
    real Amiga run with --bind <LAN address> and --page-host <that address>)
  - mentions "agent"                                  -> tool_task.sse (Task, Explore)
  - mentions "question"                               -> tool_ask.sse (AskUserQuestion)
  - mentions "plan"                                   -> tool_enterplan.sse
    (EnterPlanMode, then ExitPlanMode with a plan)
  - mentions "background"                             -> tool_bg.sse (Bash Wait 2 in
    the background, then BashOutput)
  - mentions "grep"                                   -> tool_grep.sse (Grep for the Set... commands in S)
  - mentions "slow"                                   -> tool_slow.sse (Bash Wait 30 in the
    foreground: Ctrl+B moves it to the background, Esc stops it)
  - mentions "edit"                                   -> tool_edit.sse
    (TodoWrite, Read and Edit claude-test.txt "hello" -> "hello from the
    Amiga": Echo hello >RAM:claude-test.txt, start Claude with ROOT=RAM:)
  - mentions "startup" or "tool"                      -> tool_use.sse
    (Read S/Startup-Sequence and Glob * in S: start Claude with ROOT=SYS:)
  - mentions "refuse"                                 -> refusal.sse
  - mentions "busy"                                   -> overloaded.sse
  - anything else                                     -> text.sse
  (--stream NAME answers every request with NAME.sse instead.)

It also answers amiga-pi's POST .../chat/completions (openai-completions)
from tests/pi/openai_*.sse: after the results of openai_edit's calls
openai_edit_final, after other tool results openai_final, a prompt naming
"edit" openai_edit (edit RAM:pi-test.txt "hello" -> "hello from the Amiga"
and bash List RAM:; Echo hello >RAM:pi-test.txt first), "startup" or
"tool" openai_tools, else openai_text; the log line says whether an
Authorization header came (over plain http it must not), and a line per
tool result gives its text (the first 300 characters, newlines as \\n).

On the Amiga:  Claude URL=http://<this machine>:8080/v1/messages ROOT=SYS:
Each request is summarised here: model, messages, tool results, and whether
an x-api-key header came (it must not: the key only goes over https).

  python3 tools/claude_fixture.py [--bind 127.0.0.1] [--port 8080] [--stream text] [--delay 0.05]
"""
import argparse
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
STREAMS = os.path.join(HERE, "..", "tests", "claude")


PAGE = (b"<html><head><title>UP-Term test page</title></head><body><h1>Hello from the fixture</h1>"
        b"<p>This is the <a href=\"https://example.org/\">UP-Term</a> test page.</p>"
        b"<ul><li>one</li><li>two</li></ul></body></html>")

# the call a tool_result answers -> the next recording
AFTER = {"toolu_01EnterPlan": "tool_exitplan", "toolu_01Background": "tool_bgout", "toolu_01Loop": "loop_final"}


def system_text(body):
    sysp = body.get("system") or ""
    if isinstance(sysp, list):
        return " ".join(b.get("text", "") for b in sysp if isinstance(b, dict))
    return str(sysp)


def pick(body, forced):
    if forced:
        return forced
    msgs = body.get("messages") or []
    if not msgs:
        return "text"
    if str(body.get("model", "")).startswith("claude-haiku"):
        return "fetch_answer"
    last = msgs[-1]
    content = last.get("content")
    blocks = content if isinstance(content, list) else [{"type": "text", "text": str(content)}]
    results = [b for b in blocks if b.get("type") == "tool_result"]
    if "performing a web search" in system_text(body):
        return "websearch"
    if "file search specialist" in system_text(body):
        return "agent_final" if results else "agent_tool"
    if results:
        return AFTER.get(results[-1].get("tool_use_id"), "tool_final")
    text = " ".join(b.get("text", "") for b in blocks if b.get("type") == "text").lower()
    if "loop test" in text:
        return "tool_loop"
    for word, name in (("search the web", "tool_websearch"), ("monitor", "tool_monitor"), ("schedule", "tool_cron"),
                       ("time limit", "tool_timelimit"), ("fetch", "tool_fetch"), ("agent", "tool_task"),
                       ("question", "tool_ask"), ("plan", "tool_enterplan"), ("background", "tool_bg"),
                       ("grep", "tool_grep"), ("slow", "tool_slow")):
        if word in text:
            return name
    if "edit" in text:
        return "tool_edit"
    if "startup" in text or "tool" in text:
        return "tool_use"
    if "refuse" in text:
        return "refusal"
    if "busy" in text:
        return "overloaded"
    return "text"


PI = os.path.join(HERE, "..", "tests", "pi")


def pick_openai(body):
    """amiga-pi's openai-completions requests (POST .../chat/completions):
    after openai_edit's results (call_Pi...) -> openai_edit_final.sse, after other
    tool results -> openai_final.sse, a prompt naming "edit" -> openai_edit.sse
    (edit and bash), "startup" or
    "tool" -> openai_tools.sse (read and ls, the arguments in pieces), else
    openai_text.sse."""
    msgs = body.get("messages") or []
    last = msgs[-1] if msgs else {}
    if last.get("role") == "tool":
        return "openai_edit_final" if str(last.get("tool_call_id", "")).startswith("call_Pi") else "openai_final"
    content = last.get("content")
    text = content if isinstance(content, str) else " ".join(
        b.get("text", "") for b in (content or []) if isinstance(b, dict))
    text = text.lower()
    if "edit" in text:
        return "openai_edit"
    if "startup" in text or "tool" in text:
        return "openai_tools"
    return "openai_text"


def tool_tail(msgs):
    """the tool messages at the end of an openai conversation (the last round's results)"""
    tail = []
    for m in reversed(msgs):
        if m.get("role") != "tool":
            break
        tail.insert(0, m)
    return tail


def result_text(m):
    """a tool message's text, the first 300 characters, newlines as \\n"""
    c = m.get("content")
    text = c if isinstance(c, str) else " ".join(b.get("text", "") for b in (c or []) if isinstance(b, dict))
    return text[:300].replace("\n", "\\n")


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    forced = None
    delay = 0.05
    page_host = None
    dump = None
    ndump = 0

    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        # WebFetch's page (tool_fetch.sse asks for /page)
        if self.path != "/page":
            self.send_error(404)
            return
        print("[INFO] GET /page (WebFetch)", flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)
        try:
            body = json.loads(raw)
        except ValueError:
            self.send_error(400, "the body is not JSON")
            return
        openai = self.path.endswith("/chat/completions")
        name = pick_openai(body) if openai else pick(body, self.forced)
        if self.dump:
            # every request body as it came, numbered, for the rig checks to read
            Handler.ndump += 1
            with open(os.path.join(self.dump, "%03d.json" % Handler.ndump), "wb") as f:
                f.write(raw)
        msgs = body.get("messages") or []
        results = sum(1 for m in msgs if isinstance(m.get("content"), list)
                      for b in m["content"] if b.get("type") == "tool_result")
        if openai:
            results = sum(1 for m in msgs if m.get("role") == "tool")
        key = self.headers.get("Authorization") if openai else self.headers.get("x-api-key")
        print("[INFO] %s model=%s messages=%d tool_results=%d key_header=%s -> %s.sse" % (
            self.path, body.get("model"), len(msgs), results,
            "PRESENT (wrong!)" if key else "none", name), flush=True)
        if openai:
            # the latest tool results' text (amiga-pi's rig checks read it)
            for m in tool_tail(msgs):
                print("[INFO]   tool_result %s: %s" % (m.get("tool_call_id"), result_text(m)), flush=True)
        with open(os.path.join(PI if openai else STREAMS, name + ".sse"), "rb") as f:
            data = f.read()
        if self.page_host and name == "tool_fetch":
            data = data.replace(b"127.0.0.1:8080", self.page_host.encode())
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("request-id", "req_fixture")
        self.end_headers()
        step = 64
        for i in range(0, len(data), step):
            piece = data[i:i + step]
            self.wfile.write(b"%x\r\n" % len(piece) + piece + b"\r\n")
            self.wfile.flush()
            if self.delay:
                time.sleep(self.delay)
        self.wfile.write(b"0\r\n\r\n")
        self.wfile.flush()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--bind", default="127.0.0.1",
                    help="address to listen on: 127.0.0.1 for the rig (FS-UAE uses the host's stack); the Mac's LAN address for a real Amiga (never 0.0.0.0)")
    ap.add_argument("--stream", help="answer every request with tests/claude/NAME.sse")
    ap.add_argument("--delay", type=float, default=0.05, help="seconds between 64-byte chunks")
    ap.add_argument("--dump", help="write each request body to DIR/NNN.json (tools/rig/claude_rig.py reads them)")
    ap.add_argument("--page-host", help="host:port WebFetch's recorded call fetches /page from (default 127.0.0.1:8080)")
    a = ap.parse_args()
    if a.stream and not os.path.exists(os.path.join(STREAMS, a.stream + ".sse")):
        sys.exit("[ERROR] no tests/claude/%s.sse" % a.stream)
    Handler.forced = a.stream
    Handler.delay = a.delay
    Handler.page_host = a.page_host
    Handler.dump = a.dump
    Handler.ndump = 0
    if a.dump:
        os.makedirs(a.dump, exist_ok=True)
    srv = ThreadingHTTPServer((a.bind, a.port), Handler)
    print("[INFO] serving the recorded streams on port %d (Ctrl+C ends)" % a.port, flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
