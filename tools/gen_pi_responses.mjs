// Goldens for llm/be_openai_responses.c, made by pi's own code: pi-ai's openai-responses provider
// (api/openai-responses.js streamSimple) run on each case of tests/pi/responses/cases.json with a fake fetch that
// answers with the case's .sse file. Writes, per case, next to cases.json:
//   <name>.body     the request body pi sent (the exact text)
//   <name>.headers  the session headers pi sent (session_id, x-client-request-id), when it sent any
//   <name>.digest   what pi made of the stream (stop, error, usage, blocks), the form tests/test_pi_llm.c rebuilds
// A message "$reply" in a case's messages is replaced by the assistant message pi made for the named case.
// A case's "env" (name -> value) is set in process.env while pi runs it, its "baseUrl" replaces the model's (azure models
// ship without one).
//   node tools/gen_pi_responses.mjs [azure]   (PI_NPM: the pi install, default the pinned amiga-pi/vendor/pi-1.1.0)
// With "azure" the cases of tests/pi/azure run through api/azure-openai-responses.js; each also gets
//   <name>.req      the request line target ("POST <url>") and the api-key header pi's AzureOpenAI client sent
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const azure = process.argv[2] === "azure";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p) => import(pathToFileURL(join(dist, p)).href);
const { MODELS } = await imp("models.generated.js");
const { streamSimple } = await imp(azure ? "api/azure-openai-responses.js" : "api/openai-responses.js");
const { normalizeContext } = await imp("utils/transcript.js");
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", azure ? "azure" : "responses");
const { cases } = JSON.parse(readFileSync(join(dir, "cases.json"), "utf8"));
const replies = {};

function digest(m) {
	const j = (v) => JSON.stringify(v);
	let s = `stop=${m.stopReason}\nerror=${m.errorMessage ?? "-"}\n`;
	s += `usage=${m.usage.input},${m.usage.output},${m.usage.cacheRead},${m.usage.cacheWrite}\n`;
	s += `total=${m.usage.totalTokens}\n`;
	for (const b of m.content) {
		if (b.type === "thinking") s += `thinking=${j(b.thinking)} sig=${b.thinkingSignature === undefined ? "-" : j(b.thinkingSignature)}\n`;
		else if (b.type === "text") s += `text=${j(b.text)} sig=${b.textSignature === undefined ? "-" : j(b.textSignature)}\n`;
		else if (b.type === "toolCall") s += `tool=${b.id} ${b.name} ${j(b.arguments)}\n`;
	}
	return s;
}

for (const c of cases) {
	const [provider, id] = c.model.split("/");
	// "window" in a case replaces the model's context window (a small one makes the output cap clamp on a short input)
	let model = c.window ? { ...MODELS[provider][id], contextWindow: c.window } : MODELS[provider][id];
	if (c.baseUrl !== undefined) model = { ...model, baseUrl: c.baseUrl };
	for (const [k, v] of Object.entries(c.env ?? {})) process.env[k] = v;
	const sse = readFileSync(join(dir, c.sse + ".sse"), "utf8");
	const messages = c.messages.map((m) => (m.$reply ? replies[m.$reply] : m));
	let body, headers, reqUrl;
	const fetch = async (url, init) => {
		reqUrl = String(url);
		body = init.body;
		headers = new Headers(init.headers);
		return new Response(sse, { status: 200, headers: { "content-type": "text/event-stream" } });
	};
	const context = normalizeContext({ systemPrompt: c.system, messages, tools: c.tools });
	const s = streamSimple(model, context, { apiKey: "sk-test-not-real", fetch, reasoning: c.level === "off" ? undefined : c.level, sessionId: c.sessionId });
	let final;
	for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
	for (const k of Object.keys(c.env ?? {})) delete process.env[k];
	replies[c.name] = final;
	writeFileSync(join(dir, c.name + ".body"), body);
	writeFileSync(join(dir, c.name + ".digest"), digest(final));
	if (azure) writeFileSync(join(dir, c.name + ".req"), `POST ${reqUrl}\napi-key: ${headers.get("api-key")}\n`);
	const h = ["session_id", "x-client-request-id"].filter((k) => headers.get(k)).map((k) => `${k}: ${headers.get(k)}\n`).join("");
	if (h) writeFileSync(join(dir, c.name + ".headers"), h);
}
console.log("[OK] tests/pi/" + (azure ? "azure" : "responses") + " written, " + cases.length + " cases");
