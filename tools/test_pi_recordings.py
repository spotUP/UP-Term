#!/usr/bin/env python3
"""amiga-pi's OpenAI recordings (tests/pi/openai_*) against the documented wire format.

The recordings were written by hand in phase B (no OpenAI-compatible server
was at hand), so this check holds them to OpenAI's own schema:
openai/openai-openapi e95c0fe615f45a19e8878af923a729948445a6bd openapi.yaml,
CreateChatCompletionStreamResponse (line 44796; required choices, created, id,
model, object = "chat.completion.chunk"; each choice requires delta and index,
finish_reason one of stop, length, tool_calls, content_filter, function_call
or null; choices may be empty on the include_usage chunk),
ChatCompletionMessageToolCallChunk (line 42158; index required, type
"function"), CompletionUsage (line 43376; prompt_tokens, completion_tokens,
total_tokens required) and Error (line 49534; type, message, param, code
required, under "error"). The stream ends with "data: [DONE]" (pi f10993bc
openai-completions.ts reads it as the end). Run: python3 -I tools/test_pi_recordings.py
"""
import glob
import json
import os
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FINISH = {"stop", "length", "tool_calls", "content_filter", "function_call", None}


def chunks(path):
    """the data: payloads of an SSE file, in order"""
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f.read().split("\n"):
            if line.startswith("data: "):
                out.append(line[6:])
            elif line and not line.startswith(":"):
                raise AssertionError("%s: a line that is not data: %r" % (path, line[:60]))
    return out


def problems_chunk(c):
    bad = []
    for k in ("choices", "created", "id", "model", "object"):
        if k not in c:
            bad.append("missing " + k)
    if c.get("object") != "chat.completion.chunk":
        bad.append("object %r" % c.get("object"))
    if not isinstance(c.get("created"), int):
        bad.append("created not an integer")
    for ch in c.get("choices", []):
        for k in ("delta", "index"):
            if k not in ch:
                bad.append("choice missing " + k)
        if ch.get("finish_reason", None) not in FINISH:
            bad.append("finish_reason %r" % ch.get("finish_reason"))
        for tc in ch.get("delta", {}).get("tool_calls", []) or []:
            if "index" not in tc:
                bad.append("tool call chunk without index")
            if "type" in tc and tc["type"] != "function":
                bad.append("tool call type %r" % tc["type"])
    u = c.get("usage")
    if u is not None:
        for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
            if not isinstance(u.get(k), int):
                bad.append("usage missing " + k)
    return bad


def problems_sse(path, cut=False):
    bad = []
    data = chunks(path)
    if not data:
        return ["no data lines"]
    if not cut and data[-1] != "[DONE]":
        bad.append("does not end with [DONE]")
    for i, d in enumerate(data):
        if d == "[DONE]":
            if i != len(data) - 1:
                bad.append("[DONE] before the end")
            continue
        try:
            c = json.loads(d)
        except ValueError:
            if cut and i == len(data) - 1:
                continue        # the cut recording may end inside a chunk
            bad.append("chunk %d is not JSON" % i)
            continue
        bad += ["chunk %d: %s" % (i, p) for p in problems_chunk(c)]
    return bad


def problems_error(path):
    with open(path, "rb") as f:
        raw = f.read()
    head, _, body = raw.partition(b"\r\n\r\n")
    bad = []
    lines = head.decode("latin-1").split("\r\n")
    hdr = dict(l.split(": ", 1) for l in lines[1:])
    if int(hdr.get("Content-Length", -1)) != len(body):
        bad.append("Content-Length %s, body %d" % (hdr.get("Content-Length"), len(body)))
    e = json.loads(body).get("error", {})
    for k in ("type", "message", "param", "code"):
        if k not in e:
            bad.append("error missing " + k)
    return bad


class Recordings(unittest.TestCase):
    def test_streams(self):
        files = sorted(glob.glob(os.path.join(ROOT, "tests/pi/openai_*.sse")))
        self.assertGreaterEqual(len(files), 4)
        for p in files:
            self.assertEqual(problems_sse(p, cut=p.endswith("_cut.sse")), [], p)

    def test_error(self):
        self.assertEqual(problems_error(os.path.join(ROOT, "tests/pi/openai_429.http")), [])

    def test_checker_rejects(self):
        self.assertIn("missing created", problems_chunk({"id": "a", "model": "m", "object": "chat.completion.chunk",
                                                          "choices": []}))
        self.assertIn("finish_reason 'tool_use'", problems_chunk(
            {"id": "a", "model": "m", "object": "chat.completion.chunk", "created": 1,
             "choices": [{"index": 0, "delta": {}, "finish_reason": "tool_use"}]}))


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
