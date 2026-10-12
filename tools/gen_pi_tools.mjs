// pi's real tool declarations (read, bash, edit, write: pi-coding-agent dist/core/tools/*.js, through pi-ai's
// toToolDeclaration, constrainedSampling included) as test input.
//   node tools/gen_pi_tools.mjs
// Writes tests/pi/pi_tools.json and puts the real declaration in place of every tool of that name in the cases of
// tests/pi/{anthropic,responses,completions}/cases.json (a tool of another name stays as written). Then run the
// gen_pi_*.mjs generators. Goldens are made from these, never from plain tools.
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
const npm = process.env.PI_NPM || "/opt/homebrew/lib/node_modules/@earendil-works/pi-coding-agent";
const imp = (p) => import(pathToFileURL(join(npm, p)).href);
const { toToolDeclaration } = await imp("node_modules/@earendil-works/pi-ai/dist/utils/transcript.js");
const real = {};
for (const n of ["read", "bash", "edit", "write"]) {
	const m = await imp(`dist/core/tools/${n}.js`);
	const f = Object.entries(m).find(([k]) => /^create.*Tool$/.test(k) && !/Definition/.test(k))[1];
	real[n] = toToolDeclaration(f("/tmp"));
}
const root = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi");
writeFileSync(join(root, "pi_tools.json"), JSON.stringify(Object.values(real), null, 1) + "\n");
for (const d of ["anthropic", "responses", "completions"]) {
	const f = join(root, d, "cases.json");
	if (!existsSync(f)) continue;
	const j = JSON.parse(readFileSync(f, "utf8"));
	for (const c of j.cases) if (Array.isArray(c.tools)) c.tools = c.tools.map((t) => real[t.name] ?? t);
	writeFileSync(f, JSON.stringify(j, null, 1) + "\n");
}
console.log("[OK] tests/pi/pi_tools.json and the cases' tools written");
