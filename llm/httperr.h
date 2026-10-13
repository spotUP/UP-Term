/* httperr -- the text pi shows for a failed HTTP answer, per SDK (pi 1.1.0: the OpenAI SDK 7.19.0, the Anthropic SDK
 * 0.129.0, @google/genai 2.21.0 and pi-ai's utils/error-body.ts on top). Portable C89, host-tested
 * (tests/test_pi_llm.c, goldens from pi: tests/pi/httperr, tools/gen_pi_httperr.mjs). */
#ifndef LLM_HTTPERR_H
#define LLM_HTTPERR_H

#include "backend.h"

/* openai-completions (no prefix), openai-responses ("OpenAI API error", else "<provider> API error") and
 * azure-openai-responses ("Azure OpenAI API error"): the OpenAI SDK's APIError message `<status> <message>`, with
 * pi's formatProviderError putting `<status>: <body>` or `<prefix> (<status>): ...` in front. openrouter_raw: also
 * append error.metadata.raw (openai-completions.ts:722-727) */
void llm_httperr_openai(const llm_http_fail *f, const char *prefix, int openrouter_raw, char *out, long cap);
/* anthropic-messages: the Anthropic SDK's APIError message, whole */
void llm_httperr_anthropic(const llm_http_fail *f, char *out, long cap);
/* google-generative-ai: @google/genai's throwErrorIfNotOK, JSON.stringify of the error body */
void llm_httperr_google(const llm_http_fail *f, char *out, long cap);

#endif
