// Goldens for llm/constrain.c, made by pi's own code: pi-ai api/constrained-sampling.js makeStrictJsonSchema (with
// Anthropic's isAnthropicStrictUnsupportedKeyword, taken from api/anthropic-messages.js) over each case of
// tests/pi/strict/cases.json ({name, kw: "none"|"anthropic", schema}); written back as "strict": the schema pi makes,
// or null when pi refuses it (UnsupportedStrictJsonSchemaError).
//   node tools/gen_pi_strict.mjs        (PI_NPM: the pi install, default the Homebrew one)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
const { makeStrictJsonSchema } = await import(pathToFileURL(join(dist, "api/constrained-sampling.js")).href);
const src = readFileSync(join(dist, "api/anthropic-messages.js"), "utf8");
const a = src.indexOf("const ANTHROPIC_STRICT_UNSUPPORTED_KEYWORDS");
const b = src.indexOf("function convertTools(");
const antKw = new Function(src.slice(a, b) + "\nreturn isAnthropicStrictUnsupportedKeyword;")();
const f = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "strict", "cases.json");
const j = JSON.parse(readFileSync(f, "utf8"));
for (const c of j.cases) {
	try { c.strict = makeStrictJsonSchema(c.schema, c.kw === "anthropic" ? antKw : undefined); } catch (e) { if (e.constructor.name !== "UnsupportedStrictJsonSchemaError") throw e; c.strict = null; }
}
writeFileSync(f, JSON.stringify(j, null, 1) + "\n");
console.log("[OK] tests/pi/strict/cases.json: " + j.cases.map((c) => c.name + (c.strict ? "" : "=refused")).join(" "));
