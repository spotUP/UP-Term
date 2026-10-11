/* creds -- config values and the credential order (plan F2).
 *
 * A config value (models.json apiKey, header values, auth.json key) is, as
 * in pi's resolve-config-value.ts: `!command` (the rest is run, its trimmed
 * stdout is the value; run ONCE per process per command text, the result
 * cached), or a template where `$NAME` / `${NAME}` is an environment
 * variable (the value is unresolved when one is missing), `$$` is a "$" and
 * `$!` a "!"; anything else is a literal. The LLM layer runs no AmigaDOS
 * command and reads no variable itself: the agent supplies both through
 * llm_cfg.
 *
 * The credential for a model's provider, in pi's order (docs/models.md):
 * the runtime --api-key, a stored auth.json api_key, the models.json apiKey,
 * the provider's environment variable <PROVIDER>_API_KEY.
 * Portable C89, host-tested (amiga-pi tests/test_pi_model.c). */
#ifndef LLM_CREDS_H
#define LLM_CREDS_H

#include "backend.h"

#define LLM_CMDS 16

typedef struct llm_cfg {
    void *u;
    /* an environment variable: its length, -1 not set (or empty) */
    long (*getenv)(void *u, const char *name, char *out, long cap);
    /* run a command, its stdout (trimmed, up to cap-1 bytes) into out: its length, -1 failed or empty */
    long (*run)(void *u, const char *cmd, char *out, long cap);
    /* the cache of command results */
    struct { char *cmd; char *val; } cache[LLM_CMDS];
    int ncache;
    long runs;                  /* commands run (the tests' count) */
    /* the runtime --api-key and the provider it was given for ("" any) */
    char runtime_key[512], runtime_provider[48];
    /* auth.json text (pi: {"provider":{"type":"api_key","key":"..."}}), 0 none; owned by the caller */
    const char *auth;
    long auth_n;
} llm_cfg;

void llm_cfg_init(llm_cfg *c);
void llm_cfg_free(llm_cfg *c);
/* a config value resolved into out: its length, -1 unresolved */
long llm_cfg_value(llm_cfg *c, const char *cfg, char *out, long cap);
/* the model's credential: the length, 0 none; *src names where it came from ("--api-key", "auth.json",
 * "models.json", "environment", 0 none) */
long llm_cfg_key(llm_cfg *c, const llm_model *m, char *out, long cap, const char **src);
/* "OPENAI_API_KEY" for provider "openai" */
void llm_key_env_name(const char *provider, char *out, long cap);
/* the model's extra request headers (models.json headers, resolved; authHeader's Authorization) as lines
 * "Name: value\r\n" into w; 0, -1 when a value cannot be resolved (w untouched then) */
int llm_cfg_headers(llm_cfg *c, const llm_model *m, const char *key, jw *w);

#endif
