// Dump the models of pi-ai's catalog that llm/models_builtin.json carries, as JSON on stdout.
//   node tools/dump_pi_models.mjs > tools/pi_models_dump.json
// pi = npm @earendil-works/pi-coding-agent (PI_NPM, default the Homebrew install). The dump is committed: the host
// tests and `python3 tools/gen_models_builtin.py --check` need no npm. tools/gen_models_builtin.py --from-pi turns it
// into llm/models_builtin.json.
import { pathToFileURL } from "node:url";
import { join } from "node:path";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
const ai = join(npm, "node_modules/@earendil-works/pi-ai/dist/models.generated.js");
const { MODELS } = await import(pathToFileURL(ai).href);
const pkg = JSON.parse((await import("node:fs")).readFileSync(join(npm, "package.json"), "utf8"));
const subset = {
	anthropic: ["claude-opus-5-5", "claude-opus-5", "claude-sonnet-5-5", "claude-haiku-4-5"],
	openai: ["gpt-5.5", "gpt-4.1"],
};
const out = { pi: pkg.name + "@" + pkg.version, providers: {} };
for (const [p, ids] of Object.entries(subset)) {
	out.providers[p] = ids.map((id) => {
		const m = MODELS[p][id];
		if (!m) throw new Error("pi has no " + p + "/" + id);
		return m;
	});
}
process.stdout.write(JSON.stringify(out, null, 1) + "\n");
