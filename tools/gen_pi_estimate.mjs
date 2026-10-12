// Goldens for llm/estimate.c, made by pi's own code: pi-ai utils/estimate.js estimateContextTokens over each case of
// tests/pi/estimate/cases.json ({name, messages}); the result is written back as "tokens" into the same file.
//   node tools/gen_pi_estimate.mjs        (PI_NPM: the pi install, default the Homebrew one)
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
const { estimateContextTokens } = await import(pathToFileURL(join(npm, "node_modules/@earendil-works/pi-ai/dist/utils/estimate.js")).href);
const f = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "estimate", "cases.json");
const j = JSON.parse(readFileSync(f, "utf8"));
for (const c of j.cases) c.tokens = estimateContextTokens(c.messages).tokens;
writeFileSync(f, JSON.stringify(j, null, 1) + "\n");
console.log("[OK] tests/pi/estimate/cases.json: " + j.cases.map((c) => c.name + "=" + c.tokens).join(" "));
