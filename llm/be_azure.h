/* be_azure -- the azure-openai-responses backend (Phase M3) and pi's Azure endpoint rules (api/azure-openai-config.ts).
 * Portable C89, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_BE_AZURE_H
#define LLM_BE_AZURE_H

#include "backend.h"

typedef struct llm_azure_ep {
    char base[300];             /* resolveAzureBaseUrl: below it /responses is posted */
    char version[48];           /* resolveAzureConfig's apiVersion: the api-version query */
    char deployment[100];       /* resolveDeploymentName: the request's "model" */
} llm_azure_ep;

/* pi's resolveAzureConfig + resolveDeploymentName for model m from the four environment values (0 or "" unset):
 * AZURE_OPENAI_BASE_URL, AZURE_OPENAI_RESOURCE_NAME, AZURE_OPENAI_API_VERSION, AZURE_OPENAI_DEPLOYMENT_NAME_MAP.
 * The base URL is the first of: the base URL variable, https://<resource>.openai.azure.com/openai/v1, m->base_url.
 * 0, or -1 with pi's message in err (no base URL, an invalid one) */
int llm_azure_resolve(const llm_model *m, const char *e_base, const char *e_res, const char *e_ver, const char *e_map,
                      llm_azure_ep *out, char *err, long errcap);

#endif
