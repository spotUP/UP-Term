// Goldens for llm/be_anthropic_pi.c, made by pi's own code: pi-ai's anthropic-messages provider
// (api/anthropic-messages.js streamSimple) run on each case of tests/pi/anthropic/cases.json with a fake fetch that
// answers with the case's .sse file. Writes, per case, next to cases.json:
//   <name>.body     the request body pi sent (the exact text; the SDK moves `betas` into the header)
//   <name>.digest   what pi made of the stream (stop, error, usage, blocks), the form tests/test_pi_llm.c rebuilds
//   <name>.headers  the anthropic-beta header pi sent, when it sent one
// A message {"$reply": "<case>"} in a case's messages is replaced by the assistant message pi made for that case. Unaltered models: the compat
// flags (supportsMidConvo*) are pi's own.
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-ai> node tools/gen_pi_anthropic.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
// A case may carry pi's SimpleStreamOptions "temperature", "toolChoice" and "thinkingBudgets".
import { readFileSync, writeFileSync, rmSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p) => import(pathToFileURL(join(dist, p)).href);
const { MODELS } = await imp("models.generated.js");
const { streamSimple } = await imp("api/anthropic-messages.js");
const { normalizeContext } = await imp("utils/transcript.js");
const { clampThinkingLevel } = await imp("models.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "anthropic");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const replies = {};

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
	const [provider, id] = c.model.split("/");
	// "compat" in a case replaces the model's compat (a models.json model with only that flag)
	// "window" replaces the model's context window (a small one makes the output cap clamp on a short input)
	let model = c.compat ? { ...MODELS[provider][id], compat: c.compat } : MODELS[provider][id];
	if (c.window) model = { ...model, contextWindow: c.window };
	const sse = readFileSync(join(dir, c.sse + ".sse"), "utf8");
	const messages = c.messages.map((m) => (m.$reply ? replies[m.$reply] : m));
	let body, headers;
	const fetch = async (url, init) => {
		body = init.body;
		headers = new Headers(init.headers);
		return new Response(sse, { status: 200, headers: { "content-type": "text/event-stream" } });
	};
	const context = normalizeContext({ systemPrompt: c.system, messages, tools: c.tools });
	const level = clampThinkingLevel(model, c.level);
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", fetch, reasoning: level === "off" ? undefined : level,
		...(c.temperature !== undefined && { temperature: c.temperature }), ...(c.toolChoice && { toolChoice: c.toolChoice }),
		...(c.thinkingBudgets && { thinkingBudgets: c.thinkingBudgets }) });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	replies[c.name] = final;
	writeFileSync(join(dir, c.name + ".body"), body);
	writeFileSync(join(dir, c.name + ".digest"), digest(final));
	const beta = headers.get("anthropic-beta");
	if (beta) writeFileSync(join(dir, c.name + ".headers"), "anthropic-beta: " + beta + "\n");
	else rmSync(join(dir, c.name + ".headers"), { force: true });
}
console.log("[OK] tests/pi/anthropic written, " + cases.length + " cases");
