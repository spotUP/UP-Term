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
#include "be_azure.h"
#include "be_bedrock.h"

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
    /* llm_cfg_endpoint: the strings an azure model's base URL, api-version and deployment point to, and the base URL
     * the catalog gave it */
    llm_azure_ep az;
    char az_orig[300];
    /* llm_cfg_endpoint for bedrock-converse-stream: the credentials and region, the endpoint, and the catalog's base URL */
    llm_aws aws;
    char bd_ep[300], bd_orig[300];
    /* the "env" object of the stored api_key credential of the provider being resolved (pi's credential.env, the
     * ProviderEnv of the request): its non-empty values win over getenv. Set for the length of one llm_cfg_* call. */
    const char *cenv;
    long cenv_n;
} llm_cfg;

void llm_cfg_init(llm_cfg *c);
void llm_cfg_free(llm_cfg *c);
/* a config value resolved into out: its length, -1 unresolved */
long llm_cfg_value(llm_cfg *c, const char *cfg, char *out, long cap);
/* pi's resolveConfigValueOrThrow (core/resolve-config-value.js): the value resolved into out, its length (empty is
 * fine for a template); -1 with pi's message in err ("Failed to resolve <what> from environment variable: X",
 * "... variables: X, Y", "... from shell command: <cmd>"). A "!command" runs every time: no cache, as pi does not
 * cache on this path. */
long llm_cfg_value_strict(llm_cfg *c, const char *cfg, const char *what, char *out, long cap, char *err, long errcap);
/* the model's credential: the length, 0 none; *src names where it came from ("--api-key", "auth.json",
 * "models.json", "environment", 0 none) */
long llm_cfg_key(llm_cfg *c, const llm_model *m, char *out, long cap, const char **src);
/* azure-openai-responses: m's base URL, api version and deployment name from the environment (AZURE_OPENAI_BASE_URL,
 * _RESOURCE_NAME, _API_VERSION, _DEPLOYMENT_NAME_MAP; pi's api/azure-openai-config.ts) and the model's own; the strings
 * live in c. Any other api: m untouched. 0, or -1 with pi's message in err */
int llm_cfg_endpoint(llm_cfg *c, llm_model *m, char *err, long errcap);
/* does pi 1.1.0 offer a subscription login for the provider here (openai, openai-codex, github-copilot, kimi-coding, xai,
 * meta; never anthropic) */
int llm_oauth_provider(const char *provider);
/* what a stored OAuth credential (auth.json {"type":"oauth","access":...}) changes about the request (the provider's
 * toAuth, auth/oauth/<provider>.ts): the access token is llm_cfg_key's credential; base: the base URL the credential carries
 * (github-copilot, from the token's proxy-ep, else the enterprise domain), "" none; bearer: the credential goes out as
 * Authorization: Bearer in place of the backend's own key header (kimi-coding, copilot's anthropic-messages);
 * copilot: Copilot's per-request headers (api/github-copilot-headers.ts) go along. 1 when the provider's stored
 * credential is OAuth, else 0 (u zeroed) */
typedef struct llm_oauth_use {
    char base[300];
    int bearer, copilot;
} llm_oauth_use;
int llm_cfg_oauth(llm_cfg *c, const llm_model *m, llm_oauth_use *u);
/* github-copilot's filterModels: with an OAuth credential that lists availableModelIds only those models are the
 * account's; 1 for every other model and credential */
int llm_cfg_model_ok(llm_cfg *c, const llm_model *m);
/* base64url (or base64) text into bytes: the count, -1 not base64 / too long */
long llm_b64url_decode(const char *s, long n, unsigned char *out, long cap);
/* the chatgpt_account_id claim of an OpenAI access token (a JWT; pi extracts it with atob, this decodes base64url):
 * 0, -1 */
int llm_jwt_account_id(const char *token, char *out, long cap);
/* pi's checkAuth for a model: a credential for its provider (llm_cfg_key), or for amazon-bedrock the AWS ones too
 * (AWS_PROFILE, AWS_ACCESS_KEY_ID with AWS_SECRET_ACCESS_KEY, the container and web identity variables: pi's
 * env-api-keys.ts lists them; a request can use the access keys and the bearer token only, llm_cfg_endpoint says so) */
int llm_cfg_authed(llm_cfg *c, const llm_model *m);
/* "OPENAI_API_KEY" for provider "openai" (pi's env-api-keys.ts names for azure, google and amazon-bedrock) */
void llm_key_env_name(const char *provider, char *out, long cap);
/* the model's extra request headers (models.json headers, resolved; authHeader's Authorization) as lines
 * "Name: value\r\n" into w; 0, -1 when a value cannot be resolved (w untouched then) */
int llm_cfg_headers(llm_cfg *c, const llm_model *m, const char *key, jw *w);

#endif
