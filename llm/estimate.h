/* estimate -- the context estimate and the output cap derived from it (amiga-pi plan PX2), after pi-ai
 * utils/estimate.js (estimateContextTokens) and api/simple-options.js (clampMaxTokensToContext).
 *
 * pi-ai's buildBaseOptions caps every request's output at
 * min(maxTokens, max(1, contextWindow - estimate(messages) - 4096)); the estimate is the last good answer's
 * reported usage plus 3.5 characters per token for what follows it (images 4800 characters). The messages are
 * the transcript before any provider transform: system messages included (their text, sections and tool
 * declarations), an answer that ended in error or abort included.
 * Portable C89 over claude/json.h, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_ESTIMATE_H
#define LLM_ESTIMATE_H

#include "backend.h"

#define LLM_CONTEXT_SAFETY 4096L

/* estimateContextTokens(messages).tokens over pi-shaped message JSON texts */
long llm_estimate_context(const char *const *msgs, const long *n, int count);

/* clampMaxTokensToContext: maxTokens limited by what the model's context window leaves after est tokens */
long llm_clamp_max_tokens(const llm_model *m, long est, long max_tokens);

/* the cap a request carries: the caller's max_tokens (else the model's) clamped by c->est_tokens; 0 when
 * neither is set (the member is left out) */
long llm_ctx_max_tokens(const llm_ctx *c);

/* simple-options.ts DEFAULT_THINKING_BUDGETS: the budget of thinking level (xhigh and max count as high) */
long llm_think_budget_of(int level);
/* a budget-based thinking request (pi's anthropic and bedrock streamSimple): base is the cap buildBaseOptions made
 * (already clamped to the context window); the cap the request carries after adjustMaxTokensForThinking and a second
 * clampMaxTokensToContext in *maxt, the thinking budget min(budget, maxt - 1024) in *budget */
void llm_think_budget(const llm_model *m, long est, long base, int lv, long custom, long *maxt, long *budget);

#endif
