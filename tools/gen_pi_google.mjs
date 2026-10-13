// Goldens for llm/be_google.c, made by pi's own code: pi-ai's google-generative-ai provider (api/google-generative-ai.js
// streamSimple, on the @google/genai SDK) run on each case of tests/pi/google/cases.json with globalThis.fetch replaced by
// a fake that answers with the case's .sse file (the adapter refuses a custom fetch option). Writes, per case, next to
// cases.json:
//   <name>.body     the request body the SDK sent (the exact text)
//   <name>.req      the request line target and the headers the SDK sent that the test compares
//   <name>.digest   what pi made of the stream (stop, error, usage, blocks, raw stop reason, response id)
// Date.now is fixed (1760100000000) and each case imports its own copy of the adapter, so the generated tool-call ids
// ("<name>_<ms>_<counter>") start at counter 1 in every case.
// A message "$reply" in a case's messages is replaced by the assistant message pi made for the named case; a message
// {"$result": {reply, index, text, isError?}} becomes the toolResult for that reply's tool call; "$id" on a message
// gives its toolCallId from a reply's tool call.
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-ai> node tools/gen_pi_google.mjs
//   (the pinned install: amiga-pi/vendor/pi-1.1.0)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p, q = "") => import(pathToFileURL(join(dist, p)).href + q);
const { MODELS } = await imp("models.generated.js");
const { normalizeContext } = await imp("utils/transcript.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "google");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const NOW = 1760100000000;
Date.now = () => NOW;
const replies = {};

function digest(m) {
	const j = (v) => JSON.stringify(v);
	let s = `stop=${m.stopReason}\nerror=${m.errorMessage ?? "-"}\n`;
	s += `usage=${m.usage.input},${m.usage.output},${m.usage.cacheRead},${m.usage.cacheWrite}\n`;
	s += `usage2=${m.usage.reasoning}\n`;
	s += `raw=${m.rawStopReason ?? "-"} rid=${m.responseId ?? "-"}\n`;
	for (const b of m.content) {
		if (b.type === "thinking") s += `thinking=${j(b.thinking)} sig=${b.thinkingSignature === undefined ? "-" : j(b.thinkingSignature)}\n`;
		else if (b.type === "text") s += `text=${j(b.text)} sig=${b.textSignature === undefined ? "-" : j(b.textSignature)}\n`;
		else if (b.type === "toolCall") s += `tool=${b.id} ${b.name} ${j(b.arguments)} sig=${b.thoughtSignature === undefined ? "-" : j(b.thoughtSignature)}\n`;
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

let n = 0;
for (const c of cases) {
	const [provider, id] = c.model.split("/");
	const model = c.window ? { ...MODELS[provider][id], contextWindow: c.window } : MODELS[provider][id];
	const sse = readFileSync(join(dir, c.sse + ".sse"), "utf8");
	let url, init;
	globalThis.fetch = async (u, i) => {
		url = String(u);
		init = i;
		return new Response(sse, { status: 200, headers: { "content-type": "text/event-stream" } });
	};
	const { streamSimple } = await imp("api/google-generative-ai.js", "?case=" + n++);
	const context = normalizeContext({ systemPrompt: c.system, messages: c.messages.map(expand), tools: c.tools });
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", reasoning: c.level === "off" ? undefined : c.level });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	replies[c.name] = final;
	const h = new Headers(init.headers);
	writeFileSync(join(dir, c.name + ".body"), init.body);
	writeFileSync(join(dir, c.name + ".req"), `POST ${url}\nx-goog-api-key: ${h.get("x-goog-api-key")}\ncontent-type: ${h.get("content-type")}\n`);
	writeFileSync(join(dir, c.name + ".digest"), digest(final));
}
console.log("[OK] tests/pi/google written, " + cases.length + " cases");
