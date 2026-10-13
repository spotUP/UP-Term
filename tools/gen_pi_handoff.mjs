// Goldens for the handoff between apis (Phase M7), made by pi's own code: a conversation produced by one api replayed to
// another. tests/pi/handoff/cases.json lists the models ("sources", each also a target). For each source pi's provider
// (api/<api>.js streamSimple) runs on the recorded stream tests/pi/handoff/<sse>.sse with a fake fetch; the assistant
// message pi makes is the first answer of a conversation  user -> assistant -> tool results -> user, and every other
// source's provider (pi's transformMessages inside it) builds the request for it. Writes
//   src_<name>.digest     what pi made of the source stream (the digest form of gen_pi_responses.mjs)
//   <src>__<dst>.body     the request body pi sent to <dst> for the conversation <src> produced (every ordered pair of
//                         models whose api or model differs)
// Date.now is fixed and every run imports its own copy of the adapter (google's id counter starts at 1 each time).
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-ai> node tools/gen_pi_handoff.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p, q = "") => import(pathToFileURL(join(dist, p)).href + q);
const { MODELS } = await imp("models.generated.js");
const { normalizeContext } = await imp("utils/transcript.js");
const { clampThinkingLevel } = await imp("models.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "handoff");
const cfg = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const NOW = 1760100000000;
Date.now = () => NOW;
const MODULE = {
	"anthropic-messages": "api/anthropic-messages.js",
	"openai-completions": "api/openai-completions.js",
	"google-generative-ai": "api/google-generative-ai.js",
	"openai-responses": "api/openai-responses.js",
	"azure-openai-responses": "api/azure-openai-responses.js",
};

function modelOf(s) {
	if (s.custom) {
		return { id: s.custom.id, name: s.custom.id, api: "openai-completions", provider: s.custom.provider, baseUrl: s.custom.baseUrl,
			reasoning: false, input: ["text"], cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0 },
			contextWindow: s.custom.window, maxTokens: s.custom.maxTokens };
	}
	const [provider, id] = s.model.split("/");
	const m = MODELS[provider][id];
	return s.baseUrl !== undefined ? { ...m, baseUrl: s.baseUrl } : m;
}

function digest(m) {
	const j = (v) => JSON.stringify(v);
	let s = `stop=${m.stopReason}\nerror=${m.errorMessage ?? "-"}\n`;
	s += `usage=${m.usage.input},${m.usage.output},${m.usage.cacheRead},${m.usage.cacheWrite}\n`;
	for (const b of m.content) {
		if (b.type === "thinking") s += `thinking=${j(b.thinking)} sig=${b.thinkingSignature === undefined ? "-" : j(b.thinkingSignature)}\n`;
		else if (b.type === "text") s += `text=${j(b.text)} sig=${b.textSignature === undefined ? "-" : j(b.textSignature)}\n`;
		else if (b.type === "toolCall") s += `tool=${b.id} ${b.name} ${j(b.arguments)}\n`;
	}
	return s;
}

let n = 0;
// one provider run: the final message and the request body it sent
async function run(model, sse, messages, tools) {
	let body;
	globalThis.fetch = async (u, init) => {
		body = init.body;
		return new Response(sse, { status: 200, headers: { "content-type": "text/event-stream" } });
	};
	const { streamSimple } = await imp(MODULE[model.api], "?run=" + n++);
	const context = normalizeContext({ systemPrompt: cfg.system, messages, tools });
	const level = clampThinkingLevel(model, cfg.level);
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", fetch: globalThis.fetch, reasoning: level === "off" ? undefined : level });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	return { final, body };
}

const U = (t, ts) => ({ role: "user", content: [{ type: "text", text: t }], timestamp: ts });
const produced = {};
for (const s of cfg.sources) {
	const sse = readFileSync(join(dir, s.sse + ".sse"), "utf8");
	const { final } = await run(modelOf(s), sse, [U("look at the startup sequence", 1760100000000)], cfg.tools);
	produced[s.name] = final;
	writeFileSync(join(dir, "src_" + s.name + ".digest"), digest(final));
}
let pairs = 0;
for (const s of cfg.sources) {
	const a = produced[s.name];
	const results = a.content.filter((b) => b.type === "toolCall").map((b) => ({
		role: "toolResult", toolCallId: b.id, toolName: b.name, content: [{ type: "text", text: "result of " + b.name }],
		isError: false, timestamp: 1760100002000 }));
	const messages = [U("look at the startup sequence", 1760100000000), a, ...results, U("thanks", 1760100003000)];
	for (const t of cfg.sources) {
		if (t.name === s.name) continue;
		const { body } = await run(modelOf(t), readFileSync(join(dir, t.sse + ".sse"), "utf8"), messages, cfg.tools);
		writeFileSync(join(dir, s.name + "__" + t.name + ".body"), body);
		pairs++;
	}
}
console.log("[OK] tests/pi/handoff written, " + cfg.sources.length + " sources, " + pairs + " ordered pairs");
