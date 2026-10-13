// Goldens for llm/be_bedrock.c (Phase M6), made by pi's own code: pi-ai's bedrock-converse-stream provider (api/
// bedrock-converse-stream.js streamSimple, on @aws-sdk/client-bedrock-runtime 3.1127.0) run on each case of
// tests/pi/bedrock/cases.json against a recorded answer (tests/pi/bedrock/streams.json), through tools/pi_bedrock_oracle.mjs
// (node:https redirected to a local server, so pi and the SDK run their real endpoint, region and signing code). Writes
// next to cases.json:
//   <stream>.es      a recorded event-stream answer, framed by the SDK's own codec (corrupt/cut streams damaged afterwards)
//   <stream>.http    a recorded error answer (status line, headers, body)
//   <case>.body      the request body the SDK sent (the exact text)
//   <case>.req       "POST <https://host/path>" and the headers the test compares: content-type, x-amz-date,
//                    x-amz-security-token, credential (the Credential= part of the SDK's Authorization), authorization (Bearer)
//   <case>.digest    what pi made of the answer (stop, error, usage, raw stop reason, blocks)
//   sdkauth.json     for every signed request: the headers the SDK signed, its credentials and the Authorization it
//                    wrote (llm/sigv4.c must write the same from the same headers)
// A message "$reply" in a case's messages is replaced by the assistant message pi made for the named case; a message
// {"$result": {reply, index, text, isError?}} becomes the toolResult for that reply's tool call; "$id" on a message
// gives its toolCallId from a reply's tool call. A case's "override" {id, name} replaces the model's, "window" the
// context window, "baseUrl" the base URL, "env" sets (or, null, removes) AWS_* variables on top of the default
// access keys. A case may carry pi's SimpleStreamOptions "temperature", "toolChoice", "thinkingBudgets" and "cacheRetention"
// (thinkingDisplay and requestMetadata are options of stream() only: streamSimple never sets them). Date.now is fixed (1760100000000 = 20251010T124000Z).
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-ai> node tools/gen_pi_bedrock.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { load, frames, server, install, clearAws } from "./pi_bedrock_oracle.mjs";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const state = install();
const { imp, codec, enc } = load(npm);
const { MODELS } = await imp("models.generated.js");
const { normalizeContext } = await imp("utils/transcript.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "bedrock");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const { streams } = JSON.parse(readFileSync(join(dir, "streams.json"), "utf8"));
const NOW = 1760100000000;
Date.now = () => NOW;
const BASE_ENV = { AWS_ACCESS_KEY_ID: "AKIDEXAMPLE", AWS_SECRET_ACCESS_KEY: "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY" };
const srv = await server(state);
const replies = {};
const sdkauth = [];

function digest(m) {
	const j = (v) => JSON.stringify(v);
	let s = `stop=${m.stopReason}\\nerror=${m.errorMessage ?? "-"}\\n`.replaceAll("\\n", "\n");
	s += `usage=${m.usage.input},${m.usage.output},${m.usage.cacheRead},${m.usage.cacheWrite}\n`;
	s += `total=${m.usage.totalTokens} w1h=${m.usage.cacheWrite1h ?? "-"}\n`;
	s += `raw=${m.rawStopReason ?? "-"}\n`;
	for (const b of m.content) {
		if (b.type === "thinking") s += `thinking=${j(b.thinking)} sig=${b.thinkingSignature === undefined ? "-" : j(b.thinkingSignature)} red=${b.redacted ? 1 : 0}\n`;
		else if (b.type === "text") s += `text=${j(b.text)}\n`;
		else if (b.type === "toolCall") s += `tool=${b.id} ${b.name} ${j(b.arguments)}\n`;
	}
	return s;
}

function expand(m) {
	if (m.$reply) return replies[m.$reply];
	const tcall = (r) => replies[r.reply].content.filter((b) => b.type === "toolCall")[r.index];
	if (m.$result) {
		const t = tcall(m.$result);
		return { role: "toolResult", toolCallId: t.id, toolName: t.name, content: [{ type: "text", text: m.$result.text }], isError: !!m.$result.isError, timestamp: 1760100002000 };
	}
	if (m.$id) {
		const { $id, ...rest } = m;
		return { ...rest, toolCallId: tcall($id).id };
	}
	return m;
}

// the answer a stream spec describes: the reply the server gives, and the files to keep
function reply(name) {
	const sp = streams[name];
	if (!sp) throw new Error("no stream " + name);
	if (sp.http) {
		const b = Buffer.from(sp.http.body, "utf8");
		const headers = { ...sp.http.headers };
		if (b.length) headers["content-length"] = String(b.length);
		const raw = `HTTP/1.1 ${sp.http.status} X\r\n` + Object.entries(sp.http.headers).map(([k, v]) => `${k}: ${v}\r\n`).join("") + `content-length: ${b.length}\r\nconnection: keep-alive\r\n\r\n`;
		writeFileSync(join(dir, name + ".http"), Buffer.concat([Buffer.from(raw), b]));
		return { status: sp.http.status, headers, body: b };
	}
	let body;
	if (sp.corrupt) {
		const fr = sp.events.map((e) => frames(codec, enc, [e]));
		const f = Buffer.from(fr[sp.corrupt.frame]);
		const at = sp.corrupt.byte < 0 ? f.length + sp.corrupt.byte : sp.corrupt.byte;
		f[at] ^= 0x40;
		fr[sp.corrupt.frame] = f;
		body = Buffer.concat(fr);
	} else body = frames(codec, enc, sp.events);
	if (sp.cut) body = body.subarray(0, body.length - sp.cut);
	writeFileSync(join(dir, name + ".es"), body);
	return { status: 200, headers: { "content-type": "application/vnd.amazon.eventstream", "x-amzn-requestid": "req-123" }, body };
}

let n = 0;
for (const c of cases) {
	const [provider, id] = c.model.split("/");
	let model = { ...MODELS[provider][id] };
	if (c.override) model = { ...model, ...c.override };
	if (c.window) model.contextWindow = c.window;
	if (c.baseUrl) model.baseUrl = c.baseUrl;
	clearAws();
	delete process.env.PI_CACHE_RETENTION;
	process.env.AWS_BEDROCK_FORCE_HTTP1 = "1";
	for (const [k, v] of Object.entries({ ...BASE_ENV, ...(c.env || {}) })) if (v !== null) process.env[k] = v;
	state.reply = reply(c.stream);
	state.last = null;
	const { streamSimple } = await imp("api/bedrock-converse-stream.js", "?case=" + n++);
	const context = normalizeContext({ systemPrompt: c.system, messages: c.messages.map(expand), tools: c.tools });
	const s = streamSimple(model, context, { reasoning: c.level === "off" ? undefined : c.level,
		...(c.temperature !== undefined && { temperature: c.temperature }), ...(c.toolChoice && { toolChoice: c.toolChoice }),
		...(c.thinkingBudgets && { thinkingBudgets: c.thinkingBudgets }), ...(c.cacheRetention && { cacheRetention: c.cacheRetention }) });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	replies[c.name] = final;
	const r = state.last;
	if (!r) throw new Error(c.name + ": no request reached the server");
	writeFileSync(join(dir, c.name + ".body"), r.body);
	const t = r.target;
	const host = t.hostname + (t.port && t.port !== (t.protocol === "http:" ? 80 : 443) ? ":" + t.port : "");
	let req = `POST ${t.protocol === "http:" ? "http" : "https"}://${host}${t.path}\n`;
	const h = r.headers;
	req += `content-type: ${h["content-type"]}\n`;
	if (h.authorization?.startsWith("Bearer ")) req += `authorization: ${h.authorization}\n`;
	if (h.authorization?.startsWith("AWS4-HMAC-SHA256")) {
		req += `x-amz-date: ${h["x-amz-date"]}\n`;
		if (h["x-amz-security-token"]) req += `x-amz-security-token: ${h["x-amz-security-token"]}\n`;
		req += `credential: ${/Credential=([^,]*)/.exec(h.authorization)[1]}\n`;
		const signed = /SignedHeaders=([^,]*)/.exec(h.authorization)[1].split(";");
		sdkauth.push({
			name: c.name, method: "POST", path: t.path, body: r.body,
			headers: Object.fromEntries(signed.map((k) => [k, k === "host" ? h.host : h[k]])),
			...(/Credential=dummy-access-key\//.test(h.authorization)
				? { access: "dummy-access-key", secret: "dummy-secret-key", token: null }          // AWS_BEDROCK_SKIP_AUTH
				: { access: process.env.AWS_ACCESS_KEY_ID, secret: process.env.AWS_SECRET_ACCESS_KEY, token: process.env.AWS_SESSION_TOKEN ?? null }),
			region: /Credential=[^/]*\/[^/]*\/([^/]*)\//.exec(h.authorization)[1], service: "bedrock", amzdate: h["x-amz-date"], authorization: h.authorization,
		});
	}
	writeFileSync(join(dir, c.name + ".req"), req);
	writeFileSync(join(dir, c.name + ".digest"), digest(final));
}
writeFileSync(join(dir, "sdkauth.json"), JSON.stringify({ requests: sdkauth }, null, 1) + "\n");
await srv.close();
console.log("[OK] tests/pi/bedrock written, " + cases.length + " cases, " + sdkauth.length + " signed");
