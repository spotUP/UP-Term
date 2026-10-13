#!/usr/bin/env python3
"""Tests for tools/claude_fixture.py (amiga-pi plan B10).

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
    ("amiga-pi rig step 14 (the model name of its RPC golden)", body(user("hello"), model="fake-model"), "pirpc_text"),
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


OPENAI = [
    ("text", {"model": "m", "messages": [{"role": "user", "content": "hello"}]}, "openai_text"),
    ("startup", {"model": "m", "messages": [{"role": "user", "content": "show me the startup"}]}, "openai_tools"),
    ("tool in blocks", {"model": "m", "messages": [{"role": "user", "content": [{"type": "text", "text": "use a tool"}]}]},
     "openai_tools"),
    ("after results", {"model": "m", "messages": [{"role": "user", "content": "tool"},
                                                  {"role": "tool", "tool_call_id": "c", "content": "x"}]}, "openai_final"),
    ("shell", {"model": "m", "messages": [{"role": "user", "content": "ask the shell"}]}, "openai_shell"),
    ("blocked bash", {"model": "m", "messages": [{"role": "user", "content": "try the blocked bash"}]}, "openai_bash"),
    ("mcp", {"model": "m", "messages": [{"role": "user", "content": "use the mcp tools"}]}, "openai_mcp"),
    ("after the mcp results", {"model": "m", "messages": [
        {"role": "user", "content": "use the mcp tools"},
        {"role": "tool", "tool_call_id": "call_McpMac", "content": "echo: from the Amiga"}]}, "openai_final"),
    ("edit (before startup/tool)", {"model": "m", "messages": [{"role": "user", "content": "please edit the tool"}]},
     "openai_edit"),
    ("after openai_edit's results", {"model": "m", "messages": [
        {"role": "user", "content": "please edit it"},
        {"role": "tool", "tool_call_id": "call_PiEditTest", "content": "ok"},
        {"role": "tool", "tool_call_id": "call_PiListRam", "content": "pi-test.txt"}]}, "openai_edit_final"),
]


class ToolResultLog(unittest.TestCase):
    def test_the_last_rounds_results_are_logged(self):
        msgs = [{"role": "user", "content": "x"}, {"role": "tool", "tool_call_id": "a", "content": "old"},
                {"role": "assistant", "content": "y"}, {"role": "tool", "tool_call_id": "b", "content": "one\ntwo"},
                {"role": "tool", "tool_call_id": "c", "content": [{"type": "text", "text": "z" * 400}]}]
        tail = fx.tool_tail(msgs)
        self.assertEqual([m["tool_call_id"] for m in tail], ["b", "c"])
        self.assertEqual(fx.result_text(tail[0]), "one\\ntwo")
        self.assertEqual(len(fx.result_text(tail[1])), 300)


class OpenAiRoute(unittest.TestCase):
    def test_every_route_picks_its_recording(self):
        for what, b, want in OPENAI:
            with self.subTest(what):
                self.assertEqual(fx.pick_openai(b), want)
                self.assertTrue((ROOT / "tests" / "pi" / (want + ".sse")).is_file())


RESPONSES = [
    ("text", {"model": "m", "input": [{"role": "user", "content": [{"type": "input_text", "text": "hello"}]}]},
     "responses/plain"),
    ("startup", {"model": "m", "input": [{"role": "user", "content": [{"type": "input_text", "text": "the startup"}]}]},
     "responses/tool"),
    ("after a result", {"model": "m", "input": [{"role": "user", "content": "the startup"},
                                                  {"type": "function_call_output", "call_id": "c", "output": "x"}]},
     "responses/plain"),
]


GOOGLE = [
    ("text", {"contents": [{"role": "user", "parts": [{"text": "hello"}]}]}, "google/plain"),
    ("startup", {"contents": [{"role": "user", "parts": [{"text": "the startup"}]}]}, "google/tools2"),
    ("after a result", {"contents": [{"role": "user", "parts": [{"text": "the startup"}]},
                                      {"role": "model", "parts": [{"functionCall": {"name": "read", "args": {}}}]},
                                      {"role": "user", "parts": [{"functionResponse": {"name": "read", "response": {"output": "x"}}}]}]},
     "google/plain"),
]


BEDROCK = [
    ("text", {"messages": [{"role": "user", "content": [{"text": "hello"}]}]}, "bedrock/plain"),
    ("startup", {"messages": [{"role": "user", "content": [{"text": "the startup"}]}]}, "bedrock/tools2"),
    ("after a result", {"messages": [{"role": "user", "content": [{"text": "the startup"}]},
                                      {"role": "assistant", "content": [{"toolUse": {"toolUseId": "t", "name": "read", "input": {}}}]},
                                      {"role": "user", "content": [{"toolResult": {"toolUseId": "t", "content": [{"text": "x"}], "status": "success"}}]}]},
     "bedrock/plain"),
]


def event_stream_messages(data):
    """the messages of an AWS event stream (total length, headers length, prelude CRC, headers, payload, message CRC), the
    checksums verified with zlib: [(event type, payload bytes)]"""
    import struct
    import zlib
    out = []
    i = 0
    while i < len(data):
        total, hlen, pcrc = struct.unpack(">III", data[i:i + 12])
        assert zlib.crc32(data[i:i + 8]) == pcrc, "prelude CRC"
        msg = data[i:i + total]
        assert zlib.crc32(msg[:-4]) == struct.unpack(">I", msg[-4:])[0], "message CRC"
        hdr = msg[12:12 + hlen]
        j = 0
        et = None
        while j < len(hdr):
            nl = hdr[j]
            name = hdr[j + 1:j + 1 + nl].decode()
            j += 1 + nl
            assert hdr[j] == 7, "string header"
            vl = struct.unpack(">H", hdr[j + 1:j + 3])[0]
            val = hdr[j + 3:j + 3 + vl].decode()
            j += 3 + vl
            if name == ":event-type":
                et = val
        out.append((et, msg[12 + hlen:-4]))
        i += total
    return out


class BedrockRoute(unittest.TestCase):
    def test_every_route_picks_its_recording(self):
        for what, b, want in BEDROCK:
            with self.subTest(what):
                self.assertEqual(fx.pick_bedrock(b), want)
                self.assertTrue((ROOT / "tests" / "pi" / (want + ".es")).is_file())

    def test_the_recordings_are_valid_event_streams(self):
        for _, _, want in BEDROCK:
            with self.subTest(want):
                msgs = event_stream_messages((ROOT / "tests" / "pi" / (want + ".es")).read_bytes())
                self.assertEqual(msgs[0][0], "messageStart")
                self.assertEqual(msgs[-1][0], "metadata")
                self.assertIn("messageStop", [m[0] for m in msgs])


class GoogleRoute(unittest.TestCase):
    def test_every_route_picks_its_recording(self):
        for what, b, want in GOOGLE:
            with self.subTest(what):
                self.assertEqual(fx.pick_google(b), want)
                self.assertTrue((ROOT / "tests" / "pi" / (want + ".sse")).is_file())


class ResponsesRoute(unittest.TestCase):
    def test_every_route_picks_its_recording(self):
        for what, b, want in RESPONSES:
            with self.subTest(what):
                self.assertEqual(fx.pick_responses(b), want)
                self.assertTrue((ROOT / "tests" / "pi" / (want + ".sse")).is_file())


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

    def post(self, b, path="/v1/messages", headers=None):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        raw = json.dumps(b).encode()
        c.request("POST", path, raw, {"Content-Type": "application/json", "Content-Length": str(len(raw)), **(headers or {})})
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

    def test_chat_completions_streams_the_openai_recording(self):
        for what, b, want in OPENAI:
            with self.subTest(what):
                r, data = self.post(b, "/v1/chat/completions")
                self.assertEqual(r.status, 200)
                self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".sse")).read_bytes())
        # and /v1/messages still answers with the Messages API's recordings
        r, data = self.post({"model": "m", "messages": [{"role": "user", "content": "hello"}]})
        self.assertEqual(data, (ROOT / "tests" / "claude" / "text.sse").read_bytes())

    def test_responses_streams_the_openai_responses_recording(self):
        for what, b, want in RESPONSES:
            with self.subTest(what):
                r, data = self.post(b, "/v1/responses")
                self.assertEqual(r.status, 200)
                self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".sse")).read_bytes())

    def test_codex_responses_streams_the_responses_recording_and_logs_the_originator(self):
        import io
        log = io.StringIO()
        sys.stdout = log
        try:
            for what, b, want in RESPONSES:
                with self.subTest(what):
                    r, data = self.post(b, "/codex/responses", {"originator": "pi"})
                    self.assertEqual(r.status, 200)
                    self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".sse")).read_bytes())
        finally:
            sys.stdout = open(os.devnull, "w")
        self.assertEqual(log.getvalue().count("[INFO]   codex originator=pi"), len(RESPONSES))
        self.assertIn("[INFO] /codex/responses ", log.getvalue())

    def test_azure_responses_with_an_api_version_query_gets_the_responses_recording(self):
        for what, b, want in RESPONSES:
            with self.subTest(what):
                r, data = self.post(b, "/openai/v1/responses?api-version=v1")
                self.assertEqual(r.status, 200)
                self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".sse")).read_bytes())

    def test_google_streams_the_google_recording(self):
        for what, b, want in GOOGLE:
            with self.subTest(what):
                r, data = self.post(b, "/v1beta/models/gemini-3-flash-preview:streamGenerateContent?alt=sse")
                self.assertEqual(r.status, 200)
                self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".sse")).read_bytes())

    def test_bedrock_streams_an_event_stream(self):
        for what, b, want in BEDROCK:
            with self.subTest(what):
                r, data = self.post(b, "/model/anthropic.claude-sonnet-5-5/converse-stream")
                self.assertEqual(r.status, 200)
                self.assertEqual(r.getheader("Content-Type"), "application/vnd.amazon.eventstream")
                self.assertEqual(r.getheader("Transfer-Encoding"), "chunked")
                self.assertEqual(data, (ROOT / "tests" / "pi" / (want + ".es")).read_bytes())
                self.assertEqual(event_stream_messages(data)[0][0], "messageStart")
        # an id with a colon is written %3A in the path
        r, data = self.post(BEDROCK[0][1], "/model/amazon.nova-pro-v1%3A0/converse-stream")
        self.assertEqual(r.status, 200)

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


class OAuthServer(unittest.TestCase):
    """--oauth DIR: amiga-pi's /login tests (phase O) play the providers' OAuth servers from DIR/script.json."""

    @classmethod
    def setUpClass(cls):
        import tempfile
        cls.dir = tempfile.mkdtemp()
        fx.Handler.oauth = fx.OAuthScript(cls.dir)
        fx.Handler.forced = None
        fx.Handler.delay = 0
        cls.srv = ThreadingHTTPServer(("127.0.0.1", 0), fx.Handler)
        cls.port = cls.srv.server_address[1]
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        fx.Handler.oauth = None
        cls.srv.shutdown()
        cls.srv.server_close()

    def script(self, responses):
        with open(os.path.join(self.dir, "script.json"), "w") as f:
            json.dump({"responses": responses}, f)
        try:
            os.remove(os.path.join(self.dir, "requests.log"))
        except OSError:
            pass

    def call(self, method, path, body=b"", headers=None):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        h = {"Content-Type": "application/x-www-form-urlencoded", **(headers or {})}
        c.request(method, path, body or None, h)
        r = c.getresponse()
        data = r.read()
        c.close()
        return r, data

    def log(self):
        with open(os.path.join(self.dir, "requests.log"), encoding="utf-8") as f:
            return f.read()

    def test_answers_in_order_per_url_and_the_last_repeats(self):
        url = "https://auth.example.org/oauth/token"
        self.script([{"url": url, "status": 400, "json": {"error": "authorization_pending"}},
                     {"url": url, "status": 200, "json": {"access_token": "a"}},
                     {"url": "https://other.example.org/x", "status": 200, "text": "plain"}])
        r, data = self.call("POST", "/auth.example.org/oauth/token", b"a=1")
        self.assertEqual((r.status, data), (400, b'{"error":"authorization_pending"}'))
        r, data = self.call("POST", "/auth.example.org/oauth/token", b"a=1")
        self.assertEqual((r.status, data), (200, b'{"access_token":"a"}'))
        r, data = self.call("POST", "/auth.example.org/oauth/token", b"a=1")
        self.assertEqual((r.status, data), (200, b'{"access_token":"a"}'))
        r, data = self.call("GET", "/other.example.org/x")
        self.assertEqual((r.status, data, r.getheader("Content-Type")), (200, b"plain", "text/plain"))

    def test_unscripted_url_is_404_and_the_script_is_read_again_when_it_changes(self):
        self.script([])
        r, _ = self.call("POST", "/auth.example.org/nothing", b"x")
        self.assertEqual(r.status, 404)
        self.script([{"url": "https://auth.example.org/nothing", "status": 201, "json": {}}])
        r, _ = self.call("POST", "/auth.example.org/nothing", b"x")
        self.assertEqual(r.status, 201)

    def test_the_request_log_is_the_oracles_text(self):
        self.script([{"url": "https://auth.example.org/t?x=1", "json": {}}])
        self.call("POST", "/auth.example.org/t?x=1", b"grant_type=refresh_token&client_id=c",
                  {"Accept": "application/json", "Content-Type": "application/x-www-form-urlencoded"})
        self.assertEqual(self.log(), "POST https://auth.example.org/t?x=1\naccept: application/json\n"
                         "content-type: application/x-www-form-urlencoded\n\ngrant_type=refresh_token&client_id=c\n--\n")

    def test_a_path_without_a_host_is_not_oauth(self):
        self.script([])
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        c.request("POST", "/v1/messages", b"{", {"Content-Length": "1"})
        r = c.getresponse()
        r.read()
        c.close()
        self.assertEqual(r.status, 400)             # the Messages route's "the body is not JSON"

    def test_extra_headers_are_sent(self):
        self.script([{"url": "https://auth.example.org/h", "status": 429, "text": "", "headers": {"Retry-After": "2"}}])
        r, _ = self.call("GET", "/auth.example.org/h")
        self.assertEqual((r.status, r.getheader("Retry-After")), (429, "2"))


if __name__ == "__main__":
    unittest.main(verbosity=1)
