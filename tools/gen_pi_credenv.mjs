// Goldens for llm/creds.c's credential env layer, made by pi's own resolveConfigValue (coding-agent core/resolve-config-value.js):
// an api_key credential in auth.json carries an optional `env` object whose non-empty values win over the process environment
// for that provider (auth/helpers.ts, providers/anthropic.ts:31 `env: credential.env`; getProviderEnvValue: env?.[name] || process.env[name]).
// Each case: the stored key (a config value), the credential's env, the process env; `want` is what pi resolves the key to
// (null: unresolved). Writes tests/pi/credenv/cases.json.
//   PI_NPM=<dir holding node_modules/@earendil-works/pi-coding-agent> node tools/gen_pi_credenv.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
import { writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const { resolveConfigValue } = await import(pathToFileURL(join(npm, "node_modules/@earendil-works/pi-coding-agent/dist/core/resolve-config-value.js")).href);
const cases = [
	{ name: "literal", key: "sk-literal", credEnv: { A: "cred" }, procEnv: { A: "proc" } },
	{ name: "cred_wins", key: "$A", credEnv: { A: "cred" }, procEnv: { A: "proc" } },
	{ name: "proc_when_no_cred_env", key: "$A", procEnv: { A: "proc" } },
	{ name: "proc_when_cred_value_empty", key: "$A", credEnv: { A: "" }, procEnv: { A: "proc" } },
	{ name: "per_name_merge", key: "${A}-${B}", credEnv: { A: "cred" }, procEnv: { A: "proc", B: "procB" } },
	{ name: "cred_only", key: "pre-$A-post", credEnv: { A: "cred" }, procEnv: {} },
	{ name: "unresolved", key: "$A", credEnv: { B: "cred" }, procEnv: {} },
	{ name: "workspace_id", key: "$ANTHROPIC_WORKSPACE_ID", credEnv: { ANTHROPIC_WORKSPACE_ID: "wrkspc_cred" }, procEnv: { ANTHROPIC_WORKSPACE_ID: "wrkspc_proc" } },
];
for (const c of cases) {
	const saved = { ...process.env };
	for (const n of ["A", "B", "ANTHROPIC_WORKSPACE_ID"]) delete process.env[n];
	Object.assign(process.env, c.procEnv);
	c.want = resolveConfigValue(c.key, c.credEnv) ?? null;
	for (const n of ["A", "B", "ANTHROPIC_WORKSPACE_ID"]) delete process.env[n];
	Object.assign(process.env, saved);
}
writeFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "credenv", "cases.json"), JSON.stringify({ cases }, null, 1) + "\n");
console.log("[OK] tests/pi/credenv written, " + cases.length + " cases");
