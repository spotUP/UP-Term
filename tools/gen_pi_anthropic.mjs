// Goldens for llm/be_anthropic_pi.c, made by pi's own code: pi-ai's anthropic-messages provider
// (api/anthropic-messages.js streamSimple) run on each case of tests/pi/anthropic/cases.json with a fake fetch that
// answers with the case's .sse file. Writes, per case, next to cases.json:
//   <name>.body     the request body pi sent (the exact text; the SDK moves `betas` into the header)
//   <name>.headers  the anthropic-beta header pi sent, when it sent one
// A message {"$reply": "<case>"} in a case's messages is replaced by the assistant message pi made for that case. Unaltered models: the compat
// flags (supportsMidConvo*) are pi's own.
//   node tools/gen_pi_anthropic.mjs        (PI_NPM: the pi install, default the Homebrew one)
import { readFileSync, writeFileSync, rmSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p) => import(pathToFileURL(join(dist, p)).href);
const { MODELS } = await imp("models.generated.js");
const { streamSimple } = await imp("api/anthropic-messages.js");
const { normalizeContext } = await imp("utils/transcript.js");
const { clampThinkingLevel } = await imp("models.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "anthropic");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const replies = {};
for (const c of cases) {
	const [provider, id] = c.model.split("/");
	// "compat" in a case replaces the model's compat (a models.json model with only that flag)
	const model = c.compat ? { ...MODELS[provider][id], compat: c.compat } : MODELS[provider][id];
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
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", fetch, reasoning: level === "off" ? undefined : level });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	replies[c.name] = final;
	writeFileSync(join(dir, c.name + ".body"), body);
	const beta = headers.get("anthropic-beta");
	if (beta) writeFileSync(join(dir, c.name + ".headers"), "anthropic-beta: " + beta + "\n");
	else rmSync(join(dir, c.name + ".headers"), { force: true });
}
console.log("[OK] tests/pi/anthropic written, " + cases.length + " cases");
