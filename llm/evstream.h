/* evstream -- the AWS event-stream binary framing (Phase M4): the body of an Amazon Bedrock converse-stream answer
 * (Content-Type application/vnd.amazon.eventstream), as @smithy/core event-streams/eventstream-codec decodes it.
 *
 * A message is  total length (4, big endian) | headers length (4) | CRC32 of those 8 bytes (4) | headers | payload |
 * CRC32 of everything before it (4). A header is  name length (1) | name | type (1) | value, the types 0 true, 1 false,
 * 2 byte, 3 short, 4 integer, 5 long, 6 byte array (2 byte length), 7 string (2 byte length), 8 timestamp (8), 9 uuid (16).
 * The decoder takes the answer's bytes in pieces of any size (a message may be split anywhere, also inside the prelude)
 * and calls back for each complete, checked message. The prelude is checked as soon as its 12 bytes are in, so a
 * corrupted length never makes it wait for (or allocate) bytes that are not coming. Failures carry the codec's own words
 * (splitMessage.js): the prelude checksum, the message checksum, a length that does not hold the overhead.
 * Portable C89, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_EVSTREAM_H
#define LLM_EVSTREAM_H

/* the largest message taken (the limit of the format is 16 MB; a Bedrock event is far below this) */
#define LLM_EVS_MAX 1048576L

/* what the pieces of a message say: the string headers Bedrock sets (":message-type" "event" or "exception",
 * ":event-type", ":exception-type", ":content-type"; "" when absent) and the payload */
typedef struct llm_evmsg {
    const char *message_type, *event_type, *exception_type, *content_type;
    const unsigned char *payload;
    long n;
} llm_evmsg;

typedef struct llm_evs {
    void (*fn)(void *u, const llm_evmsg *m);
    void *u;
    unsigned char pre[12];
    unsigned char *msg;             /* the message being gathered (after the prelude is checked) */
    long cap, have, total, hlen;
    int state;                      /* 0 prelude, 1 rest of the message, -1 failed */
    long messages;                  /* complete messages so far */
    char err[160];                  /* why it failed */
} llm_evs;

void llm_evs_init(llm_evs *s, void (*fn)(void *u, const llm_evmsg *m), void *u);
/* n bytes of the stream: 0, or -1 when the framing is corrupt (s->err says how; the decoder stays failed) */
int llm_evs_feed(llm_evs *s, const void *p, long n);
/* the stream ended: 0 when it ended between messages, -1 when one is cut short (s->err) */
int llm_evs_end(llm_evs *s);
void llm_evs_free(llm_evs *s);

/* the CRC-32 (IEEE 802.3, as the codec's Crc32) of n bytes, continued from crc (0 to start) */
unsigned long llm_crc32(unsigned long crc, const void *p, long n);
/* a message with the string headers given (type, value pairs of ":name", value; nh of them) and a payload, written to
 * out: its length, -1 when cap is too small (the tests' and the fixture's encoder) */
long llm_evs_encode(const char *const *hdr, int nh, const void *payload, long n, unsigned char *out, long cap);

#endif
