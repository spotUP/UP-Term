// Shared by gen_pi_bedrock.mjs and gen_pi_handoff.mjs: runs pi 1.1.0's bedrock-converse-stream provider (api/
// bedrock-converse-stream.js on @aws-sdk/client-bedrock-runtime 3.1127.0) against a local HTTP server that records the
// request the SDK sent and answers with a recorded event stream. The SDK takes no fake fetch: AWS_BEDROCK_FORCE_HTTP1=1
// makes pi use NodeHttpHandler (node:https), and install() redirects every request node:http / node:https makes to the
// local server after noting where it was going, so the model keeps its real base URL (the host the SDK signs is the real
// one, and the endpoint and region rules of pi and the SDK run as they do for a user). randomUUID is fixed, so the
// SDK's amz-sdk-invocation-id (a signed header) is the same on every run.
//   spec of a stream: an array of {event, payload} (a Bedrock event: :message-type event, :event-type <event>) and
//   {exception, payload} (:message-type exception, :exception-type <exception>), encoded by the SDK's own
//   EventStreamCodec, so the framing the C decoder reads was not written by this repository's encoder.
import { createRequire } from "node:module";
import http, { createServer } from "node:http";
import https from "node:https";
import { syncBuiltinESMExports } from "node:module";
import { join } from "node:path";
import { pathToFileURL } from "node:url";

export const PORT = Number(process.env.BEDROCK_ORACLE_PORT || 38417);

export function load(npm) {
	const dist = join(npm, "node_modules/@earendil-works/pi-ai/dist");
	const imp = (p, q = "") => import(pathToFileURL(join(dist, p)).href + q);
	const req = createRequire(join(npm, "node_modules/@aws-sdk/client-bedrock-runtime/"));
	const { EventStreamCodec } = req("@smithy/core/event-streams");
	const enc = new TextEncoder();
	const dec = new TextDecoder();
	const codec = new EventStreamCodec((b) => dec.decode(b), (s) => enc.encode(s));
	return { imp, codec, enc };
}

export function frames(codec, enc, events) {
	const out = [];
	for (const e of events) {
		const headers = { ":content-type": { type: "string", value: "application/json" } };
		if (e.event) {
			headers[":event-type"] = { type: "string", value: e.event };
			headers[":message-type"] = { type: "string", value: "event" };
		} else {
			headers[":exception-type"] = { type: "string", value: e.exception };
			headers[":message-type"] = { type: "string", value: "exception" };
		}
		out.push(Buffer.from(codec.encode({ headers, body: enc.encode(JSON.stringify(e.payload)) })));
	}
	return Buffer.concat(out);
}

// Redirects node:http and node:https requests to the local server, remembering the original target. Call before the first
// import of pi's bedrock adapter (the SDK takes node:https's request at import).
export function install() {
	const state = { last: null, reply: null, target: null };
	Object.defineProperty(globalThis.crypto, "randomUUID", { value: () => "00000000-0000-4000-8000-000000000001", configurable: true, writable: true });
	const realRequest = http.request;
	const redirect = () => (opts, cb) => {
		if (typeof opts === "string" || opts instanceof URL) throw new Error("oracle: URL form not expected");
		state.target = { protocol: opts.protocol, hostname: opts.hostname || opts.host, port: opts.port, path: opts.path, method: opts.method };
		const o = { ...opts, protocol: "http:", hostname: "127.0.0.1", host: "127.0.0.1", port: PORT, agent: undefined };
		delete o.servername;
		return realRequest(o, cb);
	};
	https.request = redirect();
	http.request = redirect();
	syncBuiltinESMExports();
	return state;
}

// a server that records the last request and answers with state.reply = {status, headers, body}
export async function server(state) {
	const srv = createServer((req, res) => {
		const chunks = [];
		req.on("data", (c) => chunks.push(c));
		req.on("end", () => {
			state.last = { method: req.method, url: req.url, headers: req.headers, body: Buffer.concat(chunks).toString("utf8"), target: state.target };
			const r = state.reply;
			res.sendDate = false; // a Date header would make the SDK correct its clock (systemClockOffset) and sign with the real time
			res.writeHead(r.status, r.headers);
			res.end(r.body);
		});
	});
	await new Promise((ok, bad) => srv.once("error", bad).listen(PORT, "127.0.0.1", ok));
	return { close: () => new Promise((ok) => srv.close(ok)) };
}

export function clearAws() {
	for (const k of Object.keys(process.env)) if (k.startsWith("AWS_")) delete process.env[k];
}
