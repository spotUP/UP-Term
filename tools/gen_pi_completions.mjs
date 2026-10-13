// Goldens for llm/be_openai_chat.c's tools and output cap, made by pi's own code: pi-ai's openai-completions provider
// (api/openai-completions.js streamSimple) run on each case of tests/pi/completions/cases.json with a fake fetch that
// answers with the case's .sse file (tests/pi/<sse>.sse). Writes <name>.body next to cases.json.
// A case's "model" is a models.json style model (compat as pi's, "window" the context window, "provider" and
// "baseUrl" the host, default a LAN one). supportsStore is left to pi's host rule except in the cases that set it.
// A case with "digest": true also writes <name>.digest, what pi made of the stream (the form of gen_pi_anthropic.mjs).
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-ai> node tools/gen_pi_completions.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p) => import(pathToFileURL(join(dist, p)).href);
const { streamSimple } = await imp("api/openai-completions.js");
const { normalizeContext } = await imp("utils/transcript.js");
const root = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi");
const dir = join(root, "completions");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
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
for (const c of cases) {
	const model = {
		id: c.model.id, name: c.model.id, api: "openai-completions", provider: c.model.provider ?? "lan",
		baseUrl: c.model.baseUrl ?? "http://192.168.1.10:11434/v1", reasoning: false, input: ["text"],
		cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0 }, contextWindow: c.model.window, maxTokens: c.model.maxTokens,
		compat: c.model.compat,
	};
	const sse = readFileSync(join(root, c.sse + ".sse"), "utf8");
	let body;
	const fetch = async (url, init) => {
		body = init.body;
		return new Response(sse, { status: 200, headers: { "content-type": "text/event-stream" } });
	};
	const context = normalizeContext({ systemPrompt: c.system, messages: c.messages, tools: c.tools });
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", fetch });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") { final = ev.type === "done" ? ev.message : ev.error; break; }
	writeFileSync(join(dir, c.name + ".body"), body);
	if (c.digest) writeFileSync(join(dir, c.name + ".digest"), digest(final));
}
console.log("[OK] tests/pi/completions written, " + cases.length + " cases");
