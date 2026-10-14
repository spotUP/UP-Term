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

#endif
