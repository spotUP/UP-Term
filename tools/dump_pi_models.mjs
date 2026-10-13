// Dump the models of pi-ai's catalog that llm/models_builtin.json carries, as JSON on stdout.
//   node tools/dump_pi_models.mjs > tools/pi_models_dump.json
// pi = npm @earendil-works/pi-coding-agent (PI_NPM, default the Homebrew install). The dump is committed: the host
// tests and `python3 tools/gen_models_builtin.py --check` need no npm. tools/gen_models_builtin.py --from-pi turns it
// into llm/models_builtin.json.
import { pathToFileURL } from "node:url";
import { join } from "node:path";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
// pi-ai sits inside the package's node_modules, or beside it when npm hoisted it (the vendor/pi-1.1.0 oracle install)
import { existsSync } from "node:fs";
const nested = join(npm, "node_modules/@earendil-works/pi-ai/dist/models.generated.js");
const ai = existsSync(nested) ? nested : join(npm, "../pi-ai/dist/models.generated.js");
const { MODELS } = await import(pathToFileURL(ai).href);
const pkg = JSON.parse((await import("node:fs")).readFileSync(join(npm, "package.json"), "utf8"));
const subset = {
	anthropic: ["claude-opus-5-5", "claude-opus-5", "claude-sonnet-5-5", "claude-haiku-5-5", "claude-haiku-4-5"],
	openai: ["gpt-5.5", "gpt-4.1", "gpt-4"],
	google: ["gemini-3-flash-preview", "gemini-3.1-pro-preview", "gemini-2.5-flash", "gemini-2.5-pro"],
	azure: ["gpt-5.5", "gpt-4.1", "gpt-4"],
};
// the subscription providers (amiga-pi phase O, /login): every model pi offers for them, so what a login unlocks is what pi shows
for (const p of ["openai-codex", "github-copilot", "kimi-coding", "xai", "meta"]) subset[p] = Object.keys(MODELS[p]);
const out = { pi: pkg.name + "@" + pkg.version, providers: {} };
for (const [p, ids] of Object.entries(subset)) {
	out.providers[p] = ids.map((id) => {
		const m = MODELS[p][id];
		if (!m) throw new Error("pi has no " + p + "/" + id);
		return m;
	});
}
process.stdout.write(JSON.stringify(out, null, 1) + "\n");
