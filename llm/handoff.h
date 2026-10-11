/* handoff -- the messages one request carries when the model may not be the
 * one that wrote them (amiga-pi plan F3): pi-ai's transformMessages (pi
 * f10993bc packages/ai/src/api/transform-messages.ts), both passes.
 *
 * First pass, by whether the answering message was written by the same model
 * (provider, api and id all equal):
 *   - images in user and tool-result content become one text placeholder
 *     when the model takes no images;
 *   - thinking: redacted blocks and raw provider blocks only for the same
 *     model; a signed block for the same model kept; an empty one dropped;
 *     any other becomes a plain text block (no tags);
 *   - text of another model loses its signature; tool calls of another
 *     model lose their thoughtSignature and get the id this api accepts
 *     (anthropic-messages: [^a-zA-Z0-9_-] -> "_", 64 characters;
 *     openai-completions: "call|item" -> "call_item", at most 40, a hash
 *     when longer; provider openai cuts at 40; openai-responses: "call|item"
 *     with the item "fc_<hash of the item>" for another provider or api,
 *     else cleaned, always fc_-prefixed), and their tool results the same id;
 *   - a null content is an empty array.
 * Second pass: llm_transform (transform.h).
 * Portable C89, host-tested (amiga-pi tests/test_pi_model.c). */
#ifndef LLM_HANDOFF_H
#define LLM_HANDOFF_H

#include "backend.h"
#include "transform.h"

/* the messages for a request to model `to`: 0, -1 out of memory or a message that is not JSON */
int llm_prepare(const llm_model *to, const char *const *msgs, const long *n, int count, llm_ts ts, llm_view *v);

/* pi-ai's shortHash (utils/hash.ts), the tests read it */
void llm_short_hash(const char *s, char *out, long cap);

#endif
