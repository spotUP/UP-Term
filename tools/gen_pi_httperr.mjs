// Goldens for llm/httperr.c, made by pi's own code: the text pi shows for a failed HTTP answer, per API. Each case is one
// answer (status, reason phrase, content type, body) given to pi-ai's provider for the API (a fake fetch; for google
// globalThis.fetch) and the errorMessage pi made of it. Writes tests/pi/httperr/cases.json:
//   {cases: [{name, api, provider, model, status, reason, ctype, body, want}]}
// tests/test_pi_llm.c (test_httperr) builds the HTTP answer from the fields, runs llm_request on it and compares.
//   node tools/gen_pi_httperr.mjs   (PI_NPM: the pi install, default the pinned amiga-pi/vendor/pi-1.1.0)
import { writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const imp = (p) => import(pathToFileURL(join(dist, p)).href);
const { MODELS } = await imp("models.generated.js");
const { normalizeContext } = await imp("utils/transcript.js");
const out = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "httperr", "cases.json");

const apis = {
	"anthropic-messages": { file: "api/anthropic-messages.js", model: MODELS.anthropic["claude-haiku-4-5"], provider: "anthropic" },
	"openai-completions": { file: "api/openai-completions.js", model: { ...MODELS.openai["gpt-5.5"], api: "openai-completions", provider: "lan", baseUrl: "http://192.168.1.10:11434/v1" }, provider: "lan" },
	"openai-responses": { file: "api/openai-responses.js", model: MODELS.openai["gpt-5.5"], provider: "openai" },
	"openai-responses/other": { file: "api/openai-responses.js", model: { ...MODELS.openai["gpt-5.5"], provider: "myprov" }, provider: "myprov", api: "openai-responses" },
	"azure-openai-responses": { file: "api/azure-openai-responses.js", model: { ...MODELS.azure["gpt-4.1"], baseUrl: "https://res.openai.azure.com/openai/v1" }, provider: "azure" },
	"google-generative-ai": { file: "api/google-generative-ai.js", model: MODELS.google["gemini-2.5-flash"], provider: "google" },
};
const J = "application/json";
const longMsg = "x".repeat(1200);
const hugeMsg = "y".repeat(4100);      // an OpenAI-family body over the 4000 characters pi keeps
const bodies = [
	["rate_limit", 429, "Too Many Requests", J, '{"error":{"message":"Rate limit reached for requests","type":"rate_limit_exceeded","param":null,"code":"rate_limit_exceeded"}}'],
	["error_string", 400, "Bad Request", J, '{"error":"bad thing"}'],
	["text_500", 500, "Internal Server Error", "text/plain", "upstream broke"],
	["html_502", 502, "Bad Gateway", "text/html", "<html>Bad Gateway</html>"],
	["empty_403", 403, "Forbidden", J, ""],
	["no_message", 401, "Unauthorized", J, '{"error":{"code":"invalid_api_key"}}'],
	["detail_404", 404, "Not Found", J, '{"detail":"nope"}'],
	["array", 400, "Bad Request", J, "[1,2]"],
	["empty_object", 400, "Bad Request", J, "{}"],
	["null", 502, "Bad Gateway", J, "null"],
	["number_message", 422, "Unprocessable Entity", J, '{"error":{"message":42}}'],
	["object_message", 400, "Bad Request", J, '{"error":{"message":{"a":1}}}'],
	["top_message", 403, "Forbidden", J, '{"message":"Missing Authentication Token"}'],
	["anthropic_overloaded", 529, "", J, '{"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}}'],
	["unicode", 400, "Bad Request", J, '{"error":{"message":"caf\\u00e9 \\u2603","detail":"\\u00e9\\n\\t\\"q\\""}}'],
	["long_message", 400, "Bad Request", J, JSON.stringify({ error: { message: "short", detail: longMsg } })],
	["huge_body", 400, "Bad Request", J, JSON.stringify({ error: { message: "short", detail: hugeMsg } })],
	["metadata_raw", 502, "Bad Gateway", J, '{"error":{"message":"Provider returned error","code":502,"metadata":{"raw":"upstream said no","provider_name":"X"}}}'],
	["google_error", 429, "Too Many Requests", J + "; charset=UTF-8", '{"error":{"code":429,"message":"Resource has been exhausted","status":"RESOURCE_EXHAUSTED"}}'],
	["google_text_429", 429, "Too Many Requests", "text/plain; charset=utf-8", "slow down"],
	["json_ctype_html", 502, "Bad Gateway", J, "<html>Bad Gateway</html>"],
	["json_ctype_short", 500, "Internal Server Error", J, "upstream broke"],
	["json_ctype_long", 500, "Internal Server Error", J, "<!doctype html><html><body>an error page that is long</body></html>"],
	["json_ctype_blank", 503, "Service Unavailable", J, "  \n"],
	["no_ctype", 500, "Internal Server Error", "", '{"error":{"message":"no content type"}}'],
];
const cases = [];
for (const [aname, a] of Object.entries(apis)) {
	const mod = await imp(a.file);
	for (const [bn, status, reason, ctype, body] of bodies) {
		const headers = ctype ? { "content-type": ctype } : {};
		const fetch = async () => new Response(body, { status, statusText: reason, headers });
		if (a.provider === "google") globalThis.fetch = fetch;
		if (a.provider === "azure") process.env.AZURE_OPENAI_BASE_URL = "https://res.openai.azure.com/openai/v1";
		const context = normalizeContext({ systemPrompt: "s", messages: [{ role: "user", content: [{ type: "text", text: "hello" }], timestamp: 1760100000000 }], tools: [] });
		const s = mod.streamSimple(a.model, context, { apiKey: "sk-test-not-real", fetch, maxRetries: 0 });
		let final;
		for await (const ev of s) if (ev.type === "done" || ev.type === "error") final = ev.type === "done" ? ev.message : ev.error;
		if (final.stopReason !== "error") throw new Error(aname + " " + bn + ": no error");
		cases.push({ name: aname.replace("/", "_") + "_" + bn, api: a.api ?? aname, provider: a.provider, model: a.model.id, status, reason, ctype, body, want: final.errorMessage });
	}
}
writeFileSync(out, JSON.stringify({ cases }, null, 1));
console.log("[OK] tests/pi/httperr written, " + cases.length + " cases");
