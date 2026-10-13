/* be_bedrock -- pi's Amazon Bedrock endpoint, region and credential rules (api/bedrock-converse-stream.ts:160-230) for
 * the bedrock-converse-stream backend (Phase M6). Portable C89, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_BE_BEDROCK_H
#define LLM_BE_BEDROCK_H

#include "backend.h"

/* The environment values (0 or "" unset) of AWS_REGION, AWS_DEFAULT_REGION, AWS_PROFILE, AWS_ACCESS_KEY_ID,
 * AWS_SECRET_ACCESS_KEY, AWS_SESSION_TOKEN, AWS_BEDROCK_SKIP_AUTH; bearer: a bearer token is configured
 * (AWS_BEARER_TOKEN_BEDROCK, a stored key); with AWS_BEDROCK_SKIP_AUTH=1 only the environment's counts: the SDK reads
 * that variable itself, whatever pi decides (a stored key is dropped, pi: useBearerToken). */
typedef struct llm_bedrock_env {
    const char *region, *default_region, *profile, *access, *secret, *token, *skip_auth, *force_cache, *cache_retention;
    int bearer;
} llm_bedrock_env;

/* The region the request is signed for, the endpoint it goes to (endpoint) and the credentials, as pi's stream() settles
 * them: the region of an inference-profile ARN model id, else the configured one, else the standard endpoint's own, else
 * us-east-1; a standard bedrock-runtime base URL gives way to the configured region's endpoint, any other base URL is
 * used as given. 0, or -1 with the reason in err: no credentials this build can use (a bearer token, AWS_ACCESS_KEY_ID with
 * AWS_SECRET_ACCESS_KEY, or AWS_BEDROCK_SKIP_AUTH=1; profiles, SSO, container and web identity credentials are not
 * supported). */
int llm_bedrock_resolve(const llm_model *m, const llm_bedrock_env *e, llm_aws *aws, char *endpoint, long epcap,
                        char *err, long errcap);

#endif
