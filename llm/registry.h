/* registry -- the models amiga-pi can talk to (plan F1, F2): pi's Model
 * descriptors (id, name, api, provider, baseUrl, input, cost, reasoning,
 * thinkingLevelMap, contextWindow, maxTokens, headers, compat) in one
 * llm_model each, owned by a registry.
 *
 * The bundled catalog is a models.json (llm/models_builtin.json, embedded as
 * llm/models_builtin.h by tools/gen_models_builtin.py); the user's
 * ENVARC:amiga-pi/models.json is laid over it by the same reader
 * (llm/models_json.c): a provider block changes baseUrl / api / apiKey /
 * headers / compat of its models, `models` adds or replaces one by id,
 * `modelOverrides` changes an existing one -- pi's provider-composer.ts.
 *
 * Thinking levels are pi's (models.ts getSupportedThinkingLevels and
 * clampThinkingLevel). Model patterns are pi's resolveCliModel: provider/id,
 * an exact id, then a partial id or name, with an optional ":level".
 * Portable C89, host-tested (amiga-pi tests/test_pi_model.c). */
#ifndef LLM_REGISTRY_H
#define LLM_REGISTRY_H

#include "backend.h"

typedef struct llm_reg {
    llm_model *m;               /* the models; their strings live in own[] */
    int n, cap;
    char **own;
    int nown, owncap;
    int oom;
    char err[256];              /* why the last overlay was refused, "" none */
} llm_reg;

/* a model with no value set (the compat flags LC_UNSET, as a model made by hand wants them) */
void llm_model_init(llm_model *m);

void llm_reg_init(llm_reg *r);
void llm_reg_free(llm_reg *r);
/* a copy of s[0..n) owned by the registry (terminated); 0 out of memory */
char *llm_reg_dup(llm_reg *r, const char *s, long n);

/* the bundled catalog: 0, -1 */
int llm_reg_builtin(llm_reg *r);
/* models.json text laid over the registry: 0, or -1 with r->err set (the registry is unchanged then) */
int llm_reg_overlay(llm_reg *r, const char *text, long n);

/* a provider's model by id (exact), 0 none */
const llm_model *llm_reg_find(const llm_reg *r, const char *provider, const char *id);
int llm_reg_has_provider(const llm_reg *r, const char *provider);

/* pi's resolveCliModel for "provider/pattern[:level]" or "pattern[:level]" (cli_provider: --provider, 0 none).
 * The model in *out (a copy; its strings stay valid as long as the registry), *level the ":level" (LT_*, -1 none);
 * 0, or -1 with err set (a message as pi words it). A known provider with an id the catalog lacks gets a clone of
 * its first model under that id (pi's buildFallbackModel). */
int llm_reg_resolve(llm_reg *r, const char *cli_provider, const char *pattern, llm_model *out, int *level,
                    char *err, long errcap);

/* ---- thinking levels (models.ts) ---- */
const char *llm_think_name(int level);          /* "off" ... "max" */
int llm_think_of(const char *name);             /* the reverse; -1 unknown */
/* the levels the model supports, in order, as a bit mask of 1 << LT_* */
unsigned llm_think_levels(const llm_model *m);
/* clampThinkingLevel: the level itself when supported, else the next higher, else the nearest lower */
int llm_think_clamp(const llm_model *m, int level);

#endif
