#!/usr/bin/env python3
"""claude_fixture -- the Messages API's streamed answers from recordings, over
plain HTTP on the LAN, for C:Claude on the rig or a real Amiga (ledger A2).

It never talks to Anthropic and needs no key. It answers POST /v1/messages
with one of tests/claude/*.sse, chunked, a few bytes at a time like the real
stream:

  - the request's last message holds a tool_result  -> tool_final.sse
  - the last prompt mentions "startup" or "tool"      -> tool_use.sse
    (read_file S/Startup-Sequence and list_dir S: start Claude with ROOT=SYS:)
  - mentions "refuse"                                 -> refusal.sse
  - mentions "busy"                                   -> overloaded.sse
  - anything else                                     -> text.sse
  (--stream NAME answers every request with NAME.sse instead.)

On the Amiga:  Claude URL=http://<this machine>:8080/v1/messages ROOT=SYS:
Each request is summarised here: model, messages, tool results, and whether
an x-api-key header came (it must not: the key only goes over https).

  python3 tools/claude_fixture.py [--port 8080] [--stream text] [--delay 0.05]
"""
import argparse
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
STREAMS = os.path.join(HERE, "..", "tests", "claude")


def pick(body, forced):
    if forced:
        return forced
    msgs = body.get("messages") or []
    if not msgs:
        return "text"
    last = msgs[-1]
    content = last.get("content")
    blocks = content if isinstance(content, list) else [{"type": "text", "text": str(content)}]
    if any(b.get("type") == "tool_result" for b in blocks):
        return "tool_final"
    text = " ".join(b.get("text", "") for b in blocks if b.get("type") == "text").lower()
    if "startup" in text or "tool" in text:
        return "tool_use"
    if "refuse" in text:
        return "refusal"
    if "busy" in text:
        return "overloaded"
    return "text"


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    forced = None
    delay = 0.05

    def log_message(self, fmt, *args):
        pass

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)
        try:
            body = json.loads(raw)
        except ValueError:
            self.send_error(400, "the body is not JSON")
            return
        name = pick(body, self.forced)
        msgs = body.get("messages") or []
        results = sum(1 for m in msgs if isinstance(m.get("content"), list)
                      for b in m["content"] if b.get("type") == "tool_result")
        print("[INFO] %s model=%s messages=%d tool_results=%d key_header=%s -> %s.sse" % (
            self.path, body.get("model"), len(msgs), results,
            "PRESENT (wrong!)" if self.headers.get("x-api-key") else "none", name), flush=True)
        with open(os.path.join(STREAMS, name + ".sse"), "rb") as f:
            data = f.read()
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
    ap.add_argument("--stream", help="answer every request with tests/claude/NAME.sse")
    ap.add_argument("--delay", type=float, default=0.05, help="seconds between 64-byte chunks")
    a = ap.parse_args()
    if a.stream and not os.path.exists(os.path.join(STREAMS, a.stream + ".sse")):
        sys.exit("[ERROR] no tests/claude/%s.sse" % a.stream)
    Handler.forced = a.stream
    Handler.delay = a.delay
    srv = ThreadingHTTPServer(("0.0.0.0", a.port), Handler)
    print("[INFO] serving the recorded streams on port %d (Ctrl+C ends)" % a.port, flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
