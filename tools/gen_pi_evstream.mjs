// Goldens for llm/evstream.c (Phase M4): AWS event-stream messages written by the SDK's own codec (@smithy/core
// event-streams EventStreamCodec, the one @aws-sdk/client-bedrock-runtime 3.1127.0 reads Bedrock answers with), and what
// that codec says of each: the headers and payload of every message, or the error it throws for a damaged one. Writes
// tests/pi/evstream/<name>.bin and cases.json (name, messages: [{message_type, event_type, exception_type, content_type,
// payload (utf8)}], error: the codec's message or null; ours: the start of the text llm_evs says instead, where it differs).
//   PI_NPM=<dir holding node_modules/@aws-sdk/client-bedrock-runtime> node tools/gen_pi_evstream.mjs   (the pinned amiga-pi/vendor/pi-1.1.0)
import { writeFileSync, mkdirSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { load } from "./pi_bedrock_oracle.mjs";
const npm = process.env.PI_NPM || join(dirname(fileURLToPath(import.meta.url)), "..", "..", "amiga-pi", "vendor", "pi-1.1.0");
const { codec, enc } = load(npm);
const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "tests", "pi", "evstream");
mkdirSync(dir, { recursive: true });
const S = (value) => ({ type: "string", value });
const ev = (type, payload) =>
	Buffer.from(codec.encode({ headers: { ":event-type": S(type), ":content-type": S("application/json"), ":message-type": S("event") }, body: enc.encode(JSON.stringify(payload)) }));
const ex = (type, payload) =>
	Buffer.from(codec.encode({ headers: { ":exception-type": S(type), ":content-type": S("application/json"), ":message-type": S("exception") }, body: enc.encode(JSON.stringify(payload)) }));
const odd = Buffer.from(codec.encode({
	headers: {
		"a-true": { type: "boolean", value: true }, "b-false": { type: "boolean", value: false }, "c-byte": { type: "byte", value: -5 },
		"d-short": { type: "short", value: 1234 }, "e-int": { type: "integer", value: 100000 }, "f-long": { type: "long", value: { bytes: new Uint8Array([0, 0, 0, 0, 0, 1, 0, 0]) } },
		"g-bin": { type: "binary", value: new Uint8Array([1, 2, 3]) }, ":event-type": S("odd"), "h-time": { type: "timestamp", value: { valueOf: () => 1760100000000 } },
		"i-uuid": { type: "uuid", value: "01234567-89ab-cdef-0123-456789abcdef" }, ":message-type": S("event"),
	},
	body: enc.encode('{"unicode":"é中"}'),
}));
const bigText = "x".repeat(70000);
const clean = {
	one: [ev("messageStart", { role: "assistant" })],
	stream: [ev("messageStart", { role: "assistant" }), ev("contentBlockDelta", { contentBlockIndex: 0, delta: { text: "Hello, é中" } }), ev("contentBlockStop", { contentBlockIndex: 0 }), ev("messageStop", { stopReason: "end_turn" }), ev("metadata", { usage: { inputTokens: 5, outputTokens: 2, totalTokens: 7 }, metrics: { latencyMs: 3 } })],
	exception: [ev("messageStart", { role: "assistant" }), ex("throttlingException", { message: "Too many requests" })],
	headers_all_types: [odd],
	empty_payload: [Buffer.from(codec.encode({ headers: { ":event-type": S("ping"), ":message-type": S("event") }, body: new Uint8Array(0) }))],
	big: [ev("contentBlockDelta", { contentBlockIndex: 0, delta: { text: bigText } })],
};
const flip = (buf, at) => { const b = Buffer.from(buf); b[at] ^= 0x40; return b; };
const m1 = ev("messageStart", { role: "assistant" });
const m2 = ev("contentBlockDelta", { contentBlockIndex: 0, delta: { text: "second" } });
const bad = {
	prelude_length: [m1, flip(m2, 1)],            // the total length byte: the prelude checksum fails
	prelude_crc: [m1, flip(m2, 9)],               // a byte of the prelude's own checksum
	message_payload: [m1, flip(m2, m2.length - 12)],
	message_crc: [m1, flip(m2, m2.length - 1)],
	header_bytes: [m1, flip(m2, 14)],
	cut: [m1, m2.subarray(0, m2.length - 5)],
};
function decodeAll(buf) {
	const msgs = [];
	let off = 0;
	let error = null;
	while (off < buf.length) {
		try {
			if (buf.length - off < 4) throw new Error("Provided message too short to accommodate event stream message overhead");
			const len = buf.readUInt32BE(off);
			const piece = buf.subarray(off, off + len);
			if (piece.length < len) throw new Error("Reported message length does not match received message length");
			const m = codec.decode(piece);
			const h = (n) => (m.headers[n] ? String(m.headers[n].value) : "");
			msgs.push({ message_type: h(":message-type"), event_type: h(":event-type"), exception_type: h(":exception-type"), content_type: h(":content-type"), payload: Buffer.from(m.body).toString("utf8") });
			off += len;
		} catch (e) {
			error = e.message;
			break;
		}
	}
	return { messages: msgs, error };
}
const cases = [];
for (const [name, frames] of Object.entries({ ...clean, ...bad })) {
	const buf = Buffer.concat(frames);
	writeFileSync(join(dir, name + ".bin"), buf);
	const d = decodeAll(buf);
	// a flipped length byte: the SDK reads the stream by the length and fails when the bytes do not add up; the C decoder
	// checks the prelude's own checksum as soon as the 12 bytes are in, so it says so before waiting for the rest
	// a message cut short: the SDK's stream reader (getChunkedStream.js) says so before the codec sees anything
	if (name === "cut") d.error = "Truncated event message received.";
	cases.push({ name, messages: d.messages, error: d.error, ...(name === "prelude_length" ? { ours: "The prelude checksum specified in the message (" } : {}) });
}
writeFileSync(join(dir, "cases.json"), JSON.stringify({ cases }, null, 1) + "\n");
console.log("[OK] tests/pi/evstream written, " + cases.length + " cases");
