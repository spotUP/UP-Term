/* radius -- the Radius gateway's catalog (amiga-pi L3; pi 1.1.0 ai/src/providers/radius.ts, radius-config.ts).
 *
 * GET <gateway>/v1/config answers {baseUrl, models[]}; the models of the account replace the provider's models (organization
 * owners can disable models, so a catalog that is known replaces the shipped one, an empty one leaves none). The catalog is kept
 * as Model objects in models-store.json ({"<providerId>":{"models":[...],"checkedAt":<ms>}}), pi's own file, so a start with no
 * network restores it. The file and the network are the app's (amiga-pi/radius.c); this is the pure part.
 * Portable C89, host-tested (amiga-pi tests/test_pi_radius.c). */
#ifndef LLM_RADIUS_H
#define LLM_RADIUS_H

#include "registry.h"

#define LLM_RADIUS_GATEWAY "https://radius.pi.dev"

/* normalizeRadiusGatewayUrl: a scheme is added ("radius.example" -> "https://radius.example"), trailing slashes dropped; with
 * strip_v1 also a trailing "/v1" (configureRadiusProviders: baseUrl.replace(/\/v1\/?$/u, "")). into out (may be value) */
void llm_radius_gateway(const char *value, int strip_v1, char *out, long cap);

/* sanitizeRadiusGatewayConfig + getRadiusModelsFromConfig: a gateway config {baseUrl, models[]} to the provider's Model objects,
 * a JSON array in *models ({...model, "api":"pi-messages","provider":id,"baseUrl":config.baseUrl}); models that lack a required
 * field (id, name: strings; reasoning: boolean; input: array; cost: object; contextWindow, maxTokens: numbers) are left out.
 * 0, -1 when the text is no config (no baseUrl string or no models array) */
int llm_radius_models(const char *provider, const char *config, long n, jw *models);

/* the provider's models replaced by a JSON array of Model objects (the form llm_radius_models writes and models-store.json keeps;
 * entries of another provider are ignored). 0, or -1 with the words in err; the registry is unchanged then */
int llm_radius_apply(llm_reg *r, const char *provider, const char *models, long n, char *err, long cap);

/* ---- OpenRouter's per-key catalog (pi 1.2.0 ai/src/providers/openrouter.ts personalizeOpenRouterCatalog, openrouter-api.ts) ----
 * With a credential, GET <baseUrl>/models/user lists the chat models the key may use with their limits and prices; the shipped
 * catalog is narrowed to them (401/403/404 keep it), the result is kept in models-store.json under "openrouter" and restored with
 * no network. The answer is read as a stream and only the entries of the shipped models are kept, whatever its size. The entries
 * are llm_or_entry; the registry takes them with llm_or_apply. Tiers (prompt-length prices) are not kept: the registry has one price. */
#define LLM_OPENROUTER "openrouter"
#define LLM_OPENROUTER_BASE "https://openrouter.ai/api/v1"
#define LLM_OR_MAX 48

typedef struct llm_or_entry {
    char id[96];
    long ctx, maxtok;           /* 0: the shipped value stays (`||`) */
    int has_cost;               /* pricing present: cost replaces the shipped price */
    long cost[4];               /* micro-dollars per million tokens: input, output, cacheRead, cacheWrite */
} llm_or_entry;

/* openRouterCost's perMillion for one $/token decimal string, in micro-dollars per million tokens; missing, invalid and negative: fallback */
long llm_or_price(const char *s, long fallback);

/* the shipped OpenRouter models laid over the registry again (values and models as shipped), before a fresh answer narrows them */
int llm_or_reset(llm_reg *r);

typedef struct llm_or_user llm_or_user;
llm_or_user *llm_or_user_new(void);
/* a piece of the answer's body: 0, -1 out of memory */
int llm_or_user_feed(llm_or_user *p, const char *s, long n);
/* the body ended: 0 when it was {"data":[...]}, -1 when it was not (pi: "Invalid OpenRouter model list response") */
int llm_or_user_end(llm_or_user *p);
int llm_or_user_count(const llm_or_user *p);
const llm_or_entry *llm_or_user_get(const llm_or_user *p, int i);
void llm_or_user_free(llm_or_user *p);

/* the registry's shipped OpenRouter models narrowed to the entries (by id), their limits and prices replaced where the entry has them;
 * models the registry holds from models.json (not shipped) are untouched. 0 */
int llm_or_apply(llm_reg *r, const llm_or_entry *e, int n);

/* the registry's shipped OpenRouter models as the Model objects models-store.json keeps ([{id,name,api,provider,baseUrl,reasoning,input,
 * cost,contextWindow,maxTokens}]) */
void llm_or_store(const llm_reg *r, jw *models);
/* the entries of a stored array (Model objects of the provider): how many, at most max */
int llm_or_stored(const char *models, long n, llm_or_entry *out, int max);

#endif
