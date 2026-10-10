#!/usr/bin/env python3
"""Tests for tools/claude_fixture.py (amiga-pi plan B10, first half).

The fixture's /v1/messages routing is what C:Claude's rig runs and the owner's
Amiga checks depend on. This snapshot pins every route -- which recording each
kind of request is answered with -- and checks over a real socket that the
answer is the recording's bytes, so a later route (amiga-pi's
/v1/chat/completions) cannot change what C:Claude gets.

  python3 tools/test_claude_fixture.py      (in `make test`; ONLY=claude_fixture alone)
"""
import http.client
import json
import os
import pathlib
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import claude_fixture as fx  # noqa: E402


def user(text):
    return {"role": "user", "content": text}


def result(tool_use_id):
    return {"role": "user", "content": [{"type": "tool_result", "tool_use_id": tool_use_id, "content": "x"}]}


def body(*msgs, **kw):
    b = {"model": kw.get("model", "claude-opus-5-5"), "messages": list(msgs)}
    if "system" in kw:
        b["system"] = kw["system"]
    return b


# (what, request body, recording) -- every branch of fx.pick, in its order
ROUTES = [
    ("no messages", body(), "text"),
    ("Haiku (WebFetch's small model)", body(user("x"), model="claude-haiku-4-5"), "fetch_answer"),
    ("web search's own request", body(user("x"), system="You are performing a web search"), "websearch"),
    ("system as blocks", body(user("x"), system=[{"type": "text", "text": "performing a web search"}]), "websearch"),
    ("subagent, first", body(user("find"), system="a file search specialist"), "agent_tool"),
    ("subagent, after its tool", body(user("find"), result("toolu_x"), system="a file search specialist"),
     "agent_final"),
    ("result of EnterPlanMode", body(user("p"), result("toolu_01EnterPlan")), "tool_exitplan"),
    ("result of a background Bash", body(user("p"), result("toolu_01Background")), "tool_bgout"),
    ("result of ScheduleWakeup", body(user("p"), result("toolu_01Loop")), "loop_final"),
    ("any other tool result", body(user("p"), result("toolu_01ReadStartup")), "tool_final"),
    ("loop test (before plan/schedule words)", body(user("/loop loop test, plan a schedule")), "tool_loop"),
    ("search the web", body(user("Search the web for Amiga")), "tool_websearch"),
    ("monitor", body(user("monitor it")), "tool_monitor"),
    ("schedule", body(user("schedule it")), "tool_cron"),
    ("time limit", body(user("time limit")), "tool_timelimit"),
    ("fetch", body(user("fetch the page")), "tool_fetch"),
    ("agent", body(user("use an agent")), "tool_task"),
    ("question", body(user("ask me a question")), "tool_ask"),
    ("plan", body(user("make a plan")), "tool_enterplan"),
    ("background", body(user("in the background")), "tool_bg"),
    ("grep", body(user("grep S")), "tool_grep"),
    ("slow", body(user("something slow")), "tool_slow"),
    ("edit", body(user("edit the file")), "tool_edit"),
    ("startup", body(user("show the startup")), "tool_use"),
    ("tool", body(user("use a tool")), "tool_use"),
    ("refuse", body(user("refuse this")), "refusal"),
    ("busy", body(user("are you busy")), "overloaded"),
    ("text blocks", body({"role": "user", "content": [{"type": "text", "text": "hello"}]}), "text"),
    ("anything else", body(user("hello")), "text"),
]


class Routes(unittest.TestCase):
    def test_every_route_picks_its_recording(self):
        for what, b, want in ROUTES:
            with self.subTest(what):
                self.assertEqual(fx.pick(b, None), want)

    def test_every_route_names_a_recording(self):
        for what, _, want in ROUTES:
            with self.subTest(what):
                self.assertTrue((ROOT / "tests" / "claude" / (want + ".sse")).is_file())

    def test_forced_stream_wins(self):
        self.assertEqual(fx.pick(body(user("edit")), "text"), "text")


class OverHttp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fx.Handler.delay = 0
        fx.Handler.forced = None
        fx.Handler.dump = None
        fx.Handler.page_host = None
        cls.srv = ThreadingHTTPServer(("127.0.0.1", 0), fx.Handler)
        cls.port = cls.srv.server_address[1]
        cls.t = threading.Thread(target=cls.srv.serve_forever, daemon=True)
        cls.t.start()
        cls._out = sys.stdout
        sys.stdout = open(os.devnull, "w")      # the fixture's one-line request summaries

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()
        cls.srv.server_close()
        sys.stdout.close()
        sys.stdout = cls._out

    def post(self, b, path="/v1/messages"):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        raw = json.dumps(b).encode()
        c.request("POST", path, raw, {"Content-Type": "application/json", "Content-Length": str(len(raw))})
        r = c.getresponse()
        data = r.read()
        c.close()
        return r, data

    def test_answers_are_the_recordings_bytes(self):
        for what, b, want in ROUTES:
            with self.subTest(what):
                r, data = self.post(b)
                self.assertEqual(r.status, 200)
                self.assertEqual(r.getheader("Content-Type"), "text/event-stream; charset=utf-8")
                self.assertEqual(r.getheader("Transfer-Encoding"), "chunked")
                self.assertEqual(data, (ROOT / "tests" / "claude" / (want + ".sse")).read_bytes())

    def test_body_not_json_is_400(self):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        c.request("POST", "/v1/messages", b"{", {"Content-Length": "1"})
        r = c.getresponse()
        r.read()
        c.close()
        self.assertEqual(r.status, 400)

    def test_page_for_webfetch(self):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        c.request("GET", "/page")
        r = c.getresponse()
        data = r.read()
        c.close()
        self.assertEqual((r.status, data), (200, fx.PAGE))


if __name__ == "__main__":
    unittest.main(verbosity=1)
